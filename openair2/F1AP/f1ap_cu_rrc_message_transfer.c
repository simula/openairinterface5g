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

/*! \file f1ap_cu_rrc_message_transfer.c
 * \brief f1ap rrc message transfer for CU
 * \author EURECOM/NTUST
 * \date 2018
 * \version 0.1
 * \company Eurecom
 * \email: navid.nikaein@eurecom.fr, bing-kai.hong@eurecom.fr
 * \note
 * \warning
 */

#include "f1ap_common.h"
#include "f1ap_encoder.h"
#include "f1ap_ids.h"
#include "f1ap_itti_messaging.h"
#include "f1ap_cu_rrc_message_transfer.h"
#include "common/ran_context.h"
#include "openair3/UTILS/conversions.h"
#include "LAYER2/nr_pdcp/nr_pdcp_oai_api.h"

#include "f1ap_rrc_message_transfer.h"

/*
    Initial UL RRC Message Transfer
*/

int CU_handle_INITIAL_UL_RRC_MESSAGE_TRANSFER(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  f1ap_initial_ul_rrc_message_t msg;
  if (!decode_initial_ul_rrc_message_transfer(pdu, &msg)) {
    LOG_E(F1AP, "cannot decode F1 initial UL RRC message Transfer\n");
    return -1;
  }

  // create an ITTI message and copy SDU
  MessageDef *message_p = itti_alloc_new_message(TASK_CU_F1, 0, F1AP_INITIAL_UL_RRC_MESSAGE);
  message_p->ittiMsgHeader.originInstance = assoc_id;
  f1ap_initial_ul_rrc_message_t *ul_rrc = &F1AP_INITIAL_UL_RRC_MESSAGE(message_p);
  *ul_rrc = msg; /* "move" message into ITTI, RRC thread will free it */
  itti_send_msg_to_task(TASK_RRC_GNB, instance, message_p);

  return 0;
}

/*
    DL RRC Message Transfer.
*/
int CU_send_DL_RRC_MESSAGE_TRANSFER(sctp_assoc_t assoc_id, f1ap_dl_rrc_message_t *msg)
{
  /* encode DL RRC Message Transfer */
  F1AP_F1AP_PDU_t *pdu = encode_dl_rrc_message_transfer(msg);
  if (pdu == NULL) {
    LOG_E(F1AP, "Failed to encode F1 DL RRC MESSAGE TRANSFER: can't send message!\n");
    ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
    return -1;
  }
  /* encode F1AP PDU */
  uint8_t *buffer = NULL;
  uint32_t len = 0;
  if (f1ap_encode_pdu(pdu, &buffer, &len) < 0) {
    LOG_E(F1AP, "Failed to encode F1 DL RRC MESSAGE TRANSFER \n");
    ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
    return -1;
  }
  /* Free F1AP PDU */
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  /* Send to DU */
  LOG_D(F1AP, "CU send DL_RRC_MESSAGE_TRANSFER \n");
  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  return 0;
}

/*
    UL RRC Message Transfer
*/
int CU_handle_UL_RRC_MESSAGE_TRANSFER(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  if (stream != 0) {
    LOG_E(F1AP, "[SCTP %d] Received F1 on stream != 0 (%d)\n",
          assoc_id, stream);
    return -1;
  }

  f1ap_ul_rrc_message_t msg = {0};

  LOG_D(F1AP, "CU_handle_UL_RRC_MESSAGE_TRANSFER \n");
  if (!decode_ul_rrc_message_transfer(pdu, &msg)) {
    LOG_E(F1AP, "cannot decode F1 UL RRC message Transfer\n");
    free_ul_rrc_message_transfer(&msg);
    return -1;
  }

  if (!cu_exists_f1_ue_data(msg.gNB_CU_ue_id)) {
    LOG_E(F1AP, "unknown CU UE ID %d\n", msg.gNB_CU_ue_id);
    free_ul_rrc_message_transfer(&msg);
    return -1;
  }

  /* the RLC-PDCP does not transport the DU UE ID (yet), so we drop it here.
   * For the moment, let's hope this won't become relevant; to sleep in peace,
   * let's put an assert to check that it is the expected DU UE ID. */
  f1_ue_data_t ue_data = cu_get_f1_ue_data(msg.gNB_CU_ue_id);
  if (ue_data.secondary_ue != msg.gNB_DU_ue_id) {
    LOG_E(F1AP, "unexpected DU UE ID %u received, expected it to be %u\n", ue_data.secondary_ue, msg.gNB_DU_ue_id);
    free_ul_rrc_message_transfer(&msg);
    return -1;
  }

  if (msg.srb_id < 1)
    LOG_E(F1AP, "Unexpected UL RRC MESSAGE for SRB %d \n", msg.srb_id);
  else
    LOG_D(F1AP, "UL RRC MESSAGE for SRB %d in DCCH \n", msg.srb_id);

  protocol_ctxt_t ctxt = {
      .instance = instance,
      .module_id = instance,
      .enb_flag = 1,
      .eNB_index = 0,
      .rntiMaybeUEid = msg.gNB_CU_ue_id,
  };

  LOG_D(F1AP,
        "[UE %lx] calling pdcp_data_ind for SRB %d with size %d (DCCH) \n",
        ctxt.rntiMaybeUEid,
        msg.srb_id,
        msg.rrc_container_length);
  uint8_t *mb = malloc16(msg.rrc_container_length);
  memcpy(mb, msg.rrc_container, msg.rrc_container_length);

  nr_pdcp_data_ind(&ctxt,
                   1, // srb_flag
                   0, // embms_flag
                   msg.srb_id,
                   msg.rrc_container_length,
                   mb,
                   NULL,
                   NULL);
  /* Free UL RRC Message Transfer */
  free_ul_rrc_message_transfer(&msg);
  return 0;
}
