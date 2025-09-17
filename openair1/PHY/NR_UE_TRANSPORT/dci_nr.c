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

/*! \file dci_nr.c
 * \brief Implements PDCCH physical channel TX/RX procedures (36.211) and DCI encoding/decoding (36.212/36.213). Current LTE
 * compliance V8.6 2009-03. \author R. Knopp, A. Mico Pereperez \date 2018 \version 0.1 \company Eurecom \email: knopp@eurecom.fr
 * \note
 * \warning
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "executables/softmodem-common.h"
#include "nr_transport_proto_ue.h"
#include "PHY/CODING/nrPolar_tools/nr_polar_dci_defs.h"
#include "PHY/phy_extern.h"
#include "PHY/CODING/coding_extern.h"
#include "PHY/nr_phy_common/inc/nr_phy_common.h"
#include "PHY/sse_intrin.h"
#include "common/utils/nr/nr_common.h"
#include <openair1/PHY/TOOLS/phy_scope_interface.h>
#include "openair1/PHY/NR_REFSIG/nr_refsig_common.h"

#include "assertions.h"
#include "T.h"

// #define NR_PDCCH_DCI_DEBUG // activates NR_PDCCH_DCI_DEBUG logs
#ifdef NR_PDCCH_DCI_DEBUG
#define LOG_DDD(a, ...) printf("<-NR_PDCCH_DCI_DEBUG (%s)-> " a, __func__, ##__VA_ARGS__ )
#define LOG_DSYMB(b)                                                               \
  LOG_DDD("RB[c_rb %d] \t RE[re %d] => rxF_ext[%d]=(%d,%d)\t rxF[%d]=(%d,%d)\n" b, \
          c_rb,                                                                    \
          i,                                                                       \
          j,                                                                       \
          rxF_ext[j].r,                                                            \
          rxF_ext[j].i,                                                            \
          i,                                                                       \
          rxF[i].r,                                                                \
          rxF[i].i)
#else
#define LOG_DDD(a...)
#define LOG_DSYMB(a...)
#endif

#define NR_NBR_CORESET_ACT_BWP 3 // The number of CoreSets per BWP is limited to 3 (including initial CORESET: ControlResourceId 0)
#define NR_NBR_SEARCHSPACE_ACT_BWP \
  10 // The number of SearSpaces per BWP is limited to 10 (including initial SEARCHSPACE: SearchSpaceId 0)

#define RE_PER_RB 12
// after removing the 3 DMRS RE, the RB contains 9 RE with PDCCH
#define RE_PER_RB_OUT_DMRS 9

static void nr_pdcch_demapping_deinterleaving(uint32_t coreset_nbr_rb,
                                              c16_t llr[][coreset_nbr_rb * RE_PER_RB_OUT_DMRS],
                                              c16_t *e_rx,
                                              uint8_t coreset_time_dur,
                                              uint8_t reg_bundle_size_L,
                                              uint8_t coreset_interleaver_size_R,
                                              uint8_t n_shift,
                                              uint8_t number_of_candidates,
                                              uint16_t *CCE,
                                              uint8_t *L)
{
  /*
   * This function will do demapping and deinterleaving from llr containing demodulated symbols
   * Demapping will regroup in REG and bundles
   * Deinterleaving will order the bundles
   *
   * In the following example we can see the process. The llr contains the demodulated IQs, but they are not ordered from
   REG 0,1,2,..
   * In e_rx (z) we will order the REG ids and group them into bundles.
   * Then we will put the bundles in the correct order as indicated in subclause 7.3.2.2
   *
   llr --------------------------> e_rx (z) ----> e_rx (z)
   |   ...
   |   ...
   |   REG 26
   symbol 2    |   ...
   |   ...
   |   REG 5
   |   REG 2

   |   ...
   |   ...
   |   REG 25
   symbol 1    |   ...
   |   ...
   |   REG 4
   |   REG 1

   |   ...
   |   ...                           ...              ...
   |   REG 24 (bundle 7)             ...              ...
   symbol 0    |   ...                           bundle 3         bundle 6
   |   ...                           bundle 2         bundle 1
   |   REG 3                         bundle 1         bundle 7
   |   REG 0  (bundle 0)             bundle 0         bundle 0

  */

  uint32_t coreset_C = 0;
  int coreset_interleaved = 0;
  const int N_regs = coreset_nbr_rb * coreset_time_dur;

  if (reg_bundle_size_L != 0) { // interleaving will be done only if reg_bundle_size_L != 0
    coreset_interleaved = 1;
    coreset_C = (uint32_t)(N_regs / (coreset_interleaver_size_R * reg_bundle_size_L));
  } else {
    reg_bundle_size_L = 6;
  }

  int B_rb = reg_bundle_size_L / coreset_time_dur; // nb of RBs occupied by each REG bundle
  int num_bundles_per_cce = 6 / reg_bundle_size_L;
  int n_cce = N_regs / 6;
  int max_bundles = n_cce * num_bundles_per_cce;
  int f_bundle_j_list[max_bundles];
  // for each bundle
  int c = 0, r = 0, f_bundle_j = 0;
  for (int nb = 0; nb < max_bundles; nb++) {
    if (coreset_interleaved == 0)
      f_bundle_j = nb;
    else {
      if (r == coreset_interleaver_size_R) {
        r = 0;
        c++;
      }
      f_bundle_j = ((r * coreset_C) + c + n_shift) % (N_regs / reg_bundle_size_L);
      r++;
    }
    f_bundle_j_list[nb] = f_bundle_j;
  }

  // Get cce_list indices by bundle index in ascending order
  int f_bundle_j_list_ord[number_of_candidates][max_bundles];
  for (int c_id = 0; c_id < number_of_candidates; c_id++) {
    int start_bund_cand = CCE[c_id] * num_bundles_per_cce;
    int max_bund_per_cand = L[c_id] * num_bundles_per_cce;
    int f_bundle_j_list_id = 0;
    for (int nb = 0; nb < max_bundles; nb++) {
      for (int bund_cand = start_bund_cand; bund_cand < start_bund_cand + max_bund_per_cand; bund_cand++) {
        if (f_bundle_j_list[bund_cand] == nb) {
          f_bundle_j_list_ord[c_id][f_bundle_j_list_id] = nb;
          f_bundle_j_list_id++;
        }
      }
    }
  }

  int rb_count = 0;
  for (int c_id = 0; c_id < number_of_candidates; c_id++) {
    for (int symbol_idx = 0; symbol_idx < coreset_time_dur; symbol_idx++) {
      for (int cce_count = 0; cce_count < L[c_id]; cce_count++) {
        for (int k = 0; k < NR_NB_REG_PER_CCE / reg_bundle_size_L; k++) { // loop over REG bundles
          int f = f_bundle_j_list_ord[c_id][k + NR_NB_REG_PER_CCE * cce_count / reg_bundle_size_L];
          c16_t *in = llr[symbol_idx] + f * B_rb * RE_PER_RB_OUT_DMRS;
          // loop over the RBs of the bundle
          memcpy(e_rx + RE_PER_RB_OUT_DMRS * rb_count, in, B_rb * RE_PER_RB_OUT_DMRS * sizeof(*e_rx));
          rb_count += B_rb;
        }
      }
    }
  }
}

