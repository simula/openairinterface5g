/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

/*!\file nrLDPC_decoder_offload.c
 * \note: based on testbbdev test_bbdev_perf.c functions. Harq buffer offset added.
 * \mbuf and mempool allocated at the init step, LDPC parameters updated from OAI.
 */

#include <stdint.h>
#include "PHY/sse_intrin.h"
#include "nrLDPCdecoder_defs.h"
#include "nrLDPC_types.h"
#include "nrLDPC_init.h"
#include "nrLDPC_mPass.h"
#include "nrLDPC_cnProc.h"
#include "nrLDPC_bnProc.h"
#include <common/utils/LOG/log.h>
#define NR_LDPC_ENABLE_PARITY_CHECK

#ifdef NR_LDPC_DEBUG_MODE
#include "nrLDPC_tools/nrLDPC_debug.h"
#endif

#include "openair1/PHY/CODING/nrLDPC_extern.h"

#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <rte_eal.h>
#include <rte_common.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_pdump.h>
#include "nrLDPC_offload.h"

#include <math.h>

#include <rte_dev.h>
#include <rte_launch.h>
#include <rte_bbdev.h>
#include <rte_malloc.h>
#include <rte_random.h>
#include <rte_hexdump.h>
#include <rte_interrupts.h>

// this socket is the NUMA socket, so the hardware CPU id (numa is complex)
#define GET_SOCKET(socket_id) (((socket_id) == SOCKET_ID_ANY) ? 0 : (socket_id))
#define MAX_QUEUES 32
#define OPS_CACHE_SIZE 256U
#define OPS_POOL_SIZE_MIN 511U /* 0.5K per queue */
#define SYNC_WAIT 0
#define SYNC_START 1
#define TIME_OUT_POLL 1e8
/* Increment for next code block in external HARQ memory */
#define HARQ_INCR 32768
/* Headroom for filler LLRs insertion in HARQ buffer */
#define FILLER_HEADROOM 1024

pthread_mutex_t encode_mutex;
pthread_mutex_t decode_mutex;

const char *typeStr[] = {
    "RTE_BBDEV_OP_NONE", /**< Dummy operation that does nothing */
    "RTE_BBDEV_OP_TURBO_DEC", /**< Turbo decode */
    "RTE_BBDEV_OP_TURBO_ENC", /**< Turbo encode */
    "RTE_BBDEV_OP_LDPC_DEC", /**< LDPC decode */
    "RTE_BBDEV_OP_LDPC_ENC", /**< LDPC encode */
    "RTE_BBDEV_OP_TYPE_COUNT", /**< Count of different op types */
};

/* Represents tested active devices */
struct active_device {
  const char *driver_name;
  uint8_t dev_id;
  int dec_queue;
  int enc_queue;
  uint16_t queue_ids[MAX_QUEUES];
  uint16_t nb_queues;
  struct rte_mempool *bbdev_dec_op_pool;
  struct rte_mempool *bbdev_enc_op_pool;
  struct rte_mempool *in_mbuf_pool;
  struct rte_mempool *hard_out_mbuf_pool;
  struct rte_mempool *soft_out_mbuf_pool;
  struct rte_mempool *harq_in_mbuf_pool;
  struct rte_mempool *harq_out_mbuf_pool;
} active_devs[RTE_BBDEV_MAX_DEVS];
static int nb_active_devs;

/* Data buffers used by BBDEV ops */
struct data_buffers {
  struct rte_bbdev_op_data *inputs;
  struct rte_bbdev_op_data *hard_outputs;
  struct rte_bbdev_op_data *soft_outputs;
  struct rte_bbdev_op_data *harq_inputs;
  struct rte_bbdev_op_data *harq_outputs;
};

/* Operation parameters specific for given test case */
struct test_op_params {
  struct rte_mempool *mp_dec;
  struct rte_mempool *mp_enc;
  struct rte_bbdev_dec_op *ref_dec_op;
  struct rte_bbdev_enc_op *ref_enc_op;
  uint16_t burst_sz;
  uint16_t num_to_process;
  uint16_t num_lcores;
  int vector_mask;
  rte_atomic16_t sync;
  struct data_buffers q_bufs[RTE_MAX_NUMA_NODES][MAX_QUEUES];
};

/* Contains per lcore params */
struct thread_params {
  uint8_t dev_id;
  uint16_t queue_id;
  uint32_t lcore_id;
  struct nrLDPCoffload_params *p_offloadParams;
  uint8_t iter_count;
  uint8_t *p_out;
  uint8_t ulsch_id;
  rte_atomic16_t nb_dequeued;
  rte_atomic16_t processing_status;
  rte_atomic16_t burst_sz;
  struct test_op_params *op_params;
  struct rte_bbdev_dec_op *dec_ops[MAX_BURST];
  struct rte_bbdev_enc_op *enc_ops[MAX_BURST];
};

// DPDK BBDEV copy
static inline void
mbuf_reset(struct rte_mbuf *m)
{
  m->pkt_len = 0;

  do {
    m->data_len = 0;
    m = m->next;
  } while (m != NULL);
}

/* Read flag value 0/1 from bitmap */
// DPDK BBDEV copy
static inline bool
check_bit(uint32_t bitmap, uint32_t bitmask)
{
  return bitmap & bitmask;
}

/* calculates optimal mempool size not smaller than the val */
// DPDK BBDEV copy
static unsigned int
optimal_mempool_size(unsigned int val)
{
  return rte_align32pow2(val + 1) - 1;
}

