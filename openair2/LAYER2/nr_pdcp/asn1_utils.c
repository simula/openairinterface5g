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

#include "nr_pdcp_asn1_utils.h"
#include "common/utils/LOG/log.h"
#include "nr_pdcp_entity.h"
#include "nr_pdcp_configuration.h"

#define ENCODE_DECODE(a, b)                          \
int decode_##a(int v)                                \
{                                                    \
  static const int tab[] = { VALUES_NR_PDCP_##b };   \
  AssertFatal(v >= 0 && v < SIZEOF_NR_PDCP_##b,      \
              "bad encoded value " #a " %d\n", v);   \
  return tab[v];                                     \
}                                                    \
                                                     \
int encode_##a(int v)                                \
{                                                    \
  static const int tab[] = { VALUES_NR_PDCP_##b };   \
  for (int ret = 0; ret < SIZEOF_NR_PDCP_##b; ret++) \
    if (tab[ret] == v)                               \
      return ret;                                    \
  AssertFatal(0, "bad " #a " value %d\n", v);        \
}

ENCODE_DECODE(t_reordering, T_REORDERING)
ENCODE_DECODE(sn_size_ul, SN_SIZE)
ENCODE_DECODE(sn_size_dl, SN_SIZE)
ENCODE_DECODE(discard_timer, DISCARD_TIMER)