static void nr_pdcch_llr(uint32_t sz, c16_t *rxF, c16_t *llr)
{
  for (int i = 0; i < sz; i++) {
    // We clip the signal
    c16_t res;
    res.r = min(rxF->r, 31);
    res.r = max(-32, res.r);
    res.i = min(rxF->i, 31);
    res.i = max(-32, res.i);
    *llr++ = res;
    LOG_DDD("llr logs: rb=%d i=%d rxF:%d,%d => pdcch_llr:%d,%d\n", i / 18, i, rxF->r, rxF->i, llr->r, llr->i);
    rxF++;
  }
}

// This function will extract the mapped DM-RS PDCCH REs as per 38.211 Section 7.4.1.3.2 (Mapping to physical resources)
static void nr_pdcch_extract_rbs_single(uint32_t rxdataF_sz,
                                        uint32_t coreset_nbr_rb,
                                        c16_t rxdataF[][rxdataF_sz],
                                        int32_t est_size,
                                        c16_t dl_ch_estimates[][est_size],
                                        int arraySz,
                                        c16_t rxdataF_ext[][arraySz],
                                        c16_t dl_ch_estimates_ext[][arraySz],
                                        int symbol,
                                        NR_DL_FRAME_PARMS *frame_parms,
                                        uint8_t *coreset_freq_dom,
                                        uint32_t n_BWP_start)
{
  /*
   * This function is demapping DM-RS PDCCH RE
   * Implementing 38.211 Section 7.4.1.3.2 Mapping to physical resources
   * PDCCH DM-RS signals are mapped on RE a_k_l where:
   * k = 12*n + 4*kprime + 1
   * n=0,1,..
   * kprime=0,1,2
   * According to this equations, DM-RS PDCCH are mapped on k where k%12==1 || k%12==5 || k%12==9
   *
   */

  for (int aarx = 0; aarx < frame_parms->nb_antennas_rx; aarx++) {
    const c16_t *dl_ch0 = dl_ch_estimates[aarx] + frame_parms->ofdm_symbol_size * symbol;
    c16_t *rxFbase = rxdataF[aarx] + frame_parms->ofdm_symbol_size * symbol;
    LOG_DDD("dl_ch0 = &dl_ch_estimates[aarx = (%d)][0]\n", aarx);

    c16_t *dl_ch0_ext = dl_ch_estimates_ext[aarx];
    c16_t *rxF_ext = rxdataF_ext[aarx];

    /*
     * The following for loop handles treatment of PDCCH contained in table rxdataF (in frequency domain)
     * In NR the PDCCH IQ symbols are contained within RBs in the CORESET defined by higher layers which is located within the BWP
     * Lets consider that the first RB to be considered as part of the CORESET and part of the PDCCH is n_BWP_start
     * Several cases have to be handled differently as IQ symbols are situated in different parts of rxdataF:
     * 1. Number of RBs in the system bandwidth is even
     *    1.1 The RB is <  than the N_RB_DL/2 -> IQ symbols are in the second half of the rxdataF (from first_carrier_offset)
     *    1.2 The RB is >= than the N_RB_DL/2 -> IQ symbols are in the first half of the rxdataF (from element 0)
     * 2. Number of RBs in the system bandwidth is odd
     * (particular case when the RB with DC as it is treated differently: it is situated in symbol borders of rxdataF)
     *    2.1 The RB is <  than the N_RB_DL/2 -> IQ symbols are in the second half of the rxdataF (from first_carrier_offset)
     *    2.2 The RB is >  than the N_RB_DL/2 -> IQ symbols are in the first half of the rxdataF (from element 0 + 2nd half RB
     * containing DC) 2.3 The RB is == N_RB_DL/2          -> IQ symbols are in the upper border of the rxdataF for first 6 IQ
     * element and the lower border of the rxdataF for the last 6 IQ elements If the first RB containing PDCCH within the UE BWP
     * and within the CORESET is higher than half of the system bandwidth (N_RB_DL), then the IQ symbol is going to be found at
     * the position 0+c_rb-N_RB_DL/2 in rxdataF and we have to point the pointer at (1+c_rb-N_RB_DL/2) in rxdataF
     */

    int c_rb = 0;
    for (int rb = 0; rb < coreset_nbr_rb; rb++, c_rb++) {
      int c_rb_by6 = c_rb / 6;

      // skip zeros in frequency domain bitmap
      while ((coreset_freq_dom[c_rb_by6 / 8] & (1 << (7 - (c_rb_by6 & 7)))) == 0) {
        c_rb += 6;
        c_rb_by6 = c_rb / 6;
      }
      c16_t *rxF = NULL;
      if ((frame_parms->N_RB_DL & 1) == 0) {
        if ((c_rb + n_BWP_start) < frame_parms->N_RB_DL / 2)
          // if RB to be treated is lower than middle system bandwidth then rxdataF pointed
          // at (offset + c_br + symbol * ofdm_symbol_size): even case
          rxF = rxFbase + frame_parms->first_carrier_offset + RE_PER_RB * (c_rb + n_BWP_start);
        else
          // number of RBs is even  and c_rb is higher than half system bandwidth (we don't skip DC)
          // if these conditions are true the pointer has to be situated at the 1st part of the rxdataF
          // we point at the 1st part of the rxdataF in symbol
          rxF = rxFbase + RE_PER_RB * (c_rb + n_BWP_start - frame_parms->N_RB_DL / 2);
      } else {
        if ((c_rb + n_BWP_start) <= frame_parms->N_RB_DL / 2)
          // if RB to be treated is lower than middle system bandwidth then rxdataF pointed
          //  at (offset + c_br + symbol * ofdm_symbol_size): odd case
          rxF = rxFbase + frame_parms->first_carrier_offset + RE_PER_RB * (c_rb + n_BWP_start);
        else
          // number of RBs is odd  and c_rb is higher than half system bandwidth + 1
          // if these conditions are true the pointer has to be situated at the 1st part of
          // the rxdataF just after the first IQ symbols of the RB containing DC
          // we point at the 1st part of the rxdataF in symbol
          rxF = rxFbase + RE_PER_RB * (c_rb + n_BWP_start - frame_parms->N_RB_DL / 2) - 6;
      }
      const int valid_re[RE_PER_RB_OUT_DMRS] = {0, 2, 3, 4, 6, 7, 8, 10, 11};
      for (int i = 0; i < sizeofArray(valid_re); i++) {
        *rxF_ext++ = rxF[valid_re[i]];
        *dl_ch0_ext++ = dl_ch0[valid_re[i]];
      }
      dl_ch0 += RE_PER_RB;
    }
  }
}

