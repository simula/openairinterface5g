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

#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 500

#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <byteswap.h>
#include <signal.h>
#include <ctype.h>
#include <termios.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "xdma_diag.h"
#include "nrLDPC_coding_xdma_offload.h"

#include "common/utils/assertions.h"

typedef unsigned long long U64;
void* map_base;
int fd;
int fd_enc_write, fd_enc_read;
char *dev_enc_write, *dev_enc_read;
int fd_dec_write, fd_dec_read;
char *dev_dec_write, *dev_dec_read;
char allocated_write[24 * 1024] __attribute__((aligned(4096)));
char allocated_read[24 * 1024 * 3] __attribute__((aligned(4096)));

// dma_from_device.c

// [Start] #include "dma_utils.c" ===================================

/*
 * man 2 write:
 * On Linux, write() (and similar system calls) will transfer at most
 * 	0x7ffff000 (2,147,479,552) bytes, returning the number of bytes
 *	actually transferred.  (This is true on both 32-bit and 64-bit
 *	systems.)
 */

#define RW_MAX_SIZE 0x7ffff000

int verbose = 0;

uint64_t getopt_integer(char* optarg)
{
  int rc;
  uint64_t value;

  rc = sscanf(optarg, "0x%lx", &value);
  if (rc <= 0)
    rc = sscanf(optarg, "%lu", &value);

  return value;
}

ssize_t read_to_buffer(char* fname, int fd, char* buffer, uint64_t size, uint64_t base)
{
  ssize_t rc;
  uint64_t count = 0;
  char* buf = buffer;
  off_t offset = base;

  while (count < size) {
    uint64_t bytes = size - count;

    if (bytes > RW_MAX_SIZE)
      bytes = RW_MAX_SIZE;

    if (offset) {
      rc = lseek(fd, offset, SEEK_SET);
      if (rc != offset) {
        fprintf(stderr, "%s, seek off 0x%lx != 0x%lx.\n", fname, rc, offset);
        perror("seek file");
        return -EIO;
      }
    }
    /* read data from file into memory buffer */
    rc = read(fd, buf, bytes);

    if (rc != bytes) {
      fprintf(stderr, "%s, R off 0x%lx, 0x%lx != 0x%lx.\n", fname, count, rc, bytes);
      perror("read file");
      return -EIO;
    }

    count += bytes;
    buf += bytes;
    offset += bytes;
  }

  if (count != size) {
    fprintf(stderr, "%s, R failed 0x%lx != 0x%lx.\n", fname, count, size);
    return -EIO;
  }
  return count;
}

ssize_t write_from_buffer(char* fname, int fd, char* buffer, uint64_t size, uint64_t base)
{
  ssize_t rc;
  uint64_t count = 0;
  char* buf = buffer;
  off_t offset = base;

  while (count < size) {
    uint64_t bytes = size - count;

    if (bytes > RW_MAX_SIZE)
      bytes = RW_MAX_SIZE;

    if (offset) {
      rc = lseek(fd, offset, SEEK_SET);
      if (rc != offset) {
        fprintf(stderr, "%s, seek off 0x%lx != 0x%lx.\n", fname, rc, offset);
        perror("seek file");
        return -EIO;
      }
    }

    /* write data to file from memory buffer */
    rc = write(fd, buf, bytes);
    if (rc != bytes) {
      fprintf(stderr, "%s, W off 0x%lx, 0x%lx != 0x%lx.\n", fname, offset, rc, bytes);
      perror("write file");
      return -EIO;
    }

    count += bytes;
    buf += bytes;
    offset += bytes;
  }

  if (count != size) {
    fprintf(stderr, "%s, R failed 0x%lx != 0x%lx.\n", fname, count, size);
    return -EIO;
  }
  return count;
}

// [End] #include "dma_utils.c" ===================================

