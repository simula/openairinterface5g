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
/*! \file nfapi/tests/p5/nr_fapi_config_request_test.c
 * \brief
 * \author Ruben S. Silva
 * \date 2024
 * \version 0.1
 * \company OpenAirInterface Software Alliance
 * \email: contact@openairinterface.org, rsilva@allbesmart.pt
 * \note
 * \warning
 */
#include "nfapi/tests/nr_fapi_test.h"
#include "nr_fapi_p5_utils.h"

void fill_config_request_tlv(nfapi_nr_config_request_scf_t *nfapi_resp)
{
  FILL_TLV(nfapi_resp->carrier_config.dl_bandwidth, NFAPI_NR_CONFIG_DL_BANDWIDTH_TAG, rand16());
  nfapi_resp->num_tlv++;

  FILL_TLV(nfapi_resp->carrier_config.dl_frequency, NFAPI_NR_CONFIG_DL_FREQUENCY_TAG, rand32());
  nfapi_resp->num_tlv++;

  for (int i = 0; i < 5; ++i) {
    FILL_TLV(nfapi_resp->carrier_config.dl_k0[i], NFAPI_NR_CONFIG_DL_K0_TAG, rand16());
  }
  // these 5 are 1 tlv
  nfapi_resp->num_tlv++;
  for (int i = 0; i < 5; ++i) {
    FILL_TLV(nfapi_resp->carrier_config.dl_grid_size[i], NFAPI_NR_CONFIG_DL_GRID_SIZE_TAG, rand16());
  }
  // these 5 are 1 tlv
  nfapi_resp->num_tlv++;

  FILL_TLV(nfapi_resp->carrier_config.num_tx_ant, NFAPI_NR_CONFIG_NUM_TX_ANT_TAG, rand16());
  nfapi_resp->num_tlv++;

  FILL_TLV(nfapi_resp->carrier_config.uplink_bandwidth, NFAPI_NR_CONFIG_UPLINK_BANDWIDTH_TAG, rand16());
  nfapi_resp->num_tlv++;

  FILL_TLV(nfapi_resp->carrier_config.uplink_frequency, NFAPI_NR_CONFIG_UPLINK_FREQUENCY_TAG, rand16());
  nfapi_resp->num_tlv++;

  for (int i = 0; i < 5; ++i) {
    FILL_TLV(nfapi_resp->carrier_config.ul_k0[i], NFAPI_NR_CONFIG_UL_K0_TAG, rand16());
  }
  // these 5 are 1 tlv
  nfapi_resp->num_tlv++;
  for (int i = 0; i < 5; ++i) {
    FILL_TLV(nfapi_resp->carrier_config.ul_grid_size[i], NFAPI_NR_CONFIG_UL_GRID_SIZE_TAG, rand16());
  }
  // these 5 are 1 tlv
  nfapi_resp->num_tlv++;

  FILL_TLV(nfapi_resp->carrier_config.num_rx_ant, NFAPI_NR_CONFIG_NUM_RX_ANT_TAG, rand16());
  nfapi_resp->num_tlv++;

  FILL_TLV(nfapi_resp->carrier_config.frequency_shift_7p5khz, NFAPI_NR_CONFIG_FREQUENCY_SHIFT_7P5KHZ_TAG, rand8());
  nfapi_resp->num_tlv++;

  FILL_TLV(nfapi_resp->cell_config.phy_cell_id, NFAPI_NR_CONFIG_PHY_CELL_ID_TAG, rand16());
  nfapi_resp->num_tlv++;

  FILL_TLV(nfapi_resp->cell_config.frame_duplex_type, NFAPI_NR_CONFIG_FRAME_DUPLEX_TYPE_TAG, rand8());
  nfapi_resp->num_tlv++;

  FILL_TLV(nfapi_resp->ssb_config.ss_pbch_power, NFAPI_NR_CONFIG_SS_PBCH_POWER_TAG, (int32_t)rand32());
  nfapi_resp->num_tlv++;

  FILL_TLV(nfapi_resp->ssb_config.bch_payload, NFAPI_NR_CONFIG_BCH_PAYLOAD_TAG, rand8());
  nfapi_resp->num_tlv++;

  // NOTE: MUST be between 0 & 4 ( inclusive ) , since this value is used as index to obtain slots per frame
  FILL_TLV(nfapi_resp->ssb_config.scs_common, NFAPI_NR_CONFIG_SCS_COMMON_TAG, rand8_range(0, 4));
  nfapi_resp->num_tlv++;

  FILL_TLV(nfapi_resp->prach_config.prach_sequence_length, NFAPI_NR_CONFIG_PRACH_SEQUENCE_LENGTH_TAG, rand8());
  nfapi_resp->num_tlv++;

  FILL_TLV(nfapi_resp->prach_config.prach_sub_c_spacing, NFAPI_NR_CONFIG_PRACH_SUB_C_SPACING_TAG, rand8());
  nfapi_resp->num_tlv++;

  FILL_TLV(nfapi_resp->prach_config.restricted_set_config, NFAPI_NR_CONFIG_RESTRICTED_SET_CONFIG_TAG, rand8());
  nfapi_resp->num_tlv++;
  /*Number of RACH frequency domain
    occasions. Corresponds to the
    parameter 𝑀𝑀 in [38.211, sec 6.3.3.2]
    which equals the higher layer
    parameter msg1FDM
    Value: 1,2,4,8*/
  const uint8_t num_prach_fd_ocasions[] = {1, 2, 4, 8};
  FILL_TLV(nfapi_resp->prach_config.num_prach_fd_occasions,
           NFAPI_NR_CONFIG_NUM_PRACH_FD_OCCASIONS_TAG,
           num_prach_fd_ocasions[rand8_range(0, sizeof(num_prach_fd_ocasions) - 1)]);
  nfapi_resp->num_tlv++;

  FILL_TLV(nfapi_resp->prach_config.prach_ConfigurationIndex, NFAPI_NR_CONFIG_PRACH_CONFIG_INDEX_TAG, rand8());
  nfapi_resp->num_tlv++;

  nfapi_resp->prach_config.num_prach_fd_occasions_list = (nfapi_nr_num_prach_fd_occasions_t *)malloc(
      nfapi_resp->prach_config.num_prach_fd_occasions.value * sizeof(nfapi_nr_num_prach_fd_occasions_t));
  for (int i = 0; i < nfapi_resp->prach_config.num_prach_fd_occasions.value; i++) {
    nfapi_nr_num_prach_fd_occasions_t *prach_fd_occasion = &(nfapi_resp->prach_config.num_prach_fd_occasions_list[i]);

    FILL_TLV(prach_fd_occasion->prach_root_sequence_index, NFAPI_NR_CONFIG_PRACH_ROOT_SEQUENCE_INDEX_TAG, rand16());
    nfapi_resp->num_tlv++;

    FILL_TLV(prach_fd_occasion->num_root_sequences, NFAPI_NR_CONFIG_NUM_ROOT_SEQUENCES_TAG, rand8());
    nfapi_resp->num_tlv++;

    FILL_TLV(prach_fd_occasion->k1, NFAPI_NR_CONFIG_K1_TAG, rand8());
    nfapi_resp->num_tlv++;

    FILL_TLV(prach_fd_occasion->prach_zero_corr_conf, NFAPI_NR_CONFIG_PRACH_ZERO_CORR_CONF_TAG, rand8());
    nfapi_resp->num_tlv++;

    // This doesn't make sense to be more than the total num_root_sequences
    /* SCF 222.10.02 : Number of unused sequences available
       for noise estimation per FD occasion. At
       least one unused root sequence is
       required per FD occasion.  */
    if (prach_fd_occasion->num_root_sequences.value != 0) {
      FILL_TLV(prach_fd_occasion->num_unused_root_sequences,
               NFAPI_NR_CONFIG_NUM_UNUSED_ROOT_SEQUENCES_TAG,
               rand8_range(1, prach_fd_occasion->num_root_sequences.value));
    } else {
      FILL_TLV(prach_fd_occasion->num_unused_root_sequences, NFAPI_NR_CONFIG_NUM_UNUSED_ROOT_SEQUENCES_TAG, 1);
    }
    nfapi_resp->num_tlv++;

    prach_fd_occasion->unused_root_sequences_list =
        (nfapi_uint8_tlv_t *)malloc(prach_fd_occasion->num_unused_root_sequences.value * sizeof(nfapi_uint8_tlv_t));
    for (int k = 0; k < prach_fd_occasion->num_unused_root_sequences.value; k++) {
      FILL_TLV(prach_fd_occasion->unused_root_sequences_list[k], NFAPI_NR_CONFIG_UNUSED_ROOT_SEQUENCES_TAG, rand16());
      nfapi_resp->num_tlv++;
    }
  }

  FILL_TLV(nfapi_resp->prach_config.ssb_per_rach, NFAPI_NR_CONFIG_SSB_PER_RACH_TAG, rand8());
  nfapi_resp->num_tlv++;

  FILL_TLV(nfapi_resp->prach_config.prach_multiple_carriers_in_a_band,
           NFAPI_NR_CONFIG_PRACH_MULTIPLE_CARRIERS_IN_A_BAND_TAG,
           rand8());
  nfapi_resp->num_tlv++;

  FILL_TLV(nfapi_resp->ssb_table.ssb_offset_point_a, NFAPI_NR_CONFIG_SSB_OFFSET_POINT_A_TAG, rand16());
  nfapi_resp->num_tlv++;

  FILL_TLV(nfapi_resp->ssb_table.ssb_period, NFAPI_NR_CONFIG_SSB_PERIOD_TAG, rand16());
  nfapi_resp->num_tlv++;

  FILL_TLV(nfapi_resp->ssb_table.ssb_subcarrier_offset, NFAPI_NR_CONFIG_SSB_SUBCARRIER_OFFSET_TAG, rand8());
  nfapi_resp->num_tlv++;

  FILL_TLV(nfapi_resp->ssb_table.MIB, NFAPI_NR_CONFIG_MIB_TAG, rand32());
  nfapi_resp->num_tlv++;

  FILL_TLV(nfapi_resp->ssb_table.ssb_mask_list[0].ssb_mask, NFAPI_NR_CONFIG_SSB_MASK_TAG, rand32());
  nfapi_resp->num_tlv++;

  FILL_TLV(nfapi_resp->ssb_table.ssb_mask_list[1].ssb_mask, NFAPI_NR_CONFIG_SSB_MASK_TAG, rand32());
  nfapi_resp->num_tlv++;

  for (int i = 0; i < 64; i++) {
    FILL_TLV(nfapi_resp->ssb_table.ssb_beam_id_list[i].beam_id, NFAPI_NR_CONFIG_BEAM_ID_TAG, rand8());
    nfapi_resp->num_tlv++;
  }

  FILL_TLV(nfapi_resp->tdd_table.tdd_period, NFAPI_NR_CONFIG_TDD_PERIOD_TAG, rand8());
  nfapi_resp->num_tlv++;
  const uint8_t slotsperframe[5] = {10, 20, 40, 80, 160};
  // Assuming always CP_Normal, because Cyclic prefix is not included in CONFIG.request 10.02, but is present in 10.04
  uint8_t cyclicprefix = 1;
  bool normal_CP = cyclicprefix ? false : true;
  // 3GPP 38.211 Table 4.3.2.1 & Table 4.3.2.2
  uint8_t number_of_symbols_per_slot = normal_CP ? 14 : 12;

  nfapi_resp->tdd_table.max_tdd_periodicity_list = (nfapi_nr_max_tdd_periodicity_t *)malloc(
      slotsperframe[nfapi_resp->ssb_config.scs_common.value] * sizeof(nfapi_nr_max_tdd_periodicity_t));

  for (int i = 0; i < slotsperframe[nfapi_resp->ssb_config.scs_common.value]; i++) {
    nfapi_resp->tdd_table.max_tdd_periodicity_list[i].max_num_of_symbol_per_slot_list =
        (nfapi_nr_max_num_of_symbol_per_slot_t *)malloc(number_of_symbols_per_slot * sizeof(nfapi_nr_max_num_of_symbol_per_slot_t));
  }

  for (int i = 0; i < slotsperframe[nfapi_resp->ssb_config.scs_common.value]; i++) { // TODO check right number of slots
    for (int k = 0; k < number_of_symbols_per_slot; k++) { // TODO can change?
      FILL_TLV(nfapi_resp->tdd_table.max_tdd_periodicity_list[i].max_num_of_symbol_per_slot_list[k].slot_config,
               NFAPI_NR_CONFIG_SLOT_CONFIG_TAG,
               rand8());
      nfapi_resp->num_tlv++;
    }
  }

  FILL_TLV(nfapi_resp->measurement_config.rssi_measurement, NFAPI_NR_CONFIG_RSSI_MEASUREMENT_TAG, rand8());
  nfapi_resp->num_tlv++;

  nfapi_resp->nfapi_config.p7_vnf_address_ipv4.tl.tag = NFAPI_NR_NFAPI_P7_VNF_ADDRESS_IPV4_TAG;
  for (int i = 0; i < NFAPI_IPV4_ADDRESS_LENGTH; ++i) {
    nfapi_resp->nfapi_config.p7_vnf_address_ipv4.address[i] = rand8();
  }
  nfapi_resp->num_tlv++;

  nfapi_resp->nfapi_config.p7_vnf_address_ipv6.tl.tag = NFAPI_NR_NFAPI_P7_VNF_ADDRESS_IPV6_TAG;
  for (int i = 0; i < NFAPI_IPV6_ADDRESS_LENGTH; ++i) {
    nfapi_resp->nfapi_config.p7_vnf_address_ipv6.address[i] = rand8();
  }
  nfapi_resp->num_tlv++;

  FILL_TLV(nfapi_resp->nfapi_config.p7_vnf_port, NFAPI_NR_NFAPI_P7_VNF_PORT_TAG, rand16());
  nfapi_resp->num_tlv++;

  FILL_TLV(nfapi_resp->nfapi_config.timing_window, NFAPI_NR_NFAPI_TIMING_WINDOW_TAG, rand8());
  nfapi_resp->num_tlv++;

  FILL_TLV(nfapi_resp->nfapi_config.timing_info_mode, NFAPI_NR_NFAPI_TIMING_INFO_MODE_TAG, rand8());
  nfapi_resp->num_tlv++;

  FILL_TLV(nfapi_resp->nfapi_config.timing_info_period, NFAPI_NR_NFAPI_TIMING_INFO_PERIOD_TAG, rand8());
  nfapi_resp->num_tlv++;
}

