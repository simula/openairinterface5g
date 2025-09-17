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

/*! \file nr_ulsch_ue.c
 * \brief Top-level routines for transmission of the PUSCH TS 38.211 v 15.4.0
 * \author Khalid Ahmed
 * \date 2019
 * \version 0.1
 * \company Fraunhofer IIS
 * \email: khalid.ahmed@iis.fraunhofer.de
 * \note
 * \warning
 */
#include <stdint.h>
#include "PHY/NR_REFSIG/dmrs_nr.h"
#include "PHY/NR_REFSIG/ptrs_nr.h"
#include "PHY/NR_REFSIG/refsig_defs_ue.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_ue.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "PHY/MODULATION/nr_modulation.h"
#include "PHY/MODULATION/modulation_common.h"
#include "common/utils/assertions.h"
#include "common/utils/nr/nr_common.h"
#include "common/utils/LOG/vcd_signal_dumper.h"
#include "PHY/NR_TRANSPORT/nr_transport_common_proto.h"
#include "PHY/NR_TRANSPORT/nr_sch_dmrs.h"
#include "PHY/defs_nr_common.h"
#include "PHY/TOOLS/tools_defs.h"
#include "executables/nr-softmodem.h"
#include "executables/softmodem-common.h"
#include "PHY/NR_REFSIG/ul_ref_seq_nr.h"
#include <openair2/UTIL/OPT/opt.h>

//#define DEBUG_PUSCH_MAPPING
//#define DEBUG_MAC_PDU
//#define DEBUG_DFT_IDFT

//extern int32_t uplink_counter;

void nr_pusch_codeword_scrambling_uci(uint8_t *in, uint32_t size, uint32_t Nid, uint32_t n_RNTI, uint32_t* out)
{
  uint32_t *seq = gold_cache((n_RNTI << 15) + Nid, (size + 31) / 32);
  for (int i=0; i<size; i++) {
    int idx = i / 32;
    int b_idx = i % 32;
    if (in[i]==NR_PUSCH_x)
      out[idx] ^= 1 << b_idx;
    else if (in[i]==NR_PUSCH_y){
      if (b_idx)
        out[idx] ^= (out[idx] & (1 << (b_idx - 1))) << 1;
      else{
        uint32_t temp_out = out[idx - 1];
        out[idx] ^= temp_out >> 31;
      }
    }
    else
      out[idx] ^= (((in[i]) & 1) ^ ((seq[idx] >> b_idx) & 1)) << b_idx;
    //printf("i %d b_idx %d in %d s 0x%08x out 0x%08x\n", i, b_idx, in[i], s, *out);
  }
}

void nr_pusch_codeword_scrambling(uint8_t *in, uint32_t size, uint32_t Nid, uint32_t n_RNTI, bool uci_on_pusch, uint32_t* out)
{
  if (uci_on_pusch)
    nr_pusch_codeword_scrambling_uci(in, size, Nid, n_RNTI, out);
  else
    nr_codeword_scrambling(in, size, 0, Nid, n_RNTI, out);
}

/*
The function pointers are set once before calling the mapping funcion for
all symbols based on different parameters. Then the mapping is done for
each symbol by calling the function pointers.
*/
static void (*map_dmrs_ptr)(const unsigned int, const c16_t *, c16_t *);
static void (*map_data_dmrs_ptr)(const unsigned int num_cdm_no_data, const c16_t *, c16_t *);

/*
The following set of functions map dmrs and/or data REs in one RB based on
configuration of DMRS type, number of CDM groups with no data and delta.
For all other combinations of the parameters not present below is not
applicable.
*/

/*
DMRS mapping in a RB for Type 1.
Mapping as in TS 38.211 6.4.1.1.3 k = 4n + 2k^prime + delta
*/
static void map_dmrs_type1_cdm1_rb(const unsigned int delta, const c16_t *dmrs, c16_t *out)
{
  *(out + delta) = *dmrs++;
  *(out + delta + 2) = *dmrs++;
  *(out + delta + 4) = *dmrs++;
  *(out + delta + 6) = *dmrs++;
  *(out + delta + 8) = *dmrs++;
  *(out + delta + 10) = *dmrs++;
}

/*
Data in DMRS symbol for Type 1, NumCDMGroupNoData = 1 and delta 0 (antenna port 0 and 1).
There is no data in DMRS symbol for other scenarios in type 1.
*/
static void map_data_dmrs_type1_cdm1_rb(const unsigned int num_cdm_no_data, const c16_t *data, c16_t *out)
{
  *(out + 1) = *data++;
  *(out + 3) = *data++;
  *(out + 5) = *data++;
  *(out + 7) = *data++;
  *(out + 9) = *data++;
  *(out + 11) = *data++;
}

#define NR_DMRS_TYPE2_CDM_GRP_SIZE 2
#define NR_DMRS_TYPE2_NUM_CDM_GRP 3

/*
Map DMRS for type 2
Mapping as in TS 38.211 6.4.1.1.3 k = 6n + k^prime + delta
*/
static void map_dmrs_type2_rb(const unsigned int delta, const c16_t *dmrs, c16_t *out)
{
  memcpy(out + delta, dmrs, sizeof(c16_t) * NR_DMRS_TYPE2_CDM_GRP_SIZE);
  out += (NR_DMRS_TYPE2_CDM_GRP_SIZE * NR_DMRS_TYPE2_NUM_CDM_GRP);
  dmrs += NR_DMRS_TYPE2_CDM_GRP_SIZE;
  memcpy(out + delta, dmrs, sizeof(c16_t) * NR_DMRS_TYPE2_CDM_GRP_SIZE);
}