int test_dma_enc_read(char* EncOut, EncIPConf Confparam)
{
  ssize_t rc;

  void* virt_addr;

  uint64_t size;
  uint32_t writeval;

  uint32_t Z_val;

  uint16_t max_schedule, mb, id, bg, z_j, kb, z_a;
  uint16_t z_set;
  uint32_t ctrl_data;
  uint32_t CB_num = CB_PROCESS_NUMBER;

  // this values should be given by Shane
  max_schedule = 0;
  mb = Confparam.mb;
  id = CB_num;
  bg = Confparam.BGSel - 1;
  z_set = Confparam.z_set - 1;
  z_j = Confparam.z_j;

  if (z_set == 0)
    z_a = 2;
  else if (z_set == 1)
    z_a = 3;
  else if (z_set == 2)
    z_a = 5;
  else if (z_set == 3)
    z_a = 7;
  else if (z_set == 4)
    z_a = 9;
  else if (z_set == 5)
    z_a = 11;
  else if (z_set == 6)
    z_a = 13;
  else
    z_a = 15;

  if (bg == 0)
    kb = 22;
  else if (bg == 1)
    kb = 10;
  else if (bg == 2)
    kb = 9;
  else if (bg == 3)
    kb = 8;
  else
    kb = 6;
  mb = Confparam.kb_1 + kb;
  Z_val = (unsigned int)(z_a << z_j);
  ctrl_data = (max_schedule << 30) | ((mb - kb) << 24) | (id << 19) | (bg << 6) | (z_set << 3) | z_j;
  uint32_t OutDataNUM = Z_val * mb;
  uint32_t Out_dwNumItems_p128;
  uint32_t Out_dwNumItems;

  if ((OutDataNUM & 0x7F) == 0)
    Out_dwNumItems_p128 = OutDataNUM >> 5;
  else
    Out_dwNumItems_p128 = ((OutDataNUM >> 7) + 1) << 2;
  Out_dwNumItems = ((Out_dwNumItems_p128 << 2) * CB_num);
  size = Out_dwNumItems;
  writeval = ctrl_data;

  /* calculate the virtual address to be accessed */
  virt_addr = map_base + OFFSET_ENC_OUT;

  /* swap 32-bit endianess if host is not little-endian */
  writeval = htoll(writeval);
  *((uint32_t*)virt_addr) = writeval;
  if (fd_enc_read < 0) {
    fprintf(stderr, "unable to open device %s, %d.\n", dev_enc_read, fd_enc_read);
    perror("open device");
    return -EINVAL;
  }

  /* lseek & read data from AXI MM into buffer using SGDMA */
  rc = read_to_buffer(dev_enc_read, fd_enc_read, EncOut, size, 0);
  if (rc < 0)
    goto out;

  rc = 0;

out:

  return rc;
}

int test_dma_enc_write(char* data, EncIPConf Confparam)
{
  ssize_t rc;
  void* virt_addr;

  uint64_t size;
  uint32_t writeval;

  uint32_t Z_val;
  uint16_t max_schedule, mb, id, bg, z_j, kb, z_a;
  uint16_t z_set;
  uint32_t ctrl_data;
  uint32_t CB_num = CB_PROCESS_NUMBER;

  // this values should be given by Shane
  max_schedule = 0;

  mb = Confparam.mb;
  id = CB_num;
  bg = Confparam.BGSel - 1;
  z_set = Confparam.z_set - 1;
  z_j = Confparam.z_j;

  if (z_set == 0)
    z_a = 2;
  else if (z_set == 1)
    z_a = 3;
  else if (z_set == 2)
    z_a = 5;
  else if (z_set == 3)
    z_a = 7;
  else if (z_set == 4)
    z_a = 9;
  else if (z_set == 5)
    z_a = 11;
  else if (z_set == 6)
    z_a = 13;
  else
    z_a = 15;

  if (bg == 0)
    kb = 22;
  else if (bg == 1)
    kb = 10;
  else if (bg == 2)
    kb = 9;
  else if (bg == 3)
    kb = 8;
  else
    kb = 6;
  mb = Confparam.kb_1 + kb;
  Z_val = (unsigned int)(z_a << z_j);
  ctrl_data = (max_schedule << 30) | ((mb - kb) << 24) | (id << 19) | (bg << 6) | (z_set << 3) | z_j;
  uint32_t InDataNUM = Z_val * kb;
  uint32_t In_dwNumItems_p128;
  uint32_t In_dwNumItems;

  if ((InDataNUM & 0x7F) == 0)
    In_dwNumItems_p128 = InDataNUM >> 5;
  else
    In_dwNumItems_p128 = ((InDataNUM >> 7) + 1) << 2;

  In_dwNumItems = ((In_dwNumItems_p128 << 2) * CB_num);
  size = In_dwNumItems;
  writeval = ctrl_data;

  /* calculate the virtual address to be accessed */
  virt_addr = map_base + OFFSET_ENC_IN;

  /* swap 32-bit endianess if host is not little-endian */
  writeval = htoll(writeval);
  *((uint32_t*)virt_addr) = writeval;
  if (fd_enc_write < 0) {
    fprintf(stderr, "unable to open device %s, %d.\n", dev_enc_write, fd_enc_write);
    perror("open device");
    return -EINVAL;
  }

  rc = write_from_buffer(dev_enc_write, fd_enc_write, data, size, 0);
  if (rc < 0)
    goto out;
  rc = 0;

out:

  return rc;
}