// based on DPDK BBDEV create_mempools
static int create_mempools(struct active_device *ad, int socket_id, uint16_t num_ops, int out_buff_sz, int in_max_sz)
{
  unsigned int ops_pool_size, mbuf_pool_size, data_room_size = 0;
  num_ops = 1;
  uint8_t nb_segments = 1;
  ops_pool_size = optimal_mempool_size(RTE_MAX(
      /* Ops used plus 1 reference op */
      RTE_MAX((unsigned int)(ad->nb_queues * num_ops + 1),
              /* Minimal cache size plus 1 reference op */
              (unsigned int)(1.5 * rte_lcore_count() * OPS_CACHE_SIZE + 1)),
      OPS_POOL_SIZE_MIN));

  /* Decoder ops mempool */
  ad->bbdev_dec_op_pool = rte_bbdev_op_pool_create("bbdev_op_pool_dec", RTE_BBDEV_OP_LDPC_DEC,
  /* Encoder ops mempool */                         ops_pool_size, OPS_CACHE_SIZE, socket_id);
  ad->bbdev_enc_op_pool = rte_bbdev_op_pool_create("bbdev_op_pool_enc", RTE_BBDEV_OP_LDPC_ENC,
                                                    ops_pool_size, OPS_CACHE_SIZE, socket_id);

  if ((ad->bbdev_dec_op_pool == NULL) || (ad->bbdev_enc_op_pool == NULL))
    AssertFatal(1 == 0, "ERROR Failed to create %u items ops pool for dev %u on socket %d.", ops_pool_size, ad->dev_id, socket_id);

  /* Inputs */
  mbuf_pool_size = optimal_mempool_size(ops_pool_size * nb_segments);
  data_room_size = RTE_MAX(in_max_sz + RTE_PKTMBUF_HEADROOM + FILLER_HEADROOM, (unsigned int)RTE_MBUF_DEFAULT_BUF_SIZE);
  ad->in_mbuf_pool = rte_pktmbuf_pool_create("in_mbuf_pool", mbuf_pool_size, 0, 0, data_room_size, socket_id);
  AssertFatal(ad->in_mbuf_pool != NULL,
              "ERROR Failed to create %u items input pktmbuf pool for dev %u on socket %d.",
              mbuf_pool_size,
              ad->dev_id,
              socket_id);

  /* Hard outputs */
  data_room_size = RTE_MAX(out_buff_sz + RTE_PKTMBUF_HEADROOM + FILLER_HEADROOM, (unsigned int)RTE_MBUF_DEFAULT_BUF_SIZE);
  ad->hard_out_mbuf_pool = rte_pktmbuf_pool_create("hard_out_mbuf_pool", mbuf_pool_size, 0, 0, data_room_size, socket_id);
  AssertFatal(ad->hard_out_mbuf_pool != NULL,
              "ERROR Failed to create %u items hard output pktmbuf pool for dev %u on socket %d.",
              mbuf_pool_size,
              ad->dev_id,
              socket_id);
  return 0;
}

const char *ldpcenc_flag_bitmask[] = {
    /** Set for bit-level interleaver bypass on output stream. */
    "RTE_BBDEV_LDPC_INTERLEAVER_BYPASS",
    /** If rate matching is to be performed */
    "RTE_BBDEV_LDPC_RATE_MATCH",
    /** Set for transport block CRC-24A attach */
    "RTE_BBDEV_LDPC_CRC_24A_ATTACH",
    /** Set for code block CRC-24B attach */
    "RTE_BBDEV_LDPC_CRC_24B_ATTACH",
    /** Set for code block CRC-16 attach */
    "RTE_BBDEV_LDPC_CRC_16_ATTACH",
    /** Set if a device supports encoder dequeue interrupts. */
    "RTE_BBDEV_LDPC_ENC_INTERRUPTS",
    /** Set if a device supports scatter-gather functionality. */
    "RTE_BBDEV_LDPC_ENC_SCATTER_GATHER",
    /** Set if a device supports concatenation of non byte aligned output */
    "RTE_BBDEV_LDPC_ENC_CONCATENATION",
};

const char *ldpcdec_flag_bitmask[] = {
    /** Set for transport block CRC-24A checking */
    "RTE_BBDEV_LDPC_CRC_TYPE_24A_CHECK",
    /** Set for code block CRC-24B checking */
    "RTE_BBDEV_LDPC_CRC_TYPE_24B_CHECK",
    /** Set to drop the last CRC bits decoding output */
    "RTE_BBDEV_LDPC_CRC_TYPE_24B_DROP"
    /** Set for bit-level de-interleaver bypass on Rx stream. */
    "RTE_BBDEV_LDPC_DEINTERLEAVER_BYPASS",
    /** Set for HARQ combined input stream enable. */
    "RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE",
    /** Set for HARQ combined output stream enable. */
    "RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE",
    /** Set for LDPC decoder bypass.
     *  RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE must be set.
     */
    "RTE_BBDEV_LDPC_DECODE_BYPASS",
    /** Set for soft-output stream enable */
    "RTE_BBDEV_LDPC_SOFT_OUT_ENABLE",
    /** Set for Rate-Matching bypass on soft-out stream. */
    "RTE_BBDEV_LDPC_SOFT_OUT_RM_BYPASS",
    /** Set for bit-level de-interleaver bypass on soft-output stream. */
    "RTE_BBDEV_LDPC_SOFT_OUT_DEINTERLEAVER_BYPASS",
    /** Set for iteration stopping on successful decode condition
     *  i.e. a successful syndrome check.
     */
    "RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE",
    /** Set if a device supports decoder dequeue interrupts. */
    "RTE_BBDEV_LDPC_DEC_INTERRUPTS",
    /** Set if a device supports scatter-gather functionality. */
    "RTE_BBDEV_LDPC_DEC_SCATTER_GATHER",
    /** Set if a device supports input/output HARQ compression. */
    "RTE_BBDEV_LDPC_HARQ_6BIT_COMPRESSION",
    /** Set if a device supports input LLR compression. */
    "RTE_BBDEV_LDPC_LLR_COMPRESSION",
    /** Set if a device supports HARQ input from
     *  device's internal memory.
     */
    "RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_IN_ENABLE",
    /** Set if a device supports HARQ output to
     *  device's internal memory.
     */
    "RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_OUT_ENABLE",
    /** Set if a device supports loop-back access to
     *  HARQ internal memory. Intended for troubleshooting.
     */
    "RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK",
    /** Set if a device includes LLR filler bits in the circular buffer
     *  for HARQ memory. If not set, it is assumed the filler bits are not
     *  in HARQ memory and handled directly by the LDPC decoder.
     */
    "RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_FILLERS",
};

