#include <gtest/gtest.h>
#include <simde/x86/avx2.h>
#include "../../sse_intrin.h"

TEST(conj_vectors, 128) {
    
    // Create a 128-bit input vector with 8 16-bit integers in natural order.
    simde__m128i in = simde_mm_setr_epi16(0, 1, 2, 3, 4, 5, 6, 7);

    // Call function to test
    simde__m128i out = oai_mm_conj(in);

    // Store the results for comparison
    int16_t out_result[8];
    simde_mm_storeu_si128((simde__m128i*)out_result, out);

    // Create the expected vectors
    int16_t out_expected[8] = {0, -1, 2, -3, 4, -5, 6, -7};

    // Check for mismatch
    for (int i = 0; i < 8; i++) {
        EXPECT_EQ(out_result[i], out_expected[i]) << "Conjugate mismatch on index " << i;
    }
}

TEST(conj_vectors, 256) {
    
    // Create a 256-bit input vector with 16 16-bit integers in natural order.
    simde__m256i in = simde_mm256_setr_epi16(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);

    // Call function to test
    simde__m256i out = oai_mm256_conj(in);

    // Store the results for comparison
    int16_t out_result[16];
    simde_mm256_storeu_si256((simde__m256i*)out_result, out);

    // Create the expected vectors
    int16_t out_expected[16] = {0, -1, 2, -3, 4, -5, 6, -7, 8, -9, 10, -11, 12, -13, 14, -15};

    // Check for mismatch
    for (int i = 0; i < 16; i++) {
        EXPECT_EQ(out_result[i], out_expected[i]) << "Conjugate mismatch on index " << i;
    }
}

TEST(swap_vectors, 128) {
    
    // Create a 128-bit input vector with 8 16-bit integers in natural order.
    simde__m128i in = simde_mm_setr_epi16(0, 1, 2, 3, 4, 5, 6, 7);

    // Call function to test
    simde__m128i out = oai_mm_swap(in);

    // Store the results for comparison
    int16_t out_result[8];
    simde_mm_storeu_si128((simde__m128i*)out_result, out);

    // Create the expected vectors
    int16_t out_expected[8] = {1, 0, 3, 2, 5, 4, 7, 6};
    
    // Check for mismatch
    for (int i = 0; i < 8; i++) {
        EXPECT_EQ(out_result[i], out_expected[i]) << "IQ swap mismatch on index " << i;
    }
}

TEST(swap_vectors, 256) {
    
    // Create a 256-bit input vector with 16 16-bit integers in natural order.
    simde__m256i in = simde_mm256_setr_epi16(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);

    // Call function to test
    simde__m256i out = oai_mm256_swap(in);

    // Store the results for comparison
    int16_t out_result[16];
    simde_mm256_storeu_si256((simde__m256i*)out_result, out);

    // Create the expected vector
    int16_t out_expected[16] = {1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14};
    
    // Check for mismatch
    for (int i = 0; i < 16; i++) {
        EXPECT_EQ(out_result[i], out_expected[i]) << "Conjugate mismatch on index " << i;
    }
}

TEST(ShiftedMultiplyAndAdd, 128) {
  
  // Set up two 128-bit vectors with 8 16-bit elements each.
  simde__m128i vec1 = simde_mm_setr_epi16( 1,  2,  3,  4, -5, -6,  7,  8);
  simde__m128i vec2 = simde_mm_setr_epi16(10, 20, 30, 40, 50, 60, 70, 80);

  // Call function to test
  int shift = 1;
  simde__m128i res = oai_mm_smadd(vec1, vec2, shift);

  // Store the results for comparison
  int32_t output[4];
  simde_mm_storeu_si128((simde__m128i*)output, res);

  // Create the expected vector
  // Pair 0:  1*10 +  2*20 =   10 +   40 =   50>>1 =   25
  // Pair 1:  3*30 +  4*40 =   90 +  160 =  250>>1 =  125
  // Pair 2: -5*50 + -6*60 = -250 + -360 = -610>>1 = -305
  // Pair 3:  7*70 +  8*80 =  490 +  640 = 1130>>1 =  565
  int32_t expected[4] = {25, 125, -305, 565};
  
  // Check for mismatch
  for (int i = 0; i < 4; i++) {
    EXPECT_EQ(output[i], expected[i]) << "Mismatch at index " << i;
  }
}

