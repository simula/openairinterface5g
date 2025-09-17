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

/**********************************************************************
*
* FILENAME    :  pss_nr.c
*
* MODULE      :  synchronisation signal
*
* DESCRIPTION :  generation of pss
*                3GPP TS 38.211 7.4.2.2 Primary synchronisation signal
*
************************************************************************/

#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <nr-uesoftmodem.h>

#include "PHY/defs_nr_UE.h"

#include "PHY/NR_REFSIG/ss_pbch_nr.h"
#include "common/utils/LOG/vcd_signal_dumper.h"

#define DEFINE_VARIABLES_PSS_NR_H
#include "PHY/NR_REFSIG/pss_nr.h"
#undef DEFINE_VARIABLES_PSS_NR_H

#include "PHY/NR_REFSIG/sss_nr.h"
#include "PHY/NR_UE_TRANSPORT/cic_filter_nr.h"

//#define DBG_PSS_NR
static time_stats_t generic_time[TIME_LAST];
static int pss_search_time_nr(const c16_t **rxdata,
                              const NR_DL_FRAME_PARMS *frame_parms,
                              const c16_t pssTime[NUMBER_PSS_SEQUENCE][frame_parms->ofdm_symbol_size],
                              bool fo_flag,
                              int is,
                              int target_Nid_cell,
                              int *nid2,
                              int *f_off,
                              int *pssPeak,
                              int *pssAvg);

static int16_t primary_synchro_nr2[NUMBER_PSS_SEQUENCE][LENGTH_PSS_NR] = {0};
static c16_t primary_synchro[NUMBER_PSS_SEQUENCE][LENGTH_PSS_NR] = {0};
int16_t *get_primary_synchro_nr2(const int nid2)
{
  return primary_synchro_nr2[nid2];
}

/*******************************************************************
*
* NAME :         generate_pss_nr
*
* PARAMETERS :   N_ID_2 : element 2 of physical layer cell identity
*                value : { 0, 1, 2}
*
* RETURN :       generate binary pss sequence (this is a m-sequence)
*
* DESCRIPTION :  3GPP TS 38.211 7.4.2.2 Primary synchronisation signal
*                Sequence generation
*
*********************************************************************/

void generate_pss_nr(NR_DL_FRAME_PARMS *fp, int N_ID_2)
{
  AssertFatal(fp->ofdm_symbol_size > 127,"Illegal ofdm_symbol_size %d\n",fp->ofdm_symbol_size);
  AssertFatal(N_ID_2>=0 && N_ID_2 <=2,"Illegal N_ID_2 %d\n",N_ID_2);
  int16_t d_pss[LENGTH_PSS_NR];
  int16_t x[LENGTH_PSS_NR];

  int16_t *primary_synchro2 = primary_synchro_nr2[N_ID_2]; /* pss in complex with alternatively i then q */

#define INITIAL_PSS_NR (7)
  const int16_t x_initial[INITIAL_PSS_NR] = {0, 1, 1, 0, 1, 1, 1};
  assert(N_ID_2 < NUMBER_PSS_SEQUENCE);
  memcpy(x, x_initial, sizeof(x_initial));

  for (int i = 0; i < (LENGTH_PSS_NR - INITIAL_PSS_NR); i++)
    x[i + INITIAL_PSS_NR] = (x[i + 4] + x[i]) % (2);

  for (int n=0; n < LENGTH_PSS_NR; n++) {
    const int m = (n + 43 * N_ID_2) % (LENGTH_PSS_NR);
    d_pss[n] = 1 - 2*x[m];
  }

  /* PSS is directly mapped to subcarrier without modulation 38.211 */
  for (int i=0; i < LENGTH_PSS_NR; i++) {
    primary_synchro[N_ID_2][i].r = (d_pss[i] * SHRT_MAX) >> SCALING_PSS_NR; /* Maximum value for type short int ie int16_t */
    primary_synchro2[i] = d_pss[i];
  }

#ifdef DBG_PSS_NR

  if (N_ID_2 == 0) {
    char output_file[255];
    char sequence_name[255];
    sprintf(output_file, "pss_seq_%d_%u.m", N_ID_2, fp->ofdm_symbol_size);
    sprintf(sequence_name, "pss_seq_%d_%u", N_ID_2, fp->ofdm_symbol_size);
    printf("file %s sequence %s\n", output_file, sequence_name);

    LOG_M(output_file, sequence_name, primary_synchro, LENGTH_PSS_NR, 1, 1);
  }

#endif
}

  /* call of IDFT should be done with ordered input as below
  *
  *                n input samples
  *  <------------------------------------------------>
  *  0                                                n
  *  are written into input buffer for IFFT
  *   -------------------------------------------------
  *  |xxxxxxx                       N/2       xxxxxxxx|
  *  --------------------------------------------------
  *  ^      ^                 ^               ^          ^
  *  |      |                 |               |          |
  * n/2    end of            n=0            start of    n/2-1
  *         pss                               pss
  *
  *                   Frequencies
  *      positives                   negatives
  * 0                 (+N/2)(-N/2)
  * |-----------------------><-------------------------|
  *
  * sample 0 is for continuous frequency which is used here
  */
