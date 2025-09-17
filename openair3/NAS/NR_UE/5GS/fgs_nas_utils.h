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

#ifndef FGS_NAS_UTILS_H
#define FGS_NAS_UTILS_H

#include <stdint.h>
#include <arpa/inet.h>
#include <string.h> // For memcpy
#include <stdio.h>

#define PRINT_NAS_ERROR(...) fprintf(stderr, ##__VA_ARGS__)

#define _NAS_EQ_CHECK_GENERIC(condition, fmt, ...)                                                      \
  do {                                                                                                  \
    if (!(condition)) {                                                                                 \
      PRINT_NAS_ERROR("NAS Equality Check failure: %s:%d: Condition '%s' failed: " fmt " != " fmt "\n", \
                      __FILE__,                                                                         \
                      __LINE__,                                                                         \
                      #condition,                                                                       \
                      ##__VA_ARGS__);                                                                   \
      return false;                                                                                     \
    }                                                                                                   \
  } while (0)

#define _NAS_EQ_CHECK_LONG(A, B) _NAS_EQ_CHECK_GENERIC(A == B, "%ld", A, B);
#define _NAS_EQ_CHECK_INT(A, B) _NAS_EQ_CHECK_GENERIC(A == B, "%d", A, B);
#define _NAS_EQ_CHECK_STR(A, B) _NAS_EQ_CHECK_GENERIC(strcmp(A, B) == 0, "'%s'", A, B);

/* Map task id to printable name. */
typedef struct {
  int id;
  char text[256];
} text_info_t;

#define TO_TEXT(LabEl, nUmID) {nUmID, #LabEl},
#define TO_ENUM(LabEl, nUmID ) LabEl = nUmID,

#define GET_SHORT(input, size) ({           \
    uint16_t tmp16;                         \
    memcpy(&tmp16, (input), sizeof(tmp16)); \
    size += htons(tmp16);                   \
})

#endif // FGS_NAS_UTILS_H
