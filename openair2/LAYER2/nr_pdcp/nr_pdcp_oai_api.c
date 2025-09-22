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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "nr_pdcp_oai_api.h"
#include <errno.h>
#include <fcntl.h>
#include <openair3/ocp-gtpu/gtp_itf.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "LAYER2/MAC/mac_extern.h"
#include "LTE_DRB-ToAddModList.h"
#include "LTE_DRB-ToReleaseList.h"
#include "LTE_PMCH-InfoList-r9.h"
#include "LTE_SRB-ToAddModList.h"
#include "NR_DRB-ToAddMod.h"
#include "NR_QFI.h"
#include "NR_SDAP-Config.h"
#include "NR_SRB-ToAddMod.h"
#include "SDAP/nr_sdap/nr_sdap_entity.h"
#include "assertions.h"
#include "common/ngran_types.h"
#include "common/platform_constants.h"
#include "common/ran_context.h"
#include "common/utils/T/T.h"
#include "common/utils/tun_if.h"
#include "cuup_cucp_if.h"
#include "executables/lte-softmodem.h"
#include "executables/softmodem-common.h"
#include "f1ap_messages_types.h"
#include "gnb_config.h"
#include "gtpv1_u_messages_types.h"
#include "hashtable.h"
#include "intertask_interface.h"
#include "common/utils/LOG/log.h"
#include "nfapi/oai_integration/vendor_ext.h"
#include "nr_pdcp_asn1_utils.h"
#include "nr_pdcp_timer_thread.h"
#include "nr_pdcp_ue_manager.h"
#include "openair2/F1AP/f1ap_ids.h"
#include "openair2/SDAP/nr_sdap/nr_sdap.h"
#include "pdcp.h"
#include "pdcp_messages_types.h"
#include "openair2/LAYER2/nr_rlc/nr_rlc_oai_api.h"
#include "utils.h"

#define TODO do { \
    printf("%s:%d:%s: todo\n", __FILE__, __LINE__, __FUNCTION__); \
    exit(1); \
  } while (0)

static nr_pdcp_ue_manager_t *nr_pdcp_ue_manager;

/* necessary globals for OAI, not used internally */
hash_table_t  *pdcp_coll_p;
static uint64_t pdcp_optmask;

static ngran_node_t node_type;

nr_pdcp_entity_t *nr_pdcp_get_rb(nr_pdcp_ue_t *ue, int rb_id, bool srb_flag)
{
  nr_pdcp_entity_t *rb;

  if (srb_flag) {
    if (rb_id < 1 || rb_id > 2)
      rb = NULL;
    else
      rb = ue->srb[rb_id - 1];
  } else {
    if (rb_id < 1 || rb_id > MAX_DRBS_PER_UE)
      rb = NULL;
    else
      rb = ue->drb[rb_id - 1];
  }

  return rb;
}

/****************************************************************************/
/* rlc_data_req queue - begin                                               */
/****************************************************************************/


#include <pthread.h>

/* NR PDCP and RLC both use "big locks". In some cases a thread may do
 * lock(rlc) followed by lock(pdcp) (typically when running 'rx_sdu').
 * Another thread may first do lock(pdcp) and then lock(rlc) (typically
 * the GTP module calls 'nr_pdcp_data_req' that, in a previous implementation
 * was indirectly calling 'rlc_data_req' which does lock(rlc)).
 * To avoid the resulting deadlock it is enough to ensure that a call
 * to lock(pdcp) will never be followed by a call to lock(rlc). So,
 * here we chose to have a separate thread that deals with rlc_data_req,
 * out of the PDCP lock. Other solutions may be possible.
 * So instead of calling 'rlc_data_req' directly we have a queue and a
 * separate thread emptying it.
 */

typedef struct {
  protocol_ctxt_t ctxt_pP;
  srb_flag_t      srb_flagP;
  rb_id_t         rb_idP;
  mui_t           muiP;
  confirm_t       confirmP;
  sdu_size_t      sdu_sizeP;
  uint8_t *sdu_pP;
} rlc_data_req_queue_item;

#define RLC_DATA_REQ_QUEUE_SIZE 10000

typedef struct {
  rlc_data_req_queue_item q[RLC_DATA_REQ_QUEUE_SIZE];
  volatile int start;
  volatile int length;
  pthread_mutex_t m;
  pthread_cond_t c;
} rlc_data_req_queue;

static rlc_data_req_queue q;

static void *rlc_data_req_thread(void *_)
{
  int i;

  pthread_setname_np(pthread_self(), "RLC queue");
  while (1) {
    if (pthread_mutex_lock(&q.m) != 0) abort();
    while (q.length == 0)
      if (pthread_cond_wait(&q.c, &q.m) != 0) abort();
    i = q.start;
    if (pthread_mutex_unlock(&q.m) != 0) abort();

    nr_rlc_data_req(&q.q[i].ctxt_pP,
                    q.q[i].srb_flagP,
                    q.q[i].rb_idP,
                    q.q[i].muiP,
                    q.q[i].sdu_sizeP,
                    q.q[i].sdu_pP);

    if (pthread_mutex_lock(&q.m) != 0) abort();

    q.length--;
    q.start = (q.start + 1) % RLC_DATA_REQ_QUEUE_SIZE;

    if (pthread_cond_signal(&q.c) != 0) abort();
    if (pthread_mutex_unlock(&q.m) != 0) abort();
  }
}