void generate_pss_nr_time(const NR_DL_FRAME_PARMS *fp, const int N_ID_2, int ssbFirstSCS, c16_t pssTime[fp->ofdm_symbol_size])
{
  unsigned int subcarrier_start = get_softmodem_params()->sl_mode == 0 ? PSS_SSS_SUB_CARRIER_START : PSS_SSS_SUB_CARRIER_START_SL;
  unsigned int k = fp->first_carrier_offset + ssbFirstSCS + subcarrier_start;
  if (k>= fp->ofdm_symbol_size) k-=fp->ofdm_symbol_size;
  c16_t synchroF_tmp[fp->ofdm_symbol_size] __attribute__((aligned(32)));
  memset(synchroF_tmp, 0, sizeof(synchroF_tmp));
  for (int i=0; i < LENGTH_PSS_NR; i++) {
    synchroF_tmp[k % fp->ofdm_symbol_size] = primary_synchro[N_ID_2][i];
    k++;
  }

  /* IFFT will give temporal signal of Pss */
  idft((int16_t)get_idft(fp->ofdm_symbol_size),
       (int16_t *)synchroF_tmp, /* complex input but legacy type is wrong*/
       (int16_t *)pssTime, /* complex output */
       1); /* scaling factor */

#ifdef DBG_PSS_NR

  if (N_ID_2 == 0) {
    char output_file[255];
    char sequence_name[255];
    sprintf(output_file, "%s%d_%u%s","pss_seq_t_", N_ID_2, length, ".m");
    sprintf(sequence_name, "%s%d_%u","pss_seq_t_", N_ID_2, length);

    printf("file %s sequence %s\n", output_file, sequence_name);

    LOG_M(output_file, sequence_name, primary_synchro_time, length, 1, 1);
    sprintf(output_file, "%s%d_%u%s","pss_seq_f_", N_ID_2, length, ".m");
    sprintf(sequence_name, "%s%d_%u","pss_seq_f_", N_ID_2, length);
    LOG_M(output_file, sequence_name, synchroF_tmp, length, 1, 1);
  }

#endif



#if 0

/* it allows checking that process of idft on a signal and then dft gives same signal with limited errors */

  if ((N_ID_2 == 0) && (length == 256)) {

    LOG_M("pss_f00.m","pss_f00",synchro_tmp,length,1,1);


    bzero(synchroF_tmp, size);

  

    /* get pss in the time domain by applying an inverse FFT */
    dft((int16_t)get_dft(length),
    	synchro_tmp,           /* complex input */
        synchroF_tmp,          /* complex output */
        1);                 /* scaling factor */

    if ((N_ID_2 == 0) && (length == 256)) {
      LOG_M("pss_f_0.m","pss_f_0",synchroF_tmp,length,1,1);
    }

    /* check Pss */
    k = length - (LENGTH_PSS_NR/2);

#define LIMIT_ERROR_FFT   (10)

    for (int i=0; i < LENGTH_PSS_NR; i++) {
      if (abs(synchroF_tmp[2*k] - primary_synchro[i].r) > LIMIT_ERROR_FFT) {
      printf("Pss Error[%d] Compute %d Reference %d \n", k, synchroF_tmp[2*k], primary_synchro[i].r);
      }
    
      if (abs(synchroF_tmp[2*k+1] - primary_synchro[i].i) > LIMIT_ERROR_FFT) {
        printf("Pss Error[%d] Compute %d Reference %d\n", (2*k+1), synchroF_tmp[2*k+1], primary_synchro[i].i);
      }

      k++;

      if (k >= length) {
        k-=length;
      }
    }
  }
#endif
}

