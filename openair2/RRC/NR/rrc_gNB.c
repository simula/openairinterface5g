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

/*! \file rrc_gNB.c
 * \brief rrc procedures for gNB
 * \author Navid Nikaein and  Raymond Knopp , WEI-TAI CHEN
 * \date 2011 - 2014 , 2018
 * \version 1.0
 * \company Eurecom, NTUST
 * \email: navid.nikaein@eurecom.fr and raymond.knopp@eurecom.fr, kroempa@gmail.com
 */
#define RRC_GNB_C
#define RRC_GNB_C

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#include "nr_rrc_config.h"
#include "nr_rrc_defs.h"
#include "nr_rrc_extern.h"
#include "assertions.h"
#include "common/ran_context.h"
#include "oai_asn1.h"
#include "rrc_gNB_radio_bearers.h"

#include "RRC/L2_INTERFACE/openair_rrc_L2_interface.h"
#include "LAYER2/NR_MAC_gNB/mac_proto.h"
#include "common/utils/LOG/log.h"
#include "RRC/NR/MESSAGES/asn1_msg.h"
#include "openair2/E1AP/e1ap_asnc.h"

#include "NR_BCCH-BCH-Message.h"
#include "NR_UL-DCCH-Message.h"
#include "NR_DL-DCCH-Message.h"
#include "NR_DL-CCCH-Message.h"
#include "NR_UL-CCCH-Message.h"
#include "NR_RRCReject.h"
#include "NR_RejectWaitTime.h"
#include "NR_RRCSetup.h"

#include "NR_CellGroupConfig.h"
#include "NR_MeasResults.h"
#include "NR_UL-CCCH-Message.h"
#include "NR_RRCSetupRequest-IEs.h"
#include "NR_RRCSetupComplete-IEs.h"
#include "NR_RRCReestablishmentRequest-IEs.h"
#include "NR_MIB.h"
#include "uper_encoder.h"
#include "uper_decoder.h"

#include "common/platform_types.h"
#include "common/utils/LOG/vcd_signal_dumper.h"

#include "T.h"

#include "openair3/SECU/secu_defs.h"

#include "rrc_gNB_NGAP.h"
#include "rrc_gNB_du.h"

#include "rrc_gNB_GTPV1U.h"

#include "nr_pdcp/nr_pdcp_entity.h"
#include "nr_pdcp/nr_pdcp_oai_api.h"

#include "intertask_interface.h"
#include "SIMULATION/TOOLS/sim.h" // for taus

#include "executables/softmodem-common.h"
#include <openair2/RRC/NR/rrc_gNB_UE_context.h>
#include <openair2/X2AP/x2ap_eNB.h>
#include <openair3/SECU/key_nas_deriver.h>
#include <openair3/ocp-gtpu/gtp_itf.h>
#include <openair2/RRC/NR/nr_rrc_proto.h>
#include "openair2/F1AP/f1ap_common.h"
#include "openair2/F1AP/f1ap_ids.h"
#include "openair2/F1AP/lib/f1ap_lib_extern.h"
#include "openair2/SDAP/nr_sdap/nr_sdap_entity.h"
#include "openair2/E1AP/e1ap.h"
#include "cucp_cuup_if.h"

#include "BIT_STRING.h"
#include "assertions.h"

#ifdef E2_AGENT
#include "openair2/E2AP/RAN_FUNCTION/O-RAN/ran_func_rc_extern.h"
#endif

//#define XER_PRINT

extern RAN_CONTEXT_t RC;

mui_t rrc_gNB_mui = 0;

// the assoc_id might be 0 (if the DU goes offline). Below helper macro to
// print an error and return from the function in that case
#define RETURN_IF_INVALID_ASSOC_ID(assoc_id)                               \
  {                                                                        \
    if (assoc_id == 0) {                                                   \
      LOG_E(NR_RRC, "cannot send data: invalid assoc_id 0, DU offline\n"); \
      return;                                                              \
    }                                                                      \
  }

///---------------------------------------------------------------------------------------------------------------///
///---------------------------------------------------------------------------------------------------------------///

static void clear_nas_pdu(ngap_pdu_t *pdu)
{
  DevAssert(pdu != NULL);
  free(pdu->buffer); // does nothing if NULL
  pdu->buffer = NULL;
  pdu->length = 0;
}

static void freeDRBlist(NR_DRB_ToAddModList_t *list)
{
  //ASN_STRUCT_FREE(asn_DEF_NR_DRB_ToAddModList, list);
  return;
}

const neighbour_cell_configuration_t *get_neighbour_config(int serving_cell_nr_cellid)
{
  const gNB_RRC_INST *rrc = RC.nrrrc[0];
  seq_arr_t *neighbour_cell_configuration = rrc->neighbour_cell_configuration;
  if (!neighbour_cell_configuration)
    return NULL;

  for (int cellIdx = 0; cellIdx < neighbour_cell_configuration->size; cellIdx++) {
    neighbour_cell_configuration_t *neighbour_config =
        (neighbour_cell_configuration_t *)seq_arr_at(neighbour_cell_configuration, cellIdx);
    if (neighbour_config->nr_cell_id == serving_cell_nr_cellid)
      return neighbour_config;
  }
  return NULL;
}

const nr_neighbour_gnb_configuration_t *get_neighbour_cell_information(int serving_cell_nr_cellid, int neighbour_cell_phy_id)
{
  const gNB_RRC_INST *rrc = RC.nrrrc[0];
  seq_arr_t *neighbour_cell_configuration = rrc->neighbour_cell_configuration;
  for (int cellIdx = 0; cellIdx < neighbour_cell_configuration->size; cellIdx++) {
    neighbour_cell_configuration_t *neighbour_config =
        (neighbour_cell_configuration_t *)seq_arr_at(neighbour_cell_configuration, cellIdx);
    if (!neighbour_config)
      continue;

    for (int neighbourIdx = 0; neighbourIdx < neighbour_config->neighbour_cells->size; neighbourIdx++) {
      nr_neighbour_gnb_configuration_t *neighbour =
          (nr_neighbour_gnb_configuration_t *)seq_arr_at(neighbour_config->neighbour_cells, neighbourIdx);
      if (neighbour != NULL && neighbour->physicalCellId == neighbour_cell_phy_id)
        return neighbour;
    }
  }
  return NULL;
}

typedef struct deliver_dl_rrc_message_data_s {
  const gNB_RRC_INST *rrc;
  f1ap_dl_rrc_message_t *dl_rrc;
  sctp_assoc_t assoc_id;
} deliver_dl_rrc_message_data_t;
static void rrc_deliver_dl_rrc_message(void *deliver_pdu_data, ue_id_t ue_id, int srb_id, char *buf, int size, int sdu_id)
{
  DevAssert(deliver_pdu_data != NULL);
  deliver_dl_rrc_message_data_t *data = (deliver_dl_rrc_message_data_t *)deliver_pdu_data;
  data->dl_rrc->rrc_container = (uint8_t *)buf;
  data->dl_rrc->rrc_container_length = size;
  DevAssert(data->dl_rrc->srb_id == srb_id);
  data->rrc->mac_rrc.dl_rrc_message_transfer(data->assoc_id, data->dl_rrc);
}

void nr_rrc_transfer_protected_rrc_message(const gNB_RRC_INST *rrc,
                                           const gNB_RRC_UE_t *ue_p,
                                           uint8_t srb_id,
                                           const uint8_t *buffer,
                                           int size)
{
  DevAssert(size > 0);
  f1_ue_data_t ue_data = cu_get_f1_ue_data(ue_p->rrc_ue_id);
  RETURN_IF_INVALID_ASSOC_ID(ue_data.du_assoc_id);
  f1ap_dl_rrc_message_t dl_rrc = {.gNB_CU_ue_id = ue_p->rrc_ue_id, .gNB_DU_ue_id = ue_data.secondary_ue, .srb_id = srb_id};
  deliver_dl_rrc_message_data_t data = {.rrc = rrc, .dl_rrc = &dl_rrc, .assoc_id = ue_data.du_assoc_id};
  nr_pdcp_data_req_srb(ue_p->rrc_ue_id,
                       srb_id,
                       rrc_gNB_mui++,
                       size,
                       (unsigned char *const)buffer,
                       rrc_deliver_dl_rrc_message,
                       &data);
}

///---------------------------------------------------------------------------------------------------------------///
///---------------------------------------------------------------------------------------------------------------///

static void init_NR_SI(gNB_RRC_INST *rrc)
{
  if (!NODE_IS_DU(rrc->node_type)) {
    rrc->carrier.SIB23 = (uint8_t *) malloc16(100);
    AssertFatal(rrc->carrier.SIB23 != NULL, "cannot allocate memory for SIB");
    rrc->carrier.sizeof_SIB23 = do_SIB23_NR(&rrc->carrier);
    LOG_I(NR_RRC,"do_SIB23_NR, size %d \n ", rrc->carrier.sizeof_SIB23);
    AssertFatal(rrc->carrier.sizeof_SIB23 != 255,"FATAL, RC.nrrrc[mod].carrier[CC_id].sizeof_SIB23 == 255");
  }

  if (get_softmodem_params()->phy_test > 0 || get_softmodem_params()->do_ra > 0) {
    AssertFatal(NODE_IS_MONOLITHIC(rrc->node_type), "phy_test and do_ra only work in monolithic\n");
    rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_allocate_new_ue_context(rrc);
    LOG_I(NR_RRC,"Adding new user (%p)\n",ue_context_p);
    if (!NODE_IS_CU(RC.nrrrc[0]->node_type)) {
      rrc_add_nsa_user(rrc,ue_context_p,NULL);
    }
  }
}

static void rrc_gNB_CU_DU_init(gNB_RRC_INST *rrc)
{
  switch (rrc->node_type) {
    case ngran_gNB_CUCP:
      mac_rrc_dl_f1ap_init(&rrc->mac_rrc);
      cucp_cuup_message_transfer_e1ap_init(rrc);
      break;
    case ngran_gNB_CU:
      mac_rrc_dl_f1ap_init(&rrc->mac_rrc);
      cucp_cuup_message_transfer_direct_init(rrc);
      break;
    case ngran_gNB:
      mac_rrc_dl_direct_init(&rrc->mac_rrc);
      cucp_cuup_message_transfer_direct_init(rrc);
       break;
    case ngran_gNB_DU:
      /* silently drop this, as we currently still need the RRC at the DU. As
       * soon as this is not the case anymore, we can add the AssertFatal() */
      //AssertFatal(1==0,"nothing to do for DU\n");
      break;
    default:
      AssertFatal(0 == 1, "Unknown node type %d\n", rrc->node_type);
      break;
  }
  cu_init_f1_ue_data();
}

void openair_rrc_gNB_configuration(gNB_RRC_INST *rrc, gNB_RrcConfigurationReq *configuration)
{
  AssertFatal(rrc != NULL, "RC.nrrrc not initialized!");
  AssertFatal(MAX_MOBILES_PER_GNB < (module_id_t)0xFFFFFFFFFFFFFFFF, " variable overflow");
  AssertFatal(configuration!=NULL,"configuration input is null\n");
  rrc->module_id = 0;
  rrc_gNB_CU_DU_init(rrc);
  uid_linear_allocator_init(&rrc->uid_allocator);
  RB_INIT(&rrc->rrc_ue_head);
  RB_INIT(&rrc->cuups);
  RB_INIT(&rrc->dus);
  rrc->configuration = *configuration;
   /// System Information INIT
  init_NR_SI(rrc);
  return;
} // END openair_rrc_gNB_configuration

static void rrc_gNB_process_AdditionRequestInformation(const module_id_t gnb_mod_idP, x2ap_ENDC_sgnb_addition_req_t *m)
{
  struct NR_CG_ConfigInfo *cg_configinfo = NULL;
  asn_dec_rval_t dec_rval = uper_decode_complete(NULL,
                            &asn_DEF_NR_CG_ConfigInfo,
                            (void **)&cg_configinfo,
                            (uint8_t *)m->rrc_buffer,
                            (int) m->rrc_buffer_size);//m->rrc_buffer_size);
  gNB_RRC_INST         *rrc=RC.nrrrc[gnb_mod_idP];

  if ((dec_rval.code != RC_OK) && (dec_rval.consumed == 0)) {
    AssertFatal(1==0,"NR_UL_DCCH_MESSAGE decode error\n");
    // free the memory
    SEQUENCE_free(&asn_DEF_NR_CG_ConfigInfo, cg_configinfo, 1);
    return;
  }

  xer_fprint(stdout,&asn_DEF_NR_CG_ConfigInfo, cg_configinfo);
  // recreate enough of X2 EN-DC Container
  AssertFatal(cg_configinfo->criticalExtensions.choice.c1->present == NR_CG_ConfigInfo__criticalExtensions__c1_PR_cg_ConfigInfo,
              "ueCapabilityInformation not present\n");
  parse_CG_ConfigInfo(rrc,cg_configinfo,m);
  LOG_A(NR_RRC, "Successfully parsed CG_ConfigInfo of size %zu bits. (%zu bytes)\n",
        dec_rval.consumed, (dec_rval.consumed +7/8));
}

//-----------------------------------------------------------------------------
unsigned int rrc_gNB_get_next_transaction_identifier(module_id_t gnb_mod_idP)
//-----------------------------------------------------------------------------
{
  static unsigned int transaction_id[NUMBER_OF_gNB_MAX] = {0};
  // used also in NGAP thread, so need thread safe operation
  unsigned int tmp = __atomic_add_fetch(&transaction_id[gnb_mod_idP], 1, __ATOMIC_SEQ_CST);
  tmp %= NR_RRC_TRANSACTION_IDENTIFIER_NUMBER;
  LOG_T(NR_RRC, "generated xid is %d\n", tmp);
  return tmp;
}

/**
 * @brief Create srb-ToAddModList for RRCSetup and RRCReconfiguration messages
*/
static NR_SRB_ToAddModList_t *createSRBlist(gNB_RRC_UE_t *ue, bool reestablish)
{
  if (!ue->Srb[1].Active) {
    LOG_E(NR_RRC, "Call SRB list while SRB1 doesn't exist\n");
    return NULL;
  }
  NR_SRB_ToAddModList_t *list = CALLOC(sizeof(*list), 1);
  for (int i = 0; i < NR_NUM_SRB; i++)
    if (ue->Srb[i].Active) {
      asn1cSequenceAdd(list->list, NR_SRB_ToAddMod_t, srb);
      srb->srb_Identity = i;
      /* Set reestablishPDCP only for SRB2 */
      if (reestablish && i == 2) {
        asn1cCallocOne(srb->reestablishPDCP, NR_SRB_ToAddMod__reestablishPDCP_true);
      }
    }
  return list;
}

static NR_DRB_ToAddModList_t *createDRBlist(gNB_RRC_UE_t *ue, bool reestablish)
{
  NR_DRB_ToAddMod_t *DRB_config = NULL;
  NR_DRB_ToAddModList_t *DRB_configList = CALLOC(sizeof(*DRB_configList), 1);

  for (int i = 0; i < MAX_DRBS_PER_UE; i++) {
    if (ue->established_drbs[i].status != DRB_INACTIVE) {
      DRB_config = generateDRB_ASN1(&ue->established_drbs[i]);
      if (reestablish) {
        asn1cCallocOne(DRB_config->reestablishPDCP, NR_DRB_ToAddMod__reestablishPDCP_true);
      }
      asn1cSeqAdd(&DRB_configList->list, DRB_config);
    }
  }
  if (DRB_configList->list.count == 0) {
    free(DRB_configList);
    return NULL;
  }
  return DRB_configList;
}

static void freeSRBlist(NR_SRB_ToAddModList_t *l)
{
  if (l) {
    for (int i = 0; i < l->list.count; i++)
      free(l->list.array[i]);
    free(l);
  } else
    LOG_E(NR_RRC, "Call free SRB list on NULL pointer\n");
}

static void activate_srb(gNB_RRC_UE_t *UE, int srb_id)
{
  AssertFatal(srb_id == 1 || srb_id == 2, "handling only SRB 1 or 2\n");
  if (UE->Srb[srb_id].Active == 1) {
    LOG_W(RRC, "UE %d SRB %d already activated\n", UE->rrc_ue_id, srb_id);
    return;
  }
  LOG_I(RRC, "activate SRB %d of UE %d\n", srb_id, UE->rrc_ue_id);
  UE->Srb[srb_id].Active = 1;

  NR_SRB_ToAddModList_t *list = CALLOC(sizeof(*list), 1);
  asn1cSequenceAdd(list->list, NR_SRB_ToAddMod_t, srb);
  srb->srb_Identity = srb_id;

  if (srb_id == 1) {
    nr_pdcp_entity_security_keys_and_algos_t null_security_parameters = {0};
    nr_pdcp_add_srbs(true, UE->rrc_ue_id, list, &null_security_parameters);
  } else {
    nr_pdcp_entity_security_keys_and_algos_t security_parameters;
    security_parameters.ciphering_algorithm = UE->ciphering_algorithm;
    security_parameters.integrity_algorithm = UE->integrity_algorithm;
    nr_derive_key(RRC_ENC_ALG, UE->ciphering_algorithm, UE->kgnb, security_parameters.ciphering_key);
    nr_derive_key(RRC_INT_ALG, UE->integrity_algorithm, UE->kgnb, security_parameters.integrity_key);

    nr_pdcp_add_srbs(true,
                     UE->rrc_ue_id,
                     list,
                     &security_parameters);
  }
  freeSRBlist(list);
}

//-----------------------------------------------------------------------------
static void rrc_gNB_generate_RRCSetup(instance_t instance,
                                      rnti_t rnti,
                                      rrc_gNB_ue_context_t *const ue_context_pP,
                                      const uint8_t *masterCellGroup,
                                      int masterCellGroup_len)
