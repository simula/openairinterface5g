/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

#include "nr_phy_common.h"

#ifdef __aarch64__
#define USE_128BIT
#endif

int16_t saturating_sub(int16_t a, int16_t b)
{
  int32_t result = (int32_t)a - (int32_t)b;

  if (result < INT16_MIN) {
    return INT16_MIN;
  } else if (result > INT16_MAX) {
    return INT16_MAX;
  } else {
    return (int16_t)result;
  }
}

//----------------------------------------------------------------------------------------------
// QPSK
//----------------------------------------------------------------------------------------------
void nr_qpsk_llr(int32_t *rxdataF_comp, int16_t *llr, uint32_t nb_re)
{
  c16_t *rxF   = (c16_t *)rxdataF_comp;
  c16_t *llr32 = (c16_t *)llr;
  for (int i = 0; i < nb_re; i++) {
    llr32[i].r = rxF[i].r >> 4;
    llr32[i].i = rxF[i].i >> 4;
  }
}

//----------------------------------------------------------------------------------------------
// 16-QAM
//----------------------------------------------------------------------------------------------

void nr_16qam_llr(int32_t *rxdataF_comp, int32_t *ch_mag_in, int16_t *llr, uint32_t nb_re)
{
  simde__m256i *rxF_256 = (simde__m256i *)rxdataF_comp;
  simde__m256i *ch_mag = (simde__m256i *)ch_mag_in;
  int64_t *llr_64 = (int64_t *)llr;

#ifndef USE_128BIT
  simde__m256i xmm0, xmm1, xmm2;

  for (int i = 0; i < (nb_re >> 3); i++) {
    // registers of even index in xmm0-> |y_R|, registers of odd index in xmm0-> |y_I|
    xmm0 = simde_mm256_abs_epi16(*rxF_256);
    // registers of even index in xmm0-> |y_R|-|h|^2, registers of odd index in xmm0-> |y_I|-|h|^2
    xmm0 = simde_mm256_subs_epi16(*ch_mag, xmm0);

    xmm1 = simde_mm256_unpacklo_epi32(*rxF_256, xmm0);
    xmm2 = simde_mm256_unpackhi_epi32(*rxF_256, xmm0);

    // xmm1 |1st 2ed 3rd 4th  9th 10th 13rd 14th|
    // xmm2 |5th 6th 7th 8th 11st 12ed 15th 16th|

    *llr_64++ = simde_mm256_extract_epi64(xmm1, 0);
    *llr_64++ = simde_mm256_extract_epi64(xmm1, 1);
    *llr_64++ = simde_mm256_extract_epi64(xmm2, 0);
    *llr_64++ = simde_mm256_extract_epi64(xmm2, 1);
    *llr_64++ = simde_mm256_extract_epi64(xmm1, 2);
    *llr_64++ = simde_mm256_extract_epi64(xmm1, 3);
    *llr_64++ = simde_mm256_extract_epi64(xmm2, 2);
    *llr_64++ = simde_mm256_extract_epi64(xmm2, 3);
    rxF_256++;
    ch_mag++;
  }

  nb_re &= 0x7;
#endif

  simde__m128i *rxF_128 = (simde__m128i *)rxF_256;
  simde__m128i *ch_mag_128 = (simde__m128i *)ch_mag;
  simde__m128i *llr_128 = (simde__m128i *)llr_64;

  // Each iteration does 4 RE (gives 16 16bit-llrs)
  for (int i = 0; i < (nb_re >> 2); i++) {
    // registers of even index in xmm0-> |y_R|, registers of odd index in xmm0-> |y_I|
    simde__m128i xmm0 = simde_mm_abs_epi16(*rxF_128);
    // registers of even index in xmm0-> |y_R|-|h|^2, registers of odd index in xmm0-> |y_I|-|h|^2
    xmm0 = simde_mm_subs_epi16(*ch_mag_128, xmm0);

    llr_128[0] = simde_mm_unpacklo_epi32(*rxF_128, xmm0); // llr128[0] contains the llrs of the 1st,2nd,5th and 6th REs
    llr_128[1] = simde_mm_unpackhi_epi32(*rxF_128, xmm0); // llr128[1] contains the llrs of the 3rd, 4th, 7th and 8th REs
    llr_128 += 2;
    rxF_128++;
    ch_mag_128++;
  }

  simde_mm_empty();

  nb_re &= 0x3;
  int16_t *rxDataF_i16 = (int16_t *)rxF_128;
  int16_t *ch_mag_i16 = (int16_t *)ch_mag_128;
  int16_t *llr_i16 = (int16_t *)llr_128;
  for (uint i = 0U; i < nb_re; i++) {
    int16_t real = rxDataF_i16[2 * i];
    int16_t imag = rxDataF_i16[2 * i + 1];
    int16_t mag_real = ch_mag_i16[2 * i];
    int16_t mag_imag = ch_mag_i16[2 * i + 1];
    llr_i16[4 * i] = real;
    llr_i16[4 * i + 1] = imag;
    llr_i16[4 * i + 2] = saturating_sub(mag_real, abs(real));
    llr_i16[4 * i + 3] = saturating_sub(mag_imag, abs(imag));
  }
}