// based on DPDK BBDEV add_bbdev_dev
static int add_dev(uint8_t dev_id, struct rte_bbdev_info *info)
{
  int ret;
  struct active_device *ad = &active_devs[nb_active_devs];
  unsigned int nb_queues;
  nb_queues = RTE_MIN(rte_lcore_count(), info->drv.max_num_queues);
  nb_queues = RTE_MIN(nb_queues, (unsigned int)MAX_QUEUES);

  /* Display for debug the capabilities of the card */
  for (int i = 0; info->drv.capabilities[i].type != RTE_BBDEV_OP_NONE; i++) {
    printf("device: %d, capability[%d]=%s\n", dev_id, i, typeStr[info->drv.capabilities[i].type]);
    if (info->drv.capabilities[i].type == RTE_BBDEV_OP_LDPC_ENC) {
      const struct rte_bbdev_op_cap_ldpc_enc cap = info->drv.capabilities[i].cap.ldpc_enc;
      printf("    buffers: src = %d, dst = %d\n   capabilites: ", cap.num_buffers_src, cap.num_buffers_dst);
      for (int j = 0; j < sizeof(cap.capability_flags) * 8; j++)
        if (cap.capability_flags & (1ULL << j))
          printf("%s ", ldpcenc_flag_bitmask[j]);
      printf("\n");
    }
    if (info->drv.capabilities[i].type == RTE_BBDEV_OP_LDPC_DEC) {
      const struct rte_bbdev_op_cap_ldpc_dec cap = info->drv.capabilities[i].cap.ldpc_dec;
      printf("    buffers: src = %d, hard out = %d, soft_out %d, llr size %d, llr decimals %d \n   capabilities: ",
             cap.num_buffers_src,
             cap.num_buffers_hard_out,
             cap.num_buffers_soft_out,
             cap.llr_size,
             cap.llr_decimals);
      for (int j = 0; j < sizeof(cap.capability_flags) * 8; j++)
        if (cap.capability_flags & (1ULL << j))
          printf("%s ", ldpcdec_flag_bitmask[j]);
      printf("\n");
    }
  }

  /* setup device */
  ret = rte_bbdev_setup_queues(dev_id, nb_queues, info->socket_id);
  if (ret < 0) {
    printf("rte_bbdev_setup_queues(%u, %u, %d) ret %i\n", dev_id, nb_queues, info->socket_id, ret);
    return -1;
  }

  /* setup device queues */
  struct rte_bbdev_queue_conf qconf = {
      .socket = info->socket_id,
      .queue_size = info->drv.default_queue_conf.queue_size,
  };

  // Search a queue linked to HW capability ldpc decoding
  qconf.op_type = RTE_BBDEV_OP_LDPC_ENC;
  int queue_id;
  for (queue_id = 0; queue_id < nb_queues; ++queue_id) {
    ret = rte_bbdev_queue_configure(dev_id, queue_id, &qconf);
    if (ret == 0) {
      printf("Found LDCP encoding queue (id=%d) at prio%u on dev%u\n", queue_id, qconf.priority, dev_id);
      qconf.priority++;
      ad->enc_queue = queue_id;
      ad->queue_ids[queue_id] = queue_id;
      break;
    }
  }
  AssertFatal(queue_id != nb_queues, "ERROR Failed to configure encoding queues on dev %u", dev_id);

  // Search a queue linked to HW capability ldpc encoding
  qconf.op_type = RTE_BBDEV_OP_LDPC_DEC;
  for (queue_id++; queue_id < nb_queues; ++queue_id) {
    ret = rte_bbdev_queue_configure(dev_id, queue_id, &qconf);
    if (ret == 0) {
      printf("Found LDCP decoding queue (id=%d) at prio%u on dev%u\n", queue_id, qconf.priority, dev_id);
      qconf.priority++;
      ad->dec_queue = queue_id;
      ad->queue_ids[queue_id] = queue_id;
      break;
    }
  }
  AssertFatal(queue_id != nb_queues, "ERROR Failed to configure encoding queues on dev %u", dev_id);
  ad->nb_queues = 2;
  return 0;
}

// based on DPDK BBDEV init_op_data_objs
static int init_op_data_objs_dec(struct rte_bbdev_op_data *bufs,
                                 uint8_t *input,
                                 t_nrLDPCoffload_params *offloadParams,
                                 struct rte_mempool *mbuf_pool,
                                 const uint16_t n,
                                 enum op_data_type op_type,
                                 uint16_t min_alignment)
{
  bool large_input = false;
  for (int i = 0; i < n; ++i) {
    uint32_t data_len = offloadParams->perCB[i].E_cb;
    char *data;
    struct rte_mbuf *m_head = rte_pktmbuf_alloc(mbuf_pool);
    AssertFatal(m_head != NULL,
                "Not enough mbufs in %d data type mbuf pool (needed %u, available %u)",
                op_type,
                n,
                mbuf_pool->size);

    if (data_len > RTE_BBDEV_LDPC_E_MAX_MBUF) {
      printf("Warning: Larger input size than DPDK mbuf %u\n", data_len);
      large_input = true;
    }
    bufs[i].data = m_head;
    bufs[i].offset = 0;
    bufs[i].length = 0;

    if ((op_type == DATA_INPUT) || (op_type == DATA_HARQ_INPUT)) {
      if ((op_type == DATA_INPUT) && large_input) {
        /* Allocate a fake overused mbuf */
        data = rte_malloc(NULL, data_len, 0);
        AssertFatal(data != NULL, "rte malloc failed with %u bytes", data_len);
        memcpy(data, &input[i * LDPC_MAX_CB_SIZE], data_len);
        m_head->buf_addr = data;
        m_head->buf_iova = rte_malloc_virt2iova(data);
        m_head->data_off = 0;
        m_head->data_len = data_len;
      } else {
        rte_pktmbuf_reset(m_head);
        data = rte_pktmbuf_append(m_head, data_len);
        AssertFatal(data != NULL, "Couldn't append %u bytes to mbuf from %d data type mbuf pool", data_len, op_type);
        AssertFatal(data == RTE_PTR_ALIGN(data, min_alignment),
                    "Data addr in mbuf (%p) is not aligned to device min alignment (%u)",
                    data,
                    min_alignment);
        rte_memcpy(data, &input[i * LDPC_MAX_CB_SIZE], data_len);
      }
      bufs[i].length += data_len;
    }
  }
  return 0;
}