TEST(ShiftedMultiplyAndAdd, 256) {

  // Set up two 128-bit vectors with 16 16-bit elements each.
  simde__m256i vec1 = simde_mm256_setr_epi16( 1,  2,  3,  4,  5,  6, -7, -8,  9,  10,  11,  12,  13,  14,  15,  16);
  simde__m256i vec2 = simde_mm256_setr_epi16(10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160);

  // Call function to test
  int shift = 1;
  simde__m256i res = oai_mm256_smadd(vec1, vec2, shift);

  // Store the results for comparison
  int32_t output[8];
  simde_mm256_storeu_si256((simde__m256i*)output, res);

  // Create the expected vector
  // Pair 0:  1* 10 +  2* 20 =    50>>1 =   25
  // Pair 1:  3* 30 +  4* 40 =   250>>1 =  125
  // Pair 2:  5* 50 +  6* 60 =   610>>1 =  305
  // Pair 3: -7* 70 + -8* 80 = -1130>>1 = -565
  // Pair 4:  9* 90 + 10*100 =  1810>>1 =  905
  // Pair 5: 11*110 + 12*120 =  2650>>1 = 1325
  // Pair 6: 13*130 + 14*140 =  3650>>1 = 1825
  // Pair 7: 15*150 + 16*160 =  4810>>1 = 2405
  int32_t expected[8] = {25, 125, 305, -565, 905, 1325, 1825, 2405};

  // Check for mismatch
  for (int i = 0; i < 8; i++) {
    EXPECT_EQ(output[i], expected[i]) << "Mismatch at index " << i;
  }
}

TEST(Pack, 128normal) {

  // Set up two 128-bit vectors containing 4 32-bit integers each.
  simde__m128i a = simde_mm_setr_epi32(1000, 2000, 3000, 4000);
  simde__m128i b = simde_mm_setr_epi32(5000, 6000, 7000, 8000);

  // Call function to test
  simde__m128i res = oai_mm_pack(a, b);

  // Store the results for comparison
  int16_t result[8];
  simde_mm_storeu_si128((simde__m128i*) result, res);

  // Create the expected vector (here without saturation)
  int16_t expected[8] = {1000, 5000, 2000, 6000, 3000, 7000, 4000, 8000};

  for (int i = 0; i < 8; i++) {
    EXPECT_EQ(result[i], expected[i]) << "Mismatch at index " << i;
  }
}

TEST(Pack, 128saturation) {
  
  // Set up two 128-bit vectors containing 4 32-bit integers each.
  simde__m128i a = simde_mm_setr_epi32(-40000, 40000, -1000, 1000);
  simde__m128i b = simde_mm_setr_epi32(-50000, 50000, -2000, 2000);

  // Call function to test
  simde__m128i res = oai_mm_pack(a, b);

  // Store the results for comparison
  int16_t result[8];
  simde_mm_storeu_si128((simde__m128i*) result, res);

  // Create the expected vector (here saturation occours)
  int16_t expected[8] = {
    -32768, -32768, 32767, 32767,  // saturation applied to the first unpacked vector
    -1000,  -2000,   1000,  2000   // values remain unchanged for the second unpacked vector
  };

  for (int i = 0; i < 8; i++) {
    EXPECT_EQ(result[i], expected[i]) << "Mismatch at index " << i;
  }
}

TEST(Pack, 256normal) {
  
  // Set up two 256-bit vectors with 16 16-bit values each.
  simde__m256i vec1 = simde_mm256_setr_epi16( 1,  2,  3,  4,  5,  6, -7, -8,  9,  10,  11,  12,  13,  14,  15,  16);
  simde__m256i vec2 = simde_mm256_setr_epi16(10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160);
  
  // Call function to test with arithmetic shift by 1 (i.e. divide by 2).
  int shift = 1;
  simde__m256i res = oai_mm256_smadd(vec1, vec2, shift);

  // Store the results for comparison
  int32_t output[8];
  simde_mm256_storeu_si256((simde__m256i*)output, res);

  // The expected results are calculated as follows:
  // Pair 0:  1* 10 +  2* 20 =    50>>1 =   25
  // Pair 1:  3* 30 +  4* 40 =   250>>1 =  125
  // Pair 2:  5* 50 +  6* 60 =   610>>1 =  305
  // Pair 3: -7* 70 + -8* 80 = -1130>>1 = -565
  // Pair 4:  9* 90 + 10*100 =  1810>>1 =  905
  // Pair 5: 11*110 + 12*120 =  2650>>1 = 1325
  // Pair 6: 13*130 + 14*140 =  3650>>1 = 1825
  // Pair 7: 15*150 + 16*160 =  4810>>1 = 2405
  int32_t expected[8] = {25, 125, 305, -565, 905, 1325, 1825, 2405};

  // Check for mismatch
  for (int i = 0; i < 8; i++) {
    EXPECT_EQ(output[i], expected[i]) << "Mismatch at index " << i;
  }
}

