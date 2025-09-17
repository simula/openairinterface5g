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

/*! \file nr_transport_proto_ue.h
 * \brief Function prototypes for PHY physical/transport channel processing and generation V8.6 2009-03
 * \author R. Knopp, F. Kaltenberger
 * \date 2011
 * \version 0.1
 * \company Eurecom
 * \email: knopp@eurecom.fr
 * \note
 * \warning
 */
#ifndef __NR_TRANSPORT_PROTO_UE__H__
#define __NR_TRANSPORT_PROTO_UE__H__
#include "PHY/defs_nr_UE.h"
#include "SCHED_NR_UE/defs.h"
#include "PHY/NR_TRANSPORT/nr_transport_common_proto.h"
#include <math.h>
#include "nfapi_interface.h"
#include <openair1/PHY/LTE_TRANSPORT/transport_proto.h>

#define NR_PUSCH_x 2 // UCI placeholder bit TS 38.212 V15.4.0 subclause 5.3.3.1
#define NR_PUSCH_y 3 // UCI placeholder bit

// Specifies the data that should be copied to the scope during PDSCH RX
typedef struct pdsch_scope_req_s {
  bool copy_chanest_to_scope;
  bool copy_rxdataF_to_scope;
  size_t scope_rxdataF_offset;
} pdsch_scope_req_t;

// Functions below implement 36-211 and 36-212

/** @addtogroup _PHY_TRANSPORT_
 * @{
 */


/** \brief This function initialises structures for DLSCH at UE
*/
void nr_ue_dlsch_init(NR_UE_DLSCH_t *dlsch_list, int num_dlsch, uint8_t max_ldpc_iterations);

void nr_conjch0_mult_ch1(c16_t *ch0, c16_t *ch1, c16_t *ch0conj_ch1, unsigned short nb_rb, unsigned char output_shift0);

/** \brief This is the alternative top-level entry point for DLSCH decoding in UE.
    It handles all the HARQ processes in only one call. The routine first
    computes the segmentation information and then call LDPC decoder on the
    received LLRs computed by dlsch_demodulation.
    It stops after either unsuccesful decoding of at least
    one segment or correct decoding of all segments. Only the segment CRCs are checked for the moment, the
    overall CRC is ignored. Finally transport block reassembly is performed.
    @param[in] phy_vars_ue Pointer to ue variables
    @param[in] proc
    @param[in] dlsch_llr Pointers to LLR values computed by dlsch_demodulation
    @param[in] b
    @param[in] G array of Gs
    @param[in] nb_dlsch number of active downlink shared channels
    @param[in] DLSCH_ids array of active downlink shared channels
    @returns 0 on success, 1 on unsuccessful decoding
*/
void nr_dlsch_decoding(PHY_VARS_NR_UE *phy_vars_ue,
                       const UE_nr_rxtx_proc_t *proc,
                       NR_UE_DLSCH_t *dlsch,
                       short **dlsch_llr,
                       uint8_t **b,
                       int *G,
                       int nb_dlsch,
                       uint8_t *DLSCH_ids);

/** \brief This is the alternative top-level entry point for ULSCH encoding in UE.
    It handles all the HARQ processes in only one call. The routine first
    computes the segmentation information, followed by LDPC encoding algorithm of the
    Transport Block.
    @param[in] phy_vars_ue pointer to ue variables
    @param[in] ulsch Pointer to ULSCH descriptor
    @param[in] frame frame index
    @param[in] slot slot index
    @param[in] G array of Gs
    @param[in] nb_ulsch number of uplink shared channels
    @param[in] ULSCH_ids array of uplink shared channel ids
    @returns 0 on success, -1 on unsuccessful decoding
*/
int nr_ulsch_encoding(PHY_VARS_NR_UE *ue,
                      NR_UE_ULSCH_t *ulsch,
                      const uint32_t frame,
                      const uint8_t slot,
                      unsigned int *G,
                      int nb_ulsch,
                      uint8_t *ULSCH_ids,
                      uint16_t number_dmrs_symbols);

