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

// Initial unit test for nr_layer_precoder_simd_[cm, simd] functions
// Issue: gNB: SIMD Precoding Bug in nr_layer_precoder_simd for PMI = 4
// https://gitlab.eurecom.fr/oai/openairinterface5g/-/issues/955

#include "gtest/gtest.h"
#include <cstdint>
#include <simde/x86/avx512.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "openair1/PHY/TOOLS/tools_defs.h" // c16_t
#include "nfapi/open-nFAPI/nfapi/public_inc/nfapi_nr_interface_scf.h" // nfapi_nr_pm_pdu_t
#include "nfapi/open-nFAPI/nfapi/public_inc/fapi_nr_ue_interface.h" // fapi_nr_dl_config_dci_dl_pdu_rel15_t

//#include "openair1/PHY/MODULATION/nr_modulation.h" // nr_layer_precoder_simd_[cm, simd]

// Forward declare the function but hide the Variable-Length Array (VLA) feature from C not available in C++
c16_t nr_layer_precoder_cm(int n_layers,
                           int symbol_size,
                           c16_t (*dataF_in)[/*symbol_size*/], // c16_t dataF_in[n_layers][symbol_size]
                           int ant,
                           nfapi_nr_pm_pdu_t *pmi_pdu,
                           int offset);

// Forward declare the function but hide the Variable-Length Array (VLA) feature from C not available in C++
void nr_layer_precoder_simd(int n_layers,
                            int symbol_size,
                            const c16_t (*dataF_in)[/*symbol_size*/], // c16_t dataF_in[n_layers][symbol_size]
                            int ant,
                            const nfapi_nr_pm_pdu_t *pmi_pdu,
                            int offset,
                            int re_cnt,
                            c16_t *dataF_out);

#ifdef __cplusplus
}
#endif

TEST(NrLayerPrecoderTest, Basic)
{
  constexpr int n_layers = 2;
  constexpr int symbol_size = 24;
  constexpr int n_ants = 2;
  constexpr int ant = 1;
  constexpr int re_cnt = 24;

  // Initialize the 2D input data buffer
  std::vector<c16_t> buffer_in(n_layers * symbol_size);
  std::vector<c16_t> buffer_out(n_layers * symbol_size);

  for (int i = 0; i < n_layers * symbol_size; ++i) {
    buffer_in[i] = {static_cast<int16_t>(i + 1), static_cast<int16_t>(-i - 1)};
  }
  for (int i = 0; i < n_ants * symbol_size; ++i) {
    buffer_out[i] = {static_cast<int16_t>(0), static_cast<int16_t>(0)};
  }

  // Cast flat buffer to the required 2D Variable-Length Array (VLA) style pointer
  c16_t(*dataF_in)[symbol_size] = reinterpret_cast<c16_t(*)[symbol_size]>(buffer_in.data());
  c16_t(*dataF_out)[symbol_size] = reinterpret_cast<c16_t(*)[symbol_size]>(buffer_out.data());

  // Create and populate the pmi_pdu structure
  nfapi_nr_pm_pdu_t pmi_pdu = {0};
  for (int layer = 0; layer < n_layers; ++layer) {
    pmi_pdu.weights[layer][ant].precoder_weight_Re = -SHRT_MAX; // Q15 representation of (~-1.0) -0.999969482421875 (-1+2^-15)
    pmi_pdu.weights[layer][ant].precoder_weight_Im = 0;
  }

  // Call the C function
  for (int symbol = 0; symbol < re_cnt; symbol++)
    dataF_out[ant][symbol] = nr_layer_precoder_cm(n_layers, symbol_size, dataF_in, ant, &pmi_pdu, symbol);

  // You can calculate the expected value manually or test rough correctness
  for (int symbol = 0; symbol < re_cnt; symbol++) {
    int result_real = -26 - (symbol<<1);
    int result_imag = -24 - (symbol<<1);

    EXPECT_EQ(dataF_out[ant][symbol].r, result_real)
        << " at [" << ant << "][" << symbol << "] got real part: " << dataF_out[ant][symbol].r;
    EXPECT_EQ(dataF_out[ant][symbol].i, -result_imag)
        << " at [" << ant << "][" << symbol << "] got imag part: " << dataF_out[ant][symbol].i;
  }
}

