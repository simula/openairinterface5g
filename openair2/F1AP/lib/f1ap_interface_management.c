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

#include <string.h>

#include "common/utils/assertions.h"
#include "openair3/UTILS/conversions.h"
#include "common/utils/oai_asn1.h"
#include "common/utils/utils.h"

#include "f1ap_interface_management.h"
#include "f1ap_lib_common.h"
#include "f1ap_lib_includes.h"
#include "f1ap_messages_types.h"
#include "f1ap_lib_extern.h"

static const int nrb_lut[29] = {11,  18,  24,  25,  31,  32,  38,  51,  52,  65,  66,  78,  79,  93, 106,
                                107, 121, 132, 133, 135, 160, 162, 189, 216, 217, 245, 264, 270, 273};

static int to_NRNRB(int nrb)
{
  for (int i = 0; i < sizeofArray(nrb_lut); i++)
    if (nrb_lut[i] == nrb)
      return i;
  AssertFatal(1 == 0, "nrb %d is not in the list of possible NRNRB\n", nrb);
}

static int read_slice_info(const F1AP_ServedPLMNs_Item_t *plmn, nssai_t *nssai, int max_nssai)
{
  if (plmn->iE_Extensions == NULL)
    return 0;

  const F1AP_ProtocolExtensionContainer_10696P34_t *p = (F1AP_ProtocolExtensionContainer_10696P34_t *)plmn->iE_Extensions;
  if (p->list.count == 0)
    return 0;

  const F1AP_ServedPLMNs_ItemExtIEs_t *splmn = p->list.array[0];
  DevAssert(splmn->id == F1AP_ProtocolIE_ID_id_TAISliceSupportList);
  DevAssert(splmn->extensionValue.present == F1AP_ServedPLMNs_ItemExtIEs__extensionValue_PR_SliceSupportList);
  const F1AP_SliceSupportList_t *ssl = &splmn->extensionValue.choice.SliceSupportList;
  AssertFatal(ssl->list.count <= max_nssai, "cannot handle more than 16 slices\n");
  for (int s = 0; s < ssl->list.count; ++s) {
    const F1AP_SliceSupportItem_t *sl = ssl->list.array[s];
    nssai_t *n = &nssai[s];
    OCTET_STRING_TO_INT8(&sl->sNSSAI.sST, n->sst);
    n->sd = 0xffffff;
    if (sl->sNSSAI.sD != NULL)
      OCTET_STRING_TO_INT24(sl->sNSSAI.sD, n->sd);
  }

  return ssl->list.count;
}

/**
 * @brief F1AP Setup Request memory management
 */
static void free_f1ap_cell(const f1ap_served_cell_info_t *info, const f1ap_gnb_du_system_info_t *sys_info)
{
  if (sys_info) {
    free(sys_info->mib);
    free(sys_info->sib1);
    free((void *)sys_info);
  }
  free(info->measurement_timing_config);
  free(info->tac);
}

/**
 * @brief Encoding of Served Cell Information (9.3.1.10 of 3GPP TS 38.473)
 */
static F1AP_Served_Cell_Information_t encode_served_cell_info(const f1ap_served_cell_info_t *c)
{
  F1AP_Served_Cell_Information_t scell_info = {0};
  // NR CGI
  MCC_MNC_TO_PLMNID(c->plmn.mcc, c->plmn.mnc, c->plmn.mnc_digit_length, &(scell_info.nRCGI.pLMN_Identity));
  NR_CELL_ID_TO_BIT_STRING(c->nr_cellid, &(scell_info.nRCGI.nRCellIdentity));
  // NR PCI
  scell_info.nRPCI = c->nr_pci; // int 0..1007
  // 5GS TAC
  if (c->tac != NULL) {
    uint32_t tac = htonl(*c->tac);
    asn1cCalloc(scell_info.fiveGS_TAC, netOrder);
    OCTET_STRING_fromBuf(netOrder, ((char *)&tac) + 1, 3);
  }
  // Served PLMNs
  asn1cSequenceAdd(scell_info.servedPLMNs.list, F1AP_ServedPLMNs_Item_t, servedPLMN_item);
  MCC_MNC_TO_PLMNID(c->plmn.mcc, c->plmn.mnc, c->plmn.mnc_digit_length, &servedPLMN_item->pLMN_Identity);
  // NR-Mode-Info
  F1AP_NR_Mode_Info_t *nR_Mode_Info = &scell_info.nR_Mode_Info;
  if (c->mode == F1AP_MODE_FDD) { // FDD
    const f1ap_fdd_info_t *fdd = &c->fdd;
    nR_Mode_Info->present = F1AP_NR_Mode_Info_PR_fDD;
    asn1cCalloc(nR_Mode_Info->choice.fDD, fDD_Info);
    /* FDD.1.1 UL NRFreqInfo ARFCN */
    fDD_Info->uL_NRFreqInfo.nRARFCN = fdd->ul_freqinfo.arfcn;
    /* FDD.1.3 freqBandListNr */
    int ul_band = 1;
    for (int j = 0; j < ul_band; j++) {
      asn1cSequenceAdd(fDD_Info->uL_NRFreqInfo.freqBandListNr.list, F1AP_FreqBandNrItem_t, nr_freqBandNrItem);
      /* FDD.1.3.1 freqBandIndicatorNr*/
      nr_freqBandNrItem->freqBandIndicatorNr = fdd->ul_freqinfo.band;
    }
    /* FDD.2.1 DL NRFreqInfo ARFCN */
    fDD_Info->dL_NRFreqInfo.nRARFCN = fdd->dl_freqinfo.arfcn;
    /* FDD.2.3 freqBandListNr */
    int dl_bands = 1;
    for (int j = 0; j < dl_bands; j++) {
      asn1cSequenceAdd(fDD_Info->dL_NRFreqInfo.freqBandListNr.list, F1AP_FreqBandNrItem_t, nr_freqBandNrItem);
      /* FDD.2.3.1 freqBandIndicatorNr*/
      nr_freqBandNrItem->freqBandIndicatorNr = fdd->dl_freqinfo.band;
    } // for FDD : DL freq_Bands
    /* FDD.3 UL Transmission Bandwidth */
    fDD_Info->uL_Transmission_Bandwidth.nRSCS = fdd->ul_tbw.scs;
    fDD_Info->uL_Transmission_Bandwidth.nRNRB = to_NRNRB(fdd->ul_tbw.nrb);
    /* FDD.4 DL Transmission Bandwidth */
    fDD_Info->dL_Transmission_Bandwidth.nRSCS = fdd->dl_tbw.scs;
    fDD_Info->dL_Transmission_Bandwidth.nRNRB = to_NRNRB(fdd->dl_tbw.nrb);
  } else if (c->mode == F1AP_MODE_TDD) {
    const f1ap_tdd_info_t *tdd = &c->tdd;
    nR_Mode_Info->present = F1AP_NR_Mode_Info_PR_tDD;
    asn1cCalloc(nR_Mode_Info->choice.tDD, tDD_Info);
    /* TDD.1.1 nRFreqInfo ARFCN */
    tDD_Info->nRFreqInfo.nRARFCN = tdd->freqinfo.arfcn;
    /* TDD.1.3 freqBandListNr */
    int bands = 1;
    for (int j = 0; j < bands; j++) {
      asn1cSequenceAdd(tDD_Info->nRFreqInfo.freqBandListNr.list, F1AP_FreqBandNrItem_t, nr_freqBandNrItem);
      /* TDD.1.3.1 freqBandIndicatorNr*/
      nr_freqBandNrItem->freqBandIndicatorNr = tdd->freqinfo.band;
    }
    /* TDD.2 transmission_Bandwidth */
    tDD_Info->transmission_Bandwidth.nRSCS = tdd->tbw.scs;
    tDD_Info->transmission_Bandwidth.nRNRB = to_NRNRB(tdd->tbw.nrb);
  } else {
    AssertFatal(false, "unknown duplex mode %d\n", c->mode);
  }
  // Measurement Timing Configuration
  OCTET_STRING_fromBuf(&scell_info.measurementTimingConfiguration,
                       (const char *)c->measurement_timing_config,
                       c->measurement_timing_config_len);

  return scell_info;
}

static bool decode_served_cell_info(const F1AP_Served_Cell_Information_t *in, f1ap_served_cell_info_t *info)
{
  AssertError(in != NULL, return false, "Input message pointer is NULL");
  // 5GS TAC (O)
  if (in->fiveGS_TAC) {
    info->tac = malloc_or_fail(sizeof(*info->tac));
    OCTET_STRING_TO_INT24(in->fiveGS_TAC, *info->tac);
  }
  // NR CGI (M)
  TBCD_TO_MCC_MNC(&(in->nRCGI.pLMN_Identity), info->plmn.mcc, info->plmn.mnc, info->plmn.mnc_digit_length);
  // NR Cell Identity (M)
  BIT_STRING_TO_NR_CELL_IDENTITY(&in->nRCGI.nRCellIdentity, info->nr_cellid);
  // NR PCI (M)
  info->nr_pci = in->nRPCI;
  // Served PLMNs (>= 1)
  AssertError(in->servedPLMNs.list.count == 1, return false, "at least and only 1 PLMN must be present");
  info->num_ssi = read_slice_info(in->servedPLMNs.list.array[0], info->nssai, 16);
  // FDD Info
  if (in->nR_Mode_Info.present == F1AP_NR_Mode_Info_PR_fDD) {
    info->mode = F1AP_MODE_FDD;
    f1ap_fdd_info_t *FDDs = &info->fdd;
    F1AP_FDD_Info_t *fDD_Info = in->nR_Mode_Info.choice.fDD;
    FDDs->ul_freqinfo.arfcn = fDD_Info->uL_NRFreqInfo.nRARFCN;
    // Note: cannot handle more than one UL frequency band
    int num_ulBands = fDD_Info->uL_NRFreqInfo.freqBandListNr.list.count;
    AssertError(num_ulBands == 1, return false, "1 FDD UL frequency band must be present, more is not supported");
    for (int f = 0; f < num_ulBands && f < 1; f++) {
      F1AP_FreqBandNrItem_t *FreqItem = fDD_Info->uL_NRFreqInfo.freqBandListNr.list.array[f];
      FDDs->ul_freqinfo.band = FreqItem->freqBandIndicatorNr;
      AssertError(FreqItem->supportedSULBandList.list.count == 0, return false, "cannot handle FDD SUL bands");
    }
    // Note: cannot handle more than one DL frequency band
    FDDs->dl_freqinfo.arfcn = fDD_Info->dL_NRFreqInfo.nRARFCN;
    int num_dlBands = fDD_Info->dL_NRFreqInfo.freqBandListNr.list.count;
    AssertError(num_dlBands == 1, return false, "1 FDD DL frequency band must be present, more is not supported");
    for (int dlB = 0; dlB < num_dlBands && dlB < 1; dlB++) {
      F1AP_FreqBandNrItem_t *FreqItem = fDD_Info->dL_NRFreqInfo.freqBandListNr.list.array[dlB];
      FDDs->dl_freqinfo.band = FreqItem->freqBandIndicatorNr;
      AssertError(FreqItem->supportedSULBandList.list.count == 0, return false, "cannot handle FDD SUL bands");
    }
    FDDs->ul_tbw.scs = fDD_Info->uL_Transmission_Bandwidth.nRSCS;
    FDDs->ul_tbw.nrb = nrb_lut[fDD_Info->uL_Transmission_Bandwidth.nRNRB];
    FDDs->dl_tbw.scs = fDD_Info->dL_Transmission_Bandwidth.nRSCS;
    FDDs->dl_tbw.nrb = nrb_lut[fDD_Info->dL_Transmission_Bandwidth.nRNRB];
  } else if (in->nR_Mode_Info.present == F1AP_NR_Mode_Info_PR_tDD) {
    info->mode = F1AP_MODE_TDD;
    f1ap_tdd_info_t *TDDs = &info->tdd;
    F1AP_TDD_Info_t *tDD_Info = in->nR_Mode_Info.choice.tDD;
    TDDs->freqinfo.arfcn = tDD_Info->nRFreqInfo.nRARFCN;
    // Handle frequency bands
    int num_tddBands = tDD_Info->nRFreqInfo.freqBandListNr.list.count;
    AssertError(num_tddBands == 1, return false, "1 TDD DL frequency band must be present, more is not supported");
    for (int f = 0; f < num_tddBands && f < 1; f++) {
      struct F1AP_FreqBandNrItem *FreqItem = tDD_Info->nRFreqInfo.freqBandListNr.list.array[f];
      TDDs->freqinfo.band = FreqItem->freqBandIndicatorNr;
      AssertError(FreqItem->supportedSULBandList.list.count == 0, return false, "cannot handle TDD SUL bands");
    }
    TDDs->tbw.scs = tDD_Info->transmission_Bandwidth.nRSCS;
    TDDs->tbw.nrb = nrb_lut[tDD_Info->transmission_Bandwidth.nRNRB];
  } else {
    AssertError(1 == 0, return false, "unknown or missing NR Mode info %d\n", in->nR_Mode_Info.present);
  }
  // MeasurementConfig (M)
  AssertError(in->measurementTimingConfiguration.size > 0, return false, "measurementTimingConfigurationc size is 0");
  info->measurement_timing_config = cp_octet_string(&in->measurementTimingConfiguration, &info->measurement_timing_config_len);
  return true;
}

static F1AP_GNB_DU_System_Information_t *encode_system_info(const f1ap_gnb_du_system_info_t *sys_info)
{
  if (sys_info == NULL)
    return NULL; /* optional: can be NULL */

  F1AP_GNB_DU_System_Information_t *enc_sys_info = calloc_or_fail(1, sizeof(*enc_sys_info));

  AssertFatal(sys_info->mib != NULL, "MIB must be present in DU sys info\n");
  OCTET_STRING_fromBuf(&enc_sys_info->mIB_message, (const char *)sys_info->mib, sys_info->mib_length);

  AssertFatal(sys_info->sib1 != NULL, "SIB1 must be present in DU sys info\n");
  OCTET_STRING_fromBuf(&enc_sys_info->sIB1_message, (const char *)sys_info->sib1, sys_info->sib1_length);

  return enc_sys_info;
}

