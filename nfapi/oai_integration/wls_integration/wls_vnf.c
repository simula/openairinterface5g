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
#include "wls_vnf.h"

#include "vnf_p7.h"

#include <rte_eal.h>
#include <rte_string_fns.h>
#include <rte_common.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <nr_fapi_p5.h>
#include <common/platform_constants.h>

static nfapi_vnf_config_t *config;
static WLS_MAC_CTX wls_mac_iface;
vnf_t *_vnf = NULL;
// A copy of the VNF P7 config is needed for the message processing thread, in other transport mechanisms where P7 is handled in a
// different thread, the config is passed to it upon the thread creation
nfapi_vnf_p7_config_t *wls_vnf_p7_config = NULL;

void wls_vnf_set_p7_config(void *p7_config)
{
  wls_vnf_p7_config = (nfapi_vnf_p7_config_t *)p7_config;
}

static PWLS_MAC_CTX wls_mac_get_ctx(void)
{
  return &wls_mac_iface;
}

static uint8_t alloc_track[ALLOC_TRACK_SIZE];
//-------------------------------------------------------------------------------------------
/** @ingroup group_testmac
 *
 *  @param[in]   pMemArray Pointer to WLS Memory management structure
 *  @param[in]   pMemArrayMemory Pointer to flat buffer that was allocated
 *  @param[in]   totalSize Total Size of flat buffer allocated
 *  @param[in]   nBlockSize Size of each block that needs to be partitioned by memory manager
 *
 *  @return  0 if SUCCESS
 *
 *  @description
 *  This function creates memory blocks from a flat buffer which will be used for communciation
 *  between MAC and PHY
 *
 **/
//-------------------------------------------------------------------------------------------
static bool wls_mac_create_mem_array(PWLS_MAC_MEM_SRUCT pMemArray, void *pMemArrayMemory, uint32_t totalSize, uint32_t nBlockSize)
{
  const uint32_t numBlocks = totalSize / nBlockSize;

  printf("wls_mac_create_mem_array: pMemArray[%p] pMemArrayMemory[%p] totalSize[%d] nBlockSize[%d] numBlocks[%d]\n",
         pMemArray,
         pMemArrayMemory,
         totalSize,
         nBlockSize,
         numBlocks);
  AssertFatal(nBlockSize >= sizeof(void *), "WLS nBlockSize too small");
  AssertFatal(totalSize > sizeof(void *), "WLS Can't allocate less than 1 block");
  pMemArray->ppFreeBlock = (void **)pMemArrayMemory;
  pMemArray->pStorage = pMemArrayMemory;
  pMemArray->pEndOfStorage = ((unsigned long *)pMemArrayMemory) + numBlocks * nBlockSize / sizeof(unsigned long);
  pMemArray->nBlockSize = nBlockSize;
  pMemArray->nBlockCount = numBlocks;

  // Initialize single-linked list of free blocks;
  void **ptr = (void **)pMemArrayMemory;
  for (uint32_t i = 0; i < pMemArray->nBlockCount; i++) {
    if (i == pMemArray->nBlockCount - 1) {
      *ptr = NULL; // End of list
    } else {
      // Points to the next block
      *ptr = (void **)(((uint8_t *)ptr) + nBlockSize);
      ptr += nBlockSize / sizeof(unsigned long);
    }
  }

  memset(alloc_track, 0, sizeof(uint8_t) * ALLOC_TRACK_SIZE);

  return true;
}

void mac_dpdk_init()
{
  char *my_argv[] = {"OAI_VNF", "-c3", "--proc-type=auto", "--file-prefix", WLS_DEV_NAME, "--iova-mode=pa"};
  printf("\nCalling rte_eal_init: ");
  for (unsigned long i = 0; i < RTE_DIM(my_argv); i++) {
    printf("%s ", my_argv[i]);
  }
  printf("\n");

  if (rte_eal_init(RTE_DIM(my_argv), my_argv) < 0)
    rte_panic("\nCannot init EAL\n");
}

