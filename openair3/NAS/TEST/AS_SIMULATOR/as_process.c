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

/*****************************************************************************
      Eurecom OpenAirInterface 3
      Copyright(c) 2012 Eurecom

Source    as_process.c

Version   0.1

Date    2013/04/11

Product   Access-Stratum sublayer simulator

Subsystem AS message processing

Author    Frederic Maurel

Description Defines functions executed by the Access-Stratum sublayer
    upon receiving AS messages from the Non-Access-Stratum.

*****************************************************************************/

#include "as_process.h"
#include "nas_process.h"

#include "commonDef.h"
#include "as_data.h"

#include <sys/types.h>
#include <stdio.h>  // snprintf
#include <string.h> // memcpy

/****************************************************************************/
/****************  E X T E R N A L    D E F I N I T I O N S  ****************/
/****************************************************************************/

/****************************************************************************/
/*******************  L O C A L    D E F I N I T I O N S  *******************/
/****************************************************************************/

/* Message Sequence Chart Generator's buffer */
#define MSCGEN_BUFFER_SIZE  1024
static char _mscgen_buffer[MSCGEN_BUFFER_SIZE];

/* Tracking area code */
#define DEFAULT_TAC 0xCAFE    // two byte in hexadecimal format

/* Cell identity */
#define DEFAULT_CI  0x01020304  // four byte in hexadecimal format

/* Reference signal received power */
#define DEFAULT_RSRP  27

/* Reference signal received quality */
#define DEFAULT_RSRQ  55

static ssize_t _process_plmn(char* buffer, const plmn_t* plmn, size_t len);
static int _process_dump(char* buffer, int len, const char* msg, int size);

/****************************************************************************/
/******************  E X P O R T E D    F U N C T I O N S  ******************/
/****************************************************************************/

/*
 * =============================================================================
 *  Functions used to process messages received from the UE NAS process
 * =============================================================================
 */

/*
 * -----------------------------------------------------------------------------
 *        Process cell information request message
 * -----------------------------------------------------------------------------
 */
int process_cell_info_req(int msg_id, const cell_info_req_t* req,
                          cell_info_cnf_t* cnf)
{
  int index = 0;

  printf("INFO\t: %s - Process cell information request\n", __FUNCTION__);

  index += _process_plmn(_mscgen_buffer, &req->plmnID, MSCGEN_BUFFER_SIZE);
  snprintf(_mscgen_buffer + index, MSCGEN_BUFFER_SIZE - index,
           ", rat = 0x%x%c", req->rat, '\0');
  printf("[%s][%s][--- Cell Information Request (0x%.4x)\\n%s --->][%s]\n",
         getTime(), _ue_id, msg_id, _mscgen_buffer, _as_id);

  /* Setup cell information confirm message */
  cnf->errCode = AS_SUCCESS;
  cnf->tac = DEFAULT_TAC;
  cnf->cellID = DEFAULT_CI;
  cnf->rat = AS_EUTRAN;
  cnf->rsrp = DEFAULT_RSRP;
  cnf->rsrq = DEFAULT_RSRQ;

  snprintf(_mscgen_buffer, MSCGEN_BUFFER_SIZE,
           "tac = 0x%.4x, cellID = 0x%.8x, rat = 0x%x, RSRQ = %u, RSRP = %u%c",
           cnf->tac, cnf->cellID, cnf->rat, cnf->rsrq, cnf->rsrp, '\0');
  printf("[%s][%s][--- Cell Information Confirm (0x%.4x)\\n%s --->][%s]\n",
         getTime(), _as_id, AS_CELL_INFO_CNF, _mscgen_buffer, _ue_id);

  printf("INFO\t: %s - Send cell informtion confirm\n", __FUNCTION__);
  return (AS_CELL_INFO_CNF);
}

/*
 * -----------------------------------------------------------------------------
 *  Process NAS signalling connection establishment request message
 * -----------------------------------------------------------------------------
 */