/*! \brief Perform PUSCH scrambling. TS 38.211 V15.4.0 subclause 6.3.1.1
  @param[in] in Pointer to input bits
  @param[in] size of input bits
  @param[in] Nid cell id
  @param[in] n_RNTI CRNTI
  @param[in] uci_on_pusch whether UCI placeholder bits need to be scrambled (true -> no optimized scrambling)
  @param[out] out the scrambled bits
*/
void nr_pusch_codeword_scrambling(uint8_t *in,
                                  uint32_t size,
                                  uint32_t Nid,
                                  uint32_t n_RNTI,
                                  bool uci_on_pusch,
                                  uint32_t* out);


/** \brief Alternative entry point to UE uplink shared channels procedures.
    It handles all the HARQ processes in only one call.
    Performs the following functionalities:
    - encoding
    - scrambling
    - modulation
    - transform precoding
    @param[in] UE pointer to ue variables
    @param[in] frame frame index
    @param[in] slot slot index
    @param[in] phy_data PHY layer informations
    @param[in] c16_t
*/
void nr_ue_ulsch_procedures(PHY_VARS_NR_UE *UE,
                            const uint32_t frame,
                            const uint8_t slot,
                            nr_phy_data_tx_t *phy_data,
                            c16_t **txdataF,
                            bool was_symbol_used[NR_NUMBER_OF_SYMBOLS_PER_SLOT]);

/** \brief This function does IFFT for PUSCH
*/

uint8_t nr_ue_pusch_common_procedures(PHY_VARS_NR_UE *UE,
                                      const uint8_t slot,
                                      const NR_DL_FRAME_PARMS *frame_parms,
                                      const uint8_t n_antenna_ports,
                                      c16_t **txdataF,
                                      c16_t **txdata,
                                      uint32_t linktype,
                                      bool was_symbol_used[NR_NUMBER_OF_SYMBOLS_PER_SLOT]);

void clean_UE_harq(PHY_VARS_NR_UE *UE);

void nr_dlsch_unscrambling(int16_t* llr,
			   uint32_t size,
			   uint8_t q,
			   uint32_t Nid,
			   uint32_t n_RNTI);

void nr_rx_pdcch(PHY_VARS_NR_UE *ue,
                 const UE_nr_rxtx_proc_t *proc,
                 int32_t pdcch_est_size,
                 c16_t pdcch_dl_ch_estimates[][pdcch_est_size],
                 c16_t *pdcch_e_rx,
                 fapi_nr_dl_config_dci_dl_pdu_rel15_t *rel15,
                 c16_t rxdataF[][ue->frame_parms.samples_per_slot_wCP]);

/*! \brief Performs detection of SSS to find cell ID and other framing parameters (FDD/TDD, normal/extended prefix)
  @param phy_vars_ue Pointer to UE variables
  @param tot_metric Pointer to variable containing maximum metric under framing hypothesis (to be compared to other hypotheses
  @param flip_max Pointer to variable indicating if start of frame is in second have of RX buffer (i.e. PSS/SSS is flipped)
  @param phase_max Pointer to variable (0 ... 6) containing rought phase offset between PSS and SSS (can be used for carrier
  frequency adjustment. 0 means -pi/3, 6 means pi/3.
  @returns 0 on success
*/
int rx_sss(PHY_VARS_NR_UE *phy_vars_ue,int32_t *tot_metric,uint8_t *flip_max,uint8_t *phase_max);

/*! \brief receiver for the PBCH
  \returns number of tx antennas or -1 if error
*/

int nr_rx_pbch(PHY_VARS_NR_UE *ue,
               const UE_nr_rxtx_proc_t *proc,
               bool is_synchronized,
               int estimateSz,
               struct complex16 dl_ch_estimates[][estimateSz],
               const NR_DL_FRAME_PARMS *frame_parms,
               uint8_t i_ssb,
               int ssb_start_subcarrier,
               int Nid_cell,
               fapiPbch_t *result,
               int *half_frame_bit,
               int *ssb_index,
               int *ret_symbol_offset,
               int rxdataFSize,
               const struct complex16 rxdataF[][rxdataFSize]);

