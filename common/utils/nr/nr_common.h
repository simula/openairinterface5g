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

/* \file config_ue.c
 * \brief common utility functions for NR (gNB and UE)
 * \author R. Knopp,
 * \date 2019
 * \version 0.1
 * \company Eurecom
 * \email: knopp@eurecom.fr
 * \note
 * \warning
 */

#ifndef __COMMON_UTILS_NR_NR_COMMON__H__
#define __COMMON_UTILS_NR_NR_COMMON__H__

#include <stdint.h>
#include <stdlib.h>
#include "assertions.h"
#include "PHY/defs_common.h"

#define NR_MAX_PDSCH_TBS 3824
#define MAX_NUM_BEAM_PERIODS 4
#define MAX_BWP_SIZE 275
#define NR_MAX_NUM_BWP 4
#define NR_MAX_HARQ_PROCESSES 32
#define NR_NB_REG_PER_CCE 6
#define NR_NB_SC_PER_RB 12
#define NR_MAX_NUM_LCID 32
#define NR_MAX_NUM_QFI 64
#define RNTI_NAMES /* see 38.321  Table 7.1-2  RNTI usage */      \
  R(TYPE_C_RNTI_) /* Cell RNTI */                                  \
  R(TYPE_CS_RNTI_) /* Configured Scheduling RNTI */                \
  R(TYPE_TC_RNTI_) /* Temporary C-RNTI */                          \
  R(TYPE_P_RNTI_) /* Paging RNTI */                                \
  R(TYPE_SI_RNTI_) /* System information RNTI */                   \
  R(TYPE_RA_RNTI_) /* Random Access RNTI */                        \
  R(TYPE_MSGB_RNTI_) /* Random Access MsgB RNTI */                 \
  R(TYPE_SP_CSI_RNTI_) /* Semipersistent CSI reporting on PUSCH */ \
  R(TYPE_SFI_RNTI_) /* Slot Format Indication on the given cell */ \
  R(TYPE_INT_RNTI_) /* Indication pre-emption in DL */            \
  R(TYPE_TPC_PUSCH_RNTI_) /* PUSCH power control */               \
  R(TYPE_TPC_PUCCH_RNTI_) /* PUCCH power control */               \
  R(TYPE_TPC_SRS_RNTI_)                                           \
  R(TYPE_MCS_C_RNTI_)

#define R(k) k ,
typedef enum { RNTI_NAMES } nr_rnti_type_t;
#undef R

#define R(k) \
  case k:       \
    return #k;
static inline const char *rnti_types(nr_rnti_type_t rr)
{
  switch (rr) {
    RNTI_NAMES
    default:
      return "Not existing RNTI type";
  }
}
#undef R

#define MU_SCS(m) (15 << m)
#define MAX_GSCN_BAND 620 // n78 has the highest GSCN range of 619

#define NR_MAX_NB_LAYERS 4 // 8

// Since the IQ samples are represented by SQ15 R+I (see https://en.wikipedia.org/wiki/Q_(number_format)) we need to compensate when
// calcualting signal energy. Instead of shifting each sample right by 15, we can normalize the result in dB scale once its
// calcualted. Signal energy is calculated using RMS^2, where each sample is squared before taking the average of the sum, therefore
// the total shift is 2 * 15, in dB scale thats 10log10(2^(15*2))
#define SQ15_SQUARED_NORM_FACTOR_DB 90.3089986992

typedef struct nr_bandentry_s {
  int16_t band;
  uint64_t ul_min;
  uint64_t ul_max;
  uint64_t dl_min;
  uint64_t dl_max;
  uint64_t step_size;
  uint64_t N_OFFs_DL;
  uint8_t deltaf_raster;
} nr_bandentry_t;

typedef struct {
  int band;
  int scs_index;
  int first_gscn;
  int step_gscn;
  int last_gscn;
} sync_raster_t;

typedef struct {
  int gscn;
  double ssRef;
  int ssbFirstSC;
} nr_gscn_info_t;

typedef enum frequency_range_e {
  FR1 = 0,
  FR2
} frequency_range_t;

typedef struct {
  /// Time shift in number of samples estimated based on DMRS-PDSCH/PUSCH
  int est_delay;
  /// Max position in OFDM symbol related to time shift estimation based on DMRS-PDSCH/PUSCH
  int delay_max_pos;
  /// Max value related to time shift estimation based on DMRS-PDSCH/PUSCH
  int delay_max_val;
} delay_t;

typedef struct {
  bool active;
  uint32_t counter;
  uint32_t target;
  uint32_t step;
} NR_timer_t;

/**
 * @brief To start a timer
 * @param timer Timer to be started
 */
void nr_timer_start(NR_timer_t *timer);
/**
 * @brief To stop a timer
 * @param timer Timer to stopped
 */
void nr_timer_stop(NR_timer_t *timer);
/**
 * @brief If active, it increases timer counter by an amout of units equal to step. It stops timer if expired
 * @param timer Timer to be handled
 * @return Indication if the timer is expired or not
 */
bool nr_timer_tick(NR_timer_t *timer);
/**
 * @brief To setup a timer
 * @param timer Timer to setup
 * @param target Target value for timer (when reached, timer is considered expired)
 * @param step Amount of units to add to timer counter every tick
 */