// int test_dma_dec_read(unsigned int *DecOut, DecIPConf Confparam)
int test_dma_dec_read(char* DecOut, DecIPConf Confparam)
{
  ssize_t rc;

  void* virt_addr;

  uint64_t size;
  uint32_t writeval;

  uint32_t Z_val;

  uint16_t max_schedule, mb, id, bg, z_j, kb, z_a, max_iter, sc_idx;
  uint16_t z_set;
  uint32_t ctrl_data;
  uint32_t CB_num = Confparam.CB_num;

  // this values should be given by Shane
  max_schedule = 0;
  mb = Confparam.mb;
  id = CB_num;
  bg = Confparam.BGSel - 1;
  z_set = Confparam.z_set - 1;
  z_j = Confparam.z_j;
  max_iter = 8;
  sc_idx = 12;

  if (z_set == 0)
    z_a = 2;
  else if (z_set == 1)
    z_a = 3;
  else if (z_set == 2)
    z_a = 5;
  else if (z_set == 3)
    z_a = 7;
  else if (z_set == 4)
    z_a = 9;
  else if (z_set == 5)
    z_a = 11;
  else if (z_set == 6)
    z_a = 13;
  else
    z_a = 15;

  if (bg == 0)
    kb = 22;
  else if (bg == 1)
    kb = 10;
  else if (bg == 2)
    kb = 9;
  else if (bg == 3)
    kb = 8;
  else
    kb = 6;

  Z_val = (unsigned int)(z_a << z_j);
  ctrl_data =
      (max_schedule << 30) | ((mb - kb) << 24) | (id << 19) | (max_iter << 13) | (sc_idx << 9) | (bg << 6) | (z_set) << 3 | z_j;

  uint32_t OutDataNUM = Z_val * kb;
  uint32_t Out_dwNumItems_p128;
  uint32_t Out_dwNumItems;

  if (CB_num & 0x01) // odd cb number
  {
    if ((OutDataNUM & 0xFF) == 0)
      Out_dwNumItems_p128 = OutDataNUM;
    else
      Out_dwNumItems_p128 = 256 * ((OutDataNUM / 256) + 1);

    Out_dwNumItems = (Out_dwNumItems_p128 * CB_num) >> 3;
  } else {
    if ((OutDataNUM & 0x7F) == 0)
      Out_dwNumItems_p128 = OutDataNUM;
    else
      Out_dwNumItems_p128 = 128 * ((OutDataNUM / 128) + 1);

    Out_dwNumItems = (Out_dwNumItems_p128 * CB_num) >> 3;
    if ((Out_dwNumItems & 0x1f) != 0)
      Out_dwNumItems = ((Out_dwNumItems + 31) >> 5) << 5;

  }
  size = Out_dwNumItems;
  writeval = ctrl_data;

  /* calculate the virtual address to be accessed */
  virt_addr = map_base + OFFSET_DEC_OUT;

  /* swap 32-bit endianess if host is not little-endian */
  writeval = htoll(writeval);
  *((uint32_t*)virt_addr) = writeval;

  if (fd_dec_read < 0) {
    fprintf(stderr, "unable to open device %s, %d.\n", dev_dec_read, fd_dec_read);
    perror("open device");
    return -EINVAL;
  }

  /* lseek & read data from AXI MM into buffer using SGDMA */
  rc = read_to_buffer(dev_dec_read, fd_dec_read, DecOut, size, 0);
  if (rc < 0)
    goto out;

  rc = 0;

out:

  return rc;
}