static void decode_system_info(struct F1AP_GNB_DU_System_Information *DUsi, f1ap_gnb_du_system_info_t *sys_info)
{
  /* mib */
  sys_info->mib = calloc_or_fail(DUsi->mIB_message.size, sizeof(*sys_info->mib));
  memcpy(sys_info->mib, DUsi->mIB_message.buf, DUsi->mIB_message.size);
  sys_info->mib_length = DUsi->mIB_message.size;
  /* sib1 */
  sys_info->sib1 = calloc_or_fail(DUsi->sIB1_message.size, sizeof(*sys_info->sib1));
  memcpy(sys_info->sib1, DUsi->sIB1_message.buf, DUsi->sIB1_message.size);
  sys_info->sib1_length = DUsi->sIB1_message.size;
}

static void encode_cells_to_activate(const served_cells_to_activate_t *cell, F1AP_Cells_to_be_Activated_List_ItemIEs_t *cells_to_be_activated_ies)
{
  cells_to_be_activated_ies->id = F1AP_ProtocolIE_ID_id_Cells_to_be_Activated_List_Item;
  cells_to_be_activated_ies->criticality = F1AP_Criticality_reject;
  cells_to_be_activated_ies->value.present = F1AP_Cells_to_be_Activated_List_ItemIEs__value_PR_Cells_to_be_Activated_List_Item;
  // Cells to be Activated List Item (M)
  F1AP_Cells_to_be_Activated_List_Item_t *cells_to_be_activated_item =
      &cells_to_be_activated_ies->value.choice.Cells_to_be_Activated_List_Item;
  // NR CGI (M)
  MCC_MNC_TO_PLMNID(cell->plmn.mcc,
                    cell->plmn.mnc,
                    cell->plmn.mnc_digit_length,
                    &(cells_to_be_activated_item->nRCGI.pLMN_Identity));
  NR_CELL_ID_TO_BIT_STRING(cell->nr_cellid, &(cells_to_be_activated_item->nRCGI.nRCellIdentity));
  // NR PCI (O)
  asn1cCalloc(cells_to_be_activated_item->nRPCI, tmp);
  *tmp = cell->nrpci;
  // gNB-CU System Information (O)
  for (int n = 0; n < cell->num_SI; n++) {
    F1AP_ProtocolExtensionContainer_10696P112_t *p = calloc_or_fail(1, sizeof(*p));
    cells_to_be_activated_item->iE_Extensions = (struct F1AP_ProtocolExtensionContainer *)p;
    asn1cSequenceAdd(p->list, F1AP_Cells_to_be_Activated_List_ItemExtIEs_t, cells_to_be_activated_itemExtIEs);
    cells_to_be_activated_itemExtIEs->id = F1AP_ProtocolIE_ID_id_gNB_CUSystemInformation;
    cells_to_be_activated_itemExtIEs->criticality = F1AP_Criticality_reject;
    cells_to_be_activated_itemExtIEs->extensionValue.present =
        F1AP_Cells_to_be_Activated_List_ItemExtIEs__extensionValue_PR_GNB_CUSystemInformation;
    // gNB-CU System Information message
    F1AP_GNB_CUSystemInformation_t *gNB_CUSystemInformation =
        &cells_to_be_activated_itemExtIEs->extensionValue.choice.GNB_CUSystemInformation;
    const f1ap_sib_msg_t *SI_msg = &cell->SI_msg[n];
    if (SI_msg->SI_container != NULL) {
      if (SI_msg->SI_type > 6 && SI_msg->SI_type != 9) {
        asn1cSequenceAdd(gNB_CUSystemInformation->sibtypetobeupdatedlist.list, F1AP_SibtypetobeupdatedListItem_t, sib_item);
        sib_item->sIBtype = SI_msg->SI_type;
        OCTET_STRING_fromBuf(&sib_item->sIBmessage, (const char *)SI_msg->SI_container, SI_msg->SI_container_length);
      }
    }
  }
}

static bool decode_cells_to_activate(served_cells_to_activate_t *out, const F1AP_Cells_to_be_Activated_List_ItemIEs_t *in)
{
  AssertError(in->id == F1AP_ProtocolIE_ID_id_Cells_to_be_Activated_List_Item,
              return false,
              "ID != F1AP_ProtocolIE_ID_id_Cells_to_be_Activated_List_Item\n");
  AssertError(in->value.present == F1AP_Cells_to_be_Activated_List_ItemIEs__value_PR_Cells_to_be_Activated_List_Item,
              return false,
              "in->value.present != F1AP_Cells_to_be_Activated_List_ItemIEs__value_PR_Cells_to_be_Activated_List_Item\n");
  const F1AP_Cells_to_be_Activated_List_Item_t *cell = &in->value.choice.Cells_to_be_Activated_List_Item;
  // NR CGI (M)
  TBCD_TO_MCC_MNC(&cell->nRCGI.pLMN_Identity, out->plmn.mcc, out->plmn.mnc, out->plmn.mnc_digit_length);
  BIT_STRING_TO_NR_CELL_IDENTITY(&cell->nRCGI.nRCellIdentity, out->nr_cellid);
  // NR PCI (O)
  if (cell->nRPCI != NULL)
    out->nrpci = *cell->nRPCI;
  /* IE extensions (O) */
  F1AP_ProtocolExtensionContainer_10696P112_t *ext = (F1AP_ProtocolExtensionContainer_10696P112_t *)cell->iE_Extensions;
  if (ext != NULL) {
    for (int cnt = 0; cnt < ext->list.count; cnt++) {
      AssertError(ext->list.count == 1, return false, "At least one SI message should be present, and only 1 is supported");
      F1AP_Cells_to_be_Activated_List_ItemExtIEs_t *cells_to_be_activated_list_itemExtIEs =
          (F1AP_Cells_to_be_Activated_List_ItemExtIEs_t *)ext->list.array[cnt];
      /* IE extensions items (O) */
      switch (cells_to_be_activated_list_itemExtIEs->id) {
        /* gNB-CU System Information */
        case F1AP_ProtocolIE_ID_id_gNB_CUSystemInformation: {
          out->nrpci = (cell->nRPCI != NULL) ? *cell->nRPCI : 0;
          F1AP_GNB_CUSystemInformation_t *gNB_CUSystemInformation =
              (F1AP_GNB_CUSystemInformation_t *)&cells_to_be_activated_list_itemExtIEs->extensionValue.choice.GNB_CUSystemInformation;
          /* System Information */
          out->num_SI = gNB_CUSystemInformation->sibtypetobeupdatedlist.list.count;
          for (int s = 0; s < out->num_SI; s++) {
            F1AP_SibtypetobeupdatedListItem_t *sib_item = gNB_CUSystemInformation->sibtypetobeupdatedlist.list.array[s];
            /* SI container */
            f1ap_sib_msg_t *SI_msg = &out->SI_msg[s];
            SI_msg->SI_container = cp_octet_string(&sib_item->sIBmessage, &SI_msg->SI_container_length);
            /* SIB type */
            SI_msg->SI_type = sib_item->sIBtype;
          }
          break;
        }
        case F1AP_ProtocolIE_ID_id_AvailablePLMNList:
          AssertError(1 == 0, return false, "F1AP_ProtocolIE_ID_id_AvailablePLMNList is not supported");
          break;
        case F1AP_ProtocolIE_ID_id_ExtendedAvailablePLMN_List:
          AssertError(1 == 0, return false, "F1AP_ProtocolIE_ID_id_ExtendedAvailablePLMN_List is not supported");
          break;
        case F1AP_ProtocolIE_ID_id_IAB_Info_IAB_donor_CU:
          AssertError(1 == 0, return false, "F1AP_ProtocolIE_ID_id_IAB_Info_IAB_donor_CU is not supported");
          break;
        case F1AP_ProtocolIE_ID_id_AvailableSNPN_ID_List:
          AssertError(1 == 0, return false, "F1AP_ProtocolIE_ID_id_AvailableSNPN_ID_List is not supported");
          break;
        default:
          AssertError(1 == 0, return false, "F1AP_ProtocolIE_ID_id %ld unknown\n", cells_to_be_activated_list_itemExtIEs->id);
          break;
      }
    }
  }
  return true;
}

/* ====================================
 *          F1 SETUP REQUEST
 * ==================================== */

/**
 * @brief F1 SETUP REQUEST encoding (9.2.1.4 of 3GPP TS 38.473)
 *        gNB-DU → gNB-CU
 */
F1AP_F1AP_PDU_t *encode_f1ap_setup_request(const f1ap_setup_req_t *msg)
{
  F1AP_F1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));

  /* Create */
  /* 0. pdu Type */
  pdu->present = F1AP_F1AP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu->choice.initiatingMessage, initMsg);
  initMsg->procedureCode = F1AP_ProcedureCode_id_F1Setup;
  initMsg->criticality = F1AP_Criticality_reject;
  initMsg->value.present = F1AP_InitiatingMessage__value_PR_F1SetupRequest;
  F1AP_F1SetupRequest_t *f1Setup = &initMsg->value.choice.F1SetupRequest;
  // Transaction ID (M)
  asn1cSequenceAdd(f1Setup->protocolIEs.list, F1AP_F1SetupRequestIEs_t, ieC1);
  ieC1->id = F1AP_ProtocolIE_ID_id_TransactionID;
  ieC1->criticality = F1AP_Criticality_reject;
  ieC1->value.present = F1AP_F1SetupRequestIEs__value_PR_TransactionID;
  ieC1->value.choice.TransactionID = msg->transaction_id;
  // gNB-DU ID (M)
  asn1cSequenceAdd(f1Setup->protocolIEs.list, F1AP_F1SetupRequestIEs_t, ieC2);
  ieC2->id = F1AP_ProtocolIE_ID_id_gNB_DU_ID;
  ieC2->criticality = F1AP_Criticality_reject;
  ieC2->value.present = F1AP_F1SetupRequestIEs__value_PR_GNB_DU_ID;
  asn_int642INTEGER(&ieC2->value.choice.GNB_DU_ID, msg->gNB_DU_id);
  // gNB-DU Name (O)
  if (msg->gNB_DU_name) {
    asn1cSequenceAdd(f1Setup->protocolIEs.list, F1AP_F1SetupRequestIEs_t, ieC3);
    ieC3->id = F1AP_ProtocolIE_ID_id_gNB_DU_Name;
    ieC3->criticality = F1AP_Criticality_ignore;
    ieC3->value.present = F1AP_F1SetupRequestIEs__value_PR_GNB_DU_Name;
    OCTET_STRING_fromBuf(&ieC3->value.choice.GNB_DU_Name, msg->gNB_DU_name, strlen(msg->gNB_DU_name));
  }
  /// gNB-DU Served Cells List (0..1)
  asn1cSequenceAdd(f1Setup->protocolIEs.list, F1AP_F1SetupRequestIEs_t, ieCells);
  ieCells->id = F1AP_ProtocolIE_ID_id_gNB_DU_Served_Cells_List;
  ieCells->criticality = F1AP_Criticality_reject;
  ieCells->value.present = F1AP_F1SetupRequestIEs__value_PR_GNB_DU_Served_Cells_List;
  for (int i = 0; i < msg->num_cells_available; i++) {
    // gNB-DU Served Cells Item (M)
    const f1ap_served_cell_info_t *cell = &msg->cell[i].info;
    const f1ap_gnb_du_system_info_t *sys_info = msg->cell[i].sys_info;
    asn1cSequenceAdd(ieCells->value.choice.GNB_DU_Served_Cells_List.list, F1AP_GNB_DU_Served_Cells_ItemIEs_t, duServedCell);
    duServedCell->id = F1AP_ProtocolIE_ID_id_GNB_DU_Served_Cells_Item;
    duServedCell->criticality = F1AP_Criticality_reject;
    duServedCell->value.present = F1AP_GNB_DU_Served_Cells_ItemIEs__value_PR_GNB_DU_Served_Cells_Item;
    F1AP_GNB_DU_Served_Cells_Item_t *scell_item = &duServedCell->value.choice.GNB_DU_Served_Cells_Item;
    scell_item->served_Cell_Information = encode_served_cell_info(cell);
    scell_item->gNB_DU_System_Information = encode_system_info(sys_info);
  }
  // gNB-DU RRC version (M)
  asn1cSequenceAdd(f1Setup->protocolIEs.list, F1AP_F1SetupRequestIEs_t, ie2);
  ie2->id = F1AP_ProtocolIE_ID_id_GNB_DU_RRC_Version;
  ie2->criticality = F1AP_Criticality_reject;
  ie2->value.present = F1AP_F1SetupRequestIEs__value_PR_RRC_Version;
  // RRC Version: "This IE is not used in this release."
  // we put one bit for each byte in rrc_ver that is != 0
  uint8_t bits = 0;
  for (int i = 0; i < sizeofArray(msg->rrc_ver); ++i)
    bits |= (msg->rrc_ver[i] != 0) << i;
  BIT_STRING_t *bs = &ie2->value.choice.RRC_Version.latest_RRC_Version;
  bs->buf = calloc_or_fail(1, sizeof(*bs->buf));
  bs->buf[0] = bits;
  bs->size = 1;
  bs->bits_unused = 5;

  F1AP_ProtocolExtensionContainer_10696P228_t *p = calloc_or_fail(1, sizeof(*p));
  asn1cSequenceAdd(p->list, F1AP_RRC_Version_ExtIEs_t, rrcv_ext);
  rrcv_ext->id = F1AP_ProtocolIE_ID_id_latest_RRC_Version_Enhanced;
  rrcv_ext->criticality = F1AP_Criticality_ignore;
  rrcv_ext->extensionValue.present = F1AP_RRC_Version_ExtIEs__extensionValue_PR_OCTET_STRING_SIZE_3_;
  OCTET_STRING_t *os = &rrcv_ext->extensionValue.choice.OCTET_STRING_SIZE_3_;
  os->size = sizeofArray(msg->rrc_ver);
  os->buf = malloc_or_fail(sizeof(msg->rrc_ver));
  for (int i = 0; i < sizeofArray(msg->rrc_ver); ++i)
    os->buf[i] = msg->rrc_ver[i];
  ie2->value.choice.RRC_Version.iE_Extensions = (struct F1AP_ProtocolExtensionContainer *)p;
  return pdu;
}

