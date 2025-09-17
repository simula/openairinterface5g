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

/*! \file nr_nas_msg_sim.c
 * \brief simulator for nr nas message
 * \author Yoshio INOUE, Masayuki HARADA
 * \email yoshio.inoue@fujitsu.com,masayuki.harada@fujitsu.com
 * \date 2020
 * \version 0.1
 *
 * 2023.01.27 Vladimir Dorovskikh 16 digits IMEISV
 */

#include <string.h> // memset
#include <stdlib.h> // malloc, free

#include "nas_log.h"
#include "TLVDecoder.h"
#include "TLVEncoder.h"
#include "nr_nas_msg_sim.h"
#include "aka_functions.h"
#include "secu_defs.h"
#include "kdf.h"
#include "key_nas_deriver.h"
#include "PduSessionEstablishRequest.h"
#include "PduSessionEstablishmentAccept.h"
#include "RegistrationAccept.h"
#include "FGSDeregistrationRequestUEOriginating.h"
#include "intertask_interface.h"
#include "common/utils/tun_if.h"
#include <openair3/NAS/COMMON/NR_NAS_defs.h>
#include "openair2/SDAP/nr_sdap/nr_sdap.h"
#include "openair3/SECU/nas_stream_eia2.h"
#include "openair3/UTILS/conversions.h"

#define MAX_NAS_UE 4

extern uint16_t NB_UE_INST;
static nr_ue_nas_t nr_ue_nas[MAX_NAS_UE] = {0};
static nr_nas_msg_snssai_t nas_allowed_nssai[8];

typedef enum {
  NAS_SECURITY_NO_SECURITY_CONTEXT,
  NAS_SECURITY_UNPROTECTED,
  NAS_SECURITY_INTEGRITY_FAILED,
  NAS_SECURITY_INTEGRITY_PASSED,
  NAS_SECURITY_BAD_INPUT
} security_state_t;

security_state_t nas_security_rx_process(nr_ue_nas_t *nas, uint8_t *pdu_buffer, int pdu_length)
{
  if (nas->security_container == NULL)
    return NAS_SECURITY_NO_SECURITY_CONTEXT;
  if (pdu_buffer[1] == 0)
    return NAS_SECURITY_UNPROTECTED;
  /* header is 7 bytes, require at least one byte of payload */
  if (pdu_length < 8)
    return NAS_SECURITY_BAD_INPUT;
  /* only accept "integrity protected and ciphered" messages */
  if (pdu_buffer[1] != 2) {
    LOG_E(NAS, "todo: unhandled security type %d\n", pdu_buffer[2]);
    return NAS_SECURITY_BAD_INPUT;
  }

  /* synchronize NAS SQN, based on 24.501 4.4.3.1 */
  int nas_sqn = pdu_buffer[6];
  int target_sqn = nas->security.nas_count_dl & 0xff;
  if (nas_sqn != target_sqn) {
    if (nas_sqn < target_sqn)
      nas->security.nas_count_dl += 256;
    nas->security.nas_count_dl &= ~255;
    nas->security.nas_count_dl |= nas_sqn;
  }
  if (nas->security.nas_count_dl > 0x00ffffff) {
    /* it's doubtful that this will happen, so let's simply exit for the time being */
    /* to be refined if needed */
    LOG_E(NAS, "max NAS COUNT DL reached\n");
    exit(1);
  }

  /* check integrity */
  uint8_t computed_mac[4];
  nas_stream_cipher_t stream_cipher;
  stream_cipher.context = nas->security_container->integrity_context;
  stream_cipher.count = nas->security.nas_count_dl;
  stream_cipher.bearer = 1; /* todo: don't hardcode */
  stream_cipher.direction = 1;
  stream_cipher.message = pdu_buffer + 6;
  /* length in bits */
  stream_cipher.blength = (pdu_length - 6) << 3;
  stream_compute_integrity(nas->security_container->integrity_algorithm, &stream_cipher, computed_mac);

  uint8_t *received_mac = pdu_buffer + 2;

  if (memcmp(received_mac, computed_mac, 4) != 0)
    return NAS_SECURITY_INTEGRITY_FAILED;

  /* decipher */
  uint8_t buf[pdu_length - 7];
  stream_cipher.context = nas->security_container->ciphering_context;
  stream_cipher.count = nas->security.nas_count_dl;
  stream_cipher.bearer = 1; /* todo: don't hardcode */
  stream_cipher.direction = 1;
  stream_cipher.message = pdu_buffer + 7;
  /* length in bits */
  stream_cipher.blength = (pdu_length - 7) << 3;
  stream_compute_encrypt(nas->security_container->ciphering_algorithm, &stream_cipher, buf);
  memcpy(pdu_buffer + 7, buf, pdu_length - 7);

  nas->security.nas_count_dl++;

  return NAS_SECURITY_INTEGRITY_PASSED;
}

static int nas_protected_security_header_encode(char *buffer, const fgs_nas_message_security_header_t *header, int length)
{
  LOG_FUNC_IN;

  int size = 0;

  /* Encode the protocol discriminator) */
  ENCODE_U8(buffer, header->protocol_discriminator, size);

  /* Encode the security header type */
  ENCODE_U8(buffer + size, (header->security_header_type & 0xf), size);

  /* Encode the message authentication code */
  ENCODE_U32(buffer + size, header->message_authentication_code, size);
  /* Encode the sequence number */
  ENCODE_U8(buffer + size, header->sequence_number, size);

  LOG_FUNC_RETURN(size);
}

static int _nas_mm_msg_encode_header(const mm_msg_header_t *header, uint8_t *buffer, uint32_t len)
{
  int size = 0;

  /* Check the buffer length */
  if (len < sizeof(mm_msg_header_t)) {
    return (TLV_ENCODE_BUFFER_TOO_SHORT);
  }

  /* Check the protocol discriminator */
  if (header->ex_protocol_discriminator != FGS_MOBILITY_MANAGEMENT_MESSAGE) {
    LOG_TRACE(ERROR, "ESM-MSG   - Unexpected extened protocol discriminator: 0x%x", header->ex_protocol_discriminator);
    return (TLV_ENCODE_PROTOCOL_NOT_SUPPORTED);
  }

  /* Encode the extendedprotocol discriminator */
  ENCODE_U8(buffer + size, header->ex_protocol_discriminator, size);
  /* Encode the security header type */
  ENCODE_U8(buffer + size, (header->security_header_type & 0xf), size);
  /* Encode the message type */
  ENCODE_U8(buffer + size, header->message_type, size);
  return (size);
}

static int fill_suci(FGSMobileIdentity *mi, const uicc_t *uicc)
{
  mi->suci.typeofidentity = FGS_MOBILE_IDENTITY_SUCI;
  mi->suci.mncdigit1 = uicc->nmc_size == 2 ? uicc->imsiStr[3] - '0' : uicc->imsiStr[4] - '0';
  mi->suci.mncdigit2 = uicc->nmc_size == 2 ? uicc->imsiStr[4] - '0' : uicc->imsiStr[5] - '0';
  mi->suci.mncdigit3 = uicc->nmc_size == 2 ? 0xF : uicc->imsiStr[3] - '0';
  mi->suci.mccdigit1 = uicc->imsiStr[0] - '0';
  mi->suci.mccdigit2 = uicc->imsiStr[1] - '0';
  mi->suci.mccdigit3 = uicc->imsiStr[2] - '0';
  memcpy(mi->suci.schemeoutput, uicc->imsiStr + 3 + uicc->nmc_size, strlen(uicc->imsiStr) - (3 + uicc->nmc_size));
  return sizeof(Suci5GSMobileIdentity_t);
}

static int fill_guti(FGSMobileIdentity *mi, const Guti5GSMobileIdentity_t *guti)
{
  AssertFatal(guti != NULL, "UE has no GUTI\n");
  mi->guti = *guti;
  return 13;
}

