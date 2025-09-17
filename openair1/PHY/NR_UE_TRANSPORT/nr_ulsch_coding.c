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

/*! \file PHY/NR_UE_TRANSPORT/nr_ulsch_coding_slot.c
 */

#include "PHY/defs_UE.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "PHY/CODING/coding_defs.h"
#include "PHY/CODING/coding_extern.h"
#include "PHY/CODING/lte_interleaver_inline.h"
#include "PHY/CODING/nrLDPC_extern.h"
#include "PHY/CODING/nrLDPC_coding/nrLDPC_coding_interface.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_ue.h"
#include "executables/nr-uesoftmodem.h"
#include "common/utils/LOG/vcd_signal_dumper.h"
#include "PHY/log_tools.h"

int nr_ulsch_encoding(PHY_VARS_NR_UE *ue,
                      NR_UE_ULSCH_t *ulsch,
                      const uint32_t frame,
                      const uint8_t slot,
                      unsigned int *G,
                      int nb_ulsch,
                      uint8_t *ULSCH_ids,
                      uint16_t number_dmrs_symbols)
{
  start_meas_nr_ue_phy(ue, ULSCH_ENCODING_STATS);
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_NR_UE_ULSCH_ENCODING, VCD_FUNCTION_IN);

  nrLDPC_TB_encoding_parameters_t TBs[nb_ulsch];
  memset(TBs, 0, sizeof(TBs));
  nrLDPC_slot_encoding_parameters_t slot_parameters = {
    .frame = frame,
    .slot = slot,
    .nb_TBs = nb_ulsch,
    .threadPool = &get_nrUE_params()->Tpool,
    .tinput = NULL,
    .tprep = NULL,
    .tparity = NULL,
    .toutput = NULL,
    .TBs = TBs
  };

  int max_num_segments = 0;

  for (uint8_t pusch_id = 0; pusch_id < nb_ulsch; pusch_id++) {
    uint8_t ULSCH_id = ULSCH_ids[pusch_id];
    uint8_t harq_pid = ulsch[ULSCH_id].pusch_pdu.pusch_data.harq_process_id;

    nrLDPC_TB_encoding_parameters_t *TB_parameters = &TBs[pusch_id];
    /* Neither harq_pid nor ULSCH_id are unique in the instance
     * but their combination is.
     * Since ULSCH_id < 2
     * then 2 * harq_pid + ULSCH_id is unique.
     */
    TB_parameters->harq_unique_pid = 2 * harq_pid + ULSCH_id;

    /////////////////////////parameters and variables initialization/////////////////////////

    unsigned int crc = 1;
    NR_UL_UE_HARQ_t *harq_process = &ue->ul_harq_processes[harq_pid];
    const nfapi_nr_ue_pusch_pdu_t *pusch_pdu = &ulsch->pusch_pdu;
    uint16_t nb_rb = pusch_pdu->rb_size;
    uint32_t A = pusch_pdu->pusch_data.tb_size << 3;
    uint8_t Qm = pusch_pdu->qam_mod_order;
    // target_code_rate is in 0.1 units
    float Coderate = (float)pusch_pdu->target_code_rate / 10240.0f;

    LOG_D(NR_PHY, "ulsch coding nb_rb %d, Nl = %d\n", nb_rb, pusch_pdu->nrOfLayers);
    LOG_D(NR_PHY, "ulsch coding A %d G %d mod_order %d Coderate %f\n", A, G[pusch_id], Qm, Coderate);
    LOG_D(NR_PHY, "harq_pid %d, pusch_data.new_data_indicator %d\n", harq_pid, pusch_pdu->pusch_data.new_data_indicator);

    ///////////////////////// a---->| add CRC |---->b /////////////////////////

    int max_payload_bytes = MAX_NUM_NR_ULSCH_SEGMENTS_PER_LAYER * pusch_pdu->nrOfLayers * 1056;
    int B;
    if (A > NR_MAX_PDSCH_TBS) {
      // Add 24-bit crc (polynomial A) to payload
      crc = crc24a(harq_process->payload_AB, A) >> 8;
      harq_process->payload_AB[A >> 3] = ((uint8_t *)&crc)[2];
      harq_process->payload_AB[1 + (A >> 3)] = ((uint8_t *)&crc)[1];
      harq_process->payload_AB[2 + (A >> 3)] = ((uint8_t *)&crc)[0];
      B = A + 24;
      AssertFatal((A / 8) + 4 <= max_payload_bytes, "A %d is too big (A/8+4 = %d > %d)\n", A, (A / 8) + 4, max_payload_bytes);
    } else {
      // Add 16-bit crc (polynomial A) to payload
      crc = crc16(harq_process->payload_AB, A) >> 16;
      harq_process->payload_AB[A >> 3] = ((uint8_t *)&crc)[1];
      harq_process->payload_AB[1 + (A >> 3)] = ((uint8_t *)&crc)[0];
      B = A + 16;
      AssertFatal((A / 8) + 3 <= max_payload_bytes, "A %d is too big (A/8+3 = %d > %d)\n", A, (A / 8) + 3, max_payload_bytes);
    }

#if T_TRACER
    if (T_ACTIVE(T_UE_PHY_UL_PAYLOAD_TX_BITS)) {
      // Get Time Stamp for T-tracer messages
      char trace_tx_payload_time_stamp_str[30];
      get_time_stamp_usec(trace_tx_payload_time_stamp_str);
      // trace_time_stamp_str = 8 bytes timestamp = YYYYMMDD
      //                      + 9 bytes timestamp = HHMMSSMMM
      // Log UE_PHY_UL_PAYLOAD_TX_BITS using T-Tracer if activated
      // FORMAT = int,frame : int,slot : int,datetime_yyyymmdd : int,datetime_hhmmssmmm :
      // int,frame_type : int,freq_range : int,subcarrier_spacing : int,cyclic_prefix : int,symbols_per_slot :
      // int,Nid_cell : int,rnti :
      // int,rb_size : int,rb_start : int,start_symbol_index : int,nr_of_symbols :
      // int,qam_mod_order : int,mcs_index : int,mcs_table : int,nrOfLayers :
      // int,transform_precoding : int,dmrs_config_type : int,ul_dmrs_symb_pos :  int,number_dmrs_symbols : int,dmrs_port :
      // int,dmrs_nscid : nb_antennas_tx : int,number_of_bits : buffer,data Define the subcarrier spacing vector
      // int subcarrier_spacing_vect[] = {15000, 30000, 60000, 120000};
      NR_DL_FRAME_PARMS *frame_parms = &ue->frame_parms;
      int subcarrier_spacing_index = frame_parms->subcarrier_spacing / 15000 - 1;
      T(T_UE_PHY_UL_PAYLOAD_TX_BITS,
        T_INT((int)frame),
        T_INT((int)slot),
        T_INT((int)split_time_stamp_and_convert_to_int(trace_tx_payload_time_stamp_str, 0, 8)),
        T_INT((int)split_time_stamp_and_convert_to_int(trace_tx_payload_time_stamp_str, 8, 9)),
        T_INT((int)frame_parms->frame_type), // Frame type (0 FDD, 1 TDD)  frame_structure
        T_INT((int)frame_parms->freq_range), // Frequency range (0 FR1, 1 FR2)
        T_INT((int)subcarrier_spacing_index), // Subcarrier spacing (0 15kHz, 1 30kHz, 2 60kHz)
        T_INT((int)ulsch->pusch_pdu.cyclic_prefix), // Normal or extended prefix (0 normal, 1 extended)
        T_INT((int)frame_parms->symbols_per_slot), // Number of symbols per slot
        T_INT((int)frame_parms->Nid_cell),
        T_INT((int)ulsch->pusch_pdu.rnti),
        T_INT((int)ulsch->pusch_pdu.rb_size),
        T_INT((int)ulsch->pusch_pdu.rb_start),
        T_INT((int)ulsch->pusch_pdu.start_symbol_index), // start_ofdm_symbol
        T_INT((int)ulsch->pusch_pdu.nr_of_symbols), // num_ofdm_symbols
        T_INT((int)ulsch->pusch_pdu.qam_mod_order), // modulation
        T_INT((int)ulsch->pusch_pdu.mcs_index), // mcs
        T_INT((int)ulsch->pusch_pdu.mcs_table), // mcs_table_index
        T_INT((int)ulsch->pusch_pdu.nrOfLayers), // num_layer
        T_INT((int)ulsch->pusch_pdu.transform_precoding), // transformPrecoder_enabled = 0, transformPrecoder_disabled = 1
        T_INT((int)ulsch->pusch_pdu.dmrs_config_type), // dmrs_resource_map_config: pusch_dmrs_type1 = 0, pusch_dmrs_type2 = 1
        T_INT((int)ulsch->pusch_pdu.ul_dmrs_symb_pos), // used to derive the DMRS symbol positions
        T_INT((int)number_dmrs_symbols),
        // dmrs_start_ofdm_symbol
        // dmrs_duration_num_ofdm_symbols
        // dmrs_num_add_positions
        T_INT((int)get_dmrs_port(0, ulsch->pusch_pdu.dmrs_ports)), // dmrs_antenna_port
        T_INT((int)ulsch->pusch_pdu.scid), // dmrs_nscid
        T_INT((int)frame_parms->nb_antennas_tx), // number of tx antennas
        T_INT((int)A), // number_of_bits
        T_BUFFER((uint8_t *)harq_process->payload_AB, A / 8));
    }
#endif
    ///////////////////////// b---->| block segmentation |---->c /////////////////////////

    harq_process->BG = pusch_pdu->ldpcBaseGraph;

    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_NR_SEGMENTATION, VCD_FUNCTION_IN);
    start_meas_nr_ue_phy(ue, ULSCH_SEGMENTATION_STATS);
    TB_parameters->Kb = nr_segmentation(harq_process->payload_AB,
                                             harq_process->c,
                                             B,
                                             &harq_process->C,
                                             &harq_process->K,
                                             &harq_process->Z,
                                             &harq_process->F,
                                             harq_process->BG);
    TB_parameters->C = harq_process->C;
    TB_parameters->K = harq_process->K;
    TB_parameters->Z = harq_process->Z;
    TB_parameters->F = harq_process->F;
    TB_parameters->BG = harq_process->BG;
    if (TB_parameters->C > MAX_NUM_NR_DLSCH_SEGMENTS_PER_LAYER * pusch_pdu->nrOfLayers) {
      LOG_E(PHY, "nr_segmentation.c: too many segments %d, B %d\n", TB_parameters->C, B);
      return (-1);
    }
    stop_meas_nr_ue_phy(ue, ULSCH_SEGMENTATION_STATS);
    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_NR_SEGMENTATION, VCD_FUNCTION_OUT);
    max_num_segments = max(max_num_segments, TB_parameters->C);

    TB_parameters->nb_rb = nb_rb;
    TB_parameters->Qm = Qm;
    TB_parameters->mcs = pusch_pdu->mcs_index;
    TB_parameters->nb_layers = pusch_pdu->nrOfLayers;
    TB_parameters->rv_index = pusch_pdu->pusch_data.rv_index;
    TB_parameters->G = G[pusch_id];
    TB_parameters->tbslbrm = pusch_pdu->tbslbrm;
    TB_parameters->A = A;
  } // pusch_id

  nrLDPC_segment_encoding_parameters_t segments[nb_ulsch][max_num_segments];
  memset(segments, 0, sizeof(segments));

  for (uint8_t pusch_id = 0; pusch_id < nb_ulsch; pusch_id++) {
    uint8_t ULSCH_id = ULSCH_ids[pusch_id];
    uint8_t harq_pid = ulsch[ULSCH_id].pusch_pdu.pusch_data.harq_process_id;

    nrLDPC_TB_encoding_parameters_t *TB_parameters = &TBs[pusch_id];
    NR_UL_UE_HARQ_t *harq_process = &ue->ul_harq_processes[harq_pid];
    TB_parameters->segments = segments[pusch_id];

    const nfapi_nr_ue_pusch_pdu_t *pusch_pdu = &ulsch->pusch_pdu;
    uint16_t nb_rb = pusch_pdu->rb_size;
    memset(harq_process->f, 0, 14 * nb_rb * 12 * 16);
    TB_parameters->output = harq_process->f;

    for (int r = 0; r < TB_parameters->C; r++) {
      nrLDPC_segment_encoding_parameters_t *segment_parameters = &TB_parameters->segments[r];
      segment_parameters->c = harq_process->c[r];
      segment_parameters->E = nr_get_E(TB_parameters->G,
                                            TB_parameters->C,
                                            TB_parameters->Qm,
                                            TB_parameters->nb_layers,
                                            r);

      reset_meas(&segment_parameters->ts_interleave);
      reset_meas(&segment_parameters->ts_rate_match);
      reset_meas(&segment_parameters->ts_ldpc_encode);

    } // TB_parameters->C
  } // pusch_id

  ///////////////////////// | LDCP coding | ////////////////////////////////////

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_LDPC_ENCODER_OPTIM, VCD_FUNCTION_IN);

  ue->nrLDPC_coding_interface.nrLDPC_coding_encoder(&slot_parameters);

  for (uint8_t pusch_id = 0; pusch_id < nb_ulsch; pusch_id++) {
    nrLDPC_TB_encoding_parameters_t *TB_parameters = &TBs[pusch_id];
    for (int r = 0; r < TB_parameters->C; r++) {
      nrLDPC_segment_encoding_parameters_t *segment_parameters = &TB_parameters->segments[r];
      merge_meas(&ue->phy_cpu_stats.cpu_time_stats[ULSCH_INTERLEAVING_STATS], &segment_parameters->ts_interleave);
      merge_meas(&ue->phy_cpu_stats.cpu_time_stats[ULSCH_RATE_MATCHING_STATS], &segment_parameters->ts_rate_match);
      merge_meas(&ue->phy_cpu_stats.cpu_time_stats[ULSCH_LDPC_ENCODING_STATS], &segment_parameters->ts_ldpc_encode);
    }
  }

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_LDPC_ENCODER_OPTIM, VCD_FUNCTION_OUT);

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_NR_UE_ULSCH_ENCODING, VCD_FUNCTION_OUT);
  stop_meas_nr_ue_phy(ue, ULSCH_ENCODING_STATS);
  return 0;
}