// int test_dma_dec_write(unsigned int *data, DecIPConf Confparam)
int test_dma_dec_write(char* data, DecIPConf Confparam)
{
  ssize_t rc;

  void* virt_addr;

  uint64_t size;
  uint32_t writeval;

  uint32_t Z_val;
  uint16_t max_schedule, mb, id, bg, z_j, kb, z_a, max_iter, sc_idx;
  uint16_t z_set;
  uint32_t ctrl_data;
  uint32_t CB_num = Confparam.CB_num; // CB_PROCESS_NUMBER_Dec;//

  // this values should be given by Shane
  max_schedule = 0;
  mb = Confparam.mb;
  id = CB_num;
  bg = Confparam.BGSel - 1;
  z_set = Confparam.z_set - 1;
  z_j = Confparam.z_j;

  max_iter = 8;
  sc_idx = 12;

  if (z_set == 0)
    z_a = 2;
  else if (z_set == 1)
    z_a = 3;
  else if (z_set == 2)
    z_a = 5;
  else if (z_set == 3)
    z_a = 7;
  else if (z_set == 4)
    z_a = 9;
  else if (z_set == 5)
    z_a = 11;
  else if (z_set == 6)
    z_a = 13;
  else
    z_a = 15;

  if (bg == 0)
    kb = 22;
  else if (bg == 1)
    kb = 10;
  else if (bg == 2)
    kb = 9;
  else if (bg == 3)
    kb = 8;
  else
    kb = 6;

  Z_val = (unsigned int)(z_a << z_j);
  ctrl_data =
      (max_schedule << 30) | ((mb - kb) << 24) | (id << 19) | (max_iter << 13) | (sc_idx << 9) | (bg << 6) | (z_set) << 3 | z_j;

  uint32_t InDataNUM = Z_val * mb;
  uint32_t In_dwNumItems_p128;
  uint32_t In_dwNumItems;

  InDataNUM = Z_val * mb * 8;
  if ((InDataNUM & 0x7F) == 0)
    In_dwNumItems_p128 = InDataNUM;
  else
    In_dwNumItems_p128 = 128 * ((InDataNUM / 128) + 1);

  In_dwNumItems = (In_dwNumItems_p128 * CB_num) >> 3;
  if ((In_dwNumItems & 0x1f) != 0)
    In_dwNumItems = ((In_dwNumItems + 31) >> 5) << 5;

  size = In_dwNumItems;
  writeval = ctrl_data;

  /* calculate the virtual address to be accessed */
  virt_addr = map_base + OFFSET_DEC_IN;

  /* swap 32-bit endianess if host is not little-endian */
  writeval = htoll(writeval);
  *((uint32_t*)virt_addr) = writeval;

  if (fd_dec_write < 0) {
    fprintf(stderr, "unable to open device %s, %d.\n", dev_dec_write, fd_dec_write);
    perror("open device");
    return -EINVAL;
  }

  rc = write_from_buffer(dev_dec_write, fd_dec_write, data, size, 0);
  if (rc < 0)
    goto out;

  rc = 0;

out:

  return rc;
}

void test_dma_init(devices_t devices)
{
  /* access width */
  char* device2 = devices.user_device;

  AssertFatal((fd = open(device2, O_RDWR | O_SYNC)) != -1, "CHARACTER DEVICE %s OPEN FAILURE\n", device2);
  fflush(stdout);

  /* map one page */
  map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  AssertFatal(map_base != (void*)-1, "MEMORY MAP AT ADDRESS %p FAILED\n", map_base);

  void* virt_addr;
  virt_addr = map_base + OFFSET_RESET;
  *((uint32_t*)virt_addr) = 1;

  dev_enc_write = devices.enc_write_device;
  dev_enc_read = devices.enc_read_device;
  dev_dec_write = devices.dec_write_device;
  dev_dec_read = devices.dec_read_device;

  fd_enc_write = open(dev_enc_write, O_RDWR);
  fd_enc_read = open(dev_enc_read, O_RDWR);
  fd_dec_write = open(dev_dec_write, O_RDWR);
  fd_dec_read = open(dev_dec_read, O_RDWR);

  fflush(stdout);

}

void dma_reset(devices_t devices)
{
  char* device2 = devices.user_device;

  void* virt_addr;
  virt_addr = map_base + PCIE_OFF;
  *((uint32_t*)virt_addr) = 1;

  AssertFatal(munmap(map_base, MAP_SIZE) != -1, "munmap failure");
  close(fd_enc_write);
  close(fd_enc_read);
  close(fd_dec_write);
  close(fd_dec_read);
  close(fd);

  AssertFatal((fd = open(device2, O_RDWR | O_SYNC)) != -1, "CHARACTER DEVICE %s OPEN FAILURE\n", device2);
  fflush(stdout);

  /* map one page */
  map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  AssertFatal(map_base != (void*)-1, "MEMORY MAP AT ADDRESS %p FAILED\n", map_base);

  virt_addr = map_base + PCIE_OFF;
  *((uint32_t*)virt_addr) = 1;

  virt_addr = map_base + OFFSET_RESET;
  *((uint32_t*)virt_addr) = 1;

  dev_enc_write = devices.enc_write_device;
  dev_enc_read = devices.enc_read_device;
  dev_dec_write = devices.dec_write_device;
  dev_dec_read = devices.dec_read_device;

  fd_enc_write = open(dev_enc_write, O_RDWR);
  fd_enc_read = open(dev_enc_read, O_RDWR);
  fd_dec_write = open(dev_dec_write, O_RDWR);
  fd_dec_read = open(dev_dec_read, O_RDWR);

  fflush(stdout);
}