// based on DPDK BBDEV init_op_data_objs
static int init_op_data_objs_enc(struct rte_bbdev_op_data *bufs,
                                 uint8_t **input_enc,
                                 t_nrLDPCoffload_params *offloadParams,
                                 struct rte_mbuf *m_head,
                                 struct rte_mempool *mbuf_pool,
                                 const uint16_t n,
                                 enum op_data_type op_type,
                                 uint16_t min_alignment)
{
  bool large_input = false;
  for (int i = 0; i < n; ++i) {
    uint32_t data_len = offloadParams->Kr;
    char *data;
    struct rte_mbuf *m_head = rte_pktmbuf_alloc(mbuf_pool);
    AssertFatal(m_head != NULL,
                "Not enough mbufs in %d data type mbuf pool (needed %u, available %u)",
                op_type,
                n,
                mbuf_pool->size);

    if (data_len > RTE_BBDEV_LDPC_E_MAX_MBUF) {
      printf("Warning: Larger input size than DPDK mbuf %u\n", data_len);
      large_input = true;
    }
    bufs[i].data = m_head;
    bufs[i].offset = 0;
    bufs[i].length = 0;

    if ((op_type == DATA_INPUT) || (op_type == DATA_HARQ_INPUT)) {
      if ((op_type == DATA_INPUT) && large_input) {
        /* Allocate a fake overused mbuf */
        data = rte_malloc(NULL, data_len, 0);
        AssertFatal(data != NULL, "rte malloc failed with %u bytes", data_len);
        memcpy(data, &input_enc[0], data_len);
        m_head->buf_addr = data;
        m_head->buf_iova = rte_malloc_virt2iova(data);
        m_head->data_off = 0;
        m_head->data_len = data_len;
      } else {
        rte_pktmbuf_reset(m_head);
        data = rte_pktmbuf_append(m_head, data_len);
        AssertFatal(data != NULL, "Couldn't append %u bytes to mbuf from %d data type mbuf pool", data_len, op_type);
        AssertFatal(data == RTE_PTR_ALIGN(data, min_alignment),
                    "Data addr in mbuf (%p) is not aligned to device min alignment (%u)",
                    data,
                    min_alignment);
        rte_memcpy(data, input_enc[i], data_len);
      }
      bufs[i].length += data_len;
    }
  }
  return 0;
}

// DPDK BBEV copy
static int allocate_buffers_on_socket(struct rte_bbdev_op_data **buffers, const int len, const int socket)
{
  int i;

  *buffers = rte_zmalloc_socket(NULL, len, 0, socket);
  if (*buffers == NULL) {
    printf("WARNING: Failed to allocate op_data on socket %d\n", socket);
    /* try to allocate memory on other detected sockets */
    for (i = 0; i < socket; i++) {
      *buffers = rte_zmalloc_socket(NULL, len, 0, i);
      if (*buffers != NULL)
        break;
    }
  }

  return (*buffers == NULL) ? -1 : 0;
}

// DPDK BBDEV copy
static void
free_buffers(struct active_device *ad, struct test_op_params *op_params)
{
  rte_mempool_free(ad->bbdev_dec_op_pool);
  rte_mempool_free(ad->bbdev_enc_op_pool);
  rte_mempool_free(ad->in_mbuf_pool);
  rte_mempool_free(ad->hard_out_mbuf_pool);
  rte_mempool_free(ad->soft_out_mbuf_pool);
  rte_mempool_free(ad->harq_in_mbuf_pool);
  rte_mempool_free(ad->harq_out_mbuf_pool);

  for (int i = 2; i < rte_lcore_count(); ++i) {
    for (int j = 0; j < RTE_MAX_NUMA_NODES; ++j) {
      rte_free(op_params->q_bufs[j][i].inputs);
      rte_free(op_params->q_bufs[j][i].hard_outputs);
      rte_free(op_params->q_bufs[j][i].soft_outputs);
      rte_free(op_params->q_bufs[j][i].harq_inputs);
      rte_free(op_params->q_bufs[j][i].harq_outputs);
    }
  }
}

// based on DPDK BBDEV copy_reference_ldpc_dec_op
static void set_ldpc_dec_op(struct rte_bbdev_dec_op **ops,
                            unsigned int start_idx,
                            struct data_buffers *bufs,
                            uint8_t ulsch_id,
                            t_nrLDPCoffload_params *p_offloadParams)
{
  unsigned int i;
  for (i = 0; i < p_offloadParams->C; ++i) {
    ops[i]->ldpc_dec.cb_params.e = p_offloadParams->perCB[i].E_cb;
    ops[i]->ldpc_dec.basegraph = p_offloadParams->BG;
    ops[i]->ldpc_dec.z_c = p_offloadParams->Z;
    ops[i]->ldpc_dec.q_m = p_offloadParams->Qm;
    ops[i]->ldpc_dec.n_filler = p_offloadParams->F;
    ops[i]->ldpc_dec.n_cb = p_offloadParams->n_cb;
    ops[i]->ldpc_dec.iter_max = p_offloadParams->numMaxIter;
    ops[i]->ldpc_dec.rv_index = p_offloadParams->rv;
    ops[i]->ldpc_dec.op_flags = RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE |
                                RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_IN_ENABLE |
                                RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_OUT_ENABLE |
                                RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE;
    if (p_offloadParams->setCombIn) {
      ops[i]->ldpc_dec.op_flags |= RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE;
    }
    if (p_offloadParams->C > 1) {
      ops[i]->ldpc_dec.op_flags |= RTE_BBDEV_LDPC_CRC_TYPE_24B_DROP;
      ops[i]->ldpc_dec.op_flags |= RTE_BBDEV_LDPC_CRC_TYPE_24B_CHECK;
    }
    ops[i]->ldpc_dec.code_block_mode = 1;
    ops[i]->ldpc_dec.harq_combined_input.offset = ulsch_id * NR_LDPC_MAX_NUM_CB * LDPC_MAX_CB_SIZE + i * LDPC_MAX_CB_SIZE;
    ops[i]->ldpc_dec.harq_combined_output.offset = ulsch_id * NR_LDPC_MAX_NUM_CB * LDPC_MAX_CB_SIZE + i * LDPC_MAX_CB_SIZE;
    if (bufs->hard_outputs != NULL)
      ops[i]->ldpc_dec.hard_output = bufs->hard_outputs[start_idx + i];
    if (bufs->inputs != NULL)
      ops[i]->ldpc_dec.input = bufs->inputs[start_idx + i];
    if (bufs->soft_outputs != NULL)
      ops[i]->ldpc_dec.soft_output = bufs->soft_outputs[start_idx + i];
    if (bufs->harq_inputs != NULL)
      ops[i]->ldpc_dec.harq_combined_input = bufs->harq_inputs[start_idx + i];
    if (bufs->harq_outputs != NULL)
      ops[i]->ldpc_dec.harq_combined_output = bufs->harq_outputs[start_idx + i];
  }
}

