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
/*! \file nfapi/tests/p5/nr_fapi_test.h
 * \brief
 * \author Ruben S. Silva
 * \date 2024
 * \version 0.1
 * \company OpenAirInterface Software Alliance
 * \email: contact@openairinterface.org, rsilva@allbesmart.pt
 * \note
 * \warning
 */

#ifndef OPENAIRINTERFACE_NR_FAPI_TEST_H
#define OPENAIRINTERFACE_NR_FAPI_TEST_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef _STANDALONE_TESTING_
#include "common/utils/LOG/log.h"
#endif
#include "nfapi_nr_interface.h"
#include "nfapi_nr_interface_scf.h"

#define FILL_TLV(TlV, TaG, VaL) \
  do {                          \
    TlV.tl.tag = TaG;           \
    TlV.value = VaL;            \
  } while (0)

uint8_t rand8()
{
  return (rand() & 0xff);
}
uint16_t rand16()
{
  return rand8() << 8 | rand8();
}
uint32_t rand24()
{
  return rand16() << 8 | rand8();
}
uint32_t rand32()
{
  return rand24() << 8 | rand8();
}
uint64_t rand64()
{
  return (uint64_t)rand32() << 32 | rand32();
}

uint8_t rand8_range(uint8_t lower, uint8_t upper)
{
  return (rand() % (upper - lower + 1)) + lower;
}

uint16_t rand16_range(uint16_t lower, uint16_t upper)
{
  return (rand() % (upper - lower + 1)) + lower;
}

int16_t rands16_range(int16_t lower, int16_t upper)
{
  return (rand() % (upper - lower + 1)) + lower;
}

uint32_t rand32_range(uint32_t lower, uint32_t upper)
{
  return (rand() % (upper - lower + 1)) + lower;
}

int main(int n, char *v[]);

static inline void fapi_test_init_seeded(time_t seed)
{
  srand(seed);
  printf("srand seed is %ld\n", seed);
  logInit();
  set_glog(OAILOG_DISABLE);
}

static inline void fapi_test_init()
{
  fapi_test_init_seeded(time(NULL));
}
#endif // OPENAIRINTERFACE_NR_FAPI_TEST_H