/**
 * @brief F1AP Setup Request decoding
 */
bool decode_f1ap_setup_request(const F1AP_F1AP_PDU_t *pdu, f1ap_setup_req_t *out)
{
  /* Check presence of message type */
  _F1_EQ_CHECK_INT(pdu->present, F1AP_F1AP_PDU_PR_initiatingMessage);
  AssertError(pdu->choice.initiatingMessage != NULL, return false, "pdu->choice.initiatingMessage is NULL");
  _F1_EQ_CHECK_LONG(pdu->choice.initiatingMessage->procedureCode, F1AP_ProcedureCode_id_F1Setup);
  _F1_EQ_CHECK_INT(pdu->choice.initiatingMessage->value.present, F1AP_InitiatingMessage__value_PR_F1SetupRequest);
  /* Check presence of mandatory IEs */
  F1AP_F1SetupRequest_t *in = &pdu->choice.initiatingMessage->value.choice.F1SetupRequest;
  F1AP_F1SetupRequestIEs_t *ie;
  F1AP_LIB_FIND_IE(F1AP_F1SetupRequestIEs_t, ie, in, F1AP_ProtocolIE_ID_id_TransactionID, true);
  F1AP_LIB_FIND_IE(F1AP_F1SetupRequestIEs_t, ie, in, F1AP_ProtocolIE_ID_id_gNB_DU_ID, true);
  F1AP_LIB_FIND_IE(F1AP_F1SetupRequestIEs_t, ie, in, F1AP_ProtocolIE_ID_id_GNB_DU_RRC_Version, true);
  /* Loop over all IEs */
  for (int i = 0; i < in->protocolIEs.list.count; i++) {
    AssertError(in->protocolIEs.list.array[i] != NULL, return false, "in->protocolIEs.list.array[i] is NULL");
    ie = in->protocolIEs.list.array[i];
    switch (ie->id) {
      case F1AP_ProtocolIE_ID_id_TransactionID:
        // Transaction ID (M)
        _F1_EQ_CHECK_INT(ie->value.present, F1AP_F1SetupRequestIEs__value_PR_TransactionID);
        AssertError(ie->value.choice.TransactionID != -1, return false, "ie->value.choice.TransactionID is -1");
        out->transaction_id = ie->value.choice.TransactionID;
        break;
      case F1AP_ProtocolIE_ID_id_gNB_DU_ID:
        // gNB-DU ID (M)
        _F1_EQ_CHECK_INT(ie->value.present, F1AP_F1SetupRequestIEs__value_PR_GNB_DU_ID);
        asn_INTEGER2ulong(&ie->value.choice.GNB_DU_ID, &out->gNB_DU_id);
        break;
      case F1AP_ProtocolIE_ID_id_gNB_DU_Name: {
        const F1AP_GNB_DU_Name_t *du_name = &ie->value.choice.GNB_DU_Name;
        out->gNB_DU_name = calloc_or_fail(du_name->size + 1, sizeof(*out->gNB_DU_name));
        strncpy(out->gNB_DU_name, (char *)du_name->buf, du_name->size);
        break;
      }
      case F1AP_ProtocolIE_ID_id_gNB_DU_Served_Cells_List:
        /* GNB_DU_Served_Cells_List */
        out->num_cells_available = ie->value.choice.GNB_DU_Served_Cells_List.list.count;
        AssertError(out->num_cells_available > 0, return false, "at least 1 cell must be present");
        /* Loop over gNB-DU Served Cells Items */
        for (int i = 0; i < out->num_cells_available; i++) {
          F1AP_GNB_DU_Served_Cells_Item_t *served_cells_item =
              &(((F1AP_GNB_DU_Served_Cells_ItemIEs_t *)ie->value.choice.GNB_DU_Served_Cells_List.list.array[i])
                    ->value.choice.GNB_DU_Served_Cells_Item);
          // gNB-DU System Information (O)
          struct F1AP_GNB_DU_System_Information *DUsi = served_cells_item->gNB_DU_System_Information;
          if (DUsi != NULL) {
            // System Information
            out->cell[i].sys_info = calloc_or_fail(1, sizeof(*out->cell[i].sys_info));
            f1ap_gnb_du_system_info_t *sys_info = out->cell[i].sys_info;
            /* mib */
            sys_info->mib = cp_octet_string(&DUsi->mIB_message, &sys_info->mib_length);
            /* sib1 */
            sys_info->sib1 = cp_octet_string(&DUsi->sIB1_message, &sys_info->sib1_length);
          }
          /* Served Cell Information (M) */
          if (!decode_served_cell_info(&served_cells_item->served_Cell_Information, &out->cell[i].info))
            return false;
        }
        break;
      case F1AP_ProtocolIE_ID_id_GNB_DU_RRC_Version:
        /* gNB-DU RRC version (M) */
        if (ie->value.choice.RRC_Version.iE_Extensions) {
          F1AP_ProtocolExtensionContainer_10696P228_t *ext =
              (F1AP_ProtocolExtensionContainer_10696P228_t *)ie->value.choice.RRC_Version.iE_Extensions;
          if (ext->list.count > 0) {
            F1AP_RRC_Version_ExtIEs_t *rrcext = ext->list.array[0];
            OCTET_STRING_t *os = &rrcext->extensionValue.choice.OCTET_STRING_SIZE_3_;
            DevAssert(os->size == sizeofArray(out->rrc_ver));
            for (int i = 0; i < os->size; ++i)
              out->rrc_ver[i] = os->buf[i];
          }
        }
        break;
      default:
        AssertError(1 == 0, return false, "F1AP_ProtocolIE_ID_id %ld unknown\n", ie->id);
        break;
    }
  }
  return true;
}

static void copy_f1ap_served_cell_info(f1ap_served_cell_info_t *dest, const f1ap_served_cell_info_t *src) {
  *dest = *src;
  dest->mode = src->mode;
  dest->tdd = src->tdd;
  dest->fdd = src->fdd;
  dest->plmn = src->plmn;
  if (src->tac) {
    dest->tac = malloc_or_fail(sizeof(*dest->tac));
    *dest->tac = *src->tac;
  }
  if (src->measurement_timing_config_len) {
    dest->measurement_timing_config = calloc_or_fail(src->measurement_timing_config_len, sizeof(*dest->measurement_timing_config));
    memcpy(dest->measurement_timing_config, src->measurement_timing_config, src->measurement_timing_config_len);
  }
}

/**
 * @brief F1AP Setup Request deep copy
 */
f1ap_setup_req_t cp_f1ap_setup_request(const f1ap_setup_req_t *msg)
{
  f1ap_setup_req_t cp = {0};
  /* gNB_DU_id */
  cp.gNB_DU_id = msg->gNB_DU_id;
  /* gNB_DU_name */
  if (msg->gNB_DU_name != NULL)
    cp.gNB_DU_name = strdup(msg->gNB_DU_name);
  /* transaction_id */
  cp.transaction_id = msg->transaction_id;
  /* num_cells_available */
  cp.num_cells_available = msg->num_cells_available;
  for (int n = 0; n < msg->num_cells_available; n++) {
    /* cell.info */
    f1ap_served_cell_info_t *sci = &cp.cell[n].info;
    const f1ap_served_cell_info_t *msg_sci = &msg->cell[n].info;
    copy_f1ap_served_cell_info(sci, msg_sci);
    /* cell.sys_info */
    if (msg->cell[n].sys_info) {
      f1ap_gnb_du_system_info_t *orig_sys_info = msg->cell[n].sys_info;
      f1ap_gnb_du_system_info_t *copy_sys_info = calloc_or_fail(1, sizeof(*copy_sys_info));
      cp.cell[n].sys_info = copy_sys_info;
      if (orig_sys_info->mib_length > 0) {
        copy_sys_info->mib = calloc_or_fail(orig_sys_info->mib_length, sizeof(*copy_sys_info->mib));
        copy_sys_info->mib_length = orig_sys_info->mib_length;
        memcpy(copy_sys_info->mib, orig_sys_info->mib, copy_sys_info->mib_length);
      }
      if (orig_sys_info->sib1_length > 0) {
        copy_sys_info->sib1 = calloc_or_fail(orig_sys_info->sib1_length, sizeof(*copy_sys_info->sib1));
        copy_sys_info->sib1_length = orig_sys_info->sib1_length;
        memcpy(copy_sys_info->sib1, orig_sys_info->sib1, copy_sys_info->sib1_length);
      }
    }
  }
  for (int i = 0; i < sizeofArray(msg->rrc_ver); i++)
    cp.rrc_ver[i] = msg->rrc_ver[i];
  return cp;
}

/**
 * @brief F1AP Setup Request equality check
 */
bool eq_f1ap_setup_request(const f1ap_setup_req_t *a, const f1ap_setup_req_t *b)
{
  _F1_EQ_CHECK_LONG(a->gNB_DU_id, b->gNB_DU_id);
  _F1_EQ_CHECK_STR(a->gNB_DU_name, b->gNB_DU_name);
  _F1_EQ_CHECK_LONG(a->transaction_id, b->transaction_id);
  _F1_EQ_CHECK_INT(a->num_cells_available, b->num_cells_available);
  for (int i = 0; i < a->num_cells_available; i++) {
    if (!eq_f1ap_cell_info(&a->cell[i].info, &b->cell[i].info))
      return false;
    if (!eq_f1ap_sys_info(a->cell[i].sys_info, b->cell[i].sys_info))
      return false;
  }
  _F1_EQ_CHECK_LONG(sizeofArray(a->rrc_ver), sizeofArray(b->rrc_ver));
  for (int i = 0; i < sizeofArray(a->rrc_ver); i++) {
    _F1_EQ_CHECK_INT(a->rrc_ver[i], b->rrc_ver[i]);
  }
  return true;
}

/**
 * @brief F1AP Setup Request memory management
 */
void free_f1ap_setup_request(const f1ap_setup_req_t *msg)
{
  DevAssert(msg != NULL);
  free(msg->gNB_DU_name);
  for (int i = 0; i < msg->num_cells_available; i++) {
    free_f1ap_cell(&msg->cell[i].info, msg->cell[i].sys_info);
  }
}

/* ====================================
 *          F1 Setup Response
 * ==================================== */

/**
 * @brief F1 Setup Response encoding (9.2.1.5 of 3GPP TS 38.473)
 *        gNB-CU → gNB-DU
 */
F1AP_F1AP_PDU_t *encode_f1ap_setup_response(const f1ap_setup_resp_t *msg)
{
  F1AP_F1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));

  /* Create */
  /* 0. Message Type */
  pdu->present = F1AP_F1AP_PDU_PR_successfulOutcome;
  asn1cCalloc(pdu->choice.successfulOutcome, tmp);
  tmp->procedureCode = F1AP_ProcedureCode_id_F1Setup;
  tmp->criticality = F1AP_Criticality_reject;
  tmp->value.present = F1AP_SuccessfulOutcome__value_PR_F1SetupResponse;
  F1AP_F1SetupResponse_t *out = &pdu->choice.successfulOutcome->value.choice.F1SetupResponse;
  // Transaction ID (M)
  asn1cSequenceAdd(out->protocolIEs.list, F1AP_F1SetupResponseIEs_t, ie1);
  ie1->id = F1AP_ProtocolIE_ID_id_TransactionID;
  ie1->criticality = F1AP_Criticality_reject;
  ie1->value.present = F1AP_F1SetupResponseIEs__value_PR_TransactionID;
  ie1->value.choice.TransactionID = msg->transaction_id;
  // gNB-CU Name (O)
  if (msg->gNB_CU_name != NULL) {
    asn1cSequenceAdd(out->protocolIEs.list, F1AP_F1SetupResponseIEs_t, ie2);
    ie2->id = F1AP_ProtocolIE_ID_id_gNB_CU_Name;
    ie2->criticality = F1AP_Criticality_ignore;
    ie2->value.present = F1AP_F1SetupResponseIEs__value_PR_GNB_CU_Name;
    OCTET_STRING_fromBuf(&ie2->value.choice.GNB_CU_Name, msg->gNB_CU_name, strlen(msg->gNB_CU_name));
  }
  // Cells to be Activated List (O)
  for (int i = 0; i < msg->num_cells_to_activate; i++) {
    asn1cSequenceAdd(out->protocolIEs.list, F1AP_F1SetupResponseIEs_t, ie3);
    ie3->id = F1AP_ProtocolIE_ID_id_Cells_to_be_Activated_List;
    ie3->criticality = F1AP_Criticality_reject;
    ie3->value.present = F1AP_F1SetupResponseIEs__value_PR_Cells_to_be_Activated_List;
    asn1cSequenceAdd(ie3->value.choice.Cells_to_be_Activated_List.list,
                     F1AP_Cells_to_be_Activated_List_ItemIEs_t,
                     cells_to_be_activated_ies);
    encode_cells_to_activate(&msg->cells_to_activate[i], cells_to_be_activated_ies);
  }
  // gNB-CU RRC version (M)
  asn1cSequenceAdd(out->protocolIEs.list, F1AP_F1SetupResponseIEs_t, ie4);
  ie4->id = F1AP_ProtocolIE_ID_id_GNB_CU_RRC_Version;
  ie4->criticality = F1AP_Criticality_reject;
  ie4->value.present = F1AP_F1SetupResponseIEs__value_PR_RRC_Version;
  // RRC Version: "This IE is not used in this release."
  // we put one bit for each byte in rrc_ver that is != 0
  uint8_t bits = 0;
  for (int i = 0; i < sizeofArray(msg->rrc_ver); ++i)
    bits |= (msg->rrc_ver[i] != 0) << i;
  BIT_STRING_t *bs = &ie4->value.choice.RRC_Version.latest_RRC_Version;
  bs->buf = calloc_or_fail(1, sizeof(*bs->buf));
  bs->buf[0] = bits;
  bs->size = 1;
  bs->bits_unused = 5;

  F1AP_ProtocolExtensionContainer_10696P228_t *p = calloc_or_fail(1, sizeof(*p));
  asn1cSequenceAdd(p->list, F1AP_RRC_Version_ExtIEs_t, rrcv_ext);
  rrcv_ext->id = F1AP_ProtocolIE_ID_id_latest_RRC_Version_Enhanced;
  rrcv_ext->criticality = F1AP_Criticality_ignore;
  rrcv_ext->extensionValue.present = F1AP_RRC_Version_ExtIEs__extensionValue_PR_OCTET_STRING_SIZE_3_;
  OCTET_STRING_t *os = &rrcv_ext->extensionValue.choice.OCTET_STRING_SIZE_3_;
  os->size = sizeofArray(msg->rrc_ver);
  os->buf = malloc_or_fail(sizeof(msg->rrc_ver));
  for (int i = 0; i < sizeofArray(msg->rrc_ver); ++i)
    os->buf[i] = msg->rrc_ver[i];
  ie4->value.choice.RRC_Version.iE_Extensions = (struct F1AP_ProtocolExtensionContainer *)p;

  return pdu;
}