// based on DPDK BBDEV copy_reference_ldpc_enc_op
static void set_ldpc_enc_op(struct rte_bbdev_enc_op **ops,
                            unsigned int start_idx,
                            struct rte_bbdev_op_data *inputs,
                            struct rte_bbdev_op_data *outputs,
                            t_nrLDPCoffload_params *p_offloadParams)
{
  for (int i = 0; i < p_offloadParams->C; ++i) {
    ops[i]->ldpc_enc.cb_params.e = p_offloadParams->perCB[i].E_cb;
    ops[i]->ldpc_enc.basegraph = p_offloadParams->BG;
    ops[i]->ldpc_enc.z_c = p_offloadParams->Z;
    ops[i]->ldpc_enc.q_m = p_offloadParams->Qm;
    ops[i]->ldpc_enc.n_filler = p_offloadParams->F;
    ops[i]->ldpc_enc.n_cb = p_offloadParams->n_cb;
    ops[i]->ldpc_enc.rv_index = p_offloadParams->rv;
    ops[i]->ldpc_enc.op_flags = RTE_BBDEV_LDPC_RATE_MATCH;
    ops[i]->ldpc_enc.code_block_mode = 1;
    ops[i]->ldpc_enc.output = outputs[start_idx + i];
    ops[i]->ldpc_enc.input = inputs[start_idx + i];
  }
}

static int retrieve_ldpc_dec_op(struct rte_bbdev_dec_op **ops, const uint16_t n, const int vector_mask, uint8_t *p_out)
{
  struct rte_bbdev_op_data *hard_output;
  uint16_t data_len = 0;
  struct rte_mbuf *m;
  unsigned int i;
  char *data;
  int offset = 0;
  for (i = 0; i < n; ++i) {
    hard_output = &ops[i]->ldpc_dec.hard_output;
    m = hard_output->data;
    data_len = rte_pktmbuf_data_len(m) - hard_output->offset;
    data = m->buf_addr;
    memcpy(&p_out[offset], data + m->data_off, data_len);
    offset += data_len;
    rte_pktmbuf_free(ops[i]->ldpc_dec.hard_output.data);
    rte_pktmbuf_free(ops[i]->ldpc_dec.input.data);
  }
  return 0;
}

static int retrieve_ldpc_enc_op(struct rte_bbdev_enc_op **ops, const uint16_t n, uint8_t *p_out, nrLDPC_params_per_cb_t *perCB)
{
  struct rte_bbdev_op_data *output;
  struct rte_mbuf *m;
  unsigned int i;
  char *data;
  uint8_t *out;
  int offset = 0;
  for (i = 0; i < n; ++i) {
    output = &ops[i]->ldpc_enc.output;
    m = output->data;
    uint16_t data_len = rte_pktmbuf_data_len(m) - output->offset;
    out = &p_out[offset];
    data = m->buf_addr;
    for (int byte = 0; byte < data_len; byte++)
      for (int bit = 0; bit < 8; bit++)
        out[byte * 8 + bit] = (data[m->data_off + byte] >> (7 - bit)) & 1;
    offset += perCB[i].E_cb;
    rte_pktmbuf_free(ops[i]->ldpc_enc.output.data);
    rte_pktmbuf_free(ops[i]->ldpc_enc.input.data);
  }
  return 0;
}

// based on DPDK BBDEV init_test_op_params
static int init_test_op_params(struct test_op_params *op_params,
                               enum rte_bbdev_op_type op_type,
                               struct rte_mempool *ops_mp,
                               uint16_t burst_sz,
                               uint16_t num_to_process,
                               uint16_t num_lcores)
{
  int ret = 0;
  if (op_type == RTE_BBDEV_OP_LDPC_DEC) {
    ret = rte_bbdev_dec_op_alloc_bulk(ops_mp, &op_params->ref_dec_op, num_to_process);
    op_params->mp_dec = ops_mp;
  } else {
    ret = rte_bbdev_enc_op_alloc_bulk(ops_mp, &op_params->ref_enc_op, num_to_process);
    op_params->mp_enc = ops_mp;
  }

  AssertFatal(ret == 0, "rte_bbdev_op_alloc_bulk() failed");

  op_params->burst_sz = burst_sz;
  op_params->num_to_process = num_to_process;
  op_params->num_lcores = num_lcores;
  return 0;
}

// based on DPDK BBDEV throughput_pmd_lcore_ldpc_dec
static int
pmd_lcore_ldpc_dec(void *arg)
{
  struct thread_params *tp = arg;
  uint16_t enq, deq;
  int time_out = 0;
  const uint16_t queue_id = tp->queue_id;
  const uint16_t num_segments = tp->p_offloadParams->C;
  struct rte_bbdev_dec_op *ops_enq[num_segments];
  struct rte_bbdev_dec_op *ops_deq[num_segments];
  uint8_t ulsch_id = tp->ulsch_id;
  struct data_buffers *bufs = NULL;
  int i, ret;
  struct rte_bbdev_info info;
  uint16_t num_to_enq;
  uint8_t *p_out = tp->p_out;
  t_nrLDPCoffload_params *p_offloadParams = tp->p_offloadParams;

  AssertFatal((num_segments < MAX_BURST), "BURST_SIZE should be <= %u", MAX_BURST);
  rte_bbdev_info_get(tp->dev_id, &info);
  bufs = &tp->op_params->q_bufs[GET_SOCKET(info.socket_id)][queue_id];
  while (rte_atomic16_read(&tp->op_params->sync) == SYNC_WAIT)
    rte_pause();

  ret = rte_bbdev_dec_op_alloc_bulk(tp->op_params->mp_dec, ops_enq, num_segments);
  AssertFatal(ret == 0, "Allocation failed for %d ops", num_segments);
  set_ldpc_dec_op(ops_enq, 0, bufs, ulsch_id, p_offloadParams);

  for (enq = 0, deq = 0; enq < num_segments;) {
    num_to_enq = num_segments;
    if (unlikely(num_segments - enq < num_to_enq))
      num_to_enq = num_segments - enq;

    enq += rte_bbdev_enqueue_ldpc_dec_ops(tp->dev_id, queue_id, &ops_enq[enq], num_to_enq);
    deq += rte_bbdev_dequeue_ldpc_dec_ops(tp->dev_id, queue_id, &ops_deq[deq], enq - deq);
  }

  /* dequeue the remaining */
  while (deq < enq) {
    deq += rte_bbdev_dequeue_ldpc_dec_ops(tp->dev_id, queue_id, &ops_deq[deq], enq - deq);
    time_out++;
    DevAssert(time_out <= TIME_OUT_POLL);
  }
  if (deq == enq) {
    tp->iter_count = 0;
    /* get the max of iter_count for all dequeued ops */
    for (i = 0; i < num_segments; ++i) {
      uint8_t *status = tp->p_offloadParams->perCB[i].p_status_cb;
      tp->iter_count = RTE_MAX(ops_enq[i]->ldpc_dec.iter_count, tp->iter_count);
      *status = ops_enq[i]->status;
    }
    ret = retrieve_ldpc_dec_op(ops_deq, num_segments, tp->op_params->vector_mask, p_out);
    AssertFatal(ret == 0, "LDPC offload decoder failed!");
  }

  rte_bbdev_dec_op_free_bulk(ops_enq, num_segments);
  // Return the max number of iterations accross all segments
  return tp->iter_count;
}