static uint32_t wls_mac_alloc_mem_array(PWLS_MAC_MEM_SRUCT pMemArray, void **ppBlock)
{
  int idx;

  if (pMemArray->ppFreeBlock == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "wls_mac_alloc_mem_array pMemArray->ppFreeBlock = NULL\n");
    return 1;
  }

  // FIXME: Remove after debugging
  if (((void *)pMemArray->ppFreeBlock < pMemArray->pStorage) || ((void *)pMemArray->ppFreeBlock >= pMemArray->pEndOfStorage)) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR,
                "wls_mac_alloc_mem_array ERROR: Corrupted MemArray;Arr=%p,Stor=%p,Free=%p\n",
                pMemArray,
                pMemArray->pStorage,
                pMemArray->ppFreeBlock);
    return 1;
  }

  pMemArray->ppFreeBlock = (void **)((unsigned long)pMemArray->ppFreeBlock & 0xFFFFFFFFFFFFFFF0);
  *pMemArray->ppFreeBlock = (void **)((unsigned long)*pMemArray->ppFreeBlock & 0xFFFFFFFFFFFFFFF0);

  if ((*pMemArray->ppFreeBlock != NULL)
      && (((*pMemArray->ppFreeBlock) < pMemArray->pStorage) || ((*pMemArray->ppFreeBlock) >= pMemArray->pEndOfStorage))) {
    fprintf(stderr,
            "ERROR: Corrupted MemArray;Arr=%p,Stor=%p,Free=%p,Curr=%p\n",
            pMemArray,
            pMemArray->pStorage,
            pMemArray->ppFreeBlock,
            *pMemArray->ppFreeBlock);
    return 1;
  }

  *ppBlock = (void *)pMemArray->ppFreeBlock;
  pMemArray->ppFreeBlock = (void **)(*pMemArray->ppFreeBlock);

  idx = (((uint64_t)*ppBlock - (uint64_t)pMemArray->pStorage)) / pMemArray->nBlockSize;
  if (alloc_track[idx]) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR,
                "wls_mac_alloc_mem_array Double alloc Arr=%p,Stor=%p,Free=%p,Curr=%p\n",
                pMemArray,
                pMemArray->pStorage,
                pMemArray->ppFreeBlock,
                *pMemArray->ppFreeBlock);
  } else {
    alloc_track[idx] = 1;
  }

  return 0;
}

void wls_mac_print_stats(void)
{
  PWLS_MAC_CTX pWls = wls_mac_get_ctx();

  NFAPI_TRACE(NFAPI_TRACE_DEBUG, "wls_mac_free_list_all:\n");
  NFAPI_TRACE(NFAPI_TRACE_DEBUG,
              "        nTotalBlocks[%d] nAllocBlocks[%d] nFreeBlocks[%d]\n",
              pWls->nTotalBlocks,
              pWls->nAllocBlocks,
              (pWls->nTotalBlocks - pWls->nAllocBlocks));
  NFAPI_TRACE(NFAPI_TRACE_DEBUG,
              "        nTotalAllocCnt[%d] nTotalFreeCnt[%d] Diff[%d]\n",
              pWls->nTotalAllocCnt,
              pWls->nTotalFreeCnt,
              (pWls->nTotalAllocCnt - pWls->nTotalFreeCnt));
  NFAPI_TRACE(NFAPI_TRACE_DEBUG,
              "        nDlBufAllocCnt[%d] nDlBufFreeCnt[%d] Diff[%d]\n",
              pWls->nTotalDlBufAllocCnt,
              pWls->nTotalDlBufFreeCnt,
              (pWls->nTotalDlBufAllocCnt - pWls->nTotalDlBufFreeCnt));
  NFAPI_TRACE(NFAPI_TRACE_DEBUG,
              "        nUlBufAllocCnt[%d] nUlBufFreeCnt[%d] Diff[%d]\n\n",
              pWls->nTotalUlBufAllocCnt,
              pWls->nTotalUlBufFreeCnt,
              (pWls->nTotalUlBufAllocCnt - pWls->nTotalUlBufFreeCnt));
}

