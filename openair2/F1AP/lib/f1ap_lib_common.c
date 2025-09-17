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

#include "f1ap_lib_common.h"
#include "f1ap_messages_types.h"

#include "OCTET_STRING.h"
#include "common/utils/utils.h"
#include "common/utils/assertions.h"
#include "common/utils/utils.h"

bool eq_f1ap_plmn(const f1ap_plmn_t *a, const f1ap_plmn_t *b)
{
  _F1_EQ_CHECK_INT(a->mcc, b->mcc);
  _F1_EQ_CHECK_INT(a->mnc, b->mnc);
  _F1_EQ_CHECK_INT(a->mnc_digit_length, b->mnc_digit_length);
  return true;
}

bool eq_f1ap_freq_info(const f1ap_nr_frequency_info_t *a, const f1ap_nr_frequency_info_t *b)
{
  if (a->arfcn != b->arfcn)
    return false;
  if (a->band != b->band)
    return false;
  return true;
}

bool eq_f1ap_tx_bandwidth(const f1ap_transmission_bandwidth_t *a, const f1ap_transmission_bandwidth_t *b)
{
  if (a->nrb != b->nrb)
    return false;
  if (a->scs != b->scs)
    return false;
  return true;
}

bool eq_f1ap_cell_info(const f1ap_served_cell_info_t *a, const f1ap_served_cell_info_t *b)
{
  if (a->nr_cellid != b->nr_cellid)
    return false;
  if (a->nr_pci != b->nr_pci)
    return false;
  if (*a->tac != *b->tac)
    return false;
  if (a->mode != b->mode)
    return false;
  if (a->mode == F1AP_MODE_TDD) {
    /* TDD */
    if (!eq_f1ap_tx_bandwidth(&a->tdd.tbw, &b->tdd.tbw))
      return false;
    if (!eq_f1ap_freq_info(&a->tdd.freqinfo, &b->tdd.freqinfo))
      return false;
  } else if (a->mode == F1AP_MODE_FDD) {
    /* FDD */
    if (!eq_f1ap_tx_bandwidth(&a->fdd.dl_tbw, &b->fdd.dl_tbw))
      return false;
    if (!eq_f1ap_freq_info(&a->fdd.dl_freqinfo, &b->fdd.dl_freqinfo))
      return false;
    if (!eq_f1ap_tx_bandwidth(&a->fdd.ul_tbw, &b->fdd.ul_tbw))
      return false;
    if (!eq_f1ap_freq_info(&a->fdd.ul_freqinfo, &b->fdd.ul_freqinfo))
      return false;
  }
  if (a->measurement_timing_config_len != b->measurement_timing_config_len)
    return false;
  if (*a->measurement_timing_config != *b->measurement_timing_config)
    return false;
  if (!eq_f1ap_plmn(&a->plmn, &b->plmn))
    return false;
  return true;
}

bool eq_f1ap_sys_info(const f1ap_gnb_du_system_info_t *a, const f1ap_gnb_du_system_info_t *b)
{
  /* MIB */
  if (a->mib_length != b->mib_length)
    return false;
  for (int i = 0; i < a->mib_length; i++) {
      if (a->mib[i] != b->mib[i])
          return false;
  }
  /* SIB1 */
  if (a->sib1_length != b->sib1_length)
    return false;
  for (int i = 0; i < a->sib1_length; i++) {
      if (a->sib1[i] != b->sib1[i])
          return false;
  }
  return true;
}

uint8_t *cp_octet_string(const OCTET_STRING_t *os, int *len)
{
  uint8_t *buf = calloc_or_fail(os->size, sizeof(*buf));
  memcpy(buf, os->buf, os->size);
  *len = os->size;
  return buf;
}