double nr_ue_pbch_freq_offset(const NR_DL_FRAME_PARMS *frame_parms,
                              int estimateSz,
                              const c16_t dl_ch_estimates[][estimateSz]);

#ifndef modOrder
#define modOrder(I_MCS,I_TBS) ((I_MCS-I_TBS)*2+2) // Find modulation order from I_TBS and I_MCS
#endif

/*!
  \brief This function performs the initial cell search procedure - PSS detection, SSS detection and PBCH detection.  At the
  end, the basic frame parameters are known (Frame configuration - TDD/FDD and cyclic prefix length,
  N_RB_DL, PHICH_CONFIG and Nid_cell) and the UE can begin decoding PDCCH and DLSCH SI to retrieve the rest.  Once these
  parameters are know, the routine calls some basic initialization routines (cell-specific reference signals, etc.)
@param proc
  @param phy_vars_ue Pointer to UE variables
@param n_frames
  @param sa current running mode
*/
nr_initial_sync_t nr_initial_sync(UE_nr_rxtx_proc_t *proc,
                                  PHY_VARS_NR_UE *phy_vars_ue,
                                  int n_frames,
                                  int sa,
                                  nr_gscn_info_t gscnInfo[MAX_GSCN_BAND],
                                  int numGscn);

/*!
  \brief This function gets the carrier frequencies either from FP or command-line-set global variables, depending on the
  availability of the latter
  @param ue
  @param dl_Carrier Pointer to DL carrier to be set
  @param ul_Carrier Pointer to UL carrier to be set
*/
void nr_get_carrier_frequencies(PHY_VARS_NR_UE *ue,
                                uint64_t *dl_Carrier,
                                uint64_t *ul_Carrier);

/*!
  \brief This function gets the carrier frequencies either from FP or command-line-set global variables, depending on the availability of the latter
  @param ue         Pointer to PHY UE
  @param sl_Carrier Pointer to SL carrier to be set
*/
void nr_get_carrier_frequencies_sl(PHY_VARS_NR_UE *ue,
                                   uint64_t *sl_Carrier);

/*!
  \brief This function sets the OAI RF card rx/tx params
  @param openair0_cfg   Pointer OAI config for a specific card
  @param rx_gain_off    Rx gain offset
*/
void nr_rf_card_config_gain(openair0_config_t *openair0_cfg,
                            double rx_gain_off);

void nr_rf_card_config_freq(openair0_config_t *openair0_cfg,
                            uint64_t ul_Carrier,
                            uint64_t dl_Carrier,
                            int freq_offset);

void nr_sl_rf_card_config_freq(PHY_VARS_NR_UE *ue,
                               openair0_config_t *openair0_cfg,
                               int freq_offset);

void nr_dci_decoding_procedure(PHY_VARS_NR_UE *ue,
                               const UE_nr_rxtx_proc_t *proc,
                               c16_t *pdcch_e_rx,
                               fapi_nr_dci_indication_t *dci_ind,
                               fapi_nr_dl_config_dci_dl_pdu_rel15_t *rel15);

