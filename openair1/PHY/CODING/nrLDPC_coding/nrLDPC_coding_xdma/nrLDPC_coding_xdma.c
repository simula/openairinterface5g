/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.0  (the "License"); you may not use this
 * file except in compliance with the License. You may obtain a copy of the
 * License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * -------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*! \file PHY/CODING/nrLDPC_coding/nrLDPC_coding_xdma/nrLDPC_coding_xdma.c
 * \brief Top-level routines for decoding LDPC (ULSCH) transport channels
 * decoding implemented using a FEC IP core on FPGA through XDMA driver
 */

// [from gNB coding]
#include <syscall.h>

#include <nr_rate_matching.h>
#include "PHY/CODING/coding_defs.h"
#include "PHY/CODING/coding_extern.h"
#include "PHY/CODING/lte_interleaver_inline.h"
#include "PHY/CODING/nrLDPC_coding/nrLDPC_coding_xdma/nrLDPC_coding_xdma_offload.h"
#include "PHY/CODING/nrLDPC_extern.h"
#include "PHY/NR_TRANSPORT/nr_dlsch.h"
#include "PHY/NR_TRANSPORT/nr_transport_common_proto.h"
#include "PHY/NR_TRANSPORT/nr_transport_proto.h"
#include "PHY/NR_TRANSPORT/nr_ulsch.h"
#include "PHY/defs_gNB.h"
#include "SCHED_NR/fapi_nr_l1.h"
#include "SCHED_NR/sched_nr.h"
#include "common/utils/LOG/log.h"
#include "common/utils/LOG/vcd_signal_dumper.h"
#include "defs.h"
// #define DEBUG_ULSCH_DECODING
// #define gNB_DEBUG_TRACE

#define OAI_UL_LDPC_MAX_NUM_LLR 27000 // 26112 // NR_LDPC_NCOL_BG1*NR_LDPC_ZMAX = 68*384
// #define DEBUG_CRC
#ifdef DEBUG_CRC
#define PRINT_CRC_CHECK(a) a
#else
#define PRINT_CRC_CHECK(a)
#endif

// extern double cpuf;

#include "nfapi/open-nFAPI/nfapi/public_inc/nfapi_interface.h"
#include "nfapi/open-nFAPI/nfapi/public_inc/nfapi_nr_interface.h"

#include "PHY/CODING/nrLDPC_coding/nrLDPC_coding_interface.h"

// Global var to limit the rework of the dirty legacy code
int num_threads_prepare_max = 0;
char *user_device = NULL;
char *enc_read_device = NULL;
char *enc_write_device = NULL;
char *dec_read_device = NULL;
char *dec_write_device = NULL;

/*!
 * \typedef args_fpga_decode_prepare_t
 * \struct args_fpga_decode_prepare_s
 * \brief arguments structure for passing arguments to the nr_ulsch_FPGA_decoding_prepare_blocks function
 */
typedef struct args_fpga_decode_prepare_s {
  nrLDPC_TB_decoding_parameters_t *TB_params; /*!< transport blocks parameters */

  uint8_t *multi_indata; /*!< pointer to the head of the block destination array that is then passed to the FPGA decoding */
  int no_iteration_ldpc; /*!< pointer to the number of iteration set by this function */
  uint32_t r_first; /*!< index of the first block to be prepared within this function */
  uint32_t r_span; /*!< number of blocks to be prepared within this function */
  int r_offset; /*!< r index expressed in bits */
  int input_CBoffset; /*!< */
  int Kc; /*!< ratio between the number of columns in the parity check graph and the lifting size */
  int Kprime; /*!< size of payload and CRC bits in a code block */
  task_ans_t *ans; /*!< pointer to the answer that is used by thread pool to detect job completion */
} args_fpga_decode_prepare_t;

int32_t nrLDPC_coding_init(void);
int32_t nrLDPC_coding_shutdown(void);
int32_t nrLDPC_coding_decoder(nrLDPC_slot_decoding_parameters_t *slot_params, int frame_rx, int slot_rx);
// int32_t nrLDPC_coding_encoder(void);
int decoder_xdma(nrLDPC_TB_decoding_parameters_t *TB_params, int frame_rx, int slot_rx, tpool_t *ldpc_threadPool);
void nr_ulsch_FPGA_decoding_prepare_blocks(void *args);