/*******************************************************************
*
* NAME :         init_context_pss_nr
*
* PARAMETERS :   structure NR_DL_FRAME_PARMS give frame parameters
*
* RETURN :       generate binary pss sequences (this is a m-sequence)
*
* DESCRIPTION :  3GPP TS 38.211 7.4.2.2 Primary synchronisation signal
*                Sequence generation
*
*********************************************************************/

static void init_context_pss_nr(NR_DL_FRAME_PARMS *frame_parms_ue)
{
  AssertFatal(frame_parms_ue->ofdm_symbol_size > 127, "illegal ofdm_symbol_size %d\n", frame_parms_ue->ofdm_symbol_size);
  int pss_sequence = get_softmodem_params()->sl_mode == 0 ? NUMBER_PSS_SEQUENCE : NUMBER_PSS_SEQUENCE_SL;
  for (int i = 0; i < pss_sequence; i++) {
    generate_pss_nr(frame_parms_ue,i);
  }
}

/*******************************************************************
*
* NAME :         init_context_synchro_nr
*
* PARAMETERS :   none
*
* RETURN :       generate context for pss and sss
*
* DESCRIPTION :  initialise contexts and buffers for synchronisation
*
*********************************************************************/

void init_context_synchro_nr(NR_DL_FRAME_PARMS *frame_parms_ue)
{
  /* initialise global buffers for synchronisation */
  init_context_pss_nr(frame_parms_ue);
  init_context_sss_nr(AMP);
}

/*******************************************************************
*
* NAME :         set_frame_context_pss_nr
*
* PARAMETERS :   configuration for UE with new FFT size
*
* RETURN :       0 if OK else error
*
* DESCRIPTION :  initialisation of UE contexts
*
*********************************************************************/

void set_frame_context_pss_nr(NR_DL_FRAME_PARMS *frame_parms_ue, int rate_change)
{
  /* set new value according to rate_change */
  frame_parms_ue->ofdm_symbol_size = (frame_parms_ue->ofdm_symbol_size / rate_change);
  frame_parms_ue->samples_per_subframe = (frame_parms_ue->samples_per_subframe / rate_change);

  /* pss reference have to be rebuild with new parameters ie ofdm symbol size */
  init_context_synchro_nr(frame_parms_ue);

#ifdef SYNCHRO_DECIMAT
  set_pss_nr(frame_parms_ue->ofdm_symbol_size);
#endif
}

/*******************************************************************
*
* NAME :         restore_frame_context_pss_nr
*
* PARAMETERS :   configuration for UE and eNB with new FFT size
*
* RETURN :       0 if OK else error
*
* DESCRIPTION :  initialisation of UE and eNode contexts
*
*********************************************************************/

void restore_frame_context_pss_nr(NR_DL_FRAME_PARMS *frame_parms_ue, int rate_change)
{
  frame_parms_ue->ofdm_symbol_size = frame_parms_ue->ofdm_symbol_size * rate_change;
  frame_parms_ue->samples_per_subframe = frame_parms_ue->samples_per_subframe * rate_change;

  /* pss reference have to be rebuild with new parameters ie ofdm symbol size */
  init_context_synchro_nr(frame_parms_ue);
#ifdef SYNCHRO_DECIMAT
  set_pss_nr(frame_parms_ue->ofdm_symbol_size);
#endif
}

/********************************************************************
*
* NAME :         decimation_synchro_nr
*
* INPUT :        UE context
*                for first and second pss sequence
*                - position of pss in the received UE buffer
*                - number of pss sequence
*
* RETURN :      0 if OK else error
*
* DESCRIPTION :  detect pss sequences in the received UE buffer
*
********************************************************************/

