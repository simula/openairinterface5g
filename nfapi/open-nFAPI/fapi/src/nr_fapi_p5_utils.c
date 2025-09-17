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
/*! \file nfapi/open-nFAPI/fapi/src/nr_fapi_p5_utils.c
 * \brief
 * \author Ruben S. Silva
 * \date 2024
 * \version 0.1
 * \company OpenAirInterface Software Alliance
 * \email: contact@openairinterface.org, rsilva@allbesmart.pt
 * \note
 * \warning
 */
#include "nr_fapi_p5_utils.h"

void copy_vendor_extension_value(nfapi_vendor_extension_tlv_t *dst, const nfapi_vendor_extension_tlv_t *src)
{
  nfapi_tl_t *dst_tlv = (nfapi_tl_t *)dst;
  nfapi_tl_t *src_tlv = (nfapi_tl_t *)src;

  switch (dst_tlv->tag) {
    case VENDOR_EXT_TLV_2_TAG: {
      vendor_ext_tlv_2 *dst_ve = (vendor_ext_tlv_2 *)dst_tlv;
      vendor_ext_tlv_2 *src_ve = (vendor_ext_tlv_2 *)src_tlv;

      dst_ve->dummy = src_ve->dummy;
    } break;
    case VENDOR_EXT_TLV_1_TAG: {
      vendor_ext_tlv_1 *dst_ve = (vendor_ext_tlv_1 *)dst_tlv;
      vendor_ext_tlv_1 *src_ve = (vendor_ext_tlv_1 *)src_tlv;

      dst_ve->dummy = src_ve->dummy;
    } break;
  }
}

bool eq_param_request(const nfapi_nr_param_request_scf_t *unpacked_req, const nfapi_nr_param_request_scf_t *req)
{
  EQ(unpacked_req->header.message_id, req->header.message_id);
  EQ(unpacked_req->header.message_length, req->header.message_length);
  return true;
}

bool eq_param_response(const nfapi_nr_param_response_scf_t *unpacked_req, const nfapi_nr_param_response_scf_t *req)
{
  EQ(unpacked_req->header.message_id, req->header.message_id);
  EQ(unpacked_req->header.message_length, req->header.message_length);
  EQ(unpacked_req->num_tlv, req->num_tlv);
  EQ(unpacked_req->error_code, req->error_code);

  EQ_TLV(unpacked_req->cell_param.release_capability, req->cell_param.release_capability);

  EQ_TLV(unpacked_req->cell_param.phy_state, req->cell_param.phy_state);

  EQ_TLV(unpacked_req->cell_param.skip_blank_dl_config, req->cell_param.skip_blank_dl_config);

  EQ_TLV(unpacked_req->cell_param.skip_blank_ul_config, req->cell_param.skip_blank_ul_config);

  EQ_TLV(unpacked_req->cell_param.num_config_tlvs_to_report, req->cell_param.num_config_tlvs_to_report);

  for (int i = 0; i < unpacked_req->cell_param.num_config_tlvs_to_report.value; ++i) {
    EQ_TLV(unpacked_req->cell_param.config_tlvs_to_report_list[i], req->cell_param.config_tlvs_to_report_list[i]);
  }

  EQ_TLV(unpacked_req->carrier_param.cyclic_prefix, req->carrier_param.cyclic_prefix);

  EQ_TLV(unpacked_req->carrier_param.supported_subcarrier_spacings_dl, req->carrier_param.supported_subcarrier_spacings_dl);

  EQ_TLV(unpacked_req->carrier_param.supported_bandwidth_dl, req->carrier_param.supported_bandwidth_dl);

  EQ_TLV(unpacked_req->carrier_param.supported_subcarrier_spacings_ul, req->carrier_param.supported_subcarrier_spacings_ul);

  EQ_TLV(unpacked_req->carrier_param.supported_bandwidth_ul, req->carrier_param.supported_bandwidth_ul);

  EQ_TLV(unpacked_req->pdcch_param.cce_mapping_type, req->pdcch_param.cce_mapping_type);

  EQ_TLV(unpacked_req->pdcch_param.coreset_outside_first_3_of_ofdm_syms_of_slot,
         req->pdcch_param.coreset_outside_first_3_of_ofdm_syms_of_slot);

  EQ_TLV(unpacked_req->pdcch_param.coreset_precoder_granularity_coreset, req->pdcch_param.coreset_precoder_granularity_coreset);

  EQ_TLV(unpacked_req->pdcch_param.pdcch_mu_mimo, req->pdcch_param.pdcch_mu_mimo);

  EQ_TLV(unpacked_req->pdcch_param.pdcch_precoder_cycling, req->pdcch_param.pdcch_precoder_cycling);

  EQ_TLV(unpacked_req->pdcch_param.max_pdcch_per_slot, req->pdcch_param.max_pdcch_per_slot);

  EQ_TLV(unpacked_req->pucch_param.pucch_formats, req->pucch_param.pucch_formats);

  EQ_TLV(unpacked_req->pucch_param.max_pucchs_per_slot, req->pucch_param.max_pucchs_per_slot);

  EQ_TLV(unpacked_req->pdsch_param.pdsch_mapping_type, req->pdsch_param.pdsch_mapping_type);

  EQ_TLV(unpacked_req->pdsch_param.pdsch_allocation_types, req->pdsch_param.pdsch_allocation_types);

  EQ_TLV(unpacked_req->pdsch_param.pdsch_vrb_to_prb_mapping, req->pdsch_param.pdsch_vrb_to_prb_mapping);

  EQ_TLV(unpacked_req->pdsch_param.pdsch_cbg, req->pdsch_param.pdsch_cbg);

  EQ_TLV(unpacked_req->pdsch_param.pdsch_dmrs_config_types, req->pdsch_param.pdsch_dmrs_config_types);

  EQ_TLV(unpacked_req->pdsch_param.pdsch_dmrs_max_length, req->pdsch_param.pdsch_dmrs_max_length);

  EQ_TLV(unpacked_req->pdsch_param.pdsch_dmrs_additional_pos, req->pdsch_param.pdsch_dmrs_additional_pos);

  EQ_TLV(unpacked_req->pdsch_param.max_pdsch_tbs_per_slot, req->pdsch_param.max_pdsch_tbs_per_slot);

  EQ_TLV(unpacked_req->pdsch_param.max_number_mimo_layers_pdsch, req->pdsch_param.max_number_mimo_layers_pdsch);

  EQ_TLV(unpacked_req->pdsch_param.supported_max_modulation_order_dl, req->pdsch_param.supported_max_modulation_order_dl);

  EQ_TLV(unpacked_req->pdsch_param.max_mu_mimo_users_dl, req->pdsch_param.max_mu_mimo_users_dl);

  EQ_TLV(unpacked_req->pdsch_param.pdsch_data_in_dmrs_symbols, req->pdsch_param.pdsch_data_in_dmrs_symbols);

  EQ_TLV(unpacked_req->pdsch_param.premption_support, req->pdsch_param.premption_support);

  EQ_TLV(unpacked_req->pdsch_param.pdsch_non_slot_support, req->pdsch_param.pdsch_non_slot_support);

  EQ_TLV(unpacked_req->pusch_param.uci_mux_ulsch_in_pusch, req->pusch_param.uci_mux_ulsch_in_pusch);

  EQ_TLV(unpacked_req->pusch_param.uci_only_pusch, req->pusch_param.uci_only_pusch);

  EQ_TLV(unpacked_req->pusch_param.pusch_frequency_hopping, req->pusch_param.pusch_frequency_hopping);

  EQ_TLV(unpacked_req->pusch_param.pusch_dmrs_config_types, req->pusch_param.pusch_dmrs_config_types);

  EQ_TLV(unpacked_req->pusch_param.pusch_dmrs_max_len, req->pusch_param.pusch_dmrs_max_len);

  EQ_TLV(unpacked_req->pusch_param.pusch_dmrs_additional_pos, req->pusch_param.pusch_dmrs_additional_pos);

  EQ_TLV(unpacked_req->pusch_param.pusch_cbg, req->pusch_param.pusch_cbg);

  EQ_TLV(unpacked_req->pusch_param.pusch_mapping_type, req->pusch_param.pusch_mapping_type);

  EQ_TLV(unpacked_req->pusch_param.pusch_allocation_types, req->pusch_param.pusch_allocation_types);

  EQ_TLV(unpacked_req->pusch_param.pusch_vrb_to_prb_mapping, req->pusch_param.pusch_vrb_to_prb_mapping);

  EQ_TLV(unpacked_req->pusch_param.pusch_max_ptrs_ports, req->pusch_param.pusch_max_ptrs_ports);

  EQ_TLV(unpacked_req->pusch_param.max_pduschs_tbs_per_slot, req->pusch_param.max_pduschs_tbs_per_slot);

  EQ_TLV(unpacked_req->pusch_param.max_number_mimo_layers_non_cb_pusch, req->pusch_param.max_number_mimo_layers_non_cb_pusch);

  EQ_TLV(unpacked_req->pusch_param.supported_modulation_order_ul, req->pusch_param.supported_modulation_order_ul);

  EQ_TLV(unpacked_req->pusch_param.max_mu_mimo_users_ul, req->pusch_param.max_mu_mimo_users_ul);

  EQ_TLV(unpacked_req->pusch_param.dfts_ofdm_support, req->pusch_param.dfts_ofdm_support);

  EQ_TLV(unpacked_req->pusch_param.pusch_aggregation_factor, req->pusch_param.pusch_aggregation_factor);

  EQ_TLV(unpacked_req->prach_param.prach_long_formats, req->prach_param.prach_long_formats);

  EQ_TLV(unpacked_req->prach_param.prach_short_formats, req->prach_param.prach_short_formats);

  EQ_TLV(unpacked_req->prach_param.prach_restricted_sets, req->prach_param.prach_restricted_sets);

  EQ_TLV(unpacked_req->prach_param.max_prach_fd_occasions_in_a_slot, req->prach_param.max_prach_fd_occasions_in_a_slot);

  EQ_TLV(unpacked_req->measurement_param.rssi_measurement_support, req->measurement_param.rssi_measurement_support);

  return true;
}