// based on DPDK BBDEV throughput_pmd_lcore_ldpc_enc
static int pmd_lcore_ldpc_enc(void *arg)
{
  struct thread_params *tp = arg;
  uint16_t enq, deq;
  int time_out = 0;
  const uint16_t queue_id = tp->queue_id;
  const uint16_t num_segments = tp->p_offloadParams->C;
  tp->op_params->num_to_process = num_segments;
  struct rte_bbdev_enc_op *ops_enq[num_segments];
  struct rte_bbdev_enc_op *ops_deq[num_segments];
  struct rte_bbdev_info info;
  int ret;
  struct data_buffers *bufs = NULL;
  uint16_t num_to_enq;
  uint8_t *p_out = tp->p_out;
  t_nrLDPCoffload_params *p_offloadParams = tp->p_offloadParams;

  AssertFatal((num_segments < MAX_BURST), "BURST_SIZE should be <= %u", MAX_BURST);

  rte_bbdev_info_get(tp->dev_id, &info);
  bufs = &tp->op_params->q_bufs[GET_SOCKET(info.socket_id)][queue_id];
  while (rte_atomic16_read(&tp->op_params->sync) == SYNC_WAIT)
    rte_pause();
  ret = rte_bbdev_enc_op_alloc_bulk(tp->op_params->mp_enc, ops_enq, num_segments);
  AssertFatal(ret == 0, "Allocation failed for %d ops", num_segments);

  set_ldpc_enc_op(ops_enq, 0, bufs->inputs, bufs->hard_outputs, p_offloadParams);
  for (enq = 0, deq = 0; enq < num_segments;) {
    num_to_enq = num_segments;
    if (unlikely(num_segments - enq < num_to_enq))
      num_to_enq = num_segments - enq;
    enq += rte_bbdev_enqueue_ldpc_enc_ops(tp->dev_id, queue_id, &ops_enq[enq], num_to_enq);
    deq += rte_bbdev_dequeue_ldpc_enc_ops(tp->dev_id, queue_id, &ops_deq[deq], enq - deq);
  }
  /* dequeue the remaining */
  while (deq < enq) {
    deq += rte_bbdev_dequeue_ldpc_enc_ops(tp->dev_id, queue_id, &ops_deq[deq], enq - deq);
    time_out++;
    DevAssert(time_out <= TIME_OUT_POLL);
  }

  ret = retrieve_ldpc_enc_op(ops_deq, num_segments, p_out, tp->p_offloadParams->perCB);
  AssertFatal(ret == 0, "Failed to retrieve LDPC encoding op!");
  rte_bbdev_enc_op_free_bulk(ops_enq, num_segments);
  return ret;
}

// based on DPDK BBDEV throughput_pmd_lcore_dec
int start_pmd_dec(struct active_device *ad,
                  struct test_op_params *op_params,
                  t_nrLDPCoffload_params *p_offloadParams,
                  uint8_t ulsch_id,
                  uint8_t *p_out)
{
  int ret;
  unsigned int lcore_id, used_cores = 0;
  uint16_t num_lcores;
  /* Set number of lcores */
  num_lcores = (ad->nb_queues < (op_params->num_lcores)) ? ad->nb_queues : op_params->num_lcores;
  /* Allocate memory for thread parameters structure */
  struct thread_params *t_params = rte_zmalloc(NULL, num_lcores * sizeof(struct thread_params), RTE_CACHE_LINE_SIZE);
  AssertFatal(t_params != NULL,
              "Failed to alloc %zuB for t_params",
              RTE_ALIGN(sizeof(struct thread_params) * num_lcores, RTE_CACHE_LINE_SIZE));
  rte_atomic16_set(&op_params->sync, SYNC_WAIT);
  /* Master core is set at first entry */
  t_params[0].dev_id = ad->dev_id;
  t_params[0].lcore_id = rte_lcore_id();
  t_params[0].op_params = op_params;
  t_params[0].queue_id = ad->dec_queue;
  t_params[0].iter_count = 0;
  t_params[0].p_out = p_out;
  t_params[0].p_offloadParams = p_offloadParams;
  t_params[0].ulsch_id = ulsch_id;
  used_cores++;
  // For now, we never enter here, we don't use the DPDK thread pool
  RTE_LCORE_FOREACH_WORKER(lcore_id) {
    if (used_cores >= num_lcores)
      break;
    t_params[used_cores].dev_id = ad->dev_id;
    t_params[used_cores].lcore_id = lcore_id;
    t_params[used_cores].op_params = op_params;
    t_params[used_cores].queue_id = ad->queue_ids[used_cores];
    t_params[used_cores].iter_count = 0;
    t_params[used_cores].p_out = p_out;
    t_params[used_cores].p_offloadParams = p_offloadParams;
    t_params[used_cores].ulsch_id = ulsch_id;
    rte_eal_remote_launch(pmd_lcore_ldpc_dec, &t_params[used_cores++], lcore_id);
  }
  rte_atomic16_set(&op_params->sync, SYNC_START);
  ret = pmd_lcore_ldpc_dec(&t_params[0]);
  /* Master core is always used */
  // for (used_cores = 1; used_cores < num_lcores; used_cores++)
  //	ret |= rte_eal_wait_lcore(t_params[used_cores].lcore_id);
  rte_free(t_params);
  return ret;
}

