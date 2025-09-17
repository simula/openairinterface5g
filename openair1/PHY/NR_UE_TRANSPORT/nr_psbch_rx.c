/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.0  (the "License"); you may not use this file
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

#include "PHY/defs_nr_UE.h"
#include "PHY/CODING/nrPolar_tools/nr_polar_psbch_defs.h"
#include "PHY/CODING/nrPolar_tools/nr_polar_defs.h"
#include "common/utils/LOG/log.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "PHY/TOOLS/phy_scope_interface.h"

// #define DEBUG_PSBCH

static void nr_psbch_extract(uint32_t rxdataF_sz,
                             c16_t rxdataF[][rxdataF_sz],
                             int estimateSz,
                             struct complex16 dl_ch_estimates[][estimateSz],
                             struct complex16 rxdataF_ext[][SL_NR_NUM_PSBCH_DATA_RE_IN_ONE_SYMBOL],
                             struct complex16 dl_ch_estimates_ext[][SL_NR_NUM_PSBCH_DATA_RE_IN_ONE_SYMBOL],
                             uint32_t symbol,
                             NR_DL_FRAME_PARMS *frame_params)
{
  uint16_t rb;
  uint8_t i, j, aarx;
  struct complex16 *dl_ch0, *dl_ch0_ext, *rxF, *rxF_ext;
  const uint8_t nb_rb = SL_NR_NUM_PSBCH_RBS_IN_ONE_SYMBOL;

  AssertFatal((symbol == 0 || symbol >= 5), "SIDELINK: PSBCH DMRS not contained in symbol %d \n", symbol);

  for (aarx = 0; aarx < frame_params->nb_antennas_rx; aarx++) {
    unsigned int rx_offset = frame_params->first_carrier_offset + frame_params->ssb_start_subcarrier;
    rx_offset = rx_offset % frame_params->ofdm_symbol_size;

    rxF = &rxdataF[aarx][symbol * frame_params->ofdm_symbol_size];
    rxF_ext = &rxdataF_ext[aarx][0];

    dl_ch0 = &dl_ch_estimates[aarx][symbol * frame_params->ofdm_symbol_size];
    dl_ch0_ext = &dl_ch_estimates_ext[aarx][0];

#ifdef DEBUG_PSBCH
    LOG_I(PHY, "extract_rbs: rx_offset=%d, symbol %u\n", (rx_offset + (symbol * frame_params->ofdm_symbol_size)), symbol);
#endif

    for (rb = 0; rb < nb_rb; rb++) {
      j = 0;

      for (i = 0; i < NR_NB_SC_PER_RB; i++) {
        if (i % 4 != 0) {
          rxF_ext[j] = rxF[rx_offset];
          dl_ch0_ext[j] = dl_ch0[i];

#ifdef DEBUG_PSBCH

          LOG_I(PHY,
                "rxF ext[%d] = (%d,%d) rxF [%u]= (%d,%d)\n",
                (9 * rb) + j,
                rxF_ext[j].r,
                rxF_ext[j].i,
                rx_offset,
                rxF[rx_offset].r,
                rxF[rx_offset].i);
          LOG_I(PHY,
                "dl ch0 ext[%d] = (%d,%d)  dl_ch0 [%d]= (%d,%d)\n",
                (9 * rb) + j,
                dl_ch0_ext[j].r,
                dl_ch0_ext[j].i,
                i,
                dl_ch0[i].r,
                dl_ch0[i].i);
#endif
          j++;
        }
        rx_offset = (rx_offset + 1) % (frame_params->ofdm_symbol_size);
      }

      rxF_ext += SL_NR_NUM_PSBCH_DATA_RE_IN_ONE_RB;
      dl_ch0_ext += SL_NR_NUM_PSBCH_DATA_RE_IN_ONE_RB;
      dl_ch0 += NR_NB_SC_PER_RB;
    }

#ifdef DEBUG_PSBCH
    char filename[40], varname[25];
    sprintf(filename, "psbch_dlch_sym_%d.m", symbol);
    sprintf(varname, "psbch_dlch%d.m", symbol);
    LOG_M(filename, varname, (void *)dl_ch0, frame_params->ofdm_symbol_size, 1, 1);
    sprintf(filename, "psbch_dlchext_sym_%d.m", symbol);
    sprintf(varname, "psbch_dlchext%d.m", symbol);
    LOG_M(filename, varname, (void *)&dl_ch_estimates_ext[0][0], SL_NR_NUM_PSBCH_DATA_RE_IN_ONE_SYMBOL, 1, 1);
#endif
  }

  return;
}