void decimation_synchro_nr(PHY_VARS_NR_UE *PHY_vars_UE, int rate_change, int **rxdata)
{
  NR_DL_FRAME_PARMS *frame_parms = &(PHY_vars_UE->frame_parms);
  int samples_for_frame = 2*frame_parms->samples_per_frame;

  start_meas(&generic_time[TIME_RATE_CHANGE]);
/* build with cic filter does not work properly. Performances are significantly deteriorated */
#ifdef CIC_DECIMATOR

  cic_decimator((int16_t *)&(PHY_vars_UE->common_vars.rxdata[0][0]), (int16_t *)&(rxdata[0][0]),
                            samples_for_frame, rate_change, CIC_FILTER_STAGE_NUMBER, 0, FIR_RATE_CHANGE);
#else

  fir_decimator((int16_t *)&(PHY_vars_UE->common_vars.rxdata[0][0]), (int16_t *)&(rxdata[0][0]),
                            samples_for_frame, rate_change, 0);

#endif

  set_frame_context_pss_nr(frame_parms, rate_change);

#if TEST_SYNCHRO_TIMING_PSS

  stop_meas(&generic_time[TIME_RATE_CHANGE]);

  printf("Rate change execution duration %5.2f \n", generic_time[TIME_RATE_CHANGE].p_time/(cpuf*1000.0));

#endif
}

/*******************************************************************
*
* NAME :         pss_synchro_nr
*
* PARAMETERS :   int rate_change
*
* RETURN :       position of detected pss
*
* DESCRIPTION :  pss search can be done with sampling decimation.*
*
*********************************************************************/

int pss_synchro_nr(const c16_t **rxdata,
                   const NR_DL_FRAME_PARMS *frame_parms,
                   const c16_t pssTime[NUMBER_PSS_SEQUENCE][frame_parms->ofdm_symbol_size],
                   int is,
                   bool fo_flag,
                   int target_Nid_cell,
                   int *nid2,
                   int *f_off,
                   int *pssPeak,
                   int *pssAvg)
{
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PSS_SYNCHRO_NR, VCD_FUNCTION_IN);
#ifdef DBG_PSS_NR

  LOG_M("rxdata0_rand.m","rxd0_rand", &PHY_vars_UE->common_vars.rxdata[0][0], frame_parms->samples_per_frame, 1, 1);

#endif

#ifdef DBG_PSS_NR

  LOG_M("rxdata0_des.m","rxd0_des", &rxdata[0][0], frame_parms->samples_per_frame,1,1);

#endif

  start_meas(&generic_time[TIME_PSS]);

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PSS_SEARCH_TIME_NR, VCD_FUNCTION_IN);

  int synchro_position =
      pss_search_time_nr(rxdata, frame_parms, pssTime, fo_flag, is, target_Nid_cell, nid2, f_off, pssPeak, pssAvg);

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PSS_SEARCH_TIME_NR, VCD_FUNCTION_OUT);

#if TEST_SYNCHRO_TIMING_PSS

  stop_meas(&generic_time[TIME_PSS]);

  int duration_ms = generic_time[TIME_PSS].p_time/(cpuf*1000.0);

  #ifndef NR_UNIT_TEST

  LOG_D(PHY,"PSS execution duration %4d microseconds \n", duration_ms);

  #endif

#endif

#ifdef SYNCHRO_DECIMAT

  if (rate_change != 1) {

    if (rxdata[0] != NULL) {

      for (int aa=0;aa<frame_parms->nb_antennas_rx;aa++) {
        free(rxdata[aa]);
      }

      free(rxdata);
    }

    restore_frame_context_pss_nr(frame_parms, rate_change);  
  }
#endif

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PSS_SYNCHRO_NR, VCD_FUNCTION_OUT);
  return synchro_position;
}