// based on DPDK BBDEV throughput_pmd_lcore_enc
int32_t start_pmd_enc(struct active_device *ad,
                      struct test_op_params *op_params,
                      t_nrLDPCoffload_params *p_offloadParams,
                      uint8_t *p_out)
{
  unsigned int lcore_id, used_cores = 0;
  uint16_t num_lcores;
  int ret;
  num_lcores = (ad->nb_queues < (op_params->num_lcores)) ? ad->nb_queues : op_params->num_lcores;
  struct thread_params *t_params = rte_zmalloc(NULL, num_lcores * sizeof(struct thread_params), RTE_CACHE_LINE_SIZE);
  rte_atomic16_set(&op_params->sync, SYNC_WAIT);
  t_params[0].dev_id = ad->dev_id;
  t_params[0].lcore_id = rte_lcore_id() + 1;
  t_params[0].op_params = op_params;
  t_params[0].queue_id = ad->enc_queue;
  t_params[0].iter_count = 0;
  t_params[0].p_out = p_out;
  t_params[0].p_offloadParams = p_offloadParams;
  used_cores++;
  // For now, we never enter here, we don't use the DPDK thread pool
  RTE_LCORE_FOREACH_WORKER(lcore_id) {
    if (used_cores >= num_lcores)
      break;
    t_params[used_cores].dev_id = ad->dev_id;
    t_params[used_cores].lcore_id = lcore_id;
    t_params[used_cores].op_params = op_params;
    t_params[used_cores].queue_id = ad->queue_ids[1];
    t_params[used_cores].iter_count = 0;
    rte_eal_remote_launch(pmd_lcore_ldpc_enc, &t_params[used_cores++], lcore_id);
  }
  rte_atomic16_set(&op_params->sync, SYNC_START);
  ret = pmd_lcore_ldpc_enc(&t_params[0]);
  rte_free(t_params);
  return ret;
}

struct test_op_params *op_params = NULL;

struct rte_mbuf *m_head[DATA_NUM_TYPES];

// OAI CODE
int32_t LDPCinit()
{
  pthread_mutex_init(&encode_mutex, NULL);
  pthread_mutex_init(&decode_mutex, NULL);
  int ret;
  int dev_id = 0;
  struct rte_bbdev_info info;
  struct active_device *ad = active_devs;
  char *dpdk_dev = NULL; // PCI address of the card
  char *dpdk_core_list = NULL; // cores used by DPDK for T2
  char *dpdk_file_prefix = NULL;
  paramdef_t LoaderParams[] = {
    {"dpdk_dev",         NULL, 0, .strptr = &dpdk_dev,         .defstrval = NULL,     TYPE_STRING, 0, NULL},
    {"dpdk_core_list",   NULL, 0, .strptr = &dpdk_core_list,   .defstrval = "11-12",  TYPE_STRING, 0, NULL},
    {"dpdk_file_prefix", NULL, 0, .strptr = &dpdk_file_prefix, .defstrval = "b6",     TYPE_STRING, 0, NULL}
  };
  config_get(config_get_if(), LoaderParams, sizeofArray(LoaderParams), "ldpc_offload");
  AssertFatal(dpdk_dev != NULL, "ldpc_offload.dpdk_dev was not provided");
  char *argv_re[] = {"bbdev", "-a", dpdk_dev, "-l", dpdk_core_list, "--file-prefix", dpdk_file_prefix, "--"};
  // EAL initialization, if already initialized (init in xran lib) try to probe DPDK device
  ret = rte_eal_init(sizeofArray(argv_re), argv_re);
  if (ret < 0) {
    printf("EAL initialization failed, probing DPDK device %s\n", dpdk_dev);
    if (rte_dev_probe(dpdk_dev) != 0) {
      LOG_E(PHY, "T2 card %s not found\n", dpdk_dev);
      return (-1);
    }
  }
  // Use only device 0 - first detected device
  rte_bbdev_info_get(0, &info);
  // Set number of queues based on number of initialized cores (-l option) and driver
  // capabilities
  AssertFatal(add_dev(dev_id, &info) == 0, "Failed to setup bbdev");
  AssertFatal(rte_bbdev_stats_reset(dev_id) == 0, "Failed to reset stats of bbdev %d", dev_id);
  AssertFatal(rte_bbdev_start(dev_id) == 0, "Failed to start bbdev %d", dev_id);

  //the previous calls have populated this global variable (beurk)
  // One more global to remove, not thread safe global op_params
  op_params = rte_zmalloc(NULL, sizeof(struct test_op_params), RTE_CACHE_LINE_SIZE);
  AssertFatal(op_params != NULL,
              "Failed to alloc %zuB for op_params",
              RTE_ALIGN(sizeof(struct test_op_params), RTE_CACHE_LINE_SIZE));

  int socket_id = GET_SOCKET(info.socket_id);
  int out_max_sz = 8448; // max code block size (for BG1), 22 * 384
  int in_max_sz = LDPC_MAX_CB_SIZE; // max number of encoded bits (for BG2 and MCS0)
  int num_queues = 1;
  int f_ret = create_mempools(ad, socket_id, num_queues, out_max_sz, in_max_sz);
  if (f_ret != 0) {
    printf("Couldn't create mempools");
    return -1;
  }
  f_ret = init_test_op_params(op_params, RTE_BBDEV_OP_LDPC_DEC, ad->bbdev_dec_op_pool, num_queues, num_queues, 1);
  f_ret = init_test_op_params(op_params, RTE_BBDEV_OP_LDPC_ENC, ad->bbdev_enc_op_pool, num_queues, num_queues, 1);
  if (f_ret != 0) {
    printf("Couldn't init test op params");
    return -1;
  }
  return 0;
}

int32_t LDPCshutdown()
{
  struct active_device *ad = active_devs;
  int dev_id = 0;
  struct rte_bbdev_stats stats;
  free_buffers(ad, op_params);
  rte_free(op_params);
  rte_bbdev_stats_get(dev_id, &stats);
  rte_bbdev_stop(dev_id);
  rte_bbdev_close(dev_id);
  memset(active_devs, 0, sizeof(active_devs));
  nb_active_devs = 0;
  return 0;
}

