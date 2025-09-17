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

#include "gtpu_extensions.h"
#include "common/utils/assertions.h"
#include "common/utils/ds/byte_array_producer.h"
#include "LOG/log.h"

/* 29.281 Figure 5.2.1-3 */
#define NR_RAN_CONTAINER        0x84
#define PDU_SESSION_CONTAINER   0x85

/* from an extension type, returns its "extension header type"
 * as defined in 29.281 Figure 5.2.1-3
 */
int serialize_gtpu_extension_type(gtpu_extension_header_type_t type)
{
  switch (type) {
    case GTPU_EXT_NONE:
      /* 0, no more extension */
      return 0;
    case GTPU_EXT_UL_PDU_SESSION_INFORMATION:
      return PDU_SESSION_CONTAINER;
    case GTPU_EXT_DL_DATA_DELIVERY_STATUS:
    case GTPU_EXT_DL_USER_DATA:
      return NR_RAN_CONTAINER;
    default:
      AssertFatal(0, "unknown GTPU extension type %d\n", type);
  }
}

/* returns 0 on error, 1 on success */
static int serialize_ul_pdu_session_information(byte_array_producer_t *b, ul_pdu_session_information_t *ext)
{
  /* see 38.415 5.5.2.2 */
  AssertFatal(!ext->qmp && !ext->dl_delay_ind && !ext->ul_delay_ind
              && !ext->snp && !ext->n3n9_delay_ind && !ext->new_ie_flag,
              "todo\n");

  uint8_t b1 = (1 << 4) | (ext->qmp << 3) | (ext->dl_delay_ind << 2) | (ext->ul_delay_ind << 1) | ext->snp;
  uint8_t b2 = (ext->n3n9_delay_ind << 7) | (ext->new_ie_flag << 6) | ext->qfi;
  return byte_array_producer_put_byte(b, b1) && byte_array_producer_put_byte(b, b2);
}

/* returns 0 on error, 1 on success */
static int serialize_dl_data_delivery_status(byte_array_producer_t *b, dl_data_delivery_status_t *ext)
{
  /* see 38.425 5.5.2.2 */
  AssertFatal(!ext->highest_delivered_nr_pdcp_sn_ind && !ext->final_frame_ind
              && !ext->lost_packet_report && !ext->delivered_nr_pdcp_sn_range_ind
              && !ext->data_rate_ind && !ext->retransmitted_nr_pdcp_sn_ind
              && !ext->delivered_retransmitted_nr_pdcp_ind && !ext->cause_report,
              "todo\n");

  uint8_t b1 = (1 << 4)
             | (ext->highest_transmitted_nr_pdcp_sn_ind << 3)
             | (ext->highest_delivered_nr_pdcp_sn_ind << 2)
             | (ext->final_frame_ind << 1)
             | ext->lost_packet_report;
  uint8_t b2 = (ext->delivered_nr_pdcp_sn_range_ind << 4)
             | (ext->data_rate_ind << 3)
             | (ext->retransmitted_nr_pdcp_sn_ind << 2)
             | (ext->delivered_retransmitted_nr_pdcp_ind << 1)
             | ext->cause_report;
  if (!byte_array_producer_put_byte(b, b1) || !byte_array_producer_put_byte(b, b2))
    return 0;

  if (!byte_array_producer_put_u32_be(b, ext->desired_buffer_size))
    return 0;

  if (ext->highest_transmitted_nr_pdcp_sn_ind)
    if (!byte_array_producer_put_u24_be(b, ext->highest_transmitted_nr_pdcp_sn))
      return 0;

  return 1;
}

/* returns 0 on error, 1 on success */
static int serialize_dl_user_data(byte_array_producer_t *b, dl_user_data_t *ext)
{
  /* see 38.425 5.5.2.1 */
  AssertFatal(!ext->dl_discard_blocks && !ext->dl_flush && !ext->report_polling
              && !ext->request_out_of_seq_report && !ext->report_delivered
              && !ext->user_data_existence_flag && !ext->assistance_info_report_polling_flag
              && !ext->retransmission_flag,
              "todo\n");

  uint8_t b1 = (0 << 4) | (ext->dl_discard_blocks << 2) | (ext->dl_flush << 1) | ext->report_polling;
  uint8_t b2 = (ext->request_out_of_seq_report << 4)
             | (ext->report_delivered << 3)
             | (ext->user_data_existence_flag << 2)
             | (ext->assistance_info_report_polling_flag << 1)
             | ext->retransmission_flag;
  if (!byte_array_producer_put_byte(b, b1) || !byte_array_producer_put_byte(b, b2))
    return 0;

  if (!byte_array_producer_put_u24_be(b, ext->nru_sequence_number))
    return 0;

  return 1;
}

/* returns -1 on error, number of serialized bytes on success */
int serialize_extension(gtpu_extension_header_t *ext, gtpu_extension_header_type_t next, uint8_t *out_buf, int out_len)
{
  byte_array_producer_t b;

  b = byte_array_producer_from_buffer(out_buf, out_len);
  /* length - will be set later */
  if (!byte_array_producer_put_byte(&b, 0))
    goto error;

  switch (ext->type) {
    case GTPU_EXT_UL_PDU_SESSION_INFORMATION:
      if (!serialize_ul_pdu_session_information(&b, &ext->ul_pdu_session_information))
        goto error;
      break;
    case GTPU_EXT_DL_DATA_DELIVERY_STATUS:
      if (!serialize_dl_data_delivery_status(&b, &ext->dl_data_delivery_status))
        goto error;
      break;
    case GTPU_EXT_DL_USER_DATA:
      if (!serialize_dl_user_data(&b, &ext->dl_user_data))
        goto error;
      break;
    default:
      LOG_E(GTPU, "unknown extension type %d\n", ext->type);
      return -1;
  }

  /* padding */
  while ((b.pos & 3) != 3)
    if (!byte_array_producer_put_byte(&b, 0))
      goto error;

  /* next */
  if (!byte_array_producer_put_byte(&b, serialize_gtpu_extension_type(next)))
    goto error;

  /* length is now know */
  DevAssert(b.pos / 4 <= 255);
  out_buf[0] = b.pos / 4;

  return b.pos;

error:
  LOG_E(GTPU, "error serializing extension\n");
  return -1;
}
