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

#ifndef E1AP_LIB_COMMON_H_
#define E1AP_LIB_COMMON_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "openair3/UTILS/conversions.h"
#include "e1ap_messages_types.h"

#ifdef E1AP_LOG_ERRORS
#define PRINT_ERROR(fmt, ...) fprintf(stderr, "[%s] " fmt, __func__, ##__VA_ARGS__)
#else
#define PRINT_ERROR(...) // Do nothing
#endif

#define CHECK_E1AP_DEC(exp)                                                             \
  do {                                                                                  \
    if (!(exp)) {                                                                       \
        PRINT_ERROR("Failed executing " #exp " in %s() line %d\n", __func__, __LINE__); \
        return false;                                                                   \
    }                                                                                   \
  } while (0)

#define _E1_EQ_CHECK_GENERIC(condition, fmt, ...)                                                  \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      PRINT_ERROR("E1 Equality Check failure: %s:%d: Condition '%s' failed: " fmt " != " fmt "\n", \
                  __FILE__,                                                                        \
                  __LINE__,                                                                        \
                  #condition,                                                                      \
                  ##__VA_ARGS__);                                                                  \
      return false;                                                                                \
    }                                                                                              \
  } while (0)

#define _E1_EQ_CHECK_LONG(A, B) _E1_EQ_CHECK_GENERIC(A == B, "%ld", A, B);
#define _E1_EQ_CHECK_INT(A, B) _E1_EQ_CHECK_GENERIC(A == B, "%d", A, B);
#define _E1_EQ_CHECK_STR(A, B) _E1_EQ_CHECK_GENERIC(strcmp(A, B) == 0, "'%s'", A, B);

// Macro to look up IE. If mandatory and not found, macro will log an error and return false.
#define E1AP_LIB_FIND_IE(IE_TYPE, ie, container, IE_ID, mandatory)                                           \
  do {                                                                                                       \
    ie = NULL;                                                                                               \
    for (int i = 0; i < (container)->protocolIEs.list.count; ++i) {                                          \
      IE_TYPE *current_ie = (container)->protocolIEs.list.array[i];                                          \
      if (current_ie->id == (IE_ID)) {                                                                       \
        ie = current_ie;                                                                                     \
        break;                                                                                               \
      }                                                                                                      \
    }                                                                                                        \
    if (mandatory && ie == NULL) {                                                                           \
      fprintf(stderr, "%s(): Mandatory element not found: ID" #IE_ID " with type " #IE_TYPE "\n", __func__); \
      return false;                                                                                          \
    }                                                                                                        \
  } while (0)

/* deep copy of optional E1AP IE */
#define _E1_CP_OPTIONAL_IE(dest, src, field)                  \
  do {                                                        \
    if ((src)->field) {                                       \
      (dest)->field = malloc_or_fail(sizeof(*(dest)->field)); \
      *(dest)->field = *(src)->field;                         \
    }                                                         \
  } while (0)

#define _E1_EQ_CHECK_OPTIONAL_IE(a, b, field, EQ_MACRO)               \
  do {                                                                \
    if (((a)->field && !(b)->field) || (!(a)->field && (b)->field)) { \
      return false; /* One of the two is not allocated */             \
    }                                                                 \
    if ((a)->field && (b)->field) {                                   \
      EQ_MACRO(*(a)->field, *(b)->field)                              \
    }                                                                 \
  } while (0)

#define _E1_EQ_CHECK_OPTIONAL_PTR(a, b, field)                           \
  do {                                                                   \
    if (((a)->field && !(b)->field) || (!(a)->field && (b)->field)) {    \
      PRINT_ERROR("%s:%d: optional IE '%s' not allocated: %p, %p\n",     \
                  __FILE__,                                              \
                  __LINE__,                                              \
                  #field,                                                \
                  (void *)(a)->field,                                    \
                  (void *)(b)->field);                                   \
      return false; /* One of the two is not allocated */                \
    }                                                                    \
  } while (0)

struct E1AP_Cause;
struct E1AP_Cause e1_encode_cause_ie(const e1ap_cause_t *cause);
e1ap_cause_t e1_decode_cause_ie(const struct E1AP_Cause *ie);

#endif /* E1AP_LIB_COMMON_H_ */
