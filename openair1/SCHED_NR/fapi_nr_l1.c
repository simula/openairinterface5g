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

/*! \file fapi_nr_l1.c
 * \brief functions for FAPI L1 interface
 * \author R. Knopp, WEI-TAI CHEN
 * \date 2017, 2018
 * \version 0.1
 * \company Eurecom, NTUST
 * \email: knopp@eurecom.fr, kroempa@gmail.com
 * \note
 * \warning
 */
#include "fapi_nr_l1.h"
#include "common/ran_context.h"
#include "PHY/NR_TRANSPORT/nr_transport_proto.h"
#include "PHY/NR_TRANSPORT/nr_dlsch.h"
#include "PHY/NR_TRANSPORT/nr_dci.h"
#include "nfapi/oai_integration/vendor_ext.h"
#include "openair2/NR_PHY_INTERFACE/nr_sched_response.h"
#include "nfapi/oai_integration/nfapi_vnf.h"

void handle_nr_nfapi_ssb_pdu(processingData_L1tx_t *msgTx,int frame,int slot,
                             nfapi_nr_dl_tti_request_pdu_t *dl_tti_pdu)
{

  AssertFatal(dl_tti_pdu->ssb_pdu.ssb_pdu_rel15.bchPayloadFlag== 1, "bchPayloadFlat %d != 1\n",
              dl_tti_pdu->ssb_pdu.ssb_pdu_rel15.bchPayloadFlag);

  uint8_t i_ssb = dl_tti_pdu->ssb_pdu.ssb_pdu_rel15.SsbBlockIndex;

  LOG_D(NR_PHY,"%d.%d : ssb index %d pbch_pdu: %x\n",frame,slot,i_ssb,dl_tti_pdu->ssb_pdu.ssb_pdu_rel15.bchPayload);
  if (msgTx->ssb[i_ssb].active)
    AssertFatal(1==0,"SSB PDU with index %d already active\n",i_ssb);
  else {
    msgTx->ssb[i_ssb].active = true;
    memcpy((void*)&msgTx->ssb[i_ssb].ssb_pdu,&dl_tti_pdu->ssb_pdu,sizeof(dl_tti_pdu->ssb_pdu));
  }
}

void handle_nfapi_nr_csirs_pdu(processingData_L1tx_t *msgTx, int frame, int slot, nfapi_nr_dl_tti_csi_rs_pdu *csirs_pdu)
{
  int found = 0;

  for (int id = 0; id < NR_NUMBER_OF_SYMBOLS_PER_SLOT; id++) {
    NR_gNB_CSIRS_t *csirs = &msgTx->csirs_pdu[id];
    if (csirs->active == 0) {
      LOG_D(NR_PHY,"Frame %d Slot %d CSI_RS with ID %d is now active\n",frame,slot,id);
      csirs->active = 1;
      memcpy((void*)&csirs->csirs_pdu, (void*)csirs_pdu, sizeof(nfapi_nr_dl_tti_csi_rs_pdu));
      found = 1;
      break;
    }
  }
  if (found == 0)
    LOG_E(MAC,"CSI-RS list is full\n");
}

