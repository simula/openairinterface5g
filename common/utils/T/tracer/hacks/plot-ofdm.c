#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <X11/Xlib.h>
#include <stdint.h>
#include <pthread.h>
#include <complex.h>
#include <fftw3.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"

const int width = 14 * 20 * 4;
int height;

/* when vzoom = 1 each pixel represents 1 RE
 * when 2, it is max of 2 vertical REs
 * useful if height of plot is to big
 * (with 106 RB we already reach 106*12 = 1272 pixels, so with even bigger
 * bandwidths...)
 */
int vzoom = 1;
/* when hzoom = 1 each pixel represents 1 RE
 * when 2, it is max of 2 horizontal REs
 * useful to slow down display
 */
int hzoom = 1;
/* intensity is to scale the input
 * normally, as float, cabs(input) is in [0..1]
 * to get pixel color we *255
 * but maybe it's too intense, or not enough,
 * so let's have a control the user can set at will
 */
float intensity = 255;

pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

void lock(void)
{
  pthread_mutex_lock(&m);
}

void unlock(void)
{
  pthread_mutex_unlock(&m);
}

typedef struct {
  Display *d;
  Window w;
  XImage *img;
  unsigned char *img_data;
  int img_pos;
} gui_t;

/* delay management begin */

long double frame_time;

void time_start(void)
{
  struct timespec l;
  if (clock_gettime(CLOCK_REALTIME, &l)) abort();
  frame_time = l.tv_sec * 1000000000. + l.tv_nsec;
}

void delay(int fps)
{
  struct timespec t;
  struct timespec delay;
  long double cur_time;

  frame_time += 1000000000. / fps;

  if (clock_gettime(CLOCK_REALTIME, &t)) abort();

  cur_time = t.tv_sec * 1000000000. + t.tv_nsec;

  if (frame_time < cur_time) { fprintf(stderr, "ERROR: we late\n"); return; }

  delay.tv_nsec = ((uint64_t)(frame_time-cur_time)) % 1000000000;
  delay.tv_sec = ((uint64_t)(frame_time-cur_time)) / 1000000000;

  if (pselect(0, NULL, NULL, NULL, &delay, NULL)) { fprintf(stderr, "ERROR: pselect fails\n"); exit(1); }
}

/* delay management end */

static void *gui_thread(void *_g)
{
  time_start();

  gui_t *g = _g;
  while (1) {
    XEvent e; while (XPending(g->d)) XNextEvent(g->d, &e);
    lock();
    XPutImage(g->d, g->w, DefaultGC(g->d, DefaultScreen(g->d)), g->img, 0, 0, 0, 0,
              width, height/vzoom);
    unlock();
    delay(10);
  }
  return 0;
}

unsigned char re_to_pixel(float re)
{
  float v = fminf(255, re * intensity);
  return v;
}

unsigned char mix(unsigned char a, unsigned char b)
{
  return a > b ? a : b;
}

void usage(void)
{
  printf("options:\n");
  printf("    -f                      input is float, not s16\n");
  printf("    -srate <sampling rate>  set sampling rate (default 61440000)\n");
  printf("    -prb <prb>              set number of PRBs (default 106)\n");
  printf("    -mu <mu>                set mu (default 1, 0 is for 15KHz, 1 for 30KHz, etc.)\n");
  printf("    -no-fft                 don't do fft, input is in frequency domain\n");
  printf("    -vzoom <zoom>           add some sort of vertical zoom, default 1, passing 2 divides height by 2, etc.\n");
  printf("    -hzoom <zoom>           add some sort of speed control, default 1, passing 2 means 2 REs per pixel, etc.\n");
  printf("    -delay <delay>          delay 'delay' microseconds between processing of each symbol\n");
  printf("    -intensity <intensity>  set intensity of data pixel (default 255.)\n");
  printf("    -skip <count>           skip 'count' samples at beginning\n");
  exit(0);
}