static int fill_imeisv(FGSMobileIdentity *mi, const uicc_t *uicc)
{
  int i = 0;
  mi->imeisv.typeofidentity = FGS_MOBILE_IDENTITY_IMEISV;
  mi->imeisv.digittac01 = getImeisvDigit(uicc, i++);
  mi->imeisv.digittac02 = getImeisvDigit(uicc, i++);
  mi->imeisv.digittac03 = getImeisvDigit(uicc, i++);
  mi->imeisv.digittac04 = getImeisvDigit(uicc, i++);
  mi->imeisv.digittac05 = getImeisvDigit(uicc, i++);
  mi->imeisv.digittac06 = getImeisvDigit(uicc, i++);
  mi->imeisv.digittac07 = getImeisvDigit(uicc, i++);
  mi->imeisv.digittac08 = getImeisvDigit(uicc, i++);
  mi->imeisv.digit09 = getImeisvDigit(uicc, i++);
  mi->imeisv.digit10 = getImeisvDigit(uicc, i++);
  mi->imeisv.digit11 = getImeisvDigit(uicc, i++);
  mi->imeisv.digit12 = getImeisvDigit(uicc, i++);
  mi->imeisv.digit13 = getImeisvDigit(uicc, i++);
  mi->imeisv.digit14 = getImeisvDigit(uicc, i++);
  mi->imeisv.digitsv1 = getImeisvDigit(uicc, i++);
  mi->imeisv.digitsv2 = getImeisvDigit(uicc, i++);
  mi->imeisv.spare = 0x0f;
  mi->imeisv.oddeven = 0;
  return 19;
}

int mm_msg_encode(MM_msg *mm_msg, uint8_t *buffer, uint32_t len)
{
  LOG_FUNC_IN;
  int header_result;
  int encode_result;
  uint8_t msg_type = mm_msg->header.message_type;

  /* First encode the EMM message header */
  header_result = _nas_mm_msg_encode_header(&mm_msg->header, buffer, len);

  if (header_result < 0) {
    LOG_TRACE(ERROR,
              "EMM-MSG   - Failed to encode EMM message header "
              "(%d)",
              header_result);
    LOG_FUNC_RETURN(header_result);
  }

  buffer += header_result;
  len -= header_result;

  switch (msg_type) {
    case REGISTRATION_REQUEST:
      encode_result = encode_registration_request(&mm_msg->registration_request, buffer, len);
      break;
    case FGS_IDENTITY_RESPONSE:
      encode_result = encode_identiy_response(&mm_msg->fgs_identity_response, buffer, len);
      break;
    case FGS_AUTHENTICATION_RESPONSE:
      encode_result = encode_fgs_authentication_response(&mm_msg->fgs_auth_response, buffer, len);
      break;
    case FGS_SECURITY_MODE_COMPLETE:
      encode_result = encode_fgs_security_mode_complete(&mm_msg->fgs_security_mode_complete, buffer, len);
      break;
    case FGS_UPLINK_NAS_TRANSPORT:
      encode_result = encode_fgs_uplink_nas_transport(&mm_msg->uplink_nas_transport, buffer, len);
      break;
    case FGS_DEREGISTRATION_REQUEST_UE_ORIGINATING:
      encode_result =
          encode_fgs_deregistration_request_ue_originating(&mm_msg->fgs_deregistration_request_ue_originating, buffer, len);
      break;
    default:
      LOG_TRACE(ERROR, "EMM-MSG   - Unexpected message type: 0x%x", mm_msg->header.message_type);
      encode_result = TLV_ENCODE_WRONG_MESSAGE_TYPE;
      break;
      /* TODO: Handle not standard layer 3 messages: SERVICE_REQUEST */
  }

  if (encode_result < 0) {
    LOG_TRACE(ERROR,
              "EMM-MSG   - Failed to encode L3 EMM message 0x%x "
              "(%d)",
              mm_msg->header.message_type,
              encode_result);
  }

  if (encode_result < 0)
    LOG_FUNC_RETURN(encode_result);

  LOG_FUNC_RETURN(header_result + encode_result);
}

void transferRES(uint8_t ck[16], uint8_t ik[16], uint8_t *input, uint8_t rand[16], uint8_t *output, uicc_t *uicc)
{
  uint8_t S[100] = {0};
  S[0] = 0x6B;
  servingNetworkName(S + 1, uicc->imsiStr, uicc->nmc_size);
  int netNamesize = strlen((char *)S + 1);
  S[1 + netNamesize] = (netNamesize & 0xff00) >> 8;
  S[2 + netNamesize] = (netNamesize & 0x00ff);
  for (int i = 0; i < 16; i++)
    S[3 + netNamesize + i] = rand[i];
  S[19 + netNamesize] = 0x00;
  S[20 + netNamesize] = 0x10;
  for (int i = 0; i < 8; i++)
    S[21 + netNamesize + i] = input[i];
  S[29 + netNamesize] = 0x00;
  S[30 + netNamesize] = 0x08;

  uint8_t plmn[3] = {0x02, 0xf8, 0x39};
  uint8_t oldS[100];
  oldS[0] = 0x6B;
  memcpy(&oldS[1], plmn, 3);
  oldS[4] = 0x00;
  oldS[5] = 0x03;
  for (int i = 0; i < 16; i++)
    oldS[6 + i] = rand[i];
  oldS[22] = 0x00;
  oldS[23] = 0x10;
  for (int i = 0; i < 8; i++)
    oldS[24 + i] = input[i];
  oldS[32] = 0x00;
  oldS[33] = 0x08;

  uint8_t key[32] = {0};
  memcpy(&key[0], ck, 16);
  memcpy(&key[16], ik, 16); // KEY
  uint8_t out[32] = {0};

  byte_array_t data = {.buf = S, .len = 31 + netNamesize};
  kdf(key, data, 32, out);

  memcpy(output, out + 16, 16);
}

void derive_kausf(uint8_t ck[16], uint8_t ik[16], uint8_t sqn[6], uint8_t kausf[32], uicc_t *uicc)
{
  uint8_t S[100] = {0};
  uint8_t key[32] = {0};

  memcpy(&key[0], ck, 16);
  memcpy(&key[16], ik, 16); // KEY
  S[0] = 0x6A;
  servingNetworkName(S + 1, uicc->imsiStr, uicc->nmc_size);
  int netNamesize = strlen((char *)S + 1);
  S[1 + netNamesize] = (uint8_t)((netNamesize & 0xff00) >> 8);
  S[2 + netNamesize] = (uint8_t)(netNamesize & 0x00ff);
  for (int i = 0; i < 6; i++) {
    S[3 + netNamesize + i] = sqn[i];
  }
  S[9 + netNamesize] = 0x00;
  S[10 + netNamesize] = 0x06;

  byte_array_t data = {.buf = S, .len = 11 + netNamesize};
  kdf(key, data, 32, kausf);
}

void derive_kseaf(uint8_t kausf[32], uint8_t kseaf[32], uicc_t *uicc)
{
  uint8_t S[100] = {0};
  S[0] = 0x6C; // FC
  servingNetworkName(S + 1, uicc->imsiStr, uicc->nmc_size);
  int netNamesize = strlen((char *)S + 1);
  S[1 + netNamesize] = (uint8_t)((netNamesize & 0xff00) >> 8);
  S[2 + netNamesize] = (uint8_t)(netNamesize & 0x00ff);

  byte_array_t data = {.buf = S, .len = 3 + netNamesize};
  kdf(kausf, data, 32, kseaf);
}

void derive_kamf(uint8_t *kseaf, uint8_t *kamf, uint16_t abba, uicc_t *uicc)
{
  int imsiLen = strlen(uicc->imsiStr);
  uint8_t S[100] = {0};
  S[0] = 0x6D; // FC = 0x6D
  memcpy(&S[1], uicc->imsiStr, imsiLen);
  S[1 + imsiLen] = (uint8_t)((imsiLen & 0xff00) >> 8);
  S[2 + imsiLen] = (uint8_t)(imsiLen & 0x00ff);
  S[3 + imsiLen] = abba & 0x00ff;
  S[4 + imsiLen] = (abba & 0xff00) >> 8;
  S[5 + imsiLen] = 0x00;
  S[6 + imsiLen] = 0x02;

  byte_array_t data = {.buf = S, .len = 7 + imsiLen};
  kdf(kseaf, data, 32, kamf);
}

//------------------------------------------------------------------------------
void derive_knas(algorithm_type_dist_t nas_alg_type, uint8_t nas_alg_id, uint8_t kamf[32], uint8_t *knas)
{
  uint8_t S[20] = {0};
  uint8_t out[32] = {0};
  S[0] = 0x69; // FC
  S[1] = (uint8_t)(nas_alg_type & 0xFF);
  S[2] = 0x00;
  S[3] = 0x01;
  S[4] = nas_alg_id;
  S[5] = 0x00;
  S[6] = 0x01;

  byte_array_t data = {.buf = S, .len = 7};
  kdf(kamf, data, 32, out);

  memcpy(knas, out + 16, 16);
}