void nr_schedule_dl_tti_req(PHY_VARS_gNB *gNB, nfapi_nr_dl_tti_request_t *DL_req)
{
  DevAssert(gNB != NULL);
  DevAssert(DL_req != NULL);
  nfapi_nr_config_request_scf_t *cfg = &gNB->gNB_config;

  int frame = DL_req->SFN;
  int slot = DL_req->Slot;

  int slot_type = nr_slot_select(cfg, frame, slot);
  DevAssert(slot_type == NR_DOWNLINK_SLOT || slot_type == NR_MIXED_SLOT);

  processingData_L1tx_t *msgTx = gNB->msgDataTx;
  msgTx->slot = slot;
  msgTx->frame = frame;

  uint8_t number_dl_pdu = DL_req->dl_tti_request_body.nPDUs;

  for (int i = 0; i < number_dl_pdu; i++) {
    nfapi_nr_dl_tti_request_pdu_t *dl_tti_pdu = &DL_req->dl_tti_request_body.dl_tti_pdu_list[i];
    LOG_D(NR_PHY, "NFAPI: dl_pdu %d : type %d\n", i, dl_tti_pdu->PDUType);
    switch (dl_tti_pdu->PDUType) {
      case NFAPI_NR_DL_TTI_SSB_PDU_TYPE:
        handle_nr_nfapi_ssb_pdu(msgTx, frame, slot, dl_tti_pdu);
        break;

      case NFAPI_NR_DL_TTI_PDCCH_PDU_TYPE:
        LOG_D(NR_PHY, "frame %d, slot %d, Got NFAPI_NR_DL_TTI_PDCCH_PDU_TYPE for %d.%d\n", frame, slot, DL_req->SFN, DL_req->Slot);
        msgTx->pdcch_pdu[msgTx->num_dl_pdcch] = dl_tti_pdu->pdcch_pdu;
        msgTx->num_dl_pdcch++;
        break;

      case NFAPI_NR_DL_TTI_CSI_RS_PDU_TYPE:
        LOG_D(NR_PHY, "frame %d, slot %d, Got NFAPI_NR_DL_TTI_CSI_RS_PDU_TYPE for %d.%d\n", frame, slot, DL_req->SFN, DL_req->Slot);
        handle_nfapi_nr_csirs_pdu(msgTx, frame, slot, &dl_tti_pdu->csi_rs_pdu);
        break;

      case NFAPI_NR_DL_TTI_PDSCH_PDU_TYPE:
        LOG_D(NR_PHY, "frame %d, slot %d, Got NFAPI_NR_DL_TTI_PDSCH_PDU_TYPE for %d.%d\n", frame, slot, DL_req->SFN, DL_req->Slot);
        nr_fill_dlsch_dl_tti_req(msgTx, &dl_tti_pdu->pdsch_pdu);
    }
  }
}

void nr_schedule_ul_tti_req(PHY_VARS_gNB *gNB, nfapi_nr_ul_tti_request_t *UL_tti_req)
{
  DevAssert(gNB != NULL);
  DevAssert(UL_tti_req != NULL);

  int frame = UL_tti_req->SFN;
  int slot = UL_tti_req->Slot;

  for (int i = 0; i < UL_tti_req->n_pdus; i++) {
    switch (UL_tti_req->pdus_list[i].pdu_type) {
      case NFAPI_NR_UL_CONFIG_PUSCH_PDU_TYPE:
        LOG_D(NR_PHY,
              "frame %d, slot %d, Got NFAPI_NR_UL_TTI_PUSCH_PDU_TYPE for %d.%d\n",
              frame,
              slot,
              UL_tti_req->SFN,
              UL_tti_req->Slot);
        nr_fill_ulsch(gNB, UL_tti_req->SFN, UL_tti_req->Slot, &UL_tti_req->pdus_list[i].pusch_pdu);
        break;
      case NFAPI_NR_UL_CONFIG_PUCCH_PDU_TYPE:
        LOG_D(NR_PHY,
              "frame %d, slot %d, Got NFAPI_NR_UL_TTI_PUCCH_PDU_TYPE for %d.%d\n",
              frame,
              slot,
              UL_tti_req->SFN,
              UL_tti_req->Slot);
        nr_fill_pucch(gNB, UL_tti_req->SFN, UL_tti_req->Slot, &UL_tti_req->pdus_list[i].pucch_pdu);
        break;
      case NFAPI_NR_UL_CONFIG_PRACH_PDU_TYPE:
        LOG_D(NR_PHY,
              "frame %d, slot %d, Got NFAPI_NR_UL_TTI_PRACH_PDU_TYPE for %d.%d\n",
              frame,
              slot,
              UL_tti_req->SFN,
              UL_tti_req->Slot);
        nfapi_nr_prach_pdu_t *prach_pdu = &UL_tti_req->pdus_list[i].prach_pdu;
        int id = nr_fill_prach(gNB, UL_tti_req->SFN, UL_tti_req->Slot, prach_pdu);
        if (gNB->RU_list[0]->if_south == LOCAL_RF || gNB->RU_list[0]->if_south == REMOTE_IF5)
          nr_fill_prach_ru(gNB->RU_list[0], UL_tti_req->SFN, UL_tti_req->Slot, prach_pdu, gNB->prach_vars.list[id].beam_nb);
        break;
      case NFAPI_NR_UL_CONFIG_SRS_PDU_TYPE:
        LOG_D(NR_PHY,
              "frame %d, slot %d, Got NFAPI_NR_UL_CONFIG_SRS_PDU_TYPE for %d.%d\n",
              frame,
              slot,
              UL_tti_req->SFN,
              UL_tti_req->Slot);
        nr_fill_srs(gNB, UL_tti_req->SFN, UL_tti_req->Slot, &UL_tti_req->pdus_list[i].srs_pdu);
        break;
    }
  }
}