//-----------------------------------------------------------------------------
{
  LOG_I(NR_RRC, "rrc_gNB_generate_RRCSetup for RNTI %04x\n", rnti);

  gNB_RRC_UE_t *ue_p = &ue_context_pP->ue_context;
  gNB_RRC_INST *rrc = RC.nrrrc[instance];
  unsigned char buf[1024];
  uint8_t xid = rrc_gNB_get_next_transaction_identifier(instance);
  ue_p->xids[xid] = RRC_SETUP;
  NR_SRB_ToAddModList_t *SRBs = createSRBlist(ue_p, false);

  int size = do_RRCSetup(ue_context_pP, buf, xid, masterCellGroup, masterCellGroup_len, &rrc->configuration, SRBs);
  AssertFatal(size > 0, "do_RRCSetup failed\n");
  AssertFatal(size <= 1024, "memory corruption\n");

  LOG_DUMPMSG(NR_RRC, DEBUG_RRC, (char *)buf, size, "[MSG] RRC Setup\n");
  freeSRBlist(SRBs);
  f1_ue_data_t ue_data = cu_get_f1_ue_data(ue_p->rrc_ue_id);
  RETURN_IF_INVALID_ASSOC_ID(ue_data.du_assoc_id);
  f1ap_dl_rrc_message_t dl_rrc = {
    .gNB_CU_ue_id = ue_p->rrc_ue_id,
    .gNB_DU_ue_id = ue_data.secondary_ue,
    .rrc_container = buf,
    .rrc_container_length = size,
    .srb_id = CCCH
  };
  rrc->mac_rrc.dl_rrc_message_transfer(ue_data.du_assoc_id, &dl_rrc);
}

static void rrc_gNB_generate_RRCReject(module_id_t module_id, rrc_gNB_ue_context_t *const ue_context_pP)
//-----------------------------------------------------------------------------
{
  LOG_I(NR_RRC, "rrc_gNB_generate_RRCReject \n");
  gNB_RRC_INST *rrc = RC.nrrrc[module_id];
  gNB_RRC_UE_t *ue_p = &ue_context_pP->ue_context;

  unsigned char buf[1024];
  int size = do_RRCReject(buf);
  AssertFatal(size > 0, "do_RRCReject failed\n");
  AssertFatal(size <= 1024, "memory corruption\n");

  LOG_DUMPMSG(NR_RRC, DEBUG_RRC,
              (char *)buf,
              size,
              "[MSG] RRCReject \n");
  LOG_I(NR_RRC, " [RAPROC] ue %04x Logical Channel DL-CCCH, Generating NR_RRCReject (bytes %d)\n", ue_p->rnti, size);

  f1_ue_data_t ue_data = cu_get_f1_ue_data(ue_p->rrc_ue_id);
  RETURN_IF_INVALID_ASSOC_ID(ue_data.du_assoc_id);
  f1ap_dl_rrc_message_t dl_rrc = {
    .gNB_CU_ue_id = ue_p->rrc_ue_id,
    .gNB_DU_ue_id = ue_data.secondary_ue,
    .rrc_container = buf,
    .rrc_container_length = size,
    .srb_id = CCCH,
    .execute_duplication  = 1,
    .RAT_frequency_priority_information.en_dc = 0
  };
  rrc->mac_rrc.dl_rrc_message_transfer(ue_data.du_assoc_id, &dl_rrc);
}

//-----------------------------------------------------------------------------
/*
* Process the rrc setup complete message from UE (SRB1 Active)
*/
static void rrc_gNB_process_RRCSetupComplete(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, NR_RRCSetupComplete_IEs_t *rrcSetupComplete)
//-----------------------------------------------------------------------------
{
  LOG_A(NR_RRC, "UE %d Processing NR_RRCSetupComplete from UE\n", UE->rrc_ue_id);
  UE->Srb[1].Active = 1;
  UE->Srb[2].Active = 0;

  rrc_gNB_send_NGAP_NAS_FIRST_REQ(rrc, UE, rrcSetupComplete);
}

static void rrc_gNB_generate_dedicatedRRCReconfiguration(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue_p)
{
  uint8_t xid = rrc_gNB_get_next_transaction_identifier(rrc->module_id);
  ue_p->xids[xid] = RRC_PDUSESSION_ESTABLISH;
  struct NR_RRCReconfiguration_v1530_IEs__dedicatedNAS_MessageList *dedicatedNAS_MessageList = CALLOC(1, sizeof(*dedicatedNAS_MessageList));

  for (int i = 0; i < ue_p->nb_of_pdusessions; i++) {
    if (ue_p->pduSession[i].param.nas_pdu.buffer != NULL) {
      asn1cSequenceAdd(dedicatedNAS_MessageList->list, NR_DedicatedNAS_Message_t, msg);
      OCTET_STRING_fromBuf(msg,
                           (char *)ue_p->pduSession[i].param.nas_pdu.buffer,
                           ue_p->pduSession[i].param.nas_pdu.length);

      LOG_D(NR_RRC, "add NAS info with size %d (pdusession idx %d)\n", ue_p->pduSession[i].param.nas_pdu.length, i);
      ue_p->pduSession[i].xid = xid;
    }
    if (ue_p->pduSession[i].status < PDU_SESSION_STATUS_ESTABLISHED) {
      ue_p->pduSession[i].status = PDU_SESSION_STATUS_DONE;
    }
  }

  if (ue_p->nas_pdu.length) {
    asn1cSequenceAdd(dedicatedNAS_MessageList->list, NR_DedicatedNAS_Message_t, msg);
    OCTET_STRING_fromBuf(msg, (char *)ue_p->nas_pdu.buffer, ue_p->nas_pdu.length);
  }

  /* If list is empty free the list and reset the address */
  if (dedicatedNAS_MessageList->list.count == 0) {
    free(dedicatedNAS_MessageList);
    dedicatedNAS_MessageList = NULL;
  }

  /* Free all NAS PDUs */
  for (int i = 0; i < ue_p->nb_of_pdusessions; i++)
    clear_nas_pdu(&ue_p->pduSession[i].param.nas_pdu);

  NR_CellGroupConfig_t *cellGroupConfig = ue_p->masterCellGroup;

  const nr_rrc_du_container_t *du = get_du_for_ue(rrc, ue_p->rrc_ue_id);
  DevAssert(du != NULL);
  f1ap_served_cell_info_t *cell_info = &du->setup_req->cell[0].info;
  NR_MeasConfig_t *measconfig = NULL;

  if (du->mtc != NULL) {
    int scs = get_ssb_scs(cell_info);
    int band = get_dl_band(cell_info);
    const NR_MeasTimingList_t *mtlist = du->mtc->criticalExtensions.choice.c1->choice.measTimingConf->measTiming;
    const NR_MeasTiming_t *mt = mtlist->list.array[0];
    const neighbour_cell_configuration_t *neighbour_config = get_neighbour_config(cell_info->nr_cellid);
    seq_arr_t *neighbour_cells = NULL;
    if (neighbour_config)
      neighbour_cells = neighbour_config->neighbour_cells;

    measconfig = get_MeasConfig(mt, band, scs, &rrc->measurementConfiguration, neighbour_cells);
  }

  if (ue_p->measConfig)
    free_MeasConfig(ue_p->measConfig);

  ue_p->measConfig = measconfig;

  uint8_t buffer[NR_RRC_BUF_SIZE] = {0};
  NR_SRB_ToAddModList_t *SRBs = createSRBlist(ue_p, false);
  NR_DRB_ToAddModList_t *DRBs = createDRBlist(ue_p, false);

  int size = do_RRCReconfiguration(ue_p,
                                   buffer,
                                   NR_RRC_BUF_SIZE,
                                   xid,
                                   SRBs,
                                   DRBs,
                                   ue_p->DRB_ReleaseList,
                                   NULL,
                                   measconfig,
                                   dedicatedNAS_MessageList,
                                   cellGroupConfig);
  LOG_DUMPMSG(NR_RRC, DEBUG_RRC, (char *)buffer, size, "[MSG] RRC Reconfiguration\n");
  freeSRBlist(SRBs);
  freeDRBlist(DRBs);
  ASN_STRUCT_FREE(asn_DEF_NR_DRB_ToReleaseList, ue_p->DRB_ReleaseList);
  ue_p->DRB_ReleaseList = NULL;

  LOG_I(NR_RRC, "UE %d: Generate RRCReconfiguration (bytes %d, xid %d)\n", ue_p->rrc_ue_id, size, xid);
  nr_rrc_transfer_protected_rrc_message(rrc, ue_p, DCCH, buffer, size);
}

void rrc_gNB_modify_dedicatedRRCReconfiguration(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue_p)
{
  int qos_flow_index = 0;
  uint8_t xid = rrc_gNB_get_next_transaction_identifier(rrc->module_id);
  ue_p->xids[xid] = RRC_PDUSESSION_MODIFY;

  struct NR_RRCReconfiguration_v1530_IEs__dedicatedNAS_MessageList *dedicatedNAS_MessageList =
      CALLOC(1, sizeof(*dedicatedNAS_MessageList));

  for (int i = 0; i < ue_p->nb_of_pdusessions; i++) {
    // bypass the new and already configured pdu sessions
    if (ue_p->pduSession[i].status >= PDU_SESSION_STATUS_DONE) {
      ue_p->pduSession[i].xid = xid;
      continue;
    }

    if (ue_p->pduSession[i].cause != NGAP_CAUSE_NOTHING) {
      // set xid of failure pdu session
      ue_p->pduSession[i].xid = xid;
      ue_p->pduSession[i].status = PDU_SESSION_STATUS_FAILED;
      continue;
    }

    // search exist DRB_config
    int j;
    for (j = 0; i < MAX_DRBS_PER_UE; j++) {
      if (ue_p->established_drbs[j].status != DRB_INACTIVE
          && ue_p->established_drbs[j].cnAssociation.sdap_config.pdusession_id == ue_p->pduSession[i].param.pdusession_id)
        break;
    }

    if (j == MAX_DRBS_PER_UE) {
      ue_p->pduSession[i].xid = xid;
      ue_p->pduSession[i].status = PDU_SESSION_STATUS_FAILED;
      ue_p->pduSession[i].cause = NGAP_CAUSE_RADIO_NETWORK;
      ue_p->pduSession[i].cause_value = NGAP_CauseRadioNetwork_unspecified;
      continue;
    }

    // Reference TS23501 Table 5.7.4-1: Standardized 5QI to QoS characteristics mapping
    for (qos_flow_index = 0; qos_flow_index < ue_p->pduSession[i].param.nb_qos; qos_flow_index++) {
      switch (ue_p->pduSession[i].param.qos[qos_flow_index].fiveQI) {
        case 1: //100ms
        case 2: //150ms
        case 3: //50ms
        case 4: //300ms
        case 5: //100ms
        case 6: //300ms
        case 7: //100ms
        case 8: //300ms
        case 9: //300ms Video (Buffered Streaming)TCP-based (e.g., www, e-mail, chat, ftp, p2p file sharing, progressive video, etc.)
          // TODO
          break;

        default:
          LOG_E(NR_RRC, "not supported 5qi %lu\n", ue_p->pduSession[i].param.qos[qos_flow_index].fiveQI);
          ue_p->pduSession[i].status = PDU_SESSION_STATUS_FAILED;
          ue_p->pduSession[i].xid = xid;
          ue_p->pduSession[i].cause = NGAP_CAUSE_RADIO_NETWORK;
          ue_p->pduSession[i].cause_value = NGAP_CauseRadioNetwork_not_supported_5QI_value;
          continue;
      }
        LOG_I(NR_RRC,
              "index %d, QOS flow %d, 5QI %ld \n",
              i,
              qos_flow_index,
              ue_p->pduSession[i].param.qos[qos_flow_index].fiveQI);
    }

    ue_p->pduSession[i].status = PDU_SESSION_STATUS_DONE;
    ue_p->pduSession[i].xid = xid;

    if (ue_p->pduSession[i].param.nas_pdu.buffer != NULL) {
      asn1cSequenceAdd(dedicatedNAS_MessageList->list,NR_DedicatedNAS_Message_t, dedicatedNAS_Message);
      OCTET_STRING_fromBuf(dedicatedNAS_Message, (char *)ue_p->pduSession[i].param.nas_pdu.buffer, ue_p->pduSession[i].param.nas_pdu.length);
      LOG_I(NR_RRC, "add NAS info with size %d (pdusession id %d)\n", ue_p->pduSession[i].param.nas_pdu.length, ue_p->pduSession[i].param.pdusession_id);
    }
  }

  /* If list is empty free the list and reset the address */
  if (dedicatedNAS_MessageList->list.count == 0) {
    free(dedicatedNAS_MessageList);
    dedicatedNAS_MessageList = NULL;
  }

  NR_DRB_ToAddModList_t *DRBs = createDRBlist(ue_p, false);
  uint8_t buffer[NR_RRC_BUF_SIZE];
  int size = do_RRCReconfiguration(ue_p,
                                   buffer,
                                   NR_RRC_BUF_SIZE,
                                   xid,
                                   NULL,
                                   DRBs,
                                   NULL,
                                   NULL,
                                   NULL,
                                   dedicatedNAS_MessageList,
                                   NULL);
  LOG_DUMPMSG(NR_RRC, DEBUG_RRC, (char *)buffer, size, "[MSG] RRC Reconfiguration\n");
  freeDRBlist(DRBs);

  /* Free all NAS PDUs */
  for (int i = 0; i < ue_p->nb_of_pdusessions; i++)
    clear_nas_pdu(&ue_p->pduSession[i].param.nas_pdu);

  LOG_I(NR_RRC, "UE %d: Generate RRCReconfiguration (bytes %d)\n", ue_p->rrc_ue_id, size);
  nr_rrc_transfer_protected_rrc_message(rrc, ue_p, DCCH, buffer, size);
}

//-----------------------------------------------------------------------------
void rrc_gNB_generate_dedicatedRRCReconfiguration_release(gNB_RRC_INST *rrc,
                                                          gNB_RRC_UE_t *ue_p,
                                                          uint8_t xid,
                                                          uint32_t nas_length,
                                                          uint8_t *nas_buffer)
//-----------------------------------------------------------------------------
{
  NR_DRB_ToReleaseList_t *DRB_Release_configList2 = CALLOC(sizeof(*DRB_Release_configList2), 1);

  for (int i = 0; i < NB_RB_MAX; i++) {
    if ((ue_p->pduSession[i].status == PDU_SESSION_STATUS_TORELEASE) && ue_p->pduSession[i].xid == xid) {
      asn1cSequenceAdd(DRB_Release_configList2->list, NR_DRB_Identity_t, DRB_release);
      *DRB_release = i + 1;
    }
  }

  /* If list is empty free the list and reset the address */
  struct NR_RRCReconfiguration_v1530_IEs__dedicatedNAS_MessageList *dedicatedNAS_MessageList = NULL;
  if (nas_length > 0) {
    dedicatedNAS_MessageList = CALLOC(1, sizeof(*dedicatedNAS_MessageList));
    asn1cSequenceAdd(dedicatedNAS_MessageList->list, NR_DedicatedNAS_Message_t, dedicatedNAS_Message);
    OCTET_STRING_fromBuf(dedicatedNAS_Message, (char *)nas_buffer, nas_length);
    LOG_I(NR_RRC,"add NAS info with size %d\n", nas_length);
  } else {
    LOG_W(NR_RRC,"dedlicated NAS list is empty\n");
  }

  uint8_t buffer[NR_RRC_BUF_SIZE] = {0};
  int size = do_RRCReconfiguration(ue_p,
                                   buffer,
                                   NR_RRC_BUF_SIZE,
                                   xid,
                                   NULL,
                                   NULL,
                                   DRB_Release_configList2,
                                   NULL,
                                   NULL,
                                   dedicatedNAS_MessageList,
                                   NULL);
  LOG_DUMPMSG(NR_RRC,DEBUG_RRC,(char *)buffer,size, "[MSG] RRC Reconfiguration\n");

  /* Free all NAS PDUs */
  if (nas_length > 0) {
    /* Free the NAS PDU buffer and invalidate it */
    free(nas_buffer);
  }

  LOG_I(NR_RRC, "UE %d: Generate NR_RRCReconfiguration (bytes %d)\n", ue_p->rrc_ue_id, size);
  nr_rrc_transfer_protected_rrc_message(rrc, ue_p, DCCH, buffer, size);
}

/* \brief find existing PDU session inside E1AP Bearer Modif message, or
 * point to new one.
 * \param bearer_modif E1AP Bearer Modification Message
 * \param pdu_id PDU session ID
 * \return pointer to existing PDU session, or to new/unused one. */
static pdu_session_to_mod_t *find_or_next_pdu_session(e1ap_bearer_mod_req_t *bearer_modif, int pdu_id)
{
  for (int i = 0; i < bearer_modif->numPDUSessionsMod; ++i) {
    if (bearer_modif->pduSessionMod[i].sessionId == pdu_id)
      return &bearer_modif->pduSessionMod[i];
  }
  /* E1AP Bearer Modification has no PDU session to modify with that ID, create
   * new entry */
  DevAssert(bearer_modif->numPDUSessionsMod < E1AP_MAX_NUM_PDU_SESSIONS - 1);
  bearer_modif->numPDUSessionsMod += 1;
  return &bearer_modif->pduSessionMod[bearer_modif->numPDUSessionsMod - 1];
}

/**
 * @brief Notify E1 re-establishment to CU-UP
 */