int process_nas_establish_req(int msg_id, const nas_establish_req_t* req,
                              nas_establish_ind_t* ind,
                              nas_establish_cnf_t* cnf)
{
  int bytes;

  printf("INFO\t: %s - Process NAS signalling connection establishment "
         "request\n", __FUNCTION__);

  snprintf(_mscgen_buffer, MSCGEN_BUFFER_SIZE,
           "cause = %s, type = %s, tmsi = {MMEcode = %d, m_tmsi = %u}%c",
           rrcCause(req->cause), rrcType(req->type),
           req->s_tmsi.MMEcode, req->s_tmsi.m_tmsi, '\0');
  printf("[%s][%s][--- NAS Signalling Connection Establishment Request "
         "(0x%.4x)\\n%s --->][%s]\n",
         getTime(), _ue_id, msg_id, _mscgen_buffer, _as_id);

  _process_dump(_mscgen_buffer, MSCGEN_BUFFER_SIZE,
                (char*)req->initialNasMsg.data, req->initialNasMsg.length);
  printf("[%s][%s][%s]\n", _ue_id, _as_id, _mscgen_buffer);

  /* Process initial NAS message */
  bytes = nas_process(_mscgen_buffer, MSCGEN_BUFFER_SIZE,
                      (char*)req->initialNasMsg.data,
                      req->initialNasMsg.length);

  if (bytes < 0) {
    printf("[%s][%s][--- NAS Signalling Connection Establishment "
           "Indication (0x%.4x)\\ntac = 0x%.4x\\n%s ---x][%s]\n", getTime(),
           _as_id, AS_NAS_ESTABLISH_IND, ind->tac, _mscgen_buffer, _mme_id);
    /* Setup NAS signalling connection establishment confirmation message */
    cnf->errCode = AS_TERMINATED_AS;
    return (0);
  }

  /* Setup NAS signalling connection establishment indication message */
  ind->UEid = 1;  // Valid UEid starts at index 1
  ind->tac = DEFAULT_TAC;

  if (req->initialNasMsg.length > 0) {
    ind->initialNasMsg.data = malloc(req->initialNasMsg.length);

    if (ind->initialNasMsg.data) {
      memcpy(ind->initialNasMsg.data, req->initialNasMsg.data,
             req->initialNasMsg.length);
      ind->initialNasMsg.length = req->initialNasMsg.length;
    }
  }

  // Attach_Request.messagetype = WRONG_MESSAGE_TYPE
  //ind->initialNasMsg.data[1] = 0x47; // MME DEBUG (Send EMM status)

  /* Setup NAS signalling connection establishment confirmation message */
  cnf->errCode = AS_SUCCESS;
  // Transmission failure of Attach Request message
  //cnf->errCode = AS_FAILURE; // UE DEBUG

  printf("[%s][%s][--- NAS Signalling Connection Establishment Indication "
         "(0x%.4x)\\nUEid = %u, tac = 0x%.4x\\n%s --->][%s]\n", getTime(),
         _as_id, AS_NAS_ESTABLISH_IND, ind->UEid, ind->tac, _mscgen_buffer, _mme_id);

  printf("INFO\t: %s - Send NAS signalling connection establishment "
         "indication\n", __FUNCTION__);
  return (AS_NAS_ESTABLISH_IND);
}

/*
 * -----------------------------------------------------------------------------
 *    Process Uplink data transfer request message
 * -----------------------------------------------------------------------------
 */
int process_ul_info_transfer_req(int msg_id, const ul_info_transfer_req_t* req,
                                 ul_info_transfer_ind_t* ind,
                                 ul_info_transfer_cnf_t* cnf)
{
  int bytes;

  printf("INFO\t: %s - Process uplink data transfer request\n", __FUNCTION__);

  snprintf(_mscgen_buffer, MSCGEN_BUFFER_SIZE,
           "tmsi = {MMEcode = %d, m_tmsi = %u}%c",
           req->s_tmsi.MMEcode, req->s_tmsi.m_tmsi, '\0');
  printf("[%s][%s][--- Uplink Information Request "
         "(0x%.4x)\\n%s --->][%s]\n",
         getTime(), _ue_id, msg_id, _mscgen_buffer, _as_id);

  _process_dump(_mscgen_buffer, MSCGEN_BUFFER_SIZE,
                (char*)req->nasMsg.data, req->nasMsg.length);
  printf("[%s][%s][%s]\n", _ue_id, _as_id, _mscgen_buffer);

  /* Process NAS message */
  bytes = nas_process(_mscgen_buffer, MSCGEN_BUFFER_SIZE,
                      (char*)req->nasMsg.data,
                      req->nasMsg.length);

  if (bytes < 0) {
    printf("[%s][%s][--- Uplink Information Indication "
           "(0x%.4x)\\n%s ---x][%s]\n", getTime(),
           _as_id, AS_UL_INFO_TRANSFER_IND, _mscgen_buffer, _mme_id);
    /* Setup uplink information confirmation message */
    cnf->errCode = AS_TERMINATED_AS;
    return (0);
  }

  /* Setup uplink information indication message */
  ind->UEid = 1;  // Valid UEid starts at index 1

  if (req->nasMsg.length > 0) {
    ind->nasMsg.data = malloc(req->nasMsg.length);

    if (ind->nasMsg.data) {
      memcpy(ind->nasMsg.data, req->nasMsg.data, req->nasMsg.length);
      ind->nasMsg.length = req->nasMsg.length;
    }
  }

  /* Setup uplink information confirmation message */
  cnf->UEid = 0;
  cnf->errCode = AS_SUCCESS;

  printf("[%s][%s][--- Uplink Information Indication "
         "(0x%.4x), UEid = %u\\n%s --->][%s]\n", getTime(),
         _as_id, AS_UL_INFO_TRANSFER_IND, ind->UEid, _mscgen_buffer, _mme_id);

  printf("INFO\t: %s - Send uplink data transfer indication\n", __FUNCTION__);
  return (AS_UL_INFO_TRANSFER_IND);
}