bool eq_config_request(const nfapi_nr_config_request_scf_t *unpacked_req, const nfapi_nr_config_request_scf_t *req)
{
  EQ(unpacked_req->header.message_id, req->header.message_id);
  EQ(unpacked_req->header.message_length, req->header.message_length);
  EQ(unpacked_req->num_tlv, req->num_tlv);

  EQ_TLV(unpacked_req->carrier_config.dl_bandwidth, req->carrier_config.dl_bandwidth);

  EQ_TLV(unpacked_req->carrier_config.dl_frequency, req->carrier_config.dl_frequency);

  for (int i = 0; i < 5; ++i) {
    EQ_TLV(unpacked_req->carrier_config.dl_k0[i], req->carrier_config.dl_k0[i]);

    EQ_TLV(unpacked_req->carrier_config.dl_grid_size[i], req->carrier_config.dl_grid_size[i]);
  }

  EQ_TLV(unpacked_req->carrier_config.num_tx_ant, req->carrier_config.num_tx_ant);

  EQ_TLV(unpacked_req->carrier_config.uplink_bandwidth, req->carrier_config.uplink_bandwidth);

  EQ_TLV(unpacked_req->carrier_config.uplink_frequency, req->carrier_config.uplink_frequency);

  for (int i = 0; i < 5; ++i) {
    EQ_TLV(unpacked_req->carrier_config.ul_k0[i], req->carrier_config.ul_k0[i]);

    EQ_TLV(unpacked_req->carrier_config.ul_grid_size[i], req->carrier_config.ul_grid_size[i]);
  }

  EQ_TLV(unpacked_req->carrier_config.num_rx_ant, req->carrier_config.num_rx_ant);

  EQ_TLV(unpacked_req->carrier_config.frequency_shift_7p5khz, req->carrier_config.frequency_shift_7p5khz);

  EQ_TLV(unpacked_req->cell_config.phy_cell_id, req->cell_config.phy_cell_id);

  EQ_TLV(unpacked_req->cell_config.frame_duplex_type, req->cell_config.frame_duplex_type);

  EQ_TLV(unpacked_req->ssb_config.ss_pbch_power, req->ssb_config.ss_pbch_power);

  EQ_TLV(unpacked_req->ssb_config.bch_payload, req->ssb_config.bch_payload);

  EQ_TLV(unpacked_req->ssb_config.scs_common, req->ssb_config.scs_common);

  EQ_TLV(unpacked_req->prach_config.prach_sequence_length, req->prach_config.prach_sequence_length);

  EQ_TLV(unpacked_req->prach_config.prach_sub_c_spacing, req->prach_config.prach_sub_c_spacing);

  EQ_TLV(unpacked_req->prach_config.restricted_set_config, req->prach_config.restricted_set_config);

  EQ_TLV(unpacked_req->prach_config.num_prach_fd_occasions, req->prach_config.num_prach_fd_occasions);

  EQ_TLV(unpacked_req->prach_config.prach_ConfigurationIndex, req->prach_config.prach_ConfigurationIndex);

  for (int i = 0; i < unpacked_req->prach_config.num_prach_fd_occasions.value; i++) {
    nfapi_nr_num_prach_fd_occasions_t unpacked_prach_fd_occasion = unpacked_req->prach_config.num_prach_fd_occasions_list[i];
    nfapi_nr_num_prach_fd_occasions_t req_prach_fd_occasion = req->prach_config.num_prach_fd_occasions_list[i];

    EQ_TLV(unpacked_prach_fd_occasion.prach_root_sequence_index, req_prach_fd_occasion.prach_root_sequence_index);

    EQ_TLV(unpacked_prach_fd_occasion.num_root_sequences, req_prach_fd_occasion.num_root_sequences);

    EQ_TLV(unpacked_prach_fd_occasion.k1, req_prach_fd_occasion.k1);

    EQ_TLV(unpacked_prach_fd_occasion.prach_zero_corr_conf, req_prach_fd_occasion.prach_zero_corr_conf);

    EQ_TLV(unpacked_prach_fd_occasion.num_unused_root_sequences, req_prach_fd_occasion.num_unused_root_sequences);
    for (int k = 0; k < unpacked_prach_fd_occasion.num_unused_root_sequences.value; k++) {
      EQ_TLV(unpacked_prach_fd_occasion.unused_root_sequences_list[k], req_prach_fd_occasion.unused_root_sequences_list[k]);
    }
  }

  EQ_TLV(unpacked_req->prach_config.ssb_per_rach, req->prach_config.ssb_per_rach);

  EQ_TLV(unpacked_req->prach_config.prach_multiple_carriers_in_a_band, req->prach_config.prach_multiple_carriers_in_a_band);

  EQ_TLV(unpacked_req->ssb_table.ssb_offset_point_a, req->ssb_table.ssb_offset_point_a);

  EQ_TLV(unpacked_req->ssb_table.ssb_period, req->ssb_table.ssb_period);

  EQ_TLV(unpacked_req->ssb_table.ssb_subcarrier_offset, req->ssb_table.ssb_subcarrier_offset);

  EQ_TLV(unpacked_req->ssb_table.MIB, req->ssb_table.MIB);

  EQ_TLV(unpacked_req->ssb_table.ssb_mask_list[0].ssb_mask, req->ssb_table.ssb_mask_list[0].ssb_mask);

  EQ_TLV(unpacked_req->ssb_table.ssb_mask_list[1].ssb_mask, req->ssb_table.ssb_mask_list[1].ssb_mask);

  for (int i = 0; i < 64; i++) {
    EQ_TLV(unpacked_req->ssb_table.ssb_beam_id_list[i].beam_id, req->ssb_table.ssb_beam_id_list[i].beam_id);
  }

  EQ_TLV(unpacked_req->tdd_table.tdd_period, req->tdd_table.tdd_period);

  const uint8_t slotsperframe[5] = {10, 20, 40, 80, 160};
  // Assuming always CP_Normal, because Cyclic prefix is not included in CONFIG.request 10.02, but is present in 10.04
  uint8_t cyclicprefix = 1;
  bool normal_CP = cyclicprefix ? false : true;
  // 3GPP 38.211 Table 4.3.2.1 & Table 4.3.2.2
  uint8_t number_of_symbols_per_slot = normal_CP ? 14 : 12;

  for (int i = 0; i < slotsperframe[unpacked_req->ssb_config.scs_common.value]; i++) {
    for (int k = 0; k < number_of_symbols_per_slot; k++) {
      EQ_TLV(unpacked_req->tdd_table.max_tdd_periodicity_list[i].max_num_of_symbol_per_slot_list[k].slot_config,
             req->tdd_table.max_tdd_periodicity_list[i].max_num_of_symbol_per_slot_list[k].slot_config);
    }
  }

  EQ_TLV(unpacked_req->measurement_config.rssi_measurement, req->measurement_config.rssi_measurement);

  EQ(unpacked_req->nfapi_config.p7_vnf_address_ipv4.tl.tag, req->nfapi_config.p7_vnf_address_ipv4.tl.tag);
  for (int i = 0; i < NFAPI_IPV4_ADDRESS_LENGTH; ++i) {
    EQ(unpacked_req->nfapi_config.p7_vnf_address_ipv4.address[i], req->nfapi_config.p7_vnf_address_ipv4.address[i]);
  }

  EQ(unpacked_req->nfapi_config.p7_vnf_address_ipv6.tl.tag, req->nfapi_config.p7_vnf_address_ipv6.tl.tag);
  for (int i = 0; i < NFAPI_IPV6_ADDRESS_LENGTH; ++i) {
    EQ(unpacked_req->nfapi_config.p7_vnf_address_ipv6.address[i], req->nfapi_config.p7_vnf_address_ipv6.address[i]);
  }

  EQ_TLV(unpacked_req->nfapi_config.p7_vnf_port, req->nfapi_config.p7_vnf_port);

  EQ_TLV(unpacked_req->nfapi_config.timing_window, req->nfapi_config.timing_window);

  EQ_TLV(unpacked_req->nfapi_config.timing_info_mode, req->nfapi_config.timing_info_mode);

  EQ_TLV(unpacked_req->nfapi_config.timing_info_period, req->nfapi_config.timing_info_period);
  return true;
}