/**
 * @brief F1 Setup Response decoding (9.2.1.5 of 3GPP TS 38.473)
 *        gNB-CU → gNB-DU
 */
bool decode_f1ap_setup_response(const F1AP_F1AP_PDU_t *pdu, f1ap_setup_resp_t *out)
{
  /* Check presence of message type */
  _F1_EQ_CHECK_INT(pdu->present, F1AP_F1AP_PDU_PR_successfulOutcome);
  _F1_EQ_CHECK_LONG(pdu->choice.successfulOutcome->procedureCode, F1AP_ProcedureCode_id_F1Setup);
  _F1_EQ_CHECK_INT(pdu->choice.successfulOutcome->value.present, F1AP_SuccessfulOutcome__value_PR_F1SetupResponse);
  /* Check presence of mandatory IEs */
  F1AP_F1SetupResponse_t *in = &pdu->choice.successfulOutcome->value.choice.F1SetupResponse;
  F1AP_F1SetupResponseIEs_t *ie;
  F1AP_LIB_FIND_IE(F1AP_F1SetupResponseIEs_t, ie, in, F1AP_ProtocolIE_ID_id_TransactionID, true);
  F1AP_LIB_FIND_IE(F1AP_F1SetupResponseIEs_t, ie, in, F1AP_ProtocolIE_ID_id_GNB_CU_RRC_Version, true);
  /* Loop over all IEs */
  for (int i = 0; i < in->protocolIEs.list.count; i++) {
    ie = in->protocolIEs.list.array[i];
    switch (ie->id) {
      case F1AP_ProtocolIE_ID_id_TransactionID:
      // Transaction ID (M)
        _F1_EQ_CHECK_INT(ie->value.present, F1AP_F1SetupResponseIEs__value_PR_TransactionID);
        AssertError(ie->value.choice.TransactionID != -1, return false, "ie->value.choice.TransactionID is -1");
        out->transaction_id = ie->value.choice.TransactionID;
        break;

      case F1AP_ProtocolIE_ID_id_gNB_CU_Name: {
        // gNB-CU Name (O)
        _F1_EQ_CHECK_INT(ie->value.present, F1AP_F1SetupResponseIEs__value_PR_GNB_CU_Name);
        const F1AP_GNB_CU_Name_t *cu_name = &ie->value.choice.GNB_CU_Name;
        out->gNB_CU_name = calloc_or_fail(cu_name->size + 1, sizeof(*out->gNB_CU_name));
        strncpy(out->gNB_CU_name, (char *)cu_name->buf, cu_name->size);
        } break;

      case F1AP_ProtocolIE_ID_id_GNB_CU_RRC_Version:
      // gNB-CU RRC version (M)
        _F1_EQ_CHECK_INT(ie->value.present, F1AP_F1SetupResponseIEs__value_PR_RRC_Version);
        // RRC Version: "This IE is not used in this release."
        if (ie->value.choice.RRC_Version.iE_Extensions) {
          F1AP_ProtocolExtensionContainer_10696P228_t *ext =
              (F1AP_ProtocolExtensionContainer_10696P228_t *)ie->value.choice.RRC_Version.iE_Extensions;
          if (ext->list.count > 0) {
            F1AP_RRC_Version_ExtIEs_t *rrcext = ext->list.array[0];
            OCTET_STRING_t *os = &rrcext->extensionValue.choice.OCTET_STRING_SIZE_3_;
            for (int i = 0; i < sizeofArray(out->rrc_ver); i++)
              out->rrc_ver[i] = os->buf[i];
          }
        }
        break;

      case F1AP_ProtocolIE_ID_id_Cells_to_be_Activated_List: {
        // Cells to be Activated List (O)
        if (ie->value.present != F1AP_F1SetupResponseIEs__value_PR_Cells_to_be_Activated_List)
          break;
        struct F1AP_Cells_to_be_Activated_List *Cells_to_be_Activated_List = &ie->value.choice.Cells_to_be_Activated_List;
        out->num_cells_to_activate = Cells_to_be_Activated_List->list.count;
        // Loop Cells to be Activated List Items (count >= 1)
        AssertError(out->num_cells_to_activate > 0, return false, "At least 1 cell must be present");
        for (int i = 0; i < out->num_cells_to_activate; i++) {
          const F1AP_Cells_to_be_Activated_List_ItemIEs_t *itemIEs
            = (F1AP_Cells_to_be_Activated_List_ItemIEs_t *)Cells_to_be_Activated_List->list.array[i];
          if (!decode_cells_to_activate(&out->cells_to_activate[i], itemIEs))
            return false;
        }
        break;
      }
      default:
        AssertError(1 == 0, return false, "F1AP_ProtocolIE_ID_id %ld unknown\n", ie->id);
        break;
    }
  }

  return true;
}

/**
 * @brief F1AP Setup Response equality check
 */
bool eq_f1ap_setup_response(const f1ap_setup_resp_t *a, const f1ap_setup_resp_t *b)
{
  _F1_EQ_CHECK_STR(a->gNB_CU_name, b->gNB_CU_name);
  _F1_EQ_CHECK_INT(a->num_cells_to_activate, b->num_cells_to_activate);
  _F1_EQ_CHECK_LONG(a->transaction_id, b->transaction_id);
  if (a->num_cells_to_activate) {
    for (int i = 0; i < a->num_cells_to_activate; i++) {
      const served_cells_to_activate_t *a_cell = &a->cells_to_activate[i];
      const served_cells_to_activate_t *b_cell = &b->cells_to_activate[i];
      _F1_EQ_CHECK_LONG(a_cell->nr_cellid, b_cell->nr_cellid);
      _F1_EQ_CHECK_INT(a_cell->nrpci, b_cell->nrpci);
      _F1_EQ_CHECK_INT(a_cell->num_SI, b_cell->num_SI);
      _F1_EQ_CHECK_LONG(a_cell->nr_cellid, b_cell->nr_cellid);
      if (!eq_f1ap_plmn(&a_cell->plmn, &b_cell->plmn))
        return false;
      if (sizeofArray(a->cells_to_activate[i].SI_msg) != sizeofArray(b->cells_to_activate[i].SI_msg))
        return false;
      for (int j = 0; j < b->cells_to_activate[i].num_SI; j++) {
        const f1ap_sib_msg_t *a_SI_msg = &a->cells_to_activate[i].SI_msg[j];
        const f1ap_sib_msg_t *b_SI_msg = &b->cells_to_activate[i].SI_msg[j];
        _F1_EQ_CHECK_INT(*a_SI_msg->SI_container, *b_SI_msg->SI_container);
        _F1_EQ_CHECK_INT(a_SI_msg->SI_container_length, b_SI_msg->SI_container_length);
        _F1_EQ_CHECK_INT(a_SI_msg->SI_type, b_SI_msg->SI_type);
      }
    }
  }
  _F1_EQ_CHECK_LONG(sizeofArray(a->rrc_ver), sizeofArray(b->rrc_ver));
  for (int i = 0; i < sizeofArray(a->rrc_ver); i++) {
    _F1_EQ_CHECK_INT(a->rrc_ver[i], b->rrc_ver[i]);
  }
  return true;
}

/**
 * @brief F1AP Setup Response deep copy
 */
f1ap_setup_resp_t cp_f1ap_setup_response(const f1ap_setup_resp_t *msg)
{
  f1ap_setup_resp_t cp = {0};
  /* gNB_CU_name */
  if (msg->gNB_CU_name != NULL)
    cp.gNB_CU_name = strdup(msg->gNB_CU_name);
  /* transaction_id */
  cp.transaction_id = msg->transaction_id;
  /* num_cells_available */
  cp.num_cells_to_activate = msg->num_cells_to_activate;
  for (int n = 0; n < msg->num_cells_to_activate; n++) {
    /* cell.info */
    served_cells_to_activate_t *cp_cell = &cp.cells_to_activate[n];
    const served_cells_to_activate_t *msg_cell = &msg->cells_to_activate[n];
    cp_cell->nr_cellid = msg_cell->nr_cellid;
    cp_cell->nrpci = msg_cell->nrpci;
    cp_cell->num_SI = msg_cell->num_SI;
    cp_cell->plmn = msg_cell->plmn;
    for (int j = 0; j < cp_cell->num_SI; j++) {
      f1ap_sib_msg_t *cp_SI_msg = &cp_cell->SI_msg[j];
      const f1ap_sib_msg_t *b_SI_msg = &msg_cell->SI_msg[j];
      cp_SI_msg->SI_container_length = b_SI_msg->SI_container_length;
      cp_SI_msg->SI_container = malloc_or_fail(cp_SI_msg->SI_container_length);
      *cp_SI_msg->SI_container = *b_SI_msg->SI_container;
      cp_SI_msg->SI_type = b_SI_msg->SI_type;
    }
  }
  for (int i = 0; i < sizeofArray(msg->rrc_ver); i++)
    cp.rrc_ver[i] = msg->rrc_ver[i];
  return cp;
}

/**
 * @brief F1AP Setup Response memory management
 */
void free_f1ap_setup_response(const f1ap_setup_resp_t *msg)
{
  DevAssert(msg != NULL);
  free(msg->gNB_CU_name);
  for (int i = 0; i < msg->num_cells_to_activate; i++)
    for (int j = 0; j < msg->cells_to_activate[i].num_SI; j++)
      if (msg->cells_to_activate[i].SI_msg[j].SI_container_length > 0)
        free(msg->cells_to_activate[i].SI_msg[j].SI_container);
}

/* ====================================
 *          F1AP Setup Failure
 * ==================================== */

/**
 * @brief F1AP Setup Failure encoding
 */
F1AP_F1AP_PDU_t *encode_f1ap_setup_failure(const f1ap_setup_failure_t *msg)
{
  F1AP_F1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));
  /* Create */
  /* 0. Message Type */
  asn1cCalloc(pdu->choice.unsuccessfulOutcome, UnsuccessfulOutcome);
  pdu->present = F1AP_F1AP_PDU_PR_unsuccessfulOutcome;
  UnsuccessfulOutcome->procedureCode = F1AP_ProcedureCode_id_F1Setup;
  UnsuccessfulOutcome->criticality = F1AP_Criticality_reject;
  UnsuccessfulOutcome->value.present = F1AP_UnsuccessfulOutcome__value_PR_F1SetupFailure;
  F1AP_F1SetupFailure_t *out = &pdu->choice.unsuccessfulOutcome->value.choice.F1SetupFailure;
  /* mandatory */
  /* c1. Transaction ID (integer value)*/
  asn1cSequenceAdd(out->protocolIEs.list, F1AP_F1SetupFailureIEs_t, ie1);
  ie1->id = F1AP_ProtocolIE_ID_id_TransactionID;
  ie1->criticality = F1AP_Criticality_reject;
  ie1->value.present = F1AP_F1SetupFailureIEs__value_PR_TransactionID;
  ie1->value.choice.TransactionID = msg->transaction_id;
  /* mandatory */
  /* c2. Cause */
  asn1cSequenceAdd(out->protocolIEs.list, F1AP_F1SetupFailureIEs_t, ie2);
  ie2->id = F1AP_ProtocolIE_ID_id_Cause;
  ie2->criticality = F1AP_Criticality_ignore;
  ie2->value.present = F1AP_F1SetupFailureIEs__value_PR_Cause;
  ie2->value.choice.Cause.present = F1AP_Cause_PR_radioNetwork;
  ie2->value.choice.Cause.choice.radioNetwork = msg->cause;
  /* optional */
  /* c3. TimeToWait */
  if (msg->time_to_wait > 0) {
    asn1cSequenceAdd(out->protocolIEs.list, F1AP_F1SetupFailureIEs_t, ie3);
    ie3->id = F1AP_ProtocolIE_ID_id_TimeToWait;
    ie3->criticality = F1AP_Criticality_ignore;
    ie3->value.present = F1AP_F1SetupFailureIEs__value_PR_TimeToWait;
    ie3->value.choice.TimeToWait = F1AP_TimeToWait_v10s;
  }
  /* optional */
  /* c4. CriticalityDiagnostics*/
  if (msg->criticality_diagnostics) {
    asn1cSequenceAdd(out->protocolIEs.list, F1AP_F1SetupFailureIEs_t, ie4);
    ie4->id = F1AP_ProtocolIE_ID_id_CriticalityDiagnostics;
    ie4->criticality = F1AP_Criticality_ignore;
    ie4->value.present = F1AP_F1SetupFailureIEs__value_PR_CriticalityDiagnostics;
    asn1cCallocOne(ie4->value.choice.CriticalityDiagnostics.procedureCode, F1AP_ProcedureCode_id_UEContextSetup);
    asn1cCallocOne(ie4->value.choice.CriticalityDiagnostics.triggeringMessage, F1AP_TriggeringMessage_initiating_message);
    asn1cCallocOne(ie4->value.choice.CriticalityDiagnostics.procedureCriticality, F1AP_Criticality_reject);
    asn1cCallocOne(ie4->value.choice.CriticalityDiagnostics.transactionID, 0);
  }
  return pdu;
}