static void cuup_notify_reestablishment(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue_p)
{
  e1ap_bearer_mod_req_t req = {
      .gNB_cu_cp_ue_id = ue_p->rrc_ue_id,
      .gNB_cu_up_ue_id = ue_p->rrc_ue_id,
  };
  if (!ue_associated_to_cuup(rrc, ue_p))
    return;
  /* loop through active DRBs */
  for (int drb_id = 1; drb_id <= MAX_DRBS_PER_UE; drb_id++) {
    drb_t *drb = get_drb(ue_p, drb_id);
    if (drb->status == DRB_INACTIVE)
      continue;
    /* fetch an existing PDU session for this DRB */
    rrc_pdu_session_param_t *pdu = find_pduSession_from_drbId(ue_p, drb_id);
    if (pdu == NULL) {
      LOG_E(RRC, "UE %d: E1 Bearer Context Modification: no PDU session for DRB ID %d\n", ue_p->rrc_ue_id, drb_id);
      continue;
    }
    /* Get pointer to existing (or new one) PDU session to modify in E1 */
    pdu_session_to_mod_t *pdu_e1 = find_or_next_pdu_session(&req, pdu->param.pdusession_id);
    AssertError(pdu != NULL,
                continue,
                "UE %u: E1 Bearer Context Modification: PDU session %d to setup is null\n",
                ue_p->rrc_ue_id,
                pdu->param.pdusession_id);
    /* Prepare PDU for E1 Bearear Context Modification Request */
    pdu_e1->sessionId = pdu->param.pdusession_id;
    /* Fill DRB to setup with ID, DL TL and DL TEID */
    DRB_nGRAN_to_mod_t *drb_e1 = &pdu_e1->DRBnGRanModList[pdu_e1->numDRB2Modify];
    drb_e1->id = drb_id;
    drb_e1->numDlUpParam = 1;
    memcpy(&drb_e1->DlUpParamList[0].tlAddress, &drb->du_tunnel_config.addr.buffer, sizeof(uint8_t) * 4);
    drb_e1->DlUpParamList[0].teId = drb->du_tunnel_config.teid;
    /* PDCP configuration */
    bearer_context_pdcp_config_t *pdcp_config = &drb_e1->pdcp_config;
    set_bearer_context_pdcp_config(pdcp_config, drb, rrc->configuration.um_on_default_drb);
    pdcp_config->pDCP_Reestablishment = true;
    /* increase DRB to modify counter */
    pdu_e1->numDRB2Modify += 1;
  }

  req.cipheringAlgorithm = rrc->security.do_drb_ciphering ? ue_p->ciphering_algorithm : 0;
  req.integrityProtectionAlgorithm = rrc->security.do_drb_integrity ? ue_p->integrity_algorithm : 0;
  nr_derive_key(UP_ENC_ALG, req.cipheringAlgorithm, ue_p->kgnb, (uint8_t *)req.encryptionKey);
  nr_derive_key(UP_INT_ALG, req.integrityProtectionAlgorithm, ue_p->kgnb, (uint8_t *)req.integrityProtectionKey);

  /* Send E1 Bearer Context Modification Request (3GPP TS 38.463) */
  sctp_assoc_t assoc_id = get_existing_cuup_for_ue(rrc, ue_p);
  rrc->cucp_cuup.bearer_context_mod(assoc_id, &req);
}

/**
 * @brief RRCReestablishment message
 *        Direction: Network to UE
 */
static void rrc_gNB_generate_RRCReestablishment(rrc_gNB_ue_context_t *ue_context_pP,
                                                const uint8_t *masterCellGroup_from_DU,
                                                const rnti_t old_rnti,
                                                const nr_rrc_du_container_t *du)
{
  module_id_t module_id = 0;
  gNB_RRC_INST *rrc = RC.nrrrc[module_id];
  gNB_RRC_UE_t *ue_p = &ue_context_pP->ue_context;
  uint8_t buffer[NR_RRC_BUF_SIZE] = {0};
  uint8_t xid = rrc_gNB_get_next_transaction_identifier(module_id);
  ue_p->xids[xid] = RRC_REESTABLISH;
  const f1ap_served_cell_info_t *cell_info = &du->setup_req->cell[0].info;
  uint32_t ssb_arfcn = get_ssb_arfcn(du);
  int size = do_RRCReestablishment(ue_context_pP, buffer, NR_RRC_BUF_SIZE, xid, cell_info->nr_pci, ssb_arfcn);

  LOG_I(NR_RRC, "[RAPROC] UE %04x Logical Channel DL-DCCH, Generating NR_RRCReestablishment (bytes %d)\n", ue_p->rnti, size);

  /* Ciphering and Integrity according to TS 33.501 */
  nr_pdcp_entity_security_keys_and_algos_t security_parameters;
  /* Derive the keys from kgnb */
  nr_derive_key(RRC_ENC_ALG, ue_p->ciphering_algorithm, ue_p->kgnb, security_parameters.ciphering_key);
  nr_derive_key(RRC_INT_ALG, ue_p->integrity_algorithm, ue_p->kgnb, security_parameters.integrity_key);
  LOG_I(NR_RRC,
        "Set PDCP security UE %d RNTI %04x nea %ld nia %d in RRCReestablishment\n",
        ue_p->rrc_ue_id,
        ue_p->rnti,
        ue_p->ciphering_algorithm,
        ue_p->integrity_algorithm);
  /* RRCReestablishment is integrity protected but not ciphered,
   * so let's configure only integrity protection right now.
   * Ciphering is enabled below, after generating RRCReestablishment.
   */
  security_parameters.integrity_algorithm = ue_p->integrity_algorithm;
  security_parameters.ciphering_algorithm = 0;

  /* SRBs */
  for (int srb_id = 1; srb_id < NR_NUM_SRB; srb_id++) {
    if (ue_p->Srb[srb_id].Active)
      nr_pdcp_config_set_security(ue_p->rrc_ue_id, srb_id, true, &security_parameters);
  }
  /* Re-establish PDCP for SRB1, according to 5.3.7.4 of 3GPP TS 38.331 */
  nr_pdcp_reestablishment(ue_p->rrc_ue_id,
                          1,
                          true,
                          &security_parameters);
  /* F1AP DL RRC Message Transfer */
  f1_ue_data_t ue_data = cu_get_f1_ue_data(ue_p->rrc_ue_id);
  RETURN_IF_INVALID_ASSOC_ID(ue_data.du_assoc_id);
  uint32_t old_gNB_DU_ue_id = old_rnti;
  f1ap_dl_rrc_message_t dl_rrc = {.gNB_CU_ue_id = ue_p->rrc_ue_id,
                                  .gNB_DU_ue_id = ue_data.secondary_ue,
                                  .srb_id = DCCH,
                                  .old_gNB_DU_ue_id = &old_gNB_DU_ue_id};
  deliver_dl_rrc_message_data_t data = {.rrc = rrc, .dl_rrc = &dl_rrc, .assoc_id = ue_data.du_assoc_id};
  nr_pdcp_data_req_srb(ue_p->rrc_ue_id, DCCH, rrc_gNB_mui++, size, (unsigned char *const)buffer, rrc_deliver_dl_rrc_message, &data);

  /* RRCReestablishment has been generated, let's enable ciphering now. */
  security_parameters.ciphering_algorithm = ue_p->ciphering_algorithm;
  /* SRBs */
  for (int srb_id = 1; srb_id < NR_NUM_SRB; srb_id++) {
    if (ue_p->Srb[srb_id].Active)
      nr_pdcp_config_set_security(ue_p->rrc_ue_id, srb_id, true, &security_parameters);
  }
}

/// @brief Function tha processes RRCReestablishmentComplete message sent by the UE, after RRCReestasblishment request.
static void rrc_gNB_process_RRCReestablishmentComplete(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue_p, const uint8_t xid)
{
  LOG_I(NR_RRC, "UE %d Processing NR_RRCReestablishmentComplete from UE\n", ue_p->rrc_ue_id);

  int i = 0;

  ue_p->xids[xid] = RRC_ACTION_NONE;

  ue_p->Srb[1].Active = 1;

  NR_CellGroupConfig_t *cellGroupConfig = calloc(1, sizeof(NR_CellGroupConfig_t));

  cellGroupConfig->spCellConfig = ue_p->masterCellGroup->spCellConfig;
  cellGroupConfig->mac_CellGroupConfig = ue_p->masterCellGroup->mac_CellGroupConfig;
  cellGroupConfig->physicalCellGroupConfig = ue_p->masterCellGroup->physicalCellGroupConfig;
  cellGroupConfig->rlc_BearerToReleaseList = NULL;
  cellGroupConfig->rlc_BearerToAddModList = calloc(1, sizeof(*cellGroupConfig->rlc_BearerToAddModList));

  /*
   * Get SRB2, DRB configuration from the existing UE context,
   * also start from SRB2 (i=1) and not from SRB1 (i=0).
   */
  for (i = 1; i < ue_p->masterCellGroup->rlc_BearerToAddModList->list.count; ++i)
    asn1cSeqAdd(&cellGroupConfig->rlc_BearerToAddModList->list, ue_p->masterCellGroup->rlc_BearerToAddModList->list.array[i]);

  /*
   * At this stage, PDCP entity are re-established and reestablishRLC is flagged
   * with RRCReconfiguration to complete RLC re-establishment of remaining bearers
   */
  for (i = 0; i < cellGroupConfig->rlc_BearerToAddModList->list.count; i++) {
    asn1cCallocOne(cellGroupConfig->rlc_BearerToAddModList->list.array[i]->reestablishRLC,
                   NR_RLC_BearerConfig__reestablishRLC_true);
  }

  /* Re-establish SRB2 according to clause 5.3.5.6.3 of 3GPP TS 38.331
   * (SRB1 is re-established with RRCReestablishment message)
   */
  int srb_id = 2;
  if (ue_p->Srb[srb_id].Active) {
    nr_pdcp_entity_security_keys_and_algos_t security_parameters;
    security_parameters.ciphering_algorithm = ue_p->ciphering_algorithm;
    security_parameters.integrity_algorithm = ue_p->integrity_algorithm;
    nr_derive_key(RRC_ENC_ALG, ue_p->ciphering_algorithm, ue_p->kgnb, security_parameters.ciphering_key);
    nr_derive_key(RRC_INT_ALG, ue_p->integrity_algorithm, ue_p->kgnb, security_parameters.integrity_key);

    nr_pdcp_reestablishment(ue_p->rrc_ue_id, srb_id, true, &security_parameters);
  }
  /* PDCP Reestablishment of DRBs according to 5.3.5.6.5 of 3GPP TS 38.331 (over E1) */
  cuup_notify_reestablishment(rrc, ue_p);

  /* Create srb-ToAddModList */
  NR_SRB_ToAddModList_t *SRBs = createSRBlist(ue_p, true);
  /* Create drb-ToAddModList */
  NR_DRB_ToAddModList_t *DRBs = createDRBlist(ue_p, true);

  uint8_t new_xid = rrc_gNB_get_next_transaction_identifier(rrc->module_id);
  ue_p->xids[new_xid] = RRC_REESTABLISH_COMPLETE;
  uint8_t buffer[NR_RRC_BUF_SIZE] = {0};
  int size = do_RRCReconfiguration(ue_p,
                                   buffer,
                                   NR_RRC_BUF_SIZE,
                                   new_xid,
                                   SRBs,
                                   DRBs,
                                   NULL,
                                   NULL,
                                   NULL, // MeasObj_list,
                                   NULL,
                                   cellGroupConfig);
  freeSRBlist(SRBs);
  freeDRBlist(DRBs);
  LOG_DUMPMSG(NR_RRC, DEBUG_RRC, (char *)buffer, size, "[MSG] RRC Reconfiguration\n");

  AssertFatal(size > 0, "cannot encode RRC Reconfiguration\n");
  LOG_I(NR_RRC,
        "UE %d RNTI %04x: Generate NR_RRCReconfiguration after reestablishment complete (bytes %d)\n",
        ue_p->rrc_ue_id,
        ue_p->rnti,
        size);
  nr_rrc_transfer_protected_rrc_message(rrc, ue_p, DCCH, buffer, size);
}
//-----------------------------------------------------------------------------

int nr_rrc_reconfiguration_req(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue_p, const int dl_bwp_id, const int ul_bwp_id)
{
  uint8_t xid = rrc_gNB_get_next_transaction_identifier(rrc->module_id);
  ue_p->xids[xid] = RRC_DEDICATED_RECONF;

  NR_CellGroupConfig_t *masterCellGroup = ue_p->masterCellGroup;
  if (dl_bwp_id > 0) {
    *masterCellGroup->spCellConfig->spCellConfigDedicated->firstActiveDownlinkBWP_Id = dl_bwp_id;
    *masterCellGroup->spCellConfig->spCellConfigDedicated->defaultDownlinkBWP_Id = dl_bwp_id;
  }
  if (ul_bwp_id > 0) {
    *masterCellGroup->spCellConfig->spCellConfigDedicated->uplinkConfig->firstActiveUplinkBWP_Id = ul_bwp_id;
  }

  uint8_t buffer[NR_RRC_BUF_SIZE];
  int size = do_RRCReconfiguration(ue_p, buffer, NR_RRC_BUF_SIZE, xid, NULL, NULL, NULL, NULL, NULL, NULL, masterCellGroup);

  nr_rrc_transfer_protected_rrc_message(rrc, ue_p, DCCH, buffer, size);

  return 0;
}

static void rrc_handle_RRCSetupRequest(gNB_RRC_INST *rrc,
                                       sctp_assoc_t assoc_id,
                                       const NR_RRCSetupRequest_IEs_t *rrcSetupRequest,
                                       const f1ap_initial_ul_rrc_message_t *msg)
{
  rrc_gNB_ue_context_t *ue_context_p = NULL;
  if (NR_InitialUE_Identity_PR_randomValue == rrcSetupRequest->ue_Identity.present) {
    /* randomValue                         BIT STRING (SIZE (39)) */
    if (rrcSetupRequest->ue_Identity.choice.randomValue.size != 5) { // 39-bit random value
      LOG_E(NR_RRC,
            "wrong InitialUE-Identity randomValue size, expected 5, provided %lu",
            (long unsigned int)rrcSetupRequest->ue_Identity.choice.randomValue.size);
      return;
    }
    uint64_t random_value = 0;
    memcpy(((uint8_t *)&random_value) + 3,
           rrcSetupRequest->ue_Identity.choice.randomValue.buf,
           rrcSetupRequest->ue_Identity.choice.randomValue.size);

    ue_context_p = rrc_gNB_create_ue_context(assoc_id, msg->crnti, rrc, random_value, msg->gNB_DU_ue_id);
  } else if (NR_InitialUE_Identity_PR_ng_5G_S_TMSI_Part1 == rrcSetupRequest->ue_Identity.present) {
    /* <5G-S-TMSI> = <AMF Set ID><AMF Pointer><5G-TMSI> 48-bit */
    /* ng-5G-S-TMSI-Part1                  BIT STRING (SIZE (39)) */
    if (rrcSetupRequest->ue_Identity.choice.ng_5G_S_TMSI_Part1.size != 5) {
      LOG_E(NR_RRC,
            "wrong ng_5G_S_TMSI_Part1 size, expected 5, provided %lu \n",
            (long unsigned int)rrcSetupRequest->ue_Identity.choice.ng_5G_S_TMSI_Part1.size);
      return;
    }

    uint64_t s_tmsi_part1 = BIT_STRING_to_uint64(&rrcSetupRequest->ue_Identity.choice.ng_5G_S_TMSI_Part1);
    LOG_I(NR_RRC, "Received UE 5G-S-TMSI-Part1 %ld\n", s_tmsi_part1);

    ue_context_p = rrc_gNB_create_ue_context(assoc_id, msg->crnti, rrc, s_tmsi_part1, msg->gNB_DU_ue_id);
    AssertFatal(ue_context_p != NULL, "out of memory\n");
    gNB_RRC_UE_t *UE = &ue_context_p->ue_context;
    UE->Initialue_identity_5g_s_TMSI.presence = true;
    UE->ng_5G_S_TMSI_Part1 = s_tmsi_part1;
  } else {
    uint64_t random_value = 0;
    memcpy(((uint8_t *)&random_value) + 3,
           rrcSetupRequest->ue_Identity.choice.randomValue.buf,
           rrcSetupRequest->ue_Identity.choice.randomValue.size);

    ue_context_p = rrc_gNB_create_ue_context(assoc_id, msg->crnti, rrc, random_value, msg->gNB_DU_ue_id);
    LOG_E(NR_RRC, "RRCSetupRequest without random UE identity or S-TMSI not supported, let's reject the UE %04x\n", msg->crnti);
    rrc_gNB_generate_RRCReject(0, ue_context_p);
    return;
  }

  gNB_RRC_UE_t *UE = &ue_context_p->ue_context;
  UE = &ue_context_p->ue_context;
  UE->establishment_cause = rrcSetupRequest->establishmentCause;
  activate_srb(UE, 1);
  rrc_gNB_generate_RRCSetup(0, msg->crnti, ue_context_p, msg->du2cu_rrc_container, msg->du2cu_rrc_container_length);
}

static const char *get_reestab_cause(NR_ReestablishmentCause_t c)
{
  switch (c) {
    case NR_ReestablishmentCause_otherFailure:
      return "Other Failure";
    case NR_ReestablishmentCause_handoverFailure:
      return "Handover Failure";
    case NR_ReestablishmentCause_reconfigurationFailure:
      return "Reconfiguration Failure";
    default:
      break;
  }
  return "UNKNOWN Failure (ASN.1 decoder error?)";
}