//----------------------------------------------------------------------------------------------
// 64-QAM
//----------------------------------------------------------------------------------------------

void nr_64qam_llr(int32_t *rxdataF_comp, int32_t *ch_mag, int32_t *ch_mag2, int16_t *llr, uint32_t nb_re)
{
  simde__m256i *rxF = (simde__m256i *)rxdataF_comp;

  simde__m256i *ch_maga = (simde__m256i *)ch_mag;
  simde__m256i *ch_magb = (simde__m256i *)ch_mag2;

  int32_t *llr_32 = (int32_t *)llr;

#ifndef USE_128BIT
  simde__m256i xmm0, xmm1, xmm2;
  for (int i = 0; i < (nb_re >> 3); i++) {
    xmm0 = *rxF;
    // registers of even index in xmm0-> |y_R|, registers of odd index in xmm0-> |y_I|
    xmm1 = simde_mm256_abs_epi16(xmm0);
    // registers of even index in xmm0-> |y_R|-|h|^2, registers of odd index in xmm0-> |y_I|-|h|^2
    xmm1 = simde_mm256_subs_epi16(*ch_maga, xmm1);
    xmm2 = simde_mm256_abs_epi16(xmm1);
    xmm2 = simde_mm256_subs_epi16(*ch_magb, xmm2);
    // xmm0 |1st 4th 7th 10th 13th 16th 19th 22ed|
    // xmm1 |2ed 5th 8th 11th 14th 17th 20th 23rd|
    // xmm2 |3rd 6th 9th 12th 15th 18th 21st 24th|

    *llr_32++ = simde_mm256_extract_epi32(xmm0, 0);
    *llr_32++ = simde_mm256_extract_epi32(xmm1, 0);
    *llr_32++ = simde_mm256_extract_epi32(xmm2, 0);

    *llr_32++ = simde_mm256_extract_epi32(xmm0, 1);
    *llr_32++ = simde_mm256_extract_epi32(xmm1, 1);
    *llr_32++ = simde_mm256_extract_epi32(xmm2, 1);

    *llr_32++ = simde_mm256_extract_epi32(xmm0, 2);
    *llr_32++ = simde_mm256_extract_epi32(xmm1, 2);
    *llr_32++ = simde_mm256_extract_epi32(xmm2, 2);

    *llr_32++ = simde_mm256_extract_epi32(xmm0, 3);
    *llr_32++ = simde_mm256_extract_epi32(xmm1, 3);
    *llr_32++ = simde_mm256_extract_epi32(xmm2, 3);

    *llr_32++ = simde_mm256_extract_epi32(xmm0, 4);
    *llr_32++ = simde_mm256_extract_epi32(xmm1, 4);
    *llr_32++ = simde_mm256_extract_epi32(xmm2, 4);

    *llr_32++ = simde_mm256_extract_epi32(xmm0, 5);
    *llr_32++ = simde_mm256_extract_epi32(xmm1, 5);
    *llr_32++ = simde_mm256_extract_epi32(xmm2, 5);

    *llr_32++ = simde_mm256_extract_epi32(xmm0, 6);
    *llr_32++ = simde_mm256_extract_epi32(xmm1, 6);
    *llr_32++ = simde_mm256_extract_epi32(xmm2, 6);

    *llr_32++ = simde_mm256_extract_epi32(xmm0, 7);
    *llr_32++ = simde_mm256_extract_epi32(xmm1, 7);
    *llr_32++ = simde_mm256_extract_epi32(xmm2, 7);
    rxF++;
    ch_maga++;
    ch_magb++;
  }

  nb_re &= 0x7;
#endif

  simde__m128i *rxF_128 = (simde__m128i *)rxF;
  simde__m128i *ch_mag_128 = (simde__m128i *)ch_maga;
  simde__m128i *ch_magb_128 = (simde__m128i *)ch_magb;

  simde__m64 *llr64 = (simde__m64 *)llr_32;

  // Each iteration does 4 RE (gives 24 16bit-llrs)
  for (int i = 0; i < (nb_re >> 2); i++) {
    simde__m128i xmm0, xmm1, xmm2;
    xmm0 = *rxF_128;
    xmm1 = simde_mm_abs_epi16(xmm0);
    xmm1 = simde_mm_subs_epi16(*ch_mag_128, xmm1);
    xmm2 = simde_mm_abs_epi16(xmm1);
    xmm2 = simde_mm_subs_epi16(*ch_magb_128, xmm2);

    *llr64++ = simde_mm_set_pi32(simde_mm_extract_epi32(xmm1, 0), simde_mm_extract_epi32(xmm0, 0));
    *llr64++ = simde_mm_set_pi32(simde_mm_extract_epi32(xmm0, 1), simde_mm_extract_epi32(xmm2, 0));
    *llr64++ = simde_mm_set_pi32(simde_mm_extract_epi32(xmm2, 1), simde_mm_extract_epi32(xmm1, 1));
    *llr64++ = simde_mm_set_pi32(simde_mm_extract_epi32(xmm1, 2), simde_mm_extract_epi32(xmm0, 2));
    *llr64++ = simde_mm_set_pi32(simde_mm_extract_epi32(xmm0, 3), simde_mm_extract_epi32(xmm2, 2));
    *llr64++ = simde_mm_set_pi32(simde_mm_extract_epi32(xmm2, 3), simde_mm_extract_epi32(xmm1, 3));
    rxF_128++;
    ch_mag_128++;
    ch_magb_128++;
  }

  nb_re &= 0x3;

  int16_t *rxDataF_i16 = (int16_t *)rxF_128;
  int16_t *ch_mag_i16 = (int16_t *)ch_mag_128;
  int16_t *ch_magb_i16 = (int16_t *)ch_magb_128;
  int16_t *llr_i16 = (int16_t *)llr64;
  for (int i = 0; i < nb_re; i++) {
    int16_t real = rxDataF_i16[2 * i];
    int16_t imag = rxDataF_i16[2 * i + 1];
    int16_t mag_real = ch_mag_i16[2 * i];
    int16_t mag_imag = ch_mag_i16[2 * i + 1];
    llr_i16[6 * i] = real;
    llr_i16[6 * i + 1] = imag;
    llr_i16[6 * i + 2] = saturating_sub(mag_real, abs(real));
    llr_i16[6 * i + 3] = saturating_sub(mag_imag, abs(imag));
    int16_t mag_realb = ch_magb_i16[2 * i];
    int16_t mag_imagb = ch_magb_i16[2 * i + 1];
    llr_i16[6 * i + 4] = saturating_sub(mag_realb, abs(llr_i16[6 * i + 2]));
    llr_i16[6 * i + 5] = saturating_sub(mag_imagb, abs(llr_i16[6 * i + 3]));
  }
  simde_mm_empty();
}

