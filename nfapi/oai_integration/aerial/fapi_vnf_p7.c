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

/*! \file fapi/oai-integration/fapi_vnf_p7.c
* \brief OAI FAPI VNF P7 message handler procedures
* \author Ruben S. Silva
* \date 2023
* \version 0.1
* \company OpenAirInterface Software Alliance
* \email: contact@openairinterface.org, rsilva@allbesmart.pt
* \note
* \warning
 */

#include "fapi_vnf_p7.h"
#include "nr_nfapi_p7.h"

#include "openair2/LAYER2/NR_MAC_gNB/mac_proto.h" // for handle_nr_srs_measurements()

extern pthread_cond_t nfapi_sync_cond;
extern pthread_mutex_t nfapi_sync_mutex;
extern int nfapi_sync_var;

int aerial_phy_nr_crc_indication(nfapi_nr_crc_indication_t *ind)
{
  nfapi_nr_crc_indication_t *crc_ind = CALLOC(1, sizeof(*crc_ind));
  crc_ind->header.message_id = ind->header.message_id;
  crc_ind->number_crcs = ind->number_crcs;
  crc_ind->sfn = ind->sfn;
  crc_ind->slot = ind->slot;
  if (ind->number_crcs > 0) {
    crc_ind->crc_list = CALLOC(NFAPI_NR_CRC_IND_MAX_PDU, sizeof(nfapi_nr_crc_t));
    AssertFatal(crc_ind->crc_list != NULL, "Memory not allocated for crc_ind->crc_list in phy_nr_crc_indication.");
  }
  for (int j = 0; j < ind->number_crcs; j++) {
    crc_ind->crc_list[j].handle = ind->crc_list[j].handle;
    crc_ind->crc_list[j].rnti = ind->crc_list[j].rnti;
    crc_ind->crc_list[j].harq_id = ind->crc_list[j].harq_id;
    crc_ind->crc_list[j].tb_crc_status = ind->crc_list[j].tb_crc_status;
    crc_ind->crc_list[j].num_cb = ind->crc_list[j].num_cb;
    crc_ind->crc_list[j].cb_crc_status = ind->crc_list[j].cb_crc_status;
    crc_ind->crc_list[j].ul_cqi = ind->crc_list[j].ul_cqi;
    crc_ind->crc_list[j].timing_advance = ind->crc_list[j].timing_advance;
    crc_ind->crc_list[j].rssi = ind->crc_list[j].rssi;
    if (crc_ind->crc_list[j].tb_crc_status != 0) {
      LOG_D(NR_MAC,
            "Received crc_ind.harq_id %d status %d for index %d SFN SLot %u %u with rnti %04x\n",
            crc_ind->crc_list[j].harq_id,
            crc_ind->crc_list[j].tb_crc_status,
            j,
            crc_ind->sfn,
            crc_ind->slot,
            crc_ind->crc_list[j].rnti);
    }
  }
  if (!put_queue(&gnb_crc_ind_queue, crc_ind)) {
    LOG_E(NR_MAC, "Put_queue failed for crc_ind\n");
    free(crc_ind->crc_list);
    free(crc_ind);
  }
  return 1;
}

int aerial_phy_nr_rx_data_indication(nfapi_nr_rx_data_indication_t *ind)
{
  nfapi_nr_rx_data_indication_t *rx_ind = CALLOC(1, sizeof(*rx_ind));
  rx_ind->header.message_id = ind->header.message_id;
  rx_ind->sfn = ind->sfn;
  rx_ind->slot = ind->slot;
  rx_ind->number_of_pdus = ind->number_of_pdus;

  if (ind->number_of_pdus > 0) {
    rx_ind->pdu_list = CALLOC(NFAPI_NR_RX_DATA_IND_MAX_PDU, sizeof(nfapi_nr_rx_data_pdu_t));
    AssertFatal(rx_ind->pdu_list != NULL, "Memory not allocated for rx_ind->pdu_list in phy_nr_rx_data_indication.");
  }
  for (int j = 0; j < ind->number_of_pdus; j++) {
    rx_ind->pdu_list[j].handle = ind->pdu_list[j].handle;
    rx_ind->pdu_list[j].rnti = ind->pdu_list[j].rnti;
    rx_ind->pdu_list[j].harq_id = ind->pdu_list[j].harq_id;
    rx_ind->pdu_list[j].pdu_length = ind->pdu_list[j].pdu_length;
    rx_ind->pdu_list[j].ul_cqi = ind->pdu_list[j].ul_cqi;
    rx_ind->pdu_list[j].timing_advance = ind->pdu_list[j].timing_advance;
    rx_ind->pdu_list[j].rssi = ind->pdu_list[j].rssi;
    // Only copy PDU data if there's any to copy
    if (rx_ind->pdu_list[j].pdu_length > 0) {
      rx_ind->pdu_list[j].pdu = calloc(rx_ind->pdu_list[j].pdu_length, sizeof(uint8_t));
      memcpy(rx_ind->pdu_list[j].pdu, ind->pdu_list[j].pdu, ind->pdu_list[j].pdu_length);
    }
    LOG_D(NR_MAC,
          "(%d.%d) Handle %d for index %d, RNTI, %04x, HARQID %d\n",
          ind->sfn,
          ind->slot,
          ind->pdu_list[j].handle,
          j,
          ind->pdu_list[j].rnti,
          ind->pdu_list[j].harq_id);
  }
  if (!put_queue(&gnb_rx_ind_queue, rx_ind)) {
    LOG_E(NR_MAC, "Put_queue failed for rx_ind\n");
    free(rx_ind->pdu_list);
    free(rx_ind);
  }
  return 1;
}