void test_dma_shutdown()
{

  void* virt_addr;
  virt_addr = map_base + PCIE_OFF;
  *((uint32_t*)virt_addr) = 1;

  AssertFatal(munmap(map_base, MAP_SIZE) != -1, "munmap failure");
  close(fd_enc_write);
  close(fd_enc_read);
  close(fd_dec_write);
  close(fd_dec_read);
  close(fd);

}

// reg_rx.c
int nrLDPC_decoder_FPGA_PYM(uint8_t* buf_in, uint8_t* buf_out, DecIFConf dec_conf)
{
  struct timespec ts_start0; // evaluate time from input setting to output setting including xdma

  int Zc;
  int nRows;
  int baseGraph;
  int CB_num;

  DecIPConf Confparam;
  int z_a, z_tmp;
  int z_j = 0;


  int input_CBoffset, output_CBoffset;

  uint8_t i_LS;

  devices_t devices = {
    .user_device = dec_conf.user_device,
    .enc_write_device = dec_conf.enc_write_device,
    .enc_read_device = dec_conf.enc_read_device,
    .dec_write_device = dec_conf.dec_write_device,
    .dec_read_device = dec_conf.dec_read_device
  };

  static int init_flag = 0;
  if (init_flag == 0) {
    /*Init*/
    test_dma_init(devices);
    init_flag = 1;
  } else {
    dma_reset(devices);
  }

  clock_gettime(CLOCK_MONOTONIC, &ts_start0); // time start0
  // LDPC input parameter
  Zc = dec_conf.Zc; // shifting size
  nRows = dec_conf.nRows; // number of Rows
  baseGraph = dec_conf.BG; // base graph
  CB_num = dec_conf.numCB; // 31 number of code block

  // calc xdma LDPC parameter
  // calc i_LS
  if ((Zc % 15) == 0)
    i_LS = 7;
  else if ((Zc % 13) == 0)
    i_LS = 6;
  else if ((Zc % 11) == 0)
    i_LS = 5;
  else if ((Zc % 9) == 0)
    i_LS = 4;
  else if ((Zc % 7) == 0)
    i_LS = 3;
  else if ((Zc % 5) == 0)
    i_LS = 2;
  else if ((Zc % 3) == 0)
    i_LS = 1;
  else
    i_LS = 0;

  // calc z_a
  if (i_LS == 0)
    z_a = 2;
  else
    z_a = i_LS * 2 + 1;

  // calc z_j
  z_tmp = Zc / z_a;
  while (z_tmp % 2 == 0) {
    z_j = z_j + 1;
    z_tmp = z_tmp / 2;
  }

  // calc CB_num and mb
  Confparam.CB_num = CB_num;
  if (baseGraph == 1)
    Confparam.mb = 22 + nRows;
  else
    Confparam.mb = 10 + nRows;

  // set BGSel, z_set, z_j
  Confparam.BGSel = baseGraph;
  Confparam.z_set = i_LS + 1;
  Confparam.z_j = z_j;

  // Calc input CB offset
  input_CBoffset = Zc * Confparam.mb * 8;
  if ((input_CBoffset & 0x7F) == 0)
    input_CBoffset = input_CBoffset / 8;
  else
    input_CBoffset = 16 * ((input_CBoffset / 128) + 1);

  // Calc output CB offset
  output_CBoffset = Zc * (Confparam.mb - nRows);
  if ((output_CBoffset & 0x7F) == 0)
    output_CBoffset = output_CBoffset / 8;
  else
    output_CBoffset = 16 * ((output_CBoffset / 128) + 1);

  // LDPC accelerator start
  // write into accelerator
  if (test_dma_dec_write((char *)buf_in, Confparam) != 0) {
    exit(1);
    printf("write exit!!\n");
  }

  // read output of accelerator
  if (test_dma_dec_read((char *)buf_out, Confparam) != 0) {
    exit(1);
    printf("read exit!!\n");
  }

  return 0;
}

