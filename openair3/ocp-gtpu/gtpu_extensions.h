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

#ifndef GTPU_EXTENSIONS_H
#define GTPU_EXTENSIONS_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
  GTPU_EXT_NONE,
  /* 38.415 */
  GTPU_EXT_UL_PDU_SESSION_INFORMATION,
  /* 38.425 */
  GTPU_EXT_DL_DATA_DELIVERY_STATUS,
  GTPU_EXT_DL_USER_DATA,
} gtpu_extension_header_type_t;

/* 38.415 */
typedef struct {
  /* not all fields are present, to be refined if needed */
  bool qmp;
  bool dl_delay_ind;
  bool ul_delay_ind;
  bool snp;
  bool n3n9_delay_ind;
  bool new_ie_flag;
  int qfi;
} ul_pdu_session_information_t;

/* 38.425 */
typedef struct {
  /* not all fields are present, to be refined if needed */
  bool highest_transmitted_nr_pdcp_sn_ind;
  bool highest_delivered_nr_pdcp_sn_ind;
  bool final_frame_ind;
  bool lost_packet_report;
  bool delivered_nr_pdcp_sn_range_ind;
  bool data_rate_ind;
  bool retransmitted_nr_pdcp_sn_ind;
  bool delivered_retransmitted_nr_pdcp_ind;
  bool cause_report;
  uint32_t desired_buffer_size;
  uint32_t highest_transmitted_nr_pdcp_sn;
} dl_data_delivery_status_t;

/* 38.425 */
typedef struct {
  /* not all fields are present, to be refined if needed */
  bool dl_discard_blocks;
  bool dl_flush;
  bool report_polling;
  bool request_out_of_seq_report;
  bool report_delivered;
  bool user_data_existence_flag;
  bool assistance_info_report_polling_flag;
  bool retransmission_flag;
  uint32_t nru_sequence_number;
} dl_user_data_t;

typedef struct {
  gtpu_extension_header_type_t type;
  union {
    ul_pdu_session_information_t ul_pdu_session_information;
    dl_data_delivery_status_t dl_data_delivery_status;
    dl_user_data_t dl_user_data;
  };
} gtpu_extension_header_t;

int serialize_gtpu_extension_type(gtpu_extension_header_type_t type);
int serialize_extension(gtpu_extension_header_t *ext, gtpu_extension_header_type_t next, uint8_t *out_buf, int out_len);

#endif /* GTPU_EXTENSIONS_H */