int aerial_phy_nr_rach_indication(nfapi_nr_rach_indication_t *ind)
{
  nfapi_nr_rach_indication_t *rach_ind = CALLOC(1, sizeof(*rach_ind));
  rach_ind->header.message_id = ind->header.message_id;
  rach_ind->sfn = ind->sfn;
  rach_ind->slot = ind->slot;
  rach_ind->number_of_pdus = ind->number_of_pdus;
  rach_ind->pdu_list = CALLOC(rach_ind->number_of_pdus, sizeof(*rach_ind->pdu_list));
  AssertFatal(rach_ind->pdu_list != NULL, "Memory not allocated for rach_ind->pdu_list in phy_nr_rach_indication.");
  for (int i = 0; i < ind->number_of_pdus; i++) {
    rach_ind->pdu_list[i].phy_cell_id = ind->pdu_list[i].phy_cell_id;
    rach_ind->pdu_list[i].symbol_index = ind->pdu_list[i].symbol_index;
    rach_ind->pdu_list[i].slot_index = ind->pdu_list[i].slot_index;
    rach_ind->pdu_list[i].freq_index = ind->pdu_list[i].freq_index;
    rach_ind->pdu_list[i].avg_rssi = ind->pdu_list[i].avg_rssi;
    rach_ind->pdu_list[i].avg_snr = ind->pdu_list[i].avg_snr;
    rach_ind->pdu_list[i].num_preamble = ind->pdu_list[i].num_preamble;
    for (int j = 0; j < ind->pdu_list[i].num_preamble; j++) {
      rach_ind->pdu_list[i].preamble_list[j].preamble_index = ind->pdu_list[i].preamble_list[j].preamble_index;
      rach_ind->pdu_list[i].preamble_list[j].timing_advance = ind->pdu_list[i].preamble_list[j].timing_advance;
      rach_ind->pdu_list[i].preamble_list[j].preamble_pwr = ind->pdu_list[i].preamble_list[j].preamble_pwr;
    }
  }
  if (!put_queue(&gnb_rach_ind_queue, rach_ind)) {
    LOG_E(NR_MAC, "Put_queue failed for rach_ind\n");
    free(rach_ind->pdu_list);
    free(rach_ind);
  } else {
    LOG_I(NR_MAC, "RACH.indication put_queue successfull\n");
  }
  return 1;
}