void derive_kgnb(uint8_t kamf[32], uint32_t count, uint8_t *kgnb)
{
  /* Compute the KDF input parameter
   * S = FC(0x6E) || UL NAS Count || 0x00 0x04 || 0x01 || 0x00 0x01
   */
  uint8_t input[32] = {0};
  //    uint16_t length    = 4;
  //    int      offset    = 0;

  LOG_TRACE(INFO, "%s  with count= %d", __FUNCTION__, count);
  memset(input, 0, 32);
  input[0] = 0x6E;
  // P0
  input[1] = count >> 24;
  input[2] = (uint8_t)(count >> 16);
  input[3] = (uint8_t)(count >> 8);
  input[4] = (uint8_t)count;
  // L0
  input[5] = 0;
  input[6] = 4;
  // P1
  input[7] = 0x01;
  // L1
  input[8] = 0;
  input[9] = 1;

  byte_array_t data = {.buf = input, .len = 10};
  kdf(kamf, data, 32, kgnb);

  printf("kgnb : ");
  for (int pp = 0; pp < 32; pp++)
    printf("%02x ", kgnb[pp]);
  printf("\n");
}

void derive_ue_keys(uint8_t *buf, nr_ue_nas_t *nas)
{
  uint8_t ak[6];
  uint8_t sqn[6];

  DevAssert(nas != NULL);
  uint8_t *kausf = nas->security.kausf;
  uint8_t *kseaf = nas->security.kseaf;
  uint8_t *kamf = nas->security.kamf;
  uint8_t *output = nas->security.res;
  uint8_t *rand = nas->security.rand;
  uint8_t *kgnb = nas->security.kgnb;

  // get RAND for authentication request
  for (int index = 0; index < 16; index++) {
    rand[index] = buf[8 + index];
  }

  uint8_t resTemp[16];
  uint8_t ck[16], ik[16];
  f2345(nas->uicc->key, rand, resTemp, ck, ik, ak, nas->uicc->opc);

  transferRES(ck, ik, resTemp, rand, output, nas->uicc);

  for (int index = 0; index < 6; index++) {
    sqn[index] = buf[26 + index];
  }

  derive_kausf(ck, ik, sqn, kausf, nas->uicc);
  derive_kseaf(kausf, kseaf, nas->uicc);
  derive_kamf(kseaf, kamf, 0x0000, nas->uicc);
  derive_kgnb(kamf, 0, kgnb);

  printf("kausf:");
  for (int i = 0; i < 32; i++) {
    printf("%x ", kausf[i]);
  }
  printf("\n");

  printf("kseaf:");
  for (int i = 0; i < 32; i++) {
    printf("%x ", kseaf[i]);
  }

  printf("\n");

  printf("kamf:");
  for (int i = 0; i < 32; i++) {
    printf("%x ", kamf[i]);
  }
  printf("\n");
}

nr_ue_nas_t *get_ue_nas_info(module_id_t module_id)
{
  DevAssert(module_id < MAX_NAS_UE);
  if (!nr_ue_nas[module_id].uicc) {
    nr_ue_nas[module_id].uicc = checkUicc(module_id);
    nr_ue_nas[module_id].UE_id = module_id;
  }
  return &nr_ue_nas[module_id];
}

void generateRegistrationRequest(as_nas_info_t *initialNasMsg, nr_ue_nas_t *nas)
{
  int size = sizeof(mm_msg_header_t);
  fgs_nas_message_t nas_msg = {0};
  MM_msg *mm_msg;

  mm_msg = &nas_msg.plain.mm_msg;
  // set header
  mm_msg->header.ex_protocol_discriminator = FGS_MOBILITY_MANAGEMENT_MESSAGE;
  mm_msg->header.security_header_type = PLAIN_5GS_MSG;
  mm_msg->header.message_type = REGISTRATION_REQUEST;

  // set registration request
  mm_msg->registration_request.protocoldiscriminator = FGS_MOBILITY_MANAGEMENT_MESSAGE;
  size += 1;
  mm_msg->registration_request.securityheadertype = PLAIN_5GS_MSG;
  size += 1;
  mm_msg->registration_request.messagetype = REGISTRATION_REQUEST;
  size += 1;
  mm_msg->registration_request.fgsregistrationtype = INITIAL_REGISTRATION;
  mm_msg->registration_request.naskeysetidentifier.naskeysetidentifier = 1;
  size += 1;
  if (nas->guti) {
    size += fill_guti(&mm_msg->registration_request.fgsmobileidentity, nas->guti);
  } else {
    size += fill_suci(&mm_msg->registration_request.fgsmobileidentity, nas->uicc);
  }

#if 0
  /* This cannot be sent in clear, the core network Open5GS rejects the UE.
   * TODO: do we have to send this at some point?
   * For the time being, let's keep it here for later proper fix.
   */
  mm_msg->registration_request.presencemask |= REGISTRATION_REQUEST_5GMM_CAPABILITY_PRESENT;
  mm_msg->registration_request.fgmmcapability.iei = REGISTRATION_REQUEST_5GMM_CAPABILITY_IEI;
  mm_msg->registration_request.fgmmcapability.length = 1;
  mm_msg->registration_request.fgmmcapability.value = 0x7;
  size += 3;
#endif

  mm_msg->registration_request.presencemask |= REGISTRATION_REQUEST_UE_SECURITY_CAPABILITY_PRESENT;
  mm_msg->registration_request.nruesecuritycapability.iei = REGISTRATION_REQUEST_UE_SECURITY_CAPABILITY_IEI;
  mm_msg->registration_request.nruesecuritycapability.length = 8;
  mm_msg->registration_request.nruesecuritycapability.fg_EA = 0xe0;
  mm_msg->registration_request.nruesecuritycapability.fg_IA = 0x60;
  mm_msg->registration_request.nruesecuritycapability.EEA = 0;
  mm_msg->registration_request.nruesecuritycapability.EIA = 0;
  size += 10;

  // encode the message
  initialNasMsg->data = malloc16_clear(size * sizeof(Byte_t));
  nas->registration_request_buf = initialNasMsg->data;

  initialNasMsg->length = mm_msg_encode(mm_msg, (uint8_t *)(initialNasMsg->data), size);
  nas->registration_request_len = initialNasMsg->length;
}

void generateIdentityResponse(as_nas_info_t *initialNasMsg, uint8_t identitytype, uicc_t *uicc)
{
  int size = sizeof(mm_msg_header_t);
  fgs_nas_message_t nas_msg;
  memset(&nas_msg, 0, sizeof(fgs_nas_message_t));
  MM_msg *mm_msg;

  mm_msg = &nas_msg.plain.mm_msg;
  // set header
  mm_msg->header.ex_protocol_discriminator = FGS_MOBILITY_MANAGEMENT_MESSAGE;
  mm_msg->header.security_header_type = PLAIN_5GS_MSG;
  mm_msg->header.message_type = FGS_IDENTITY_RESPONSE;

  // set identity response
  mm_msg->fgs_identity_response.protocoldiscriminator = FGS_MOBILITY_MANAGEMENT_MESSAGE;
  size += 1;
  mm_msg->fgs_identity_response.securityheadertype = PLAIN_5GS_MSG;
  size += 1;
  mm_msg->fgs_identity_response.messagetype = FGS_IDENTITY_RESPONSE;
  size += 1;
  if (identitytype == FGS_MOBILE_IDENTITY_SUCI) {
    size += fill_suci(&mm_msg->fgs_identity_response.fgsmobileidentity, uicc);
  }

  // encode the message
  initialNasMsg->data = (Byte_t *)malloc(size * sizeof(Byte_t));

  initialNasMsg->length = mm_msg_encode(mm_msg, (uint8_t *)(initialNasMsg->data), size);
}