int32_t nrLDPC_coding_init(void)
{
  paramdef_t LoaderParams[] = {
      {"num_threads_prepare", NULL, 0, .iptr = &num_threads_prepare_max, .defintval = 0, TYPE_INT, 0, NULL},
      {"user_device", NULL, 0, .strptr = &user_device, .defstrval = DEVICE_NAME_DEFAULT_USER, TYPE_STRING, 0, NULL},
      {"enc_read_device", NULL, 0, .strptr = &enc_read_device, .defstrval = DEVICE_NAME_DEFAULT_ENC_READ, TYPE_STRING, 0, NULL},
      {"enc_write_device", NULL, 0, .strptr = &enc_write_device, .defstrval = DEVICE_NAME_DEFAULT_ENC_WRITE, TYPE_STRING, 0, NULL},
      {"dec_read_device", NULL, 0, .strptr = &dec_read_device, .defstrval = DEVICE_NAME_DEFAULT_DEC_READ, TYPE_STRING, 0, NULL},
      {"dec_write_device", NULL, 0, .strptr = &dec_write_device, .defstrval = DEVICE_NAME_DEFAULT_DEC_WRITE, TYPE_STRING, 0, NULL}};
  config_get(config_get_if(), LoaderParams, sizeofArray(LoaderParams), "nrLDPC_coding_xdma");
  AssertFatal(num_threads_prepare_max != 0, "nrLDPC_coding_xdma.num_threads_prepare was not provided");

  return 0;
}

int32_t nrLDPC_coding_shutdown(void)
{
  return 0;
}

int32_t nrLDPC_coding_decoder(nrLDPC_slot_decoding_parameters_t *slot_params, int frame_rx, int slot_rx)
{
  int nbDecode = 0;
  for (int ULSCH_id = 0; ULSCH_id < slot_params->nb_TBs; ULSCH_id++)
    nbDecode += decoder_xdma(&slot_params->TBs[ULSCH_id], frame_rx, slot_rx, slot_params->threadPool);
  return nbDecode;
}

/*
int32_t nrLDPC_coding_encoder(void)
{
  return 0;
}
*/