int aerial_phy_nr_uci_indication(nfapi_nr_uci_indication_t *ind)
{
  nfapi_nr_uci_indication_t *uci_ind = CALLOC(1, sizeof(*uci_ind));
  AssertFatal(uci_ind, "Memory not allocated for uci_ind in phy_nr_uci_indication.");
  *uci_ind = *ind;

  uci_ind->uci_list = CALLOC(NFAPI_NR_UCI_IND_MAX_PDU, sizeof(nfapi_nr_uci_t));
  AssertFatal(uci_ind->uci_list != NULL, "Memory not allocated for uci_ind->uci_list in phy_nr_uci_indication.");
  for (int i = 0; i < ind->num_ucis; i++) {
    uci_ind->uci_list[i] = ind->uci_list[i];

    switch (uci_ind->uci_list[i].pdu_type) {
      case NFAPI_NR_UCI_PUSCH_PDU_TYPE:
        LOG_E(MAC, "%s(): unhandled NFAPI_NR_UCI_PUSCH_PDU_TYPE\n", __func__);
        break;

      case NFAPI_NR_UCI_FORMAT_0_1_PDU_TYPE: {
//          nfapi_nr_uci_pucch_pdu_format_0_1_t *uci_ind_pdu = &uci_ind->uci_list[i].pucch_pdu_format_0_1;
//          nfapi_nr_uci_pucch_pdu_format_0_1_t *ind_pdu = &ind->uci_list[i].pucch_pdu_format_0_1;
//          if (ind_pdu->sr) {
//            uci_ind_pdu->sr = CALLOC(1, sizeof(*uci_ind_pdu->sr));
//            AssertFatal(uci_ind_pdu->sr != NULL, "Memory not allocated for uci_ind_pdu->harq in phy_nr_uci_indication.");
//            *uci_ind_pdu->sr = *ind_pdu->sr;
//          }
//          if (ind_pdu->harq) {
//            uci_ind_pdu->harq = CALLOC(1, sizeof(*uci_ind_pdu->harq));
//            AssertFatal(uci_ind_pdu->harq != NULL, "Memory not allocated for uci_ind_pdu->harq in phy_nr_uci_indication.");
//
//            *uci_ind_pdu->harq = *ind_pdu->harq;
//            uci_ind_pdu->harq->harq_list = CALLOC(uci_ind_pdu->harq->num_harq, sizeof(*uci_ind_pdu->harq->harq_list));
//            AssertFatal(uci_ind_pdu->harq->harq_list != NULL,
//                        "Memory not allocated for uci_ind_pdu->harq->harq_list in phy_nr_uci_indication.");
//            for (int j = 0; j < uci_ind_pdu->harq->num_harq; j++)
//              uci_ind_pdu->harq->harq_list[j].harq_value = ind_pdu->harq->harq_list[j].harq_value;
//          }
        break;
      }

      case NFAPI_NR_UCI_FORMAT_2_3_4_PDU_TYPE: {
        nfapi_nr_uci_pucch_pdu_format_2_3_4_t *uci_ind_pdu = &uci_ind->uci_list[i].pucch_pdu_format_2_3_4;
        nfapi_nr_uci_pucch_pdu_format_2_3_4_t *ind_pdu = &ind->uci_list[i].pucch_pdu_format_2_3_4;
        *uci_ind_pdu = *ind_pdu;
        if (ind_pdu->harq.harq_payload) {
          uci_ind_pdu->harq.harq_payload = CALLOC(1, sizeof(*uci_ind_pdu->harq.harq_payload));
          AssertFatal(uci_ind_pdu->harq.harq_payload != NULL,
                      "Memory not allocated for uci_ind_pdu->harq.harq_payload in phy_nr_uci_indication.");
          *uci_ind_pdu->harq.harq_payload = *ind_pdu->harq.harq_payload;
        }
        if (ind_pdu->sr.sr_payload) {
          uci_ind_pdu->sr.sr_payload = CALLOC(1, sizeof(*uci_ind_pdu->sr.sr_payload));
          AssertFatal(uci_ind_pdu->sr.sr_payload != NULL,
                      "Memory not allocated for uci_ind_pdu->sr.sr_payload in phy_nr_uci_indication.");
          //SCF222.10.02 sr_bit_len values from 1 to 8, payload always just one byte
          uci_ind_pdu->sr.sr_payload[0] = ind_pdu->sr.sr_payload[0];
        }
        if (ind_pdu->csi_part1.csi_part1_payload) {
          uint8_t byte_len = (ind_pdu->csi_part1.csi_part1_bit_len / 8) + 1;
          uci_ind_pdu->csi_part1.csi_part1_payload = calloc(byte_len, sizeof(uint8_t));
          AssertFatal(uci_ind_pdu->csi_part1.csi_part1_payload != NULL,
                      "Memory not allocated for uci_ind_pdu->csi_part1.csi_part1_payload in phy_nr_uci_indication.");
          memcpy(uci_ind_pdu->csi_part1.csi_part1_payload,ind_pdu->csi_part1.csi_part1_payload,byte_len);
        }
        if (ind_pdu->csi_part2.csi_part2_payload) {
          uint8_t byte_len = (ind_pdu->csi_part2.csi_part2_bit_len / 8) + 1;
          uci_ind_pdu->csi_part2.csi_part2_payload = calloc(byte_len, sizeof(uint8_t));
          AssertFatal(uci_ind_pdu->csi_part2.csi_part2_payload != NULL,
                      "Memory not allocated for uci_ind_pdu->csi_part2.csi_part2_payload in phy_nr_uci_indication.");
          memcpy(uci_ind_pdu->csi_part2.csi_part2_payload,ind_pdu->csi_part2.csi_part2_payload,byte_len);
        }
        break;
      }
    }
  }

  if (!put_queue(&gnb_uci_ind_queue, uci_ind)) {
    LOG_E(NR_MAC, "Put_queue failed for uci_ind\n");
    for (int i = 0; i < ind->num_ucis; i++) {
      if (uci_ind->uci_list[i].pdu_type == NFAPI_NR_UCI_FORMAT_0_1_PDU_TYPE) {
//          if (uci_ind->uci_list[i].pucch_pdu_format_0_1.harq) {
//            free(uci_ind->uci_list[i].pucch_pdu_format_0_1.harq->harq_list);
//            uci_ind->uci_list[i].pucch_pdu_format_0_1.harq->harq_list = NULL;
//            free(uci_ind->uci_list[i].pucch_pdu_format_0_1.harq);
//            uci_ind->uci_list[i].pucch_pdu_format_0_1.harq = NULL;
//          }
//          if (uci_ind->uci_list[i].pucch_pdu_format_0_1.sr) {
//            free(uci_ind->uci_list[i].pucch_pdu_format_0_1.sr);
//            uci_ind->uci_list[i].pucch_pdu_format_0_1.sr = NULL;
//          }
      }
      if (uci_ind->uci_list[i].pdu_type == NFAPI_NR_UCI_FORMAT_2_3_4_PDU_TYPE) {
        free(uci_ind->uci_list[i].pucch_pdu_format_2_3_4.harq.harq_payload);
        free(uci_ind->uci_list[i].pucch_pdu_format_2_3_4.csi_part1.csi_part1_payload);
        free(uci_ind->uci_list[i].pucch_pdu_format_2_3_4.csi_part2.csi_part2_payload);
      }
    }
    free(uci_ind->uci_list);
    uci_ind->uci_list = NULL;
    free(uci_ind);
    uci_ind = NULL;
  }
  return 1;
}