static bool eq_config_response_tlv_lists(nfapi_nr_generic_tlv_scf_t *unpacked_list,
                                         nfapi_nr_generic_tlv_scf_t *req_list,
                                         uint8_t size)
{
  for (int i = 0; i < size; ++i) {
    nfapi_nr_generic_tlv_scf_t *unpacked_element = &(unpacked_list[i]);
    nfapi_nr_generic_tlv_scf_t *req_element = &(req_list[i]);

    EQ(unpacked_element->tl.tag, req_element->tl.tag);
    EQ(unpacked_element->tl.length, req_element->tl.length);
    // check according to value type
    switch (unpacked_element->tl.length) {
      case UINT_8:
        EQ(unpacked_element->value.u8, req_element->value.u8);
        break;
      case UINT_16:
        EQ(unpacked_element->value.u16, req_element->value.u16);
        break;
      case UINT_32:
        EQ(unpacked_element->value.u32, req_element->value.u32);
        break;
      case ARRAY_UINT_16:
        for (int j = 0; j < 5; ++j) {
          EQ(unpacked_element->value.array_u16[j], req_element->value.array_u16[j]);
        }
        break;
      default:
        printf("unknown length %d\n", unpacked_element->tl.length);
        DevAssert(1 == 0);
        break;
    }
  }
  return true;
}