int decoder_xdma(nrLDPC_TB_decoding_parameters_t *TB_params, int frame_rx, int slot_rx, tpool_t *ldpc_threadPool)
{
  const uint32_t K = TB_params->K;
  const int Kc = TB_params->BG == 2 ? 52 : 68;
  int r_offset = 0, offset = 0;
  int Kprime = K - TB_params->F;

  // FPGA parameter preprocessing
  static uint8_t multi_indata[27000 * 25]; // FPGA input data
  static uint8_t multi_outdata[1100 * 25]; // FPGA output data

  int bg_len = TB_params->BG == 1 ? 22 : 10;

  // Calc input CB offset
  int input_CBoffset = TB_params->Z * Kc * 8;
  if ((input_CBoffset & 0x7F) == 0)
    input_CBoffset = input_CBoffset / 8;
  else
    input_CBoffset = 16 * ((input_CBoffset / 128) + 1);

  DecIFConf dec_conf = {0};
  dec_conf.Zc = TB_params->Z;
  dec_conf.BG = TB_params->BG;
  dec_conf.max_iter = TB_params->max_ldpc_iterations;
  dec_conf.numCB = TB_params->C;
  // input soft bits length, Zc x 66 - length of filler bits
  dec_conf.numChannelLls = (Kprime - 2 * TB_params->Z) + (Kc * TB_params->Z - K);
  // filler bits length
  dec_conf.numFillerBits = TB_params->F;
  dec_conf.max_schedule = 0;
  dec_conf.SetIdx = 12;
  dec_conf.nRows = (dec_conf.BG == 1) ? 46 : 42;

  dec_conf.user_device = user_device;
  dec_conf.enc_read_device = enc_read_device;
  dec_conf.enc_write_device = enc_write_device;
  dec_conf.dec_read_device = dec_read_device;
  dec_conf.dec_write_device = dec_write_device;

  int out_CBoffset = dec_conf.Zc * bg_len;
  if ((out_CBoffset & 0x7F) == 0)
    out_CBoffset = out_CBoffset / 8;
  else
    out_CBoffset = 16 * ((out_CBoffset / 128) + 1);

#ifdef LDPC_DATA
  printf("\n------------------------\n");
  printf("BG:\t\t%d\n", dec_conf.BG);
  printf("TB_params->C: %d\n", TB_params->C);
  printf("TB_params->K: %d\n", TB_params->K);
  printf("TB_params->Z: %d\n", TB_params->Z);
  printf("TB_params->F: %d\n", TB_params->F);
  printf("numChannelLls:\t %d = (%d - 2 * %d) + (%d * %d - %d)\n",
         dec_conf.numChannelLls,
         Kprime,
         TB_params->Z,
         Kc,
         TB_params->Z,
         K);
  printf("numFillerBits:\t %d\n", TB_params->F);
  printf("------------------------\n");
  // ===================================
  // debug mode
  // ===================================
  FILE *fptr_llr, *fptr_ldpc;
  fptr_llr = fopen("../../../cmake_targets/log/ulsim_ldpc_llr.txt", "w");
  fptr_ldpc = fopen("../../../cmake_targets/log/ulsim_ldpc_output.txt", "w");
  // ===================================
#endif

  int length_dec = lenWithCrc(TB_params->C, TB_params->A);
  uint8_t crc_type = crcType(TB_params->C, TB_params->A);
  int no_iteration_ldpc = 2;

  uint32_t num_threads_prepare = 0;

  // calculate required number of jobs
  uint32_t r_while = 0;
  while (r_while < TB_params->C) {
    // calculate number of segments processed in the new job
    uint32_t modulus = (TB_params->C - r_while) % (num_threads_prepare_max - num_threads_prepare);
    uint32_t quotient = (TB_params->C - r_while) / (num_threads_prepare_max - num_threads_prepare);
    uint32_t r_span_max = modulus == 0 ? quotient : quotient + 1;

    // saturate to be sure to not go above C
    uint32_t r_span = TB_params->C - r_while < r_span_max ? TB_params->C - r_while : r_span_max;

    // increment
    num_threads_prepare++;
    r_while += r_span;
  }

  args_fpga_decode_prepare_t arr[num_threads_prepare];
  task_ans_t ans[num_threads_prepare];
  memset(ans, 0, num_threads_prepare * sizeof(task_ans_t));
  thread_info_tm_t t_info = {.buf = (uint8_t *)arr, .len = 0, .cap = num_threads_prepare, .ans = ans};

  // start the prepare jobs
  uint32_t r_remaining = 0;
  for (uint32_t r = 0; r < TB_params->C; r++) {
    nrLDPC_segment_decoding_parameters_t *segment_params = &TB_params->segments[r];
    if (r_remaining == 0) {
      // TODO: int nr_tti_rx = 0;

      args_fpga_decode_prepare_t *args = &((args_fpga_decode_prepare_t *)t_info.buf)[t_info.len];
      DevAssert(t_info.len < t_info.cap);
      args->ans = &t_info.ans[t_info.len];
      t_info.len += 1;

      args->TB_params = TB_params;
      args->multi_indata = multi_indata;
      args->no_iteration_ldpc = no_iteration_ldpc;
      args->r_first = r;

      uint32_t modulus = (TB_params->C - r) % (num_threads_prepare_max - num_threads_prepare);
      uint32_t quotient = (TB_params->C - r) / (num_threads_prepare_max - num_threads_prepare);
      uint32_t r_span_max = modulus == 0 ? quotient : quotient + 1;

      uint32_t r_span = TB_params->C - r < r_span_max ? TB_params->C - r : r_span_max;
      args->r_span = r_span;
      args->r_offset = r_offset;
      args->input_CBoffset = input_CBoffset;
      args->Kc = Kc;
      args->Kprime = Kprime;

      r_remaining = r_span;

      task_t t = {.func = &nr_ulsch_FPGA_decoding_prepare_blocks, .args = args};
      pushTpool(ldpc_threadPool, t);

      LOG_D(PHY, "Added %d block(s) to prepare for decoding, in pipe: %d to %d\n", r_span, r, r + r_span - 1);
    }
    r_offset += segment_params->E;
    offset += ((K >> 3) - (TB_params->F >> 3) - ((TB_params->C > 1) ? 3 : 0));
    r_remaining -= 1;
  }

  // reset offset in order to properly fill the output array later
  offset = 0;

  DevAssert(num_threads_prepare == t_info.len);

  // wait for the prepare jobs to complete
  join_task_ans(t_info.ans);

  for (uint32_t job = 0; job < num_threads_prepare; job++) {
    args_fpga_decode_prepare_t *args = &arr[job];
    if (args->no_iteration_ldpc >= TB_params->max_ldpc_iterations)
      no_iteration_ldpc = TB_params->max_ldpc_iterations;
  }

  // launch decode with FPGA
  LOG_I(PHY, "Run the LDPC ------[FPGA version]------\n");
  //==================================================================
  //  Xilinx FPGA LDPC decoding function -> nrLDPC_decoder_FPGA_PYM()
  //==================================================================
  start_meas(&TB_params->segments[0].ts_ldpc_decode);
  nrLDPC_decoder_FPGA_PYM(&multi_indata[0], &multi_outdata[0], dec_conf);
  // printf("Xilinx FPGA -> CB = %d\n", harq_process->C);
  stop_meas(&TB_params->segments[0].ts_ldpc_decode);

  *TB_params->processedSegments = 0;
  for (uint32_t r = 0; r < TB_params->C; r++) {
    // ------------------------------------------------------------
    // --------------------- copy FPGA output ---------------------
    // ------------------------------------------------------------
    nrLDPC_segment_decoding_parameters_t *segment_params = &TB_params->segments[r];
    if (check_crc(multi_outdata, length_dec, crc_type)) {
#ifdef DEBUG_CRC
      LOG_I(PHY, "Segment %d CRC OK\n", r);
#endif
      no_iteration_ldpc = 2;
    } else {
#ifdef DEBUG_CRC
      LOG_I(PHY, "segment %d CRC NOK\n", r);
#endif
      no_iteration_ldpc = TB_params->max_ldpc_iterations;
    }
    for (int i = 0; i < out_CBoffset; i++) {
      segment_params->c[i] = multi_outdata[i + r * out_CBoffset];
    }
    segment_params->decodeSuccess = (no_iteration_ldpc < TB_params->max_ldpc_iterations);
    if (segment_params->decodeSuccess) {
      *TB_params->processedSegments = *TB_params->processedSegments + 1;
    }
  }

  return 0;
}