NR_Sched_Rsp_t g_sched_resp;
void gNB_dlsch_ulsch_scheduler(module_id_t module_idP, frame_t frame, sub_frame_t slot, NR_Sched_Rsp_t* sched_info);
int oai_fapi_dl_tti_req(nfapi_nr_dl_tti_request_t *dl_config_req);
int oai_fapi_ul_tti_req(nfapi_nr_ul_tti_request_t *ul_tti_req);
int oai_fapi_tx_data_req(nfapi_nr_tx_data_request_t* tx_data_req);
int oai_fapi_ul_dci_req(nfapi_nr_ul_dci_request_t* ul_dci_req);
int oai_fapi_send_end_request(int cell, uint32_t frame, uint32_t slot);

int trigger_scheduler(nfapi_nr_slot_indication_scf_t *slot_ind)
{

  NR_UL_IND_t ind = {.frame = slot_ind->sfn, .slot = slot_ind->slot, };
  NR_UL_indication(&ind);
  // Call into the scheduler (this is hardcoded and should be init properly!)
  // memset(sched_resp, 0, sizeof(*sched_resp));
  gNB_dlsch_ulsch_scheduler(0, slot_ind->sfn, slot_ind->slot, &g_sched_resp);

  bool send_slt_resp = false;
  if (g_sched_resp.DL_req.dl_tti_request_body.nPDUs> 0) {
    oai_fapi_dl_tti_req(&g_sched_resp.DL_req);
    send_slt_resp = true;
  }
  if (g_sched_resp.UL_tti_req.n_pdus > 0) {
    oai_fapi_ul_tti_req(&g_sched_resp.UL_tti_req);
    send_slt_resp = true;
  }
  if (g_sched_resp.TX_req.Number_of_PDUs > 0) {
    oai_fapi_tx_data_req(&g_sched_resp.TX_req);
    send_slt_resp = true;
  }
  if (g_sched_resp.UL_dci_req.numPdus > 0) {
    oai_fapi_ul_dci_req(&g_sched_resp.UL_dci_req);
    send_slt_resp = true;
  }
  if (send_slt_resp) {
    oai_fapi_send_end_request(0,slot_ind->sfn, slot_ind->slot);
  }

  return 1;
}


int aerial_phy_nr_slot_indication(nfapi_nr_slot_indication_scf_t *ind)
{
  uint8_t vnf_slot_ahead = 0;
  uint32_t vnf_sfn_slot = sfnslot_add_slot(ind->sfn, ind->slot, vnf_slot_ahead);
  uint16_t vnf_sfn = NFAPI_SFNSLOT2SFN(vnf_sfn_slot);
  uint8_t vnf_slot = NFAPI_SFNSLOT2SLOT(vnf_sfn_slot);
  LOG_D(MAC, "VNF SFN/Slot %d.%d \n", vnf_sfn, vnf_slot);
  // printf( "VNF SFN/Slot %d.%d \n", vnf_sfn, vnf_slot);
  trigger_scheduler(ind);

  return 1;
}

int aerial_phy_nr_srs_indication(nfapi_nr_srs_indication_t *ind)
{
  // or queue to decouple, but I think it should be fast (in all likelihood,
  // the VNF has nothing to do)
  for (int i = 0; i < ind->number_of_pdus; ++i)
    handle_nr_srs_measurements(0, ind->sfn, ind->slot, &ind->pdu_list[i]);
  return 1;
}

void *aerial_vnf_allocate(size_t size)
{
  // return (void*)memory_pool::allocate(size);
  return (void *)malloc(size);
}

void aerial_vnf_deallocate(void *ptr)
{
  // memory_pool::deallocate((uint8_t*)ptr);
  free(ptr);
}

int aerial_phy_vendor_ext(struct nfapi_vnf_p7_config *config, nfapi_p7_message_header_t *msg)
{
  if (msg->message_id == P7_VENDOR_EXT_IND) {
    // vendor_ext_p7_ind* ind = (vendor_ext_p7_ind*)msg;
    // NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] vendor_ext (error_code:%d)\n", ind->error_code);
  } else {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] unknown %02x\n", msg->message_id);
  }

  return 0;
}

int aerial_phy_unpack_p7_vendor_extension(nfapi_p7_message_header_t *header,
                                          uint8_t **ppReadPackedMessage,
                                          uint8_t *end,
                                          nfapi_p7_codec_config_t *config)
{
  // NFAPI_TRACE(NFAPI_TRACE_INFO, "%s\n", __FUNCTION__);
  if (header->message_id == P7_VENDOR_EXT_IND) {
    vendor_ext_p7_ind *req = (vendor_ext_p7_ind *)(header);

    if (!pull16(ppReadPackedMessage, &req->error_code, end))
      return 0;
  }

  return 1;
}

