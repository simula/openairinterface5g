#include <gtest/gtest.h>
extern "C" {
#include "openair1/PHY/TOOLS/tools_defs.h"
extern uint32_t signal_energy_nodc(const c16_t *input, uint32_t length);
}
#include <vector>
#include <algorithm>

int32_t signal_energy_nodc_ref(const c16_t *input, uint32_t length)
{
  float sum = 0;
  for (auto i = 0U; i < length; i++) {
    sum += input[i].r * input[i].r + input[i].i * input[i].i;
  }
  return sum / length;
}

TEST(signal_energy_nodc, size_6)
{
  std::vector<c16_t> input;
  input.resize(6);
  std::fill(input.begin(), input.end(), (c16_t){42, 42});
  EXPECT_EQ(signal_energy_nodc(input.data(), input.size()),
            signal_energy_nodc_ref(input.data(), input.size()));
}

int main(int argc, char **argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