static void generateAuthenticationResp(nr_ue_nas_t *nas, as_nas_info_t *initialNasMsg, uint8_t *buf)
{
  derive_ue_keys(buf, nas);
  OctetString res;
  res.length = 16;
  res.value = calloc(1, 16);
  memcpy(res.value, nas->security.res, 16);

  int size = sizeof(mm_msg_header_t);
  fgs_nas_message_t nas_msg;
  memset(&nas_msg, 0, sizeof(fgs_nas_message_t));
  MM_msg *mm_msg;

  mm_msg = &nas_msg.plain.mm_msg;
  // set header
  mm_msg->header.ex_protocol_discriminator = FGS_MOBILITY_MANAGEMENT_MESSAGE;
  mm_msg->header.security_header_type = PLAIN_5GS_MSG;
  mm_msg->header.message_type = FGS_AUTHENTICATION_RESPONSE;

  // set authentication response
  mm_msg->fgs_identity_response.protocoldiscriminator = FGS_MOBILITY_MANAGEMENT_MESSAGE;
  size += 1;
  mm_msg->fgs_identity_response.securityheadertype = PLAIN_5GS_MSG;
  size += 1;
  mm_msg->fgs_identity_response.messagetype = FGS_AUTHENTICATION_RESPONSE;
  size += 1;

  // set response parameter
  mm_msg->fgs_auth_response.authenticationresponseparameter.res = res;
  size += 18;
  // encode the message
  initialNasMsg->data = (Byte_t *)malloc(size * sizeof(Byte_t));

  initialNasMsg->length = mm_msg_encode(mm_msg, (uint8_t *)(initialNasMsg->data), size);
  // Free res value after encode
  free(res.value);
}

int nas_itti_kgnb_refresh_req(instance_t instance, const uint8_t kgnb[32])
{
  MessageDef *message_p;
  message_p = itti_alloc_new_message(TASK_NAS_NRUE, instance, NAS_KENB_REFRESH_REQ);
  memcpy(NAS_KENB_REFRESH_REQ(message_p).kenb, kgnb, sizeof(NAS_KENB_REFRESH_REQ(message_p).kenb));
  return itti_send_msg_to_task(TASK_RRC_NRUE, instance, message_p);
}

static void generateSecurityModeComplete(nr_ue_nas_t *nas, as_nas_info_t *initialNasMsg)
{
  int size = sizeof(mm_msg_header_t);
  fgs_nas_message_t nas_msg;
  memset(&nas_msg, 0, sizeof(fgs_nas_message_t));

  MM_msg *mm_msg;
  nas_stream_cipher_t stream_cipher;
  uint8_t mac[4];
  // set security protected header
  nas_msg.header.protocol_discriminator = FGS_MOBILITY_MANAGEMENT_MESSAGE;
  nas_msg.header.security_header_type = INTEGRITY_PROTECTED_AND_CIPHERED_WITH_NEW_SECU_CTX;
  nas_msg.header.sequence_number = nas->security.nas_count_ul & 0xff;
  size += 7;

  mm_msg = &nas_msg.security_protected.plain.mm_msg;

  // set header
  mm_msg->header.ex_protocol_discriminator = FGS_MOBILITY_MANAGEMENT_MESSAGE;
  mm_msg->header.security_header_type = PLAIN_5GS_MSG;
  mm_msg->header.message_type = FGS_SECURITY_MODE_COMPLETE;

  // set security mode complete
  mm_msg->fgs_security_mode_complete.protocoldiscriminator = FGS_MOBILITY_MANAGEMENT_MESSAGE;
  size += 1;
  mm_msg->fgs_security_mode_complete.securityheadertype = PLAIN_5GS_MSG;
  size += 1;
  mm_msg->fgs_security_mode_complete.messagetype = FGS_SECURITY_MODE_COMPLETE;
  size += 1;

  size += fill_imeisv(&mm_msg->fgs_security_mode_complete.fgsmobileidentity, nas->uicc);

  mm_msg->fgs_security_mode_complete.fgsnasmessagecontainer.nasmessagecontainercontents.value = nas->registration_request_buf;
  mm_msg->fgs_security_mode_complete.fgsnasmessagecontainer.nasmessagecontainercontents.length = nas->registration_request_len;
  size += (nas->registration_request_len + 2);

  // encode the message
  initialNasMsg->data = (Byte_t *)malloc(size * sizeof(Byte_t));

  int security_header_len = nas_protected_security_header_encode((char *)(initialNasMsg->data), &(nas_msg.header), size);

  initialNasMsg->length =
      security_header_len
      + mm_msg_encode(mm_msg, (uint8_t *)(initialNasMsg->data + security_header_len), size - security_header_len);

  /* ciphering */
  uint8_t buf[initialNasMsg->length - 7];
  stream_cipher.context = nas->security_container->ciphering_context;
  AssertFatal(nas->security.nas_count_ul <= 0xffffff, "fatal: NAS COUNT UL too big (todo: fix that)\n");
  stream_cipher.count = nas->security.nas_count_ul;
  stream_cipher.bearer = 1;
  stream_cipher.direction = 0;
  stream_cipher.message = (unsigned char *)(initialNasMsg->data + 7);
  /* length in bits */
  stream_cipher.blength = (initialNasMsg->length - 7) << 3;
  stream_compute_encrypt(nas->security_container->ciphering_algorithm, &stream_cipher, buf);
  memcpy(stream_cipher.message, buf, initialNasMsg->length - 7);

  /* integrity protection */
  stream_cipher.context = nas->security_container->integrity_context;
  stream_cipher.count = nas->security.nas_count_ul++;
  stream_cipher.bearer = 1;
  stream_cipher.direction = 0;
  stream_cipher.message = (unsigned char *)(initialNasMsg->data + 6);
  /* length in bits */
  stream_cipher.blength = (initialNasMsg->length - 6) << 3;

  stream_compute_integrity(nas->security_container->integrity_algorithm, &stream_cipher, mac);

  printf("mac %x %x %x %x \n", mac[0], mac[1], mac[2], mac[3]);
  for (int i = 0; i < 4; i++) {
    initialNasMsg->data[2 + i] = mac[i];
  }
}

static void handle_security_mode_command(nr_ue_nas_t *nas, as_nas_info_t *initialNasMsg, uint8_t *pdu, int pdu_length)
{
  /* retrieve integrity and ciphering algorithms  */
  AssertFatal(pdu_length > 10, "nas: bad pdu\n");
  int ciphering_algorithm = (pdu[10] >> 4) & 0x0f;
  int integrity_algorithm = pdu[10] & 0x0f;

  uint8_t *kamf = nas->security.kamf;
  uint8_t *knas_enc = nas->security.knas_enc;
  uint8_t *knas_int = nas->security.knas_int;

  /* derive keys */
  derive_knas(0x01, ciphering_algorithm, kamf, knas_enc);
  derive_knas(0x02, integrity_algorithm, kamf, knas_int);

  printf("knas_int: ");
  for (int i = 0; i < 16; i++) {
    printf("%x ", knas_int[i]);
  }
  printf("\n");

  printf("knas_enc: ");
  for (int i = 0; i < 16; i++) {
    printf("%x ", knas_enc[i]);
  }
  printf("\n");

  /* todo: stream_security_container_delete() is not called anywhere, deal with that */
  nas->security_container = stream_security_container_init(ciphering_algorithm, integrity_algorithm, knas_enc, knas_int);

  nas_itti_kgnb_refresh_req(nas->UE_id, nas->security.kgnb);
  generateSecurityModeComplete(nas, initialNasMsg);
}

static void decodeRegistrationAccept(const uint8_t *buf, int len, nr_ue_nas_t *nas)
{
  registration_accept_msg reg_acc = {0};
  /* it seems there is no 5G corresponding emm_msg_decode() function, so here
   * we just jump to the right decision */
  buf += 7; /* skip security header */
  buf += 2; /* skip prot discriminator, security header, half octet */
  AssertFatal(*buf == 0x42, "this is not a NAS Registration Accept\n");
  buf++;
  int decoded = decode_registration_accept(&reg_acc, buf, len);
  AssertFatal(decoded > 0, "could not decode registration accept\n");
  if (reg_acc.guti) {
    AssertFatal(reg_acc.guti->guti.typeofidentity == FGS_MOBILE_IDENTITY_5G_GUTI,
                "registration accept 5GS Mobile Identity is not GUTI, but %d\n",
                reg_acc.guti->guti.typeofidentity);
    nas->guti = malloc(sizeof(*nas->guti));
    AssertFatal(nas->guti, "out of memory\n");
    *nas->guti = reg_acc.guti->guti;
    free(reg_acc.guti); /* no proper memory management for NAS decoded messages */
  } else {
    LOG_W(NAS, "no GUTI in registration accept\n");
  }
}