int aerial_phy_pack_p7_vendor_extension(nfapi_p7_message_header_t *header,
                                        uint8_t **ppWritePackedMsg,
                                        uint8_t *end,
                                        nfapi_p7_codec_config_t *config)
{
  // NFAPI_TRACE(NFAPI_TRACE_INFO, "%s\n", __FUNCTION__);
  if (header->message_id == P7_VENDOR_EXT_REQ) {
    // NFAPI_TRACE(NFAPI_TRACE_INFO, "%s\n", __FUNCTION__);
    vendor_ext_p7_req *req = (vendor_ext_p7_req *)(header);

    if (!(push16(req->dummy1, ppWritePackedMsg, end) && push16(req->dummy2, ppWritePackedMsg, end)))
      return 0;
  }

  return 1;
}

int aerial_phy_unpack_vendor_extension_tlv(nfapi_tl_t *tl,
                                           uint8_t **ppReadPackedMessage,
                                           uint8_t *end,
                                           void **ve,
                                           nfapi_p7_codec_config_t *codec)
{
  (void)tl;
  (void)ppReadPackedMessage;
  (void)ve;
  return -1;
}

int aerial_phy_pack_vendor_extension_tlv(void *ve, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *codec)
{
  // NFAPI_TRACE(NFAPI_TRACE_INFO, "phy_pack_vendor_extension_tlv\n");
  nfapi_tl_t *tlv = (nfapi_tl_t *)ve;

  switch (tlv->tag) {
    case VENDOR_EXT_TLV_1_TAG: {
      // NFAPI_TRACE(NFAPI_TRACE_INFO, "Packing VENDOR_EXT_TLV_1\n");
      vendor_ext_tlv_1 *ve = (vendor_ext_tlv_1 *)tlv;

      if (!push32(ve->dummy, ppWritePackedMsg, end))
        return 0;

      return 1;
    } break;

    default:
      return -1;
      break;
  }
}

nfapi_p7_message_header_t *aerial_phy_allocate_p7_vendor_ext(uint16_t message_id, uint16_t *msg_size)
{
  if (message_id == P7_VENDOR_EXT_IND) {
    *msg_size = sizeof(vendor_ext_p7_ind);
    return (nfapi_p7_message_header_t *)malloc(sizeof(vendor_ext_p7_ind));
  }

  return 0;
}

void aerial_phy_deallocate_p7_vendor_ext(nfapi_p7_message_header_t *header)
{
  free(header);
}

uint8_t aerial_unpack_nr_slot_indication(uint8_t **ppReadPackedMsg,
                                         uint8_t *end,
                                         nfapi_nr_slot_indication_scf_t *msg,
                                         nfapi_p7_codec_config_t *config)
{
  return unpack_nr_slot_indication(ppReadPackedMsg, end, msg, config);
}

static uint8_t aerial_unpack_nr_rx_data_indication_body(nfapi_nr_rx_data_pdu_t *value,
                                                        uint8_t **ppReadPackedMsg,
                                                        uint8_t *end,
                                                        nfapi_p7_codec_config_t *config)
{
  if (!(pull32(ppReadPackedMsg, &value->handle, end) && pull16(ppReadPackedMsg, &value->rnti, end)
        && pull8(ppReadPackedMsg, &value->harq_id, end) &&
        // For Aerial, RX_DATA.indication PDULength is changed to 32 bit field
        pull32(ppReadPackedMsg, &value->pdu_length, end) && pull8(ppReadPackedMsg, &value->ul_cqi, end)
        && pull16(ppReadPackedMsg, &value->timing_advance, end) && pull16(ppReadPackedMsg, &value->rssi, end))) {
    return 0;
  }

  // Allocate space for the pdu to be unpacked later
  value->pdu = nfapi_p7_allocate(sizeof(*value->pdu) * value->pdu_length, config);

  return 1;
}
uint8_t aerial_unpack_nr_rx_data_indication(uint8_t **ppReadPackedMsg,
                                            uint8_t *end,
                                            uint8_t **pDataMsg,
                                            uint8_t *data_end,
                                            nfapi_nr_rx_data_indication_t *msg,
                                            nfapi_p7_codec_config_t *config)
{
  // For Aerial unpacking, the PDU data is packed in pDataMsg, and we read it after unpacking the PDU headers

  nfapi_nr_rx_data_indication_t *pNfapiMsg = (nfapi_nr_rx_data_indication_t *)msg;
  // Unpack SFN, slot, nPDU
  if (!(pull16(ppReadPackedMsg, &pNfapiMsg->sfn, end) && pull16(ppReadPackedMsg, &pNfapiMsg->slot, end)
        && pull16(ppReadPackedMsg, &pNfapiMsg->number_of_pdus, end))) {
    return 0;
  }
  // Allocate the PDU list for number of PDUs
  if (pNfapiMsg->number_of_pdus > 0) {
    pNfapiMsg->pdu_list = nfapi_p7_allocate(sizeof(*pNfapiMsg->pdu_list) * pNfapiMsg->number_of_pdus, config);
  }
  // For each PDU, unpack its header
  for (int i = 0; i < pNfapiMsg->number_of_pdus; i++) {
    if (!aerial_unpack_nr_rx_data_indication_body(&pNfapiMsg->pdu_list[i], ppReadPackedMsg, end, config))
      return 0;
  }
  // After unpacking the PDU headers, unpack the PDU data from the separate buffer
  for (int i = 0; i < pNfapiMsg->number_of_pdus; i++) {
    if(!pullarray8(pDataMsg,
               pNfapiMsg->pdu_list[i].pdu,
               pNfapiMsg->pdu_list[i].pdu_length,
               pNfapiMsg->pdu_list[i].pdu_length,
               data_end)){
      return 0;
    }
  }

  return 1;
}