/**
 * @brief F1AP Setup Failure decoding
 */
bool decode_f1ap_setup_failure(const F1AP_F1AP_PDU_t *pdu, f1ap_setup_failure_t *out)
{
  F1AP_F1SetupFailureIEs_t *ie;
  F1AP_F1SetupFailure_t *in = &pdu->choice.unsuccessfulOutcome->value.choice.F1SetupFailure;
  /* Check presence of mandatory IEs */
  F1AP_LIB_FIND_IE(F1AP_F1SetupFailureIEs_t, ie, in, F1AP_ProtocolIE_ID_id_TransactionID, true);
  F1AP_LIB_FIND_IE(F1AP_F1SetupFailureIEs_t, ie, in, F1AP_ProtocolIE_ID_id_Cause, true);
  /* Loop over all IEs */
  for (int i = 0; i < in->protocolIEs.list.count; i++) {
    ie = in->protocolIEs.list.array[i];
    switch (ie->id) {
      case F1AP_ProtocolIE_ID_id_TransactionID:
        /* Transaction ID (M) */
        out->transaction_id = ie->value.choice.TransactionID;
        break;
      case F1AP_ProtocolIE_ID_id_Cause:
        /* Cause (M) */
        out->cause = ie->value.choice.Cause.choice.radioNetwork;
        break;
      case F1AP_ProtocolIE_ID_id_TimeToWait:
        out->time_to_wait = ie->value.choice.TimeToWait;
        break;
      case F1AP_ProtocolIE_ID_id_CriticalityDiagnostics:
        AssertError(1 == 0, return false, "F1AP_ProtocolIE_ID_id_CriticalityDiagnostics is not supported");
        break;
      default:
        AssertError(1 == 0, return false, "F1AP_ProtocolIE_ID_id %ld unknown\n", ie->id);
        break;
    }
  }
  return true;
}

/**
 * @brief F1AP Setup Failure equality check
 */
bool eq_f1ap_setup_failure(const f1ap_setup_failure_t *a, const f1ap_setup_failure_t *b)
{
  if (a->transaction_id != b->transaction_id)
    return false;
  return true;
}

/**
 * @brief F1AP Setup Failure deep copy
 */
f1ap_setup_failure_t cp_f1ap_setup_failure(const f1ap_setup_failure_t *msg)
{
  f1ap_setup_failure_t cp = {0};
  /* transaction_id */
  cp.transaction_id = msg->transaction_id;
  return cp;
}

/* ====================================
 *   F1AP gNB-DU Configuration Update
 * ==================================== */

/**
 * @brief F1 gNB-DU Configuration Update encoding (9.2.1.7 of 3GPP TS 38.473)
 */
F1AP_F1AP_PDU_t *encode_f1ap_du_configuration_update(const f1ap_gnb_du_configuration_update_t *msg)
{
  F1AP_F1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));
  /* Create */
  /* 0. Message Type */
  pdu->present = F1AP_F1AP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu->choice.initiatingMessage, initMsg);
  initMsg->procedureCode = F1AP_ProcedureCode_id_gNBDUConfigurationUpdate;
  initMsg->criticality   = F1AP_Criticality_reject;
  initMsg->value.present = F1AP_InitiatingMessage__value_PR_GNBDUConfigurationUpdate;
  F1AP_GNBDUConfigurationUpdate_t *out = &initMsg->value.choice.GNBDUConfigurationUpdate;
  /* mandatory */
  /* c1. Transaction ID (integer value) */
  asn1cSequenceAdd(out->protocolIEs.list, F1AP_GNBDUConfigurationUpdateIEs_t, ie1);
  ie1->id = F1AP_ProtocolIE_ID_id_TransactionID;
  ie1->criticality = F1AP_Criticality_reject;
  ie1->value.present = F1AP_GNBDUConfigurationUpdateIEs__value_PR_TransactionID;
  ie1->value.choice.TransactionID = msg->transaction_id;
  /* mandatory */
  /* c2. Served_Cells_To_Add */
  if (msg->num_cells_to_add > 0) {
    AssertFatal(false, "code for adding cells not tested\n");
    asn1cSequenceAdd(out->protocolIEs.list, F1AP_GNBDUConfigurationUpdateIEs_t, ie2);
    ie2->id = F1AP_ProtocolIE_ID_id_Served_Cells_To_Add_List;
    ie2->criticality = F1AP_Criticality_reject;
    ie2->value.present = F1AP_GNBDUConfigurationUpdateIEs__value_PR_Served_Cells_To_Add_List;

    for (int j = 0; j < msg->num_cells_to_add; j++) {
      const f1ap_served_cell_info_t *cell = &msg->cell_to_add[j].info;
      const f1ap_gnb_du_system_info_t *sys_info = msg->cell_to_add[j].sys_info;
      asn1cSequenceAdd(ie2->value.choice.Served_Cells_To_Add_List.list,
                       F1AP_Served_Cells_To_Add_ItemIEs_t,
                       served_cells_to_add_item_ies);
      served_cells_to_add_item_ies->id = F1AP_ProtocolIE_ID_id_Served_Cells_To_Add_Item;
      served_cells_to_add_item_ies->criticality = F1AP_Criticality_reject;
      served_cells_to_add_item_ies->value.present = F1AP_Served_Cells_To_Add_ItemIEs__value_PR_Served_Cells_To_Add_Item;
      F1AP_Served_Cells_To_Add_Item_t *served_cells_to_add_item =
          &served_cells_to_add_item_ies->value.choice.Served_Cells_To_Add_Item;
      served_cells_to_add_item->served_Cell_Information = encode_served_cell_info(cell);
      served_cells_to_add_item->gNB_DU_System_Information = encode_system_info(sys_info);
    }
  }

  /* mandatory */
  /* c3. Served_Cells_To_Modify */
  if (msg->num_cells_to_modify > 0) {
    asn1cSequenceAdd(out->protocolIEs.list, F1AP_GNBDUConfigurationUpdateIEs_t, ie3);
    ie3->id = F1AP_ProtocolIE_ID_id_Served_Cells_To_Modify_List;
    ie3->criticality = F1AP_Criticality_reject;
    ie3->value.present = F1AP_GNBDUConfigurationUpdateIEs__value_PR_Served_Cells_To_Modify_List;
    for (int i = 0; i < msg->num_cells_to_modify; i++) {
      const f1ap_served_cell_info_t *cell = &msg->cell_to_modify[i].info;
      const f1ap_gnb_du_system_info_t *sys_info = msg->cell_to_modify[i].sys_info;
      asn1cSequenceAdd(ie3->value.choice.Served_Cells_To_Modify_List.list,
                       F1AP_Served_Cells_To_Modify_ItemIEs_t,
                       served_cells_to_modify_item_ies);
      served_cells_to_modify_item_ies->id = F1AP_ProtocolIE_ID_id_Served_Cells_To_Modify_Item;
      served_cells_to_modify_item_ies->criticality = F1AP_Criticality_reject;
      served_cells_to_modify_item_ies->value.present = F1AP_Served_Cells_To_Modify_ItemIEs__value_PR_Served_Cells_To_Modify_Item;
      F1AP_Served_Cells_To_Modify_Item_t *served_cells_to_modify_item =
          &served_cells_to_modify_item_ies->value.choice.Served_Cells_To_Modify_Item;

      F1AP_NRCGI_t *oldNRCGI = &served_cells_to_modify_item->oldNRCGI;
      const f1ap_plmn_t *old_plmn = &msg->cell_to_modify[i].old_plmn;
      MCC_MNC_TO_PLMNID(old_plmn->mcc, old_plmn->mnc, old_plmn->mnc_digit_length, &oldNRCGI->pLMN_Identity);
      NR_CELL_ID_TO_BIT_STRING(msg->cell_to_modify[i].old_nr_cellid, &oldNRCGI->nRCellIdentity);

      served_cells_to_modify_item->served_Cell_Information = encode_served_cell_info(cell);
      served_cells_to_modify_item->gNB_DU_System_Information = encode_system_info(sys_info);
    }
  }

  /* mandatory */
  /* c4. Served_Cells_To_Delete */
  if (msg->num_cells_to_delete > 0) {
    asn1cSequenceAdd(out->protocolIEs.list, F1AP_GNBDUConfigurationUpdateIEs_t, ie4);
    ie4->id = F1AP_ProtocolIE_ID_id_Served_Cells_To_Delete_List;
    ie4->criticality = F1AP_Criticality_reject;
    ie4->value.present = F1AP_GNBDUConfigurationUpdateIEs__value_PR_Served_Cells_To_Delete_List;
    AssertFatal(msg->num_cells_to_delete == 0, "code for deleting cells not tested\n");
    for (int i = 0; i < msg->num_cells_to_delete; i++) {
      asn1cSequenceAdd(ie4->value.choice.Served_Cells_To_Delete_List.list,
                       F1AP_Served_Cells_To_Delete_ItemIEs_t,
                       served_cells_to_delete_item_ies);
      served_cells_to_delete_item_ies->id = F1AP_ProtocolIE_ID_id_Served_Cells_To_Delete_Item;
      served_cells_to_delete_item_ies->criticality = F1AP_Criticality_reject;
      served_cells_to_delete_item_ies->value.present = F1AP_Served_Cells_To_Delete_ItemIEs__value_PR_Served_Cells_To_Delete_Item;
      F1AP_Served_Cells_To_Delete_Item_t *served_cells_to_delete_item =
          &served_cells_to_delete_item_ies->value.choice.Served_Cells_To_Delete_Item;
      F1AP_NRCGI_t *oldNRCGI = &served_cells_to_delete_item->oldNRCGI;
      const f1ap_plmn_t *plmn = &msg->cell_to_delete[i].plmn;
      MCC_MNC_TO_PLMNID(plmn->mcc, plmn->mnc, plmn->mnc_digit_length, &(oldNRCGI->pLMN_Identity));
      NR_CELL_ID_TO_BIT_STRING(msg->cell_to_delete[i].nr_cellid, &(oldNRCGI->nRCellIdentity));
    }
  }

  /* optional */
  /* c5. GNB_DU_ID (integer value) */
  if (msg->gNB_DU_ID != NULL) {
    asn1cSequenceAdd(out->protocolIEs.list, F1AP_GNBDUConfigurationUpdateIEs_t, ie5);
    ie5->id = F1AP_ProtocolIE_ID_id_gNB_DU_ID;
    ie5->criticality = F1AP_Criticality_reject;
    ie5->value.present = F1AP_GNBDUConfigurationUpdateIEs__value_PR_GNB_DU_ID;
    asn_int642INTEGER(&ie5->value.choice.GNB_DU_ID, *msg->gNB_DU_ID);
  }

  return pdu;
}

/**
 * @brief F1 gNB-DU Configuration Update decoding (9.2.1.7 of 3GPP TS 38.473)
 */