/** \brief This function is the top-level entry point to PDSCH demodulation, after frequency-domain transformation and channel
   estimation.  It performs
    - RB extraction (signal and channel estimates)
    - channel compensation (matched filtering)
    - RE extraction (pilot, PBCH, synch. signals)
    - antenna combining (MRC, Alamouti, cycling)
    - LLR computation
    This function supports TM1, 2, 3, 5, and 6.
    @param ue Pointer to PHY variables
    @param proc
    @prama dlsch
    @param symbol Symbol on which to act (within sub-frame)
    @param first_symbol_flag set to 1 on first DLSCH symbol
    @param harq_pid
    @param pdsch_est_size
    @param dl_ch_estimates
    @param llr
    @param dl_valid_re
    @param rxdataF
    @param llr_offset
    @param log2_maxhrx_size_symbol
    @param rx_size_symbol
    @param nbRx
    @param rxdataF_comp
    @param ptrs_phase_per_slot
    @param ptrs_re_per_slot
*/
int nr_rx_pdsch(PHY_VARS_NR_UE *ue,
                const UE_nr_rxtx_proc_t *proc,
                NR_UE_DLSCH_t dlsch[2],
                unsigned char symbol,
                bool first_symbol_flag,
                unsigned char harq_pid,
                uint32_t pdsch_est_size,
                int32_t dl_ch_estimates[][pdsch_est_size],
                int layer_llr_size,
                int16_t layer_llr[][layer_llr_size],
                int16_t *llr[2],
                uint32_t dl_valid_re[NR_SYMBOLS_PER_SLOT],
                c16_t rxdataF[][ue->frame_parms.samples_per_slot_wCP],
                uint32_t llr_offset[NR_SYMBOLS_PER_SLOT],
                int32_t *log2_maxh,
                int rx_size_symbol,
                int nbRx,
                int32_t rxdataF_comp[][nbRx][rx_size_symbol * NR_SYMBOLS_PER_SLOT],
                c16_t ptrs_phase_per_slot[][NR_SYMBOLS_PER_SLOT],
                int32_t ptrs_re_per_slot[][NR_SYMBOLS_PER_SLOT],
                int G,
                uint32_t nvar,
                pdsch_scope_req_t *scope_req);

int32_t generate_nr_prach(PHY_VARS_NR_UE *ue, uint8_t gNB_id, int frame, uint8_t slot, c16_t **txData);

void dump_nrdlsch(PHY_VARS_NR_UE *ue,uint8_t gNB_id,uint8_t nr_slot_rx,unsigned int *coded_bits_per_codeword,int round,  unsigned char harq_pid);
void nr_a_sum_b(c16_t *input_x, c16_t *input_y, unsigned short nb_rb);

int nr_rx_psbch(PHY_VARS_NR_UE *ue,
                const UE_nr_rxtx_proc_t *proc,
                int estimateSz,
                struct complex16 dl_ch_estimates[][estimateSz],
                NR_DL_FRAME_PARMS *frame_parms,
                uint8_t *decoded_output,
                c16_t rxdataF[][frame_parms->samples_per_slot_wCP],
                uint16_t slss_id);

void nr_tx_psbch(PHY_VARS_NR_UE *UE, uint32_t frame_tx, uint32_t slot_tx, sl_nr_tx_config_psbch_pdu_t *psbch_vars, c16_t **txdataF);

nr_initial_sync_t sl_nr_slss_search(PHY_VARS_NR_UE *UE, UE_nr_rxtx_proc_t *proc, int num_frames);

// Reuse already existing PBCH functions
void nr_pbch_channel_compensation(struct complex16 rxdataF_ext[][PBCH_MAX_RE_PER_SYMBOL],
                                  struct complex16 dl_ch_estimates_ext[][PBCH_MAX_RE_PER_SYMBOL],
                                  int nb_re,
                                  struct complex16 rxdataF_comp[][PBCH_MAX_RE_PER_SYMBOL],
                                  const NR_DL_FRAME_PARMS *frame_parms,
                                  uint8_t output_shift);
void nr_pbch_unscrambling(int16_t *demod_pbch_e,
                          uint16_t Nid,
                          uint8_t nushift,
                          uint16_t M,
                          uint16_t length,
                          uint8_t bitwise,
                          uint32_t unscrambling_mask,
                          uint32_t pbch_a_prime,
                          uint32_t *pbch_a_interleaved);
void nr_pbch_quantize(int16_t *pbch_llr8, int16_t *pbch_llr, uint16_t len);
/**@}*/
#endif

