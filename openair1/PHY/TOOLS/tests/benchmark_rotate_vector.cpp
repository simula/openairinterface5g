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

#include <stdint.h>
#include <vector>
#include <algorithm>
#include <numeric>
extern "C" {
#include "openair1/PHY/TOOLS/tools_defs.h"
struct configmodule_interface_s;
struct configmodule_interface_s *uniqCfg = NULL;
void exit_function(const char *file, const char *function, const int line, const char *s, const int assert)
{
  if (assert) {
    abort();
  } else {
    exit(EXIT_SUCCESS);
  }
}
}
#include <cstdio>
#include "common/utils/LOG/log.h"
#include "benchmark/benchmark.h"
#include "openair1/PHY/TOOLS/phy_test_tools.hpp"

static void BM_rotate_cpx_vector(benchmark::State &state)
{
  int vector_size = state.range(0);
  auto input_complex_16 = generate_random_c16(vector_size);
  auto input_alpha = generate_random_c16(vector_size);
  AlignedVector512<c16_t> output;
  output.resize(vector_size);
  int shift = 2;
  for (auto _ : state) {
    rotate_cpx_vector(input_complex_16.data(), input_alpha.data(), output.data(), vector_size, shift);
  }
}

BENCHMARK(BM_rotate_cpx_vector)->RangeMultiplier(4)->Range(100, 20000);

BENCHMARK_MAIN();