static void *wls_mac_alloc_buffer(PWLS_MAC_CTX pWls, uint32_t size, uint32_t loc)
{
  void *pBlock = NULL;
  pthread_mutex_lock((pthread_mutex_t *)&pWls->lock_alloc);
  if (wls_mac_alloc_mem_array(&pWls->sWlsStruct, &pBlock) != 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "wls_mac_alloc_buffer alloc error size[%d] loc[%d]\n", size, loc);
    wls_mac_print_stats();
    pthread_mutex_unlock((pthread_mutex_t *)&pWls->lock_alloc);
    return NULL;
  }
  pWls->nAllocBlocks++;
  pWls->nTotalAllocCnt++;
  if (loc < MAX_DL_BUF_LOCATIONS)
    pWls->nTotalDlBufAllocCnt++;
  else if (loc < MAX_UL_BUF_LOCATIONS)
    pWls->nTotalUlBufAllocCnt++;

  pthread_mutex_unlock((pthread_mutex_t *)&pWls->lock_alloc);

  return pBlock;
}

static bool mac_wls_init()
{
  uint64_t nWlsMacMemorySize = 0;
  uint64_t nWlsPhyMemorySize = 0;
  PWLS_MAC_CTX pWls = wls_mac_get_ctx();

  pthread_mutex_init((pthread_mutex_t *)&pWls->lock, NULL);
  pthread_mutex_init((pthread_mutex_t *)&pWls->lock_alloc, NULL);

  pWls->nTotalAllocCnt = 0;
  pWls->nTotalFreeCnt = 0;
  pWls->nTotalUlBufAllocCnt = 0;
  pWls->nTotalUlBufFreeCnt = 0;
  pWls->nTotalDlBufAllocCnt = 0;
  pWls->nTotalDlBufFreeCnt = 0;

  /* Start by opening the WLS instance with WLS_MASTER_CLIENT */
  if ((pWls->hWls = WLS_Open(WLS_DEV_NAME, WLS_MASTER_CLIENT, &nWlsMacMemorySize, &nWlsPhyMemorySize)) == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "\nCould not open WLS Interface \n");
    return false;
  }

  /* WLS_Open was successful, we can allocate memory */
  if ((pWls->pWlsMemBase = WLS_Alloc(pWls->hWls, nWlsMacMemorySize + nWlsPhyMemorySize)) == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "\nCould not allocate WLS Memory \n");
    return false;
  }
  /* Set the total memory that MAC has access to */
  pWls->nTotalMemorySize = nWlsMacMemorySize;

  /* Partition the WLS memory */
  memset(pWls->pWlsMemBase, 0xCC, pWls->nTotalMemorySize);
  pWls->pPartitionMemBase = pWls->pWlsMemBase;
  pWls->nPartitionMemSize = pWls->nTotalMemorySize;
  pWls->nTotalBlocks = pWls->nTotalMemorySize / (NFAPI_MAX_PACKED_MESSAGE_SIZE);
  /* Create the memory array */
  if (!wls_mac_create_mem_array(&pWls->sWlsStruct, pWls->pPartitionMemBase, pWls->nPartitionMemSize, NFAPI_MAX_PACKED_MESSAGE_SIZE)) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "\nCould not create WLS Memory Array \n");
    return false;
  }
    NFAPI_TRACE(NFAPI_TRACE_INFO, "\nTotalBlocks is %d \n", pWls->nTotalBlocks);
int nBlocks = 0;
for(int i = 0 ; i< pWls->nTotalBlocks; i++){
  void* pMsg =  wls_mac_alloc_buffer(pWls, 0, i);
   /* Enqueue all blocks */
  nBlocks += WLS_EnqueueBlock(pWls->hWls, WLS_VA2PA(pWls->hWls, pMsg));
}  
  NFAPI_TRACE(NFAPI_TRACE_INFO, "\nAllocated %d Blocks \n", nBlocks);
  return true;
}