static void nr_pdcch_channel_compensation(int arraySz,
                                          c16_t rxdataF_ext[][arraySz],
                                          c16_t dl_ch_estimates_ext[][arraySz],
                                          c16_t rxdataF_comp[][arraySz],
                                          int antRx,
                                          uint8_t output_shift)
{
  for (int aarx = 0; aarx < antRx; aarx++) {
    // multiply by conjugated channel, this function require size in _m128i, else it doesn't process all samples
    mult_cpx_conj_vector(dl_ch_estimates_ext[aarx], rxdataF_ext[aarx], rxdataF_comp[aarx], arraySz, output_shift);
  }
}

static void nr_pdcch_detection_mrc(int sz, c16_t rxdataF_comp[][sz])
{
  LOG_D(NR_PHY_DCI, "we enter nr_pdcch_detection_mrc (hard coded 2 antennas)\n");
  c16_t *rx0 = rxdataF_comp[0];
  c16_t *rx1 = rxdataF_comp[1];

  // MRC on each re of rb
  // input always aligned and accepting tail padding to process all actual samples
  for (int i = 0; i < sz; i += 4) {
    *(simde__m128i *)(rx0 + i) =
        simde_mm_adds_epi16(simde_mm_srai_epi16(*(simde__m128i *)(rx0 + i), 1), simde_mm_srai_epi16(*(simde__m128i *)(rx1 + i), 1));
  }
}

