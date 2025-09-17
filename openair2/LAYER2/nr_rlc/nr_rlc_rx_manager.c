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

#include "nr_rlc_rx_manager.h"

#include "common/utils/utils.h"

nr_rlc_rx_manager_t *nr_rlc_new_rx_manager(int size)
{
  nr_rlc_rx_manager_t *ret = calloc_or_fail(1, sizeof(*ret));

  ret->rx_list = calloc_or_fail(size, sizeof(*ret->rx_list));
  ret->size = size;

  return ret;
}

void nr_rlc_clear_rx_manager(nr_rlc_rx_manager_t *m)
{
  for (int i = 0; i < m->size; i++) {
    nr_rlc_pdu_t *cur = m->rx_list[i];
    while (cur) {
      nr_rlc_pdu_t *f = cur;
      cur = cur->next;
      nr_rlc_free_pdu(f);
    }
    m->rx_list[i] = 0;
  }
  m->start = 0;
  m->sn_start = 0;
}

void nr_rlc_free_rx_manager(nr_rlc_rx_manager_t *m)
{
  nr_rlc_clear_rx_manager(m);
  free(m->rx_list);
  free(m);
}

static inline int get_pos(nr_rlc_rx_manager_t *m, int sn)
{
  return (m->start + (sn - m->sn_start + m->size * 2)) % m->size;
}

nr_rlc_pdu_t *nr_rlc_rx_manager_get_pdu_from_sn(nr_rlc_rx_manager_t *m, int sn)
{
  int pos = get_pos(m, sn);
  return m->rx_list[pos];
}

void nr_rlc_rx_manager_clear_pdu(nr_rlc_rx_manager_t *m, int sn)
{
  int pos = get_pos(m, sn);
  m->rx_list[pos] = 0;
}

/* beware: after calling this function 'sn_start' is invalid, caller has
 * to call nr_rlc_rx_manager_set_start() just after
 */
void nr_rlc_rx_manager_advance(nr_rlc_rx_manager_t *m, int count)
{
  m->start += count;
  m->start %= m->size;
}

void nr_rlc_rx_manager_set_start(nr_rlc_rx_manager_t *m, int sn_start)
{
  m->sn_start = sn_start;
}

void nr_rlc_rx_manager_add_pdu(nr_rlc_rx_manager_t *m, nr_rlc_pdu_t *pdu)
{
  int pos = get_pos(m, pdu->sn);

  nr_rlc_pdu_t head;
  nr_rlc_pdu_t *cur;
  nr_rlc_pdu_t *prev;

  head.next = m->rx_list[pos];
  cur = m->rx_list[pos];
  prev = &head;

  /* order by 'so' */
  while (cur != NULL) {
    /* check if 'pdu' is before 'cur' in the list */
    if (cur->so > pdu->so) {
      break;
    }
    prev = cur;
    cur = cur->next;
  }
  prev->next = pdu;
  pdu->next = cur;

  m->rx_list[pos] = head.next;
}