bool eq_config_response(const nfapi_nr_config_response_scf_t *unpacked_req, const nfapi_nr_config_response_scf_t *req)
{
  EQ(unpacked_req->header.message_id, req->header.message_id);
  EQ(unpacked_req->header.message_length, req->header.message_length);

  EQ(unpacked_req->error_code, req->error_code);
  EQ(unpacked_req->num_invalid_tlvs, req->num_invalid_tlvs);
  EQ(unpacked_req->num_invalid_tlvs_configured_in_idle, req->num_invalid_tlvs_configured_in_idle);
  EQ(unpacked_req->num_invalid_tlvs_configured_in_running, req->num_invalid_tlvs_configured_in_running);
  EQ(unpacked_req->num_missing_tlvs, req->num_missing_tlvs);
  // compare the list elements
  EQ(eq_config_response_tlv_lists(unpacked_req->invalid_tlvs_list, req->invalid_tlvs_list, req->num_invalid_tlvs), true);
  EQ(eq_config_response_tlv_lists(unpacked_req->invalid_tlvs_configured_in_idle_list,
                                  req->invalid_tlvs_configured_in_idle_list,
                                  req->num_invalid_tlvs_configured_in_idle),
     true);
  EQ(eq_config_response_tlv_lists(unpacked_req->invalid_tlvs_configured_in_running_list,
                                  req->invalid_tlvs_configured_in_running_list,
                                  req->num_invalid_tlvs_configured_in_running),
     true);
  EQ(eq_config_response_tlv_lists(unpacked_req->missing_tlvs_list, req->missing_tlvs_list, req->num_missing_tlvs), true);
  return true;
}

bool eq_start_request(const nfapi_nr_start_request_scf_t *unpacked_req, const nfapi_nr_start_request_scf_t *req)
{
  EQ(unpacked_req->header.message_id, req->header.message_id);
  EQ(unpacked_req->header.message_length, req->header.message_length);
  return true;
}

bool eq_start_response(const nfapi_nr_start_response_scf_t *unpacked_req, const nfapi_nr_start_response_scf_t *req)
{
  EQ(unpacked_req->header.message_id, req->header.message_id);
  EQ(unpacked_req->header.message_length, req->header.message_length);
  EQ(unpacked_req->error_code, req->error_code);
  return true;
}

bool eq_stop_request(const nfapi_nr_stop_request_scf_t *unpacked_req, const nfapi_nr_stop_request_scf_t *req)
{
  EQ(unpacked_req->header.message_id, req->header.message_id);
  EQ(unpacked_req->header.message_length, req->header.message_length);
  return true;
}

bool eq_stop_indication(const nfapi_nr_stop_indication_scf_t *unpacked_req, const nfapi_nr_stop_indication_scf_t *req)
{
  EQ(unpacked_req->header.message_id, req->header.message_id);
  EQ(unpacked_req->header.message_length, req->header.message_length);
  return true;
}

bool eq_error_indication(const nfapi_nr_error_indication_scf_t *unpacked_req, const nfapi_nr_error_indication_scf_t *req)
{
  EQ(unpacked_req->header.message_id, req->header.message_id);
  EQ(unpacked_req->header.message_length, req->header.message_length);
  EQ(unpacked_req->sfn, req->sfn);
  EQ(unpacked_req->slot, req->slot);
  EQ(unpacked_req->message_id, req->message_id);
  EQ(unpacked_req->error_code, req->error_code);
  return true;
}

void free_param_request(nfapi_nr_param_request_scf_t *msg)
{
  if (msg->vendor_extension) {
    free(msg->vendor_extension);
  }
}

void free_param_response(nfapi_nr_param_response_scf_t *msg)
{
  if (msg->vendor_extension) {
    free(msg->vendor_extension);
  }

  if (msg->cell_param.config_tlvs_to_report_list) {
    free(msg->cell_param.config_tlvs_to_report_list);
  }
}

void free_config_request(nfapi_nr_config_request_scf_t *msg)
{
  if (msg->vendor_extension) {
    free(msg->vendor_extension);
  }

  if (msg->prach_config.num_prach_fd_occasions_list) {
    for (int i = 0; i < msg->prach_config.num_prach_fd_occasions.value; i++) {
      nfapi_nr_num_prach_fd_occasions_t *prach_fd_occasion = &(msg->prach_config.num_prach_fd_occasions_list[i]);
      if (prach_fd_occasion->unused_root_sequences_list) {
        free(prach_fd_occasion->unused_root_sequences_list);
      }
    }
    free(msg->prach_config.num_prach_fd_occasions_list);
  }
  const uint8_t slotsperframe[5] = {10, 20, 40, 80, 160};
  if (msg->tdd_table.max_tdd_periodicity_list) {
    for (int i = 0; i < slotsperframe[msg->ssb_config.scs_common.value]; i++) {
      free(msg->tdd_table.max_tdd_periodicity_list[i].max_num_of_symbol_per_slot_list);
    }
    free(msg->tdd_table.max_tdd_periodicity_list);
  }

  if (msg->pmi_list.pmi_pdu) {
    free(msg->pmi_list.pmi_pdu);
  }
}

void free_config_response(nfapi_nr_config_response_scf_t *msg)
{
  if (msg->vendor_extension) {
    free(msg->vendor_extension);
  }

  if (msg->invalid_tlvs_list) {
    free(msg->invalid_tlvs_list);
  }

  if (msg->invalid_tlvs_configured_in_idle_list) {
    free(msg->invalid_tlvs_configured_in_idle_list);
  }

  if (msg->invalid_tlvs_configured_in_running_list) {
    free(msg->invalid_tlvs_configured_in_running_list);
  }

  if (msg->missing_tlvs_list) {
    free(msg->missing_tlvs_list);
  }
}

void free_start_request(nfapi_nr_start_request_scf_t *msg)
{
  if (msg->vendor_extension) {
    free(msg->vendor_extension);
  }
}

void free_start_response(nfapi_nr_start_response_scf_t *msg)
{
  if (msg->vendor_extension) {
    free(msg->vendor_extension);
  }
}

void free_stop_request(nfapi_nr_stop_request_scf_t *msg)
{
  if (msg->vendor_extension) {
    free(msg->vendor_extension);
  }
}

void free_stop_indication(nfapi_nr_stop_indication_scf_t *msg)
{
  if (msg->vendor_extension) {
    free(msg->vendor_extension);
  }
}

void free_error_indication(nfapi_nr_error_indication_scf_t *msg)
{
  if (msg->vendor_extension) {
    free(msg->vendor_extension);
  }
}

void copy_param_request(const nfapi_nr_param_request_scf_t *src, nfapi_nr_param_request_scf_t *dst)
{
  dst->header.message_id = src->header.message_id;
  dst->header.message_length = src->header.message_length;
  if (src->vendor_extension) {
    dst->vendor_extension = calloc(1, sizeof(nfapi_vendor_extension_tlv_t));
    dst->vendor_extension->tag = src->vendor_extension->tag;
    dst->vendor_extension->length = src->vendor_extension->length;
    copy_vendor_extension_value(&dst->vendor_extension, &src->vendor_extension);
  }
}