void nr_schedule_tx_req(PHY_VARS_gNB *gNB, nfapi_nr_tx_data_request_t *TX_req)
{
  DevAssert(gNB != NULL);
  DevAssert(TX_req != NULL);
  processingData_L1tx_t *msgTx = gNB->msgDataTx;

  for (int idx = 0; idx < TX_req->Number_of_PDUs; ++idx) {
    uint8_t *sdu = (uint8_t *)TX_req->pdu_list[idx].TLVs[0].value.direct;
    nr_fill_dlsch_tx_req(msgTx, idx, sdu);
  }
}

void nr_schedule_ul_dci_req(PHY_VARS_gNB *gNB, nfapi_nr_ul_dci_request_t *UL_dci_req)
{
  DevAssert(gNB != NULL);
  DevAssert(UL_dci_req != NULL);
  processingData_L1tx_t *msgTx = gNB->msgDataTx;

  msgTx->num_ul_pdcch = UL_dci_req->numPdus;
  for (int i = 0; i < UL_dci_req->numPdus; i++)
    msgTx->ul_pdcch_pdu[i] = UL_dci_req->ul_dci_pdu_list[i];
}

void nr_schedule_response(NR_Sched_Rsp_t *Sched_INFO)
{
  // copy data from L2 interface into L1 structures
  module_id_t Mod_id = Sched_INFO->module_id;
  frame_t frame = Sched_INFO->frame;
  slot_t slot = Sched_INFO->slot;

  AssertFatal(RC.gNB != NULL, "RC.gNB is null\n");
  AssertFatal(RC.gNB[Mod_id] != NULL, "RC.gNB[%d] is null\n", Mod_id);

  PHY_VARS_gNB *gNB = RC.gNB[Mod_id];
  start_meas(&gNB->schedule_response_stats);

  nfapi_nr_config_request_scf_t *cfg = &gNB->gNB_config;

  int slot_type = nr_slot_select(cfg, frame, slot);

  clear_slot_beamid(gNB, slot);  // reset beam_id information for the slot to be processed
  DevAssert(NFAPI_MODE == NFAPI_MONOLITHIC);
  bool is_dl = slot_type == NR_DOWNLINK_SLOT || slot_type == NR_MIXED_SLOT;

  processingData_L1tx_t *msgTx = gNB->msgDataTx;
  /* store the sched_response_id for the TX thread to release it when done */
  msgTx->sched_response_id = Sched_INFO->sched_response_id;

  DevAssert(Sched_INFO->DL_req.SFN == frame);
  DevAssert(Sched_INFO->DL_req.Slot == slot);

  if (is_dl) {
    nr_schedule_dl_tti_req(gNB, &Sched_INFO->DL_req);
  }

  nr_schedule_ul_tti_req(gNB, &Sched_INFO->UL_tti_req);

  if (is_dl) {
    nr_schedule_tx_req(gNB, &Sched_INFO->TX_req);

    nr_schedule_ul_dci_req(gNB, &Sched_INFO->UL_dci_req);
    /* Both the current thread and the TX thread will access the sched_info
     * at the same time, so increase its reference counter, so that it is
     * released only when both threads are done with it.
     */
    inc_ref_sched_response(Sched_INFO->sched_response_id);
  }

  /* this thread is done with the sched_info, decrease the reference counter */
  if ((slot_type == NR_DOWNLINK_SLOT || slot_type == NR_MIXED_SLOT) && NFAPI_MODE == NFAPI_MONOLITHIC) {
    LOG_D(NR_PHY, "Calling dref_sched_response for id %d in %d.%d (sched_response)\n", Sched_INFO->sched_response_id, frame, slot);
    deref_sched_response(Sched_INFO->sched_response_id);
  }
  stop_meas(&gNB->schedule_response_stats);
}