/*
Map data for type 2 DMRS
*/
static void map_data_dmrs_type2_rb(const unsigned int num_cdm_no_data, const c16_t *data, c16_t *out)
{
  unsigned int offset = num_cdm_no_data * NR_DMRS_TYPE2_CDM_GRP_SIZE;
  const unsigned int size = (NR_DMRS_TYPE2_NUM_CDM_GRP - num_cdm_no_data) * NR_DMRS_TYPE2_CDM_GRP_SIZE;
  memcpy(out + offset, data, sizeof(c16_t) * size);
  offset += NR_DMRS_TYPE2_CDM_GRP_SIZE * NR_DMRS_TYPE2_NUM_CDM_GRP;
  data += size;
  memcpy(out + offset, data, sizeof(c16_t) * size);
}

/*
Map data and PTRS in RB
*/
static void map_data_ptrs(const unsigned int ptrsIdx, const c16_t *data, const c16_t *ptrs, c16_t *out)
{
  memcpy(out, data, sizeof(c16_t) * ptrsIdx);
  data += ptrsIdx;
  *(out + ptrsIdx) = *ptrs;
  memcpy(out + ptrsIdx + 1, data, sizeof(c16_t) * NR_NB_SC_PER_RB - ptrsIdx - 1);
}

/*
Map data only in RB
*/
static void map_data_rb(const c16_t *data, c16_t *out)
{
  memcpy(out, data, sizeof(c16_t) * NR_NB_SC_PER_RB);
}

/*
This function is used when a PRB is on both sides of DC.
The destination buffer in this case in not contiguous so REs are mapped on to a temporary buffer
so that we can reuse the existing functions. Then it is copied to the destination buffer.
*/
static void map_over_dc(const unsigned int right_dc,
                        const unsigned int num_cdm_no_data,
                        const unsigned int fft_size,
                        const unsigned int dmrs_per_rb,
                        const unsigned int data_per_rb,
                        const unsigned int delta,
                        const unsigned int ptrsIdx,
                        const c16_t **ptrs,
                        const c16_t **dmrs,
                        const c16_t **data,
                        c16_t **out)
{
  // if first RE is DC no need to map in this function
  if (right_dc == 0)
    return;

  c16_t *out_tmp = *out;
  c16_t tmp_out_buf[NR_NB_SC_PER_RB];
  const unsigned int left_dc = NR_NB_SC_PER_RB - right_dc;
  /* copy out to temp buffer. incase we want to preserve the REs in the out buffer
     as we call mapping of data in DMRS symbol after mapping DMRS REs
  */
  memcpy(tmp_out_buf, out_tmp, sizeof(c16_t) * left_dc);
  out_tmp -= (fft_size - left_dc);
  memcpy(tmp_out_buf + left_dc, out_tmp, sizeof(c16_t) * right_dc);

  /* map on to temp buffer */
  if (dmrs && data) {
    map_data_dmrs_ptr(num_cdm_no_data, *data, tmp_out_buf);
    *data += data_per_rb;
  } else if (dmrs) {
    map_dmrs_ptr(delta, *dmrs, tmp_out_buf);
    *dmrs += dmrs_per_rb;
  } else if (ptrs) {
    map_data_ptrs(ptrsIdx, *data, *ptrs, tmp_out_buf);
    *data += (NR_NB_SC_PER_RB - 1);
    *ptrs += 1;
  } else if (data) {
    map_data_rb(*data, tmp_out_buf);
    *data += NR_NB_SC_PER_RB;
  } else {
    DevAssert(false);
  }

  /* copy back to out buffer */
  out_tmp = *out;
  memcpy(out_tmp, tmp_out_buf, sizeof(c16_t) * left_dc);
  out_tmp -= (fft_size - left_dc);
  memcpy(out_tmp, tmp_out_buf + left_dc, sizeof(c16_t) * right_dc);
  out_tmp += right_dc;
  *out = out_tmp;
}

/*
Holds params needed for PUSCH resoruce mapping
*/
typedef struct {
  rnti_t rnti;
  unsigned int K_ptrs;
  unsigned int k_RE_ref;
  unsigned int first_sc_offset;
  unsigned int fft_size;
  unsigned int num_rb_max;
  unsigned int symbols_per_slot;
  unsigned int slot;
  unsigned int dmrs_scrambling_id;
  unsigned int scid;
  unsigned int dmrs_port;
  int Wt;
  int *Wf;
  unsigned int dmrs_symb_pos;
  unsigned int ptrs_symb_pos;
  unsigned int pdu_bit_map;
  transformPrecoder_t transform_precoding;
  unsigned int bwp_start;
  unsigned int start_rb;
  unsigned int nb_rb;
  unsigned int start_symbol;
  unsigned int num_symbols;
  pusch_dmrs_type_t dmrs_type;
  unsigned int delta;
  unsigned int num_cdm_no_data;
} nr_phy_pxsch_params_t;