void copy_param_response(const nfapi_nr_param_response_scf_t *src, nfapi_nr_param_response_scf_t *dst)
{
  dst->header.message_id = src->header.message_id;
  dst->header.message_length = src->header.message_length;
  if (src->vendor_extension) {
    dst->vendor_extension = calloc(1, sizeof(nfapi_vendor_extension_tlv_t));
    dst->vendor_extension->tag = src->vendor_extension->tag;
    dst->vendor_extension->length = src->vendor_extension->length;
    copy_vendor_extension_value(&dst->vendor_extension, &src->vendor_extension);
  }

  dst->error_code = src->error_code;
  dst->num_tlv = src->num_tlv;

  COPY_TLV(dst->cell_param.release_capability, src->cell_param.release_capability);

  COPY_TLV(dst->cell_param.phy_state, src->cell_param.phy_state);

  COPY_TLV(dst->cell_param.skip_blank_dl_config, src->cell_param.skip_blank_dl_config);

  COPY_TLV(dst->cell_param.skip_blank_ul_config, src->cell_param.skip_blank_ul_config);

  COPY_TLV(dst->cell_param.num_config_tlvs_to_report, src->cell_param.num_config_tlvs_to_report);

  if (src->cell_param.config_tlvs_to_report_list) {
    dst->cell_param.config_tlvs_to_report_list =
        calloc(src->cell_param.num_config_tlvs_to_report.value, sizeof(nfapi_uint8_tlv_t *));
    for (int i = 0; i < src->cell_param.num_config_tlvs_to_report.value; ++i) {
      COPY_TLV(dst->cell_param.config_tlvs_to_report_list[i], src->cell_param.config_tlvs_to_report_list[i]);
    }
  }

  COPY_TLV(dst->carrier_param.cyclic_prefix, src->carrier_param.cyclic_prefix);

  COPY_TLV(dst->carrier_param.supported_subcarrier_spacings_dl, src->carrier_param.supported_subcarrier_spacings_dl);

  COPY_TLV(dst->carrier_param.supported_bandwidth_dl, src->carrier_param.supported_bandwidth_dl);

  COPY_TLV(dst->carrier_param.supported_subcarrier_spacings_ul, src->carrier_param.supported_subcarrier_spacings_ul);

  COPY_TLV(dst->carrier_param.supported_bandwidth_ul, src->carrier_param.supported_bandwidth_ul);

  COPY_TLV(dst->pdcch_param.cce_mapping_type, src->pdcch_param.cce_mapping_type);

  COPY_TLV(dst->pdcch_param.coreset_outside_first_3_of_ofdm_syms_of_slot,
           src->pdcch_param.coreset_outside_first_3_of_ofdm_syms_of_slot);

  COPY_TLV(dst->pdcch_param.coreset_precoder_granularity_coreset, src->pdcch_param.coreset_precoder_granularity_coreset);

  COPY_TLV(dst->pdcch_param.pdcch_mu_mimo, src->pdcch_param.pdcch_mu_mimo);

  COPY_TLV(dst->pdcch_param.pdcch_precoder_cycling, src->pdcch_param.pdcch_precoder_cycling);

  COPY_TLV(dst->pdcch_param.max_pdcch_per_slot, src->pdcch_param.max_pdcch_per_slot);

  COPY_TLV(dst->pucch_param.pucch_formats, src->pucch_param.pucch_formats);

  COPY_TLV(dst->pucch_param.max_pucchs_per_slot, src->pucch_param.max_pucchs_per_slot);

  COPY_TLV(dst->pdsch_param.pdsch_mapping_type, src->pdsch_param.pdsch_mapping_type);

  COPY_TLV(dst->pdsch_param.pdsch_allocation_types, src->pdsch_param.pdsch_allocation_types);

  COPY_TLV(dst->pdsch_param.pdsch_vrb_to_prb_mapping, src->pdsch_param.pdsch_vrb_to_prb_mapping);

  COPY_TLV(dst->pdsch_param.pdsch_cbg, src->pdsch_param.pdsch_cbg);

  COPY_TLV(dst->pdsch_param.pdsch_dmrs_config_types, src->pdsch_param.pdsch_dmrs_config_types);

  COPY_TLV(dst->pdsch_param.pdsch_dmrs_max_length, src->pdsch_param.pdsch_dmrs_max_length);

  COPY_TLV(dst->pdsch_param.pdsch_dmrs_additional_pos, src->pdsch_param.pdsch_dmrs_additional_pos);

  COPY_TLV(dst->pdsch_param.max_pdsch_tbs_per_slot, src->pdsch_param.max_pdsch_tbs_per_slot);

  COPY_TLV(dst->pdsch_param.max_number_mimo_layers_pdsch, src->pdsch_param.max_number_mimo_layers_pdsch);

  COPY_TLV(dst->pdsch_param.supported_max_modulation_order_dl, src->pdsch_param.supported_max_modulation_order_dl);

  COPY_TLV(dst->pdsch_param.max_mu_mimo_users_dl, src->pdsch_param.max_mu_mimo_users_dl);

  COPY_TLV(dst->pdsch_param.pdsch_data_in_dmrs_symbols, src->pdsch_param.pdsch_data_in_dmrs_symbols);

  COPY_TLV(dst->pdsch_param.premption_support, src->pdsch_param.premption_support);

  COPY_TLV(dst->pdsch_param.pdsch_non_slot_support, src->pdsch_param.pdsch_non_slot_support);

  COPY_TLV(dst->pusch_param.uci_mux_ulsch_in_pusch, src->pusch_param.uci_mux_ulsch_in_pusch);

  COPY_TLV(dst->pusch_param.uci_only_pusch, src->pusch_param.uci_only_pusch);

  COPY_TLV(dst->pusch_param.pusch_frequency_hopping, src->pusch_param.pusch_frequency_hopping);

  COPY_TLV(dst->pusch_param.pusch_dmrs_config_types, src->pusch_param.pusch_dmrs_config_types);

  COPY_TLV(dst->pusch_param.pusch_dmrs_max_len, src->pusch_param.pusch_dmrs_max_len);

  COPY_TLV(dst->pusch_param.pusch_dmrs_additional_pos, src->pusch_param.pusch_dmrs_additional_pos);

  COPY_TLV(dst->pusch_param.pusch_cbg, src->pusch_param.pusch_cbg);

  COPY_TLV(dst->pusch_param.pusch_mapping_type, src->pusch_param.pusch_mapping_type);

  COPY_TLV(dst->pusch_param.pusch_allocation_types, src->pusch_param.pusch_allocation_types);

  COPY_TLV(dst->pusch_param.pusch_vrb_to_prb_mapping, src->pusch_param.pusch_vrb_to_prb_mapping);

  COPY_TLV(dst->pusch_param.pusch_max_ptrs_ports, src->pusch_param.pusch_max_ptrs_ports);

  COPY_TLV(dst->pusch_param.max_pduschs_tbs_per_slot, src->pusch_param.max_pduschs_tbs_per_slot);

  COPY_TLV(dst->pusch_param.max_number_mimo_layers_non_cb_pusch, src->pusch_param.max_number_mimo_layers_non_cb_pusch);

  COPY_TLV(dst->pusch_param.supported_modulation_order_ul, src->pusch_param.supported_modulation_order_ul);

  COPY_TLV(dst->pusch_param.max_mu_mimo_users_ul, src->pusch_param.max_mu_mimo_users_ul);

  COPY_TLV(dst->pusch_param.dfts_ofdm_support, src->pusch_param.dfts_ofdm_support);

  COPY_TLV(dst->pusch_param.pusch_aggregation_factor, src->pusch_param.pusch_aggregation_factor);

  COPY_TLV(dst->prach_param.prach_long_formats, src->prach_param.prach_long_formats);

  COPY_TLV(dst->prach_param.prach_short_formats, src->prach_param.prach_short_formats);

  COPY_TLV(dst->prach_param.prach_restricted_sets, src->prach_param.prach_restricted_sets);

  COPY_TLV(dst->prach_param.max_prach_fd_occasions_in_a_slot, src->prach_param.max_prach_fd_occasions_in_a_slot);

  COPY_TLV(dst->measurement_param.rssi_measurement_support, src->measurement_param.rssi_measurement_support);

  COPY_TL(dst->nfapi_config.p7_vnf_address_ipv4.tl, src->nfapi_config.p7_vnf_address_ipv4.tl);
  memcpy(dst->nfapi_config.p7_vnf_address_ipv4.address,
         src->nfapi_config.p7_vnf_address_ipv4.address,
         sizeof(dst->nfapi_config.p7_vnf_address_ipv4.address));

  COPY_TL(dst->nfapi_config.p7_vnf_address_ipv6.tl, src->nfapi_config.p7_vnf_address_ipv6.tl);
  memcpy(dst->nfapi_config.p7_vnf_address_ipv6.address,
         src->nfapi_config.p7_vnf_address_ipv6.address,
         sizeof(dst->nfapi_config.p7_vnf_address_ipv6.address));

  COPY_TLV(dst->nfapi_config.p7_vnf_port, src->nfapi_config.p7_vnf_port);

  COPY_TL(dst->nfapi_config.p7_pnf_address_ipv4.tl, src->nfapi_config.p7_pnf_address_ipv4.tl);
  memcpy(dst->nfapi_config.p7_pnf_address_ipv4.address,
         src->nfapi_config.p7_pnf_address_ipv4.address,
         sizeof(dst->nfapi_config.p7_pnf_address_ipv4.address));

  COPY_TL(dst->nfapi_config.p7_pnf_address_ipv6.tl, src->nfapi_config.p7_pnf_address_ipv6.tl);
  memcpy(dst->nfapi_config.p7_pnf_address_ipv6.address,
         src->nfapi_config.p7_pnf_address_ipv6.address,
         sizeof(dst->nfapi_config.p7_pnf_address_ipv6.address));

  COPY_TLV(dst->nfapi_config.p7_pnf_port, src->nfapi_config.p7_pnf_port);

  COPY_TLV(dst->nfapi_config.timing_window, src->nfapi_config.timing_window);

  COPY_TLV(dst->nfapi_config.timing_info_mode, src->nfapi_config.timing_info_mode);

  COPY_TLV(dst->nfapi_config.timing_info_period, src->nfapi_config.timing_info_period);

  COPY_TLV(dst->nfapi_config.dl_tti_timing_offset, src->nfapi_config.dl_tti_timing_offset);

  COPY_TLV(dst->nfapi_config.ul_tti_timing_offset, src->nfapi_config.ul_tti_timing_offset);

  COPY_TLV(dst->nfapi_config.ul_dci_timing_offset, src->nfapi_config.ul_dci_timing_offset);

  COPY_TLV(dst->nfapi_config.tx_data_timing_offset, src->nfapi_config.tx_data_timing_offset);
}