TEST(NrLayerPrecoderTest, SIMD)
{
  constexpr int n_layers = 2;
  constexpr int symbol_size = 24;
  constexpr int n_ants = 2;
  constexpr int ant = 1;
  constexpr int re_cnt = 24;

  // Initialize the 2D input data buffer
  std::vector<c16_t> buffer_in(n_layers * symbol_size);
  std::vector<c16_t> buffer_out(n_layers * symbol_size);

  for (int i = 0; i < n_layers * symbol_size; ++i) {
    buffer_in[i] = {static_cast<int16_t>(i + 1), static_cast<int16_t>(-i - 1)};
  }
  for (int i = 0; i < n_ants * symbol_size; ++i) {
    buffer_out[i] = {static_cast<int16_t>(0), static_cast<int16_t>(0)};
  }

  // Cast flat buffer to the required 2D Variable-Length Array (VLA) style pointer
  c16_t(*dataF_in)[symbol_size] = reinterpret_cast<c16_t(*)[symbol_size]>(buffer_in.data());
  c16_t(*dataF_out)[symbol_size] = reinterpret_cast<c16_t(*)[symbol_size]>(buffer_out.data());

  // Create and populate the pmi_pdu structure
  nfapi_nr_pm_pdu_t pmi_pdu = {0};
  for (int layer = 0; layer < n_layers; ++layer) {
    pmi_pdu.weights[layer][ant].precoder_weight_Re = -SHRT_MAX; // Q15 representation of (~-1.0) -0.999969482421875 (-1+2^-15)
    pmi_pdu.weights[layer][ant].precoder_weight_Im = 0;
  }

  // Call the C function
  nr_layer_precoder_simd(n_layers, symbol_size, dataF_in, ant, &pmi_pdu, 0, re_cnt, dataF_out[ant]);

  // You can calculate the expected value manually or test rough correctness
  for (int symbol = 0; symbol < re_cnt; symbol++) {
    int result_real = -26 - (symbol<<1);
    int result_imag = -24 - (symbol<<1);

    EXPECT_EQ(dataF_out[ant][symbol].r, result_real)
        << " at [" << ant << "][" << symbol << "] got real part: " << dataF_out[ant][symbol].r;
    EXPECT_EQ(dataF_out[ant][symbol].i, -result_imag)
        << " at [" << ant << "][" << symbol << "] got imag part: " << dataF_out[ant][symbol].i;
  }
}

TEST(NrLayerPrecoderTest, Compare_CM_SIMD)
{
  constexpr int n_layers = 1;
  constexpr int symbol_size = 24;
  constexpr int n_ants = 2;
  constexpr int re_cnt = 24;

  // Initialize the 2D input data buffer
  std::vector<c16_t> buffer_in(n_layers * symbol_size);
  std::vector<c16_t> buffer_out_cm(n_ants * symbol_size);
  std::vector<c16_t> buffer_out_simd(n_ants * symbol_size);

  for (int i = 0; i < n_layers * symbol_size; ++i) {
	buffer_in[i] = {static_cast<int16_t>((rand() % (2 * SHRT_MAX + 1)) - SHRT_MAX), static_cast<int16_t>((rand() % (2 * SHRT_MAX + 1)) - SHRT_MAX)};
  }
  for (int i = 0; i < n_ants * symbol_size; ++i) {
    buffer_out_cm[i] = {static_cast<int16_t>(0), static_cast<int16_t>(0)};
    buffer_out_simd[i] = {static_cast<int16_t>(0), static_cast<int16_t>(0)};
  }

  // Cast flat buffer to the required 2D Variable-Length Array (VLA) style pointer
  c16_t(*dataF_in)[symbol_size] = reinterpret_cast<c16_t(*)[symbol_size]>(buffer_in.data());
  c16_t(*dataF_out_cm)[symbol_size] = reinterpret_cast<c16_t(*)[symbol_size]>(buffer_out_cm.data());
  c16_t(*dataF_out_simd)[symbol_size] = reinterpret_cast<c16_t(*)[symbol_size]>(buffer_out_simd.data());

  // Create and populate the pmi_pdu structure
  nfapi_nr_pm_pdu_t pmi_pdu = {0};
  pmi_pdu.pm_idx = 4;
  for (int layer = 0; layer < n_layers; ++layer) {
    // Could not use convert_precoder_weight() as complex.h could not be used in googletest
    // Use the logic in convert_precoder_weight()
    // precoder [−1,−j]
    pmi_pdu.weights[layer][0].precoder_weight_Re = -SHRT_MAX; // Q15 representation of (~-1.0) -0.999969482421875 (-1+2^-15)
    pmi_pdu.weights[layer][0].precoder_weight_Im = 0x0000;    // Q15 representation of 0
    pmi_pdu.weights[layer][1].precoder_weight_Re = 0x0000;    // Q15 representation of 0
    pmi_pdu.weights[layer][1].precoder_weight_Im = -SHRT_MAX; // Q15 representation of (~-1.0) -0.999969482421875 (-1+2^-15)
  }

  // Get the results for all the antenna 
  for (int ant = 0; ant < n_ants; ant++) {

    // Call the C function
    for (int symbol = 0; symbol < re_cnt; symbol++)
      dataF_out_cm[ant][symbol] = nr_layer_precoder_cm(n_layers, symbol_size, dataF_in, ant, &pmi_pdu, symbol);

    // Call the C function
    nr_layer_precoder_simd(n_layers, symbol_size, dataF_in, ant, &pmi_pdu, 0, re_cnt, dataF_out_simd[ant]);

    // Compare the result from both C function
    for (int symbol = 0; symbol < re_cnt; symbol++) {
      EXPECT_EQ(dataF_out_cm[ant][symbol].r, dataF_out_simd[ant][symbol].r)
          << " at [" << ant << "][" << symbol << "] got real part: " << dataF_out_cm[ant][symbol].r << " result " << dataF_out_simd[ant][symbol].r;
      EXPECT_EQ(dataF_out_cm[ant][symbol].i, dataF_out_simd[ant][symbol].i)
          << " at [" << ant << "][" << symbol << "] got imag part: " << dataF_out_cm[ant][symbol].i << " result " << dataF_out_simd[ant][symbol].i;
    }
  }
}

int main(int argc, char **argv)
{
  // Initialize random seed
  srand(time(0));

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