bool decode_f1ap_du_configuration_update(const F1AP_F1AP_PDU_t *pdu, f1ap_gnb_du_configuration_update_t *out)
{
  /* Check presence of message type */
  _F1_EQ_CHECK_INT(pdu->present, F1AP_F1AP_PDU_PR_initiatingMessage);
  _F1_EQ_CHECK_LONG(pdu->choice.initiatingMessage->procedureCode, F1AP_ProcedureCode_id_gNBDUConfigurationUpdate);
  _F1_EQ_CHECK_INT(pdu->choice.initiatingMessage->value.present, F1AP_InitiatingMessage__value_PR_GNBDUConfigurationUpdate);
  /* Check presence of mandatory IEs */
  F1AP_GNBDUConfigurationUpdate_t *in = &pdu->choice.initiatingMessage->value.choice.GNBDUConfigurationUpdate;
  F1AP_GNBDUConfigurationUpdateIEs_t *ie;
  /* Check mandatory IEs */
  F1AP_LIB_FIND_IE(F1AP_GNBDUConfigurationUpdateIEs_t, ie, in, F1AP_ProtocolIE_ID_id_TransactionID, true);
  /* Loop over all IEs */
  for (int i = 0; i < in->protocolIEs.list.count; i++) {
    AssertError(in->protocolIEs.list.array[i] != NULL, return false, "in->protocolIEs.list.array[i] is NULL");
    ie = in->protocolIEs.list.array[i];
    switch (ie->id) {
      case F1AP_ProtocolIE_ID_id_TransactionID: {
        out->transaction_id = ie->value.choice.TransactionID;
      } break;
      case F1AP_ProtocolIE_ID_id_Served_Cells_To_Add_List: {
        /* Served Cells To Add List */
        AssertError(out->num_cells_to_add > 0, return false, "at least 1 cell to add shall to be present");
        out->num_cells_to_add = ie->value.choice.Served_Cells_To_Add_List.list.count;
        for (int i = 0; i < out->num_cells_to_add; i++) {
          F1AP_Served_Cells_To_Add_Item_t *served_cells_item =
              &((F1AP_Served_Cells_To_Add_ItemIEs_t *)ie->value.choice.Served_Cells_To_Add_List.list.array[i])
                   ->value.choice.Served_Cells_To_Add_Item;
          /* Served Cell Information (M) */
          if (!decode_served_cell_info(&served_cells_item->served_Cell_Information, &out->cell_to_add[i].info))
            return false;
          /* gNB-DU System Information */
          if (served_cells_item->gNB_DU_System_Information != NULL) {
            out->cell_to_add[i].sys_info = calloc_or_fail(1, sizeof(*out->cell_to_add[i].sys_info));
            decode_system_info(served_cells_item->gNB_DU_System_Information, out->cell_to_add[i].sys_info);
          }
        }
      } break;
      case F1AP_ProtocolIE_ID_id_Served_Cells_To_Modify_List: {
        /* Served Cells To Modify List (O) */
        out->num_cells_to_modify = ie->value.choice.Served_Cells_To_Modify_List.list.count;
        AssertError(out->num_cells_to_modify > 0, return false, "at least 1 cell to modify shall to be present");
        for (int i = 0; i < out->num_cells_to_modify; i++) {
          /* Served Cells To Modify List item (count >= 1) */
          F1AP_Served_Cells_To_Modify_Item_t *served_cells_item =
              &((F1AP_Served_Cells_To_Modify_ItemIEs_t *)ie->value.choice.Served_Cells_To_Modify_List.list.array[i])
                   ->value.choice.Served_Cells_To_Modify_Item;
          /* Old NR CGI (M) */
          F1AP_NRCGI_t *oldNRCGI = &served_cells_item->oldNRCGI;
          f1ap_plmn_t *old_plmn = &out->cell_to_modify[i].old_plmn;
          TBCD_TO_MCC_MNC(&(oldNRCGI->pLMN_Identity), old_plmn->mcc, old_plmn->mnc, old_plmn->mnc_digit_length);
          /* Old NR CGI Cell ID */
          BIT_STRING_TO_NR_CELL_IDENTITY(&oldNRCGI->nRCellIdentity, out->cell_to_modify[i].old_nr_cellid);
          /* Served Cell Information (M) */
          if (!decode_served_cell_info(&served_cells_item->served_Cell_Information, &out->cell_to_modify[i].info))
            return false;
          /* gNB-DU System Information (O) */
          if (served_cells_item->gNB_DU_System_Information != NULL) {
            out->cell_to_modify[i].sys_info = calloc_or_fail(1, sizeof(*out->cell_to_modify[i].sys_info));
            decode_system_info(served_cells_item->gNB_DU_System_Information, out->cell_to_modify[i].sys_info);
          }
        }
      } break;
      case F1AP_ProtocolIE_ID_id_Served_Cells_To_Delete_List: {
        /* Served Cells To Delete List */
        out->num_cells_to_delete = ie->value.choice.Served_Cells_To_Delete_List.list.count;
        AssertError(out->num_cells_to_delete > 0, return false, "at least 1 cell to delete shall to be present");
        for (int i = 0; i < out->num_cells_to_delete; i++) {
          F1AP_Served_Cells_To_Delete_Item_t *served_cells_item =
              &((F1AP_Served_Cells_To_Delete_ItemIEs_t *)ie->value.choice.Served_Cells_To_Delete_List.list.array[i])
                   ->value.choice.Served_Cells_To_Delete_Item;
          F1AP_NRCGI_t *oldNRCGI = &served_cells_item->oldNRCGI;
          f1ap_plmn_t *plmn = &out->cell_to_delete[i].plmn;
          /* Old NR CGI (M) */
          TBCD_TO_MCC_MNC(&(oldNRCGI->pLMN_Identity), plmn->mcc, plmn->mnc, plmn->mnc_digit_length);
          // NR cellID
          BIT_STRING_TO_NR_CELL_IDENTITY(&oldNRCGI->nRCellIdentity, out->cell_to_delete[i].nr_cellid);
        }
      } break;
      case F1AP_ProtocolIE_ID_id_Cells_Status_List:
        /* Cells Status List (O) */
        AssertError(1 == 0, return false, "F1AP_ProtocolIE_ID_id_Cells_Status_List is not supported");
        break;
      case F1AP_ProtocolIE_ID_id_Dedicated_SIDelivery_NeededUE_List:
        /* Dedicated SI Delivery Needed UE List (O) */
        AssertError(1 == 0, return false, "F1AP_ProtocolIE_ID_id_Dedicated_SIDelivery_NeededUE_List is not supported");
        break;
      case F1AP_ProtocolIE_ID_id_gNB_DU_ID:
        /* gNB-DU ID (O)*/
        asn_INTEGER2ulong(&ie->value.choice.GNB_DU_ID, out->gNB_DU_ID);
        break;
      case F1AP_ProtocolIE_ID_id_GNB_DU_TNL_Association_To_Remove_List:
        /* Cells Status List (O) */
        AssertError(1 == 0, return false, "F1AP_ProtocolIE_ID_id_GNB_DU_TNL_Association_To_Remove_List is not supported");
        break;
    }
  }
  return true;
}

void free_f1ap_du_configuration_update(const f1ap_gnb_du_configuration_update_t *msg)
{
  free(msg->gNB_DU_ID);
  for (int i = 0; i < msg->num_cells_to_add; i++) {
    free_f1ap_cell(&msg->cell_to_add[i].info, msg->cell_to_add[i].sys_info);
  }
  for (int i = 0; i < msg->num_cells_to_modify; i++) {
    free_f1ap_cell(&msg->cell_to_modify[i].info, msg->cell_to_modify[i].sys_info);
  }
}

/**
 * @brief F1 gNB-DU Configuration Update check
 */
bool eq_f1ap_du_configuration_update(const f1ap_gnb_du_configuration_update_t *a, const f1ap_gnb_du_configuration_update_t *b)
{
  if (a->gNB_DU_ID != NULL && b->gNB_DU_ID != NULL)
    _F1_EQ_CHECK_LONG(*a->gNB_DU_ID, *b->gNB_DU_ID);
  _F1_EQ_CHECK_LONG(a->transaction_id, b->transaction_id);
  /* to add */
  _F1_EQ_CHECK_INT(a->num_cells_to_add, b->num_cells_to_add);
  for (int i = 0; i < a->num_cells_to_add; i++) {
    if (!eq_f1ap_cell_info(&a->cell_to_add[i].info, &b->cell_to_add[i].info))
      return false;
    if (a->cell_to_add[i].sys_info && b->cell_to_add[i].sys_info) {
      if (!eq_f1ap_sys_info(a->cell_to_add[i].sys_info, b->cell_to_add[i].sys_info))
        return false;
    }
  }
  /* to delete */
  _F1_EQ_CHECK_INT(a->num_cells_to_delete, b->num_cells_to_delete);
  for (int i = 0; i < a->num_cells_to_delete; i++) {
    _F1_EQ_CHECK_LONG(a->cell_to_delete[i].nr_cellid, b->cell_to_delete[i].nr_cellid);
    if (!eq_f1ap_plmn(&a->cell_to_delete[i].plmn, &b->cell_to_delete[i].plmn))
      return false;
  }
  /* to modify */
  _F1_EQ_CHECK_INT(a->num_cells_to_modify, b->num_cells_to_modify);
  for (int i = 0; i < a->num_cells_to_modify; i++) {
    if (!eq_f1ap_cell_info(&a->cell_to_modify[i].info, &b->cell_to_modify[i].info))
      return false;
    if (a->cell_to_modify[i].sys_info && b->cell_to_modify[i].sys_info) {
      if (!eq_f1ap_sys_info(a->cell_to_modify[i].sys_info, b->cell_to_modify[i].sys_info))
        return false;
    }
  }
  return true;
}

/**
 * @brief F1 gNB-DU Configuration Update deep copy
 */
f1ap_gnb_du_configuration_update_t cp_f1ap_du_configuration_update(const f1ap_gnb_du_configuration_update_t *msg)
{
  f1ap_gnb_du_configuration_update_t cp = {0};
  /* gNB_DU_ID */
  if (msg->gNB_DU_ID != NULL) {
    cp.gNB_DU_ID = calloc_or_fail(1, sizeof(*cp.gNB_DU_ID));
    *cp.gNB_DU_ID = *msg->gNB_DU_ID;
  }
  /* transaction_id */
  cp.transaction_id = msg->transaction_id;
  /* to add */
  cp.num_cells_to_add = msg->num_cells_to_add;
  /* to delete */
  cp.num_cells_to_delete = msg->num_cells_to_delete;
  for (int i = 0; i < cp.num_cells_to_delete; i++) {
    cp.cell_to_delete[i].nr_cellid = msg->cell_to_delete[i].nr_cellid;
    cp.cell_to_delete[i].plmn = msg->cell_to_delete[i].plmn;
  }
  /* to modify */
  cp.num_cells_to_modify = msg->num_cells_to_modify;
  for (int i = 0; i < cp.num_cells_to_modify; i++) {
    cp.cell_to_modify[i].info = msg->cell_to_modify[i].info;
    f1ap_served_cell_info_t *info = &cp.cell_to_modify[i].info;
    if (info->measurement_timing_config_len > 0) {
      info->measurement_timing_config = malloc_or_fail(info->measurement_timing_config_len * sizeof(*info->measurement_timing_config));
      for (int j = 0; j < info->measurement_timing_config_len; j++)
        info->measurement_timing_config[j] = msg->cell_to_modify[i].info.measurement_timing_config[j];
    }
    /* TAC */
    info->tac = calloc_or_fail(1, sizeof(*info->tac));
    *info->tac = *msg->cell_to_modify[i].info.tac;
    /* System information */
    cp.cell_to_modify[i].sys_info = malloc_or_fail(sizeof(*cp.cell_to_modify[i].sys_info));
    f1ap_gnb_du_system_info_t *sys_info = cp.cell_to_modify[i].sys_info;
    if (msg->cell_to_modify[i].sys_info->mib_length > 0) {
      sys_info->mib_length = msg->cell_to_modify[i].sys_info->mib_length;
      sys_info->mib = calloc_or_fail(msg->cell_to_modify[i].sys_info->mib_length, sizeof(*sys_info->mib));
      for (int j = 0; j < sys_info->mib_length; j++)
        sys_info->mib[j] = msg->cell_to_modify[i].sys_info->mib[j];
    }
    if (msg->cell_to_modify[i].sys_info->sib1_length > 0) {
      sys_info->sib1_length = msg->cell_to_modify[i].sys_info->sib1_length;
      sys_info->sib1 = calloc_or_fail(msg->cell_to_modify[i].sys_info->sib1_length, sizeof(*sys_info->sib1));
      for (int j = 0; j < sys_info->sib1_length; j++)
        sys_info->sib1[j] = msg->cell_to_modify[i].sys_info->sib1[j];
    }
  }
  return cp;
}

/* ====================================
 *   F1AP gNB-CU Configuration Update
 * ==================================== */

/**
 * @brief F1 gNB-CU Configuration Update encoding (9.2.1.10 of 3GPP TS 38.473)
 */
F1AP_F1AP_PDU_t *encode_f1ap_cu_configuration_update(const f1ap_gnb_cu_configuration_update_t *msg)
{
  F1AP_F1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));
  /* Create
    0. Message Type */
  pdu->present = F1AP_F1AP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu->choice.initiatingMessage, initMsg);
  initMsg->procedureCode = F1AP_ProcedureCode_id_gNBCUConfigurationUpdate;
  initMsg->criticality = F1AP_Criticality_reject;
  initMsg->value.present = F1AP_InitiatingMessage__value_PR_GNBCUConfigurationUpdate;
  F1AP_GNBCUConfigurationUpdate_t *cfgUpdate = &pdu->choice.initiatingMessage->value.choice.GNBCUConfigurationUpdate;
  /* mandatory
    c1. Transaction ID (integer value) */
  asn1cSequenceAdd(cfgUpdate->protocolIEs.list, F1AP_GNBCUConfigurationUpdateIEs_t, ieC1);
  ieC1->id = F1AP_ProtocolIE_ID_id_TransactionID;
  ieC1->criticality = F1AP_Criticality_reject;
  ieC1->value.present = F1AP_GNBCUConfigurationUpdateIEs__value_PR_TransactionID;
  ieC1->value.choice.TransactionID = msg->transaction_id;
  /* optional
    c2. Cells_to_be_Activated_List (O) */
  for (int i = 0; i < msg->num_cells_to_activate; i++) {
    asn1cSequenceAdd(cfgUpdate->protocolIEs.list, F1AP_GNBCUConfigurationUpdateIEs_t, ieC3);
    ieC3->id = F1AP_ProtocolIE_ID_id_Cells_to_be_Activated_List;
    ieC3->criticality = F1AP_Criticality_reject;
    ieC3->value.present = F1AP_GNBCUConfigurationUpdateIEs__value_PR_Cells_to_be_Activated_List;
    asn1cSequenceAdd(ieC3->value.choice.Cells_to_be_Activated_List.list,
                     F1AP_Cells_to_be_Activated_List_ItemIEs_t,
                     cells_to_be_activated_ies);
    encode_cells_to_activate(&msg->cells_to_activate[i], cells_to_be_activated_ies);
  }
  return pdu;
}

/**
 * @brief F1 gNB-CU Configuration Update decoding (9.2.1.10 of 3GPP TS 38.473)
 */