void copy_config_request(const nfapi_nr_config_request_scf_t *src, nfapi_nr_config_request_scf_t *dst)
{
  dst->header.message_id = src->header.message_id;
  dst->header.message_length = src->header.message_length;
  if (src->vendor_extension) {
    dst->vendor_extension = calloc(1, sizeof(nfapi_vendor_extension_tlv_t));
    dst->vendor_extension->tag = src->vendor_extension->tag;
    dst->vendor_extension->length = src->vendor_extension->length;
    copy_vendor_extension_value(&dst->vendor_extension, &src->vendor_extension);
  }

  dst->num_tlv = src->num_tlv;

  COPY_TLV(dst->carrier_config.dl_bandwidth, src->carrier_config.dl_bandwidth);

  COPY_TLV(dst->carrier_config.dl_frequency, src->carrier_config.dl_frequency);

  for (int i = 0; i < 5; ++i) {
    COPY_TLV(dst->carrier_config.dl_k0[i], src->carrier_config.dl_k0[i]);

    COPY_TLV(dst->carrier_config.dl_grid_size[i], src->carrier_config.dl_grid_size[i]);
  }

  COPY_TLV(dst->carrier_config.num_tx_ant, src->carrier_config.num_tx_ant);

  COPY_TLV(dst->carrier_config.uplink_bandwidth, src->carrier_config.uplink_bandwidth);

  COPY_TLV(dst->carrier_config.uplink_frequency, src->carrier_config.uplink_frequency);

  COPY_TLV(dst->carrier_config.uplink_frequency, src->carrier_config.uplink_frequency);

  for (int i = 0; i < 5; ++i) {
    COPY_TLV(dst->carrier_config.ul_k0[i], src->carrier_config.ul_k0[i]);

    COPY_TLV(dst->carrier_config.ul_grid_size[i], src->carrier_config.ul_grid_size[i]);
  }

  COPY_TLV(dst->carrier_config.num_rx_ant, src->carrier_config.num_rx_ant);

  COPY_TLV(dst->carrier_config.frequency_shift_7p5khz, src->carrier_config.frequency_shift_7p5khz);

  COPY_TLV(dst->cell_config.phy_cell_id, src->cell_config.phy_cell_id);

  COPY_TLV(dst->cell_config.frame_duplex_type, src->cell_config.frame_duplex_type);

  COPY_TLV(dst->ssb_config.ss_pbch_power, src->ssb_config.ss_pbch_power);

  COPY_TLV(dst->ssb_config.bch_payload, src->ssb_config.bch_payload);

  COPY_TLV(dst->ssb_config.scs_common, src->ssb_config.scs_common);

  COPY_TLV(dst->prach_config.prach_sequence_length, src->prach_config.prach_sequence_length);

  COPY_TLV(dst->prach_config.prach_sub_c_spacing, src->prach_config.prach_sub_c_spacing);

  COPY_TLV(dst->prach_config.restricted_set_config, src->prach_config.restricted_set_config);

  COPY_TLV(dst->prach_config.num_prach_fd_occasions, src->prach_config.num_prach_fd_occasions);

  COPY_TLV(dst->prach_config.prach_ConfigurationIndex, src->prach_config.prach_ConfigurationIndex);

  COPY_TLV(dst->prach_config.prach_ConfigurationIndex, src->prach_config.prach_ConfigurationIndex);

  dst->prach_config.num_prach_fd_occasions_list = (nfapi_nr_num_prach_fd_occasions_t *)malloc(
      dst->prach_config.num_prach_fd_occasions.value * sizeof(nfapi_nr_num_prach_fd_occasions_t));
  for (int i = 0; i < dst->prach_config.num_prach_fd_occasions.value; i++) {
    nfapi_nr_num_prach_fd_occasions_t *src_prach_fd_occasion = &(src->prach_config.num_prach_fd_occasions_list[i]);
    nfapi_nr_num_prach_fd_occasions_t *dst_prach_fd_occasion = &(dst->prach_config.num_prach_fd_occasions_list[i]);

    COPY_TLV(dst_prach_fd_occasion->prach_root_sequence_index, src_prach_fd_occasion->prach_root_sequence_index);

    COPY_TLV(dst_prach_fd_occasion->num_root_sequences, src_prach_fd_occasion->num_root_sequences);

    COPY_TLV(dst_prach_fd_occasion->k1, src_prach_fd_occasion->k1);

    COPY_TLV(dst_prach_fd_occasion->prach_zero_corr_conf, src_prach_fd_occasion->prach_zero_corr_conf);

    COPY_TLV(dst_prach_fd_occasion->num_unused_root_sequences, src_prach_fd_occasion->num_unused_root_sequences);

    dst_prach_fd_occasion->unused_root_sequences_list =
        (nfapi_uint8_tlv_t *)malloc(dst_prach_fd_occasion->num_unused_root_sequences.value * sizeof(nfapi_uint8_tlv_t));
    for (int k = 0; k < dst_prach_fd_occasion->num_unused_root_sequences.value; k++) {
      COPY_TLV(dst_prach_fd_occasion->unused_root_sequences_list[k], src_prach_fd_occasion->unused_root_sequences_list[k]);
    }
  }

  COPY_TLV(dst->prach_config.ssb_per_rach, src->prach_config.ssb_per_rach);

  COPY_TLV(dst->prach_config.prach_multiple_carriers_in_a_band, src->prach_config.prach_multiple_carriers_in_a_band);

  COPY_TLV(dst->ssb_table.ssb_offset_point_a, src->ssb_table.ssb_offset_point_a);

  COPY_TLV(dst->ssb_table.ssb_period, src->ssb_table.ssb_period);

  COPY_TLV(dst->ssb_table.ssb_subcarrier_offset, src->ssb_table.ssb_subcarrier_offset);

  COPY_TLV(dst->ssb_table.MIB, src->ssb_table.MIB);

  COPY_TLV(dst->ssb_table.ssb_mask_list[0].ssb_mask, src->ssb_table.ssb_mask_list[0].ssb_mask);

  COPY_TLV(dst->ssb_table.ssb_mask_list[1].ssb_mask, src->ssb_table.ssb_mask_list[1].ssb_mask);

  for (int i = 0; i < 64; i++) {
    COPY_TLV(dst->ssb_table.ssb_beam_id_list[i].beam_id, src->ssb_table.ssb_beam_id_list[i].beam_id);
  }

  COPY_TLV(dst->tdd_table.tdd_period, src->tdd_table.tdd_period);

  const uint8_t slotsperframe[5] = {10, 20, 40, 80, 160};
  // Assuming always CP_Normal, because Cyclic prefix is not included in CONFIG.request 10.02, but is present in 10.04
  uint8_t cyclicprefix = 1;
  bool normal_CP = cyclicprefix ? false : true;
  // 3GPP 38.211 Table 4.3.2.1 & Table 4.3.2.2
  uint8_t number_of_symbols_per_slot = normal_CP ? 14 : 12;
  dst->tdd_table.max_tdd_periodicity_list = (nfapi_nr_max_tdd_periodicity_t *)malloc(slotsperframe[dst->ssb_config.scs_common.value]
                                                                                     * sizeof(nfapi_nr_max_tdd_periodicity_t));

  for (int i = 0; i < slotsperframe[dst->ssb_config.scs_common.value]; i++) {
    dst->tdd_table.max_tdd_periodicity_list[i].max_num_of_symbol_per_slot_list =
        (nfapi_nr_max_num_of_symbol_per_slot_t *)malloc(number_of_symbols_per_slot * sizeof(nfapi_nr_max_num_of_symbol_per_slot_t));
  }
  for (int i = 0; i < slotsperframe[dst->ssb_config.scs_common.value]; i++) { // TODO check right number of slots
    for (int k = 0; k < number_of_symbols_per_slot; k++) { // TODO can change?
      COPY_TLV(dst->tdd_table.max_tdd_periodicity_list[i].max_num_of_symbol_per_slot_list[k].slot_config,
               src->tdd_table.max_tdd_periodicity_list[i].max_num_of_symbol_per_slot_list[k].slot_config);
    }
  }

  COPY_TLV(dst->measurement_config.rssi_measurement, src->measurement_config.rssi_measurement);

  COPY_TL(dst->nfapi_config.p7_vnf_address_ipv4.tl, src->nfapi_config.p7_vnf_address_ipv4.tl);
  memcpy(dst->nfapi_config.p7_vnf_address_ipv4.address,
         src->nfapi_config.p7_vnf_address_ipv4.address,
         sizeof(dst->nfapi_config.p7_vnf_address_ipv4.address));

  COPY_TL(dst->nfapi_config.p7_vnf_address_ipv6.tl, src->nfapi_config.p7_vnf_address_ipv6.tl);
  memcpy(dst->nfapi_config.p7_vnf_address_ipv6.address,
         src->nfapi_config.p7_vnf_address_ipv6.address,
         sizeof(dst->nfapi_config.p7_vnf_address_ipv6.address));

  COPY_TLV(dst->nfapi_config.p7_vnf_port, src->nfapi_config.p7_vnf_port);

  COPY_TL(dst->nfapi_config.p7_pnf_address_ipv4.tl, src->nfapi_config.p7_pnf_address_ipv4.tl);
  memcpy(dst->nfapi_config.p7_pnf_address_ipv4.address,
         src->nfapi_config.p7_pnf_address_ipv4.address,
         sizeof(dst->nfapi_config.p7_pnf_address_ipv4.address));

  COPY_TL(dst->nfapi_config.p7_pnf_address_ipv6.tl, src->nfapi_config.p7_pnf_address_ipv6.tl);
  memcpy(dst->nfapi_config.p7_pnf_address_ipv6.address,
         src->nfapi_config.p7_pnf_address_ipv6.address,
         sizeof(dst->nfapi_config.p7_pnf_address_ipv6.address));

  COPY_TLV(dst->nfapi_config.p7_pnf_port, src->nfapi_config.p7_pnf_port);

  COPY_TLV(dst->nfapi_config.timing_window, src->nfapi_config.timing_window);

  COPY_TLV(dst->nfapi_config.timing_info_mode, src->nfapi_config.timing_info_mode);

  COPY_TLV(dst->nfapi_config.timing_info_period, src->nfapi_config.timing_info_period);

  COPY_TLV(dst->nfapi_config.dl_tti_timing_offset, src->nfapi_config.dl_tti_timing_offset);

  COPY_TLV(dst->nfapi_config.ul_tti_timing_offset, src->nfapi_config.ul_tti_timing_offset);

  COPY_TLV(dst->nfapi_config.ul_dci_timing_offset, src->nfapi_config.ul_dci_timing_offset);

  COPY_TLV(dst->nfapi_config.tx_data_timing_offset, src->nfapi_config.tx_data_timing_offset);
}