/*******************************************************************
*
* NAME :         pss_search_time_nr
*
* PARAMETERS :   received buffer in time domain
*                frame parameters
*
* RETURN :       position of detected pss
*
* DESCRIPTION :  Synchronisation on pss sequence is based on a time domain correlation between received samples and pss sequence
*                A maximum likelihood detector finds the timing offset (position) that corresponds to the maximum correlation
*                Length of received buffer should be a minimum of 2 frames (see TS 38.213 4.1 Cell search)
*                Search pss in the received buffer is done each 4 samples which ensures a memory alignment to 128 bits (32 bits x 4).
*                This is required by SIMD (single instruction Multiple Data) Extensions of Intel processors
*                Correlation computation is based on a a dot product which is realized thank to SIMS extensions
*
*                                    (x frames)
*     <--------------------------------------------------------------------------->
*
*
*     -----------------------------------------------------------------------------
*     |                      Received UE data buffer                              |
*     ----------------------------------------------------------------------------
*                -------------
*     <--------->|    pss    |
*      position  -------------
*                ^
*                |
*            peak position
*            given by maximum of correlation result
*            position matches beginning of first ofdm symbol of pss sequence
*
*     Remark: memory position should be aligned on a multiple of 4 due to I & Q samples of int16
*             An OFDM symbol is composed of x number of received samples depending of Rf front end sample rate.
*
*     I & Q storage in memory
*
*             First samples       Second  samples
*     ------------------------- -------------------------  ...
*     |     I1     |     Q1    |     I2     |     Q2    |
*     ---------------------------------------------------  ...
*     ^    16  bits   16 bits  ^
*     |                        |
*     ---------------------------------------------------  ...
*     |         sample 1       |    sample   2          |
*    ----------------------------------------------------  ...
*     ^
*
*********************************************************************/

