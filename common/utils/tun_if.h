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
#include <net/if.h>

/*!
 * \brief This function generates the name of the interface based on the prefix and
 * the instance id.
 *
 * \param[in,out] ifname name of the interface
 * \param[in] ifprefix prefix of the interface
 * \param[in] instance_id unique instance number
 */
int tun_generate_ifname(char *ifname, const char *ifprefix, int instance_id);
int tun_generate_ue_ifname(char *ifname, int instance_id, int pdu_session_id);

/*!
 * \brief This function initializes the TUN interface
 * \param[in] ifname name of the interface
 * \param[in] instance_id unique instance number, used to save socket file descriptor
 */
int tun_init(const char *ifname, int instance_id);

/*!
 * \brief This function initializes the TUN interface for MBMS
 * \param[in] ifname name of the interface
 */
int tun_init_mbms(char *ifname);

/*!
 * \brief This function initializes the nasmesh interface using the basic values,
 * basic address, network mask and broadcast address, as the default configured
 * ones
 * \param[in] ifname name of the interface
 * \param[in] ipv4 IPv4 address of this interface as a string
 * \param[in] ipv6 IPv6 address of this interface as a string
 * \return true on success, otherwise false
 * \note
 * @ingroup  _nas
 */
bool tun_config(const char* ifname, const char *ipv4, const char *ipv6);

/*!
 * \brief Setup a IPv4 rule in table (interface_id - 1 + 10000) and route to
 * force packets coming into interface back through it, and workaround
 * net.ipv4.conf.all.rp_filter=2 (strict source filtering would filter out
 * responses of packets going out through interface to another IP address not
 * in same subnet).
 * \param[in] ifname name of the interface
 * \param[in] instance_id unique instance number, used to create the table
 * \param[in] ipv4 IPv4 address of the UE
 */
void setup_ue_ipv4_route(const char* ifname, int instance_id, const char *ipv4);

/*!
 * \brief This function allocates a TUN interface
 * \param[in] dev name of the interface
 * \return file descriptor of the allocated interface
 */
int tun_alloc(const char *dev);

/*!
 * \brief This function destroys the TUN interface
 * \param[in] dev name of the interface
 */
void tun_destroy(const char *dev);

#endif /*TUN_IF_H_*/
