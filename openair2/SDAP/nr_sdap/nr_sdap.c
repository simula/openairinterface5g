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

#include "nr_sdap.h"
#include "assertions.h"
#include "utils.h"
#include <inttypes.h>
#include <pthread.h>
#include <stddef.h>
#include "nr_sdap_entity.h"
#include "common/utils/LOG/log.h"
#include "errno.h"
#include "rlc.h"
#include "tun_if.h"
#include "system.h"

static void reblock_tun_socket(int fd)
{
  int f;

  f = fcntl(fd, F_GETFL, 0);
  f &= ~(O_NONBLOCK);
  if (fcntl(fd, F_SETFL, f) == -1) {
    LOG_E(PDCP, "fcntl(F_SETFL) failed on fd %d: errno %d, %s\n", fd, errno, strerror(errno));
  }
}


bool sdap_data_req(protocol_ctxt_t *ctxt_p,
                   const ue_id_t ue_id,
                   const srb_flag_t srb_flag,
                   const rb_id_t rb_id,
                   const mui_t mui,
                   const confirm_t confirm,
                   const sdu_size_t sdu_buffer_size,
                   unsigned char *const sdu_buffer,
                   const pdcp_transmission_mode_t pt_mode,
                   const uint32_t *sourceL2Id,
                   const uint32_t *destinationL2Id,
                   const uint8_t qfi,
                   const bool rqi,
                   const int pdusession_id) {
  nr_sdap_entity_t *sdap_entity;
  sdap_entity = nr_sdap_get_entity(ue_id, pdusession_id);

  if(sdap_entity == NULL) {
    LOG_E(SDAP, "%s:%d:%s: Entity not found with ue: 0x%"PRIx64" and pdusession id: %d\n", __FILE__, __LINE__, __FUNCTION__, ue_id, pdusession_id);
    return 0;
  }

  bool ret = sdap_entity->tx_entity(sdap_entity,
                                    ctxt_p,
                                    srb_flag,
                                    rb_id,
                                    mui,
                                    confirm,
                                    sdu_buffer_size,
                                    sdu_buffer,
                                    pt_mode,
                                    sourceL2Id,
                                    destinationL2Id,
                                    qfi,
                                    rqi);
  return ret;
}

void sdap_data_ind(rb_id_t pdcp_entity,
                   int is_gnb,
                   bool has_sdap_rx,
                   int pdusession_id,
                   ue_id_t ue_id,
                   char *buf,
                   int size) {
  nr_sdap_entity_t *sdap_entity;
  sdap_entity = nr_sdap_get_entity(ue_id, pdusession_id);

  if (sdap_entity == NULL) {
    LOG_E(SDAP, "%s:%d:%s: Entity not found for ue rnti/ue_id: %lx and pdusession id: %d\n", __FILE__, __LINE__, __FUNCTION__, ue_id, pdusession_id);
    return;
  }

  sdap_entity->rx_entity(sdap_entity,
                         pdcp_entity,
                         is_gnb,
                         has_sdap_rx,
                         pdusession_id,
                         ue_id,
                         buf,
                         size);
}

static void *sdap_tun_read_thread(void *arg)
{
  DevAssert(arg != NULL);
  nr_sdap_entity_t *entity = arg;

  char rx_buf[NL_MAX_PAYLOAD];
  int len;
  reblock_tun_socket(entity->pdusession_sock);

  int rb_id = 1;

  while (!entity->stop_thread) {
    len = read(entity->pdusession_sock, &rx_buf, NL_MAX_PAYLOAD);
    if (len == -1) {
      if (errno == EINTR)
        continue; // interrupted system call

      if (errno == EBADF || errno == EINVAL) {
        LOG_I(SDAP, "Socket closed, exiting TUN read thread for UE %ld, PDU session %d\n", entity->ue_id, entity->pdusession_id);
        break;
      }

      LOG_E(PDCP, "read() failed: errno %d (%s)\n", errno, strerror(errno));
      break;
    }

    if (len == 0) {
      LOG_W(SDAP, "TUN socket returned EOF - exiting thread\n");
      break;
    }

    LOG_D(SDAP, "read data of size %d\n", len);

    protocol_ctxt_t ctxt = {.enb_flag = entity->is_gnb, .rntiMaybeUEid = entity->ue_id};

    bool dc = entity->is_gnb ? false : SDAP_HDR_UL_DATA_PDU;

    DevAssert(entity != NULL);
    entity->tx_entity(entity,
                      &ctxt,
                      SRB_FLAG_NO,
                      rb_id,
                      RLC_MUI_UNDEFINED,
                      RLC_SDU_CONFIRM_NO,
                      len,
                      (unsigned char *)rx_buf,
                      PDCP_TRANSMISSION_MODE_DATA,
                      NULL,
                      NULL,
                      entity->qfi,
                      dc);
  }

  return NULL;
}

