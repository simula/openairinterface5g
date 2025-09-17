#include <stdio.h>
#include <math.h>
#include "openair1/PHY/TOOLS/tools_defs.h"
#include "openair1/SIMULATION/TOOLS/sim.h"
#include "common/utils/utils.h"

// should be FOREACH_DFTSZ but we restrict to tested DFT sizes
#define FOREACH_DFTSZ_working(SZ_DEF) \
  SZ_DEF(12)                          \
  SZ_DEF(64)                          \
  SZ_DEF(128)                         \
  SZ_DEF(256)                         \
  SZ_DEF(512)                         \
  SZ_DEF(768)                         \
  SZ_DEF(1024)                        \
  SZ_DEF(1536)                        \
  SZ_DEF(2048)                        \
  SZ_DEF(4096)                        \
  SZ_DEF(6144)                        \
  SZ_DEF(8192)                        \
  SZ_DEF(12288)

#define SZ_PTR(Sz) {Sz},
struct {
  int size;
} const dftFtab[] = {FOREACH_DFTSZ_working(SZ_PTR)};

#define SZ_iPTR(Sz) {Sz},
struct {
  int size;
} const idftFtab[] = {FOREACH_IDFTSZ(SZ_iPTR)};

bool error(c16_t v16, cd_t vd, double percent)
{
  cd_t err = {abs(v16.r - vd.r), abs(v16.i - vd.i)};
  if (err.r < 10 && err.i < 10)
    return false; // ignore quantization noise
  int denomr = min(abs(v16.r), abs(vd.r));
  if (denomr && err.r / denomr > percent / 100)
    return true;
  int denomi = min(abs(v16.i), abs(vd.i));
  if (denomi && err.i / denomi > percent / 100)
    return true;
  return false;
}

void math_dft(cd_t *in, cd_t *out, int len)
{
  for (int k = 0; k < len; k++) {
    cd_t tmp = {0};
    // wrote this way to help gcc to generate SIMD
    double phi[len], sint[len], cost[len];
    for (int n = 0; n < len; n++)
      phi[n] = -2 * M_PI * ((double)k / len) * n;
    for (int n = 0; n < len; n++)
      sint[n] = sin(phi[n]);
    for (int n = 0; n < len; n++)
      cost[n] = cos(phi[n]);
    for (int n = 0; n < len; n++) {
      cd_t coeff = {.r = cost[n], .i = sint[n]};
      cd_t component = cdMul(coeff, in[n]);
      tmp.r += component.r;
      tmp.i += component.i;
    }
    out[k].r = tmp.r / sqrt(len);
    out[k].i = tmp.i / sqrt(len);
  }
}

int main(void)
{
  int ret = 0;
  load_dftslib();
  c16_t *d16 = malloc16(12 * dftFtab[sizeofArray(dftFtab) - 1].size * sizeof(*d16));
  c16_t *o16 = malloc16(12 * dftFtab[sizeofArray(dftFtab) - 1].size * sizeof(*d16));
  for (int sz = 0; sz < sizeofArray(dftFtab); sz++) {
    const int n = dftFtab[sz].size;
    cd_t data[n];
    double coeffs[] = {0.25, 0.5, 1, 1.5, 2, 2.5, 3};
    cd_t out[n];
    for (int i = 0; i < n; i++) {
      data[i].r = gaussZiggurat(0, 1.0); // gaussZiggurat not used paramters, to fix
      data[i].i = gaussZiggurat(0, 1.0);
    }
    math_dft(data, out, n);
    double evm[sizeofArray(coeffs)] = {0};
    double samples[sizeofArray(coeffs)] = {0};
    for (int coeff = 0; coeff < sizeofArray(coeffs); coeff++) {
      double expand = coeffs[coeff] * SHRT_MAX / sqrt(n);
      if (n == 12) {
        for (int i = 0; i < n; i++)
          for (int j = 0; j < 4; j++) {
            d16[i * 4 + j].r = expand * data[i].r;
            d16[i * 4 + j].i = expand * data[i].i;
          }
      } else {
        for (int i = 0; i < n; i++) {
          d16[i].r = expand * data[i].r;
          d16[i].i = expand * data[i].i;
        }
      }
      dft(get_dft(n), (int16_t *)d16, (int16_t *)o16, 1);
      if (n == 12) {
        for (int i = 0; i < n; i++) {
          cd_t error = {.r = o16[i * 4].r / (expand * sqrt(n)) - out[i].r, .i = o16[i * 4].i / (expand * sqrt(n)) - out[i].i};
          evm[coeff] += sqrt(squaredMod(error)) / sqrt(squaredMod(out[i]));
          samples[coeff] += sqrt(squaredMod(d16[i]));
        }
      } else {
        for (int i = 0; i < n; i++) {
          cd_t error = {.r = o16[i].r / expand - out[i].r, .i = o16[i].i / expand - out[i].i};
          evm[coeff] += sqrt(squaredMod(error)) / sqrt(squaredMod(out[i]));
          samples[coeff] += sqrt(squaredMod(d16[i]));
          /*
            if (error(o16[i], out[i], 5))
            printf("Error in dft %d at %d, (%d, %d) != %f, %f)\n", n, i, o16[i].r, o16[i].i, gslout[i].r, gslout[i].i);
          */
        }
      }
    }
    printf("done DFT size %d (evm (%%), avg samples amplitude) = ", n);
    for (int coeff = 0; coeff < sizeofArray(coeffs); coeff++)
      printf("(%.2f, %.0f) ", (evm[coeff] / n) * 100, samples[coeff] / n);
    printf("\n");
    int i;
    for (i = 0; i < sizeofArray(coeffs); i++)
      if (evm[i] / n < 0.01)
        break;
    if (i == sizeofArray(coeffs)) {
      printf("DFT size: %d, minimum error is more than 1%%, setting the test as failed\n", n);
      ret = 1;
    }
    fflush(stdout);
  }
  free(d16);
  free(o16);
  return ret;
}