static int pss_search_time_nr(const c16_t **rxdata,
                              const NR_DL_FRAME_PARMS *frame_parms,
                              const c16_t pssTime[NUMBER_PSS_SEQUENCE][frame_parms->ofdm_symbol_size],
                              bool fo_flag,
                              int is,
                              int target_Nid_cell,
                              int *nid2,
                              int *f_off,
                              int *pssPeak,
                              int *pssAvg)
{
  unsigned int n, ar, peak_position, pss_source;
  int64_t peak_value;
  int64_t avg[NUMBER_PSS_SEQUENCE] = {0};
  double ffo_est = 0;

  // performing the correlation on a frame length plus one symbol for the first of the two frame
  // to take into account the possibility of PSS in between the two frames 
  unsigned int length;
  if (is==0)
    length = frame_parms->samples_per_frame + (2*frame_parms->ofdm_symbol_size);
  else
    length = frame_parms->samples_per_frame;


  AssertFatal(length>0,"illegal length %d\n",length);
  peak_value = 0;
  peak_position = 0;
  pss_source = 0;

  int maxval=0;
  int max_size = get_softmodem_params()->sl_mode == 0 ?  NUMBER_PSS_SEQUENCE : NUMBER_PSS_SEQUENCE_SL;
  for (int j = 0; j < max_size; j++)
    for (int i = 0; i < frame_parms->ofdm_symbol_size; i++) {
      maxval = max(maxval, abs(pssTime[j][i].r));
      maxval = max(maxval, abs(pssTime[j][i].i));
    }
  int shift = log2_approx(maxval);//*(frame_parms->ofdm_symbol_size+frame_parms->nb_prefix_samples)*2);

  /* Search pss in the received buffer each 4 samples which ensures a memory alignment on 128 bits (32 bits x 4 ) */
  /* This is required by SIMD (single instruction Multiple Data) Extensions of Intel processors. */
  /* Correlation computation is based on a a dot product which is realized thank to SIMS extensions */

  uint16_t pss_index_start = 0;
  uint16_t pss_index_end = get_softmodem_params()->sl_mode == 0 ? NUMBER_PSS_SEQUENCE : NUMBER_PSS_SEQUENCE_SL;
  if (target_Nid_cell != -1) {
    pss_index_start = GET_NID2(target_Nid_cell);
    pss_index_end = pss_index_start + 1;
  }

  for (int pss_index = pss_index_start; pss_index < pss_index_end; pss_index++) {
    for (n = 0; n < length; n += 4) { //

      int64_t pss_corr_ue=0;
      /* calculate dot product of primary_synchro_time_nr and rxdata[ar][n]
       * (ar=0..nb_ant_rx) and store the sum in temp[n]; */
      for (ar=0; ar<frame_parms->nb_antennas_rx; ar++) {

        /* perform correlation of rx data and pss sequence ie it is a dot product */
        const c32_t result = dot_product(pssTime[pss_index],
                                         &(rxdata[ar][n + is * frame_parms->samples_per_frame]),
                                         frame_parms->ofdm_symbol_size,
                                         shift);
        const c64_t r64 = {.r = result.r, .i = result.i};
        pss_corr_ue += squaredMod(r64);
        //((short*)pss_corr_ue[pss_index])[2*n] += ((short*) &result)[0];   /* real part */
        //((short*)pss_corr_ue[pss_index])[2*n+1] += ((short*) &result)[1]; /* imaginary part */
        //((short*)&synchro_out)[0] += ((int*) &result)[0];               /* real part */
        //((short*)&synchro_out)[1] += ((int*) &result)[1];               /* imaginary part */

      }
      
      /* calculate the absolute value of sync_corr[n] */
      avg[pss_index]+=pss_corr_ue;
      if (pss_corr_ue > peak_value) {
        peak_value = pss_corr_ue;
        peak_position = n;
        pss_source = pss_index;
        
#ifdef DEBUG_PSS_NR
        printf("pss_index %d: n %6u peak_value %15llu\n", pss_index, n, (unsigned long long)pss_corr_ue[n]);
#endif
      }
    }
  }
  
  if (fo_flag){

	  // fractional frequency offset computation according to Cross-correlation Synchronization Algorithm Using PSS
	  // Shoujun Huang, Yongtao Su, Ying He and Shan Tang, "Joint time and frequency offset estimation in LTE downlink," 7th International Conference on Communications and Networking in China, 2012.

    // Computing cross-correlation at peak on half the symbol size for first half of data
    c32_t r1 = dot_product(pssTime[pss_source],
                           &(rxdata[0][peak_position + is * frame_parms->samples_per_frame]),
                           frame_parms->ofdm_symbol_size >> 1,
                           shift);
    // Computing cross-correlation at peak on half the symbol size for data shifted by half symbol size
    // as it is real and complex it is necessary to shift by a value equal to symbol size to obtain such shift
    c32_t r2 = dot_product(pssTime[pss_source] + (frame_parms->ofdm_symbol_size >> 1),
                           &(rxdata[0][peak_position + is * frame_parms->samples_per_frame]) + (frame_parms->ofdm_symbol_size >> 1),
                           frame_parms->ofdm_symbol_size >> 1,
                           shift);
    cd_t r1d = {r1.r, r1.i}, r2d = {r2.r, r2.i};
    // estimation of fractional frequency offset: angle[(result1)'*(result2)]/pi
    ffo_est = atan2(r1d.r * r2d.i - r2d.r * r1d.i, r1d.r * r2d.r + r1d.i * r2d.i) / M_PI;

#ifdef DBG_PSS_NR
	  printf("ffo %lf\n",ffo_est);
#endif
  }

  // computing absolute value of frequency offset
  *f_off = ffo_est*frame_parms->subcarrier_spacing;

  for (int pss_index = pss_index_start; pss_index < pss_index_end; pss_index++)
    avg[pss_index] /= (length / 4);

  *nid2 = pss_source;
  *pssPeak = dB_fixed64(peak_value);
  *pssAvg = dB_fixed64(avg[pss_source]);

  LOG_D(PHY,
        "[UE] nr_synchro_time: Sync source (nid2) = %d, Peak found at pos %d, val = %ld (%d dB power over signal avg %d dB), ffo "
        "%lf\n",
        pss_source,
        peak_position,
        peak_value,
        dB_fixed64(peak_value),
        dB_fixed64(avg[pss_source]),
        ffo_est);

  if (peak_value < 5*avg[pss_source])
    return(-1);


#ifdef DBG_PSS_NR

  static int debug_cnt = 0;

  if (debug_cnt == 0) {
    if (is)
      LOG_M("rxdata1.m","rxd0",rxdata[frame_parms->samples_per_frame],length,1,1); 
    else
      LOG_M("rxdata0.m","rxd0",rxdata[0],length,1,1);
  } else {
    debug_cnt++;
  }

#endif

  return peak_position;
}