static void rrc_handle_RRCReestablishmentRequest(gNB_RRC_INST *rrc,
                                                 sctp_assoc_t assoc_id,
                                                 const NR_RRCReestablishmentRequest_IEs_t *req,
                                                 const f1ap_initial_ul_rrc_message_t *msg)
{
  uint64_t random_value = 0;
  const char *scause = get_reestab_cause(req->reestablishmentCause);
  const long physCellId = req->ue_Identity.physCellId;
  long ngap_cause = NGAP_CAUSE_RADIO_NETWORK_UNSPECIFIED; /* cause in case of NGAP release req */
  rrc_gNB_ue_context_t *ue_context_p = NULL;
  LOG_I(NR_RRC, "UE %04x physCellId %ld NR_RRCReestablishmentRequest cause %s\n", msg->crnti, physCellId, scause);

  const nr_rrc_du_container_t *du = get_du_by_assoc_id(rrc, assoc_id);
  if (du == NULL) {
    LOG_E(RRC, "received CCCH message, but no corresponding DU found\n");
    return;
  }

  // 3GPP TS 38.321 version 15.13.0 Section 7.1 Table 7.1-1: RNTI values
  if (req->ue_Identity.c_RNTI < 0x1 || req->ue_Identity.c_RNTI > 0xffef) {
    /* c_RNTI range error should not happen */
    LOG_E(NR_RRC, "NR_RRCReestablishmentRequest c_RNTI %04lx range error, fallback to RRC setup\n", req->ue_Identity.c_RNTI);
    goto fallback_rrc_setup;
  }

  if (du->mib == NULL || du->sib1 == NULL) {
    /* we don't have MIB/SIB1 of the DU, and therefore cannot generate the
     * Reestablishment (as we would need the SSB's ARFCN, which we cannot
     * compute). So generate RRC Setup instead */
    LOG_E(NR_RRC, "Reestablishment request: no MIB/SIB1 of DU present, cannot do reestablishment, force setup request\n");
    goto fallback_rrc_setup;
  }

  if (du->mtc == NULL) {
    // some UEs don't send MeasurementTimingConfiguration, so we don't know the
    // SSB ARFCN and can't do reestablishment. handle it gracefully by doing
    // RRC setup procedure instead
    LOG_E(NR_RRC, "no MeasurementTimingConfiguration for this cell, cannot perform reestablishment\n");
    ngap_cause = NGAP_CAUSE_RADIO_NETWORK_RELEASE_DUE_TO_NGRAN_GENERATED_REASON;
    goto fallback_rrc_setup;
  }

  rnti_t old_rnti = req->ue_Identity.c_RNTI;
  ue_context_p = rrc_gNB_get_ue_context_by_rnti(rrc, assoc_id, old_rnti);
  if (ue_context_p == NULL) {
    LOG_E(NR_RRC, "NR_RRCReestablishmentRequest without UE context, fallback to RRC setup\n");
    goto fallback_rrc_setup;
  }
  gNB_RRC_UE_t *UE = &ue_context_p->ue_context;

  const f1ap_served_cell_info_t *cell_info = &du->setup_req->cell[0].info;
  if (physCellId != cell_info->nr_pci) {
    /* UE was moving from previous cell so quickly that RRCReestablishment for previous cell was received in this cell */
    LOG_I(NR_RRC,
          "RRC Reestablishment Request from different physCellId (%ld) than current physCellId (%d), fallback to RRC setup\n",
          physCellId,
          cell_info->nr_pci);
    ngap_cause = NGAP_CAUSE_RADIO_NETWORK_RELEASE_DUE_TO_NGRAN_GENERATED_REASON;
    /* 38.401 8.7: "If the UE accessed from a gNB-DU other than the original
     * one, the gNB-CU should trigger the UE Context Setup procedure". Let's
     * assume that the old DU will trigger a release request, also freeing the
     * ongoing context at the CU. Hence, create new below */
    goto fallback_rrc_setup;
  }

  if (!UE->as_security_active) {
    /* no active security context, need to restart entire connection */
    LOG_E(NR_RRC, "UE requested Reestablishment without activated AS security\n");
    ngap_cause = NGAP_CAUSE_RADIO_NETWORK_RELEASE_DUE_TO_NGRAN_GENERATED_REASON;
    goto fallback_rrc_setup;
  }

  /* TODO: start timer in ITTI and drop UE if it does not come back */

  // update with new RNTI, and update secondary UE association
  UE->rnti = msg->crnti;
  f1_ue_data_t ue_data = cu_get_f1_ue_data(UE->rrc_ue_id);
  ue_data.secondary_ue = msg->gNB_DU_ue_id;
  cu_remove_f1_ue_data(UE->rrc_ue_id);
  cu_add_f1_ue_data(UE->rrc_ue_id, &ue_data);

  rrc_gNB_generate_RRCReestablishment(ue_context_p, msg->du2cu_rrc_container, old_rnti, du);
  return;

fallback_rrc_setup:
  fill_random(&random_value, sizeof(random_value));
  random_value = random_value & 0x7fffffffff; /* random value is 39 bits */

  /* request release of the "old" UE in case it exists */
  if (ue_context_p != NULL)
    rrc_gNB_send_NGAP_UE_CONTEXT_RELEASE_REQ(0, ue_context_p, NGAP_CAUSE_RADIO_NETWORK, ngap_cause);

  rrc_gNB_ue_context_t *new = rrc_gNB_create_ue_context(assoc_id, msg->crnti, rrc, random_value, msg->gNB_DU_ue_id);
  activate_srb(&new->ue_context, 1);
  rrc_gNB_generate_RRCSetup(0, msg->crnti, new, msg->du2cu_rrc_container, msg->du2cu_rrc_container_length);
  return;
}

static void process_Periodical_Measurement_Report(gNB_RRC_UE_t *ue_ctxt, NR_MeasurementReport_t *measurementReport)
{
  ASN_STRUCT_FREE(asn_DEF_NR_MeasResults, ue_ctxt->measResults);
  ue_ctxt->measResults = NULL;

  const NR_MeasId_t id = measurementReport->criticalExtensions.choice.measurementReport->measResults.measId;
  AssertFatal(id, "unexpected MeasResult for MeasurementId %ld received\n", id);
  asn1cCallocOne(ue_ctxt->measResults, measurementReport->criticalExtensions.choice.measurementReport->measResults);
  /* we "keep" the measurement report, so set to 0 */
  free(measurementReport->criticalExtensions.choice.measurementReport);
  measurementReport->criticalExtensions.choice.measurementReport = NULL;
}

static void process_Event_Based_Measurement_Report(NR_ReportConfigNR_t *report, NR_MeasurementReport_t *measurementReport)
{
  NR_EventTriggerConfig_t *event_triggered = report->reportType.choice.eventTriggered;

  int servingCellRSRP = 0;
  int neighbourCellRSRP = 0;
  int servingCellId = -1;

  switch (event_triggered->eventId.present) {
    case NR_EventTriggerConfig__eventId_PR_eventA2:
      LOG_I(NR_RRC, "HO LOG: Event A2 (Serving becomes worse than threshold)\n");
      break;

    case NR_EventTriggerConfig__eventId_PR_eventA3: {
      LOG_I(NR_RRC, "HO LOG: Event A3 Report - Neighbour Becomes Better than Serving!\n");
      const NR_MeasResults_t *measResults = &measurementReport->criticalExtensions.choice.measurementReport->measResults;

      for (int serving_cell_idx = 0; serving_cell_idx < measResults->measResultServingMOList.list.count; serving_cell_idx++) {
        const NR_MeasResultServMO_t *meas_result_serv_MO = measResults->measResultServingMOList.list.array[serving_cell_idx];
        servingCellId = *(meas_result_serv_MO->measResultServingCell.physCellId);
        if (meas_result_serv_MO->measResultServingCell.measResult.cellResults.resultsSSB_Cell) {
          servingCellRSRP = *(meas_result_serv_MO->measResultServingCell.measResult.cellResults.resultsSSB_Cell->rsrp) - 157;
        } else {
          servingCellRSRP = *(meas_result_serv_MO->measResultServingCell.measResult.cellResults.resultsCSI_RS_Cell->rsrp) - 157;
        }
        LOG_D(NR_RRC, "Serving Cell RSRP: %d\n", servingCellRSRP);
      }

      if (measResults->measResultNeighCells == NULL)
        break;

      if (measResults->measResultNeighCells->present != NR_MeasResults__measResultNeighCells_PR_measResultListNR)
        break;

      const NR_MeasResultListNR_t *measResultListNR = measResults->measResultNeighCells->choice.measResultListNR;
      for (int neigh_meas_idx = 0; neigh_meas_idx < measResultListNR->list.count; neigh_meas_idx++) {
        const NR_MeasResultNR_t *meas_result_neigh_cell = (measResultListNR->list.array[neigh_meas_idx]);
        const int neighbourCellId = *(meas_result_neigh_cell->physCellId);

        // TS 138 133 Table 10.1.6.1-1: SS-RSRP and CSI-RSRP measurement report mapping
        const struct NR_MeasResultNR__measResult__cellResults *cellResults = &(meas_result_neigh_cell->measResult.cellResults);

        if (cellResults->resultsSSB_Cell) {
          neighbourCellRSRP = *(cellResults->resultsSSB_Cell->rsrp) - 157;
        } else {
          neighbourCellRSRP = *(cellResults->resultsCSI_RS_Cell->rsrp) - 157;
        }

        LOG_I(NR_RRC,
              "HO LOG: Measurement Report for the neighbour %d with RSRP: %d\n",
              neighbourCellId,
              neighbourCellRSRP);

        const f1ap_served_cell_info_t *neighbour_cell_du_context = get_cell_information_by_phycellId(neighbourCellId);
        const f1ap_served_cell_info_t *serving_cell_du_context = get_cell_information_by_phycellId(servingCellId);
        const nr_neighbour_gnb_configuration_t *neighbour =
            get_neighbour_cell_information(serving_cell_du_context->nr_cellid, neighbourCellId);
        // CU does not have f1 connection with neighbour cell context. So  check does serving cell has this phyCellId as a
        // neighbour.
        if (!neighbour_cell_du_context && neighbour) {
          // No F1 connection but static neighbour configuration is available
          const nr_a3_event_t *a3_event_configuration = get_a3_configuration(neighbour->physicalCellId);
          // Additional check - This part can be modified according to additional cell specific Handover Margin
          if (a3_event_configuration
              && ((a3_event_configuration->a3_offset + a3_event_configuration->hysteresis)
                  < (neighbourCellRSRP - servingCellRSRP))) {
            LOG_D(NR_RRC, "HO LOG: Trigger N2 HO for the neighbour gnb: %u cell: %lu\n", neighbour->gNB_ID, neighbour->nrcell_id);
          }
        }
      }

    } break;
    default:
      LOG_D(NR_RRC, "NR_EventTriggerConfig__eventId_PR_NOTHING or Other event report\n");
      break;
  }
}

static void rrc_gNB_process_MeasurementReport(gNB_RRC_UE_t *UE, NR_MeasurementReport_t *measurementReport)
{
  NR_MeasurementReport__criticalExtensions_PR p = measurementReport->criticalExtensions.present;
  if (p != NR_MeasurementReport__criticalExtensions_PR_measurementReport
      || measurementReport->criticalExtensions.choice.measurementReport == NULL) {
    LOG_E(NR_RRC, "UE %d: expected presence of MeasurementReport, but has %d (%p)\n", UE->rrc_ue_id, p, measurementReport->criticalExtensions.choice.measurementReport);
    return;
  }

  if (LOG_DEBUGFLAG(DEBUG_ASN1))
    xer_fprint(stdout, &asn_DEF_NR_MeasurementReport, (void *)measurementReport);

  NR_MeasConfig_t *meas_config = UE->measConfig;
  if (meas_config == NULL) {
    LOG_I(NR_RRC, "Unexpected Measurement Report from UE with id: %d\n", UE->rrc_ue_id);
    return;
  }

  NR_MeasurementReport_IEs_t *measurementReport_IEs = measurementReport->criticalExtensions.choice.measurementReport;
  const NR_MeasId_t measId = measurementReport_IEs->measResults.measId;

  NR_MeasIdToAddMod_t *meas_id_s = NULL;
  for (int meas_idx = 0; meas_idx < meas_config->measIdToAddModList->list.count; meas_idx++) {
    if (measId == meas_config->measIdToAddModList->list.array[meas_idx]->measId) {
      meas_id_s = meas_config->measIdToAddModList->list.array[meas_idx];
      break;
    }
  }

  if (meas_id_s == NULL) {
    LOG_E(NR_RRC, "Incoming Meas ID with id: %d Can not Found!\n", (int)measId);
    return;
  }

  LOG_D(NR_RRC, "HO LOG: Meas Id is found: %d\n", (int)meas_id_s->measId);

  struct NR_ReportConfigToAddMod__reportConfig *report_config = NULL;
  for (int rep_id = 0; rep_id < meas_config->reportConfigToAddModList->list.count; rep_id++) {
    if (meas_id_s->reportConfigId == meas_config->reportConfigToAddModList->list.array[rep_id]->reportConfigId) {
      report_config = &meas_config->reportConfigToAddModList->list.array[rep_id]->reportConfig;
    }
  }

  if (report_config == NULL) {
    LOG_E(NR_RRC, "There is no related report configuration for this measId!\n");
    return;
  }

  if (report_config->choice.reportConfigNR->reportType.present == NR_ReportConfigNR__reportType_PR_periodical)
    return process_Periodical_Measurement_Report(UE, measurementReport);

  if (report_config->choice.reportConfigNR->reportType.present != NR_ReportConfigNR__reportType_PR_eventTriggered) {
    LOG_D(NR_RRC, "Incoming Report Type: %d is not supported! \n", report_config->choice.reportConfigNR->reportType.present);
    return;
  }

  process_Event_Based_Measurement_Report(report_config->choice.reportConfigNR, measurementReport);
}

static int handle_rrcReestablishmentComplete(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, const NR_RRCReestablishmentComplete_t *cplt)
{
  NR_RRCReestablishmentComplete__criticalExtensions_PR p = cplt->criticalExtensions.present;
  if (p != NR_RRCReestablishmentComplete__criticalExtensions_PR_rrcReestablishmentComplete) {
    LOG_E(NR_RRC, "UE %d: expected presence of rrcReestablishmentComplete, but message has %d\n", UE->rrc_ue_id, p);
    return -1;
  }
  rrc_gNB_process_RRCReestablishmentComplete(rrc, UE, cplt->rrc_TransactionIdentifier);

  UE->ue_reestablishment_counter++;
  return 0;
}

void rrc_forward_ue_nas_message(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE)
{
  if (UE->nas_pdu.buffer == NULL || UE->nas_pdu.length == 0)
    return; // no problem: the UE will re-request a NAS PDU

  uint8_t buffer[4096];
  unsigned int xid = rrc_gNB_get_next_transaction_identifier(rrc->module_id);
  uint32_t length = do_NR_DLInformationTransfer(buffer, sizeof(buffer), xid, UE->nas_pdu.length, UE->nas_pdu.buffer);
  LOG_DUMPMSG(NR_RRC, DEBUG_RRC, buffer, length, "[MSG] RRC DL Information Transfer\n");
  rb_id_t srb_id = UE->Srb[2].Active ? DCCH1 : DCCH;
  nr_rrc_transfer_protected_rrc_message(rrc, UE, srb_id, buffer, length);
  // no need to free UE->nas_pdu.buffer, do_NR_DLInformationTransfer() did that
  UE->nas_pdu.buffer = NULL;
  UE->nas_pdu.length = 0;
}