/*
Map all REs in one OFDM symbol
This function operation is as follows:
mapping is done on RB basis. if RB contains DC and if DC is in middle
of the RB, then the mapping is done via map_over_dc().
*/
static void map_current_symbol(const nr_phy_pxsch_params_t p,
                               const bool dmrs_symbol,
                               const bool ptrs_symbol,
                               const c16_t *dmrs_seq,
                               const c16_t *ptrs_seq,
                               const c16_t **data,
                               c16_t *out)
{
  const unsigned int abs_start_rb = p.bwp_start + p.start_rb;
  const unsigned int start_sc = (p.first_sc_offset + abs_start_rb * NR_NB_SC_PER_RB) % p.fft_size;
  const unsigned int dc_rb = (p.fft_size - start_sc) / NR_NB_SC_PER_RB;
  const unsigned int rb_over_dc = (p.fft_size - start_sc) % NR_NB_SC_PER_RB;
  const unsigned int n_cdm = p.num_cdm_no_data;
  const c16_t *data_tmp = *data;
  /* If current symbol is DMRS symbol */
  if (dmrs_symbol) {
    const unsigned int dmrs_per_rb = (p.dmrs_type == pusch_dmrs_type1) ? 6 : 4;
    const unsigned int data_per_rb = NR_NB_SC_PER_RB - dmrs_per_rb;

    const c16_t *p_mod_dmrs = dmrs_seq + abs_start_rb * dmrs_per_rb;
    c16_t *out_tmp = out + start_sc;
    for (unsigned int rb = 0; rb < p.nb_rb; rb++) {
      if (rb == dc_rb) {
        // map RB at DC
        if (rb_over_dc) {
          // if DC is in middle of RB, the following function handles it.
          map_over_dc(rb_over_dc, n_cdm, p.fft_size, dmrs_per_rb, data_per_rb, p.delta, 0, NULL, &p_mod_dmrs, NULL, &out_tmp);
          continue;
        } else {
          // else just move the pointer and following function will map the rb
          out_tmp -= p.fft_size;
        }
      }
      map_dmrs_ptr(p.delta, p_mod_dmrs, out_tmp);
      p_mod_dmrs += dmrs_per_rb;
      out_tmp += NR_NB_SC_PER_RB;
    }

    /* if there is data in current DMRS symbol, we map it here. */
    if (map_data_dmrs_ptr) {
      c16_t *out_tmp = out + start_sc;
      for (unsigned int rb = 0; rb < p.nb_rb; rb++) {
        if (rb == dc_rb) {
          if (rb_over_dc) {
            map_over_dc(rb_over_dc, n_cdm, p.fft_size, dmrs_per_rb, data_per_rb, p.delta, 0, NULL, &p_mod_dmrs, &data_tmp, &out_tmp);
            continue;
          } else {
            out_tmp -= p.fft_size;
          }
        }
        map_data_dmrs_ptr(n_cdm, data_tmp, out_tmp);
        data_tmp += data_per_rb;
        out_tmp += NR_NB_SC_PER_RB;
      }
    }
  /* If current symbol is a PTRS symbol */
  } else if (ptrs_symbol) {
    const unsigned int first_ptrs_re = get_first_ptrs_re(p.rnti, p.K_ptrs, p.nb_rb, p.k_RE_ref) + start_sc;
    const unsigned int ptrs_idx_re = (start_sc - first_ptrs_re) % NR_NB_SC_PER_RB; // PTRS RE index within RB
    unsigned int non_ptrs_rb = (start_sc - first_ptrs_re) / NR_NB_SC_PER_RB; // number of RBs before the first PTRS RB
    int ptrs_idx_rb = -non_ptrs_rb; // RB count to check for PTRS RB
    c16_t *out_tmp = out + start_sc;
    const c16_t *p_mod_ptrs = ptrs_seq;
    /* map data to RBs before the first PTRS RB or if current RB has no PTRS */
    for (unsigned int rb = 0; rb < p.nb_rb; rb++) {
      if (rb < non_ptrs_rb || ptrs_idx_rb % p.K_ptrs) {
        if (rb == dc_rb) {
          if (rb_over_dc) {
            map_over_dc(rb_over_dc, n_cdm, p.fft_size, 0, 0, p.delta, 0, NULL, NULL, &data_tmp, &out_tmp);
            continue;
          } else {
            out_tmp -= p.fft_size;
          }
        }
        map_data_rb(data_tmp, out_tmp);
        data_tmp += NR_NB_SC_PER_RB;
        out_tmp += NR_NB_SC_PER_RB;
      } else {
        if (rb == dc_rb) {
          if (rb_over_dc) {
            map_over_dc(rb_over_dc, n_cdm, p.fft_size, 0, 0, p.delta, ptrs_idx_re, &p_mod_ptrs, NULL, &data_tmp, &out_tmp);
            continue;
          } else {
            out_tmp -= p.fft_size;
          }
        }
        map_data_ptrs(ptrs_idx_re, data_tmp, p_mod_ptrs, out_tmp);
        p_mod_ptrs++; // increament once as only one PTRS RE per RB
        data_tmp += (NR_NB_SC_PER_RB - 1);
        out_tmp += NR_NB_SC_PER_RB;
      }
      ptrs_idx_rb++;
    }
  } else {
    /* only data in this symbol */
    c16_t *out_tmp = out + start_sc;
    for (unsigned int rb = 0; rb < p.nb_rb; rb++) {
      if (rb == dc_rb) {
        if (rb_over_dc) {
          map_over_dc(rb_over_dc, n_cdm, p.fft_size, 0, 0, p.delta, 0, NULL, NULL, &data_tmp, &out_tmp);
          continue;
        } else {
          out_tmp -= p.fft_size;
        }
      }
      map_data_rb(data_tmp, out_tmp);
      data_tmp += NR_NB_SC_PER_RB;
      out_tmp += NR_NB_SC_PER_RB;
    }
  }
  *data = data_tmp;
}

