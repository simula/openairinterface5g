/*
 * Copyright (c) 2016-present,  Xilinx, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license
 * the terms of the BSD Licence are reported below:
 *
 * BSD License
 * 
 * For Xilinx DMA IP software
 * 
 * Copyright (c) 2016-present, Xilinx, Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 *  * Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 *  * Neither the name Xilinx nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MODULES_TXCTRL_INC_XDMA_DIAG_H_
#define MODULES_TXCTRL_INC_XDMA_DIAG_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  unsigned char max_schedule; // max_schedule = 0;
  unsigned char mb; // mb = 32;
  unsigned char CB_num; // id = CB_num;
  unsigned char BGSel; // bg = 1;
  unsigned char z_set; // z_set = 0;
  unsigned char z_j; // z_j = 6;
  unsigned char max_iter; // max_iter = 8;
  unsigned char SetIdx; // sc_idx = 12;
} DecIPConf;

typedef struct {
  int SetIdx;
  int NumCBSegm;
  int PayloadLen;
  int Z;
  int z_set;
  int z_j;
  int Kbmax;
  int BGSel;
  unsigned mb;
  unsigned char CB_num;
  unsigned char kb_1;
} EncIPConf;

typedef struct {
  char *user_device, *enc_write_device, *enc_read_device, *dec_write_device, *dec_read_device;
} devices_t;

/* ltoh: little to host */
/* htol: little to host */
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define ltohl(x) (x)
#define ltohs(x) (x)
#define htoll(x) (x)
#define htols(x) (x)
#elif __BYTE_ORDER == __BIG_ENDIAN
#define ltohl(x) __bswap_32(x)
#define ltohs(x) __bswap_16(x)
#define htoll(x) __bswap_32(x)
#define htols(x) __bswap_16(x)
#endif

#define MAP_SIZE (32 * 1024UL)
#define MAP_MASK (MAP_SIZE - 1)

#define SIZE_DEFAULT (32)
#define COUNT_DEFAULT (1)

#define OFFSET_DEC_IN 0x0000
#define OFFSET_DEC_OUT 0x0004
#define OFFSET_ENC_IN 0x0008
#define OFFSET_ENC_OUT 0x000c
#define OFFSET_RESET 0x0020
#define PCIE_OFF 0x0030

#define CB_PROCESS_NUMBER 24 // add by JW
#define CB_PROCESS_NUMBER_Dec 24

// dma_from_device.c

int test_dma_enc_read(char *EncOut, EncIPConf Confparam);
int test_dma_enc_write(char *data, EncIPConf Confparam);
int test_dma_dec_read(char *DecOut, DecIPConf Confparam);
int test_dma_dec_write(char *data, DecIPConf Confparam);
void test_dma_init(devices_t devices);
void test_dma_shutdown();
void dma_reset(devices_t devices);

#ifdef __cplusplus
}
#endif
#endif