static void handle_ueCapabilityInformation(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, const NR_UECapabilityInformation_t *ue_cap_info)
{
  int xid = ue_cap_info->rrc_TransactionIdentifier;
  rrc_action_t a = UE->xids[xid];
  UE->xids[xid] = RRC_ACTION_NONE;
  if (a != RRC_UECAPABILITY_ENQUIRY) {
    LOG_E(NR_RRC, "UE %d: received unsolicited UE Capability Information, aborting procedure\n", UE->rrc_ue_id);
    return;
  }

  LOG_I(NR_RRC, "UE %d: received UE capabilities (xid %d)\n", UE->rrc_ue_id, xid);
  int eutra_index = -1;

  if (ue_cap_info->criticalExtensions.present == NR_UECapabilityInformation__criticalExtensions_PR_ueCapabilityInformation) {
    const NR_UE_CapabilityRAT_ContainerList_t *ue_CapabilityRAT_ContainerList =
        ue_cap_info->criticalExtensions.choice.ueCapabilityInformation->ue_CapabilityRAT_ContainerList;

    /* Encode UE-CapabilityRAT-ContainerList for sending to the DU */
    free(UE->ue_cap_buffer.buf);
    UE->ue_cap_buffer.len = uper_encode_to_new_buffer(&asn_DEF_NR_UE_CapabilityRAT_ContainerList,
                                                      NULL,
                                                      ue_CapabilityRAT_ContainerList,
                                                      (void **)&UE->ue_cap_buffer.buf);
    if (UE->ue_cap_buffer.len <= 0) {
      LOG_E(RRC, "could not encode UE-CapabilityRAT-ContainerList, abort handling capabilities\n");
      return;
    }

    for (int i = 0; i < ue_CapabilityRAT_ContainerList->list.count; i++) {
      const NR_UE_CapabilityRAT_Container_t *ue_cap_container = ue_CapabilityRAT_ContainerList->list.array[i];
      if (ue_cap_container->rat_Type == NR_RAT_Type_nr) {
        if (UE->UE_Capability_nr) {
          ASN_STRUCT_FREE(asn_DEF_NR_UE_NR_Capability, UE->UE_Capability_nr);
          UE->UE_Capability_nr = 0;
        }

        asn_dec_rval_t dec_rval = uper_decode(NULL,
                                              &asn_DEF_NR_UE_NR_Capability,
                                              (void **)&UE->UE_Capability_nr,
                                              ue_cap_container->ue_CapabilityRAT_Container.buf,
                                              ue_cap_container->ue_CapabilityRAT_Container.size,
                                              0,
                                              0);
        if (LOG_DEBUGFLAG(DEBUG_ASN1)) {
          xer_fprint(stdout, &asn_DEF_NR_UE_NR_Capability, UE->UE_Capability_nr);
        }

        if ((dec_rval.code != RC_OK) && (dec_rval.consumed == 0)) {
          LOG_E(NR_RRC, "UE %d: Failed to decode nr UE capabilities (%zu bytes)\n", UE->rrc_ue_id, dec_rval.consumed);
          ASN_STRUCT_FREE(asn_DEF_NR_UE_NR_Capability, UE->UE_Capability_nr);
          UE->UE_Capability_nr = 0;
        }

        UE->UE_Capability_size = ue_cap_container->ue_CapabilityRAT_Container.size;
        if (eutra_index != -1) {
          LOG_E(NR_RRC, "fatal: more than 1 eutra capability\n");
          return;
        }
        eutra_index = i;
      }

      if (ue_cap_container->rat_Type == NR_RAT_Type_eutra_nr) {
        if (UE->UE_Capability_MRDC) {
          ASN_STRUCT_FREE(asn_DEF_NR_UE_MRDC_Capability, UE->UE_Capability_MRDC);
          UE->UE_Capability_MRDC = 0;
        }
        asn_dec_rval_t dec_rval = uper_decode(NULL,
                                              &asn_DEF_NR_UE_MRDC_Capability,
                                              (void **)&UE->UE_Capability_MRDC,
                                              ue_cap_container->ue_CapabilityRAT_Container.buf,
                                              ue_cap_container->ue_CapabilityRAT_Container.size,
                                              0,
                                              0);

        if (LOG_DEBUGFLAG(DEBUG_ASN1)) {
          xer_fprint(stdout, &asn_DEF_NR_UE_MRDC_Capability, UE->UE_Capability_MRDC);
        }

        if ((dec_rval.code != RC_OK) && (dec_rval.consumed == 0)) {
          LOG_E(NR_RRC, "UE %d: Failed to decode nr UE capabilities (%zu bytes)\n", UE->rrc_ue_id, dec_rval.consumed);
          ASN_STRUCT_FREE(asn_DEF_NR_UE_MRDC_Capability, UE->UE_Capability_MRDC);
          UE->UE_Capability_MRDC = 0;
        }
        UE->UE_MRDC_Capability_size = ue_cap_container->ue_CapabilityRAT_Container.size;
      }

      if (ue_cap_container->rat_Type == NR_RAT_Type_eutra) {
        // TODO
      }
    }

    if (eutra_index == -1)
      return;
  }

  rrc_gNB_send_NGAP_UE_CAPABILITIES_IND(rrc, UE, ue_cap_info);

  if (UE->n_initial_pdu > 0) {
    /* there were PDU sessions with the NG UE Context setup, but we had to set
     * up security and request capabilities, so trigger PDU sessions now. The
     * UE NAS message will be forwarded in the corresponding reconfiguration,
     * the Initial context setup response after reconfiguration complete. */
    trigger_bearer_setup(rrc, UE, UE->n_initial_pdu, UE->initial_pdus, 0);
  } else {
    rrc_gNB_send_NGAP_INITIAL_CONTEXT_SETUP_RESP(rrc, UE);
    rrc_forward_ue_nas_message(rrc, UE);
  }

  return;
}

static void handle_rrcSetupComplete(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, const NR_RRCSetupComplete_t *setup_complete)
{
  uint8_t xid = setup_complete->rrc_TransactionIdentifier;
  UE->xids[xid] = RRC_ACTION_NONE;

  if (setup_complete->criticalExtensions.present != NR_RRCSetupComplete__criticalExtensions_PR_rrcSetupComplete) {
    LOG_E(NR_RRC, "malformed RRCSetupComplete received from UE %d\n", UE->rrc_ue_id);
    return;
  }

  NR_RRCSetupComplete_IEs_t *setup_complete_ies = setup_complete->criticalExtensions.choice.rrcSetupComplete;

  if (setup_complete_ies->ng_5G_S_TMSI_Value != NULL) {
    uint64_t fiveg_s_TMSI = 0;
    if (setup_complete_ies->ng_5G_S_TMSI_Value->present == NR_RRCSetupComplete_IEs__ng_5G_S_TMSI_Value_PR_ng_5G_S_TMSI_Part2) {
      const BIT_STRING_t *part2 = &setup_complete_ies->ng_5G_S_TMSI_Value->choice.ng_5G_S_TMSI_Part2;
      if (part2->size != 2) {
        LOG_E(NR_RRC, "wrong ng_5G_S_TMSI_Part2 size, expected 2, provided %lu", part2->size);
        return;
      }

      if (UE->Initialue_identity_5g_s_TMSI.presence) {
        uint16_t stmsi_part2 = BIT_STRING_to_uint16(part2);
        LOG_I(RRC, "s_tmsi part2 %d (%02x %02x)\n", stmsi_part2, part2->buf[0], part2->buf[1]);
        // Part2 is leftmost 9, Part1 is rightmost 39 bits of 5G-S-TMSI
        fiveg_s_TMSI = ((uint64_t) stmsi_part2) << 39 | UE->ng_5G_S_TMSI_Part1;
      } else {
        LOG_W(RRC, "UE %d received 5G-S-TMSI-Part2, but no 5G-S-TMSI-Part1 present, won't send 5G-S-TMSI to core\n", UE->rrc_ue_id);
        UE->Initialue_identity_5g_s_TMSI.presence = false;
      }
    } else if (setup_complete_ies->ng_5G_S_TMSI_Value->present == NR_RRCSetupComplete_IEs__ng_5G_S_TMSI_Value_PR_ng_5G_S_TMSI) {
      const NR_NG_5G_S_TMSI_t *bs_stmsi = &setup_complete_ies->ng_5G_S_TMSI_Value->choice.ng_5G_S_TMSI;
      if (bs_stmsi->size != 6) {
        LOG_E(NR_RRC, "wrong ng_5G_S_TMSI size, expected 6, provided %lu", bs_stmsi->size);
        return;
      }

      fiveg_s_TMSI = BIT_STRING_to_uint64(bs_stmsi);
      UE->Initialue_identity_5g_s_TMSI.presence = true;
    }

    if (UE->Initialue_identity_5g_s_TMSI.presence) {
      uint16_t amf_set_id = fiveg_s_TMSI >> 38;
      uint8_t amf_pointer = (fiveg_s_TMSI >> 32) & 0x3F;
      uint32_t fiveg_tmsi = (uint32_t) fiveg_s_TMSI;
      LOG_I(NR_RRC,
            "5g_s_TMSI: 0x%lX, amf_set_id: 0x%X (%d), amf_pointer: 0x%X (%d), 5g TMSI: 0x%X \n",
            fiveg_s_TMSI,
            amf_set_id,
            amf_set_id,
            amf_pointer,
            amf_pointer,
            fiveg_tmsi);
      UE->Initialue_identity_5g_s_TMSI.amf_set_id = amf_set_id;
      UE->Initialue_identity_5g_s_TMSI.amf_pointer = amf_pointer;
      UE->Initialue_identity_5g_s_TMSI.fiveg_tmsi = fiveg_tmsi;

      // update random identity with 5G-S-TMSI, which only contained Part1 of it
      UE->random_ue_identity = fiveg_s_TMSI;
    }
  }

  rrc_gNB_process_RRCSetupComplete(rrc, UE, setup_complete->criticalExtensions.choice.rrcSetupComplete);
  return;
}

static void handle_rrcReconfigurationComplete(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, const NR_RRCReconfigurationComplete_t *reconfig_complete)
{
  uint8_t xid = reconfig_complete->rrc_TransactionIdentifier;
  UE->ue_reconfiguration_counter++;
  LOG_I(NR_RRC, "UE %d: Receive RRC Reconfiguration Complete message (xid %d)\n", UE->rrc_ue_id, xid);

  switch (UE->xids[xid]) {
    case RRC_PDUSESSION_RELEASE: {
      gtpv1u_gnb_delete_tunnel_req_t req = {0};
      gtpv1u_delete_ngu_tunnel(rrc->module_id, &req);
      // NGAP_PDUSESSION_RELEASE_RESPONSE
      rrc_gNB_send_NGAP_PDUSESSION_RELEASE_RESPONSE(rrc, UE, xid);
    } break;
    case RRC_PDUSESSION_ESTABLISH:
      if (UE->n_initial_pdu > 0) {
        /* PDU sessions through initial UE context setup */
        rrc_gNB_send_NGAP_INITIAL_CONTEXT_SETUP_RESP(rrc, UE);
        UE->n_initial_pdu = 0;
        free(UE->initial_pdus);
        UE->initial_pdus = NULL;
      } else if (UE->nb_of_pdusessions > 0)
        rrc_gNB_send_NGAP_PDUSESSION_SETUP_RESP(rrc, UE, xid);
      break;
    case RRC_PDUSESSION_MODIFY:
      rrc_gNB_send_NGAP_PDUSESSION_MODIFY_RESP(rrc, UE, xid);
      break;
    case RRC_REESTABLISH_COMPLETE:
    case RRC_DEDICATED_RECONF:
      /* do nothing */
      break;
    case RRC_ACTION_NONE:
      LOG_E(RRC, "UE %d: Received RRC Reconfiguration Complete with xid %d while no transaction is ongoing\n", UE->rrc_ue_id, xid);
      break;
    default:
      LOG_E(RRC, "UE %d: Received unexpected transaction type %d for xid %d\n", UE->rrc_ue_id, UE->xids[xid], xid);
      break;
  }
  UE->xids[xid] = RRC_ACTION_NONE;
  for (int i = 0; i < NR_RRC_TRANSACTION_IDENTIFIER_NUMBER; ++i) {
    if (UE->xids[i] != RRC_ACTION_NONE) {
      LOG_I(RRC, "UE %d: transaction %d still ongoing for action %d\n", UE->rrc_ue_id, i, UE->xids[i]);
    }
  }
}

static void rrc_gNB_generate_UECapabilityEnquiry(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue)
{
  uint8_t buffer[100];

  T(T_ENB_RRC_UE_CAPABILITY_ENQUIRY, T_INT(rrc->module_id), T_INT(0), T_INT(0), T_INT(ue->rrc_ue_id));
  uint8_t xid = rrc_gNB_get_next_transaction_identifier(rrc->module_id);
  ue->xids[xid] = RRC_UECAPABILITY_ENQUIRY;
  int size = do_NR_SA_UECapabilityEnquiry(buffer, xid);
  LOG_I(NR_RRC, "UE %d: Logical Channel DL-DCCH, Generate NR UECapabilityEnquiry (bytes %d, xid %d)\n", ue->rrc_ue_id, size, xid);

  AssertFatal(!NODE_IS_DU(rrc->node_type), "illegal node type DU!\n");

  nr_rrc_transfer_protected_rrc_message(rrc, ue, DCCH, buffer, size);
}

static int rrc_gNB_decode_dcch(gNB_RRC_INST *rrc, const f1ap_ul_rrc_message_t *msg)
{
  /* we look up by CU UE ID! Do NOT change back to RNTI! */
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(rrc, msg->gNB_CU_ue_id);
  if (!ue_context_p) {
    LOG_E(RRC, "could not find UE context for CU UE ID %u, aborting transaction\n", msg->gNB_CU_ue_id);
    return -1;
  }
  gNB_RRC_UE_t *UE = &ue_context_p->ue_context;

  if (msg->srb_id < 1 || msg->srb_id > 2) {
    LOG_E(NR_RRC, "Received message on SRB %d, discarding message\n", msg->srb_id);
    return -1;
  }

  LOG_D(NR_RRC, "UE %d: Decoding DCCH %d size %d\n", UE->rrc_ue_id, msg->srb_id, msg->rrc_container_length);
  LOG_DUMPMSG(RRC, DEBUG_RRC, (char *)msg->rrc_container, msg->rrc_container_length, "[MSG] RRC UL Information Transfer \n");
  NR_UL_DCCH_Message_t *ul_dcch_msg = NULL;
  asn_dec_rval_t dec_rval =
      uper_decode(NULL, &asn_DEF_NR_UL_DCCH_Message, (void **)&ul_dcch_msg, msg->rrc_container, msg->rrc_container_length, 0, 0);

  if ((dec_rval.code != RC_OK) && (dec_rval.consumed == 0)) {
    LOG_E(NR_RRC, "UE %d: Failed to decode UL-DCCH (%zu bytes)\n", UE->rrc_ue_id, dec_rval.consumed);
    return -1;
  }

  if (LOG_DEBUGFLAG(DEBUG_ASN1)) {
    xer_fprint(stdout, &asn_DEF_NR_UL_DCCH_Message, (void *)ul_dcch_msg);
  }

  if (ul_dcch_msg->message.present == NR_UL_DCCH_MessageType_PR_c1) {
    switch (ul_dcch_msg->message.choice.c1->present) {
      case NR_UL_DCCH_MessageType__c1_PR_NOTHING:
        LOG_I(NR_RRC, "Received PR_NOTHING on UL-DCCH-Message\n");
        break;

      case NR_UL_DCCH_MessageType__c1_PR_rrcReconfigurationComplete:
        handle_rrcReconfigurationComplete(rrc, UE, ul_dcch_msg->message.choice.c1->choice.rrcReconfigurationComplete);
        break;

      case NR_UL_DCCH_MessageType__c1_PR_rrcSetupComplete:
        handle_rrcSetupComplete(rrc, UE, ul_dcch_msg->message.choice.c1->choice.rrcSetupComplete);
        break;

      case NR_UL_DCCH_MessageType__c1_PR_measurementReport:
        rrc_gNB_process_MeasurementReport(UE, ul_dcch_msg->message.choice.c1->choice.measurementReport);
        break;

      case NR_UL_DCCH_MessageType__c1_PR_ulInformationTransfer:
        rrc_gNB_send_NGAP_UPLINK_NAS(rrc, UE, ul_dcch_msg);
        break;

      case NR_UL_DCCH_MessageType__c1_PR_securityModeComplete:
        /* configure ciphering */
        nr_rrc_pdcp_config_security(UE, true);
        UE->as_security_active = true;

        /* trigger UE capability enquiry if we don't have them yet */
        if (UE->ue_cap_buffer.len == 0) {
          rrc_gNB_generate_UECapabilityEnquiry(rrc, UE);
          /* else blocks are executed after receiving UE capability info */
        } else if (UE->n_initial_pdu > 0) {
          /* there were PDU sessions with the NG UE Context setup, but we had
           * to set up security, so trigger PDU sessions now. The UE NAS
           * message will be forwarded in the corresponding reconfiguration,
           * the Initial context setup response after reconfiguration complete. */
          trigger_bearer_setup(rrc, UE, UE->n_initial_pdu, UE->initial_pdus, 0);
        } else {
          /* we already have capabilities, and no PDU sessions to setup, ack
           * this UE */
          rrc_gNB_send_NGAP_INITIAL_CONTEXT_SETUP_RESP(rrc, UE);
          rrc_forward_ue_nas_message(rrc, UE);
        }
        break;

      case NR_UL_DCCH_MessageType__c1_PR_securityModeFailure:
        LOG_E(NR_RRC, "UE %d: received securityModeFailure\n", ue_context_p->ue_context.rrc_ue_id);
        LOG_W(NR_RRC, "Cannot continue as no AS security is activated (implementation missing)\n");
        break;

      case NR_UL_DCCH_MessageType__c1_PR_ueCapabilityInformation:
        handle_ueCapabilityInformation(rrc, UE, ul_dcch_msg->message.choice.c1->choice.ueCapabilityInformation);
        break;

      case NR_UL_DCCH_MessageType__c1_PR_rrcReestablishmentComplete:
        handle_rrcReestablishmentComplete(rrc, UE, ul_dcch_msg->message.choice.c1->choice.rrcReestablishmentComplete);
        break;

      default:
        break;
    }
  }
  ASN_STRUCT_FREE(asn_DEF_NR_UL_DCCH_Message, ul_dcch_msg);
  return 0;
}

