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

#ifndef F1AP_LIB_COMMON_H_
#define F1AP_LIB_COMMON_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "openair3/UTILS/conversions.h"
#include "common/5g_platform_types.h"
#include "common/utils/ds/byte_array.h"

#include "F1AP_Cause.h"
#include "F1AP_SNSSAI.h"
#include "f1ap_messages_types.h"

#ifdef ENABLE_TESTS
  #define PRINT_ERROR(fmt, ...) fprintf(stderr, "%s:%d: " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#else
  #define PRINT_ERROR(...)  // Do nothing
#endif

#define _F1_EQ_CHECK_GENERIC(condition, fmt, ...)                                                                       \
  do {                                                                                                                  \
    if (!(condition)) {                                                                                                 \
      PRINT_ERROR("Equality condition '%s' failed: " fmt " != " fmt "\n", #condition, ##__VA_ARGS__); \
      return false;                                                                                                     \
    }                                                                                                                   \
  } while (0)

#define _F1_EQ_CHECK_LONG(A, B) _F1_EQ_CHECK_GENERIC(A == B, "%ld", A, B);
#define _F1_EQ_CHECK_INT(A, B) _F1_EQ_CHECK_GENERIC(A == B, "%d", A, B);
#define _F1_EQ_CHECK_STR(A, B) _F1_EQ_CHECK_GENERIC(strcmp(A, B) == 0, "'%s'", A, B);

#define _F1_CHECK_EXP(EXP)                 \
  do {                                     \
    if (!(EXP)) {                          \
      PRINT_ERROR("Failed at " #EXP "\n"); \
      return false;                        \
    }                                      \
  } while (0)

#define _F1_EQ_CHECK_OPTIONAL_IE(A, B, FIELD, EQ_MACRO)                        \
  do {                                                                         \
    _F1_CHECK_EXP(((A)->FIELD && (B)->FIELD) || (!(A)->FIELD && !(B)->FIELD)); \
    if ((A)->FIELD && (B)->FIELD)                                              \
      EQ_MACRO(*(A)->FIELD, *(B)->FIELD);                                      \
  } while (0)

/* macro to look up IE. If mandatory and not found, macro will print
 * descriptive debug message to stderr and force exit in calling function */
#define F1AP_LIB_FIND_IE(IE_TYPE, ie, IE_LIST, IE_ID, mandatory)                                     \
  do {                                                                                               \
    ie = NULL;                                                                                       \
    for (IE_TYPE **ptr = (IE_LIST)->array; ptr < &(IE_LIST)->array[(IE_LIST)->count]; ptr++) {       \
      if ((*ptr)->id == IE_ID) {                                                                     \
        ie = *ptr;                                                                                   \
        break;                                                                                       \
      }                                                                                              \
    }                                                                                                \
    if (mandatory && ie == NULL) {                                                                   \
      fprintf(stderr, "%s(): could not find element " #IE_ID " with type " #IE_TYPE "\n", __func__); \
      return false;                                                                                  \
    }                                                                                                \
  } while (0)

#define CP_OPT_BYTE_ARRAY(dst, src)          \
  do {                                       \
    if (src) {                               \
      dst = calloc_or_fail(1, sizeof(*dst)); \
      *(dst) = copy_byte_array(*(src));      \
    }                                        \
  } while (0)

#define FREE_OPT_BYTE_ARRAY(a) \
  do {                         \
    if (a) {                   \
      free_byte_array(*(a));   \
    } \
    free(a);                   \
  } while (0)

/* similar to asn1cCallocOne(), duplicated to not confuse with asn.1 types */
#define _F1_MALLOC(VaR, VaLue)          \
  do {                                  \
    VaR = malloc_or_fail(sizeof(*VaR)); \
    *VaR = VaLue;                       \
  } while (0)

bool eq_f1ap_plmn(const plmn_id_t *a, const plmn_id_t *b);
struct f1ap_served_cell_info_t;
bool eq_f1ap_cell_info(const struct f1ap_served_cell_info_t *a, const struct f1ap_served_cell_info_t *b);
struct f1ap_gnb_du_system_info_t;
bool eq_f1ap_sys_info(const struct f1ap_gnb_du_system_info_t *a, const struct f1ap_gnb_du_system_info_t *b);
struct f1ap_nr_frequency_info_t;
bool eq_f1ap_freq_info(const struct f1ap_nr_frequency_info_t *a, const struct f1ap_nr_frequency_info_t *b);
struct f1ap_transmission_bandwidth_t;
bool eq_f1ap_tx_bandwidth(const struct f1ap_transmission_bandwidth_t *a, const struct f1ap_transmission_bandwidth_t *b);

struct OCTET_STRING;
uint8_t *cp_octet_string(const struct OCTET_STRING *os, int *len);

F1AP_SNSSAI_t encode_nssai(const nssai_t *nssai);
nssai_t decode_nssai(const F1AP_SNSSAI_t *nssai);

F1AP_Cause_t encode_f1ap_cause(f1ap_Cause_t cause, long cause_value);
bool decode_f1ap_cause(F1AP_Cause_t f1_cause, f1ap_Cause_t *cause, long *cause_value);

#endif /* F1AP_LIB_COMMON_H_ */