static void generateRegistrationComplete(nr_ue_nas_t *nas,
                                         as_nas_info_t *initialNasMsg,
                                         SORTransparentContainer *sortransparentcontainer)
{
  int length = 0;
  int size = 0;
  fgs_nas_message_t nas_msg;
  nas_stream_cipher_t stream_cipher;
  uint8_t mac[4];
  memset(&nas_msg, 0, sizeof(fgs_nas_message_t));
  fgs_nas_message_security_protected_t *sp_msg;

  sp_msg = &nas_msg.security_protected;
  // set header
  sp_msg->header.protocol_discriminator = FGS_MOBILITY_MANAGEMENT_MESSAGE;
  sp_msg->header.security_header_type = INTEGRITY_PROTECTED_AND_CIPHERED;
  sp_msg->header.message_authentication_code = 0;
  sp_msg->header.sequence_number = nas->security.nas_count_ul & 0xff;
  length = 7;
  sp_msg->plain.mm_msg.registration_complete.protocoldiscriminator = FGS_MOBILITY_MANAGEMENT_MESSAGE;
  length += 1;
  sp_msg->plain.mm_msg.registration_complete.securityheadertype = PLAIN_5GS_MSG;
  sp_msg->plain.mm_msg.registration_complete.sparehalfoctet = 0;
  length += 1;
  sp_msg->plain.mm_msg.registration_complete.messagetype = REGISTRATION_COMPLETE;
  length += 1;

  if (sortransparentcontainer) {
    length += sortransparentcontainer->sortransparentcontainercontents.length;
  }

  // encode the message
  initialNasMsg->data = (Byte_t *)malloc(length * sizeof(Byte_t));
  initialNasMsg->length = length;

  /* Encode the first octet of the header (extended protocol discriminator) */
  ENCODE_U8(initialNasMsg->data + size, sp_msg->header.protocol_discriminator, size);

  /* Encode the security header type */
  ENCODE_U8(initialNasMsg->data + size, sp_msg->header.security_header_type, size);

  /* Encode the message authentication code */
  ENCODE_U32(initialNasMsg->data + size, sp_msg->header.message_authentication_code, size);

  /* Encode the sequence number */
  ENCODE_U8(initialNasMsg->data + size, sp_msg->header.sequence_number, size);

  /* Encode the extended protocol discriminator */
  ENCODE_U8(initialNasMsg->data + size, sp_msg->plain.mm_msg.registration_complete.protocoldiscriminator, size);

  /* Encode the security header type */
  ENCODE_U8(initialNasMsg->data + size, sp_msg->plain.mm_msg.registration_complete.securityheadertype, size);

  /* Encode the message type */
  ENCODE_U8(initialNasMsg->data + size, sp_msg->plain.mm_msg.registration_complete.messagetype, size);

  if (sortransparentcontainer) {
    encode_registration_complete(&sp_msg->plain.mm_msg.registration_complete, initialNasMsg->data + size, length - size);
  }

  /* ciphering */
  uint8_t buf[initialNasMsg->length - 7];
  stream_cipher.context = nas->security_container->ciphering_context;
  AssertFatal(nas->security.nas_count_ul <= 0xffffff, "fatal: NAS COUNT UL too big (todo: fix that)\n");
  stream_cipher.count = nas->security.nas_count_ul;
  stream_cipher.bearer = 1;
  stream_cipher.direction = 0;
  stream_cipher.message = (unsigned char *)(initialNasMsg->data + 7);
  /* length in bits */
  stream_cipher.blength = (initialNasMsg->length - 7) << 3;
  stream_compute_encrypt(nas->security_container->ciphering_algorithm, &stream_cipher, buf);
  memcpy(stream_cipher.message, buf, initialNasMsg->length - 7);

  /* integrity protection */
  stream_cipher.context = nas->security_container->integrity_context;
  stream_cipher.count = nas->security.nas_count_ul++;
  stream_cipher.bearer = 1;
  stream_cipher.direction = 0;
  stream_cipher.message = (unsigned char *)(initialNasMsg->data + 6);
  /* length in bits */
  stream_cipher.blength = (initialNasMsg->length - 6) << 3;
  stream_compute_integrity(nas->security_container->integrity_algorithm, &stream_cipher, mac);

  printf("mac %x %x %x %x \n", mac[0], mac[1], mac[2], mac[3]);
  for (int i = 0; i < 4; i++) {
    initialNasMsg->data[2 + i] = mac[i];
  }
}

void decodeDownlinkNASTransport(as_nas_info_t *initialNasMsg, uint8_t *pdu_buffer)
{
  uint8_t msg_type = *(pdu_buffer + 16);
  if (msg_type == FGS_PDU_SESSION_ESTABLISHMENT_ACC) {
    uint8_t *ip_p = pdu_buffer + 39;
    char ip[20];
    sprintf(ip, "%d.%d.%d.%d", *(ip_p), *(ip_p + 1), *(ip_p + 2), *(ip_p + 3));
    LOG_A(NAS, "Received PDU Session Establishment Accept\n");
    tun_config(1, ip, NULL, "oaitun_ue");
    setup_ue_ipv4_route(1, ip, "oaitun_ue");
  } else {
    LOG_E(NAS, "Received unexpected message in DLinformationTransfer %d\n", msg_type);
  }
}

static void generateDeregistrationRequest(nr_ue_nas_t *nas, as_nas_info_t *initialNasMsg, const nas_deregistration_req_t *req)
{
  fgs_nas_message_t nas_msg = {0};
  fgs_nas_message_security_protected_t *sp_msg;
  sp_msg = &nas_msg.security_protected;
  sp_msg->header.protocol_discriminator = FGS_MOBILITY_MANAGEMENT_MESSAGE;
  sp_msg->header.security_header_type = INTEGRITY_PROTECTED_AND_CIPHERED;
  sp_msg->header.message_authentication_code = 0;
  sp_msg->header.sequence_number = nas->security.nas_count_ul & 0xff;
  int size = sizeof(fgs_nas_message_security_header_t);

  fgs_deregistration_request_ue_originating_msg *dereg_req = &sp_msg->plain.mm_msg.fgs_deregistration_request_ue_originating;
  dereg_req->protocoldiscriminator = FGS_MOBILITY_MANAGEMENT_MESSAGE;
  size += 1;
  dereg_req->securityheadertype = INTEGRITY_PROTECTED_AND_CIPHERED_WITH_NEW_SECU_CTX;
  size += 1;
  dereg_req->messagetype = FGS_DEREGISTRATION_REQUEST_UE_ORIGINATING;
  size += 1;
  dereg_req->deregistrationtype.switchoff = NORMAL_DEREGISTRATION;
  dereg_req->deregistrationtype.reregistration_required = REREGISTRATION_NOT_REQUIRED;
  dereg_req->deregistrationtype.access_type = TGPP_ACCESS;
  dereg_req->naskeysetidentifier.naskeysetidentifier = 1;
  size += 1;
  size += fill_guti(&dereg_req->fgsmobileidentity, nas->guti);

  // encode the message
  initialNasMsg->data = calloc(size, sizeof(Byte_t));
  int security_header_len = nas_protected_security_header_encode((char *)(initialNasMsg->data), &nas_msg.header, size);

  initialNasMsg->length =
      security_header_len
      + mm_msg_encode(&sp_msg->plain.mm_msg, (uint8_t *)(initialNasMsg->data + security_header_len), size - security_header_len);

  nas_stream_cipher_t stream_cipher;

  /* ciphering */
  uint8_t buf[initialNasMsg->length - 7];
  stream_cipher.context = nas->security_container->ciphering_context;
  AssertFatal(nas->security.nas_count_ul <= 0xffffff, "fatal: NAS COUNT UL too big (todo: fix that)\n");
  stream_cipher.count = nas->security.nas_count_ul;
  stream_cipher.bearer = 1;
  stream_cipher.direction = 0;
  stream_cipher.message = (unsigned char *)(initialNasMsg->data + 7);
  /* length in bits */
  stream_cipher.blength = (initialNasMsg->length - 7) << 3;
  stream_compute_encrypt(nas->security_container->ciphering_algorithm, &stream_cipher, buf);
  memcpy(stream_cipher.message, buf, initialNasMsg->length - 7);

  /* integrity protection */
  stream_cipher.context = nas->security_container->integrity_context;
  stream_cipher.count = nas->security.nas_count_ul++;
  stream_cipher.bearer = 1;
  stream_cipher.direction = 0;
  stream_cipher.message = (unsigned char *)(initialNasMsg->data + 6);
  /* length in bits */
  stream_cipher.blength = (initialNasMsg->length - 6) << 3;
  uint8_t mac[4];
  stream_compute_integrity(nas->security_container->integrity_algorithm, &stream_cipher, mac);

  printf("mac %x %x %x %x \n", mac[0], mac[1], mac[2], mac[3]);
  for (int i = 0; i < 4; i++) {
    initialNasMsg->data[2 + i] = mac[i];
  }
}

