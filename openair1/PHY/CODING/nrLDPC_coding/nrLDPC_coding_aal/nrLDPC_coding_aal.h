/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef __NRLDPC_CODING_T2_H__
#define __NRLDPC_CODING_T2_H__

#include <stddef.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_log.h>

#define MAX_BURST 512U

enum op_data_type {
  DATA_INPUT = 0,
  DATA_HARD_OUTPUT,
  DATA_HARQ_OUTPUT,
  DATA_NUM_TYPES,
};

#endif