void rrc_gNB_process_initial_ul_rrc_message(sctp_assoc_t assoc_id, const f1ap_initial_ul_rrc_message_t *ul_rrc)
{
  AssertFatal(assoc_id != 0, "illegal assoc_id == 0: should be -1 (monolithic) or >0 (split)\n");

  gNB_RRC_INST *rrc = RC.nrrrc[0];
  LOG_I(NR_RRC, "Decoding CCCH: RNTI %04x, payload_size %d\n", ul_rrc->crnti, ul_rrc->rrc_container_length);
  NR_UL_CCCH_Message_t *ul_ccch_msg = NULL;
  asn_dec_rval_t dec_rval = uper_decode(NULL,
                                        &asn_DEF_NR_UL_CCCH_Message,
                                        (void **)&ul_ccch_msg,
                                        ul_rrc->rrc_container,
                                        ul_rrc->rrc_container_length,
                                        0,
                                        0);
  if (dec_rval.code != RC_OK || dec_rval.consumed == 0) {
    LOG_E(NR_RRC, " FATAL Error in receiving CCCH\n");
    return;
  }

  if (ul_ccch_msg->message.present == NR_UL_CCCH_MessageType_PR_c1) {
    switch (ul_ccch_msg->message.choice.c1->present) {
      case NR_UL_CCCH_MessageType__c1_PR_NOTHING:
        LOG_W(NR_RRC, "Received PR_NOTHING on UL-CCCH-Message, ignoring message\n");
        break;

      case NR_UL_CCCH_MessageType__c1_PR_rrcSetupRequest:
        LOG_D(NR_RRC, "Received RRCSetupRequest on UL-CCCH-Message (UE rnti %04x)\n", ul_rrc->crnti);
        rrc_handle_RRCSetupRequest(rrc, assoc_id, &ul_ccch_msg->message.choice.c1->choice.rrcSetupRequest->rrcSetupRequest, ul_rrc);
        break;

      case NR_UL_CCCH_MessageType__c1_PR_rrcResumeRequest:
        LOG_E(NR_RRC, "Received rrcResumeRequest message, but handling is not implemented\n");
        break;

      case NR_UL_CCCH_MessageType__c1_PR_rrcReestablishmentRequest: {
        LOG_D(NR_RRC, "Received RRCReestablishmentRequest on UL-CCCH-Message (UE RNTI %04x)\n", ul_rrc->crnti);
        rrc_handle_RRCReestablishmentRequest(
            rrc,
            assoc_id,
            &ul_ccch_msg->message.choice.c1->choice.rrcReestablishmentRequest->rrcReestablishmentRequest,
            ul_rrc);
      } break;

      case NR_UL_CCCH_MessageType__c1_PR_rrcSystemInfoRequest:
        LOG_I(NR_RRC, "UE %04x receive rrcSystemInfoRequest message \n", ul_rrc->crnti);
        /* TODO */
        break;

      default:
        LOG_E(NR_RRC, "UE %04x Unknown message\n", ul_rrc->crnti);
        break;
    }
  }
  ASN_STRUCT_FREE(asn_DEF_NR_UL_CCCH_Message, ul_ccch_msg);
}

void rrc_gNB_process_release_request(const module_id_t gnb_mod_idP, x2ap_ENDC_sgnb_release_request_t *m)
{
  gNB_RRC_INST *rrc = RC.nrrrc[gnb_mod_idP];
  rrc_remove_nsa_user(rrc, m->rnti);
}

void rrc_gNB_process_dc_overall_timeout(const module_id_t gnb_mod_idP, x2ap_ENDC_dc_overall_timeout_t *m)
{
  gNB_RRC_INST *rrc = RC.nrrrc[gnb_mod_idP];
  rrc_remove_nsa_user(rrc, m->rnti);
}

/* \brief fill E1 bearer modification's DRB from F1 DRB
 * \param drb_e1 pointer to a DRB inside an E1 bearer modification message
 * \param drb_f1 pointer to a DRB inside an F1 UE Ctxt modification Response */
static void fill_e1_bearer_modif(DRB_nGRAN_to_setup_t *drb_e1, const f1ap_drb_to_be_setup_t *drb_f1)
{
  drb_e1->id = drb_f1->drb_id;
  drb_e1->numDlUpParam = drb_f1->up_dl_tnl_length;
  drb_e1->DlUpParamList[0].tlAddress = drb_f1->up_dl_tnl[0].tl_address;
  drb_e1->DlUpParamList[0].teId = drb_f1->up_dl_tnl[0].teid;
}

/**
 * @brief Store F1-U DL TL and TEID in RRC
*/
static void f1u_dl_gtp_update(f1u_tunnel_t *f1u, const f1ap_up_tnl_t *p)
{
  f1u->teid = p->teid;
  memcpy(&f1u->addr.buffer, &p->tl_address, sizeof(uint8_t) * 4);
  f1u->addr.length = sizeof(in_addr_t);
}

/**
 * @brief Update DRB TEID information in RRC storage from received DRB list
 */
static void store_du_f1u_tunnel(const f1ap_drb_to_be_setup_t *drbs, int n, gNB_RRC_UE_t *ue)
{
  for (int i = 0; i < n; i++) {
    const f1ap_drb_to_be_setup_t *drb_f1 = &drbs[i];
    AssertFatal(drb_f1->up_dl_tnl_length == 1, "can handle only one UP param\n");
    AssertFatal(drb_f1->drb_id < MAX_DRBS_PER_UE, "illegal DRB ID %ld\n", drb_f1->drb_id);
    drb_t *drb = get_drb(ue, drb_f1->drb_id);
    f1u_dl_gtp_update(&drb->du_tunnel_config, &drb_f1->up_dl_tnl[0]);
  }
}

/*
 * @brief Store F1-U UL TEID and address in RRC
 */
static void f1u_ul_gtp_update(f1u_tunnel_t *f1u, const up_params_t *p)
{
  f1u->teid = p->teId;
  memcpy(&f1u->addr.buffer, &p->tlAddress, sizeof(uint8_t) * 4);
  f1u->addr.length = sizeof(in_addr_t);
}

/* \brief use list of DRBs and send the corresponding bearer update message via
 * E1 to the CU of this UE. Also updates TEID info internally */
static void e1_send_bearer_updates(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, int n, f1ap_drb_to_be_setup_t *drbs)
{
  // we assume the same UE ID in CU-UP and CU-CP
  e1ap_bearer_mod_req_t req = {
    .gNB_cu_cp_ue_id = UE->rrc_ue_id,
    .gNB_cu_up_ue_id = UE->rrc_ue_id,
  };

  for (int i = 0; i < n; i++) {
    const f1ap_drb_to_be_setup_t *drb_f1 = &drbs[i];
    rrc_pdu_session_param_t *pdu_ue = find_pduSession_from_drbId(UE, drb_f1->drb_id);
    if (pdu_ue == NULL) {
      LOG_E(RRC, "UE %d: UE Context Modif Response: no PDU session for DRB ID %ld\n", UE->rrc_ue_id, drb_f1->drb_id);
      continue;
    }
    pdu_session_to_setup_t *pdu_e1 = find_or_next_pdu_session(&req, pdu_ue->param.pdusession_id);
    DevAssert(pdu_e1 != NULL);
    pdu_e1->sessionId = pdu_ue->param.pdusession_id;
    DRB_nGRAN_to_setup_t *drb_e1 = &pdu_e1->DRBnGRanModList[pdu_e1->numDRB2Modify];
    /* Fill E1 bearer context modification */
    fill_e1_bearer_modif(drb_e1, drb_f1);
    pdu_e1->numDRB2Modify += 1;
  }
  DevAssert(req.numPDUSessionsMod > 0);
  DevAssert(req.numPDUSessions == 0);

  req.cipheringAlgorithm = rrc->security.do_drb_ciphering ? UE->ciphering_algorithm : 0;
  req.integrityProtectionAlgorithm = rrc->security.do_drb_integrity ? UE->integrity_algorithm : 0;
  nr_derive_key(UP_ENC_ALG, req.cipheringAlgorithm, UE->kgnb, (uint8_t *)req.encryptionKey);
  nr_derive_key(UP_INT_ALG, req.integrityProtectionAlgorithm, UE->kgnb, (uint8_t *)req.integrityProtectionKey);

  // send the E1 bearer modification request message to update F1-U tunnel info
  sctp_assoc_t assoc_id = get_existing_cuup_for_ue(rrc, UE);
  rrc->cucp_cuup.bearer_context_mod(assoc_id, &req);
}

static void rrc_CU_process_ue_context_setup_response(MessageDef *msg_p, instance_t instance)
{
  f1ap_ue_context_setup_t *resp = &F1AP_UE_CONTEXT_SETUP_RESP(msg_p);
  gNB_RRC_INST *rrc = RC.nrrrc[instance];
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(rrc, resp->gNB_CU_ue_id);
  if (!ue_context_p) {
    LOG_E(RRC, "could not find UE context for CU UE ID %u, aborting transaction\n", resp->gNB_CU_ue_id);
    return;
  }
  gNB_RRC_UE_t *UE = &ue_context_p->ue_context;
  UE->f1_ue_context_active = true;

  NR_CellGroupConfig_t *cellGroupConfig = NULL;
  asn_dec_rval_t dec_rval = uper_decode_complete(NULL,
                                                 &asn_DEF_NR_CellGroupConfig,
                                                 (void **)&cellGroupConfig,
                                                 (uint8_t *)resp->du_to_cu_rrc_information->cellGroupConfig,
                                                 resp->du_to_cu_rrc_information->cellGroupConfig_length);
  AssertFatal(dec_rval.code == RC_OK && dec_rval.consumed > 0, "Cell group config decode error\n");

  if (UE->masterCellGroup) {
    ASN_STRUCT_FREE(asn_DEF_NR_CellGroupConfig, UE->masterCellGroup);
    LOG_I(RRC, "UE %04x replacing existing CellGroupConfig with new one received from DU\n", UE->rnti);
  }
  UE->masterCellGroup = cellGroupConfig;
  if (LOG_DEBUGFLAG(DEBUG_ASN1))
    xer_fprint(stdout, &asn_DEF_NR_CellGroupConfig, UE->masterCellGroup);

  if (resp->drbs_to_be_setup_length > 0) {
    /* Note: we would ideally check that SRB2 is acked, but at least LiteOn DU
     * seems buggy and does not ack, so simply check that locally we activated */
    AssertFatal(UE->Srb[1].Active && UE->Srb[2].Active, "SRBs 1 and 2 must be active during DRB Establishment");
    store_du_f1u_tunnel(resp->drbs_to_be_setup, resp->drbs_to_be_setup_length, UE);
    e1_send_bearer_updates(rrc, UE, resp->drbs_to_be_setup_length, resp->drbs_to_be_setup);
  }

  rrc_gNB_generate_dedicatedRRCReconfiguration(rrc, UE);
}

static void rrc_CU_process_ue_context_release_request(MessageDef *msg_p)
{
  const int instance = 0;
  f1ap_ue_context_release_req_t *req = &F1AP_UE_CONTEXT_RELEASE_REQ(msg_p);
  gNB_RRC_INST *rrc = RC.nrrrc[instance];
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(rrc, req->gNB_CU_ue_id);
  if (!ue_context_p) {
    LOG_E(RRC, "could not find UE context for CU UE ID %u, aborting transaction\n", req->gNB_CU_ue_id);
    return;
  }

  /* TODO: marshall types correctly */
  LOG_I(NR_RRC, "received UE Context Release Request for UE %u, forwarding to AMF\n", req->gNB_CU_ue_id);
  rrc_gNB_send_NGAP_UE_CONTEXT_RELEASE_REQ(instance,
                                           ue_context_p,
                                           NGAP_CAUSE_RADIO_NETWORK,
                                           NGAP_CAUSE_RADIO_NETWORK_RADIO_CONNECTION_WITH_UE_LOST);
}

static void rrc_delete_ue_data(gNB_RRC_UE_t *UE)
{
  ASN_STRUCT_FREE(asn_DEF_NR_UE_NR_Capability, UE->UE_Capability_nr);
  ASN_STRUCT_FREE(asn_DEF_NR_CellGroupConfig, UE->masterCellGroup);
  ASN_STRUCT_FREE(asn_DEF_NR_MeasResults, UE->measResults);
  free_MeasConfig(UE->measConfig);
}

void rrc_remove_ue(gNB_RRC_INST *rrc, rrc_gNB_ue_context_t *ue_context_p)
{
  gNB_RRC_UE_t *UE = &ue_context_p->ue_context;
  /* we call nr_pdcp_remove_UE() in the handler of E1 bearer release, but if we
   * are in E1, we also need to free the UE in the CU-CP, so call it twice to
   * cover all cases */
  nr_pdcp_remove_UE(UE->rrc_ue_id);
  uint32_t pdu_sessions[256];
  for (int i = 0; i < UE->nb_of_pdusessions && i < 256; ++i)
    pdu_sessions[i] = UE->pduSession[i].param.pdusession_id;
  rrc_gNB_send_NGAP_UE_CONTEXT_RELEASE_COMPLETE(0, UE->rrc_ue_id, UE->nb_of_pdusessions, pdu_sessions);
  LOG_I(NR_RRC, "removed UE CU UE ID %u/RNTI %04x \n", UE->rrc_ue_id, UE->rnti);
  rrc_delete_ue_data(UE);
  rrc_gNB_remove_ue_context(rrc, ue_context_p);
}

static void rrc_CU_process_ue_context_release_complete(MessageDef *msg_p)
{
  const int instance = 0;
  f1ap_ue_context_release_complete_t *complete = &F1AP_UE_CONTEXT_RELEASE_COMPLETE(msg_p);
  gNB_RRC_INST *rrc = RC.nrrrc[instance];
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(rrc, complete->gNB_CU_ue_id);
  if (!ue_context_p) {
    LOG_E(RRC, "could not find UE context for CU UE ID %u, aborting transaction\n", complete->gNB_CU_ue_id);
    return;
  }

  gNB_RRC_UE_t *UE = &ue_context_p->ue_context;
  if (UE->an_release) {
    /* only trigger release if it has been requested by core
     * otherwise, it might be CU that requested release on a DU during normal
     * operation (i.e, handover) */
    rrc_remove_ue(RC.nrrrc[0], ue_context_p);
  }
}

static void rrc_CU_process_ue_context_modification_response(MessageDef *msg_p, instance_t instance)
{
  f1ap_ue_context_modif_resp_t *resp = &F1AP_UE_CONTEXT_MODIFICATION_RESP(msg_p);
  gNB_RRC_INST *rrc = RC.nrrrc[instance];
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(rrc, resp->gNB_CU_ue_id);
  if (!ue_context_p) {
    LOG_E(RRC, "could not find UE context for CU UE ID %u, aborting transaction\n", resp->gNB_CU_ue_id);
    return;
  }
  gNB_RRC_UE_t *UE = &ue_context_p->ue_context;

  if (resp->drbs_to_be_setup_length > 0) {
    store_du_f1u_tunnel(resp->drbs_to_be_setup, resp->drbs_to_be_setup_length, UE);
    e1_send_bearer_updates(rrc, UE, resp->drbs_to_be_setup_length, resp->drbs_to_be_setup);
  }

  if (resp->du_to_cu_rrc_information != NULL && resp->du_to_cu_rrc_information->cellGroupConfig != NULL) {
    NR_CellGroupConfig_t *cellGroupConfig = NULL;
    asn_dec_rval_t dec_rval = uper_decode_complete(NULL,
                                                   &asn_DEF_NR_CellGroupConfig,
                                                   (void **)&cellGroupConfig,
                                                   (uint8_t *)resp->du_to_cu_rrc_information->cellGroupConfig,
                                                   resp->du_to_cu_rrc_information->cellGroupConfig_length);
    AssertFatal(dec_rval.code == RC_OK && dec_rval.consumed > 0, "Cell group config decode error\n");

    if (UE->masterCellGroup) {
      ASN_STRUCT_FREE(asn_DEF_NR_CellGroupConfig, UE->masterCellGroup);
      LOG_I(RRC, "UE %04x replacing existing CellGroupConfig with new one received from DU\n", UE->rnti);
    }
    UE->masterCellGroup = cellGroupConfig;

    rrc_gNB_generate_dedicatedRRCReconfiguration(rrc, UE);
  }
}

static void rrc_CU_process_ue_modification_required(MessageDef *msg_p, instance_t instance)
{
  gNB_RRC_INST *rrc = RC.nrrrc[instance];
  f1ap_ue_context_modif_required_t *required = &F1AP_UE_CONTEXT_MODIFICATION_REQUIRED(msg_p);
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(rrc, required->gNB_CU_ue_id);
  if (ue_context_p == NULL) {
    LOG_E(RRC, "Could not find UE context for CU UE ID %d, cannot handle UE context modification request\n", required->gNB_CU_ue_id);
    f1ap_ue_context_modif_refuse_t refuse = {
      .gNB_CU_ue_id = required->gNB_CU_ue_id,
      .gNB_DU_ue_id = required->gNB_DU_ue_id,
      .cause = F1AP_CAUSE_RADIO_NETWORK,
      .cause_value = F1AP_CauseRadioNetwork_unknown_or_already_allocated_gnb_cu_ue_f1ap_id,
    };
    rrc->mac_rrc.ue_context_modification_refuse(msg_p->ittiMsgHeader.originInstance, &refuse);
    return;
  }

  if (required->du_to_cu_rrc_information && required->du_to_cu_rrc_information->cellGroupConfig) {
    gNB_RRC_UE_t *UE = &ue_context_p->ue_context;
    LOG_I(RRC,
          "UE Context Modification Required: new CellGroupConfig for UE ID %d/RNTI %04x, triggering reconfiguration\n",
          UE->rrc_ue_id,
          UE->rnti);

    NR_CellGroupConfig_t *cellGroupConfig = NULL;
    asn_dec_rval_t dec_rval = uper_decode_complete(NULL,
                                                   &asn_DEF_NR_CellGroupConfig,
                                                   (void **)&cellGroupConfig,
                                                   (uint8_t *)required->du_to_cu_rrc_information->cellGroupConfig,
                                                   required->du_to_cu_rrc_information->cellGroupConfig_length);
    if (dec_rval.code != RC_OK && dec_rval.consumed == 0) {
      LOG_E(RRC, "Cell group config decode error, refusing reconfiguration\n");
      f1ap_ue_context_modif_refuse_t refuse = {
        .gNB_CU_ue_id = required->gNB_CU_ue_id,
        .gNB_DU_ue_id = required->gNB_DU_ue_id,
        .cause = F1AP_CAUSE_PROTOCOL,
        .cause_value = F1AP_CauseProtocol_transfer_syntax_error,
      };
      rrc->mac_rrc.ue_context_modification_refuse(msg_p->ittiMsgHeader.originInstance, &refuse);
      return;
    }

    if (UE->masterCellGroup) {
      ASN_STRUCT_FREE(asn_DEF_NR_CellGroupConfig, UE->masterCellGroup);
      LOG_I(RRC, "UE %d/RNTI %04x replacing existing CellGroupConfig with new one received from DU\n", UE->rrc_ue_id, UE->rnti);
    }
    UE->masterCellGroup = cellGroupConfig;
    if (LOG_DEBUGFLAG(DEBUG_ASN1))
      xer_fprint(stdout, &asn_DEF_NR_CellGroupConfig, UE->masterCellGroup);

    /* trigger reconfiguration */
    nr_rrc_reconfiguration_req(rrc, UE, 0, 0);
    return;
  }
  LOG_W(RRC,
        "nothing to be done after UE Context Modification Required for UE ID %d/RNTI %04x\n",
        required->gNB_CU_ue_id,
        required->gNB_DU_ue_id);
}