int nr_rx_psbch(PHY_VARS_NR_UE *ue,
                const UE_nr_rxtx_proc_t *proc,
                int estimateSz,
                struct complex16 dl_ch_estimates[][estimateSz],
                NR_DL_FRAME_PARMS *frame_parms,
                uint8_t *decoded_output,
                c16_t rxdataF[][frame_parms->samples_per_slot_wCP],
                uint16_t slss_id)
{
  uint32_t decoderState = 0;
  int psbch_e_rx_idx = 0;
  // Extra 2 bits needed as polar decoder expects a multiple of 4 as encoder length
  // If these 2 bits are not added, runs compiled with --sanitize will fail.
  int16_t psbch_e_rx[SL_NR_POLAR_PSBCH_E_NORMAL_CP + 2] = {0};
  int16_t psbch_unClipped[SL_NR_POLAR_PSBCH_E_NORMAL_CP + 2] = {0};

#ifdef DEBUG_PSBCH
  write_output("psbch_rxdataF.m",
               "psbchrxF",
               &rxdataF[0][0],
               frame_parms->ofdm_symbol_size * SL_NR_NUM_SYMBOLS_SSB_NORMAL_CP,
               1,
               1);
#endif
  // symbol refers to symbol within SSB. symbol_offset is the offset of the SSB wrt start of slot
  double log2_maxh = 0;

  // 0 for Normal Cyclic Prefix and 1 for EXT CyclicPrefix
  const int numsym = (frame_parms->Ncp) ? SL_NR_NUM_SYMBOLS_SSB_EXT_CP : SL_NR_NUM_SYMBOLS_SSB_NORMAL_CP;

  for (int symbol = 0; symbol < numsym;) {
    const uint16_t nb_re = SL_NR_NUM_PSBCH_DATA_RE_IN_ONE_SYMBOL;
    __attribute__((aligned(32))) struct complex16 rxdataF_ext[frame_parms->nb_antennas_rx][nb_re + 1];
    __attribute__((aligned(32))) struct complex16 dl_ch_estimates_ext[frame_parms->nb_antennas_rx][nb_re + 1];
    // memset(dl_ch_estimates_ext,0, sizeof  dl_ch_estimates_ext);
    nr_psbch_extract(frame_parms->samples_per_slot_wCP,
                     rxdataF,
                     estimateSz,
                     dl_ch_estimates,
                     rxdataF_ext,
                     dl_ch_estimates_ext,
                     symbol,
                     frame_parms);
#ifdef DEBUG_PSBCH
    LOG_I(PHY, "PSBCH RX Symbol %d ofdm size %d\n", symbol, frame_parms->ofdm_symbol_size);
#endif

    int max_h = 0;
    if (symbol == 0) {
      max_h = nr_pbch_channel_level(dl_ch_estimates_ext, frame_parms, nb_re);
      // log2_maxh = 3+(log2_approx(max_h)/2);
      log2_maxh = 5 + (log2_approx(max_h) / 2); // LLR32 crc error. LLR 16 CRC works
    }
#ifdef DEBUG_PSBCH
    LOG_I(PHY, "PSBCH RX log2_maxh = %f (%d)\n", log2_maxh, max_h);
#endif

    __attribute__((aligned(32))) struct complex16 rxdataF_comp[frame_parms->nb_antennas_rx][nb_re + 1];
    nr_pbch_channel_compensation(rxdataF_ext,
                                 dl_ch_estimates_ext,
                                 nb_re,
                                 rxdataF_comp,
                                 frame_parms,
                                 log2_maxh); // log2_maxh+I0_shift

    nr_pbch_quantize(psbch_e_rx + psbch_e_rx_idx, (short *)rxdataF_comp[0], SL_NR_NUM_PSBCH_DATA_BITS_IN_ONE_SYMBOL);

    if (ue->scopeData)
      memcpy(psbch_unClipped + psbch_e_rx_idx, rxdataF_comp[0], SL_NR_NUM_PSBCH_DATA_BITS_IN_ONE_SYMBOL * sizeof(int16_t));

    psbch_e_rx_idx += SL_NR_NUM_PSBCH_DATA_BITS_IN_ONE_SYMBOL;

    // SKIP 2 SL-PSS AND 2 SL-SSS symbols
    // Symbols carrying PSBCH 0, 5-12
    symbol = (symbol == 0) ? 5 : symbol + 1;
  }

  UEscopeCopy(ue, psbchRxdataF_comp, psbch_unClipped, sizeof(c16_t), frame_parms->nb_antennas_rx, psbch_e_rx_idx / 2, 0);
  UEscopeCopy(ue, psbchLlr, psbch_e_rx, sizeof(int16_t), frame_parms->nb_antennas_rx, psbch_e_rx_idx, 0);

#ifdef DEBUG_PSBCH
  write_output("psbch_rxdataFcomp.m", "psbch_rxFcomp", psbch_unClipped, SL_NR_NUM_PSBCH_DATA_RE_IN_ALL_SYMBOLS, 1, 1);
#endif

  // un-scrambling
  LOG_D(PHY, "PSBCH RX POLAR DECODING: total PSBCH bits:%d, rx_slss_id:%d\n", psbch_e_rx_idx, slss_id);

  nr_pbch_unscrambling(psbch_e_rx, slss_id, 0, 0, psbch_e_rx_idx, 0, 0, 0, NULL);
  // polar decoding de-rate matching
  uint64_t tmp = 0;
  decoderState = polar_decoder_int16(psbch_e_rx,
                                     (uint64_t *)&tmp,
                                     0,
                                     SL_NR_POLAR_PSBCH_MESSAGE_TYPE,
                                     SL_NR_POLAR_PSBCH_PAYLOAD_BITS,
                                     SL_NR_POLAR_PSBCH_AGGREGATION_LEVEL);

  uint32_t psbch_payload = tmp;

  if (decoderState) {
    LOG_D(PHY, "%d:%d PSBCH RX: NOK \n", proc->frame_rx, proc->nr_slot_rx);
    return (decoderState);
  }

  // Decoder reversal
  uint32_t a_reversed = 0;

  for (int i = 0; i < SL_NR_POLAR_PSBCH_PAYLOAD_BITS; i++)
    a_reversed |= (((uint64_t)psbch_payload >> i) & 1) << (31 - i);

  psbch_payload = a_reversed;

  *((uint32_t *)decoded_output) = psbch_payload;

#ifdef DEBUG_PSBCH
  for (int i = 0; i < 4; i++) {
    LOG_I(PHY, "decoded_output[%d]:%x\n", i, decoded_output[i]);
  }
#endif

  ue->symbol_offset = 0;

  // retrieve DFN and slot number from SL-MIB
  uint32_t DFN = 0, slot_offset = 0;
  DFN = (((psbch_payload & 0x0700) >> 1) | ((psbch_payload & 0xFE0000) >> 17));
  slot_offset = (((psbch_payload & 0x010000) >> 10) | ((psbch_payload & 0xFC000000) >> 26));

  LOG_D(PHY, "PSBCH RX SL-MIB:%x, decoded DFN:slot %d:%d, %x\n", psbch_payload, DFN, slot_offset, *(uint32_t *)decoded_output);

  return 0;
}