void start_sdap_tun_gnb_first_ue_default_pdu_session(ue_id_t ue_id)
{
  nr_sdap_entity_t *entity = nr_sdap_get_entity(ue_id, get_softmodem_params()->default_pdu_session_id);
  DevAssert(entity != NULL);
  DevAssert(entity->is_gnb);
  char *ifprefix = get_softmodem_params()->nsa ? "oaitun_gnb" : "oaitun_enb";
  char ifname[IFNAMSIZ];
  tun_generate_ifname(ifname, ifprefix, ue_id - 1);
  entity->pdusession_sock = tun_alloc(ifname);
  tun_config(ifname, "10.0.1.1", NULL);
  threadCreate(&entity->pdusession_thread, sdap_tun_read_thread, entity, "gnb_tun_read_thread", -1, OAI_PRIORITY_RT_LOW);
}

void start_sdap_tun_ue(ue_id_t ue_id, int pdu_session_id, int sock)
{
  nr_sdap_entity_t *entity = nr_sdap_get_entity(ue_id, pdu_session_id);
  DevAssert(entity != NULL);
  DevAssert(!entity->is_gnb);
  entity->pdusession_sock = sock;
  entity->stop_thread = false;
  char thread_name[64];
  snprintf(thread_name, sizeof(thread_name), "ue_tun_read_%ld_p%d", ue_id, pdu_session_id);
  threadCreate(&entity->pdusession_thread, sdap_tun_read_thread, entity, thread_name, -1, OAI_PRIORITY_RT_LOW);
}


void create_ue_ip_if(const char *ipv4, const char *ipv6, int ue_id, int pdu_session_id)
{
  int default_pdu = get_softmodem_params()->default_pdu_session_id;
  char ifname[IFNAMSIZ];
  tun_generate_ue_ifname(ifname, ue_id, pdu_session_id != default_pdu ? pdu_session_id : -1);
  const int sock = tun_alloc(ifname);
  tun_config(ifname, ipv4, ipv6);
  if (ipv4) {
    setup_ue_ipv4_route(ifname, ue_id, ipv4);
  }
  start_sdap_tun_ue(ue_id, pdu_session_id, sock); // interface name suffix is ue_id+1
}

void remove_ip_if(nr_sdap_entity_t *entity)
{
  DevAssert(entity != NULL);
  // Stop the read thread
  entity->stop_thread = true;

  // Close the socket: read() will get EBADF and exit
  close(entity->pdusession_sock);

  int ret = pthread_join(entity->pdusession_thread, NULL);
  AssertFatal(ret == 0, "pthread_join() failed, errno: %d, %s\n", errno, strerror(errno));
  // Bring down the IP interface
  int default_pdu = get_softmodem_params()->default_pdu_session_id;
  char ifname[IFNAMSIZ];
  if (entity->is_gnb) {
    char *ifprefix = get_softmodem_params()->nsa ? "oaitun_gnb" : "oaitun_enb";
    tun_generate_ifname(ifname, ifprefix, entity->ue_id - 1);
  } else {
    tun_generate_ue_ifname(ifname, entity->ue_id, entity->pdusession_id != default_pdu ? entity->pdusession_id : -1);
  }
  tun_destroy(ifname);
}