static void generatePduSessionEstablishRequest(nr_ue_nas_t *nas, as_nas_info_t *initialNasMsg, nas_pdu_session_req_t *pdu_req)
{
  int size = 0;
  fgs_nas_message_t nas_msg = {0};

  // setup pdu session establishment request
  uint16_t req_length = 7;
  uint8_t *req_buffer = malloc(req_length);
  pdu_session_establishment_request_msg pdu_session_establish;
  pdu_session_establish.protocoldiscriminator = FGS_SESSION_MANAGEMENT_MESSAGE;
  pdu_session_establish.pdusessionid = pdu_req->pdusession_id;
  pdu_session_establish.pti = 1;
  pdu_session_establish.pdusessionestblishmsgtype = FGS_PDU_SESSION_ESTABLISHMENT_REQ;
  pdu_session_establish.maxdatarate = 0xffff;
  pdu_session_establish.pdusessiontype = pdu_req->pdusession_type;
  encode_pdu_session_establishment_request(&pdu_session_establish, req_buffer);

  MM_msg *mm_msg;
  nas_stream_cipher_t stream_cipher;
  uint8_t mac[4];
  nas_msg.header.protocol_discriminator = FGS_MOBILITY_MANAGEMENT_MESSAGE;
  nas_msg.header.security_header_type = INTEGRITY_PROTECTED_AND_CIPHERED;
  nas_msg.header.sequence_number = nas->security.nas_count_ul & 0xff;

  size += 7;

  mm_msg = &nas_msg.security_protected.plain.mm_msg;

  // set header
  mm_msg->header.ex_protocol_discriminator = FGS_MOBILITY_MANAGEMENT_MESSAGE;
  mm_msg->header.security_header_type = PLAIN_5GS_MSG;
  mm_msg->header.message_type = FGS_UPLINK_NAS_TRANSPORT;

  // set uplink nas transport
  mm_msg->uplink_nas_transport.protocoldiscriminator = FGS_MOBILITY_MANAGEMENT_MESSAGE;
  size += 1;
  mm_msg->uplink_nas_transport.securityheadertype = PLAIN_5GS_MSG;
  size += 1;
  mm_msg->uplink_nas_transport.messagetype = FGS_UPLINK_NAS_TRANSPORT;
  size += 1;

  mm_msg->uplink_nas_transport.payloadcontainertype.iei = 0;
  mm_msg->uplink_nas_transport.payloadcontainertype.type = 1;
  size += 1;
  mm_msg->uplink_nas_transport.fgspayloadcontainer.payloadcontainercontents.length = req_length;
  mm_msg->uplink_nas_transport.fgspayloadcontainer.payloadcontainercontents.value = req_buffer;
  size += (2 + req_length);
  mm_msg->uplink_nas_transport.pdusessionid = pdu_req->pdusession_id;
  mm_msg->uplink_nas_transport.requesttype = 1;
  size += 3;
  const bool has_nssai_sd = pdu_req->sd != 0xffffff; // 0xffffff means "no SD", TS 23.003
  const size_t nssai_len = has_nssai_sd ? 4 : 1;
  mm_msg->uplink_nas_transport.snssai.length = nssai_len;
  // Fixme: it seems there are a lot of memory errors in this: this value was on the stack,
  //  but pushed  in a itti message to another thread
  //  this kind of error seems in many places in 5G NAS
  mm_msg->uplink_nas_transport.snssai.value = calloc(1, nssai_len);
  mm_msg->uplink_nas_transport.snssai.value[0] = pdu_req->sst;
  if (has_nssai_sd)
    INT24_TO_BUFFER(pdu_req->sd, &mm_msg->uplink_nas_transport.snssai.value[1]);
  size += 1 + 1 + nssai_len;
  int dnnSize = strlen(nas->uicc->dnnStr);
  mm_msg->uplink_nas_transport.dnn.value = calloc(1, dnnSize + 1);
  mm_msg->uplink_nas_transport.dnn.length = dnnSize + 1;
  mm_msg->uplink_nas_transport.dnn.value[0] = dnnSize;
  memcpy(mm_msg->uplink_nas_transport.dnn.value + 1, nas->uicc->dnnStr, dnnSize);
  size += (1 + 1 + dnnSize + 1);

  // encode the message
  initialNasMsg->data = (Byte_t *)malloc(size * sizeof(Byte_t));
  int security_header_len = nas_protected_security_header_encode((char *)(initialNasMsg->data), &(nas_msg.header), size);

  initialNasMsg->length =
      security_header_len
      + mm_msg_encode(mm_msg, (uint8_t *)(initialNasMsg->data + security_header_len), size - security_header_len);

  // Free allocated memory after encode
  free(req_buffer);
  free(mm_msg->uplink_nas_transport.dnn.value);
  free(mm_msg->uplink_nas_transport.snssai.value);

  /* ciphering */
  uint8_t buf[initialNasMsg->length - 7];
  stream_cipher.context = nas->security_container->ciphering_context;
  AssertFatal(nas->security.nas_count_ul <= 0xffffff, "fatal: NAS COUNT UL too big (todo: fix that)\n");
  stream_cipher.count = nas->security.nas_count_ul;
  stream_cipher.bearer = 1;
  stream_cipher.direction = 0;
  stream_cipher.message = (unsigned char *)(initialNasMsg->data + 7);
  /* length in bits */
  stream_cipher.blength = (initialNasMsg->length - 7) << 3;
  stream_compute_encrypt(nas->security_container->ciphering_algorithm, &stream_cipher, buf);
  memcpy(stream_cipher.message, buf, initialNasMsg->length - 7);

  /* integrity protection */
  stream_cipher.context = nas->security_container->integrity_context;
  stream_cipher.count = nas->security.nas_count_ul++;
  stream_cipher.bearer = 1;
  stream_cipher.direction = 0;
  stream_cipher.message = (unsigned char *)(initialNasMsg->data + 6);
  /* length in bits */
  stream_cipher.blength = (initialNasMsg->length - 6) << 3;
  stream_compute_integrity(nas->security_container->integrity_algorithm, &stream_cipher, mac);

  printf("mac %x %x %x %x \n", mac[0], mac[1], mac[2], mac[3]);
  for (int i = 0; i < 4; i++) {
    initialNasMsg->data[2 + i] = mac[i];
  }
}

static uint8_t get_msg_type(uint8_t *pdu_buffer, uint32_t length)
{
  if (pdu_buffer == NULL)
    goto error;

  /* get security header type */
  if (length < 2)
    goto error;

  int security_header_type = pdu_buffer[1];

  if (security_header_type == 0) {
    /* plain NAS message */
    if (length < 3)
      goto error;
    return pdu_buffer[2];
  }

  if (length < 10)
    goto error;

  int msg_type = pdu_buffer[9];

  if (msg_type == FGS_DOWNLINK_NAS_TRANSPORT) {
    if (length < 17)
      goto error;

    msg_type = pdu_buffer[16];
  }

  return msg_type;

error:
  LOG_E(NAS, "[UE] Received invalid downlink message\n");
  return 0;
}

static void send_nas_uplink_data_req(nr_ue_nas_t *nas, const as_nas_info_t *initial_nas_msg)
{
  MessageDef *msg = itti_alloc_new_message(TASK_NAS_NRUE, nas->UE_id, NAS_UPLINK_DATA_REQ);
  ul_info_transfer_req_t *req = &NAS_UPLINK_DATA_REQ(msg);
  req->UEid = nas->UE_id;
  req->nasMsg.data = (uint8_t *)initial_nas_msg->data;
  req->nasMsg.length = initial_nas_msg->length;
  itti_send_msg_to_task(TASK_RRC_NRUE, nas->UE_id, msg);
}

static void send_nas_detach_req(nr_ue_nas_t *nas, bool wait_release)
{
  MessageDef *msg = itti_alloc_new_message(TASK_NAS_NRUE, nas->UE_id, NAS_DETACH_REQ);
  nas_detach_req_t *req = &NAS_DETACH_REQ(msg);
  req->wait_release = wait_release;
  itti_send_msg_to_task(TASK_RRC_NRUE, nas->UE_id, msg);
}