bool decode_f1ap_cu_configuration_update(const F1AP_F1AP_PDU_t *pdu, f1ap_gnb_cu_configuration_update_t *out)
{
  /* Check presence of message type */
  _F1_EQ_CHECK_INT(pdu->present, F1AP_F1AP_PDU_PR_initiatingMessage);
  AssertError(pdu->choice.initiatingMessage != NULL, return false, "pdu->choice.initiatingMessage is NULL");
  _F1_EQ_CHECK_LONG(pdu->choice.initiatingMessage->procedureCode, F1AP_ProcedureCode_id_gNBCUConfigurationUpdate);
  _F1_EQ_CHECK_INT(pdu->choice.initiatingMessage->value.present, F1AP_InitiatingMessage__value_PR_GNBCUConfigurationUpdate);
  /* Check presence of mandatory IEs */
  F1AP_GNBCUConfigurationUpdate_t *in = &pdu->choice.initiatingMessage->value.choice.GNBCUConfigurationUpdate;
  F1AP_GNBCUConfigurationUpdateIEs_t *ie;
  F1AP_LIB_FIND_IE(F1AP_GNBCUConfigurationUpdateIEs_t, ie, in, F1AP_ProtocolIE_ID_id_TransactionID, true);
  /* Loop over all IEs */
  for (int i = 0; i < in->protocolIEs.list.count; i++) {
    ie = in->protocolIEs.list.array[i];
    switch (ie->id) {
      case F1AP_ProtocolIE_ID_id_TransactionID:
        _F1_EQ_CHECK_INT(ie->value.present, F1AP_GNBCUConfigurationUpdateIEs__value_PR_TransactionID);
        AssertError(ie->value.choice.TransactionID != -1, return false, "ie->value.choice.TransactionID is -1");
        out->transaction_id = ie->value.choice.TransactionID;
        break;
      case F1AP_ProtocolIE_ID_id_Cells_to_be_Activated_List: {
        /* Cells to be Activated List (O) */
        _F1_EQ_CHECK_INT(ie->value.present, F1AP_GNBCUConfigurationUpdateIEs__value_PR_Cells_to_be_Activated_List);
        /* Cells to be Activated List Item (count >= 1) */
        F1AP_Cells_to_be_Activated_List_t *Cells_to_be_Activated_List = &ie->value.choice.Cells_to_be_Activated_List;
        out->num_cells_to_activate = Cells_to_be_Activated_List->list.count;
        AssertError(out->num_cells_to_activate > 0, return false, "At least 1 cell to activate must be present");
        for (int i = 0; i < out->num_cells_to_activate; i++) {
          if (!decode_cells_to_activate(&out->cells_to_activate[i],
                                        (F1AP_Cells_to_be_Activated_List_ItemIEs_t *)Cells_to_be_Activated_List->list.array[i]))
            return false;
        }
        break;
      }
      default:
        AssertError(1 == 0, return false, "F1AP_ProtocolIE_ID_id %ld unknown\n", ie->id);
        break;
    }
  }
  return true;
}

void free_f1ap_cu_configuration_update(const f1ap_gnb_cu_configuration_update_t *msg)
{
  for (int i = 0; i < msg->num_cells_to_activate; i++)
    for (int j = 0; j < msg->cells_to_activate[j].num_SI; j++)
      free(msg->cells_to_activate[i].SI_msg[j].SI_container);
}

/**
 * @brief F1 gNB-CU Configuration Update check
 */
bool eq_f1ap_cu_configuration_update(const f1ap_gnb_cu_configuration_update_t *a, const f1ap_gnb_cu_configuration_update_t *b)
{
  _F1_EQ_CHECK_LONG(a->transaction_id, b->transaction_id);
  /* to activate */
  _F1_EQ_CHECK_INT(a->num_cells_to_activate, b->num_cells_to_activate);
  for (int i = 0; i < a->num_cells_to_activate; i++) {
    _F1_EQ_CHECK_LONG(a->cells_to_activate[i].nr_cellid, b->cells_to_activate[i].nr_cellid);
    _F1_EQ_CHECK_INT(a->cells_to_activate[i].nrpci, b->cells_to_activate[i].nrpci);
    if (!eq_f1ap_plmn(&a->cells_to_activate[i].plmn, &b->cells_to_activate[i].plmn))
      return false;
    _F1_EQ_CHECK_INT(a->cells_to_activate[i].num_SI, b->cells_to_activate[i].num_SI);
    for (int s = 0; s < a->cells_to_activate[i].num_SI; s++) {
      _F1_EQ_CHECK_INT(*a->cells_to_activate[i].SI_msg[s].SI_container, *a->cells_to_activate[i].SI_msg[s].SI_container);
      _F1_EQ_CHECK_INT(a->cells_to_activate[i].SI_msg[s].SI_container_length, a->cells_to_activate[i].SI_msg[s].SI_container_length);
      _F1_EQ_CHECK_INT(a->cells_to_activate[i].SI_msg[s].SI_type, a->cells_to_activate[i].SI_msg[s].SI_type);
    }
  }
  return true;
}

/**
 * @brief F1 gNB-CU Configuration Update deep copy
 */
f1ap_gnb_cu_configuration_update_t cp_f1ap_cu_configuration_update(const f1ap_gnb_cu_configuration_update_t *msg)
{
  f1ap_gnb_cu_configuration_update_t cp = {0};
  /* transaction_id */
  cp.transaction_id = msg->transaction_id;
  cp.num_cells_to_activate = msg->num_cells_to_activate;
  for (int i = 0; i < cp.num_cells_to_activate; i++) {
    cp.cells_to_activate[i] = msg->cells_to_activate[i];
    for (int s = 0; s < cp.cells_to_activate[i].num_SI; s++) {
      cp.cells_to_activate[i].SI_msg[s] = msg->cells_to_activate[i].SI_msg[s];
      f1ap_sib_msg_t *SI_msg = &cp.cells_to_activate[i].SI_msg[s];
      SI_msg->SI_container = calloc_or_fail(SI_msg->SI_container_length, sizeof(*SI_msg->SI_container));
      for (int j = 0; j < SI_msg->SI_container_length; j++)
        SI_msg->SI_container[j] = msg->cells_to_activate[i].SI_msg[s].SI_container[j];
    }
  }
  return cp;
}

/* ====================================
 *   F1AP gNB-CU Configuration Ack
 * ==================================== */

/**
 * @brief F1 gNB-CU Configuration Update Acknowledge message encoding (9.2.1.11 of 3GPP TS 38.473)
 *        gNB-DU → gNB-CU
 */
F1AP_F1AP_PDU_t *encode_f1ap_cu_configuration_update_acknowledge(const f1ap_gnb_cu_configuration_update_acknowledge_t *msg)
{
  F1AP_F1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));
  /* Create */
  /* 0. pdu Type */
  pdu->present = F1AP_F1AP_PDU_PR_successfulOutcome;
  asn1cCalloc(pdu->choice.successfulOutcome, tmp);
  tmp->procedureCode = F1AP_ProcedureCode_id_gNBCUConfigurationUpdate;
  tmp->criticality = F1AP_Criticality_reject;
  tmp->value.present = F1AP_SuccessfulOutcome__value_PR_GNBCUConfigurationUpdateAcknowledge;
  F1AP_GNBCUConfigurationUpdateAcknowledge_t *out = &tmp->value.choice.GNBCUConfigurationUpdateAcknowledge;
  // Transaction ID (M)
  asn1cSequenceAdd(out->protocolIEs.list, F1AP_GNBCUConfigurationUpdateAcknowledgeIEs_t, ie1);
  ie1->id = F1AP_ProtocolIE_ID_id_TransactionID;
  ie1->criticality = F1AP_Criticality_reject;
  ie1->value.present = F1AP_GNBCUConfigurationUpdateAcknowledgeIEs__value_PR_TransactionID;
  ie1->value.choice.TransactionID = msg->transaction_id;
  // Cells Failed to be Activated List (0..1)
  if (msg->num_cells_failed_to_be_activated > 0) {
    asn1cSequenceAdd(out->protocolIEs.list, F1AP_GNBCUConfigurationUpdateAcknowledgeIEs_t, ie2);
    ie2->id = F1AP_ProtocolIE_ID_id_Cells_Failed_to_be_Activated_List;
    ie2->criticality = F1AP_Criticality_reject;
    ie2->value.present = F1AP_GNBCUConfigurationUpdateAcknowledgeIEs__value_PR_Cells_Failed_to_be_Activated_List;
    F1AP_Cells_Failed_to_be_Activated_List_t *failedCellsList = &ie2->value.choice.Cells_Failed_to_be_Activated_List;
    for (int i = 0; i < msg->num_cells_failed_to_be_activated; i++) {
      asn1cSequenceAdd(failedCellsList->list, F1AP_Cells_Failed_to_be_Activated_List_ItemIEs_t, failedCellIE);
      failedCellIE->id = F1AP_ProtocolIE_ID_id_Cells_Failed_to_be_Activated_List_Item;
      failedCellIE->criticality = F1AP_Criticality_reject;
      failedCellIE->value.present = F1AP_Cells_Failed_to_be_Activated_List_ItemIEs__value_PR_Cells_Failed_to_be_Activated_List_Item;
      F1AP_Cells_Failed_to_be_Activated_List_Item_t *p1 = &failedCellIE->value.choice.Cells_Failed_to_be_Activated_List_Item;
      // Cause (M)
      p1->cause.present = F1AP_Cause_PR_radioNetwork;
      p1->cause.choice.radioNetwork = msg->cells_failed_to_be_activated[i].cause;
      // NR CGI (M)
      const f1ap_plmn_t *plmn = &msg->cells_failed_to_be_activated[i].plmn;
      MCC_MNC_TO_PLMNID(plmn->mcc, plmn->mnc, plmn->mnc_digit_length, &(p1->nRCGI.pLMN_Identity));
      printf("plmn->mcc %d %d %d %ld \n",
        p1->nRCGI.pLMN_Identity.buf[0], p1->nRCGI.pLMN_Identity.buf[1], p1->nRCGI.pLMN_Identity.buf[2], p1->nRCGI.pLMN_Identity.size);
      NR_CELL_ID_TO_BIT_STRING(msg->cells_failed_to_be_activated[i].nr_cellid, &(p1->nRCGI.nRCellIdentity));
    }
  }
  return pdu;
}

/**
 * @brief F1 gNB-CU Configuration Update Acknowledge decoding (9.2.1.11 of 3GPP TS 38.473)
 */
bool decode_f1ap_cu_configuration_update_acknowledge(const F1AP_F1AP_PDU_t *pdu,
                                                     f1ap_gnb_cu_configuration_update_acknowledge_t *out)
{
  /* Message type */
  _F1_EQ_CHECK_INT(pdu->present, F1AP_F1AP_PDU_PR_successfulOutcome);
  AssertError(pdu->choice.successfulOutcome != NULL, return false, "pdu->choice.successfulOutcome is NULL");
  _F1_EQ_CHECK_LONG(pdu->choice.successfulOutcome->procedureCode, F1AP_ProcedureCode_id_gNBCUConfigurationUpdate);
  _F1_EQ_CHECK_INT(pdu->choice.successfulOutcome->value.present,
                   F1AP_SuccessfulOutcome__value_PR_GNBCUConfigurationUpdateAcknowledge);
  /* payload */
  F1AP_GNBCUConfigurationUpdateAcknowledge_t *in = &pdu->choice.successfulOutcome->value.choice.GNBCUConfigurationUpdateAcknowledge;
  F1AP_GNBCUConfigurationUpdateAcknowledgeIEs_t *ie;
  /* Check mandatory IEs */
  F1AP_LIB_FIND_IE(F1AP_GNBCUConfigurationUpdateAcknowledgeIEs_t, ie, in, F1AP_ProtocolIE_ID_id_TransactionID, true);
  /* Loop over all IEs */
  for (int i = 0; i < in->protocolIEs.list.count; i++) {
    ie = in->protocolIEs.list.array[i];
    switch (ie->id) {
      case F1AP_ProtocolIE_ID_id_TransactionID:
        _F1_EQ_CHECK_INT(ie->value.present, F1AP_GNBCUConfigurationUpdateAcknowledgeIEs__value_PR_TransactionID);
        AssertError(ie->value.choice.TransactionID != -1, return false, "ie->value.choice.TransactionID is -1");
        out->transaction_id = ie->value.choice.TransactionID;
        break;
      case F1AP_ProtocolIE_ID_id_Cells_Failed_to_be_Activated_List: {
        /* Decode Cells Failed to be Activated List */
        _F1_EQ_CHECK_INT(ie->value.present,
                         F1AP_GNBCUConfigurationUpdateAcknowledgeIEs__value_PR_Cells_Failed_to_be_Activated_List);
        F1AP_Cells_Failed_to_be_Activated_List_t *cell_fail_list = &ie->value.choice.Cells_Failed_to_be_Activated_List;
        out->num_cells_failed_to_be_activated = cell_fail_list->list.count;
        for (int j = 0; j < out->num_cells_failed_to_be_activated; j++) {
          const F1AP_Cells_Failed_to_be_Activated_List_ItemIEs_t *itemIE =
              (F1AP_Cells_Failed_to_be_Activated_List_ItemIEs_t *)cell_fail_list->list.array[j];
          const F1AP_Cells_Failed_to_be_Activated_List_Item_t *item = &itemIE->value.choice.Cells_Failed_to_be_Activated_List_Item;
          // NR CGI (M)
          f1ap_plmn_t *plmn = &out->cells_failed_to_be_activated[j].plmn;
          TBCD_TO_MCC_MNC(&(item->nRCGI.pLMN_Identity), plmn->mcc, plmn->mnc, plmn->mnc_digit_length);
          BIT_STRING_TO_NR_CELL_IDENTITY(&item->nRCGI.nRCellIdentity, out->cells_failed_to_be_activated[j].nr_cellid);
          // Cause (M)
          switch (item->cause.present) {
            case F1AP_Cause_PR_radioNetwork:
              out->cells_failed_to_be_activated[j].cause = item->cause.choice.radioNetwork;
              break;
            case F1AP_Cause_PR_transport:
              out->cells_failed_to_be_activated[j].cause = item->cause.choice.transport;
              break;
            case F1AP_Cause_PR_protocol:
              out->cells_failed_to_be_activated[j].cause = item->cause.choice.protocol;
              break;
            case F1AP_Cause_PR_misc:
              out->cells_failed_to_be_activated[j].cause = item->cause.choice.misc;
              break;
            default:
              AssertError(1 == 0, return false, "Unknown cause type %d\n", item->cause.present);
              break;
          }
        }
        break;
      }
      default:
        AssertError(1 == 0, return false, "F1AP_ProtocolIE_ID_id %ld unknown\n", ie->id);
        break;
    }
  }
  return true;
}

/**
 * @brief F1 gNB-CU Configuration Update Acknowledge check
 */