/*
TS 38.211 table 6.4.1.1.3-1 and 2
*/
static void dmrs_amp_mult(const uint32_t dmrs_port,
                          const int Wt,
                          const int Wf[2],
                          const c16_t *mod_dmrs,
                          c16_t *mod_dmrs_out,
                          const uint32_t n_dmrs,
                          const pusch_dmrs_type_t dmrs_type)
{
  /* short array that hold amplitude for k_prime = 0 and k_prime = 1 */
  int32_t alpha_dmrs[2] __attribute((aligned(16)));
  for (int_fast8_t i = 0; i < sizeofArray(alpha_dmrs); i++) {
    const int32_t a = Wf[i] * Wt * AMP;
    alpha_dmrs[i] = a;
  }

  /* multiply amplitude with complex DMRS vector */
  for (int_fast16_t i = 0; i < n_dmrs; i++) {
    mod_dmrs_out[i] = c16mulRealShift(mod_dmrs[i], alpha_dmrs[i % 2], 15);
  }
}

/*
Map ULSCH data and DMRS in all of the scheduled symbols and PRBs
*/
static void map_symbols(const nr_phy_pxsch_params_t p,
                        const unsigned int slot,
                        const c16_t *dmrs_seq,
                        const c16_t *data,
                        c16_t *out)
{
  // asign the function pointers
  if (p.dmrs_type == pusch_dmrs_type1) {
    map_dmrs_ptr = map_dmrs_type1_cdm1_rb;
    map_data_dmrs_ptr = (p.num_cdm_no_data == 1) ? map_data_dmrs_type1_cdm1_rb : NULL;
  } else {
    map_dmrs_ptr = map_dmrs_type2_rb;
    map_data_dmrs_ptr = (p.num_cdm_no_data < 3) ? map_data_dmrs_type2_rb : NULL;
  }
  // for all symbols
  const unsigned int n_dmrs = (p.bwp_start + p.start_rb + p.nb_rb) * ((p.dmrs_type == pusch_dmrs_type1) ? 6 : 4);
  const c16_t *cur_data = data;
  for (int l = p.start_symbol; l < p.start_symbol + p.num_symbols; l++) {
    const bool dmrs_symbol = is_dmrs_symbol(l, p.dmrs_symb_pos);
    const bool ptrs_symbol = is_ptrs_symbol(l, p.ptrs_symb_pos);
    c16_t mod_dmrs_amp[ALNARS_16_4(n_dmrs)] __attribute((aligned(16)));
    c16_t mod_ptrs_amp[ALNARS_16_4(p.nb_rb)] __attribute((aligned(16)));
    const uint32_t *gold = NULL;
    if (dmrs_symbol || ptrs_symbol) {
      gold = nr_gold_pusch(p.num_rb_max, p.symbols_per_slot, p.dmrs_scrambling_id, p.scid, slot, l);
    }
    if (dmrs_symbol) {
      c16_t mod_dmrs[ALNARS_16_4(n_dmrs)] __attribute((aligned(16)));
      if (p.transform_precoding == transformPrecoder_disabled) {
        nr_modulation(gold, n_dmrs * 2, DMRS_MOD_ORDER, (int16_t *)mod_dmrs);
        dmrs_amp_mult(p.dmrs_port, p.Wt, p.Wf, mod_dmrs, mod_dmrs_amp, n_dmrs, p.dmrs_type);
      } else {
        dmrs_amp_mult(p.dmrs_port, p.Wt, p.Wf, dmrs_seq, mod_dmrs_amp, n_dmrs, p.dmrs_type);
      }
    } else if ((p.pdu_bit_map & PUSCH_PDU_BITMAP_PUSCH_PTRS) && ptrs_symbol) {
      AssertFatal(p.transform_precoding == transformPrecoder_disabled, "PTRS NOT SUPPORTED IF TRANSFORM PRECODING IS ENABLED\n");
      c16_t mod_ptrs[ALNARS_16_4(p.nb_rb)] __attribute((aligned(16)));
      nr_modulation(gold, p.nb_rb, DMRS_MOD_ORDER, (int16_t *)mod_ptrs);
      const unsigned int beta_ptrs = 1; // temp value until power control is implemented
      multadd_complex_vector_real_scalar((int16_t *)mod_ptrs, beta_ptrs * AMP, (int16_t *)mod_ptrs_amp, 1, p.nb_rb);
    }
    map_current_symbol(p,
                       dmrs_symbol,
                       ptrs_symbol,
                       mod_dmrs_amp,
                       mod_ptrs_amp,
                       &cur_data, // increments every symbol
                       out + l * p.fft_size);
  }
}