unsigned int mask_flip(unsigned int x) {
  return((((x>>8) + (x<<8))&0xffff)>>6);
}

static pdusession_level_qos_parameter_t *get_qos_characteristics(const int qfi, rrc_pdu_session_param_t *pduSession)
{
  pdusession_t *pdu = &pduSession->param;
  for (int i = 0; i < pdu->nb_qos; i++) {
    if (qfi == pdu->qos[i].qfi)
      return &pdu->qos[i];
  }
  AssertFatal(1 == 0, "The pdu session %d does not contain a qos flow with qfi = %d\n", pdu->pdusession_id, qfi);
  return NULL;
}

/* \bref return F1AP QoS characteristics based on Qos flow parameters */
static f1ap_qos_characteristics_t get_qos_char_from_qos_flow_param(const pdusession_level_qos_parameter_t *qos_param)
{
  f1ap_qos_characteristics_t qos_char = {0};
  if (qos_param->fiveQI_type == DYNAMIC) {
    qos_char.qos_type = DYNAMIC;
    qos_char.dynamic.fiveqi = qos_param->fiveQI;
    qos_char.dynamic.qos_priority_level = qos_param->qos_priority;
  } else {
    qos_char.qos_type = NON_DYNAMIC;
    qos_char.non_dynamic.fiveqi = qos_param->fiveQI;
    qos_char.non_dynamic.qos_priority_level = qos_param->qos_priority;
  }
  return qos_char;
}

/* \brief fills a list of DRBs to be setup from a number of PDU sessions in E1
 * bearer setup response */
static int fill_drb_to_be_setup_from_e1_resp(const gNB_RRC_INST *rrc,
                                             gNB_RRC_UE_t *UE,
                                             const pdu_session_setup_t *pduSession,
                                             int numPduSession,
                                             f1ap_drb_to_be_setup_t drbs[32])
{
  int nb_drb = 0;
  for (int p = 0; p < numPduSession; ++p) {
    rrc_pdu_session_param_t *RRC_pduSession = find_pduSession(UE, pduSession[p].id, false);
    DevAssert(RRC_pduSession);
    for (int i = 0; i < pduSession[p].numDRBSetup; i++) {
      const DRB_nGRAN_setup_t *drb_config = &pduSession[p].DRBnGRanList[i];
      f1ap_drb_to_be_setup_t *drb = &drbs[nb_drb];
      drb->drb_id = pduSession[p].DRBnGRanList[i].id;
      drb->rlc_mode = rrc->configuration.um_on_default_drb ? F1AP_RLC_MODE_UM_BIDIR : F1AP_RLC_MODE_AM;
      drb->up_ul_tnl[0].tl_address = drb_config->UpParamList[0].tlAddress;
      drb->up_ul_tnl[0].port = rrc->eth_params_s.my_portd;
      drb->up_ul_tnl[0].teid = drb_config->UpParamList[0].teId;
      drb->up_ul_tnl_length = 1;

      /* pass QoS info to MAC */
      int nb_qos_flows = drb_config->numQosFlowSetup;
      AssertFatal(nb_qos_flows > 0, "must map at least one flow to a DRB\n");
      drb->drb_info.flows_to_be_setup_length = nb_qos_flows;
      drb->drb_info.flows_mapped_to_drb = calloc(nb_qos_flows, sizeof(f1ap_flows_mapped_to_drb_t));
      AssertFatal(drb->drb_info.flows_mapped_to_drb, "could not allocate memory\n");
      for (int j = 0; j < nb_qos_flows; j++) {
        drb->drb_info.flows_mapped_to_drb[j].qfi = drb_config->qosFlows[j].qfi;
        pdusession_level_qos_parameter_t *in_qos_char = get_qos_characteristics(drb_config->qosFlows[j].qfi, RRC_pduSession);
        drb->drb_info.flows_mapped_to_drb[j].qos_params.qos_characteristics = get_qos_char_from_qos_flow_param(in_qos_char);
      }
      /* the DRB QoS parameters: we just reuse the ones from the first flow */
      drb->drb_info.drb_qos = drb->drb_info.flows_mapped_to_drb[0].qos_params;

      /* pass NSSAI info to MAC */
      drb->nssai = RRC_pduSession->param.nssai;

      nb_drb++;
    }
  }
  return nb_drb;
}

/**
 * @brief E1AP Bearer Context Setup Response processing on CU-CP
*/
void rrc_gNB_process_e1_bearer_context_setup_resp(e1ap_bearer_setup_resp_t *resp, instance_t instance)
{
  gNB_RRC_INST *rrc = RC.nrrrc[0];
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(rrc, resp->gNB_cu_cp_ue_id);
  AssertFatal(ue_context_p != NULL, "did not find UE with CU UE ID %d\n", resp->gNB_cu_cp_ue_id);
  gNB_RRC_UE_t *UE = &ue_context_p->ue_context;

  // currently: we don't have "infrastructure" to save the CU-UP UE ID, so we
  // assume (and below check) that CU-UP UE ID == CU-CP UE ID
  AssertFatal(resp->gNB_cu_cp_ue_id == resp->gNB_cu_up_ue_id,
              "cannot handle CU-UP UE ID different from CU-CP UE ID (%d vs %d)\n",
              resp->gNB_cu_cp_ue_id,
              resp->gNB_cu_up_ue_id);

  // save the tunnel address for the PDU sessions
  for (int i = 0; i < resp->numPDUSessions; i++) {
    pdu_session_setup_t *e1_pdu = &resp->pduSession[i];
    rrc_pdu_session_param_t *rrc_pdu = find_pduSession(UE, e1_pdu->id, false);
    if (rrc_pdu == NULL) {
      LOG_W(RRC, "E1: received setup for PDU session %ld, but has not been requested\n", e1_pdu->id);
      continue;
    }
    rrc_pdu->param.gNB_teid_N3 = e1_pdu->teId;
    memcpy(&rrc_pdu->param.gNB_addr_N3.buffer, &e1_pdu->tlAddress, sizeof(uint8_t) * 4);
    rrc_pdu->param.gNB_addr_N3.length = sizeof(in_addr_t);

    // save the tunnel address for the DRBs
    for (int i = 0; i < e1_pdu->numDRBSetup; i++) {
      DRB_nGRAN_setup_t *drb_config = &e1_pdu->DRBnGRanList[i];
      // numUpParam only relevant in F1, but not monolithic
      AssertFatal(drb_config->numUpParam <= 1, "can only up to one UP param\n");
      drb_t *drb = get_drb(UE, drb_config->id);
      f1u_ul_gtp_update(&drb->cuup_tunnel_config, &drb_config->UpParamList[0]);
    }
  }

  /* Instruction towards the DU for DRB configuration and tunnel creation */
  f1ap_drb_to_be_setup_t drbs[32]; // maximum DRB can be 32
  int nb_drb = fill_drb_to_be_setup_from_e1_resp(rrc, UE, resp->pduSession, resp->numPDUSessions, drbs);

  if (!UE->f1_ue_context_active)
    rrc_gNB_generate_UeContextSetupRequest(rrc, ue_context_p, nb_drb, drbs);
  else
    rrc_gNB_generate_UeContextModificationRequest(rrc, ue_context_p, nb_drb, drbs, 0, NULL);
}

/**
 * @brief E1AP Bearer Context Modification Response processing on CU-CP
 */
void rrc_gNB_process_e1_bearer_context_modif_resp(const e1ap_bearer_modif_resp_t *resp)
{
  gNB_RRC_INST *rrc = RC.nrrrc[0];
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(rrc, resp->gNB_cu_cp_ue_id);
  if (ue_context_p == NULL) {
    LOG_E(RRC, "no UE with CU-CP UE ID %d found\n", resp->gNB_cu_cp_ue_id);
    return;
  }

  // there is not really anything to do here as of now
  for (int i = 0; i < resp->numPDUSessionsMod; ++i) {
    const pdu_session_modif_t *pdu = &resp->pduSessionMod[i];
    LOG_I(RRC, "UE %d: PDU session ID %ld modified %d bearers\n", resp->gNB_cu_cp_ue_id, pdu->id, pdu->numDRBModified);
  }
}

/**
 * @brief E1AP Bearer Context Release processing
 */
void rrc_gNB_process_e1_bearer_context_release_cplt(const e1ap_bearer_release_cplt_t *cplt)
{
  // there is not really anything to do here as of now
  // note that we don't check for the UE: it does not exist anymore if the F1
  // UE context release complete arrived from the DU first, after which we free
  // the UE context
  LOG_I(RRC, "UE %d: received bearer release complete\n", cplt->gNB_cu_cp_ue_id);
}

static void print_rrc_meas(FILE *f, const NR_MeasResults_t *measresults)
{
  DevAssert(measresults->measResultServingMOList.list.count >= 1);
  if (measresults->measResultServingMOList.list.count > 1)
    LOG_W(RRC, "Received %d MeasResultServMO, but handling only 1!\n", measresults->measResultServingMOList.list.count);

  NR_MeasResultServMO_t *measresultservmo = measresults->measResultServingMOList.list.array[0];
  NR_MeasResultNR_t *measresultnr = &measresultservmo->measResultServingCell;
  NR_MeasQuantityResults_t *mqr = measresultnr->measResult.cellResults.resultsSSB_Cell;

  fprintf(f, "    servingCellId %ld MeasResultNR for phyCellId %ld:\n      resultSSB:", measresultservmo->servCellId, *measresultnr->physCellId);
  if (mqr != NULL) {
    const long rrsrp = *mqr->rsrp - 156;
    const float rrsrq = (float) (*mqr->rsrq - 87) / 2.0f;
    const float rsinr = (float) (*mqr->sinr - 46) / 2.0f;
    fprintf(f, "RSRP %ld dBm RSRQ %.1f dB SINR %.1f dB\n", rrsrp, rrsrq, rsinr);
  } else {
    fprintf(f, "NOT PROVIDED\n");
  }
}

static const char *get_pdusession_status_text(pdu_session_status_t status)
{
  switch (status) {
    case PDU_SESSION_STATUS_NEW: return "new";
    case PDU_SESSION_STATUS_DONE: return "done";
    case PDU_SESSION_STATUS_ESTABLISHED: return "established";
    case PDU_SESSION_STATUS_REESTABLISHED: return "reestablished";
    case PDU_SESSION_STATUS_TOMODIFY: return "to-modify";
    case PDU_SESSION_STATUS_FAILED: return "failed";
    case PDU_SESSION_STATUS_TORELEASE: return "to-release";
    case PDU_SESSION_STATUS_RELEASED: return "released";
    default: AssertFatal(false, "illegal PDU status code %d\n", status); return "illegal";
  }
  return "illegal";
}

static void write_rrc_stats(const gNB_RRC_INST *rrc)
{
  const char *filename = "nrRRC_stats.log";
  FILE *f = fopen(filename, "w");
  if (f == NULL) {
    LOG_E(NR_RRC, "cannot open %s for writing\n", filename);
    return;
  }

  time_t now = time(NULL);
  int i = 0;
  rrc_gNB_ue_context_t *ue_context_p = NULL;
  /* cast is necessary to eliminate warning "discards ‘const’ qualifier" */
  RB_FOREACH(ue_context_p, rrc_nr_ue_tree_s, &((gNB_RRC_INST *)rrc)->rrc_ue_head)
  {
    const gNB_RRC_UE_t *ue_ctxt = &ue_context_p->ue_context;
    f1_ue_data_t ue_data = cu_get_f1_ue_data(ue_ctxt->rrc_ue_id);

    fprintf(f,
            "UE %d CU UE ID %u DU UE ID %d RNTI %04x random identity %016lx",
            i,
            ue_ctxt->rrc_ue_id,
            ue_data.secondary_ue,
            ue_ctxt->rnti,
            ue_ctxt->random_ue_identity);
    if (ue_ctxt->Initialue_identity_5g_s_TMSI.presence)
      fprintf(f, " S-TMSI %x", ue_ctxt->Initialue_identity_5g_s_TMSI.fiveg_tmsi);
    fprintf(f, ":\n");

    time_t last_seen = now - ue_ctxt->last_seen;
    fprintf(f, "    last RRC activity: %ld seconds ago\n", last_seen);

    if (ue_ctxt->nb_of_pdusessions == 0)
      fprintf(f, "    (no PDU sessions)\n");
    for (int nb_pdu = 0; nb_pdu < ue_ctxt->nb_of_pdusessions; ++nb_pdu) {
      const rrc_pdu_session_param_t *pdu = &ue_ctxt->pduSession[nb_pdu];
      fprintf(f, "    PDU session %d ID %d status %s\n", nb_pdu, pdu->param.pdusession_id, get_pdusession_status_text(pdu->status));
    }

    fprintf(f, "    associated DU: ");
    if (ue_data.du_assoc_id == -1)
      fprintf(f, " (local/integrated CU-DU)");
    else if (ue_data.du_assoc_id == 0)
      fprintf(f, " DU offline/unavailable");
    else
      fprintf(f, " DU assoc ID %d", ue_data.du_assoc_id);
    fprintf(f, "\n");

    if (ue_ctxt->measResults)
      print_rrc_meas(f, ue_ctxt->measResults);
    ++i;
  }

  fprintf(f, "\n");
  dump_du_info(rrc, f);

  fclose(f);
}