static int vnf_wls_init()
{
  mac_dpdk_init();

  if (!mac_wls_init()) {
    return 0;
  }

  return 1;
}

void *wls_fapi_vnf_nr_start_thread(void *ptr)
{
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] IN WLS PNF NFAPI start thread %s\n", __FUNCTION__);
  wls_fapi_nr_vnf_start((nfapi_vnf_config_t *)ptr);
  return (void *)0;
}

static void procPhyMessages(uint32_t msg_size, void *msg_buf, uint16_t msg_id)
{
  NFAPI_TRACE(NFAPI_TRACE_DEBUG, "[VNF] Received Msg ID 0x%02x\n", msg_id);
  if (msg_buf == NULL || _vnf == NULL) {
    AssertFatal(msg_buf != NULL && _vnf != NULL, "%s: NULL parameters\n", __FUNCTION__);
  }
  // Add the size of the header to the message_size, because the size in the header does not include the header length
  msg_size += NFAPI_NR_P5_HEADER_LENGTH;
  switch (msg_id) {
    case NFAPI_NR_PHY_MSG_TYPE_PARAM_RESPONSE:
    case NFAPI_NR_PHY_MSG_TYPE_CONFIG_RESPONSE:
    case NFAPI_NR_PHY_MSG_TYPE_START_RESPONSE:
    case NFAPI_NR_PHY_MSG_TYPE_STOP_INDICATION:
      vnf_nr_handle_p4_p5_message(msg_buf, msg_size, 0, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_DL_TTI_REQUEST ... NFAPI_NR_PHY_MSG_TYPE_RACH_INDICATION: {
      vnf_nr_handle_p7_message(msg_buf, msg_size + NFAPI_NR_P7_HEADER_LENGTH, (vnf_p7_t *)wls_vnf_p7_config);
      break;
    }
    default:
      break;
  }
}

int wls_fapi_nr_vnf_start(nfapi_vnf_config_t *cfg)
{
  config = cfg;

  if (config == 0) {
    return -1;
  }
  _vnf = (vnf_t *)(config);

  // init WLS connection
  if (vnf_wls_init() == 0) {
    return -1;
  }
  _vnf->terminate = 0;

  // Connect to the PNF
  if (config->pnf_nr_start_resp != 0) {
    (config->pnf_nr_start_resp)(config, 0, NULL);
  }
  /* VNF receive loop */
  /* Number of Memory blocks to get */
  uint32_t msgSize;
  uint16_t msgType;
  uint16_t flags;
  PWLS_MAC_CTX pWls = wls_mac_get_ctx();
  while (_vnf->terminate == 0) {
    int numMsgToGet = WLS_Wait(pWls->hWls);
    if (numMsgToGet == 0) {
      continue;
    }

    uint64_t PAs[numMsgToGet];
    uint8_t pa_idx = 0;
    while (numMsgToGet--) {
      PAs[pa_idx] = WLS_Get(pWls->hWls, &msgSize, &msgType, &flags);
      pa_idx++;
    }
    for (int i = 0; i < pa_idx; i++) {
      p_fapi_api_queue_elem_t msg = WLS_PA2VA(pWls->hWls, PAs[i]);
      if (msg->msg_type != FAPI_VENDOR_MSG_HEADER_IND) {
        uint8_t *msgt = (uint8_t *)(msg + 1);
        uint32_t len = msgt[7] | (msgt[6] << 8) | (msgt[5] << 16) | (msgt[4] << 24);
        procPhyMessages(len, (void *)(msg + 1), msg->msg_type);
      }
    }
    for (int i = 0; i < pa_idx; i++) {
      if (WLS_EnqueueBlock(pWls->hWls, PAs[i]) == -1) {
        NFAPI_TRACE(NFAPI_TRACE_DEBUG, "Error in WLS_EnqueueBlock for block %lu\n", PAs[i]);
      }
    }
  }
  return 1;
}

bool wls_vnf_nr_send_p5_message(vnf_t *vnf, uint16_t p5_idx, nfapi_nr_p4_p5_message_header_t *msg, uint32_t msg_len)
{
  UNUSED_VARIABLE(p5_idx);
  int packed_len =
      vnf->_public.pack_func(msg, msg_len, vnf->tx_message_buffer, sizeof(vnf->tx_message_buffer), &vnf->_public.codec_config);

  if (packed_len < 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "nfapi_p5_message_pack failed (%d)\n", packed_len);
    return false;
  }

  // printf("fapi_nr_p5_message_pack succeeded having packed %d bytes\n", packed_len);
  // printf("in msg, msg_len is %d\n", msg->message_length);
  PWLS_MAC_CTX pWls = wls_mac_get_ctx();

  int retval = wls_send_fapi_msg(pWls, msg->message_id, packed_len, vnf->tx_message_buffer);
  if (retval) {
    NFAPI_TRACE(NFAPI_TRACE_DEBUG, "wls_send_fapi_msg success\n");
  }
  return retval;
}