void nr_256qam_llr(int32_t *rxdataF_comp, int32_t *ch_mag, int32_t *ch_mag2, int32_t *ch_mag3, int16_t *llr, uint32_t nb_re)
{
  simde__m256i *rxF_256 = (simde__m256i *)rxdataF_comp;
  simde__m256i *llr256 = (simde__m256i *)llr;

  simde__m256i *ch_maga = (simde__m256i *)ch_mag;
  simde__m256i *ch_magb = (simde__m256i *)ch_mag2;
  simde__m256i *ch_magc = (simde__m256i *)ch_mag3;
#ifndef USE_128BIT
  simde__m256i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6;

  for (int i = 0; i < (nb_re >> 3); i++) {
    // registers of even index in xmm0-> |y_R|, registers of odd index in xmm0-> |y_I|
    xmm0 = simde_mm256_abs_epi16(*rxF_256);
    // registers of even index in xmm0-> |y_R|-|h|^2, registers of odd index in xmm0-> |y_I|-|h|^2
    xmm0 = simde_mm256_subs_epi16(*ch_maga, xmm0);
    //  xmmtmpD2 contains 16 LLRs
    xmm1 = simde_mm256_abs_epi16(xmm0);
    xmm1 = simde_mm256_subs_epi16(*ch_magb, xmm1); // contains 16 LLRs
    xmm2 = simde_mm256_abs_epi16(xmm1);
    xmm2 = simde_mm256_subs_epi16(*ch_magc, xmm2); // contains 16 LLRs
    // rxF[i] A0 A1 A2 A3 A4 A5 A6 A7 bits 7,6
    // xmm0   B0 B1 B2 B3 B4 B5 B6 B7 bits 5,4
    // xmm1   C0 C1 C2 C3 C4 C5 C6 C7 bits 3,2
    // xmm2   D0 D1 D2 D3 D4 D5 D6 D7 bits 1,0
    xmm3 = simde_mm256_unpacklo_epi32(*rxF_256, xmm0); // A0 B0 A1 B1 A4 B4 A5 B5
    xmm4 = simde_mm256_unpackhi_epi32(*rxF_256, xmm0); // A2 B2 A3 B3 A6 B6 A7 B7
    xmm5 = simde_mm256_unpacklo_epi32(xmm1, xmm2); // C0 D0 C1 D1 C4 D4 C5 D5
    xmm6 = simde_mm256_unpackhi_epi32(xmm1, xmm2); // C2 D2 C3 D3 C6 D6 C7 D7

    xmm0 = simde_mm256_unpacklo_epi64(xmm3, xmm5); // A0 B0 C0 D0 A4 B4 C4 D4
    xmm1 = simde_mm256_unpackhi_epi64(xmm3, xmm5); // A1 B1 C1 D1 A5 B5 C5 D5
    xmm2 = simde_mm256_unpacklo_epi64(xmm4, xmm6); // A2 B2 C2 D2 A6 B6 C6 D6
    xmm3 = simde_mm256_unpackhi_epi64(xmm4, xmm6); // A3 B3 C3 D3 A7 B7 C7 D7
    *llr256++ = simde_mm256_permute2x128_si256(xmm0, xmm1, 0x20); // A0 B0 C0 D0 A1 B1 C1 D1
    *llr256++ = simde_mm256_permute2x128_si256(xmm2, xmm3, 0x20); // A2 B2 C2 D2 A3 B3 C3 D3
    *llr256++ = simde_mm256_permute2x128_si256(xmm0, xmm1, 0x31); // A4 B4 C4 D4 A5 B5 C5 D5
    *llr256++ = simde_mm256_permute2x128_si256(xmm2, xmm3, 0x31); // A6 B6 C6 D6 A7 B7 C7 D7
    ch_magc++;
    ch_magb++;
    ch_maga++;
    rxF_256++;
  }

  nb_re &= 0x7;
#endif

  simde__m128i *rxF_128 = (simde__m128i *)rxF_256;
  simde__m128i *llr_128 = (simde__m128i *)llr256;

  simde__m128i *ch_maga_128 = (simde__m128i *)ch_maga;
  simde__m128i *ch_magb_128 = (simde__m128i *)ch_magb;
  simde__m128i *ch_magc_128 = (simde__m128i *)ch_magc;

  for (int i = 0; i < (nb_re >> 2); i++) {
    simde__m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6;
    // registers of even index in xmm0-> |y_R|, registers of odd index in xmm0-> |y_I|
    xmm0 = simde_mm_abs_epi16(*rxF_128);
    // registers of even index in xmm0-> |y_R|-|h|^2, registers of odd index in xmm0-> |y_I|-|h|^2
    xmm0 = simde_mm_subs_epi16(*ch_maga_128, xmm0);
    xmm1 = simde_mm_abs_epi16(xmm0);
    xmm1 = simde_mm_subs_epi16(*ch_magb_128, xmm1); // contains 8 LLRs
    xmm2 = simde_mm_abs_epi16(xmm1);
    xmm2 = simde_mm_subs_epi16(*ch_magc_128, xmm2); // contains 8 LLRs
    // rxF[i] A0 A1 A2 A3
    // xmm0   B0 B1 B2 B3
    // xmm1   C0 C1 C2 C3
    // xmm2   D0 D1 D2 D3
    xmm3 = simde_mm_unpacklo_epi32(*rxF_128, xmm0); // A0 B0 A1 B1
    xmm4 = simde_mm_unpackhi_epi32(*rxF_128, xmm0); // A2 B2 A3 B3
    xmm5 = simde_mm_unpacklo_epi32(xmm1, xmm2); // C0 D0 C1 D1
    xmm6 = simde_mm_unpackhi_epi32(xmm1, xmm2); // C2 D2 C3 D3

    *llr_128++ = simde_mm_unpacklo_epi64(xmm3, xmm5); // A0 B0 C0 D0
    *llr_128++ = simde_mm_unpackhi_epi64(xmm3, xmm5); // A1 B1 C1 D1
    *llr_128++ = simde_mm_unpacklo_epi64(xmm4, xmm6); // A2 B2 C2 D2
    *llr_128++ = simde_mm_unpackhi_epi64(xmm4, xmm6); // A3 B3 C3 D3

    rxF_128++;
    ch_maga_128++;
    ch_magb_128++;
    ch_magc_128++;
  }

  if (nb_re & 3) {
    for (int i = 0; i < (nb_re & 0x3); i++) {
      int16_t *rxDataF_i16 = (int16_t *)rxF_128;
      int16_t *ch_mag_i16 = (int16_t *)ch_maga_128;
      int16_t *ch_magb_i16 = (int16_t *)ch_magb_128;
      int16_t *ch_magc_i16 = (int16_t *)ch_magc_128;
      int16_t *llr_i16 = (int16_t *)llr_128;
      int16_t real = rxDataF_i16[2 * i + 0];
      int16_t imag = rxDataF_i16[2 * i + 1];
      int16_t mag_real = ch_mag_i16[2 * i];
      int16_t mag_imag = ch_mag_i16[2 * i + 1];
      llr_i16[8 * i] = real;
      llr_i16[8 * i + 1] = imag;
      llr_i16[8 * i + 2] = saturating_sub(mag_real, abs(real));
      llr_i16[8 * i + 3] = saturating_sub(mag_imag, abs(imag));
      int16_t magb_real = ch_magb_i16[2 * i];
      int16_t magb_imag = ch_magb_i16[2 * i + 1];
      llr_i16[8 * i + 4] = saturating_sub(magb_real, abs(llr_i16[8 * i + 2]));
      llr_i16[8 * i + 5] = saturating_sub(magb_imag, abs(llr_i16[8 * i + 3]));
      int16_t magc_real = ch_magc_i16[2 * i];
      int16_t magc_imag = ch_magc_i16[2 * i + 1];
      llr_i16[8 * i + 6] = saturating_sub(magc_real, abs(llr_i16[8 * i + 4]));
      llr_i16[8 * i + 7] = saturating_sub(magc_imag, abs(llr_i16[8 * i + 5]));
    }
  }
  simde_mm_empty();
}

