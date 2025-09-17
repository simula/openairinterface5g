#include <gtest/gtest.h>
extern "C" {
#include "openair1/PHY/TOOLS/tools_defs.h"
#include "common/utils/LOG/log.h"
}
#include <cmath>

uint8_t log2_approx_ref(uint32_t x)
{
  return std::round(std::log2(x));
}

TEST(log2_approx, complete)
{
  for (uint32_t i = 0; i < UINT32_MAX; i++)
    EXPECT_EQ(log2_approx(i), log2_approx_ref(i));
}

TEST(log2_approx, boundaries)
{
  for (int i = 0; i < 32; i++) {
    uint32_t i2 = std::pow(2.0, i + 0.5);
    EXPECT_EQ(log2_approx(i2), i) << "log2(" << i2 << ")";
    EXPECT_EQ(log2_approx(i2 + 1), i + 1) << "log2(" << i2 + 1 << ")";
  }
}

TEST(log2_approx64, boundaries)
{
  for (int i = 0; i < 64; i++) {
    unsigned long long int i2 = std::pow(2.0L, i + 0.5L);
    EXPECT_EQ(log2_approx64(i2), i) << "log2(" << i2 << ")";
    EXPECT_EQ(log2_approx64(i2 + 1), i + 1) << "log2(" << i2 + 1 << ")";
  }
}

int main(int argc, char **argv)
{
  logInit();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