bool wls_vnf_nr_send_p7_message(vnf_p7_t *vnf_p7, nfapi_nr_p7_message_header_t *msg)
{
  uint32_t message_id = msg->message_id;
  PWLS_MAC_CTX pWls = wls_mac_get_ctx();
  int num_avail_blocks = WLS_NumBlocks(pWls->hWls);
  NFAPI_TRACE(NFAPI_TRACE_DEBUG,"num_avail_blocks is %d\n", num_avail_blocks);
  while (num_avail_blocks < 2) {
    num_avail_blocks = WLS_NumBlocks(pWls->hWls);
    NFAPI_TRACE(NFAPI_TRACE_DEBUG,"num_avail_blocks is %d\n", num_avail_blocks);
  }
  /* get PA blocks for header and msg */
  uint64_t pa_hdr = dequeueBlock(pWls);
  AssertFatal(pa_hdr, "WLS_DequeueBlock failed for pa_hdr\n");
  uint64_t pa_msg = dequeueBlock(pWls);
  AssertFatal(pa_msg, "WLS_DequeueBlock failed for pa_msg\n");
  p_fapi_api_queue_elem_t headerElem = WLS_PA2VA(pWls->hWls, pa_hdr);
  AssertFatal(headerElem, "WLS_PA2VA failed for headerElem\n");
  p_fapi_api_queue_elem_t fapiMsgElem = WLS_PA2VA(pWls->hWls, pa_msg);
  AssertFatal(fapiMsgElem, "WLS_PA2VA failed for fapiMsgElem\n");
  //memcpy((uint8_t *)(fapiMsgElem + 1), message, packed_len + NFAPI_HEADER_LENGTH);
  const uint8_t wls_header[] = {1, 0}; // num_messages ,  opaque_handle
 
  fill_fapi_list_elem(headerElem, fapiMsgElem, FAPI_VENDOR_MSG_HEADER_IND, 1, 2);
  memcpy((uint8_t *)(headerElem + 1), wls_header, 2);
  int packed_len = ((nfapi_vnf_p7_config_t *)vnf_p7)->pack_func(msg, (fapiMsgElem + 1), NFAPI_MAX_PACKED_MESSAGE_SIZE, NULL);
  if (packed_len < 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "fapi_p7_message_pack failed (%d)\n", packed_len);
    return false;
  }
  fill_fapi_list_elem(fapiMsgElem, NULL, message_id, 1, packed_len + NFAPI_HEADER_LENGTH);
  uint8_t retval = wls_msg_send(pWls->hWls, headerElem);
  return retval;
  //return wls_send_fapi_msg(pWls, msg->message_id, packed_len, _vnf->tx_message_buffer);
}