TEST(Pack, 256saturation) {
  
  // Set up two 256-bit vectors with 16 16-bit values each.
  simde__m256i vec1 = simde_mm256_setr_epi16(-1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -11, -12, -13, -14, -15, -16);
  simde__m256i vec2 = simde_mm256_setr_epi16(10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160);

  // Call function to test with arithmetic shift by 1 (i.e. divide by 2).
  int shift = 1;
  simde__m256i res = oai_mm256_smadd(vec1, vec2, shift);

  // Store the results for comparison
  int32_t output[8];
  simde_mm256_storeu_si256((simde__m256i*)output, res);

  // The expected results are calculated as follows:
  // Pair 0:  -1* 10 +  -2* 20 =   -50>>1 =   -25
  // Pair 1:  -3* 30 +  -4* 40 =  -250>>1 =  -125
  // Pair 2:  -5* 50 +  -6* 60 =  -610>>1 =  -305
  // Pair 3:  -7* 70 +  -8* 80 = -1130>>1 =  -565
  // Pair 4:  -9* 90 + -10*100 = -1810>>1 =  -905
  // Pair 5: -11*110 + -12*120 = -2650>>1 = -1325
  // Pair 6: -13*130 + -14*140 = -3650>>1 = -1825
  // Pair 7: -15*150 + -16*160 = -4810>>1 = -2405
  int32_t expected[8] = {-25, -125, -305, -565, -905, -1325, -1825, -2405};
  
  // Check for mismatch
  for (int i = 0; i < 8; i++) {
    EXPECT_EQ(output[i], expected[i]) << "Mismatch at index " << i;
  }
}

TEST(ComplexMultiply, 128) {
  
  // Set up two 128-bit vectors with 8 16-bit elements each.
  simde__m128i z1 = simde_mm_setr_epi16( 3,  4,  5,  6,  7,  8,  9, 10);
  simde__m128i z2 = simde_mm_setr_epi16(11, 12, 13, 14, 15, 16, 17, 18);
  
  // Call function to test with arithmetic shift by 1 (i.e. divide by 2).
  int shift = 1;
  simde__m128i result = oai_mm_cpx_mult(z1, z2, shift);
  
  // Store the results for comparison
  int16_t output[8];
  simde_mm_storeu_si128((simde__m128i*)output, result);

  // Expected results calculated manually:
  // Pair 0: (3 +  4i) * (11 + 12i) = (-15 +  80i)>>1 =  -8 +  40i
  // Pair 1: (5 +  6i) * (13 + 14i) = (-19 + 148i)>>1 = -10 +  74i
  // Pair 2: (7 +  8i) * (15 + 16i) = (-23 + 232i)>>1 = -12 + 116i
  // Pair 3: (9 + 10i) * (17 + 18i) = (-27 + 332i)>>1 = -14 + 166i
  int16_t expected[8] = { -8, 40, -10, 74, -12, 116, -14, 166 };

  // Check for mismatch
  for (int i = 0; i < 8; i++) {
    EXPECT_EQ(output[i], expected[i]) << "Mismatch at index " << i;
  }
}