void sl_generate_pss(SL_NR_UE_INIT_PARAMS_t *sl_init_params, uint8_t n_sl_id2, uint16_t scaling)
{
  int i = 0, m = 0;
  int16_t x[SL_NR_PSS_SEQUENCE_LENGTH];
  const int x_initial[7] = {0, 1, 1, 0, 1, 1, 1};
  int16_t *sl_pss = sl_init_params->sl_pss[n_sl_id2];
  int16_t *sl_pss_for_sync = sl_init_params->sl_pss_for_sync[n_sl_id2];

  LOG_D(PHY, "SIDELINK PSBCH INIT: PSS Generation with N_SL_id2:%d\n", n_sl_id2);

#ifdef SL_DEBUG_INIT
  printf("SIDELINK: PSS Generation with N_SL_id2:%d\n", n_sl_id2);
#endif

  /// Sequence generation
  for (i = 0; i < 7; i++)
    x[i] = x_initial[i];

  for (i = 0; i < (SL_NR_PSS_SEQUENCE_LENGTH - 7); i++) {
    x[i + 7] = (x[i + 4] + x[i]) % 2;
  }

  for (i = 0; i < SL_NR_PSS_SEQUENCE_LENGTH; i++) {
    m = (i + 22 + 43 * n_sl_id2) % SL_NR_PSS_SEQUENCE_LENGTH;
    sl_pss_for_sync[i] = (1 - 2 * x[m]);
    sl_pss[i] = sl_pss_for_sync[i] * scaling;

#ifdef SL_DEBUG_INIT_DATA
    printf("m:%d, sl_pss[%d]:%d\n", m, i, sl_pss[i]);
#endif
  }

#ifdef SL_DUMP_INIT_SAMPLES
  LOG_M("sl_pss_seq.m", "sl_pss", (void *)sl_pss, SL_NR_PSS_SEQUENCE_LENGTH, 1, 0);
#endif
}

// This cannot be done at init time as ofdm symbol size, ssb start subcarrier depends on configuration
// done at SLSS read time.
void sl_generate_pss_ifft_samples(sl_nr_ue_phy_params_t *sl_ue_params, SL_NR_UE_INIT_PARAMS_t *sl_init_params)
{
  uint8_t id2 = 0;
  int16_t *sl_pss = NULL;
  NR_DL_FRAME_PARMS *sl_fp = &sl_ue_params->sl_frame_params;
  int16_t scaling_factor = AMP;

  int32_t *pss_T = NULL;

  uint16_t k = 0;

  c16_t pss_F[sl_fp->ofdm_symbol_size]; // IQ samples in freq domain

  LOG_I(PHY, "SIDELINK INIT: Generation of PSS time domain samples. scaling_factor:%d\n", scaling_factor);

  for (id2 = 0; id2 < SL_NR_NUM_IDs_IN_PSS; id2++) {
    k = sl_fp->first_carrier_offset + sl_fp->ssb_start_subcarrier + 2; // PSS in from REs 2-129
    if (k >= sl_fp->ofdm_symbol_size)
      k -= sl_fp->ofdm_symbol_size;

    pss_T = &sl_init_params->sl_pss_for_correlation[id2][0];
    sl_pss = sl_init_params->sl_pss[id2];

    memset(pss_T, 0, sl_fp->ofdm_symbol_size * sizeof(pss_T[0]));
    memset(pss_F, 0, sl_fp->ofdm_symbol_size * sizeof(c16_t));

    for (int i = 0; i < SL_NR_PSS_SEQUENCE_LENGTH; i++) {
      pss_F[k].r = (sl_pss[i] * scaling_factor) >> 15;
      // pss_F[2*k] = (sl_pss[i]/23170) * 4192;
      // pss_F[2*k+1] = 0;

#ifdef SL_DEBUG_INIT_DATA
      printf("id:%d, k:%d, pss_F[%d]:%d, sl_pss[%d]:%d\n", id2, k, 2 * k, pss_F[2 * k], i, sl_pss[i]);
#endif

      k++;
      if (k == sl_fp->ofdm_symbol_size)
        k = 0;
    }

    idft((int16_t)get_idft(sl_fp->ofdm_symbol_size),
         (int16_t *)&pss_F[0], /* complex input */
         (int16_t *)&pss_T[0], /* complex output */
         1); /* scaling factor */
  }

#ifdef SL_DUMP_PSBCH_TX_SAMPLES
  LOG_M("sl_pss_TD_id0.m", "pss_TD_0", (void *)sl_init_params->sl_pss_for_correlation[0], sl_fp->ofdm_symbol_size, 1, 1);
  LOG_M("sl_pss_TD_id1.m", "pss_TD_1", (void *)sl_init_params->sl_pss_for_correlation[1], sl_fp->ofdm_symbol_size, 1, 1);
#endif
}
