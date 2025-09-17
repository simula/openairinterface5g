#include <gtest/gtest.h>
#include <simde/x86/avx2.h>
#include <cstdint>

extern "C" {
  void oai_mm_separate_real_imag_parts(simde__m128i *out_re, simde__m128i *out_im, simde__m128i in0, simde__m128i in1);
  void oai_mm256_separate_real_imag_parts(simde__m256i *out_re, simde__m256i *out_im, simde__m256i in0, simde__m256i in1);
  void oai_mm256_separate_vectors(simde__m256i combined, simde__m128i *re, simde__m128i *im);
  void oai_mm256_combine_vectors(simde__m128i re, simde__m128i im, simde__m256i *combined);
}

TEST(separate_real_imag_parts, 128) {

  // Create two 128-bit input vectosr with 8 16-bit integers
  simde__m128i in0 = simde_mm_setr_epi16(0, 1,  2,  3,  4,  5,  6,  7);
  simde__m128i in1 = simde_mm_setr_epi16(8, 9, 10, 11, 12, 13, 14, 15);

  // Create two 128-bit output vectors
  simde__m128i out_re, out_im;

  // Call function to test
  oai_mm_separate_real_imag_parts(&out_re, &out_im, in0, in1);

  // Store the results for comparison
  int16_t re_result[8], im_result[8];
  simde_mm_storeu_si128(reinterpret_cast<simde__m128i*>(re_result), out_re);
  simde_mm_storeu_si128(reinterpret_cast<simde__m128i*>(im_result), out_im);

  // Create the expected vectors
  int16_t re_expected[8] = {0, 2, 4, 6, 8, 10, 12, 14};
  int16_t im_expected[8] = {1, 3, 5, 7, 9, 11, 13, 15};

  for (int i = 0; i < 8; i++) {
    EXPECT_EQ(re_result[i], re_expected[i]) << "Real part mismatch on index " << i;
    EXPECT_EQ(im_result[i], im_expected[i]) << "Imag part mismatch on index " << i;
  }
}

TEST(separate_real_imag_parts, 256) {

  // Create two 256-bit input vectosr with 16 16-bit integers
  simde__m256i in0 = simde_mm256_setr_epi16( 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15);
  simde__m256i in1 = simde_mm256_setr_epi16(16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31);

  // Create two 256-bit output vectors
  simde__m256i out_re, out_im;

  // Call function to test
  oai_mm256_separate_real_imag_parts(&out_re, &out_im, in0, in1);

  // Store the results for comparison
  int16_t re_result[16], im_result[16];
  simde_mm256_storeu_si256(reinterpret_cast<simde__m256i*>(re_result), out_re);
  simde_mm256_storeu_si256(reinterpret_cast<simde__m256i*>(im_result), out_im);

  // Create the expected vectors
  int16_t re_expected[16] = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30};
  int16_t im_expected[16] = {1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31};

  for (int i = 0; i < 16; i++) {
    EXPECT_EQ(re_result[i], re_expected[i]) << "Real part mismatch on index " << i;
    EXPECT_EQ(im_result[i], im_expected[i]) << "Imag part mismatch on index " << i;
  }
}

TEST(complex_vectors, separate) {
    
    // Create a 256-bit input vector with 16 16-bit integers in natural order.
    simde__m256i combined = simde_mm256_setr_epi16(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);

    // Create two 128-bit output vectors
    simde__m128i re, im;

    // Call function to test
    oai_mm256_separate_vectors(combined, &re, &im);

    // Store the results for comparison
    int16_t re_result[8], im_result[8];
    simde_mm_storeu_si128((simde__m128i*)re_result, re);
    simde_mm_storeu_si128((simde__m128i*)im_result, im);

    // Create the expected vectors
    int16_t re_expected[8] = {0, 2, 4, 6, 8, 10, 12, 14};
    int16_t im_expected[8] = {1, 3, 5, 7, 9, 11, 13, 15};

    for (int i = 0; i < 8; i++) {
        EXPECT_EQ(re_result[i], re_expected[i]) << "Real part mismatch on index " << i;
        EXPECT_EQ(im_result[i], im_expected[i]) << "Imag part mismatch on index " << i;
    }
}

TEST(complex_vectors, combine) {
    
    // Create two 128-bit input vectosr with 8 16-bit integers
    simde__m128i re = simde_mm_setr_epi16(0, 2, 4, 6, 8, 10, 12, 14);
    simde__m128i im = simde_mm_setr_epi16(1, 3, 5, 7, 9, 11, 13, 15);

    // Create 256-bit output vector
    simde__m256i c;

    // Call function to test
    oai_mm256_combine_vectors(re, im, &c);

    // Store the results for comparison
    int16_t c_result[16];
    simde_mm256_storeu_si256((simde__m256i*)c_result, c);

    // Create the expected vector
    int16_t c_expected[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

    for (int i = 0; i < 8; i++) {
        EXPECT_EQ(c_result[2*i+0], c_expected[2*i+0]) << "Real part mismatch on index " << 2*i+0;
        EXPECT_EQ(c_result[2*i+1], c_expected[2*i+1]) << "Imag part mismatch on index " << 2*i+1;
    }
}

int main(int argc, char **argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
