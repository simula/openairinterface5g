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

/*! \file radio/fhi_72/armral_bfp_compression.h
 * \brief BFP compression for the O-RAN 7.2 FrontHaul Interface for Arm systems using the Arm RAN Acceleration Library
 * \author Romain Beurdouche
 * \date 2025
 * \company EURECOM
 * \email romain.beurdouche@eurecom.fr
 * \note ArmRAL available at https://git.gitlab.arm.com/networking/ral.git
 * \warning
 */

#ifndef __ARMRAL_BFP_COMPRESSION_H__
#define __ARMRAL_BFP_COMPRESSION_H__

/**
 * The function operates on a fixed block size of one Physical Resource Block
 * (PRB). Each block consists of 12 16-bit complex resource elements. Each block
 * taken as input is compressed into 24 samples with a common unsigned scaling factor.
 *
 * @param[in]     iq_width  Width in bits of compressed samples.
 * @param[in]     n_prb     The number of input resource blocks.
 * @param[in]     src       Points to the input complex samples sequence.
 * @param[out]    dst       Points to the output compressed data.
 */
void armral_bfp_compression(uint32_t iq_width, uint32_t n_prb, int16_t *src, int8_t *dst);

/**
 * The function operates on a fixed block size of one Physical Resource Block
 * (PRB). Each block consists of 12 compressed complex resource elements.
 * Each block taken as input is expanded into 12 16-bit complex samples.
 *
 * @param[in]     iq_width  Width in bits of compressed samples.
 * @param[in]     n_prb     The number of input resource blocks.
 * @param[in]     src       Points to the input compressed data.
 * @param[out]    dst       Points to the output complex samples sequence.
 */
void armral_bfp_decompression(uint32_t iq_width, uint32_t n_prb, int8_t *src, int16_t *dst);

#endif