/*!
 * \fn nr_ulsch_FPGA_decoding_prepare_blocks(void *args)
 * \brief prepare blocks for LDPC decoding on FPGA
 *
 * \param args pointer to the arguments of the function in a structure of type args_fpga_decode_prepare_t
 */
void nr_ulsch_FPGA_decoding_prepare_blocks(void *args)
{
  // extract the arguments
  args_fpga_decode_prepare_t *arguments = (args_fpga_decode_prepare_t *)args;

  nrLDPC_TB_decoding_parameters_t *TB_params = arguments->TB_params;

  uint8_t Qm = TB_params->Qm;

  uint8_t BG = TB_params->BG;
  uint8_t rv_index = TB_params->rv_index;
  uint8_t max_ldpc_iterations = TB_params->max_ldpc_iterations;

  uint32_t tbslbrm = TB_params->tbslbrm;
  uint32_t K = TB_params->K;
  uint32_t Z = TB_params->Z;
  uint32_t F = TB_params->F;

  uint32_t C = TB_params->C;

  nrLDPC_segment_decoding_parameters_t *segment_params = &TB_params->segments[0];

  short *ulsch_llr = segment_params->llr;

  uint8_t *multi_indata = arguments->multi_indata;
  int no_iteration_ldpc = arguments->no_iteration_ldpc;
  uint32_t r_first = arguments->r_first;
  uint32_t r_span = arguments->r_span;
  int r_offset = arguments->r_offset;
  int input_CBoffset = arguments->input_CBoffset;
  int Kc = arguments->Kc;
  int Kprime = arguments->Kprime;

  int16_t z[68 * 384 + 16] __attribute__((aligned(16)));
  simde__m128i *pv = (simde__m128i *)&z;

  // the function processes r_span blocks starting from block at index r_first in ulsch_llr
  for (uint32_t r = r_first; r < (r_first + r_span); r++) {
    nrLDPC_segment_decoding_parameters_t *segment_params = &TB_params->segments[r];
    // ----------------------- FPGA pre process ------------------------
    simde__m128i ones = simde_mm_set1_epi8(255); // Generate a vector with all elements set to 255
    simde__m128i *temp_multi_indata = (simde__m128i *)&multi_indata[r * input_CBoffset];
    // -----------------------------------------------------------------

    // code blocks after bit selection in rate matching for LDPC code (38.212 V15.4.0 section 5.4.2.1)
    int16_t harq_e[segment_params->E];
    // -------------------------------------------------------------------------------------------
    // deinterleaving
    // -------------------------------------------------------------------------------------------
    start_meas(&segment_params->ts_deinterleave);
    nr_deinterleaving_ldpc(segment_params->E, Qm, harq_e, ulsch_llr + r_offset);
    stop_meas(&segment_params->ts_deinterleave);
    // -------------------------------------------------------------------------------------------
    // dematching
    // -------------------------------------------------------------------------------------------
    start_meas(&segment_params->ts_rate_unmatch);
    if (nr_rate_matching_ldpc_rx(tbslbrm,
                                 BG,
                                 Z,
                                 segment_params->d,
                                 harq_e,
                                 C,
                                 rv_index,
                                 *segment_params->d_to_be_cleared,
                                 segment_params->E,
                                 F,
                                 K - F - 2 * Z)
        == -1) {
      stop_meas(&segment_params->ts_rate_unmatch);
      LOG_E(PHY, "ulsch_decoding.c: Problem in rate_matching\n");
      no_iteration_ldpc = max_ldpc_iterations;
      arguments->no_iteration_ldpc = no_iteration_ldpc;
      return;
    } else {
      stop_meas(&segment_params->ts_rate_unmatch);
    }

    *segment_params->d_to_be_cleared = false;

    memset(segment_params->c, 0, K >> 3);

    // set first 2*Z_c bits to zeros
    memset(&z[0], 0, 2 * Z * sizeof(int16_t));
    // set Filler bits
    memset((&z[0] + Kprime), 127, F * sizeof(int16_t));
    // Move coded bits before filler bits
    memcpy((&z[0] + 2 * Z), segment_params->d, (Kprime - 2 * Z) * sizeof(int16_t));
    // skip filler bits
    memcpy((&z[0] + K), segment_params->d + (K - 2 * Z), (Kc * Z - K) * sizeof(int16_t));

    // Saturate coded bits before decoding into 8 bits values
    for (int i = 0, j = 0; j < ((Kc * Z) >> 4); i += 2, j++) {
      temp_multi_indata[j] =
          simde_mm_xor_si128(simde_mm_packs_epi16(pv[i], pv[i + 1]),
                             simde_mm_cmpeq_epi32(ones,
                                                  ones)); // Perform NOT operation and write the result to temp_multi_indata[j]
    }

    // the last bytes before reaching "Kc * harq_process->Z" should not be written 128 bits at a time to avoid overwritting the
    // following block in multi_indata
    simde__m128i tmp =
        simde_mm_xor_si128(simde_mm_packs_epi16(pv[2 * ((Kc * Z) >> 4)], pv[2 * ((Kc * Z) >> 4) + 1]),
                           simde_mm_cmpeq_epi32(ones,
                                                ones)); // Perform NOT operation and write the result to temp_multi_indata[j]
    uint8_t *tmp_p = (uint8_t *)&tmp;
    for (int i = 0, j = ((Kc * Z) & 0xfffffff0); j < Kc * Z; i++, j++) {
      multi_indata[r * input_CBoffset + j] = tmp_p[i];
    }

    r_offset += segment_params->E;
  }

  arguments->no_iteration_ldpc = no_iteration_ldpc;
}