bool eq_f1ap_cu_configuration_update_acknowledge(const f1ap_gnb_cu_configuration_update_acknowledge_t *a,
                                                 const f1ap_gnb_cu_configuration_update_acknowledge_t *b)
{
  // Transaction ID
  _F1_EQ_CHECK_LONG(a->transaction_id, b->transaction_id);
  // number of cells failed to be activated
  _F1_EQ_CHECK_INT(a->num_cells_failed_to_be_activated, b->num_cells_failed_to_be_activated);
  for (int i = 0; i < a->num_cells_failed_to_be_activated; i++) {
    if (!eq_f1ap_plmn(&a->cells_failed_to_be_activated[i].plmn, &b->cells_failed_to_be_activated[i].plmn))
      return false;
    _F1_EQ_CHECK_LONG(a->cells_failed_to_be_activated[i].nr_cellid, b->cells_failed_to_be_activated[i].nr_cellid);
    _F1_EQ_CHECK_INT(a->cells_failed_to_be_activated[i].cause, b->cells_failed_to_be_activated[i].cause);
  }
  // TNL Associations to setup
  _F1_EQ_CHECK_INT(a->noofTNLAssociations_to_setup, b->noofTNLAssociations_to_setup);
  for (int i = 0; i < a->noofTNLAssociations_to_setup; i++) {
    // Explicit comparison of TNL Association fields
    _F1_EQ_CHECK_INT(a->tnlAssociations_to_setup[i].tl_address, b->tnlAssociations_to_setup[i].tl_address);
    _F1_EQ_CHECK_INT(a->tnlAssociations_to_setup[i].port, b->tnlAssociations_to_setup[i].port);
  }
  // TNL Associations failed to setup
  _F1_EQ_CHECK_INT(a->noofTNLAssociations_failed, b->noofTNLAssociations_failed);
  for (int i = 0; i < a->noofTNLAssociations_failed; i++) {
    // Explicit comparison of TNL Association fields
    _F1_EQ_CHECK_INT(a->tnlAssociations_failed[i].tl_address, b->tnlAssociations_failed[i].tl_address);
    _F1_EQ_CHECK_INT(a->tnlAssociations_failed[i].port, b->tnlAssociations_failed[i].port);
  }
  // Dedicated SI Delivery Needed UE List
  _F1_EQ_CHECK_INT(a->noofDedicatedSIDeliveryNeededUEs, b->noofDedicatedSIDeliveryNeededUEs);
  for (int i = 0; i < a->noofDedicatedSIDeliveryNeededUEs; i++) {
    _F1_EQ_CHECK_INT(a->dedicatedSIDeliveryNeededUEs[i].gNB_CU_ue_id, b->dedicatedSIDeliveryNeededUEs[i].gNB_CU_ue_id);
    if (!eq_f1ap_plmn(&a->dedicatedSIDeliveryNeededUEs[i].ue_plmn, &b->dedicatedSIDeliveryNeededUEs[i].ue_plmn))
      return false;
    _F1_EQ_CHECK_LONG(a->dedicatedSIDeliveryNeededUEs[i].ue_nr_cellid, b->dedicatedSIDeliveryNeededUEs[i].ue_nr_cellid);
  }
  return true;
}

/**
 * @brief F1 gNB-CU Configuration Update Acknowledge deep copy
 */
f1ap_gnb_cu_configuration_update_acknowledge_t cp_f1ap_cu_configuration_update_acknowledge(
    const f1ap_gnb_cu_configuration_update_acknowledge_t *msg)
{
  f1ap_gnb_cu_configuration_update_acknowledge_t cp = {0};
  // Transaction ID
  cp.transaction_id = msg->transaction_id;
  // number of cells failed to be activated
  cp.num_cells_failed_to_be_activated = msg->num_cells_failed_to_be_activated;
  for (int i = 0; i < cp.num_cells_failed_to_be_activated; i++)
    cp.cells_failed_to_be_activated[i] = msg->cells_failed_to_be_activated[i];
  // TNL Associations to setup
  cp.noofTNLAssociations_to_setup = msg->noofTNLAssociations_to_setup;
  for (int i = 0; i < cp.noofTNLAssociations_to_setup; i++)
    cp.tnlAssociations_to_setup[i] = msg->tnlAssociations_to_setup[i];
  // TNL Associations failed to setup
  cp.noofTNLAssociations_failed = msg->noofTNLAssociations_failed;
  for (int i = 0; i < cp.noofTNLAssociations_failed; i++)
    cp.tnlAssociations_failed[i] = msg->tnlAssociations_failed[i];
  // Dedicated SI Delivery Needed UE List
  cp.noofDedicatedSIDeliveryNeededUEs = msg->noofDedicatedSIDeliveryNeededUEs;
  for (int i = 0; i < cp.noofDedicatedSIDeliveryNeededUEs; i++)
    cp.dedicatedSIDeliveryNeededUEs[i] = msg->dedicatedSIDeliveryNeededUEs[i];
  return cp;
}

/* ==================================
 *   F1AP gNB-DU Configuration Ack
 * ================================== */

/**
 * @brief F1 gNB-DU Configuration Update Acknowledge message encoding (9.2.1.8 of 3GPP TS 38.473)
 */
F1AP_F1AP_PDU_t *encode_f1ap_du_configuration_update_acknowledge(const f1ap_gnb_du_configuration_update_acknowledge_t *msg)
{
  F1AP_F1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));
  /* Create */
  /* 0. Message */
  pdu->present = F1AP_F1AP_PDU_PR_successfulOutcome;
  asn1cCalloc(pdu->choice.successfulOutcome, succOut);
  succOut->procedureCode = F1AP_ProcedureCode_id_gNBDUConfigurationUpdate;
  succOut->criticality = F1AP_Criticality_reject;
  succOut->value.present = F1AP_SuccessfulOutcome__value_PR_GNBDUConfigurationUpdateAcknowledge;
  F1AP_GNBDUConfigurationUpdateAcknowledge_t *ack = &succOut->value.choice.GNBDUConfigurationUpdateAcknowledge;
  /* Mandatory */
  /* Transaction Id */
  asn1cSequenceAdd(ack->protocolIEs.list, F1AP_GNBDUConfigurationUpdateAcknowledgeIEs_t, ie1);
  ie1->id = F1AP_ProtocolIE_ID_id_TransactionID;
  ie1->criticality = F1AP_Criticality_reject;
  ie1->value.present = F1AP_GNBDUConfigurationUpdateAcknowledgeIEs__value_PR_TransactionID;
  ie1->value.choice.TransactionID = msg->transaction_id;
  // Cells to be Activated List (O)
  for (int i = 0; i < msg->num_cells_to_activate; i++) {
    asn1cSequenceAdd(ack->protocolIEs.list, F1AP_GNBDUConfigurationUpdateAcknowledgeIEs_t, ie3);
    ie3->id = F1AP_ProtocolIE_ID_id_Cells_to_be_Activated_List;
    ie3->criticality = F1AP_Criticality_reject;
    ie3->value.present = F1AP_GNBDUConfigurationUpdateAcknowledgeIEs__value_PR_Cells_to_be_Activated_List;
    asn1cSequenceAdd(ie3->value.choice.Cells_to_be_Activated_List.list,
                     F1AP_Cells_to_be_Activated_List_ItemIEs_t,
                     cells_to_be_activated_ies);
    encode_cells_to_activate(&msg->cells_to_activate[i], cells_to_be_activated_ies);
  }
  return pdu;
}

/**
 * @brief F1 gNB-DU Configuration Update Acknowledge decoding
 */
bool decode_f1ap_du_configuration_update_acknowledge(const F1AP_F1AP_PDU_t *pdu,
                                                     f1ap_gnb_du_configuration_update_acknowledge_t *out)
{
  /* message type */
  _F1_EQ_CHECK_INT(pdu->present, F1AP_F1AP_PDU_PR_successfulOutcome);
  AssertError(pdu->choice.successfulOutcome != NULL, return false, "pdu->choice.successfulOutcome is NULL");
  _F1_EQ_CHECK_LONG(pdu->choice.successfulOutcome->procedureCode, F1AP_ProcedureCode_id_gNBDUConfigurationUpdate);
  _F1_EQ_CHECK_INT(pdu->choice.successfulOutcome->value.present,
                   F1AP_SuccessfulOutcome__value_PR_GNBDUConfigurationUpdateAcknowledge);
  F1AP_GNBDUConfigurationUpdateAcknowledge_t *in = &pdu->choice.successfulOutcome->value.choice.GNBDUConfigurationUpdateAcknowledge;
  F1AP_GNBDUConfigurationUpdateAcknowledgeIEs_t *ie;
  /* Check mandatory IEs */
  F1AP_LIB_FIND_IE(F1AP_GNBDUConfigurationUpdateAcknowledgeIEs_t, ie, in, F1AP_ProtocolIE_ID_id_TransactionID, true);
  /* Loop over all IEs */
  for (int i = 0; i < in->protocolIEs.list.count; i++) {
    ie = in->protocolIEs.list.array[i];
    switch (ie->id) {
      case F1AP_ProtocolIE_ID_id_TransactionID:
        _F1_EQ_CHECK_INT(ie->value.present, F1AP_GNBDUConfigurationUpdateAcknowledgeIEs__value_PR_TransactionID);
        AssertError(ie->value.choice.TransactionID != -1, return false, "ie->value.choice.TransactionID is -1");
        out->transaction_id = ie->value.choice.TransactionID;
        break;
      case F1AP_ProtocolIE_ID_id_Cells_to_be_Activated_List: {
        /* Decode Cells Failed to be Activated List */
        _F1_EQ_CHECK_INT(ie->value.present, F1AP_GNBDUConfigurationUpdateAcknowledgeIEs__value_PR_Cells_to_be_Activated_List);
        F1AP_Cells_to_be_Activated_List_t *list = &ie->value.choice.Cells_to_be_Activated_List;
        out->num_cells_to_activate = list->list.count;
        for (int j = 0; j < out->num_cells_to_activate; j++) {
          const F1AP_Cells_to_be_Activated_List_ItemIEs_t *itemIE =
              (F1AP_Cells_to_be_Activated_List_ItemIEs_t *)list->list.array[j];
          decode_cells_to_activate(&out->cells_to_activate[0], itemIE);
        }
        break;
      }
      default:
        AssertError(1 == 0, return false, "F1AP_ProtocolIE_ID_id %ld unknown\n", ie->id);
        break;
    }
  }
  return true;
}

/**
 * @brief F1 gNB-DU Configuration Update Acknowledge equality check
 */
bool eq_f1ap_du_configuration_update_acknowledge(const f1ap_gnb_du_configuration_update_acknowledge_t *a,
                                                 const f1ap_gnb_du_configuration_update_acknowledge_t *b)
{
  // Transaction ID
  _F1_EQ_CHECK_LONG(a->transaction_id, b->transaction_id);
  // number of cells to activate
  _F1_EQ_CHECK_INT(a->num_cells_to_activate, b->num_cells_to_activate);
  // loop over cells to activate
  for (int i = 0; i < a->num_cells_to_activate; i++) {
    _F1_EQ_CHECK_LONG(a->cells_to_activate[i].nr_cellid, b->cells_to_activate[i].nr_cellid);
    _F1_EQ_CHECK_INT(a->cells_to_activate[i].nrpci, b->cells_to_activate[i].nrpci);
    if (!eq_f1ap_plmn(&a->cells_to_activate[i].plmn, &b->cells_to_activate[i].plmn))
      return false;
    _F1_EQ_CHECK_INT(a->cells_to_activate[i].num_SI, b->cells_to_activate[i].num_SI);
    for (int s = 0; s < a->cells_to_activate[i].num_SI; s++) {
      _F1_EQ_CHECK_INT(*a->cells_to_activate[i].SI_msg[s].SI_container, *b->cells_to_activate[i].SI_msg[s].SI_container);
      _F1_EQ_CHECK_INT(a->cells_to_activate[i].SI_msg[s].SI_container_length,
                       b->cells_to_activate[i].SI_msg[s].SI_container_length);
      _F1_EQ_CHECK_INT(a->cells_to_activate[i].SI_msg[s].SI_type, b->cells_to_activate[i].SI_msg[s].SI_type);
    }
  }
  return true;
}

/**
 * @brief F1 gNB-DU Configuration Update Acknowledge deep copy
 */
f1ap_gnb_du_configuration_update_acknowledge_t cp_f1ap_du_configuration_update_acknowledge(
    const f1ap_gnb_du_configuration_update_acknowledge_t *msg)
{
  f1ap_gnb_du_configuration_update_acknowledge_t cp = {0};
  // Transaction ID
  cp.transaction_id = msg->transaction_id;
  // number of cells to activate
  cp.num_cells_to_activate = msg->num_cells_to_activate;
  // Loop through cells to activate
  for (int i = 0; i < cp.num_cells_to_activate; i++) {
    cp.cells_to_activate[i] = msg->cells_to_activate[i];
    for (int s = 0; s < cp.cells_to_activate[i].num_SI; s++) {
      cp.cells_to_activate[i].SI_msg[s].SI_container = calloc_or_fail(1, sizeof(*cp.cells_to_activate[i].SI_msg[s].SI_container));
      *cp.cells_to_activate[i].SI_msg[s].SI_container = *msg->cells_to_activate[i].SI_msg[s].SI_container;
    }
  }
  return cp;
}

/**
 * @brief gNB-DU Configuration Update Acknowledge memory management
 */
void free_f1ap_du_configuration_update_acknowledge(const f1ap_gnb_du_configuration_update_acknowledge_t *msg)
{
  // SI_container
  for (int i = 0; i < msg->num_cells_to_activate; i++) {
    for (int j = 0; j < msg->cells_to_activate[i].num_SI; j++) {
      if (msg->cells_to_activate[i].SI_msg[j].SI_container) {
        free(msg->cells_to_activate[i].SI_msg[j].SI_container);
      }
    }
  }
}
