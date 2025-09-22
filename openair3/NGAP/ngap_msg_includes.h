/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements. See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1 (the "License"); you may not use this file
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

/*! \file ngap_msg_includes.h
 * \brief NGAP ASN.1 message includes
 * \author Guido Casati
 * \date 2024
 * \email: hello@guidocasati.com
 * \version 1.0
 * @ingroup _ngap
 *
 * This header file contains the includes for the NGAP ASN.1 messages,
 * generated from the ASN.1 specifications.
 */

#ifndef NGAP_MSG_INCLUDES_H
#define NGAP_MSG_INCLUDES_H

#include "NGAP_AllowedNSSAI-Item.h"
#include "NGAP_AssociatedQosFlowItem.h"
#include "NGAP_BroadcastPLMNItem.h"
#include "NGAP_GlobalGNB-ID.h"
#include "NGAP_GTPTunnel.h"
#include "NGAP_InitiatingMessage.h"
#include "NGAP_NGAP-PDU.h"
#include "NGAP_PDUSessionResourceFailedToModifyItemModRes.h"
#include "NGAP_PDUSessionResourceFailedToSetupItemCxtRes.h"
#include "NGAP_PDUSessionResourceFailedToSetupItemSURes.h"
#include "NGAP_PDUSessionResourceItemCxtRelCpl.h"
#include "NGAP_PDUSessionResourceItemCxtRelReq.h"
#include "NGAP_PDUSessionResourceModifyItemModReq.h"
#include "NGAP_PDUSessionResourceModifyItemModRes.h"
#include "NGAP_PDUSessionResourceModifyResponseTransfer.h"
#include "NGAP_PDUSessionResourceModifyUnsuccessfulTransfer.h"
#include "NGAP_PDUSessionResourceReleasedItemRelRes.h"
#include "NGAP_PDUSessionResourceSetupItemCxtReq.h"
#include "NGAP_PDUSessionResourceSetupItemCxtRes.h"
#include "NGAP_PDUSessionResourceSetupItemSUReq.h"
#include "NGAP_PDUSessionResourceSetupItemSURes.h"
#include "NGAP_PDUSessionResourceSetupResponseTransfer.h"
#include "NGAP_PDUSessionResourceSetupUnsuccessfulTransfer.h"
#include "NGAP_PDUSessionResourceToReleaseItemRelCmd.h"
#include "NGAP_PLMNSupportItem.h"
#include "NGAP_ProtocolIE-Field.h"
#include "NGAP_QosFlowAddOrModifyResponseItem.h"
#include "NGAP_QosFlowAddOrModifyResponseList.h"
#include "NGAP_ServedGUAMIItem.h"
#include "NGAP_SliceSupportItem.h"
#include "NGAP_SuccessfulOutcome.h"
#include "NGAP_SupportedTAItem.h"
#include "NGAP_TAIListForPagingItem.h"
#include "NGAP_UE-NGAP-ID-pair.h"
#include "NGAP_UnsuccessfulOutcome.h"
#include "NGAP_UserLocationInformationNR.h"
#include "NGAP_asn_constant.h"
#include "NGAP_PDUSessionResourceSetupRequestTransfer.h"
#include "NGAP_QosFlowSetupRequestItem.h"
#include "NGAP_QosCharacteristics.h"
#include "NGAP_NonDynamic5QIDescriptor.h"
#include "NGAP_Dynamic5QIDescriptor.h"
#include "NGAP_PDUSessionResourceModifyRequestTransfer.h"
#include "NGAP_QosFlowAddOrModifyRequestItem.h"
#include "NGAP_PDUSessionResourceItemHORqd.h"
#include "NGAP_PDUSessionResourceInformationItem.h"
#include "NGAP_QosFlowInformationItem.h"
#include "NGAP_LastVisitedCellItem.h"
#include "NGAP_LastVisitedNGRANCellInformation.h"
#include "NGAP_PDUSessionResourceInformationList.h"
#include "NGAP_HandoverRequiredTransfer.h"
#include "NGAP_SourceNGRANNode-ToTargetNGRANNode-TransparentContainer.h"
#include "NGAP_PDUSessionResourceSetupItemHOReq.h"
#include "NGAP_HandoverCommand.h"
#include "NGAP_HandoverCommandTransfer.h"
#include "NGAP_TargetNGRANNode-ToSourceNGRANNode-TransparentContainer.h"
#include "NGAP_TargetToSource-TransparentContainer.h"
#include "NGAP_UEHistoryInformation.h"
#include "NGAP_Cause.h"
#include "NGAP_PDUSessionResourceHandoverItem.h"
#include "NGAP_SourceToTarget-TransparentContainer.h"
#include "NGAP_HandoverRequest.h"
#include "NGAP_PDUSessionResourceAdmittedList.h"
#include "NGAP_PDUSessionResourceAdmittedItem.h"
#include "NGAP_HandoverRequestAcknowledgeTransfer.h"
#include "NGAP_QosFlowItemWithDataForwarding.h"
#include "NR_HandoverPreparationInformation.h"
#include "NGAP_HandoverNotify.h"
#include "NGAP_DRBsSubjectToStatusTransferItem.h"
#include "NGAP_DRBStatusUL.h"
#include "NGAP_DRBStatusUL18.h"
#include "NGAP_DRBStatusUL12.h"
#include "NGAP_DRBStatusDL.h"
#include "NGAP_DRBStatusDL18.h"
#include "NGAP_DRBStatusDL12.h"

#endif // NGAP_MSG_INCLUDES_H