void copy_config_response(const nfapi_nr_config_response_scf_t *src, nfapi_nr_config_response_scf_t *dst)
{
  dst->header.message_id = src->header.message_id;
  dst->header.message_length = src->header.message_length;
  if (src->vendor_extension) {
    dst->vendor_extension = calloc(1, sizeof(nfapi_vendor_extension_tlv_t));
    dst->vendor_extension->tag = src->vendor_extension->tag;
    dst->vendor_extension->length = src->vendor_extension->length;
    copy_vendor_extension_value(&dst->vendor_extension, &src->vendor_extension);
  }

  dst->error_code = src->error_code;
  dst->num_invalid_tlvs = src->num_invalid_tlvs;
  dst->num_invalid_tlvs_configured_in_idle = src->num_invalid_tlvs_configured_in_idle;
  dst->num_invalid_tlvs_configured_in_running = src->num_invalid_tlvs_configured_in_running;
  dst->num_missing_tlvs = src->num_missing_tlvs;

  dst->invalid_tlvs_list = (nfapi_nr_generic_tlv_scf_t *)malloc(dst->num_invalid_tlvs * sizeof(nfapi_nr_generic_tlv_scf_t));
  dst->invalid_tlvs_configured_in_idle_list =
      (nfapi_nr_generic_tlv_scf_t *)malloc(dst->num_invalid_tlvs_configured_in_idle * sizeof(nfapi_nr_generic_tlv_scf_t));
  dst->invalid_tlvs_configured_in_running_list =
      (nfapi_nr_generic_tlv_scf_t *)malloc(dst->num_invalid_tlvs_configured_in_running * sizeof(nfapi_nr_generic_tlv_scf_t));
  dst->missing_tlvs_list = (nfapi_nr_generic_tlv_scf_t *)malloc(dst->num_missing_tlvs * sizeof(nfapi_nr_generic_tlv_scf_t));

  for (int i = 0; i < dst->num_invalid_tlvs; ++i) {
    COPY_TLV(dst->invalid_tlvs_list[i], src->invalid_tlvs_list[i]);
  }

  for (int i = 0; i < dst->num_invalid_tlvs_configured_in_idle; ++i) {
    COPY_TLV(dst->invalid_tlvs_configured_in_idle_list[i], src->invalid_tlvs_configured_in_idle_list[i]);
  }

  for (int i = 0; i < dst->num_invalid_tlvs_configured_in_running; ++i) {
    COPY_TLV(dst->invalid_tlvs_configured_in_running_list[i], src->invalid_tlvs_configured_in_running_list[i]);
  }

  for (int i = 0; i < dst->num_missing_tlvs; ++i) {
    COPY_TLV(dst->missing_tlvs_list[i], src->missing_tlvs_list[i]);
  }
}