void test_pack_unpack(nfapi_nr_config_request_scf_t *req)
{
  uint8_t msg_buf[65535];
  uint16_t msg_len = sizeof(*req);

  // first test the packing procedure
  int pack_result = fapi_nr_p5_message_pack(req, msg_len, msg_buf, sizeof(msg_buf), NULL);
  // PARAM.request message body length is 0
  DevAssert(pack_result >= 0 + NFAPI_HEADER_LENGTH);
  // update req message_length value with value calculated in message_pack procedure
  req->header.message_length = pack_result - NFAPI_HEADER_LENGTH;
  // test the unpacking of the header
  // copy first NFAPI_HEADER_LENGTH bytes into a new buffer, to simulate SCTP PEEK
  fapi_message_header_t header;
  uint32_t header_buffer_size = NFAPI_HEADER_LENGTH;
  uint8_t header_buffer[header_buffer_size];
  for (int idx = 0; idx < header_buffer_size; idx++) {
    header_buffer[idx] = msg_buf[idx];
  }
  uint8_t *pReadPackedMessage = header_buffer;
  int unpack_header_result = fapi_nr_p5_message_header_unpack(&pReadPackedMessage, NFAPI_HEADER_LENGTH, &header, sizeof(header), 0);
  DevAssert(unpack_header_result >= 0);
  DevAssert(header.message_id == req->header.message_id);
  DevAssert(header.message_length == req->header.message_length);
  // test the unpacking and compare with initial message
  nfapi_nr_config_request_scf_t unpacked_req = {0};
  int unpack_result =
      fapi_nr_p5_message_unpack(msg_buf, header.message_length + NFAPI_HEADER_LENGTH, &unpacked_req, sizeof(unpacked_req), NULL);
  DevAssert(unpack_result >= 0);
  DevAssert(eq_config_request(&unpacked_req, req));
  free_config_request(&unpacked_req);
}

void test_copy(const nfapi_nr_config_request_scf_t *msg)
{
  // Test copy function
  nfapi_nr_config_request_scf_t copy = {0};
  copy_config_request(msg, &copy);
  DevAssert(eq_config_request(msg, &copy));
  free_config_request(&copy);
}

int main(int n, char *v[])
{
  fapi_test_init();
  nfapi_nr_config_request_scf_t req = {.header.message_id = NFAPI_NR_PHY_MSG_TYPE_CONFIG_REQUEST};
  // Fill CONFIG.request TVLs
  fill_config_request_tlv(&req);
  // Perform tests
  test_pack_unpack(&req);
  test_copy(&req);
  // All tests successful!
  free_config_request(&req);
  return 0;
}
