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

#ifndef TUN_IF_H_
#define TUN_IF_H_

#include <stdbool.h>

/* TODO: doc */
int tun_init(const char *ifprefix, int num_if, int id);

/* TODO: doc */
int tun_init_mbms(char *ifsuffix, int id);

/*! \fn int  tun_config(char*, int, int)
 * \brief This function initializes the nasmesh interface using the basic values,
 * basic address, network mask and broadcast address, as the default configured
 * ones
 * \param[in] interface_id number of this interface, prepended after interface
 * name
 * \param[in] ipv4 IPv4 address of this interface as a string
 * \param[in] ipv6 IPv6 address of this interface as a string
 * \param[in] ifprefix interface name prefix to which an interface number will
 * be appended
 * \return true on success, otherwise false
 * \note
 * @ingroup  _nas
 */
bool tun_config(int interface_id, const char *ipv4, const char *ipv6, const char *ifprefix);

/*!
 * \brief Setup a IPv4 rule in table (interface_id - 1 + 10000) and route to
 * force packets coming into interface back through it, and workaround
 * net.ipv4.conf.all.rp_filter=2 (strict source filtering would filter out
 * responses of packets going out through interface to another IP address not
 * in same subnet).
 * \param[in] interface_id number of this interface, prepended after interface
 * name
 * \param[in] ipv4 IPv4 address of the UE
 * \param[in] ifprefix interface name prefix to which an interface number will
 * be appended
 */
void setup_ue_ipv4_route(int interface_id, const char *ipv4, const char *ifpref);

#endif /*TUN_IF_H_*/