uint8_t aerial_unpack_nr_crc_indication(uint8_t **ppReadPackedMsg,
                                        uint8_t *end,
                                        nfapi_nr_crc_indication_t *msg,
                                        nfapi_p7_codec_config_t *config)
{
  return unpack_nr_crc_indication(ppReadPackedMsg, end, msg, config);
}

uint8_t aerial_unpack_nr_uci_indication(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg, nfapi_p7_codec_config_t *config)
{
  return unpack_nr_uci_indication(ppReadPackedMsg, end, msg, config);
}

uint8_t aerial_unpack_nr_srs_indication(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg, nfapi_p7_codec_config_t *config)
{
  return unpack_nr_srs_indication(ppReadPackedMsg, end, msg, config);
}

uint8_t aerial_unpack_nr_rach_indication(uint8_t **ppReadPackedMsg,
                                         uint8_t *end,
                                         nfapi_nr_rach_indication_t *msg,
                                         nfapi_p7_codec_config_t *config)
{
  return unpack_nr_rach_indication(ppReadPackedMsg, end, msg, config);
}

static int32_t aerial_pack_tx_data_request(void *pMessageBuf,
                                           void *pPackedBuf,
                                           void *pDataBuf,
                                           uint32_t packedBufLen,
                                           uint32_t dataBufLen,
                                           nfapi_p7_codec_config_t *config,
                                           uint32_t *data_len)
{
  if (pMessageBuf == NULL || pPackedBuf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "P7 Pack supplied pointers are null\n");
    return -1;
  }

  nfapi_p7_message_header_t *pMessageHeader = pMessageBuf;
  uint8_t *end = pPackedBuf + packedBufLen;
  uint8_t *data_end = pDataBuf + dataBufLen;
  uint8_t *pWritePackedMessage = pPackedBuf;
  uint8_t *pDataPackedMessage = pDataBuf;
  uint8_t *pPackMessageEnd = pPackedBuf + packedBufLen;
  uint8_t *pPackedLengthField = &pWritePackedMessage[4];
  uint8_t *pPacketBodyField = &pWritePackedMessage[8];
  uint8_t *pPacketBodyFieldStart = &pWritePackedMessage[8];
  uint8_t *pPackedDataField = &pDataPackedMessage[0];
  uint8_t *pPackedDataFieldStart = &pDataPackedMessage[0];

  // PHY API message header
  // Number of messages [0]
  // Opaque handle [1]
  // PHY API Message structure
  // Message type ID [2,3]
  // Message Length [4,5,6,7]
  // Message Body [8,...]
  if (!(push8(1, &pWritePackedMessage, pPackMessageEnd) && push8(0, &pWritePackedMessage, pPackMessageEnd)
        && push16(pMessageHeader->message_id, &pWritePackedMessage, pPackMessageEnd))) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "P7 Pack header failed\n");
    return -1;
  }

  uint8_t **ppWriteBody = &pPacketBodyField;
  uint8_t **ppWriteData = &pPackedDataField;
  nfapi_nr_tx_data_request_t *pNfapiMsg = (nfapi_nr_tx_data_request_t *)pMessageHeader;

  if (!(push16(pNfapiMsg->SFN, ppWriteBody, end) && push16(pNfapiMsg->Slot, ppWriteBody, end)
        && push16(pNfapiMsg->Number_of_PDUs, ppWriteBody, end))) {
    return 0;
  }

  for (int i = 0; i < pNfapiMsg->Number_of_PDUs; i++) {
    nfapi_nr_pdu_t *value = (nfapi_nr_pdu_t *)&pNfapiMsg->pdu_list[i];
    // recalculate PDU_Length for Aerial (leave only the size occupied in the payload buffer afterward)
    // assuming there is only 1 TLV present
    value->PDU_length = value->TLVs[0].length;
    if (!(push32(value->PDU_length, ppWriteBody, end)
          && // cuBB expects TX_DATA.request PDUSize to be 32 bit
          push16(value->PDU_index, ppWriteBody, end) && push32(value->num_TLV, ppWriteBody, end))) {
      return 0;
    }
    uint16_t k = 0;
    uint16_t total_number_of_tlvs = value->num_TLV;

    for (; k < total_number_of_tlvs; ++k) {
      // For Aerial, the TLV tag is always 2
      if (!(push16(/*value->TLVs[k].tag*/ 2, ppWriteBody, end) && push16(/*value->TLVs[k].length*/ 4, ppWriteBody, end))) {
        return 0;
      }
      // value
      if (!push32(0, ppWriteBody, end)) {
        return 0;
      }
    }
  }

  //Actual payloads are packed in a separate buffer
  for (int i = 0; i < pNfapiMsg->Number_of_PDUs; i++) {
    nfapi_nr_pdu_t *value = (nfapi_nr_pdu_t *)&pNfapiMsg->pdu_list[i];

    uint16_t k = 0;
    uint16_t total_number_of_tlvs = value->num_TLV;

    for (; k < total_number_of_tlvs; ++k) {
      if (value->TLVs[k].length > 0) {
        if (value->TLVs[k].length % 4 != 0) {
          if (!pusharray32(value->TLVs[k].value.direct,
                           (dataBufLen + 3) / 4,
                           ((value->TLVs[k].length + 3) / 4) - 1,
                           ppWriteData,
                           data_end)) {
            return 0;
          }
          int bytesToAdd = 4 - (4 - (value->TLVs[k].length % 4)) % 4;
          if (bytesToAdd != 4) {
            for (int j = 0; j < bytesToAdd; j++) {
              uint8_t toPush = (uint8_t)(value->TLVs[k].value.direct[((value->TLVs[k].length + 3) / 4) - 1] >> (j * 8));
              if (!push8(toPush, ppWriteData, data_end)) {
                return 0;
              }
            }
          }
        } else {
          // no padding needed
          if (!pusharray32(value->TLVs[k].value.direct,
                           (dataBufLen + 3) / 4,
                           ((value->TLVs[k].length + 3) / 4),
                           ppWriteData,
                           data_end)) {
            return 0;
          }
        }
      } else {
        LOG_E(NR_MAC,"value->TLVs[i].length was 0! (%d.%d) \n", pNfapiMsg->SFN, pNfapiMsg->Slot);
      }
    }
  }


  // calculate data_len
  uintptr_t dataHead = (uintptr_t)pPackedDataFieldStart;
  uintptr_t dataEnd = (uintptr_t)pPackedDataField;
  data_len[0] = dataEnd - dataHead;

  // check for a valid message length
  uintptr_t msgHead = (uintptr_t)pPacketBodyFieldStart;
  uintptr_t msgEnd = (uintptr_t)pPacketBodyField;
  uint32_t packedMsgLen = msgEnd - msgHead;
  uint16_t packedMsgLen16;
  if (packedMsgLen > packedBufLen + dataBufLen) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "Packed message length error %d, buffer supplied %d\n", packedMsgLen, packedBufLen);
    return 0;
  } else {
    packedMsgLen16 = (uint16_t)packedMsgLen;
  }

  // Update the message length in the header
  pMessageHeader->message_length = packedMsgLen16;

  // Update the message length in the header
  if (!push32(packedMsgLen, &pPackedLengthField, pPackMessageEnd))
    return 0;

  return (packedMsgLen16);
}