void nr_rx_pdcch(PHY_VARS_NR_UE *ue,
                 const UE_nr_rxtx_proc_t *proc,
                 int32_t pdcch_est_size,
                 c16_t pdcch_dl_ch_estimates[][pdcch_est_size],
                 c16_t *pdcch_e_rx,
                 fapi_nr_dl_config_dci_dl_pdu_rel15_t *rel15,
                 c16_t rxdataF[][ue->frame_parms.samples_per_slot_wCP])
{
  NR_DL_FRAME_PARMS *frame_parms = &ue->frame_parms;

  int n_rb,rb_offset;
  get_coreset_rballoc(rel15->coreset.frequency_domain_resource,&n_rb,&rb_offset);
  const int antRx = frame_parms->nb_antennas_rx;

  // Pointer to llrs
  const int symb_size = n_rb * RE_PER_RB_OUT_DMRS;
  c16_t llr[rel15->coreset.duration][symb_size];

  LOG_D(NR_PHY_DCI,
        "pdcch coreset: freq %x, n_rb %d, rb_offset %d\n",
        rel15->coreset.frequency_domain_resource[0],
        n_rb,
        rb_offset);
  for (int s = 0; s < rel15->coreset.duration; s++) {
    LOG_D(NR_PHY_DCI, "in nr_pdcch_extract_rbs_single(rxdataF -> rxdataF_ext || dl_ch_estimates -> dl_ch_estimates_ext)\n");
    const int arraySz = ceil_mod(symb_size, 32);
    __attribute__((aligned(32))) c16_t rxdataF_ext[antRx][arraySz];
    __attribute__((aligned(32))) c16_t pdcch_dl_ch_estimates_ext[antRx][arraySz];
    nr_pdcch_extract_rbs_single(ue->frame_parms.samples_per_slot_wCP,
                                n_rb,
                                rxdataF,
                                pdcch_est_size,
                                pdcch_dl_ch_estimates,
                                arraySz,
                                rxdataF_ext,
                                pdcch_dl_ch_estimates_ext,
                                rel15->coreset.StartSymbolIndex + s,
                                frame_parms,
                                rel15->coreset.frequency_domain_resource,
                                rel15->BWPStart);

    LOG_D(NR_PHY_DCI, "in channel level function (dl_ch_estimates_ext -> dl_ch_estimates_ext)\n");
    // compute channel level based on ofdm symbol 0
    int avg[antRx];
    nr_channel_level(0, arraySz, pdcch_dl_ch_estimates_ext, antRx, 1, avg, n_rb);
    int avgs = avg[0];
    for (int i = 1; i < antRx; i++)
      avgs = cmax(avgs, avg[i]);
    uint8_t log2_maxh = (log2_approx(avgs) / 2) + 5; //+antRx;

#ifdef UE_DEBUG_TRACE
    LOG_D(NR_PHY_DCI, "slot %d: pdcch log2_maxh = %d (%d,%d)\n", proc->nr_slot_rx, log2_maxh, avgP[0], avgs);
#endif
#if T_TRACER
    T(T_UE_PHY_PDCCH_ENERGY, T_INT(0), T_INT(0), T_INT(proc->frame_rx % 1024), T_INT(proc->nr_slot_rx), T_INT(avgs));
#endif
    LOG_D(NR_PHY_DCI, "we enter nr_pdcch_channel_compensation(log2_maxh=%d)\n", log2_maxh);
    LOG_D(NR_PHY_DCI, "in nr_pdcch_channel_compensation(rxdataF_ext x dl_ch_estimates_ext -> rxdataF_comp)\n");
    // compute LLRs for ofdm symbol 0 only
    __attribute__((aligned(32))) c16_t rxdataF_comp[antRx][arraySz];
    nr_pdcch_channel_compensation(arraySz,
                                  rxdataF_ext,
                                  pdcch_dl_ch_estimates_ext,
                                  rxdataF_comp,
                                  antRx,
                                  log2_maxh); // log2_maxh+I0_shift

    UEscopeCopy(ue, pdcchRxdataF_comp, rxdataF_comp, sizeof(struct complex16), antRx, symb_size, 0);

    if (antRx > 1)
      nr_pdcch_detection_mrc(arraySz, rxdataF_comp);

    nr_pdcch_llr(symb_size, rxdataF_comp[0], llr[s]);
  }
  UEscopeCopy(ue, pdcchLlr, llr, sizeof(c16_t), 1, rel15->coreset.duration * symb_size, 0);

  nr_pdcch_demapping_deinterleaving(n_rb,
                                    llr,
                                    pdcch_e_rx,
                                    rel15->coreset.duration,
                                    rel15->coreset.RegBundleSize,
                                    rel15->coreset.InterleaverSize,
                                    rel15->coreset.ShiftIndex,
                                    rel15->number_of_candidates,
                                    rel15->CCE,
                                    rel15->L);
}

