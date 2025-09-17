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

#ifndef OPENAIRINTERFACE_WLS_COMMON_H
#define OPENAIRINTERFACE_WLS_COMMON_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "wls_lib.h"
#include "nfapi/open-nFAPI/fapi/inc/nr_fapi_p5_utils.h"
#include "nfapi/open-nFAPI/fapi/inc/nr_fapi_p7_utils.h"

#define WLS_DEV_NAME "wls"
#define WLS_MAC_MEMORY_SIZE 0x7F000000
#define WLS_PHY_MEMORY_SIZE 0x7F000000

#define MAX_NUM_LOCATIONS           (508)

#define MIN_DL_BUF_LOCATIONS        (0)                                             /* Used for stats collection 0-49 */
#define MIN_UL_BUF_LOCATIONS        (MIN_DL_BUF_LOCATIONS + MAX_NUM_LOCATIONS)      /* Used for stats collection 50-99 */

#define MAX_DL_BUF_LOCATIONS        (MIN_DL_BUF_LOCATIONS + MAX_NUM_LOCATIONS)          /* Used for stats collection 0-49 */
#define MAX_UL_BUF_LOCATIONS        (MIN_UL_BUF_LOCATIONS + MAX_NUM_LOCATIONS)          /* Used for stats collection 50-99 */

#define ALLOC_TRACK_SIZE                    ( 16384 )

typedef struct {
  uint64_t *values;
  uint16_t values_size;
  uint16_t values_count;
}pa_store_t;

/* ----- WLS Operation --- */
#define FAPI_VENDOR_MSG_HEADER_IND                          0x1A
// Linked list header present at the top of all messages
typedef struct _fapi_api_queue_elem {
 struct _fapi_api_queue_elem *p_next;
 // p_tx_data_elm_list used for TX_DATA.request processing
 struct _fapi_api_queue_elem *p_tx_data_elm_list;
 uint8_t msg_type;
 uint8_t num_message_in_block;
 uint32_t msg_len;
 uint32_t align_offset;
 uint64_t time_stamp;
} fapi_api_queue_elem_t,
    *p_fapi_api_queue_elem_t;


typedef struct wls_mac_mem_array
{
 void **ppFreeBlock;
 void *pStorage;
 void *pEndOfStorage;
 uint32_t nBlockSize;
 uint32_t nBlockCount;
} WLS_MAC_MEM_SRUCT, *PWLS_MAC_MEM_SRUCT;

typedef struct wls_mac_ctx
{
 void *hWls;
 void *pWlsMemBase;
 void *pWlsMemBaseUsable;
 WLS_MAC_MEM_SRUCT sWlsStruct;

 uint64_t nTotalMemorySize;
 uint64_t nTotalMemorySizeUsable;
 uint32_t nBlockSize;
 uint32_t nTotalBlocks;
 uint32_t nAllocBlocks;
 uint32_t nTotalAllocCnt;
 uint32_t nTotalFreeCnt;
 uint32_t nTotalUlBufAllocCnt;
 uint32_t nTotalUlBufFreeCnt;
 uint32_t nTotalDlBufAllocCnt;
 uint32_t nTotalDlBufFreeCnt;
 //  Support for FAPI Translator
 uint32_t nPartitionMemSize;
 void     *pPartitionMemBase;

 volatile pthread_mutex_t lock;
 volatile pthread_mutex_t lock_alloc;
} WLS_MAC_CTX, *PWLS_MAC_CTX;

void fill_fapi_list_elem(p_fapi_api_queue_elem_t currElem, p_fapi_api_queue_elem_t nextElem, const uint8_t msgType, const uint8_t numMsgInBlock, const uint32_t alignOffset);
uint64_t dequeueBlock(PWLS_MAC_CTX pWls);
void wls_return_msg_to_master(PWLS_MAC_CTX pWls, const uint64_t msg_PA);
uint8_t wls_send_fapi_msg(PWLS_MAC_CTX pWls, const uint16_t message_id, const int packed_len, const uint8_t *message );
uint8_t wls_msg_send(PWLS_MAC_CTX pWls, p_fapi_api_queue_elem_t currMsg);
#endif //OPENAIRINTERFACE_WLS_COMMON_H