void copy_start_request(const nfapi_nr_start_request_scf_t *src, nfapi_nr_start_request_scf_t *dst)
{
  dst->header.message_id = src->header.message_id;
  dst->header.message_length = src->header.message_length;
  if (src->vendor_extension) {
    dst->vendor_extension = calloc(1, sizeof(nfapi_vendor_extension_tlv_t));
    dst->vendor_extension->tag = src->vendor_extension->tag;
    dst->vendor_extension->length = src->vendor_extension->length;
    copy_vendor_extension_value(&dst->vendor_extension, &src->vendor_extension);
  }
}

void copy_start_response(const nfapi_nr_start_response_scf_t *src, nfapi_nr_start_response_scf_t *dst)
{
  dst->header.message_id = src->header.message_id;
  dst->header.message_length = src->header.message_length;
  if (src->vendor_extension) {
    dst->vendor_extension = calloc(1, sizeof(nfapi_vendor_extension_tlv_t));
    dst->vendor_extension->tag = src->vendor_extension->tag;
    dst->vendor_extension->length = src->vendor_extension->length;
    copy_vendor_extension_value(&dst->vendor_extension, &src->vendor_extension);
  }
  dst->error_code = src->error_code;
}

void copy_stop_request(const nfapi_nr_stop_request_scf_t *src, nfapi_nr_stop_request_scf_t *dst)
{
  dst->header.message_id = src->header.message_id;
  dst->header.message_length = src->header.message_length;
  if (src->vendor_extension) {
    dst->vendor_extension = calloc(1, sizeof(nfapi_vendor_extension_tlv_t));
    dst->vendor_extension->tag = src->vendor_extension->tag;
    dst->vendor_extension->length = src->vendor_extension->length;
    copy_vendor_extension_value(&dst->vendor_extension, &src->vendor_extension);
  }
}

void copy_stop_indication(const nfapi_nr_stop_indication_scf_t *src, nfapi_nr_stop_indication_scf_t *dst)
{
  dst->header.message_id = src->header.message_id;
  dst->header.message_length = src->header.message_length;
  if (src->vendor_extension) {
    dst->vendor_extension = calloc(1, sizeof(nfapi_vendor_extension_tlv_t));
    dst->vendor_extension->tag = src->vendor_extension->tag;
    dst->vendor_extension->length = src->vendor_extension->length;
    copy_vendor_extension_value(&dst->vendor_extension, &src->vendor_extension);
  }
}

void copy_error_indication(const nfapi_nr_error_indication_scf_t *src, nfapi_nr_error_indication_scf_t *dst)
{
  dst->header.message_id = src->header.message_id;
  dst->header.message_length = src->header.message_length;
  if (src->vendor_extension) {
    dst->vendor_extension = calloc(1, sizeof(nfapi_vendor_extension_tlv_t));
    dst->vendor_extension->tag = src->vendor_extension->tag;
    dst->vendor_extension->length = src->vendor_extension->length;
    copy_vendor_extension_value(&dst->vendor_extension, &src->vendor_extension);
  }

  dst->sfn = src->sfn;
  dst->slot = src->slot;
  dst->message_id = src->message_id;
  dst->error_code = src->error_code;
}
