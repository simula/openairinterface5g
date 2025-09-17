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

#ifndef __NR_RATE_MATCHING__H__
#define __NR_RATE_MATCHING__H__

#include <stdint.h>

/**
 * \brief interleave a code segment after encoding and rate matching
 * \param E size of the code segment in bits
 * \param Qm modulation order
 * \param e input rate matched segment
 * \param f output interleaved segment
 */
void nr_interleaving_ldpc(uint32_t E, uint8_t Qm, uint8_t *e, uint8_t *f);

/**
 * \brief deinterleave a code segment before RX rate matching and decoding
 * \param E size of the code segment in bits
 * \param Qm modulation order
 * \param e output deinterleaved segment
 * \param f input llr segment
 */
void nr_deinterleaving_ldpc(uint32_t E, uint8_t Qm, int16_t *e, int16_t *f);

/**
 * \brief rate match a code segment after encoding
 * \Tbslbrm Transport Block size LBRM
 * \param BG LDPC base graph number
 * \param Z segment lifting size
 * \param d input encoded segment
 * \param e output rate matched segment
 * \param C number of segments in the Transport Block
 * \param F number of filler bits in the segment
 * \param Foffset offset of the filler bits in the segment
 * \param rvidx redundancy version index
 * \param E size of the code segment in bits
 */
int nr_rate_matching_ldpc(uint32_t Tbslbrm,
                          uint8_t BG,
                          uint16_t Z,
                          uint8_t *d,
                          uint8_t *e,
                          uint8_t C,
                          uint32_t F,
                          uint32_t Foffset,
                          uint8_t rvidx,
                          uint32_t E);

/**
 * \brief rate match a code segment before decoding
 * \Tbslbrm Transport Block size LBRM
 * \param BG LDPC base graph number
 * \param Z segment lifting size
 * \param d output rate matched segment
 * \param soft_input input deinterleaved segment
 * \param C number of segments in the Transport Block
 * \param rvidx redundancy version index
 * \param clear flag to clear d on the first round of a new HARQ process
 * \param E size of the code segment in bits
 * \param F number of filler bits in the segment
 * \param Foffset offset of the filler bits in the segment
 */
int nr_rate_matching_ldpc_rx(uint32_t Tbslbrm,
                             uint8_t BG,
                             uint16_t Z,
                             int16_t *d,
                             int16_t *soft_input,
                             uint8_t C,
                             uint8_t rvidx,
                             uint8_t clear,
                             uint32_t E,
                             uint32_t F,
                             uint32_t Foffset);

#endif