void nr_timer_setup(NR_timer_t *timer, const uint32_t target, const uint32_t step);
/**
 * @brief To check if a timer is expired
 * @param timer Timer to be checked
 * @return Indication if the timer is expired or not
 */
bool nr_timer_expired(const NR_timer_t *timer);
/**
 * @brief To check if a timer is active
 * @param timer Timer to be checked
 * @return Indication if the timer is active or not
 */
bool nr_timer_is_active(const NR_timer_t *timer);
/**
 * @brief To return how much time has passed since start of timer
 * @param timer Timer to be checked
 * @return Time passed since start of timer
 */
uint32_t nr_timer_elapsed_time(const NR_timer_t *timer);

int set_default_nta_offset(frequency_range_t freq_range, uint32_t samples_per_subframe);
extern const nr_bandentry_t nr_bandtable[];

static inline int get_num_dmrs(uint16_t dmrs_mask )
{
  int num_dmrs=0;
  for (int i=0;i<16;i++) num_dmrs+=((dmrs_mask>>i)&1);
  return(num_dmrs);
}

static inline int count_bits(uint8_t *arr, int sz)
{
  AssertFatal(sz % sizeof(int) == 0, "to implement if needed\n");
  int ret = 0;
  for (uint *ptr = (uint *)arr; (uint8_t *)ptr < arr + sz; ptr++)
    ret += __builtin_popcount(*ptr);
  return ret;
}

static __attribute__((always_inline)) inline int count_bits64(uint64_t v)
{
  return __builtin_popcountll(v);
}

uint64_t reverse_bits(uint64_t in, int n_bits);
void reverse_bits_u8(uint8_t const* in, size_t sz, uint8_t* out);

uint64_t from_nrarfcn(int nr_bandP, uint8_t scs_index, uint32_t dl_nrarfcn);
uint32_t to_nrarfcn(int nr_bandP, uint64_t dl_CarrierFreq, uint8_t scs_index, uint32_t bw);

int get_first_ul_slot(int nrofDownlinkSlots, int nrofDownlinkSymbols, int nrofUplinkSymbols);
int cce_to_reg_interleaving(const int R, int k, int n_shift, const int C, int L, const int N_regs);
int get_SLIV(uint8_t S, uint8_t L);
void get_coreset_rballoc(uint8_t *FreqDomainResource,int *n_rb,int *rb_offset);
int get_nr_table_idx(int nr_bandP, uint8_t scs_index);
int32_t get_delta_duplex(int nr_bandP, uint8_t scs_index);
frame_type_t get_frame_type(uint16_t nr_bandP, uint8_t scs_index);
uint16_t get_band(uint64_t downlink_frequency, int32_t delta_duplex);
int NRRIV2BW(int locationAndBandwidth,int N_RB);
int NRRIV2PRBOFFSET(int locationAndBandwidth,int N_RB);
int PRBalloc_to_locationandbandwidth0(int NPRB,int RBstart,int BWPsize);
int PRBalloc_to_locationandbandwidth(int NPRB,int RBstart);
int get_subband_size(int NPRB,int size);
void SLIV2SL(int SLIV,int *S,int *L);
int get_dmrs_port(int nl, uint16_t dmrs_ports);
uint16_t SL_to_bitmap(int startSymbolIndex, int nrOfSymbols);
int get_nb_periods_per_frame(uint8_t tdd_period);
long rrc_get_max_nr_csrs(const int max_rbs, long b_SRS);
bool compare_relative_ul_channel_bw(int nr_band, int scs, int channel_bandwidth, frame_type_t frame_type);
int get_supported_bw_mhz(frequency_range_t frequency_range, int bw_index);
int get_supported_band_index(int scs, frequency_range_t freq_range, int n_rbs);
void get_samplerate_and_bw(int mu,
                           int n_rb,
                           int8_t threequarter_fs,
                           double *sample_rate,
                           unsigned int *samples_per_frame,
                           double *tx_bw,
                           double *rx_bw);
uint32_t get_ssb_offset_to_pointA(uint32_t absoluteFrequencySSB,
                                  uint32_t absoluteFrequencyPointA,
                                  int ssbSubcarrierSpacing,
                                  int frequency_range);
int get_ssb_subcarrier_offset(uint32_t absoluteFrequencySSB, uint32_t absoluteFrequencyPointA, int scs);
int get_delay_idx(int delay, int max_delay_comp);

int get_scan_ssb_first_sc(const double fc,
                          const int nbRB,
                          const int nrBand,
                          const int mu,
                          nr_gscn_info_t ssbStartSC[MAX_GSCN_BAND]);

void check_ssb_raster(uint64_t freq, int band, int scs);
int get_smallest_supported_bandwidth_index(int scs, frequency_range_t frequency_range, int n_rbs);
unsigned short get_m_srs(int c_srs, int b_srs);
unsigned short get_N_b_srs(int c_srs, int b_srs);

#define CEILIDIV(a,b) ((a+b-1)/b)
#define ROUNDIDIV(a,b) (((a<<1)+b)/(b<<1))

// Align up to a multiple of 16
#define ALIGN_UP_16(a) ((a + 15) & ~15)

#ifdef __cplusplus
#ifdef min
#undef min
#undef max
#endif
#else
#define max(a,b) cmax(a,b)
#define min(a,b) cmin(a,b)
#endif

#endif