/*
 * -----------------------------------------------------------------------------
 *      Process NAS signalling connection release request message
 * -----------------------------------------------------------------------------
 */
int process_nas_release_req(int msg_id, const nas_release_req_t* req)
{
  printf("INFO\t: %s - Process NAS signalling connection release request\n",
         __FUNCTION__);

  printf("[%s][%s][--- NAS Signalling Release Request "
         "(0x%.4x), tmsi = {MMEcode = %d, m_tmsi = %u}, "
         "cause = %s (%u) --->][%s]\n", getTime(), _ue_id, msg_id,
         req->s_tmsi.MMEcode, req->s_tmsi.m_tmsi,
         rrcReleaseCause(req->cause), req->cause, _as_id);
  return (AS_NAS_RELEASE_REQ);
}

/*
 * =============================================================================
 *  Functions used to process messages received from the MME NAS process
 * =============================================================================
 */

/*
 * -----------------------------------------------------------------------------
 *  Process NAS signalling connection establishment response message
 * -----------------------------------------------------------------------------
 */
int process_nas_establish_rsp(int msg_id, const nas_establish_rsp_t* rsp,
                              nas_establish_cnf_t* cnf)
{
  int bytes;

  printf("INFO\t: %s - Process NAS signalling connection establishment "
         "response\n", __FUNCTION__);

  printf("[%s][%s][--- NAS Signalling Connection Establishment "
         "Response (0x%.4x)\\nerrCode = %s, UEid = %u --->][%s]\n",
         getTime(), _mme_id, msg_id,
         rrcErrCode(rsp->errCode), rsp->UEid, _as_id);

  _process_dump(_mscgen_buffer, MSCGEN_BUFFER_SIZE,
                (char*)rsp->nasMsg.data, rsp->nasMsg.length);
  printf("[%s][%s][%s]\n", _mme_id, _as_id, _mscgen_buffer);

  // Discard Attach Accept message
  //return (0); // UE DEBUG

  /* Process initial NAS message */
  bytes = nas_process(_mscgen_buffer, MSCGEN_BUFFER_SIZE,
                      (char*)rsp->nasMsg.data,
                      rsp->nasMsg.length);

  if (bytes < 0) {
    printf("[%s][%s][--- NAS Signalling Connection Establishment "
           "Confirm (0x%.4x)\\n%s ---x][%s]\n", getTime(),
           _as_id, AS_NAS_ESTABLISH_CNF, _mscgen_buffer, _ue_id);
    return (0);
  }

  /* Setup NAS signalling connection establishment confirm message */
  cnf->errCode = rsp->errCode;

  if (rsp->nasMsg.length > 0) {
    cnf->nasMsg.data = malloc(rsp->nasMsg.length);

    if (cnf->nasMsg.data) {
      memcpy(cnf->nasMsg.data, rsp->nasMsg.data,
             rsp->nasMsg.length);
      cnf->nasMsg.length = rsp->nasMsg.length;
    }
  }

  printf("[%s][%s][--- NAS Signalling Connection Establishment "
         "Confirm (0x%.4x)\\n%s --->][%s]\n", getTime(),
         _as_id, AS_NAS_ESTABLISH_CNF, _mscgen_buffer, _ue_id);

  printf("INFO\t: %s - Send NAS signalling connection establishment "
         "confirm\n", __FUNCTION__);
  return (AS_NAS_ESTABLISH_CNF);
}

/*
 * -----------------------------------------------------------------------------
 *    Process Downlink data transfer request message
 * -----------------------------------------------------------------------------
 */