int32_t LDPCdecoder(struct nrLDPC_dec_params *p_decParams,
                    uint8_t harq_pid,
                    uint8_t ulsch_id,
                    uint8_t C,
                    int8_t *p_llr,
                    int8_t *p_out,
                    t_nrLDPC_time_stats *p_profiler,
                    decode_abort_t *ab)
{
  pthread_mutex_lock(&decode_mutex);
  // hardcoded we use first device
  struct active_device *ad = active_devs;
  t_nrLDPCoffload_params offloadParams = {.n_cb = (p_decParams->BG == 1) ? (66 * p_decParams->Z) : (50 * p_decParams->Z),
                                          .BG = p_decParams->BG,
                                          .Z = p_decParams->Z,
                                          .rv = p_decParams->rv,
                                          .F = p_decParams->F,
                                          .Qm = p_decParams->Qm,
                                          .numMaxIter = p_decParams->numMaxIter,
                                          .C = C,
                                          .setCombIn = p_decParams->setCombIn};

  for (int r = 0; r < C; r++) {
    offloadParams.perCB[r].E_cb = p_decParams->perCB[r].E_cb;
    offloadParams.perCB[r].p_status_cb = &(p_decParams->perCB[r].status_cb);
  }
  struct rte_bbdev_info info;
  int ret;
  rte_bbdev_info_get(ad->dev_id, &info);
  int socket_id = GET_SOCKET(info.socket_id);
  // fill_queue_buffers -> init_op_data_objs
  struct rte_mempool *in_mp = ad->in_mbuf_pool;
  struct rte_mempool *hard_out_mp = ad->hard_out_mbuf_pool;
  struct rte_mempool *soft_out_mp = ad->soft_out_mbuf_pool;
  struct rte_mempool *harq_in_mp = ad->harq_in_mbuf_pool;
  struct rte_mempool *harq_out_mp = ad->harq_out_mbuf_pool;
  struct rte_mempool *mbuf_pools[DATA_NUM_TYPES] = {in_mp, soft_out_mp, hard_out_mp, harq_in_mp, harq_out_mp};
  uint8_t queue_id = ad->dec_queue;
  struct rte_bbdev_op_data **queue_ops[DATA_NUM_TYPES] = {&op_params->q_bufs[socket_id][queue_id].inputs,
                                                          &op_params->q_bufs[socket_id][queue_id].soft_outputs,
                                                          &op_params->q_bufs[socket_id][queue_id].hard_outputs,
                                                          &op_params->q_bufs[socket_id][queue_id].harq_inputs,
                                                          &op_params->q_bufs[socket_id][queue_id].harq_outputs};
  for (enum op_data_type type = DATA_INPUT; type < 3; type += 2) {
    ret = allocate_buffers_on_socket(queue_ops[type], C * sizeof(struct rte_bbdev_op_data), socket_id);
    AssertFatal(ret == 0, "Couldn't allocate memory for rte_bbdev_op_data structs");
    ret = init_op_data_objs_dec(*queue_ops[type],
                                (uint8_t *)p_llr,
                                &offloadParams,
                                mbuf_pools[type],
                                C,
                                type,
                                info.drv.min_alignment);
    AssertFatal(ret == 0, "Couldn't init rte_bbdev_op_data structs");
  }
  ret = start_pmd_dec(ad, op_params, &offloadParams, ulsch_id, (uint8_t *)p_out);
  if (ret < 0) {
    printf("Couldn't start pmd dec\n");
    pthread_mutex_unlock(&decode_mutex);
    return (p_decParams->numMaxIter);
  }
  pthread_mutex_unlock(&decode_mutex);
  return ret;
}

int32_t LDPCencoder(unsigned char **input, unsigned char **output, encoder_implemparams_t *impp)
{
  pthread_mutex_lock(&encode_mutex);
  // hardcoded to use the first found board
  struct active_device *ad = active_devs;
  int ret;
  uint32_t Nref = 0;
  t_nrLDPCoffload_params offloadParams = {.n_cb = (impp->BG == 1) ? (66 * impp->Zc) : (50 * impp->Zc),
                                          .BG = impp->BG,
                                          .Z = impp->Zc,
                                          .rv = impp->rv,
                                          .F = impp->F,
                                          .Qm = impp->Qm,
                                          .C = impp->n_segments,
                                          .Kr = (impp->K - impp->F + 7) / 8};
  for (int r = 0; r < impp->n_segments; r++) {
    offloadParams.perCB[r].E_cb = impp->perCB[r].E_cb;
  }
  if (impp->Tbslbrm != 0) {
    Nref = 3 * impp->Tbslbrm / (2 * impp->n_segments);
    offloadParams.n_cb = min(offloadParams.n_cb, Nref);
  }
  struct rte_bbdev_info info;
  rte_bbdev_info_get(ad->dev_id, &info);
  int socket_id = GET_SOCKET(info.socket_id);
  // fill_queue_buffers -> init_op_data_objs
  struct rte_mempool *in_mp = ad->in_mbuf_pool;
  struct rte_mempool *hard_out_mp = ad->hard_out_mbuf_pool;
  struct rte_mempool *soft_out_mp = ad->soft_out_mbuf_pool;
  struct rte_mempool *harq_in_mp = ad->harq_in_mbuf_pool;
  struct rte_mempool *harq_out_mp = ad->harq_out_mbuf_pool;
  struct rte_mempool *mbuf_pools[DATA_NUM_TYPES] = {in_mp, soft_out_mp, hard_out_mp, harq_in_mp, harq_out_mp};
  uint8_t queue_id = ad->enc_queue;
  struct rte_bbdev_op_data **queue_ops[DATA_NUM_TYPES] = {&op_params->q_bufs[socket_id][queue_id].inputs,
                                                          &op_params->q_bufs[socket_id][queue_id].soft_outputs,
                                                          &op_params->q_bufs[socket_id][queue_id].hard_outputs,
                                                          &op_params->q_bufs[socket_id][queue_id].harq_inputs,
                                                          &op_params->q_bufs[socket_id][queue_id].harq_outputs};
  for (enum op_data_type type = DATA_INPUT; type < 3; type += 2) {
    ret = allocate_buffers_on_socket(queue_ops[type], impp->n_segments * sizeof(struct rte_bbdev_op_data), socket_id);
    AssertFatal(ret == 0, "Couldn't allocate memory for rte_bbdev_op_data structs");
    ret = init_op_data_objs_enc(*queue_ops[type],
                                input,
                                &offloadParams,
                                m_head[type],
                                mbuf_pools[type],
                                impp->n_segments,
                                type,
                                info.drv.min_alignment);
    AssertFatal(ret == 0, "Couldn't init rte_bbdev_op_data structs");
  }
  ret = start_pmd_enc(ad, op_params, &offloadParams, *output);
  pthread_mutex_unlock(&encode_mutex);
  return ret;
}