void nr_ue_ulsch_procedures(PHY_VARS_NR_UE *UE,
                            const unsigned char harq_pid,
                            const uint32_t frame,
                            const uint8_t slot,
                            const int gNB_id,
                            nr_phy_data_tx_t *phy_data,
                            c16_t **txdataF)
{
  LOG_D(PHY,"nr_ue_ulsch_procedures hard_id %d %d.%d\n",harq_pid,frame,slot);

  int l_prime[2];
  uint8_t nb_dmrs_re_per_rb;

  NR_DL_FRAME_PARMS *frame_parms = &UE->frame_parms;

  int      N_PRB_oh = 0; // higher layer (RRC) parameter xOverhead in PUSCH-ServingCellConfig
  uint16_t number_dmrs_symbols = 0;

  NR_UE_ULSCH_t *ulsch_ue = &phy_data->ulsch;
  NR_UL_UE_HARQ_t *harq_process_ul_ue = &UE->ul_harq_processes[harq_pid];
  const nfapi_nr_ue_pusch_pdu_t *pusch_pdu = &ulsch_ue->pusch_pdu;

  uint32_t tb_size = pusch_pdu->pusch_data.tb_size;
  AssertFatal(pusch_pdu->pusch_uci.harq_ack_bit_length == 0 &&
              pusch_pdu->pusch_uci.csi_part1_bit_length == 0 &&
              pusch_pdu->pusch_uci.csi_part2_bit_length == 0,
              "UCI on PUSCH not supported at PHY\n");

  int start_symbol          = pusch_pdu->start_symbol_index;
  uint16_t ul_dmrs_symb_pos = pusch_pdu->ul_dmrs_symb_pos;
  uint8_t number_of_symbols = pusch_pdu->nr_of_symbols;
  uint8_t dmrs_type         = pusch_pdu->dmrs_config_type;
  uint16_t start_rb         = pusch_pdu->rb_start;
  uint16_t nb_rb            = pusch_pdu->rb_size;
  uint8_t Nl                = pusch_pdu->nrOfLayers;
  uint8_t mod_order         = pusch_pdu->qam_mod_order;
  uint16_t rnti             = pusch_pdu->rnti;
  uint8_t cdm_grps_no_data  = pusch_pdu->num_dmrs_cdm_grps_no_data;
  uint16_t start_sc         = frame_parms->first_carrier_offset + (start_rb+pusch_pdu->bwp_start)*NR_NB_SC_PER_RB;

  if (start_sc >= frame_parms->ofdm_symbol_size)
    start_sc -= frame_parms->ofdm_symbol_size;

  ulsch_ue->Nid_cell = frame_parms->Nid_cell;

  for (int i = start_symbol; i < start_symbol + number_of_symbols; i++) {
    if((ul_dmrs_symb_pos >> i) & 0x01)
      number_dmrs_symbols += 1;
  }

  nb_dmrs_re_per_rb = ((dmrs_type == pusch_dmrs_type1) ? 6:4)*cdm_grps_no_data;

  LOG_D(PHY,"ulsch TX %x : start_rb %d nb_rb %d mod_order %d Nl %d Tpmi %d bwp_start %d start_sc %d start_symbol %d num_symbols %d cdmgrpsnodata %d num_dmrs %d dmrs_re_per_rb %d\n",
        rnti,start_rb,nb_rb,mod_order,Nl,pusch_pdu->Tpmi,pusch_pdu->bwp_start,start_sc,start_symbol,number_of_symbols,cdm_grps_no_data,number_dmrs_symbols,nb_dmrs_re_per_rb);
  // TbD num_of_mod_symbols is set but never used
  const uint32_t N_RE_prime = NR_NB_SC_PER_RB * number_of_symbols - nb_dmrs_re_per_rb * number_dmrs_symbols - N_PRB_oh;
  harq_process_ul_ue->num_of_mod_symbols = N_RE_prime*nb_rb;

  /////////////////////////PTRS parameters' initialization/////////////////////////
  ///////////

  unsigned int K_ptrs = 0, k_RE_ref = 0;
  uint32_t unav_res = 0;
  if (pusch_pdu->pdu_bit_map & PUSCH_PDU_BITMAP_PUSCH_PTRS) {
    K_ptrs = pusch_pdu->pusch_ptrs.ptrs_freq_density;
    k_RE_ref = pusch_pdu->pusch_ptrs.ptrs_ports_list[0].ptrs_re_offset;
    uint8_t L_ptrs = 1 << pusch_pdu->pusch_ptrs.ptrs_time_density;

    ulsch_ue->ptrs_symbols = 0;

    set_ptrs_symb_idx(&ulsch_ue->ptrs_symbols,
                      number_of_symbols,
                      start_symbol,
                      L_ptrs,
                      ul_dmrs_symb_pos);
    int n_ptrs = (nb_rb + K_ptrs - 1) / K_ptrs;
    int ptrsSymbPerSlot = get_ptrs_symbols_in_slot(ulsch_ue->ptrs_symbols,
                                                   start_symbol,
                                                   number_of_symbols);
    unav_res = n_ptrs * ptrsSymbPerSlot;
  }

  ///////////
  ////////////////////////////////////////////////////////////////////


  /////////////////////////ULSCH coding/////////////////////////
  ///////////

  unsigned int G = nr_get_G(nb_rb,
                            number_of_symbols,
                            nb_dmrs_re_per_rb,
                            number_dmrs_symbols,
                            unav_res,
                            mod_order,
                            Nl);
  ws_trace_t tmp = {.nr = true,
                    .direction = DIRECTION_UPLINK,
                    .pdu_buffer = harq_process_ul_ue->payload_AB,
                    .pdu_buffer_size = tb_size,
                    .ueid = 0,
                    .rntiType = WS_C_RNTI,
                    .rnti = rnti,
                    .sysFrame = frame,
                    .subframe = slot,
                    .harq_pid = harq_pid,
                    .oob_event = 0,
                    .oob_event_value = 0};
  trace_pdu(&tmp);

  if (nr_ulsch_encoding(UE, ulsch_ue, frame_parms, harq_pid, tb_size, G) == -1) {
    NR_UL_UE_HARQ_t *harq_process_ulsch = &UE->ul_harq_processes[harq_pid];
    harq_process_ulsch->ULstatus = SCH_IDLE;
    return;
  }

  ///////////
  ////////////////////////////////////////////////////////////////////

  /////////////////////////ULSCH scrambling/////////////////////////
  ///////////

  uint32_t available_bits = G;
  // +1 because size can be not modulo 4
  uint32_t scrambled_output[available_bits / (8 * sizeof(uint32_t)) + 1];
  memset(scrambled_output, 0, sizeof(scrambled_output));

  nr_pusch_codeword_scrambling(harq_process_ul_ue->f,
                               available_bits,
                               pusch_pdu->data_scrambling_id,
                               rnti,
                               false,
                               scrambled_output);

  /////////////
  //////////////////////////////////////////////////////////////////////////

  /////////////////////////ULSCH modulation/////////////////////////
  ///////////

  int max_num_re = Nl*number_of_symbols*nb_rb*NR_NB_SC_PER_RB;
  c16_t d_mod[max_num_re] __attribute__((aligned(16)));

  nr_modulation(scrambled_output, // assume one codeword for the moment
                available_bits,
                mod_order,
                (int16_t *)d_mod);

    
  ///////////
  ////////////////////////////////////////////////////////////////////////

  /////////////////////////ULSCH layer mapping/////////////////////////
  ///////////
  const int sz = available_bits / mod_order / Nl;
  c16_t ulsch_mod[Nl][sz];

  nr_ue_layer_mapping(d_mod, Nl, sz, ulsch_mod);

  ///////////
  ////////////////////////////////////////////////////////////////////////


  //////////////////////// ULSCH transform precoding ////////////////////////
  ///////////

  l_prime[0] = 0; // single symbol ap 0

  uint8_t u = 0, v = 0;
  c16_t *dmrs_seq = NULL;
  /// Transform-coded "y"-sequences (for definition see 38-211 V15.3.0 2018-09, subsection 6.3.1.4)
  c16_t ulsch_mod_tp[max_num_re] __attribute__((aligned(16)));
  memset(ulsch_mod_tp, 0, sizeof(ulsch_mod_tp));

  if (pusch_pdu->transform_precoding == transformPrecoder_enabled) {

    uint32_t nb_re_pusch=nb_rb * NR_NB_SC_PER_RB;
    uint32_t y_offset = 0;
    uint16_t num_dmrs_res_per_symbol = nb_rb*(NR_NB_SC_PER_RB/2);

    // Calculate index to dmrs seq array based on number of DMRS Subcarriers on this symbol
    int index = get_index_for_dmrs_lowpapr_seq(num_dmrs_res_per_symbol);
    u = pusch_pdu->dfts_ofdm.low_papr_group_number;
    v = pusch_pdu->dfts_ofdm.low_papr_sequence_number;
    dmrs_seq = dmrs_lowpaprtype1_ul_ref_sig[u][v][index];

    AssertFatal(index >= 0, "Num RBs not configured according to 3GPP 38.211 section 6.3.1.4. For PUSCH with transform precoding, num RBs cannot be multiple of any other primenumber other than 2,3,5\n");
    AssertFatal(dmrs_seq != NULL, "DMRS low PAPR seq not found, check if DMRS sequences are generated");

    LOG_D(PHY,"Transform Precoding params. u: %d, v: %d, index for dmrsseq: %d\n", u, v, index);

    for (int l = start_symbol; l < start_symbol + number_of_symbols; l++) {

      if((ul_dmrs_symb_pos >> l) & 0x01)
        /* In the symbol with DMRS no data would be transmitted CDM groups is 2*/
        continue;

      nr_dft(&ulsch_mod_tp[y_offset], &ulsch_mod[0][y_offset], nb_re_pusch);

      y_offset = y_offset + nb_re_pusch;

      LOG_D(PHY,"Transform precoding being done on data- symbol: %d, nb_re_pusch: %d, y_offset: %d\n", l, nb_re_pusch, y_offset);

#ifdef DEBUG_PUSCH_MAPPING
      printf("NR_ULSCH_UE: y_offset %u\t nb_re_pusch %u \t Symbol %d \t nb_rb %d \n",
             y_offset, nb_re_pusch, l, nb_rb);
#endif
    }

#ifdef DEBUG_DFT_IDFT
    int32_t debug_symbols[MAX_NUM_NR_RE] __attribute__ ((aligned(16)));
    int offset = 0;
    printf("NR_ULSCH_UE: available_bits: %u, mod_order: %d", available_bits,mod_order);

    for (int ll = 0; ll < (available_bits/mod_order); ll++) {
      debug_symbols[ll] = ulsch_ue->ulsch_mod_tp[ll];
    }
      
    printf("NR_ULSCH_UE: numSym: %d, num_dmrs_sym: %d", number_of_symbols,number_dmrs_symbols);
    for (int ll = 0; ll < (number_of_symbols-number_dmrs_symbols); ll++) {
      nr_idft(&debug_symbols[offset], nb_re_pusch);
      offset = offset + nb_re_pusch;
    }
    LOG_M("preDFT_all_symbols.m", "UE_preDFT", ulsch_mod[0], number_of_symbols * nb_re_pusch, 1, 1);
    LOG_M("postDFT_all_symbols.m", "UE_postDFT", ulsch_mod_tp, number_of_symbols * nb_re_pusch, 1, 1);
    LOG_M("DEBUG_IDFT_SYMBOLS.m","UE_Debug_IDFT", debug_symbols,number_of_symbols*nb_re_pusch,1,1);
    LOG_M("UE_DMRS_SEQ.m","UE_DMRS_SEQ", dmrs_seq,nb_re_pusch,1,1);
#endif

  }
  
  ///////////
  ////////////////////////////////////////////////////////////////////////



  /////////////////////////ULSCH RE mapping/////////////////////////
  ///////////

  const int slot_sz = frame_parms->ofdm_symbol_size * frame_parms->symbols_per_slot;
  c16_t tx_precoding[Nl][slot_sz];
  memset(tx_precoding, 0, sizeof(tx_precoding));

  for (int nl = 0; nl < Nl; nl++) {
#ifdef DEBUG_PUSCH_MAPPING
    printf("NR_ULSCH_UE: Value of CELL ID %d /t, u %d \n", frame_parms->Nid_cell, u);
#endif

    const uint8_t dmrs_port = get_dmrs_port(nl, pusch_pdu->dmrs_ports);
    const uint8_t delta = get_delta(dmrs_port, dmrs_type);
    int Wt[2];
    int Wf[2];
    get_Wt(Wt, dmrs_port, dmrs_type);
    get_Wf(Wf, dmrs_port, dmrs_type);

    c16_t *data = (pusch_pdu->transform_precoding == transformPrecoder_enabled) ? ulsch_mod_tp : ulsch_mod[nl];

    nr_phy_pxsch_params_t params = {.rnti = rnti,
                                    .K_ptrs = K_ptrs,
                                    .k_RE_ref = k_RE_ref,
                                    .first_sc_offset = frame_parms->first_carrier_offset,
                                    .fft_size = frame_parms->ofdm_symbol_size,
                                    .num_rb_max = frame_parms->N_RB_UL,
                                    .symbols_per_slot = frame_parms->symbols_per_slot,
                                    .dmrs_scrambling_id = pusch_pdu->ul_dmrs_scrambling_id,
                                    .scid = pusch_pdu->scid,
                                    .dmrs_port = dmrs_port,
                                    .Wt = Wt[l_prime[0]],
                                    .Wf = Wf,
                                    .dmrs_symb_pos = ul_dmrs_symb_pos,
                                    .ptrs_symb_pos = ulsch_ue->ptrs_symbols,
                                    .pdu_bit_map = pusch_pdu->pdu_bit_map,
                                    .transform_precoding = pusch_pdu->transform_precoding,
                                    .bwp_start = pusch_pdu->bwp_start,
                                    .start_rb = start_rb,
                                    .nb_rb = nb_rb,
                                    .start_symbol = start_symbol,
                                    .num_symbols = number_of_symbols,
                                    .dmrs_type = dmrs_type,
                                    .delta = delta,
                                    .num_cdm_no_data = cdm_grps_no_data};

    map_symbols(params, slot, dmrs_seq, data, tx_precoding[nl]);

  } // for (nl=0; nl < Nl; nl++)

  /////////////////////////ULSCH precoding/////////////////////////
  ///////////
  /// Layer Precoding and Antenna port mapping
  // ulsch_mod 0-3 are mapped on antenna ports
  // The precoding info is supported by nfapi such as num_prgs, prg_size, prgs_list and pm_idx
  // The same precoding matrix is applied on prg_size RBs, Thus
  //        pmi = prgs_list[rbidx/prg_size].pm_idx, rbidx =0,...,rbSize-1
  // The Precoding matrix:
  for (int ap = 0; ap < frame_parms->nb_antennas_tx; ap++) {
    for (int l = start_symbol; l < start_symbol + number_of_symbols; l++) {
      uint16_t k = start_sc;

      for (int rb = 0; rb < nb_rb; rb++) {
        // get pmi info
        uint8_t pmi = pusch_pdu->Tpmi;

        if (pmi == 0) { // unitary Precoding
          if (k + NR_NB_SC_PER_RB <= frame_parms->ofdm_symbol_size) { // RB does not cross DC
            if (ap < pusch_pdu->nrOfLayers)
              memcpy(&txdataF[ap][l * frame_parms->ofdm_symbol_size + k],
                     &tx_precoding[ap][l * frame_parms->ofdm_symbol_size + k],
                     NR_NB_SC_PER_RB * sizeof(c16_t));
            else
              memset(&txdataF[ap][l * frame_parms->ofdm_symbol_size + k], 0, NR_NB_SC_PER_RB * sizeof(int32_t));
          } else { // RB does cross DC
            int neg_length = frame_parms->ofdm_symbol_size - k;
            int pos_length = NR_NB_SC_PER_RB - neg_length;
            if (ap < pusch_pdu->nrOfLayers) {
              memcpy(&txdataF[ap][l * frame_parms->ofdm_symbol_size + k],
                     &tx_precoding[ap][l * frame_parms->ofdm_symbol_size + k],
                     neg_length * sizeof(c16_t));
              memcpy(&txdataF[ap][l * frame_parms->ofdm_symbol_size],
                     &tx_precoding[ap][l * frame_parms->ofdm_symbol_size],
                     pos_length * sizeof(int32_t));
            } else {
              memset(&txdataF[ap][l * frame_parms->ofdm_symbol_size + k], 0, neg_length * sizeof(int32_t));
              memset(&txdataF[ap][l * frame_parms->ofdm_symbol_size], 0, pos_length * sizeof(int32_t));
            }
          }
          k += NR_NB_SC_PER_RB;
          if (k >= frame_parms->ofdm_symbol_size) {
            k -= frame_parms->ofdm_symbol_size;
          }
        } else {
          // get the precoding matrix weights:
          const char *W_prec;
          switch (frame_parms->nb_antennas_tx) {
            case 1: // 1 antenna port
              W_prec = nr_W_1l_2p[pmi][ap];
              break;
            case 2: // 2 antenna ports
              if (pusch_pdu->nrOfLayers == 1) // 1 layer
                W_prec = nr_W_1l_2p[pmi][ap];
              else // 2 layers
                W_prec = nr_W_2l_2p[pmi][ap];
              break;
            case 4: // 4 antenna ports
              if (pusch_pdu->nrOfLayers == 1) // 1 layer
                W_prec = nr_W_1l_4p[pmi][ap];
              else if (pusch_pdu->nrOfLayers == 2) // 2 layers
                W_prec = nr_W_2l_4p[pmi][ap];
              else if (pusch_pdu->nrOfLayers == 3) // 3 layers
                W_prec = nr_W_3l_4p[pmi][ap];
              else // 4 layers
                W_prec = nr_W_4l_4p[pmi][ap];
              break;
            default:
              LOG_D(PHY, "Precoding 1,2, or 4 antenna ports are currently supported\n");
              W_prec = nr_W_1l_2p[pmi][ap];
              break;
          }

          for (int i = 0; i < NR_NB_SC_PER_RB; i++) {
            int32_t re_offset = l * frame_parms->ofdm_symbol_size + k;
            txdataF[ap][re_offset] = nr_layer_precoder(slot_sz, tx_precoding, W_prec, pusch_pdu->nrOfLayers, re_offset);
            if (++k >= frame_parms->ofdm_symbol_size) {
              k -= frame_parms->ofdm_symbol_size;
            }
          }
        }
      } // RB loop
    } // symbol loop
  } // port loop

  NR_UL_UE_HARQ_t *harq_process_ulsch = NULL;
  harq_process_ulsch = &UE->ul_harq_processes[harq_pid];
  harq_process_ulsch->ULstatus = SCH_IDLE;
  ///////////
  ////////////////////////////////////////////////////////////////////////
}