static void init_nr_rlc_data_req_queue(void)
{
  pthread_t t;

  pthread_mutex_init(&q.m, NULL);
  pthread_cond_init(&q.c, NULL);

  if (pthread_create(&t, NULL, rlc_data_req_thread, NULL) != 0) {
    LOG_E(PDCP, "%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }
}

static void enqueue_rlc_data_req(const protocol_ctxt_t *const ctxt_pP,
                                 const srb_flag_t srb_flagP,
                                 const rb_id_t rb_idP,
                                 const mui_t muiP,
                                 confirm_t confirmP,
                                 sdu_size_t sdu_sizeP,
                                 uint8_t *sdu_pP)
{
  int i;
  int logged = 0;

  if (pthread_mutex_lock(&q.m) != 0) abort();
  while (q.length == RLC_DATA_REQ_QUEUE_SIZE) {
    if (!logged) {
      logged = 1;
      LOG_W(PDCP, "%s: rlc_data_req queue is full\n", __FUNCTION__);
    }
    if (pthread_cond_wait(&q.c, &q.m) != 0) abort();
  }

  i = (q.start + q.length) % RLC_DATA_REQ_QUEUE_SIZE;
  q.length++;

  q.q[i].ctxt_pP    = *ctxt_pP;
  q.q[i].srb_flagP  = srb_flagP;
  q.q[i].rb_idP     = rb_idP;
  q.q[i].muiP       = muiP;
  q.q[i].confirmP   = confirmP;
  q.q[i].sdu_sizeP  = sdu_sizeP;
  q.q[i].sdu_pP     = sdu_pP;

  if (pthread_cond_signal(&q.c) != 0) abort();
  if (pthread_mutex_unlock(&q.m) != 0) abort();
}

/****************************************************************************/
/* rlc_data_req queue - end                                                 */
/****************************************************************************/

/****************************************************************************/
/* pdcp_data_ind thread - begin                                             */
/****************************************************************************/

typedef struct {
  protocol_ctxt_t ctxt_pP;
  srb_flag_t      srb_flagP;
  rb_id_t         rb_id;
  sdu_size_t      sdu_buffer_size;
  uint8_t *sdu_buffer;
} pdcp_data_ind_queue_item;

#define PDCP_DATA_IND_QUEUE_SIZE 10000

typedef struct {
  pdcp_data_ind_queue_item q[PDCP_DATA_IND_QUEUE_SIZE];
  volatile int start;
  volatile int length;
  pthread_mutex_t m;
  pthread_cond_t c;
} pdcp_data_ind_queue;

static pdcp_data_ind_queue pq;

static void do_pdcp_data_ind(const protocol_ctxt_t *const ctxt_pP,
                             const srb_flag_t srb_flagP,
                             const rb_id_t rb_id,
                             const sdu_size_t sdu_buffer_size,
                             uint8_t *const sdu_buffer)
{
  nr_pdcp_ue_t *ue;
  nr_pdcp_entity_t *rb;
  ue_id_t UEid = ctxt_pP->rntiMaybeUEid;

  if (ctxt_pP->module_id != 0 ||
      //ctxt_pP->enb_flag != 1 ||
      ctxt_pP->instance != 0 ||
      ctxt_pP->eNB_index != 0 ||
      ctxt_pP->brOption != 0) {
    LOG_E(PDCP, "%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  if (ctxt_pP->enb_flag)
    T(T_ENB_PDCP_UL, T_INT(ctxt_pP->module_id), T_INT(UEid), T_INT(rb_id), T_INT(sdu_buffer_size));

  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, UEid);
  rb = nr_pdcp_get_rb(ue, rb_id, srb_flagP);

  if (rb != NULL) {
    rb->recv_pdu(rb, (char *)sdu_buffer, sdu_buffer_size);
  } else {
    LOG_E(PDCP, "pdcp_data_ind: no RB found (rb_id %ld, srb_flag %d)\n", rb_id, srb_flagP);
  }

  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);

  free(sdu_buffer);
}

static void *pdcp_data_ind_thread(void *_)
{
  int i;

  pthread_setname_np(pthread_self(), "PDCP data ind");
  while (1) {
    if (pthread_mutex_lock(&pq.m) != 0) abort();
    while (pq.length == 0)
      if (pthread_cond_wait(&pq.c, &pq.m) != 0) abort();
    i = pq.start;
    if (pthread_mutex_unlock(&pq.m) != 0) abort();

    do_pdcp_data_ind(&pq.q[i].ctxt_pP,
                     pq.q[i].srb_flagP,
                     pq.q[i].rb_id,
                     pq.q[i].sdu_buffer_size,
                     pq.q[i].sdu_buffer);

    if (pthread_mutex_lock(&pq.m) != 0) abort();

    pq.length--;
    pq.start = (pq.start + 1) % PDCP_DATA_IND_QUEUE_SIZE;

    if (pthread_cond_signal(&pq.c) != 0) abort();
    if (pthread_mutex_unlock(&pq.m) != 0) abort();
  }
}

static void init_nr_pdcp_data_ind_queue(void)
{
  pthread_t t;

  pthread_mutex_init(&pq.m, NULL);
  pthread_cond_init(&pq.c, NULL);

  if (pthread_create(&t, NULL, pdcp_data_ind_thread, NULL) != 0) {
    LOG_E(PDCP, "%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }
}

static void enqueue_pdcp_data_ind(const protocol_ctxt_t *const ctxt_pP,
                                  const srb_flag_t srb_flagP,
                                  const rb_id_t rb_id,
                                  const sdu_size_t sdu_buffer_size,
                                  uint8_t *const sdu_buffer)
{
  int i;
  int logged = 0;

  if (pthread_mutex_lock(&pq.m) != 0) abort();
  while (pq.length == PDCP_DATA_IND_QUEUE_SIZE) {
    if (!logged) {
      logged = 1;
      LOG_W(PDCP, "%s: pdcp_data_ind queue is full\n", __FUNCTION__);
    }
    if (pthread_cond_wait(&pq.c, &pq.m) != 0) abort();
  }

  i = (pq.start + pq.length) % PDCP_DATA_IND_QUEUE_SIZE;
  pq.length++;

  pq.q[i].ctxt_pP         = *ctxt_pP;
  pq.q[i].srb_flagP       = srb_flagP;
  pq.q[i].rb_id           = rb_id;
  pq.q[i].sdu_buffer_size = sdu_buffer_size;
  pq.q[i].sdu_buffer      = sdu_buffer;

  if (pthread_cond_signal(&pq.c) != 0) abort();
  if (pthread_mutex_unlock(&pq.m) != 0) abort();
}

bool nr_pdcp_data_ind(const protocol_ctxt_t *const ctxt_pP,
                      const srb_flag_t srb_flagP,
                      const rb_id_t rb_id,
                      const sdu_size_t sdu_buffer_size,
                      uint8_t *const sdu_buffer)
{
  enqueue_pdcp_data_ind(ctxt_pP, srb_flagP, rb_id, sdu_buffer_size, sdu_buffer);
  return true;
}

/****************************************************************************/
/* pdcp_data_ind thread - end                                               */
/****************************************************************************/

int pdcp_fifo_flush_sdus(const protocol_ctxt_t *const ctxt_pP)
{
  return 0;
}

static void set_node_type() {
  node_type = get_node_type();
}

void nr_pdcp_layer_init(void)
{
  /* hack: be sure to initialize only once */
  static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
  static int initialized = 0;
  if (pthread_mutex_lock(&m) != 0) abort();
  if (initialized) {
    if (pthread_mutex_unlock(&m) != 0) abort();
    return;
  }
  initialized = 1;
  if (pthread_mutex_unlock(&m) != 0) abort();

  nr_pdcp_ue_manager = new_nr_pdcp_ue_manager(1);

  set_node_type();

  if ((RC.nrrrc == NULL) || (!NODE_IS_CU(node_type))) {
    init_nr_rlc_data_req_queue();
  }
  nr_pdcp_e1_if_init(node_type == ngran_gNB_CUUP || node_type == ngran_gNB_CUCP);
  init_nr_pdcp_data_ind_queue();
  nr_pdcp_init_timer_thread(nr_pdcp_ue_manager);
}

#include "nfapi/oai_integration/vendor_ext.h"
#include "executables/lte-softmodem.h"
#include "common/utils/tun_if.h"
#include "openair2/SDAP/nr_sdap/nr_sdap.h"

static void deliver_sdu_drb(void *_ue, nr_pdcp_entity_t *entity,
                            char *buf, int size,
                            const nr_pdcp_integrity_data_t *msg_integrity)
{
  nr_pdcp_ue_t *ue = _ue;
  int rb_id;
  int i;

  if (IS_SOFTMODEM_NOS1 || UE_NAS_USE_TUN) {
    LOG_D(PDCP, "IP packet received with size %d, to be sent to SDAP interface, UE ID/RNTI: %ld\n", size, ue->ue_id);
    // in NoS1 mode: the SDAP should write() packets to an FD (TUN interface),
    // so below, set is_gnb == 0 to do that
    sdap_data_ind(entity->rb_id, 0, entity->has_sdap_rx, entity->pdusession_id, ue->ue_id, buf, size);
  }
  else{
    for (i = 0; i < MAX_DRBS_PER_UE; i++) {
        if (entity == ue->drb[i]) {
          rb_id = i+1;
          goto rb_found;
        }
      }

      LOG_E(PDCP, "%s:%d:%s: fatal, no RB found for UE ID/RNTI %ld\n", __FILE__, __LINE__, __FUNCTION__, ue->ue_id);
      exit(1);

    rb_found:
    {
      LOG_D(PDCP, "%s() (drb %d) sending message to SDAP size %d\n", __func__, rb_id, size);
      sdap_data_ind(rb_id,
                    ue->drb[rb_id - 1]->is_gnb,
                    ue->drb[rb_id - 1]->has_sdap_rx,
                    ue->drb[rb_id - 1]->pdusession_id,
                    ue->ue_id,
                    buf,
                    size);
    }
  }
}

static void deliver_pdu_drb_ue(void *deliver_pdu_data, ue_id_t ue_id, int rb_id,
                               char *buf, int size, int sdu_id)
{
  DevAssert(deliver_pdu_data == NULL);
  protocol_ctxt_t ctxt = { .enb_flag = 0, .rntiMaybeUEid = ue_id };

  uint8_t *memblock = malloc16(size);
  memcpy(memblock, buf, size);
  LOG_D(PDCP, "%s(): (drb %d) calling rlc_data_req size %d UE %ld/%04lx\n", __func__, rb_id, size, ctxt.rntiMaybeUEid, ctxt.rntiMaybeUEid);
  enqueue_rlc_data_req(&ctxt, 0, rb_id, sdu_id, 0, size, memblock);
}

static void deliver_pdu_drb_gnb(void *deliver_pdu_data, ue_id_t ue_id, int rb_id,
                                char *buf, int size, int sdu_id)
{
  DevAssert(deliver_pdu_data == NULL);
  f1_ue_data_t ue_data = cu_get_f1_ue_data(ue_id);
  protocol_ctxt_t ctxt = { .enb_flag = 1, .rntiMaybeUEid = ue_data.secondary_ue };

  if (NODE_IS_CU(node_type)) {
    LOG_D(PDCP, "%s() (drb %d) sending message to gtp size %d\n", __func__, rb_id, size);
    extern instance_t CUuniqInstance;
    gtpv1uSendDirectWithNRUSeqNum(CUuniqInstance, ue_id, rb_id, (uint8_t *)buf, size);
  } else {
    uint8_t *memblock = malloc16(size);
    memcpy(memblock, buf, size);
    LOG_D(PDCP, "%s(): (drb %d) calling rlc_data_req size %d\n", __func__, rb_id, size);
    enqueue_rlc_data_req(&ctxt, 0, rb_id, sdu_id, 0, size, memblock);
  }
}

static void deliver_sdu_srb(void *_ue, nr_pdcp_entity_t *entity,
                            char *buf, int size,
                            const nr_pdcp_integrity_data_t *msg_integrity)
{
  nr_pdcp_ue_t *ue = _ue;
  int srb_id;
  int i;

  for (i = 0; i < sizeofArray(ue->srb) ; i++) {
    if (entity == ue->srb[i]) {
      srb_id = i+1;
      goto srb_found;
    }
  }

  LOG_E(PDCP, "%s:%d:%s: fatal, no SRB found for UE ID/RNTI %ld\n", __FILE__, __LINE__, __FUNCTION__, ue->ue_id);
  exit(1);

srb_found:
  if (entity->is_gnb) {
    MessageDef *message_p = itti_alloc_new_message(TASK_PDCP_GNB, 0, F1AP_UL_RRC_MESSAGE);
    AssertFatal(message_p != NULL, "OUT OF MEMORY\n");
    f1ap_ul_rrc_message_t *ul_rrc = &F1AP_UL_RRC_MESSAGE(message_p);
    ul_rrc->gNB_CU_ue_id = ue->ue_id;
    /* look up the correct secondary UE ID to provide complete information to
     * RRC, the RLC-PDCP interface does not transport this information */
    f1_ue_data_t ue_data = cu_get_f1_ue_data(ue->ue_id);
    ul_rrc->gNB_DU_ue_id = ue_data.secondary_ue;
    ul_rrc->srb_id = srb_id;
    ul_rrc->rrc_container = malloc(size);
    AssertFatal(ul_rrc->rrc_container != NULL, "OUT OF MEMORY\n");
    memcpy(ul_rrc->rrc_container, buf, size);
    ul_rrc->rrc_container_length = size;
    itti_send_msg_to_task(TASK_RRC_GNB, 0, message_p);
  } else {
    uint8_t *rrc_buffer_p = itti_malloc(TASK_PDCP_UE, TASK_RRC_NRUE, size);
    AssertFatal(rrc_buffer_p != NULL, "OUT OF MEMORY\n");
    memcpy(rrc_buffer_p, buf, size);
    MessageDef *message_p = itti_alloc_new_message(TASK_PDCP_UE, 0, NR_RRC_DCCH_DATA_IND);
    AssertFatal(message_p != NULL, "OUT OF MEMORY\n");
    NR_RRC_DCCH_DATA_IND(message_p).dcch_index = srb_id;
    NR_RRC_DCCH_DATA_IND(message_p).sdu_p = rrc_buffer_p;
    NR_RRC_DCCH_DATA_IND(message_p).sdu_size = size;
    memcpy(&NR_RRC_DCCH_DATA_IND(message_p).msg_integrity, msg_integrity, sizeof(*msg_integrity));
    ue_id_t ue_id = ue->ue_id;
    itti_send_msg_to_task(TASK_RRC_NRUE, ue_id, message_p);
  }
}

void deliver_pdu_srb_rlc(void *deliver_pdu_data, ue_id_t ue_id, int srb_id,
                         char *buf, int size, int sdu_id)
{
  protocol_ctxt_t ctxt = { .enb_flag = 1, .rntiMaybeUEid = ue_id };
  uint8_t *memblock = malloc16(size);
  memcpy(memblock, buf, size);
  enqueue_rlc_data_req(&ctxt, 1, srb_id, sdu_id, 0, size, memblock);
}

void add_srb(int is_gnb,
             ue_id_t UEid,
             struct NR_SRB_ToAddMod *s,
             const nr_pdcp_entity_security_keys_and_algos_t *security_parameters)
{
  nr_pdcp_entity_t *pdcp_srb;
  nr_pdcp_ue_t *ue;

  int srb_id = s->srb_Identity;
  int t_Reordering = -1; // infinity as per default SRB configuration in 9.2.1 of 38.331
  if (s->pdcp_Config != NULL && s->pdcp_Config->t_Reordering != NULL)
    t_Reordering = decode_t_reordering(*s->pdcp_Config->t_Reordering);

  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, UEid);
  if (nr_pdcp_get_rb(ue, srb_id, true) != NULL) {
    LOG_E(PDCP, "warning SRB %d already exist for UE ID %ld, do nothing\n", srb_id, UEid);
  } else {
    pdcp_srb = new_nr_pdcp_entity(NR_PDCP_SRB,
                                  is_gnb,
                                  srb_id,
                                  0,      // PDU session ID (not relevant)
                                  false,  // has SDAP RX (not relevant)
                                  false,  // has SDAP TX (not relevant)
                                  deliver_sdu_srb,
                                  ue,
                                  NULL,
                                  ue,
                                  SHORT_SN_SIZE,
                                  t_Reordering,
                                  -1,
                                  security_parameters);
    nr_pdcp_ue_add_srb_pdcp_entity(ue, srb_id, pdcp_srb);

    LOG_D(PDCP, "added srb %d to UE ID %ld\n", srb_id, UEid);
  }
  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

void add_drb(int is_gnb,
             ue_id_t UEid,
             struct NR_DRB_ToAddMod *s,
             const nr_pdcp_entity_security_keys_and_algos_t *security_parameters)
{
  nr_pdcp_entity_t *pdcp_drb;
  nr_pdcp_ue_t *ue;

  int drb_id = s->drb_Identity;
  if (!s->pdcp_Config || !s->pdcp_Config->drb) {
    LOG_E(PDCP, "DRB field mandatory present at setup. DRB %d not configured.\n", drb_id);
    return;
  }

  struct NR_PDCP_Config__drb *drb = s->pdcp_Config->drb;
  int sn_size_ul = decode_sn_size_ul(*drb->pdcp_SN_SizeUL);
  int sn_size_dl = decode_sn_size_dl(*drb->pdcp_SN_SizeDL);
  int discard_timer = decode_discard_timer(*drb->discardTimer);

  int has_integrity;
  int has_ciphering;

  /* if pdcp_Config->t_Reordering is not present, it means infinity (-1) */
  int t_reordering = -1;
  if (s->pdcp_Config->t_Reordering != NULL) {
    t_reordering = decode_t_reordering(*s->pdcp_Config->t_Reordering);
  }

  if (drb->integrityProtection != NULL)
    has_integrity = 1;
  else
    has_integrity = 0;

  if (s->pdcp_Config->ext1 != NULL && s->pdcp_Config->ext1->cipheringDisabled != NULL)
    has_ciphering = 0;
  else
    has_ciphering = 1;

  if ((!s->cnAssociation) || s->cnAssociation->present == NR_DRB_ToAddMod__cnAssociation_PR_NOTHING) {
    LOG_E(PDCP, "fatal, cnAssociation is missing or present is NR_DRB_ToAddMod__cnAssociation_PR_NOTHING\n");
    exit(-1);
  }

  int pdusession_id;
  bool has_sdap_rx = false;
  bool has_sdap_tx = false;
  bool is_sdap_DefaultDRB = false;
  NR_QFI_t *mappedQFIs2Add = NULL;
  uint8_t mappedQFIs2AddCount=0;
  if (s->cnAssociation->present == NR_DRB_ToAddMod__cnAssociation_PR_eps_BearerIdentity)
     pdusession_id = s->cnAssociation->choice.eps_BearerIdentity;
  else {
    if (!s->cnAssociation->choice.sdap_Config) {
      LOG_E(PDCP,"fatal, sdap_Config is null");
      exit(-1);
    }
    pdusession_id = s->cnAssociation->choice.sdap_Config->pdu_Session;
    has_sdap_rx = is_sdap_rx(is_gnb, s->cnAssociation->choice.sdap_Config);
    has_sdap_tx = is_sdap_tx(is_gnb, s->cnAssociation->choice.sdap_Config);
    is_sdap_DefaultDRB = s->cnAssociation->choice.sdap_Config->defaultDRB == true ? 1 : 0;
    mappedQFIs2Add = (NR_QFI_t*)s->cnAssociation->choice.sdap_Config->mappedQoS_FlowsToAdd->list.array[0]; 
    mappedQFIs2AddCount = s->cnAssociation->choice.sdap_Config->mappedQoS_FlowsToAdd->list.count;
    LOG_D(SDAP, "Captured mappedQoS_FlowsToAdd from RRC: %ld \n", *mappedQFIs2Add);
  }
  /* TODO(?): accept different UL and DL SN sizes? */
  if (sn_size_ul != sn_size_dl) {
    LOG_E(PDCP, "fatal, bad SN sizes, must be same. ul=%d, dl=%d\n", sn_size_ul, sn_size_dl);
    exit(1);
  }

  /* get actual ciphering and integrity algorithm based on pdcp_Config */
  nr_pdcp_entity_security_keys_and_algos_t actual_security_parameters = *security_parameters;
  actual_security_parameters.ciphering_algorithm = has_ciphering ? security_parameters->ciphering_algorithm : 0;
  actual_security_parameters.integrity_algorithm = has_integrity ? security_parameters->integrity_algorithm : 0;

  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, UEid);
  if (nr_pdcp_get_rb(ue, drb_id, false) != NULL) {
    LOG_W(PDCP, "warning DRB %d already exist for UE ID %ld, do nothing\n", drb_id, UEid);
  } else {
    pdcp_drb = new_nr_pdcp_entity(NR_PDCP_DRB_AM, is_gnb, drb_id, pdusession_id,
                                  has_sdap_rx, has_sdap_tx, deliver_sdu_drb, ue,
                                  is_gnb ?
                                    deliver_pdu_drb_gnb : deliver_pdu_drb_ue,
                                  ue,
                                  sn_size_dl, t_reordering, discard_timer,
                                  &actual_security_parameters);
    nr_pdcp_ue_add_drb_pdcp_entity(ue, drb_id, pdcp_drb);

    LOG_I(PDCP, "added drb %d to UE ID %ld\n", drb_id, UEid);
    /* add new SDAP entity for the PDU session the DRB belongs to */
    new_nr_sdap_entity(is_gnb,
                       has_sdap_rx,
                       has_sdap_tx,
                       UEid,
                       pdusession_id,
                       is_sdap_DefaultDRB,
                       drb_id,
                       mappedQFIs2Add,
                       mappedQFIs2AddCount);
  }
  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

void nr_pdcp_add_srbs(eNB_flag_t enb_flag,
                      ue_id_t UEid,
                      NR_SRB_ToAddModList_t *const srb2add_list,
                      const nr_pdcp_entity_security_keys_and_algos_t *security_parameters)
{
  if (srb2add_list != NULL) {
    for (int i = 0; i < srb2add_list->list.count; i++) {
      add_srb(enb_flag, UEid, srb2add_list->list.array[i], security_parameters);
    }
  } else
    LOG_W(PDCP, "nr_pdcp_add_srbs() with void list\n");
}

void nr_pdcp_add_drbs(eNB_flag_t enb_flag,
                      ue_id_t UEid,
                      NR_DRB_ToAddModList_t *const drb2add_list,
                      const nr_pdcp_entity_security_keys_and_algos_t *security_parameters)
{
  if (drb2add_list != NULL) {
    for (int i = 0; i < drb2add_list->list.count; i++) {
      add_drb(enb_flag,
              UEid,
              drb2add_list->list.array[i],
              security_parameters);
    }
  } else
    LOG_W(PDCP, "nr_pdcp_add_drbs() with void list\n");
}

uint64_t get_pdcp_optmask(void)
{
  return pdcp_optmask;
}

void nr_pdcp_remove_UE(ue_id_t ue_id)
{
  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  nr_pdcp_manager_remove_ue(nr_pdcp_ue_manager, ue_id);
  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

void nr_pdcp_config_set_security(ue_id_t ue_id,
                                 rb_id_t rb_id,
                                 bool is_srb,
                                 const nr_pdcp_entity_security_keys_and_algos_t *parameters)
{
  nr_pdcp_ue_t *ue;
  nr_pdcp_entity_t *rb;

  nr_pdcp_manager_lock(nr_pdcp_ue_manager);

  ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);

  rb = nr_pdcp_get_rb(ue, rb_id, is_srb);

  if (rb == NULL) {
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    LOG_E(PDCP, "no %s found (ue_id %ld, rb_id %ld)\n", is_srb ? "SRB" : "DRB", ue_id, rb_id);
    return;
  }

  rb->set_security(rb, parameters);

  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

bool nr_pdcp_check_integrity_srb(ue_id_t ue_id,
                                 int srb_id,
                                 const uint8_t *msg,
                                 int msg_size,
                                 const nr_pdcp_integrity_data_t *msg_integrity)
{
  nr_pdcp_ue_t *ue;
  nr_pdcp_entity_t *rb;

  nr_pdcp_manager_lock(nr_pdcp_ue_manager);

  ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);

  rb = nr_pdcp_get_rb(ue, srb_id, true);

  if (rb == NULL) {
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    LOG_E(PDCP, "no SRB found (ue_id %ld, rb_id %d)\n", ue_id, srb_id);
    return false;
  }

  bool ret = rb->check_integrity(rb, msg, msg_size, msg_integrity);

  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);

  return ret;
}

bool nr_pdcp_data_req_srb(ue_id_t ue_id,
                          const rb_id_t rb_id,
                          const mui_t muiP,
                          const sdu_size_t sdu_buffer_size,
                          unsigned char *const sdu_buffer,
                          deliver_pdu deliver_pdu_cb,
                          void *data)
{
  LOG_D(PDCP, "%s() called, size %d\n", __func__, sdu_buffer_size);
  nr_pdcp_ue_t *ue;
  nr_pdcp_entity_t *rb;

  nr_pdcp_manager_lock(nr_pdcp_ue_manager);

  ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);
  rb = nr_pdcp_get_rb(ue, rb_id, true);

  if (rb == NULL) {
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    LOG_E(PDCP, "no SRB found (ue_id %ld, rb_id %ld)\n", ue_id, rb_id);
    return 0;
  }

  int max_size = nr_max_pdcp_pdu_size(sdu_buffer_size);
  char pdu_buf[max_size];
  int pdu_size = rb->process_sdu(rb, (char *)sdu_buffer, sdu_buffer_size, muiP, pdu_buf, max_size);
  if (pdu_size == -1) {
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    return 0;
  }
  AssertFatal(rb->deliver_pdu == NULL, "SRB callback should be NULL, to be provided on every invocation\n");

  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);

  deliver_pdu_cb(data, ue_id, rb_id, pdu_buf, pdu_size, muiP);

  return 1;
}

void nr_pdcp_suspend_srb(ue_id_t ue_id, int srb_id)
{
  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  nr_pdcp_ue_t *ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);
  nr_pdcp_entity_t *srb = nr_pdcp_get_rb(ue, srb_id, true);
  if (srb == NULL) {
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    LOG_E(PDCP, "Trying to suspend SRB with ID %d but it is not established\n", srb_id);
    return;
  }
  srb->suspend_entity(srb);
  LOG_D(PDCP, "SRB %d suspended\n", srb_id);
  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

void nr_pdcp_suspend_drb(ue_id_t ue_id, int drb_id)
{
  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  nr_pdcp_ue_t *ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);
  nr_pdcp_entity_t *drb = nr_pdcp_get_rb(ue, drb_id, false);
  if (drb == NULL) {
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    LOG_E(PDCP, "Trying to suspend DRB with ID %d but it is not established\n", drb_id);
    return;
  }
  drb->suspend_entity(drb);
  LOG_D(PDCP, "DRB %d suspended\n", drb_id);
  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

void nr_pdcp_reconfigure_srb(ue_id_t ue_id, int srb_id, long t_Reordering)
{
  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  nr_pdcp_ue_t *ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);
  nr_pdcp_entity_t *srb = nr_pdcp_get_rb(ue, srb_id, true);
  if (srb == NULL) {
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    LOG_E(PDCP, "Trying to reconfigure SRB with ID %d but it is not established\n", srb_id);
    return;
  }
  int decoded_t_reordering = decode_t_reordering(t_Reordering);
  srb->t_reordering = decoded_t_reordering;
  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

void nr_pdcp_reconfigure_drb(ue_id_t ue_id, int drb_id, NR_PDCP_Config_t *pdcp_config)
{
  // The enabling/disabling of ciphering or integrity protection
  // can be changed only by releasing and adding the DRB
  // (so not by reconfiguring).
  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  nr_pdcp_ue_t *ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);
  nr_pdcp_entity_t *drb = nr_pdcp_get_rb(ue, drb_id, false);
  if (drb == NULL) {
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    LOG_E(PDCP, "Trying to reconfigure DRB with ID %d but it is not established\n", drb_id);
    return;
  }
  if (pdcp_config) {
    if (pdcp_config->t_Reordering)
      drb->t_reordering = decode_t_reordering(*pdcp_config->t_Reordering);
    else
      drb->t_reordering = -1;
    struct NR_PDCP_Config__drb *drb_config = pdcp_config->drb;
    if (drb_config) {
      if (drb_config->discardTimer)
        drb->discard_timer = decode_discard_timer(*drb_config->discardTimer);
      bool size_set = false;
      if (drb_config->pdcp_SN_SizeUL) {
        drb->sn_size = decode_sn_size_ul(*drb_config->pdcp_SN_SizeUL);
        size_set = true;
      }
      if (drb_config->pdcp_SN_SizeDL) {
        int size = decode_sn_size_dl(*drb_config->pdcp_SN_SizeDL);
        AssertFatal(!size_set || (size == drb->sn_size),
                    "SN sizes must be the same. dl=%d, ul=%d",
                    size, drb->sn_size);
        drb->sn_size = size;
      }
    }
  }
  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

void nr_pdcp_release_srb(ue_id_t ue_id, int srb_id)
{
  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  nr_pdcp_ue_t *ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);
  if (ue->srb[srb_id - 1] != NULL) {
    ue->srb[srb_id - 1]->delete_entity(ue->srb[srb_id - 1]);
    ue->srb[srb_id - 1] = NULL;
  }
  else
    LOG_E(PDCP, "Attempting to release SRB%d but it is not configured\n", srb_id);
  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

void nr_pdcp_release_drb(ue_id_t ue_id, int drb_id)
{
  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  nr_pdcp_ue_t *ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);
  nr_pdcp_entity_t *drb = ue->drb[drb_id - 1];
  if (drb) {
    nr_sdap_release_drb(ue_id, drb_id, drb->pdusession_id);
    drb->release_entity(drb);
    drb->delete_entity(drb);
    ue->drb[drb_id - 1] = NULL;
    LOG_I(PDCP, "release DRB %d of UE %ld\n", drb_id, ue_id);
  }
  else
    LOG_E(PDCP, "Attempting to release DRB%d but it is not configured\n", drb_id);
  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

void nr_pdcp_reestablishment(ue_id_t ue_id,
                             int rb_id,
                             bool srb_flag,
                             const nr_pdcp_entity_security_keys_and_algos_t *security_parameters)
{
  nr_pdcp_ue_t     *ue;
  nr_pdcp_entity_t *rb;

  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);
  rb = nr_pdcp_get_rb(ue, rb_id, srb_flag);

  if (rb != NULL) {
    LOG_D(PDCP, "UE %4.4lx re-establishment of %sRB %d\n", ue_id, srb_flag ? "S" : "D", rb_id);
    rb->reestablish_entity(rb, security_parameters);
    LOG_I(PDCP, "%s %d re-established\n", srb_flag ? "SRB" : "DRB" , rb_id);
  } else {
    LOG_W(PDCP, "UE %4.4lx cannot re-establish %sRB %d, RB not found\n", ue_id, srb_flag ? "S" : "D", rb_id);
  }

  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

bool nr_pdcp_data_req_drb(protocol_ctxt_t *ctxt_pP,
                          const srb_flag_t srb_flagP,
                          const rb_id_t rb_id,
                          const mui_t muiP,
                          const confirm_t confirmP,
                          const sdu_size_t sdu_buffer_size,
                          unsigned char *const sdu_buffer,
                          const pdcp_transmission_mode_t mode,
                          const uint32_t *const sourceL2Id,
                          const uint32_t *const destinationL2Id)
{
  DevAssert(srb_flagP == SRB_FLAG_NO);

  LOG_D(PDCP, "%s() called, size %d\n", __func__, sdu_buffer_size);
  nr_pdcp_ue_t *ue;
  nr_pdcp_entity_t *rb;
  ue_id_t ue_id = ctxt_pP->rntiMaybeUEid;

  if (ctxt_pP->module_id != 0 ||
      //ctxt_pP->enb_flag != 1 ||
      ctxt_pP->instance != 0 ||
      ctxt_pP->eNB_index != 0 /*||
      ctxt_pP->configured != 1 ||
      ctxt_pP->brOption != 0*/) {
    LOG_E(PDCP, "%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  nr_pdcp_manager_lock(nr_pdcp_ue_manager);

  ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);
  rb = nr_pdcp_get_rb(ue, rb_id, false);

  if (rb == NULL) {
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    LOG_E(PDCP, "[UE %lx] DRB %ld not found\n", ue_id, rb_id);
    return 0;
  }

  int max_size = nr_max_pdcp_pdu_size(sdu_buffer_size);
  char pdu_buf[max_size];
  int pdu_size = rb->process_sdu(rb, (char *)sdu_buffer, sdu_buffer_size, muiP, pdu_buf, max_size);
  if (pdu_size == -1) {
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    return 0;
  }

  deliver_pdu deliver_pdu_cb = rb->deliver_pdu;

  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);

  deliver_pdu_cb(NULL, ue_id, rb_id, pdu_buf, pdu_size, muiP);

  return 1;
}

bool cu_f1u_data_req(protocol_ctxt_t  *ctxt_pP,
                     const srb_flag_t srb_flagP,
                     const rb_id_t rb_id,
                     const mui_t muiP,
                     const confirm_t confirmP,
                     const sdu_size_t sdu_buffer_size,
                     unsigned char *const sdu_buffer,
                     const pdcp_transmission_mode_t mode,
                     const uint32_t *const sourceL2Id,
                     const uint32_t *const destinationL2Id) {
  //Force instance id to 0, OAI incoherent instance management
  ctxt_pP->instance=0;
  uint8_t *memblock = malloc16(sdu_buffer_size);
  if (memblock == NULL) {
    LOG_E(RLC, "%s:%d:%s: ERROR: malloc16 failed\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }
  memcpy(memblock, sdu_buffer, sdu_buffer_size);
  int ret = nr_pdcp_data_ind(ctxt_pP, srb_flagP, rb_id, sdu_buffer_size, memblock);
  if (!ret) {
    LOG_E(RLC, "%s:%d:%s: ERROR: pdcp_data_ind failed\n", __FILE__, __LINE__, __FUNCTION__);
    /* what to do in case of failure? for the moment: nothing */
  }
  return ret;
}

/*
 * For the SDAP API
 */
nr_pdcp_ue_manager_t *nr_pdcp_sdap_get_ue_manager() {
  return nr_pdcp_ue_manager;
}

/* returns false in case of error, true if everything ok */
bool nr_pdcp_get_statistics(ue_id_t ue_id, int srb_flag, int rb_id, nr_pdcp_statistics_t *out)
{
  nr_pdcp_ue_t     *ue;
  nr_pdcp_entity_t *rb;
  bool             ret;

  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);
  rb = nr_pdcp_get_rb(ue, rb_id, srb_flag);

  if (rb != NULL) {
    rb->get_stats(rb, out);
    ret = true;
  } else {
    ret = false;
  }

  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);

  return ret;
}

int nr_pdcp_get_num_ues(ue_id_t *ue_list, int len)
{
  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  int num_ues = nr_pdcp_manager_get_ue_count(nr_pdcp_ue_manager);
  nr_pdcp_ue_t **nr_pdcp_ue_list = nr_pdcp_manager_get_ue_list(nr_pdcp_ue_manager);
  for (int i = 0; i < num_ues && i < len; i++)
    ue_list[i] = nr_pdcp_ue_list[i]->ue_id;
  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
  
  return num_ues;
}

void nr_pdcp_count_update(ue_id_t ue_id,
                          rb_id_t drb_id,
                          nr_pdcp_count_t dl_count,
                          nr_pdcp_count_t ul_count,
                          int sn_size)
{
  nr_pdcp_manager_lock(nr_pdcp_ue_manager);

  nr_pdcp_ue_t *ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);
  if (ue == NULL) {
    LOG_E(PDCP, "UE %ld not found in PDCP manager\n", ue_id);
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    return;
  }

  nr_pdcp_entity_t *entity = nr_pdcp_get_rb(ue, drb_id, false);
  if (entity == NULL) {
    LOG_E(PDCP, "No PDCP entity found for UE %ld DRB %ld\n", ue_id, drb_id);
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    return;
  }

  entity->set_pdcp_count_dl(entity, dl_count, sn_size);
  entity->set_pdcp_count_ul(entity, ul_count, sn_size);

  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

/** * @brief Get PDCP COUNT values (PDCP-SN and HFN) for UL and DL from the PDCP entity
 * @param[in]  ue_id          Unique UE ID
 * @param[in]  rb_id          Radio Bearer ID
 * @param[out] ul_count       UL PDCP count (SN, HFN)
 * @param[out] dl_count       DL PDCP count (SN, HFN)
 */
void nr_pdcp_get_drb_count_values(ue_id_t ue_id, rb_id_t rb_id, nr_pdcp_count_t *ul_count, nr_pdcp_count_t *dl_count)
{
  nr_pdcp_manager_lock(nr_pdcp_ue_manager);

  nr_pdcp_ue_t *ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);

  nr_pdcp_entity_t *entity = nr_pdcp_get_rb(ue, rb_id, false);

  if (!entity) {
    LOG_E(PDCP, "No entity found (DRB %ld)\n", rb_id);
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    return;
  }

  *dl_count = entity->get_pdcp_count_dl(entity);
  *ul_count = entity->get_pdcp_count_ul(entity);

  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}