int fapi_nr_p7_message_pack(void *pMessageBuf, void *pPackedBuf, uint32_t packedBufLen, nfapi_p7_codec_config_t *config)
{
  if (pMessageBuf == NULL || pPackedBuf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "P7 Pack supplied pointers are null\n");
    return -1;
  }

  nfapi_p7_message_header_t *pMessageHeader = pMessageBuf;
  uint8_t *end = pPackedBuf + packedBufLen;
  uint8_t *pWritePackedMessage = pPackedBuf;
  uint8_t *pPackMessageEnd = pPackedBuf + packedBufLen;
  uint8_t *pPackedLengthField = &pWritePackedMessage[4];
  uint8_t *pPacketBodyField = &pWritePackedMessage[8];
  uint8_t *pPacketBodyFieldStart = &pWritePackedMessage[8];

  // PHY API message header
  // Number of messages [0]
  // Opaque handle [1]
  // PHY API Message structure
  // Message type ID [2,3]
  // Message Length [4,5,6,7]
  // Message Body [8,...]
  if (!(push8(1, &pWritePackedMessage, pPackMessageEnd) && push8(0, &pWritePackedMessage, pPackMessageEnd)
        && push16(pMessageHeader->message_id, &pWritePackedMessage, pPackMessageEnd))) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "P7 Pack header failed\n");
    return -1;
  }

  // look for the specific message
  uint8_t result = 0;
  switch (pMessageHeader->message_id) {
    case NFAPI_NR_PHY_MSG_TYPE_DL_TTI_REQUEST:
      result = pack_dl_tti_request(pMessageHeader, &pPacketBodyField, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_UL_TTI_REQUEST:
      result = pack_ul_tti_request(pMessageHeader, &pPacketBodyField, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_TX_DATA_REQUEST:
      // TX_DATA.request already handled by aerial_pack_tx_data_request
      break;

    case NFAPI_NR_PHY_MSG_TYPE_UL_DCI_REQUEST:
      result = pack_ul_dci_request(pMessageHeader, &pPacketBodyField, end, config);
      break;

    case NFAPI_UE_RELEASE_REQUEST:
      result = pack_ue_release_request(pMessageHeader, &pPacketBodyField, end, config);
      break;

    case NFAPI_UE_RELEASE_RESPONSE:
      result = pack_ue_release_response(pMessageHeader, &pPacketBodyField, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_SLOT_INDICATION:
      result = pack_nr_slot_indication(pMessageHeader, &pPacketBodyField, end, config);

    case NFAPI_NR_PHY_MSG_TYPE_RX_DATA_INDICATION:
      result = pack_nr_rx_data_indication(pMessageHeader, &pPacketBodyField, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_CRC_INDICATION:
      result = pack_nr_crc_indication(pMessageHeader, &pPacketBodyField, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_UCI_INDICATION:
      result = pack_nr_uci_indication(pMessageHeader, &pPacketBodyField, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_SRS_INDICATION:
      result = pack_nr_srs_indication(pMessageHeader, &pPacketBodyField, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_RACH_INDICATION:
      result = pack_nr_rach_indication(pMessageHeader, &pPacketBodyField, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_DL_NODE_SYNC:
      result = pack_nr_dl_node_sync(pMessageHeader, &pPacketBodyField, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_UL_NODE_SYNC:
      result = pack_nr_ul_node_sync(pMessageHeader, &pPacketBodyField, end, config);
      break;

    case NFAPI_TIMING_INFO:
      result = pack_nr_timing_info(pMessageHeader, &pPacketBodyField, end, config);
      break;

    case 0x8f:
      result = pack_nr_slot_indication(pMessageHeader, &pPacketBodyField, end, config);
      break;

    default: {
      if (pMessageHeader->message_id >= NFAPI_VENDOR_EXT_MSG_MIN && pMessageHeader->message_id <= NFAPI_VENDOR_EXT_MSG_MAX) {
        if (config && config->pack_p7_vendor_extension) {
          result = (config->pack_p7_vendor_extension)(pMessageHeader, &pPacketBodyField, end, config);
        } else {
          NFAPI_TRACE(NFAPI_TRACE_ERROR,
                      "%s VE NFAPI message ID %d. No ve ecoder provided\n",
                      __FUNCTION__,
                      pMessageHeader->message_id);
        }
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s NFAPI Unknown message ID %d\n", __FUNCTION__, pMessageHeader->message_id);
      }
    } break;
  }

  if (result == 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "P7 Pack failed to pack message\n");
    return -1;
  }

  // check for a valid message length
  uintptr_t msgHead = (uintptr_t)pPacketBodyFieldStart;
  uintptr_t msgEnd = (uintptr_t)pPacketBodyField;
  uint32_t packedMsgLen = msgEnd - msgHead;
  uint16_t packedMsgLen16;
  if (packedMsgLen > 0xFFFF || packedMsgLen > packedBufLen) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "Packed message length error %d, buffer supplied %d\n", packedMsgLen, packedBufLen);
    return -1;
  } else {
    packedMsgLen16 = (uint16_t)packedMsgLen;
  }

  // Update the message length in the header
  pMessageHeader->message_length = packedMsgLen16;

  // Update the message length in the header
  if (!push32(packedMsgLen, &pPackedLengthField, pPackMessageEnd))
    return -1;

  if (1) {
    // quick test
    if (pMessageHeader->message_length != packedMsgLen) {
      NFAPI_TRACE(NFAPI_TRACE_ERROR,
                  "nfapi packedMsgLen(%d) != message_length(%d) id %d\n",
                  packedMsgLen,
                  pMessageHeader->message_length,
                  pMessageHeader->message_id);
    }
  }

  return (packedMsgLen16);
}

int fapi_nr_pack_and_send_p7_message(vnf_p7_t *vnf_p7, nfapi_p7_message_header_t *header)
{
  uint8_t FAPI_buffer[1024 * 64];
  // Check if TX_DATA request, if true, need to pack to data_buf
  if (header->message_id == NFAPI_NR_PHY_MSG_TYPE_TX_DATA_REQUEST) {
    nfapi_nr_tx_data_request_t *pNfapiMsg = (nfapi_nr_tx_data_request_t *)header;
    uint64_t size = 0;
    for (int i = 0; i < pNfapiMsg->Number_of_PDUs; ++i) {
      size += pNfapiMsg->pdu_list[i].PDU_length;
    }
    AssertFatal(size <= 1024 * 1024 * 2, "Message data larger than available buffer, tried to pack %"PRId64 ,size);
    uint8_t FAPI_data_buffer[1024 * 1024 * 2]; // 2MB
    uint32_t data_len = 0;
    int32_t len_FAPI = aerial_pack_tx_data_request(header,
                                                   FAPI_buffer,
                                                   FAPI_data_buffer,
                                                   sizeof(FAPI_buffer),
                                                   sizeof(FAPI_data_buffer),
                                                   &vnf_p7->_public.codec_config,
                                                   &data_len);
    if (len_FAPI <= 0) {
      LOG_E(NFAPI_VNF, "Problem packing TX_DATA_request\n");
      return len_FAPI;
    } else {
      int retval = aerial_send_P7_msg_with_data(FAPI_buffer, len_FAPI, FAPI_data_buffer, data_len, header);
      return retval;
    }
  } else {
    // Create and send FAPI P7 message
    int len_FAPI = fapi_nr_p7_message_pack(header, FAPI_buffer, sizeof(FAPI_buffer), &vnf_p7->_public.codec_config);
    return aerial_send_P7_msg(FAPI_buffer, len_FAPI, header);
  }
}