void *rrc_gnb_task(void *args_p) {
  MessageDef *msg_p;
  instance_t                         instance;
  int                                result;

  long stats_timer_id = 1;
  if (!IS_SOFTMODEM_NOSTATS_BIT) {
    /* timer to write stats to file */
    timer_setup(1, 0, TASK_RRC_GNB, 0, TIMER_PERIODIC, NULL, &stats_timer_id);
  }
  
  itti_mark_task_ready(TASK_RRC_GNB);
  LOG_I(NR_RRC,"Entering main loop of NR_RRC message task\n");

  while (1) {
    // Wait for a message
    itti_receive_msg(TASK_RRC_GNB, &msg_p);
    const char *msg_name_p = ITTI_MSG_NAME(msg_p);
    instance = ITTI_MSG_DESTINATION_INSTANCE(msg_p);
    LOG_D(NR_RRC,
          "RRC GNB Task Received %s for instance %ld from task %s\n",
          ITTI_MSG_NAME(msg_p),
          ITTI_MSG_DESTINATION_INSTANCE(msg_p),
          ITTI_MSG_ORIGIN_NAME(msg_p));
    switch (ITTI_MSG_ID(msg_p)) {
      case TERMINATE_MESSAGE:
        LOG_W(NR_RRC, " *** Exiting NR_RRC thread\n");
        itti_exit_task();
        break;

      case MESSAGE_TEST:
        LOG_I(NR_RRC, "[gNB %ld] Received %s\n", instance, msg_name_p);
        break;

      case TIMER_HAS_EXPIRED:
        /* only this one handled for now */
        DevAssert(TIMER_HAS_EXPIRED(msg_p).timer_id == stats_timer_id);
        write_rrc_stats(RC.nrrrc[0]);
        break;

      case F1AP_INITIAL_UL_RRC_MESSAGE:
        AssertFatal(NODE_IS_CU(RC.nrrrc[instance]->node_type) || NODE_IS_MONOLITHIC(RC.nrrrc[instance]->node_type),
                    "should not receive F1AP_INITIAL_UL_RRC_MESSAGE, need call by CU!\n");
        rrc_gNB_process_initial_ul_rrc_message(msg_p->ittiMsgHeader.originInstance, &F1AP_INITIAL_UL_RRC_MESSAGE(msg_p));
        free_initial_ul_rrc_message_transfer(&F1AP_INITIAL_UL_RRC_MESSAGE(msg_p));
        break;

      /* Messages from PDCP */
      /* From DU -> CU */
      case F1AP_UL_RRC_MESSAGE:
        rrc_gNB_decode_dcch(RC.nrrrc[instance], &F1AP_UL_RRC_MESSAGE(msg_p));
        free(F1AP_UL_RRC_MESSAGE(msg_p).rrc_container);
        break;

      case NGAP_DOWNLINK_NAS:
        rrc_gNB_process_NGAP_DOWNLINK_NAS(msg_p, instance, &rrc_gNB_mui);
        break;

      case NGAP_PDUSESSION_SETUP_REQ:
        rrc_gNB_process_NGAP_PDUSESSION_SETUP_REQ(msg_p, instance);
        break;

      case NGAP_PDUSESSION_MODIFY_REQ:
        rrc_gNB_process_NGAP_PDUSESSION_MODIFY_REQ(msg_p, instance);
        break;

      case NGAP_PDUSESSION_RELEASE_COMMAND:
        rrc_gNB_process_NGAP_PDUSESSION_RELEASE_COMMAND(msg_p, instance);
        break;

      /* Messages from F1AP task */
      case F1AP_SETUP_REQ:
        AssertFatal(!NODE_IS_DU(RC.nrrrc[instance]->node_type), "should not receive F1AP_SETUP_REQUEST in DU!\n");
        rrc_gNB_process_f1_setup_req(&F1AP_SETUP_REQ(msg_p), msg_p->ittiMsgHeader.originInstance);
        break;

      case F1AP_UE_CONTEXT_SETUP_RESP:
        rrc_CU_process_ue_context_setup_response(msg_p, instance);
        break;

      case F1AP_UE_CONTEXT_MODIFICATION_RESP:
        rrc_CU_process_ue_context_modification_response(msg_p, instance);
        break;

      case F1AP_UE_CONTEXT_MODIFICATION_REQUIRED:
        rrc_CU_process_ue_modification_required(msg_p, instance);
        break;

      case F1AP_UE_CONTEXT_RELEASE_REQ:
        rrc_CU_process_ue_context_release_request(msg_p);
        break;

      case F1AP_UE_CONTEXT_RELEASE_COMPLETE:
        rrc_CU_process_ue_context_release_complete(msg_p);
        break;

      case F1AP_LOST_CONNECTION:
        rrc_CU_process_f1_lost_connection(RC.nrrrc[0], &F1AP_LOST_CONNECTION(msg_p), msg_p->ittiMsgHeader.originInstance);
        break;

      case F1AP_GNB_DU_CONFIGURATION_UPDATE:
        AssertFatal(!NODE_IS_DU(RC.nrrrc[instance]->node_type), "should not receive F1AP_SETUP_REQUEST in DU!\n");
        rrc_gNB_process_f1_du_configuration_update(&F1AP_GNB_DU_CONFIGURATION_UPDATE(msg_p), msg_p->ittiMsgHeader.originInstance);
        break;

      case F1AP_GNB_CU_CONFIGURATION_UPDATE_ACKNOWLEDGE:
        AssertFatal(!NODE_IS_DU(RC.nrrrc[instance]->node_type),
                    "should not receive F1AP_GNB_CU_CONFIGURATION_UPDATE_ACKNOWLEDGE in DU!\n");
        LOG_E(NR_RRC, "Handling of F1AP_GNB_CU_CONFIGURATION_UPDATE_ACKNOWLEDGE not implemented\n");
        break;

      /* Messages from X2AP */
      case X2AP_ENDC_SGNB_ADDITION_REQ:
        LOG_I(NR_RRC, "Received ENDC sgNB addition request from X2AP \n");
        rrc_gNB_process_AdditionRequestInformation(instance, &X2AP_ENDC_SGNB_ADDITION_REQ(msg_p));
        break;

      case X2AP_ENDC_SGNB_RECONF_COMPLETE:
        LOG_A(NR_RRC, "Handling of reconfiguration complete message at RRC gNB is pending \n");
        break;

      case NGAP_INITIAL_CONTEXT_SETUP_REQ:
        rrc_gNB_process_NGAP_INITIAL_CONTEXT_SETUP_REQ(msg_p, instance);
        break;

      case X2AP_ENDC_SGNB_RELEASE_REQUEST:
        LOG_I(NR_RRC, "Received ENDC sgNB release request from X2AP \n");
        rrc_gNB_process_release_request(instance, &X2AP_ENDC_SGNB_RELEASE_REQUEST(msg_p));
        break;

      case X2AP_ENDC_DC_OVERALL_TIMEOUT:
        rrc_gNB_process_dc_overall_timeout(instance, &X2AP_ENDC_DC_OVERALL_TIMEOUT(msg_p));
        break;

      case NGAP_UE_CONTEXT_RELEASE_REQ:
        rrc_gNB_process_NGAP_UE_CONTEXT_RELEASE_REQ(msg_p, instance);
        break;

      case NGAP_UE_CONTEXT_RELEASE_COMMAND:
        rrc_gNB_process_NGAP_UE_CONTEXT_RELEASE_COMMAND(msg_p, instance);
        break;

      case E1AP_SETUP_REQ:
        rrc_gNB_process_e1_setup_req(msg_p->ittiMsgHeader.originInstance, &E1AP_SETUP_REQ(msg_p));
        break;

      case E1AP_BEARER_CONTEXT_SETUP_RESP:
        rrc_gNB_process_e1_bearer_context_setup_resp(&E1AP_BEARER_CONTEXT_SETUP_RESP(msg_p), instance);
        break;

      case E1AP_BEARER_CONTEXT_MODIFICATION_RESP:
        rrc_gNB_process_e1_bearer_context_modif_resp(&E1AP_BEARER_CONTEXT_MODIFICATION_RESP(msg_p));
        break;

      case E1AP_BEARER_CONTEXT_RELEASE_CPLT:
        rrc_gNB_process_e1_bearer_context_release_cplt(&E1AP_BEARER_CONTEXT_RELEASE_CPLT(msg_p));
        break;

      case E1AP_LOST_CONNECTION: /* CUCP */
        rrc_gNB_process_e1_lost_connection(RC.nrrrc[0], &E1AP_LOST_CONNECTION(msg_p), msg_p->ittiMsgHeader.originInstance);
        break;

      case NGAP_PAGING_IND:
        rrc_gNB_process_PAGING_IND(msg_p, instance);
        break;

      default:
        LOG_E(NR_RRC, "[gNB %ld] Received unexpected message %s\n", instance, msg_name_p);
        break;
    }

    result = itti_free(ITTI_MSG_ORIGIN_ID(msg_p), msg_p);
    AssertFatal(result == EXIT_SUCCESS, "Failed to free memory (%d)!\n", result);
    msg_p = NULL;
  }
}

//-----------------------------------------------------------------------------
void rrc_gNB_generate_SecurityModeCommand(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue_p)
//-----------------------------------------------------------------------------
{
  uint8_t buffer[100];
  AssertFatal(!ue_p->as_security_active, "logic error: security already active\n");

  T(T_ENB_RRC_SECURITY_MODE_COMMAND, T_INT(0), T_INT(0), T_INT(0), T_INT(ue_p->rrc_ue_id));
  NR_IntegrityProtAlgorithm_t integrity_algorithm = (NR_IntegrityProtAlgorithm_t)ue_p->integrity_algorithm;
  int size = do_NR_SecurityModeCommand(buffer,
                                       rrc_gNB_get_next_transaction_identifier(rrc->module_id),
                                       ue_p->ciphering_algorithm,
                                       integrity_algorithm);
  LOG_DUMPMSG(NR_RRC, DEBUG_RRC, (char *)buffer, size, "[MSG] RRC Security Mode Command\n");
  LOG_I(NR_RRC, "UE %u Logical Channel DL-DCCH, Generate SecurityModeCommand (bytes %d)\n", ue_p->rrc_ue_id, size);

  nr_rrc_transfer_protected_rrc_message(rrc, ue_p, DCCH, buffer, size);
}

typedef struct deliver_ue_ctxt_release_data_t {
  gNB_RRC_INST *rrc;
  f1ap_ue_context_release_cmd_t *release_cmd;
  sctp_assoc_t assoc_id;
} deliver_ue_ctxt_release_data_t;
static void rrc_deliver_ue_ctxt_release_cmd(void *deliver_pdu_data, ue_id_t ue_id, int srb_id, char *buf, int size, int sdu_id)
{
  DevAssert(deliver_pdu_data != NULL);
  deliver_ue_ctxt_release_data_t *data = deliver_pdu_data;
  data->release_cmd->rrc_container = (uint8_t*) buf;
  data->release_cmd->rrc_container_length = size;
  data->rrc->mac_rrc.ue_context_release_command(data->assoc_id, data->release_cmd);
}

//-----------------------------------------------------------------------------
/*
* Generate the RRC Connection Release to UE.
* If received, UE should switch to RRC_IDLE mode.
*/
void rrc_gNB_generate_RRCRelease(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE)
{
  uint8_t buffer[NR_RRC_BUF_SIZE] = {0};
  int size = do_NR_RRCRelease(buffer, NR_RRC_BUF_SIZE, rrc_gNB_get_next_transaction_identifier(rrc->module_id));

  LOG_I(NR_RRC, "UE %d: Generate RRCRelease (bytes %d)\n", UE->rrc_ue_id, size);

  f1_ue_data_t ue_data = cu_get_f1_ue_data(UE->rrc_ue_id);
  RETURN_IF_INVALID_ASSOC_ID(ue_data.du_assoc_id);
  f1ap_ue_context_release_cmd_t ue_context_release_cmd = {
    .gNB_CU_ue_id = UE->rrc_ue_id,
    .gNB_DU_ue_id = ue_data.secondary_ue,
    .cause = F1AP_CAUSE_RADIO_NETWORK,
    .cause_value = 10, // 10 = F1AP_CauseRadioNetwork_normal_release
    .srb_id = DCCH,
  };
  deliver_ue_ctxt_release_data_t data = {.rrc = rrc, .release_cmd = &ue_context_release_cmd, .assoc_id = ue_data.du_assoc_id};
  nr_pdcp_data_req_srb(UE->rrc_ue_id, DCCH, rrc_gNB_mui++, size, buffer, rrc_deliver_ue_ctxt_release_cmd, &data);
}

int rrc_gNB_generate_pcch_msg(sctp_assoc_t assoc_id, const NR_SIB1_t *sib1, uint32_t tmsi, uint8_t paging_drx)
{
  instance_t instance = 0;
  const unsigned int Ttab[4] = {32,64,128,256};
  uint8_t Tc;
  uint8_t Tue;
  uint32_t pfoffset;
  uint32_t N;  /* N: min(T,nB). total count of PF in one DRX cycle */
  uint32_t Ns = 0;  /* Ns: max(1,nB/T) */
  uint32_t T;  /* DRX cycle */
  uint8_t buffer[NR_RRC_BUF_SIZE];

  /* get default DRX cycle from configuration */
  Tc = sib1->servingCellConfigCommon->downlinkConfigCommon.pcch_Config.defaultPagingCycle;

  Tue = paging_drx;
  /* set T = min(Tc,Tue) */
  T = Tc < Tue ? Ttab[Tc] : Ttab[Tue];
  /* set N = PCCH-Config->nAndPagingFrameOffset */
  switch (sib1->servingCellConfigCommon->downlinkConfigCommon.pcch_Config.nAndPagingFrameOffset.present) {
    case NR_PCCH_Config__nAndPagingFrameOffset_PR_oneT:
      N = T;
      pfoffset = 0;
      break;
    case NR_PCCH_Config__nAndPagingFrameOffset_PR_halfT:
      N = T/2;
      pfoffset = 1;
      break;
    case NR_PCCH_Config__nAndPagingFrameOffset_PR_quarterT:
      N = T/4;
      pfoffset = 3;
      break;
    case NR_PCCH_Config__nAndPagingFrameOffset_PR_oneEighthT:
      N = T/8;
      pfoffset = 7;
      break;
    case NR_PCCH_Config__nAndPagingFrameOffset_PR_oneSixteenthT:
      N = T/16;
      pfoffset = 15;
      break;
    default:
      LOG_E(RRC, "[gNB %ld] In rrc_gNB_generate_pcch_msg:  pfoffset error (pfoffset %d)\n",
            instance, sib1->servingCellConfigCommon->downlinkConfigCommon.pcch_Config.nAndPagingFrameOffset.present);
      return (-1);

  }

  switch (sib1->servingCellConfigCommon->downlinkConfigCommon.pcch_Config.ns) {
    case NR_PCCH_Config__ns_four:
      if(*sib1->servingCellConfigCommon->downlinkConfigCommon.initialDownlinkBWP.pdcch_ConfigCommon->choice.setup->pagingSearchSpace == 0){
        LOG_E(RRC, "[gNB %ld] In rrc_gNB_generate_pcch_msg:  ns error only 1 or 2 is allowed when pagingSearchSpace is 0\n",
              instance);
        return (-1);
      } else {
        Ns = 4;
      }
      break;
    case NR_PCCH_Config__ns_two:
      Ns = 2;
      break;
    case NR_PCCH_Config__ns_one:
      Ns = 1;
      break;
    default:
      LOG_E(RRC, "[gNB %ld] In rrc_gNB_generate_pcch_msg: ns error (ns %ld)\n",
            instance, sib1->servingCellConfigCommon->downlinkConfigCommon.pcch_Config.ns);
      return (-1);
  }

  (void) N; /* not used, suppress warning */
  (void) Ns; /* not used, suppress warning */
  (void) pfoffset; /* not used, suppress warning */

  /* Create message for PDCP (DLInformationTransfer_t) */
  int length = do_NR_Paging(instance, buffer, tmsi);

  if (length == -1) {
    LOG_I(NR_RRC, "do_Paging error\n");
    return -1;
  }
  // TODO, send message to pdcp
  (void) assoc_id;

  return 0;
}

/* F1AP UE Context Management Procedures */

//-----------------------------------------------------------------------------
void rrc_gNB_generate_UeContextSetupRequest(const gNB_RRC_INST *rrc,
                                            rrc_gNB_ue_context_t *const ue_context_pP,
                                            int n_drbs,
                                            const f1ap_drb_to_be_setup_t *drbs)
//-----------------------------------------------------------------------------
{
  gNB_RRC_UE_t *ue_p = &ue_context_pP->ue_context;
  AssertFatal(!ue_p->f1_ue_context_active, "logic error: ue context already active\n");

  AssertFatal(!NODE_IS_DU(rrc->node_type), "illegal node type DU!\n");

  cu_to_du_rrc_information_t cu2du = {0};
  cu_to_du_rrc_information_t *cu2du_p = NULL;
  if (ue_p->ue_cap_buffer.len > 0 && ue_p->ue_cap_buffer.buf != NULL) {
    cu2du_p = &cu2du;
    cu2du.uE_CapabilityRAT_ContainerList = ue_p->ue_cap_buffer.buf;
    cu2du.uE_CapabilityRAT_ContainerList_length = ue_p->ue_cap_buffer.len;
  }

  int nb_srb = 1;
  f1ap_srb_to_be_setup_t srbs[1] = {{.srb_id = 2, .lcid = 2}};
  activate_srb(ue_p, 2);

  /* the callback will fill the UE context setup request and forward it */
  f1_ue_data_t ue_data = cu_get_f1_ue_data(ue_p->rrc_ue_id);
  RETURN_IF_INVALID_ASSOC_ID(ue_data.du_assoc_id);
  f1ap_ue_context_setup_t ue_context_setup_req = {
      .gNB_CU_ue_id = ue_p->rrc_ue_id,
      .gNB_DU_ue_id = ue_data.secondary_ue,
      .plmn.mcc = rrc->configuration.mcc[0],
      .plmn.mnc = rrc->configuration.mnc[0],
      .plmn.mnc_digit_length = rrc->configuration.mnc_digit_length[0],
      .nr_cellid = rrc->nr_cellid,
      .servCellId = 0, /* TODO: correct value? */
      .srbs_to_be_setup_length = nb_srb,
      .srbs_to_be_setup = srbs,
      .drbs_to_be_setup_length = n_drbs,
      .drbs_to_be_setup = (f1ap_drb_to_be_setup_t *)drbs,
      .cu_to_du_rrc_information = cu2du_p,
  };

  rrc->mac_rrc.ue_context_setup_request(ue_data.du_assoc_id, &ue_context_setup_req);
  LOG_I(RRC, "UE %d trigger UE context setup request with %d DRBs\n", ue_p->rrc_ue_id, n_drbs);
}

//-----------------------------------------------------------------------------
void rrc_gNB_generate_UeContextModificationRequest(const gNB_RRC_INST *rrc,
                                                   rrc_gNB_ue_context_t *const ue_context_pP,
                                                   int n_drbs,
                                                   const f1ap_drb_to_be_setup_t *drbs,
                                                   int n_rel_drbs,
                                                   const f1ap_drb_to_be_released_t *rel_drbs)
//-----------------------------------------------------------------------------
{
  gNB_RRC_UE_t *ue_p = &ue_context_pP->ue_context;
  AssertFatal(ue_p->f1_ue_context_active, "logic error: calling ue context modification when context not established\n");
  AssertFatal(ue_p->Srb[1].Active && ue_p->Srb[2].Active, "SRBs should already be active\n");

  AssertFatal(!NODE_IS_DU(rrc->node_type), "illegal node type DU!\n");

  cu_to_du_rrc_information_t cu2du = {0};
  cu_to_du_rrc_information_t *cu2du_p = NULL;
  if (ue_p->ue_cap_buffer.len > 0 && ue_p->ue_cap_buffer.buf != NULL) {
    cu2du_p = &cu2du;
    cu2du.uE_CapabilityRAT_ContainerList = ue_p->ue_cap_buffer.buf;
    cu2du.uE_CapabilityRAT_ContainerList_length = ue_p->ue_cap_buffer.len;
  }

  f1_ue_data_t ue_data = cu_get_f1_ue_data(ue_p->rrc_ue_id);
  RETURN_IF_INVALID_ASSOC_ID(ue_data.du_assoc_id);
  f1ap_ue_context_modif_req_t ue_context_modif_req = {
      .gNB_CU_ue_id = ue_p->rrc_ue_id,
      .gNB_DU_ue_id = ue_data.secondary_ue,
      .plmn.mcc = rrc->configuration.mcc[0],
      .plmn.mnc = rrc->configuration.mnc[0],
      .plmn.mnc_digit_length = rrc->configuration.mnc_digit_length[0],
      .nr_cellid = rrc->nr_cellid,
      .servCellId = 0, /* TODO: correct value? */
      .drbs_to_be_setup_length = n_drbs,
      .drbs_to_be_setup = (f1ap_drb_to_be_setup_t *)drbs,
      .drbs_to_be_released_length = n_rel_drbs,
      .drbs_to_be_released = (f1ap_drb_to_be_released_t *)rel_drbs,
      .cu_to_du_rrc_information = cu2du_p,
  };
  rrc->mac_rrc.ue_context_modification_request(ue_data.du_assoc_id, &ue_context_modif_req);
  LOG_I(RRC, "UE %d trigger UE context modification request with %d DRBs\n", ue_p->rrc_ue_id, n_drbs);
}