int process_dl_info_transfer_req(int msg_id, const dl_info_transfer_req_t* req,
                                 dl_info_transfer_ind_t* ind,
                                 dl_info_transfer_cnf_t* cnf)
{
  int bytes;

  printf("INFO\t: %s - Process downlink data transfer request\n",
         __FUNCTION__);

  printf("[%s][%s][--- Downlink Information Request "
         "(0x%.4x), UEid = %u --->][%s]\n",
         getTime(), _mme_id, msg_id, req->UEid, _as_id);

  _process_dump(_mscgen_buffer, MSCGEN_BUFFER_SIZE,
                (char*)req->nasMsg.data, req->nasMsg.length);
  printf("[%s][%s][%s]\n", _mme_id, _as_id, _mscgen_buffer);

  /* Process NAS message */
  bytes = nas_process(_mscgen_buffer, MSCGEN_BUFFER_SIZE,
                      (char*)req->nasMsg.data,
                      req->nasMsg.length);

  if (bytes < 0) {
    printf("[%s][%s][--- Downlink Information Indication "
           "(0x%.4x)\\n%s ---x][%s]\n", getTime(),
           _as_id, AS_DL_INFO_TRANSFER_IND, _mscgen_buffer, _ue_id);
    /* Setup downlink information confirmation message */
    cnf->UEid = req->UEid;
    cnf->errCode = AS_TERMINATED_AS;
    return (0);
  }

  /* Setup downlink information indication message */
  if (req->nasMsg.length > 0) {
    ind->nasMsg.data = malloc(req->nasMsg.length);

    if (ind->nasMsg.data) {
      memcpy(ind->nasMsg.data, req->nasMsg.data, req->nasMsg.length);
      ind->nasMsg.length = req->nasMsg.length;
    }
  }

  /* Setup downlink information confirmation message */
  cnf->UEid = req->UEid;
  cnf->errCode = AS_SUCCESS;

  //if (req->nasMsg.data[1] == 0x55) {
  // Transmission failure of identification request message
  //  cnf->errCode = AS_FAILURE; // MME DEBUG
  // Discard identification request message
  //  return (0); // MME DEBUG
  //}

  printf("[%s][%s][--- Downlink Information Indication "
         "(0x%.4x)\\n%s --->][%s]\n", getTime(),
         _as_id, AS_DL_INFO_TRANSFER_IND, _mscgen_buffer, _ue_id);

  printf("INFO\t: %s - Send downlink data transfer indication\n", __FUNCTION__);
  return (AS_DL_INFO_TRANSFER_IND);
}

/*
 * -----------------------------------------------------------------------------
 *      Process NAS signalling connection release indication message
 * -----------------------------------------------------------------------------
 */
int process_nas_release_ind(int msg_id, const nas_release_req_t* req,
                            nas_release_ind_t* ind)
{
  printf("INFO\t: %s - Process NAS signalling connection release request\n",
         __FUNCTION__);

  printf("[%s][%s][--- NAS Signalling Connection Release Request "
         "(0x%.4x), UEid = %u, cause = %s (%u) --->][%s]\n",
         getTime(), _mme_id, msg_id, req->UEid,
         rrcReleaseCause(req->cause), req->cause, _as_id);

  /* Forward NAS release indication to the UE */
  ind->cause = req->cause;
  printf("[%s][%s][--- NAS Signalling Connection Release Indication "
         "(0x%.4x), cause = %s (%u) --->][%s]\n",
         getTime(), _as_id, msg_id, rrcReleaseCause(req->cause),
         ind->cause, _ue_id);

  printf("INFO\t: %s - Send NAS signalling connection release indication\n",
         __FUNCTION__);
  return (AS_NAS_RELEASE_IND);
}

/****************************************************************************/
/*********************  L O C A L    F U N C T I O N S  *********************/
/****************************************************************************/

/*
 * -----------------------------------------------------------------------------
 *        Display PLMN identity
 * -----------------------------------------------------------------------------
 */
static ssize_t _process_plmn(char* buffer, const plmn_t* plmn, size_t len)
{
  int index = 0;
  index += snprintf(buffer + index, len - index, "plmnID = %u%u%u%u%u",
                    plmn->MCCdigit1, plmn->MCCdigit2, plmn->MCCdigit3,
                    plmn->MNCdigit1, plmn->MNCdigit2);

  if ( (index < len) && (plmn->MNCdigit3 != 0xf) ) {
    index += snprintf(buffer + index, len - index, "%u", plmn->MNCdigit3);
  }

  return (index);
}

/*
 * -----------------------------------------------------------------------------
 *        Message dump utility
 * -----------------------------------------------------------------------------
 */
static int _process_dump(char* buffer, int len, const char* msg, int size)
{
  int index = 0;

  for (int i = 0; i < size; i++) {
    if ( (i%16) == 0 ) index += snprintf(buffer + index,
                                           len - index, "\\n");

    index += snprintf(buffer + index, len - index,
                      " %.2hhx", (unsigned char)msg[i]);
  }

  buffer[index] = '\0';
  return (index);
}