void freq2time(uint16_t ofdm_symbol_size, int16_t *freq_signal, int16_t *time_signal)
{
  const idft_size_idx_t idft_size = get_idft(ofdm_symbol_size);
  idft(idft_size, freq_signal, time_signal, 1);
}

void nr_est_delay(int ofdm_symbol_size, const c16_t *ls_est, c16_t *ch_estimates_time, delay_t *delay)
{
  freq2time(ofdm_symbol_size, (int16_t *)ls_est, (int16_t *)ch_estimates_time);

  int max_pos = delay->delay_max_pos;
  int max_val = delay->delay_max_val;
  const int sync_pos = 0;

  for (int i = 0; i < ofdm_symbol_size; i++) {
    int temp = c16amp2(ch_estimates_time[i]) >> 1;
    if (temp > max_val) {
      max_pos = i;
      max_val = temp;
    }
  }

  if (max_pos > ofdm_symbol_size / 2)
    max_pos = max_pos - ofdm_symbol_size;

  delay->delay_max_pos = max_pos;
  delay->delay_max_val = max_val;
  delay->est_delay = max_pos - sync_pos;
}

unsigned int nr_get_tx_amp(int power_dBm, int power_max_dBm, int total_nb_rb, int nb_rb)
{
  // assume power at AMP is 20dBm
  // if gain = 20 (power == 40)
  int gain_dB = power_dBm - power_max_dBm;
  double gain_lin;

  gain_lin = pow(10, .1 * gain_dB);
  if ((nb_rb > 0) && (nb_rb <= total_nb_rb)) {
    return ((int)(AMP * sqrt(gain_lin * total_nb_rb / (double)nb_rb)));
  } else {
    LOG_E(PHY, "Illegal nb_rb/N_RB_UL combination (%d/%d)\n", nb_rb, total_nb_rb);
    // mac_xface->macphy_exit("");
  }
  return (0);
}