static void parse_allowed_nssai(nr_nas_msg_snssai_t nssaiList[8], const uint8_t *buf, const uint32_t len)
{
  int nssai_cnt = 0;
  const uint8_t *end = buf + len;
  while (buf < end) {
    const int length = *buf++;
    nr_nas_msg_snssai_t *nssai = nssaiList + nssai_cnt;
    nssai->sd = 0xffffff;

    switch (length) {
      case 1:
        nssai->sst = *buf++;
        nssai_cnt++;
        break;

      case 2:
        nssai->sst = *buf++;
        nssai->hplmn_sst = *buf++;
        nssai_cnt++;
        break;

      case 4:
        nssai->sst = *buf++;
        nssai->sd = 0xffffff & ntoh_int24_buf(buf);
        buf += 3;
        nssai_cnt++;
        break;

      case 5:
        nssai->sst = *buf++;
        nssai->sd = 0xffffff & ntoh_int24_buf(buf);
        buf += 3;
        nssai->hplmn_sst = *buf++;
        nssai_cnt++;
        break;

      case 8:
        nssai->sst = *buf++;
        nssai->sd = 0xffffff & ntoh_int24_buf(buf);
        buf += 3;
        nssai->hplmn_sst = *buf++;
        nssai->hplmn_sd = 0xffffff & ntoh_int24_buf(buf);
        buf += 3;
        nssai_cnt++;
        break;

      default:
        LOG_E(NAS, "UE received unknown length in an allowed S-NSSAI\n");
        break;
    }
  }
}

/* Extract Allowed NSSAI from Regestration Accept according to
   3GPP TS 24.501 Table 8.2.7.1.1
*/
static void get_allowed_nssai(nr_nas_msg_snssai_t nssai[8], const uint8_t *pdu_buffer, const uint32_t pdu_length)
{
  if ((pdu_buffer == NULL) || (pdu_length <= 0))
    return;

  const uint8_t *end = pdu_buffer + pdu_length;
  if (((nas_msg_header_t *)(pdu_buffer))->choice.security_protected_nas_msg_header_t.security_header_type > 0) {
    pdu_buffer += SECURITY_PROTECTED_5GS_NAS_MESSAGE_HEADER_LENGTH;
  }

  pdu_buffer += 1 + 1 + 1 + 2; // Mandatory fields offset
  /* optional fields */
  while (pdu_buffer < end) {
    const int type = *pdu_buffer++;
    int length = 0;
    switch (type) {
      case 0x77: // 5GS mobile identity
        pdu_buffer += ntoh_int16_buf(pdu_buffer) + sizeof(uint16_t);
        break;

      case 0x4A: // PLMN list
      case 0x54: // 5GS tracking area identity
        pdu_buffer += *pdu_buffer + 1; // offset length + 1 byte which contains the length
        break;

      case 0x15: // allowed NSSAI
        length = *pdu_buffer++;
        parse_allowed_nssai(nssai, pdu_buffer, length);
        pdu_buffer += length;
        break;

      default:
        LOG_W(NAS, "This NAS IEI (0x%2.2x) is not handled when extracting list of allowed NSSAI\n", type);
        length = *pdu_buffer++;
        pdu_buffer += length;
        break;
    }
  }
}

static void request_default_pdusession(nr_ue_nas_t *nas, int nssai_idx)
{
  MessageDef *message_p = itti_alloc_new_message(TASK_NAS_NRUE, nas->UE_id, NAS_PDU_SESSION_REQ);
  NAS_PDU_SESSION_REQ(message_p).pdusession_id = 10; /* first or default pdu session */
  NAS_PDU_SESSION_REQ(message_p).pdusession_type = 0x91; // 0x91 = IPv4, 0x92 = IPv6, 0x93 = IPv4v6
  NAS_PDU_SESSION_REQ(message_p).sst = nas_allowed_nssai[nssai_idx].sst;
  NAS_PDU_SESSION_REQ(message_p).sd = nas_allowed_nssai[nssai_idx].sd;
  itti_send_msg_to_task(TASK_NAS_NRUE, nas->UE_id, message_p);
}

static int get_user_nssai_idx(const nr_nas_msg_snssai_t allowed_nssai[8], const nr_ue_nas_t *nas)
{
  for (int i = 0; i < 8; i++) {
    const nr_nas_msg_snssai_t *nssai = allowed_nssai + i;
    if ((nas->uicc->nssai_sst == nssai->sst) && (nas->uicc->nssai_sd == nssai->sd))
      return i;
  }
  return -1;
}

void *nas_nrue_task(void *args_p)
{
  for (int UE_id = 0; UE_id < NB_UE_INST; UE_id++) {
    // This sets UE uicc from command line. Needs to be called 2 seconds into nr-ue runtime, otherwise the unused command line
    // arguments will be reported as unused and the modem asserts.
    (void)get_ue_nas_info(UE_id);
  }
  while (1) {
    nas_nrue(NULL);
  }
}

static void handle_registration_accept(nr_ue_nas_t *nas, const uint8_t *pdu_buffer, uint32_t msg_length)
{
  LOG_I(NAS, "[UE] Received REGISTRATION ACCEPT message\n");
  decodeRegistrationAccept(pdu_buffer, msg_length, nas);
  get_allowed_nssai(nas_allowed_nssai, pdu_buffer, msg_length);

  as_nas_info_t initialNasMsg = {0};
  generateRegistrationComplete(nas, &initialNasMsg, NULL);
  if (initialNasMsg.length > 0) {
    send_nas_uplink_data_req(nas, &initialNasMsg);
    LOG_I(NAS, "Send NAS_UPLINK_DATA_REQ message(RegistrationComplete)\n");
  }
  const int nssai_idx = get_user_nssai_idx(nas_allowed_nssai, nas);
  if (nssai_idx < 0) {
    LOG_E(NAS, "NSSAI parameters not match with allowed NSSAI. Couldn't request PDU session.\n");
  } else {
    request_default_pdusession(nas, nssai_idx);
  }
}