int main(int n, char **v)
{
  int input_is_float = 0;
  int no_fft = 0;
  int delay = 0;
  int skip = 0;
  int symbols_per_subframe;
  long sampling_rate = 61440000;
  int mu = 1;
  int prb = 106;

  /* parse command line arguments */
  for (int i = 1; i < n; i++) {
    if (!strcmp(v[i], "-f")) { input_is_float = 1; continue; }
    if (!strcmp(v[i], "-no-fft")) { no_fft = 1; continue; }
    if (!strcmp(v[i], "-vzoom")) { if (i>n-2) usage(); vzoom = atoi(v[++i]); continue; }
    if (!strcmp(v[i], "-hzoom")) { if (i>n-2) usage(); hzoom = atoi(v[++i]); continue; }
    if (!strcmp(v[i], "-delay")) { if (i>n-2) usage(); delay = atoi(v[++i]); continue; }
    if (!strcmp(v[i], "-intensity")) { if (i>n-2) usage(); intensity = atof(v[++i]); continue; }
    if (!strcmp(v[i], "-srate")) { if (i>n-2) usage(); sampling_rate = atol(v[++i]); continue; }
    if (!strcmp(v[i], "-prb")) { if (i>n-2) usage(); prb = atoi(v[++i]); continue; }
    if (!strcmp(v[i], "-mu")) { if (i>n-2) usage(); mu = atoi(v[++i]); continue; }
    if (!strcmp(v[i], "-skip")) { if (i>n-2) usage(); skip = atoi(v[++i]); continue; }
    usage();
  }

  symbols_per_subframe = 14 * (1 << mu);

  /* compute durations from sampling rate and mu (scs) */
  /* this is 38.211 5.3.1, a bit simplified/reordered */
  int ofdm_duration = sampling_rate / 15000 / (1 << mu);
  int l0_cycle_prefix = sampling_rate / 480000 * (144*64 / (1 << mu) + 16*64) / 4096;
  int cycle_prefix = sampling_rate / 480000 * (144*64 / (1 << mu)) / 4096;

  /* small sanity check */
  if (prb * 12 > ofdm_duration) {
    printf("fatal: too many PRBs for this sampling rate / mu\n");
    exit(1);
  }

  height = prb * 12;

  if (height % vzoom) { fprintf(stderr, "bad vzoom for this height, try 1, 2, 4, ...\n"); return 1; }

  gui_t g = { 0 };

  /* open X11 display */
  g.d = XOpenDisplay(0);
  if (!g.d) {
    fprintf(stderr, "could not open display\n");
    return 1;
  }

  /* create window */
  g.w = XCreateSimpleWindow(g.d, DefaultRootWindow(g.d), 0, 0,
                            width, height / vzoom, 0, WhitePixel(g.d, DefaultScreen(g.d)),
                            WhitePixel(g.d, DefaultScreen(g.d)));
  XStoreName(g.d, g.w, "IQ");
  /* enable backing store */
  {
    XSetWindowAttributes att;
    att.backing_store = Always;
    XChangeWindowAttributes(g.d, g.w, CWBackingStore, &att);
  }
  XSelectInput(g.d, g.w,
               KeyPressMask      |
               ButtonPressMask   |
               ButtonReleaseMask |
               PointerMotionMask |
               ExposureMask      |
               StructureNotifyMask);
  XMapWindow(g.d, g.w);

  g.img_data = calloc(1, width * height/vzoom * 4); if (!g.img_data) abort();
  g.img = XCreateImage(g.d, DefaultVisual(g.d, DefaultScreen(g.d)), 24 /*depth*/,
                   ZPixmap, 0 /*offset*/, (char*)g.img_data, width, height/vzoom, 8, 0);

  new_thread(gui_thread, &g);

  /* fft data */
  int fft_size = ofdm_duration;
  float complex *in = fftwf_malloc(sizeof(float complex) * fft_size);
  float complex *out = fftwf_malloc(sizeof(float complex) * fft_size);
  fftwf_plan p_fft = fftwf_plan_dft_1d(fft_size, in, out,
                                       FFTW_FORWARD, FFTW_ESTIMATE);
  memset(in, 0, sizeof(float complex) * fft_size);
  fftwf_execute(p_fft);

  int symbol = 0;
  int back_color = 0;
  int hpixel = 0;

  if (skip) {
    if (input_is_float) {
      float intime[skip * 2];
      if (fread(intime, skip * 8, 1, stdin) != 1) abort();
    } else {
      short intime[skip * 2];
      if (fread(intime, skip * 4, 1, stdin) != 1) abort();
    }
  }

  while (1) {
    int cur_cycle_prefix;
    if (symbol == 0 || symbol == 7 * (1 << mu))
      cur_cycle_prefix = l0_cycle_prefix;
    else
      cur_cycle_prefix = cycle_prefix;

    if (no_fft) goto without_fft;

    /* input signal is in time domain, timing aligned, do FFT */

    /* read time domain data */
    if (input_is_float) {
      float intime[(ofdm_duration + cur_cycle_prefix) * 2];
      if (fread(intime, (ofdm_duration + cur_cycle_prefix) * 8, 1, stdin) != 1) break;

      for (int i = 0; i < fft_size; i++)
        /* skip cycle prefix */
        in[i] = intime[(cur_cycle_prefix + i) * 2] + I * intime[(cur_cycle_prefix + i) * 2 + 1];
    } else {
      short intime[(ofdm_duration + cur_cycle_prefix) * 2];
      if (fread(intime, (ofdm_duration + cur_cycle_prefix) * 4, 1, stdin) != 1) break;

      for (int i = 0; i < fft_size; i++)
        /* skip cycle prefix */
        in[i] = intime[(cur_cycle_prefix + i) * 2] / 2048. + I * intime[(cur_cycle_prefix + i) * 2 + 1] / 2048.;
    }

    /* go to freq domain */
    fftwf_execute(p_fft);

    goto after_fft;

without_fft:
    /* input data is symbols, each containing full FFT data */
    if (input_is_float) {
      float infreq[fft_size * 2];
      if (fread(infreq, fft_size * 2 * 4, 1, stdin) != 1) break;

      for (int i = 0; i < fft_size; i++)
        out[i] = infreq[i * 2] + I * infreq[i * 2 + 1];
    } else {
      short infreq[ofdm_duration * 2];
      if (fread(infreq, fft_size * 2 * 2, 1, stdin) != 1) break;

      for (int i = 0; i < fft_size; i++)
        out[i] = infreq[i * 2] / 2048. + I * infreq[i * 2 + 1] / 2048.;
    }

after_fft:
    lock();

    /* extract data of this ofdm symbol */
    unsigned char *p = g.img_data + g.img_pos * 4;
    int front_pos = (g.img_pos + 8) % width;
    unsigned char *pfront = g.img_data + front_pos * 4;
    for (int i = prb * 12 - 1; i >= 0; i -= vzoom) {
      /* take the max */
      float out_val = 0;
      for (int j = 0; j < vzoom; j++) {
        int start = - prb * 12 / 2;
        int pos = start + i + j;
        /* neg freq are at the end of fft data array */
        int pos_in_fft = pos >= 0
                         ? pos
                         : pos + fft_size;
        float v = cabs(out[pos_in_fft]);
        if (v > out_val) out_val = v;
      }
      int color = re_to_pixel(out_val);
      /* grey level image mixed with some background changing every slot */
      unsigned char back_col[2][3] = {
        { 100, 90, 90 },
        { 100, 60, 60, }
      };
      if (hpixel == 0) {
        p[0] = p[1] = p[2] = color;
      } else {
        p[0] = mix(p[0], color);
        p[1] = mix(p[1], color);
        p[2] = mix(p[2], color);
      }
      if (hpixel == hzoom - 1) {
        p[0] = mix(p[0], back_col[back_color][0]);
        p[1] = mix(p[1], back_col[back_color][1]);
        p[2] = mix(p[2], back_col[back_color][2]);
      }
      p[3] = 255;
      p += width * 4;
      pfront[0] = 255;
      pfront[1] = 128;
      pfront[2] = 128;
      pfront[3] = 255;
      pfront += width * 4;
    }

    unlock();

    hpixel++;
    if (hpixel == hzoom) {
      hpixel = 0;
      g.img_pos++;
      if (g.img_pos == width) g.img_pos = 0;
    }

    symbol++;
    if (symbol == symbols_per_subframe) symbol = 0;

    /* switch back color every slot */
    if (symbol % 14 == 0)
      back_color = 1 - back_color;

    if (delay) usleep(delay);
  }

  return 0;
}
