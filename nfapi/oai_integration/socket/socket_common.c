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
#include "socket_common.h"
#include "LOG/log.h"

int socket_send_p5_msg(const int sctp,
                       const int socket,
                       const void* remote_addr,
                       const void* msg,
                       const uint32_t len,
                       const uint16_t stream)
{
  if (sctp == 1) {
    struct sockaddr* to = (struct sockaddr*)remote_addr;
    const int retval = sctp_sendmsg(socket, msg, len, to, 0, 42, 0, stream, 0, 0);
    if (retval < 0) {
      LOG_E(NR_PHY, "sctp_sendmsg failed errno: %d\n", errno);
      return -1;
    }
  } else {
    if (send(socket, msg, len, 0) != len) {
      LOG_E(NR_PHY, "write failed errno: %d\n", errno);
      return -1;
    }
  }

  return len;
}

int socket_send_p7_msg(const int socket, const void* remote_addr, const void* msg, const uint32_t len)
{
  const struct sockaddr* to = (struct sockaddr*)remote_addr;
  const long sendto_result = sendto(socket, msg, len, 0, to, sizeof(struct sockaddr_in));
  if (sendto_result != len) {
    LOG_E(NR_PHY, "%s() sendto_result %ld %d\n", __FUNCTION__, sendto_result, errno);
    return -1;
  }

  return 0;
}