void *nas_nrue(void *args_p)
{
  // Wait for a message or an event
  MessageDef *msg_p;
  itti_receive_msg(TASK_NAS_NRUE, &msg_p);

  if (msg_p != NULL) {
    nr_ue_nas_t *nas = get_ue_nas_info(msg_p->ittiMsgHeader.destinationInstance);

    switch (ITTI_MSG_ID(msg_p)) {
      case INITIALIZE_MESSAGE:

        break;

      case TERMINATE_MESSAGE:
        itti_exit_task();
        break;

      case MESSAGE_TEST:
        break;

      case NAS_CELL_SELECTION_CNF:
        LOG_I(NAS,
              "[UE %ld] Received %s: errCode %u, cellID %u, tac %u\n",
              nas->UE_id,
              ITTI_MSG_NAME(msg_p),
              NAS_CELL_SELECTION_CNF(msg_p).errCode,
              NAS_CELL_SELECTION_CNF(msg_p).cellID,
              NAS_CELL_SELECTION_CNF(msg_p).tac);
        // as_stmsi_t s_tmsi={0, 0};
        // as_nas_info_t nas_info;
        // plmn_t plmnID={0, 0, 0, 0};
        // generateRegistrationRequest(&nas_info);
        // nr_nas_itti_nas_establish_req(0, AS_TYPE_ORIGINATING_SIGNAL, s_tmsi, plmnID, nas_info.data, nas_info.length, 0);
        break;

      case NAS_CELL_SELECTION_IND:
        LOG_I(NAS,
              "[UE %ld] Received %s: cellID %u, tac %u\n",
              nas->UE_id,
              ITTI_MSG_NAME(msg_p),
              NAS_CELL_SELECTION_IND(msg_p).cellID,
              NAS_CELL_SELECTION_IND(msg_p).tac);

        /* TODO not processed by NAS currently */
        break;

      case NAS_PAGING_IND:
        LOG_I(NAS, "[UE %ld] Received %s: cause %u\n", nas->UE_id, ITTI_MSG_NAME(msg_p), NAS_PAGING_IND(msg_p).cause);

        /* TODO not processed by NAS currently */
        break;

      case NAS_PDU_SESSION_REQ: {
        as_nas_info_t pduEstablishMsg = {0};
        nas_pdu_session_req_t *pduReq = &NAS_PDU_SESSION_REQ(msg_p);
        generatePduSessionEstablishRequest(nas, &pduEstablishMsg, pduReq);
        if (pduEstablishMsg.length > 0) {
          send_nas_uplink_data_req(nas, &pduEstablishMsg);
          LOG_I(NAS, "Send NAS_UPLINK_DATA_REQ message(PduSessionEstablishRequest)\n");
        }
        break;
      }

      case NAS_CONN_ESTABLI_CNF: {
        LOG_I(NAS,
              "[UE %ld] Received %s: errCode %u, length %u\n",
              nas->UE_id,
              ITTI_MSG_NAME(msg_p),
              NAS_CONN_ESTABLI_CNF(msg_p).errCode,
              NAS_CONN_ESTABLI_CNF(msg_p).nasMsg.length);

        uint8_t *pdu_buffer = NAS_CONN_ESTABLI_CNF(msg_p).nasMsg.data;
        int pdu_length = NAS_CONN_ESTABLI_CNF(msg_p).nasMsg.length;

        security_state_t security_state = nas_security_rx_process(nas, pdu_buffer, pdu_length);
        if (security_state != NAS_SECURITY_INTEGRITY_PASSED && security_state != NAS_SECURITY_NO_SECURITY_CONTEXT) {
          LOG_E(NAS, "NAS integrity failed, discard incoming message\n");
          break;
        }

        int msg_type = get_msg_type(pdu_buffer, pdu_length);

        if (msg_type == REGISTRATION_ACCEPT) {
          handle_registration_accept(nas, pdu_buffer, pdu_length);
        } else if (msg_type == FGS_PDU_SESSION_ESTABLISHMENT_ACC) {
          capture_pdu_session_establishment_accept_msg(pdu_buffer, pdu_length);
        }

        // Free NAS buffer memory after use (coming from RRC)
        free(pdu_buffer);
        break;
      }

      case NR_NAS_CONN_RELEASE_IND:
        LOG_I(NAS, "[UE %ld] Received %s: cause %u\n", nas->UE_id, ITTI_MSG_NAME(msg_p), NR_NAS_CONN_RELEASE_IND(msg_p).cause);
        // TODO handle connection release
        if (nas->termination_procedure) {
          /* the following is not clean, but probably necessary: we need to give
           * time to RLC to Ack the SRB1 PDU which contained the RRC release
           * message. Hence, we just below wait some time, before finally
           * unblocking the nr-uesoftmodem, which will terminate the process. */
          usleep(100000);
          itti_wait_tasks_unblock(); /* will unblock ITTI to stop nr-uesoftmodem */
        }
        break;

      case NAS_UPLINK_DATA_CNF:
        LOG_I(NAS,
              "[UE %ld] Received %s: UEid %u, errCode %u\n",
              nas->UE_id,
              ITTI_MSG_NAME(msg_p),
              NAS_UPLINK_DATA_CNF(msg_p).UEid,
              NAS_UPLINK_DATA_CNF(msg_p).errCode);

        break;

      case NAS_DEREGISTRATION_REQ: {
        LOG_I(NAS, "[UE %ld] Received %s\n", nas->UE_id, ITTI_MSG_NAME(msg_p));
        nas_deregistration_req_t *req = &NAS_DEREGISTRATION_REQ(msg_p);
        if (nas->guti) {
          if (req->cause == AS_DETACH) {
            nas->termination_procedure = true;
            send_nas_detach_req(nas, true);
          }
          as_nas_info_t initialNasMsg = {0};
          generateDeregistrationRequest(nas, &initialNasMsg, req);
          send_nas_uplink_data_req(nas, &initialNasMsg);
        } else {
          LOG_W(NAS, "No GUTI, cannot trigger deregistration request.\n");
          if (req->cause == AS_DETACH)
            send_nas_detach_req(nas, false);
        }
      } break;

      case NAS_DOWNLINK_DATA_IND: {
        LOG_I(NAS,
              "[UE %ld] Received %s: length %u , buffer %p\n",
              nas->UE_id,
              ITTI_MSG_NAME(msg_p),
              NAS_DOWNLINK_DATA_IND(msg_p).nasMsg.length,
              NAS_DOWNLINK_DATA_IND(msg_p).nasMsg.data);
        as_nas_info_t initialNasMsg = {0};

        uint8_t *pdu_buffer = NAS_DOWNLINK_DATA_IND(msg_p).nasMsg.data;
        int pdu_length = NAS_DOWNLINK_DATA_IND(msg_p).nasMsg.length;

        security_state_t security_state = nas_security_rx_process(nas, pdu_buffer, pdu_length);
        /* special cases accepted without protection */
        if (security_state == NAS_SECURITY_UNPROTECTED) {
          int msg_type = get_msg_type(pdu_buffer, pdu_length);
          /* for the moment, only FGS_DEREGISTRATION_ACCEPT is accepted */
          if (msg_type == FGS_DEREGISTRATION_ACCEPT)
            security_state = NAS_SECURITY_INTEGRITY_PASSED;
        }

        if (security_state != NAS_SECURITY_INTEGRITY_PASSED && security_state != NAS_SECURITY_NO_SECURITY_CONTEXT) {
          LOG_E(NAS, "NAS integrity failed, discard incoming message\n");
          break;
        }

        int msg_type = get_msg_type(pdu_buffer, pdu_length);

        switch (msg_type) {
          case FGS_IDENTITY_REQUEST:
            generateIdentityResponse(&initialNasMsg, *(pdu_buffer + 3), nas->uicc);
            break;
          case FGS_AUTHENTICATION_REQUEST:
            generateAuthenticationResp(nas, &initialNasMsg, pdu_buffer);
            break;
          case FGS_SECURITY_MODE_COMMAND:
            handle_security_mode_command(nas, &initialNasMsg, pdu_buffer, pdu_length);
            break;
          case FGS_DOWNLINK_NAS_TRANSPORT:
            decodeDownlinkNASTransport(&initialNasMsg, pdu_buffer);
            break;
          case REGISTRATION_ACCEPT:
            handle_registration_accept(nas, pdu_buffer, pdu_length);
            break;
          case FGS_DEREGISTRATION_ACCEPT:
            LOG_I(NAS, "received deregistration accept\n");
            break;
          case FGS_PDU_SESSION_ESTABLISHMENT_ACC: {
            uint8_t offset = 0;
            uint8_t *payload_container = pdu_buffer;
            offset += SECURITY_PROTECTED_5GS_NAS_MESSAGE_HEADER_LENGTH;
            uint32_t payload_container_length = htons(((dl_nas_transport_t *)(pdu_buffer + offset))->payload_container_length);
            if ((payload_container_length >= PAYLOAD_CONTAINER_LENGTH_MIN)
                && (payload_container_length <= PAYLOAD_CONTAINER_LENGTH_MAX))
              offset += (PLAIN_5GS_NAS_MESSAGE_HEADER_LENGTH + 3);
            if (offset < NAS_CONN_ESTABLI_CNF(msg_p).nasMsg.length)
              payload_container = pdu_buffer + offset;

            while (offset < payload_container_length) {
              if (*(payload_container + offset) == 0x29) { // PDU address IEI
                if ((*(payload_container + offset + 1) == 0x05) && (*(payload_container + offset + 2) == 0x01)) { // IPV4
                  uint8_t *ip_p = payload_container + offset + 3;
                  char ip[20];
                  snprintf(ip, sizeof(ip), "%d.%d.%d.%d", *(ip_p), *(ip_p + 1), *(ip_p + 2), *(ip_p + 3));
                  LOG_I(NAS, "Received PDU Session Establishment Accept, UE IP: %s\n", ip);
                  tun_config(1, ip, NULL, "oaitun_ue");
                  setup_ue_ipv4_route(1, ip, "oaitun_ue");
                  break;
                }
              }
              offset++;
            }
          } break;
          case FGS_PDU_SESSION_ESTABLISHMENT_REJ:
            LOG_E(NAS, "Received PDU Session Establishment reject\n");
            break;
          default:
            LOG_W(NR_RRC, "unknown message type %d\n", msg_type);
            break;
        }
        // Free NAS buffer memory after use (coming from RRC)
        free(pdu_buffer);

        if (initialNasMsg.length > 0)
          send_nas_uplink_data_req(nas, &initialNasMsg);
      } break;

      default:
        LOG_E(NAS, "[UE %ld] Received unexpected message %s\n", nas->UE_id, ITTI_MSG_NAME(msg_p));
        break;
    }

    int result = itti_free(ITTI_MSG_ORIGIN_ID(msg_p), msg_p);
    AssertFatal(result == EXIT_SUCCESS, "Failed to free memory (%d)!\n", result);
  }
  return NULL;
}
