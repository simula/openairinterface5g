/* Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
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

#ifndef RRC_GNB_MOBILITY_H_
#define RRC_GNB_MOBILITY_H_

#include <stdint.h>
#include "common/utils/ds/byte_array.h"

/* forward declarations */
typedef struct gNB_RRC_INST_s gNB_RRC_INST;
typedef struct gNB_RRC_UE_s gNB_RRC_UE_t;
typedef struct nr_rrc_du_container_t nr_rrc_du_container_t;

typedef struct NR_CellGroupConfig NR_CellGroupConfig_t;

typedef void (*ho_cancel_t)(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue);
typedef struct nr_ho_source_cu {
  /// pointer to the (source) DU structure
  const nr_rrc_du_container_t *du;
  /// (source) DU UE ID; in F1, the DU UE ID will change to the new (target) DU
  /// UE ID during handover, and the CU needs to keep track of this in case of
  /// reestablishment
  uint32_t du_ue_id;
  /// old (source) RNTI (to recognize a UE during reestablishment)
  rnti_t old_rnti;
  /// old (source) CellGroupConfig (to send the cellGroupConfig in case of
  /// reestablishment)
  NR_CellGroupConfig_t *old_cellGroupConfig;
  /// function pointer to announce the handover cancellation, e.g.,
  /// reestablishment
  ho_cancel_t ho_cancel;
} nr_ho_source_cu_t;

/* acknowledgement of handover request. buf+len is the RRC Reconfiguration */
typedef void (*ho_req_ack_t)(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue);
typedef void (*ho_success_t)(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue);
typedef struct nr_ho_target_cu {
  /// pointer to the (target) DU structure
  const nr_rrc_du_container_t *du;
  /// (target) DU UE ID; the (source) DU UE ID will change to the new (target)
  /// DU UE ID after sending a reconfiguration, which is later than when
  /// receiving this ID
  uint32_t du_ue_id;
  /// new (target) RNTI (as for du_ue_id)
  rnti_t new_rnti;
  /// function pointer to announce handover request acknowledgment
  ho_req_ack_t ho_req_ack;
  /// function pointer to announce handover success
  ho_success_t ho_success;
} nr_ho_target_cu_t;

typedef struct nr_handover_context_s {
  nr_ho_source_cu_t *source;
  nr_ho_target_cu_t *target;
} nr_handover_context_t;

void nr_rrc_trigger_f1_ho(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue, nr_rrc_du_container_t *source_du, nr_rrc_du_container_t *target_du);
void nr_rrc_finalize_ho(gNB_RRC_UE_t *ue);

#endif /* RRC_GNB_MOBILITY_H_ */