uint8_t nr_ue_pusch_common_procedures(PHY_VARS_NR_UE *UE,
                                      const uint8_t slot,
                                      const NR_DL_FRAME_PARMS *frame_parms,
                                      const uint8_t n_antenna_ports,
                                      c16_t **txdataF,
                                      uint32_t linktype)
{
  const int tx_offset = frame_parms->get_samples_slot_timestamp(slot, frame_parms, 0);

  int N_RB = (linktype == link_type_sl) ? frame_parms->N_RB_SL : frame_parms->N_RB_UL;

  c16_t **txdata = UE->common_vars.txData;
  for(int ap = 0; ap < n_antenna_ports; ap++) {
    apply_nr_rotation_TX(frame_parms,
                         txdataF[ap],
                         frame_parms->symbol_rotation[linktype],
                         slot,
                         N_RB,
                         0,
                         NR_NUMBER_OF_SYMBOLS_PER_SLOT);
  }

  for (int ap = 0; ap < n_antenna_ports; ap++) {
    if (frame_parms->Ncp == 1) { // extended cyclic prefix
      PHY_ofdm_mod((int *)txdataF[ap],
                   (int *)&txdata[ap][tx_offset],
                   frame_parms->ofdm_symbol_size,
                   12,
                   frame_parms->nb_prefix_samples,
                   CYCLIC_PREFIX);
    } else { // normal cyclic prefix
      nr_normal_prefix_mod(txdataF[ap],
                           &txdata[ap][tx_offset],
                           14,
                           frame_parms,
                           slot);
    }
  }

  ///////////
  ////////////////////////////////////////////////////
  return 0;
}