TEST(ComplexMultiply, 256) {
  
  // Set up two 256-bit vectors with 16 16-bit elements each.
  simde__m256i z1 = simde_mm256_setr_epi16( 3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18);
  simde__m256i z2 = simde_mm256_setr_epi16(19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34);
  
  // Call function to test with arithmetic shift by 1 (i.e. divide by 2).
  int shift = 1;
  simde__m256i result = oai_mm256_cpx_mult(z1, z2, shift);

  // Store the results for comparison
  int16_t output[16];
  simde_mm256_storeu_si256((simde__m256i*)output, result);

  // Expected results calculated manually:
  // Pair 0: ( 3 +  4i) * (19 + 20i) = (-23 +  136i)>>1 = -12 +  68i
  // Pair 1: ( 5 +  6i) * (21 + 22i) = (-27 +  236i)>>1 = -14 + 118i
  // Pair 2: ( 7 +  8i) * (23 + 24i) = (-31 +  352i)>>1 = -16 + 176i
  // Pair 3: ( 9 + 10i) * (25 + 26i) = (-35 +  484i)>>1 = -18 + 242i
  // Pair 4: (11 + 12i) * (27 + 28i) = (-39 +  632i)>>1 = -20 + 316i
  // Pair 5: (13 + 14i) * (29 + 30i) = (-43 +  796i)>>1 = -22 + 398i
  // Pair 6: (15 + 16i) * (31 + 32i) = (-47 +  976i)>>1 = -24 + 488i
  // Pair 7: (17 + 18i) * (33 + 34i) = (-51 + 1172i)>>1 = -26 + 586i
  int16_t expected[16] = {-12, 68, -14, 118, -16, 176, -18, 242, -20, 316, -22, 398, -24, 488, -26, 586};
  
  // Check for mismatch
  for (int i = 0; i < 16; i++) {
    EXPECT_EQ(output[i], expected[i]) << "Mismatch at index " << i;
  }
}

TEST(ComplexMultiplyConjugatedInput, 128) {

  // Set up two 128-bit vectors with 8 16-bit elements each.
  simde__m128i a = simde_mm_setr_epi16( 3,  4,  5,  6,  7,  8,  9, 10);
  simde__m128i b = simde_mm_setr_epi16(11, 12, 13, 14, 15, 16, 17, 18);

  // Call function to test with arithmetic shift by 1 (i.e. divide by 2).
  int shift = 1;
  simde__m128i result = oai_mm_cpx_mult_conj(a, b, shift);
  
  // Store the results for comparison
  int16_t output[8];
  simde_mm_storeu_si128((simde__m128i*)output, result);

  // Expected results calculated manually:
  // Pair 0: ( 3 -  4i) * (11 + 12i) = ( 81 - 8i)>>1 =  40 - 4i
  // Pair 1: ( 5 -  6i) * (13 + 14i) = (149 - 8i)>>1 =  74 - 4i
  // Pair 2: ( 7 -  8i) * (15 + 16i) = (233 - 8i)>>1 = 116 - 4i
  // Pair 3: ( 9 - 10i) * (17 + 18i) = (333 - 8i)>>1 = 166 - 4i
  int16_t expected[8] = {40, -4, 74, -4, 116, -4, 166, -4};

  // Check for mismatch
  for (int i = 0; i < 8; i++) {
    EXPECT_EQ(output[i], expected[i]) << "Mismatch at index " << i;
  }
}

TEST(ComplexMultiplyConjugatedInput, 256) {
  
  // Set up two 256-bit vectors with 16 16-bit elements each.
  simde__m256i a = simde_mm256_setr_epi16( 3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18);
  simde__m256i b = simde_mm256_setr_epi16(19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34);
  
  // Call function to test with arithmetic shift by 1 (i.e. divide by 2).
  int shift = 1;
  simde__m256i result = oai_mm256_cpx_mult_conj(a, b, shift);
  
  // Store the results for comparison
  int16_t output[16];
  simde_mm256_storeu_si256((simde__m256i*)output, result);

  // Expected results calculated manually:
  // Pair 0: ( 3 -  4i) * (19 + 20i) = ( 137 - 16i)>>1 =  68 - 8i
  // Pair 1: ( 5 -  6i) * (21 + 22i) = ( 237 - 16i)>>1 = 118 - 8i
  // Pair 2: ( 7 -  8i) * (23 + 24i) = ( 353 - 16i)>>1 = 176 - 8i
  // Pair 3: ( 9 - 10i) * (25 + 26i) = ( 485 - 16i)>>1 = 242 - 8i
  // Pair 4: (11 - 12i) * (27 + 28i) = ( 633 - 16i)>>1 = 316 - 8i
  // Pair 5: (13 - 14i) * (29 + 30i) = ( 797 - 16i)>>1 = 398 - 8i
  // Pair 6: (15 - 16i) * (31 + 32i) = ( 977 - 16i)>>1 = 488 - 8i
  // Pair 7: (17 - 18i) * (33 + 34i) = (1173 - 16i)>>1 = 586 - 8i
  int16_t expected[16] = {68, -8, 118, -8, 176, -8, 242, -8, 316, -8, 398, -8, 488, -8, 586, -8};

  // Check for mismatch
  for (int i = 0; i < 16; i++) {
    EXPECT_EQ(output[i], expected[i]) << "Mismatch at index " << i;
  }
}

int main(int argc, char **argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