static void nr_pdcch_unscrambling(c16_t *e_rx,
                                  uint16_t scrambling_RNTI,
                                  uint32_t length,
                                  uint16_t pdcch_DMRS_scrambling_id,
                                  int16_t *z2)
{
  uint32_t rnti = (uint32_t) scrambling_RNTI;
  uint16_t n_id = pdcch_DMRS_scrambling_id;
  uint32_t *seq = gold_cache(((rnti << 16) + n_id) % (1U << 31), length / 32); // this is c_init in 38.211 v15.1.0 Section 7.3.2.3
  LOG_D(NR_PHY_DCI, "PDCCH Unscrambling: scrambling_RNTI %x\n", rnti);
  int16_t *ptr = &e_rx[0].r;
  for (int i = 0; i < length; i++) {
    if (seq[i / 32] & (1UL << (i % 32)))
      z2[i] = -ptr[i];
    else
      z2[i] = ptr[i];
  }
}

void nr_dci_decoding_procedure(PHY_VARS_NR_UE *ue,
                               const UE_nr_rxtx_proc_t *proc,
                               c16_t *pdcch_e_rx,
                               fapi_nr_dci_indication_t *dci_ind,
                               fapi_nr_dl_config_dci_dl_pdu_rel15_t *rel15)
{
  int e_rx_cand_idx = 0;
  *dci_ind = (fapi_nr_dci_indication_t){.SFN = proc->frame_rx, .slot = proc->nr_slot_rx};
  // if DCI for SIB we don't break after finding 1st DCI with that RNTI
  // there might be SIB1 and otherSIB in the same slot with the same length
  bool is_SI = rel15->rnti == SI_RNTI;

  for (int j = 0; j < rel15->number_of_candidates; j++) {
    int CCEind = rel15->CCE[j];
    int L = rel15->L[j];

    // Loop over possible DCI lengths
    
    for (int k = 0; k < rel15->num_dci_options; k++) {
      // skip this candidate if we've already found one with the
      // same rnti and size at a different aggregation level
      int dci_length = rel15->dci_length_options[k];
      int ind;
      for (ind = 0; ind < dci_ind->number_of_dcis; ind++) {
        if (!is_SI && rel15->rnti == dci_ind->dci_list[ind].rnti && dci_length == dci_ind->dci_list[ind].payloadSize) {
          break;
        }
      }
      if (ind < dci_ind->number_of_dcis)
        continue;

      uint64_t dci_estimation[2] = {0};
      LOG_D(NR_PHY_DCI,
            "(%i.%i) Trying DCI candidate %d of %d number of candidates, CCE %d (%d), L %d, length %d, format %d\n",
            proc->frame_rx,
            proc->nr_slot_rx,
            j,
            rel15->number_of_candidates,
            CCEind,
            e_rx_cand_idx,
            L,
            dci_length,
            rel15->dci_format_options[k]);

      int16_t tmp_e[16 * 108];
      nr_pdcch_unscrambling(&pdcch_e_rx[e_rx_cand_idx],
                            rel15->coreset.scrambling_rnti,
                            L * 108,
                            rel15->coreset.pdcch_dmrs_scrambling_id,
                            tmp_e);

      const uint32_t crc = polar_decoder_int16(tmp_e, dci_estimation, 1, NR_POLAR_DCI_MESSAGE_TYPE, dci_length, L);

      rnti_t n_rnti = rel15->rnti;
      if (crc == n_rnti) {
        LOG_D(NR_PHY_DCI,
              "(%i.%i) Received dci indication (rnti %x,dci format %d,n_CCE %d,payloadSize %d,payload %llx)\n",
              proc->frame_rx,
              proc->nr_slot_rx,
              n_rnti,
              rel15->dci_format_options[k],
              CCEind,
              dci_length,
              *(unsigned long long *)dci_estimation);
        AssertFatal(dci_ind->number_of_dcis < sizeofArray(dci_ind->dci_list), "Fix allocation\n");
        fapi_nr_dci_indication_pdu_t *dci = dci_ind->dci_list + dci_ind->number_of_dcis;
        *dci = (fapi_nr_dci_indication_pdu_t){
            .rnti = n_rnti,
            .n_CCE = CCEind,
            .N_CCE = L,
            .dci_format = rel15->dci_format_options[k],
            .ss_type = rel15->ss_type_options[k],
            .coreset_type = rel15->coreset.CoreSetType,
        };
        int n_rb, rb_offset;
        get_coreset_rballoc(rel15->coreset.frequency_domain_resource, &n_rb, &rb_offset);
        dci->cset_start = rel15->BWPStart + rb_offset;
        dci->payloadSize = dci_length;
        memcpy(dci->payloadBits, dci_estimation, (dci_length + 7) / 8);
        dci_ind->number_of_dcis++;
        break;    // If DCI is found, no need to check for remaining DCI lengths
      } else {
        LOG_D(NR_PHY_DCI,
              "(%i.%i) Decoded crc %x does not match rnti %x for DCI format %d\n",
              proc->frame_rx,
              proc->nr_slot_rx,
              crc,
              n_rnti,
              rel15->dci_format_options[k]);
      }
    }
    e_rx_cand_idx += RE_PER_RB_OUT_DMRS * L * 6; // e_rx index for next candidate (L CCEs, 6 REGs per CCE and 9 REs per REG )
  }
}
