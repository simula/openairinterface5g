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

/* \file config_ue.c
 * \brief common utility functions for NR (gNB and UE)
 * \author R. Knopp,
 * \date 2019
 * \version 0.1
 * \company Eurecom
 * \email: knopp@eurecom.fr
 * \note
 * \warning
 */

#include <stdint.h>
#include "assertions.h"
#include "common/utils/assertions.h"
#include "nr_common.h"
#include <complex.h>

#define C_SRS_NUMBER (64)
#define B_SRS_NUMBER (4)

/* TS 38.211 Table 6.4.1.4.3-1: SRS bandwidth configuration */
static const unsigned short srs_bandwidth_config[C_SRS_NUMBER][B_SRS_NUMBER][2] = {
    /*           B_SRS = 0    B_SRS = 1   B_SRS = 2    B_SRS = 3     */
    /* C SRS   m_srs0  N_0  m_srs1 N_1  m_srs2   N_2 m_srs3   N_3    */
    /* 0  */ {{4, 1}, {4, 1}, {4, 1}, {4, 1}},
    /* 1  */ {{8, 1}, {4, 2}, {4, 1}, {4, 1}},
    /* 2  */ {{12, 1}, {4, 3}, {4, 1}, {4, 1}},
    /* 3  */ {{16, 1}, {4, 4}, {4, 1}, {4, 1}},
    /* 4  */ {{16, 1}, {8, 2}, {4, 2}, {4, 1}},
    /* 5  */ {{20, 1}, {4, 5}, {4, 1}, {4, 1}},
    /* 6  */ {{24, 1}, {4, 6}, {4, 1}, {4, 1}},
    /* 7  */ {{24, 1}, {12, 2}, {4, 3}, {4, 1}},
    /* 8  */ {{28, 1}, {4, 7}, {4, 1}, {4, 1}},
    /* 9  */ {{32, 1}, {16, 2}, {8, 2}, {4, 2}},
    /* 10 */ {{36, 1}, {12, 3}, {4, 3}, {4, 1}},
    /* 11 */ {{40, 1}, {20, 2}, {4, 5}, {4, 1}},
    /* 12 */ {{48, 1}, {16, 3}, {8, 2}, {4, 2}},
    /* 13 */ {{48, 1}, {24, 2}, {12, 2}, {4, 3}},
    /* 14 */ {{52, 1}, {4, 13}, {4, 1}, {4, 1}},
    /* 15 */ {{56, 1}, {28, 2}, {4, 7}, {4, 1}},
    /* 16 */ {{60, 1}, {20, 3}, {4, 5}, {4, 1}},
    /* 17 */ {{64, 1}, {32, 2}, {16, 2}, {4, 4}},
    /* 18 */ {{72, 1}, {24, 3}, {12, 2}, {4, 3}},
    /* 19 */ {{72, 1}, {36, 2}, {12, 3}, {4, 3}},
    /* 20 */ {{76, 1}, {4, 19}, {4, 1}, {4, 1}},
    /* 21 */ {{80, 1}, {40, 2}, {20, 2}, {4, 5}},
    /* 22 */ {{88, 1}, {44, 2}, {4, 11}, {4, 1}},
    /* 23 */ {{96, 1}, {32, 3}, {16, 2}, {4, 4}},
    /* 24 */ {{96, 1}, {48, 2}, {24, 2}, {4, 6}},
    /* 25 */ {{104, 1}, {52, 2}, {4, 13}, {4, 1}},
    /* 26 */ {{112, 1}, {56, 2}, {28, 2}, {4, 7}},
    /* 27 */ {{120, 1}, {60, 2}, {20, 3}, {4, 5}},
    /* 28 */ {{120, 1}, {40, 3}, {8, 5}, {4, 2}},
    /* 29 */ {{120, 1}, {24, 5}, {12, 2}, {4, 3}},
    /* 30 */ {{128, 1}, {64, 2}, {32, 2}, {4, 8}},
    /* 31 */ {{128, 1}, {64, 2}, {16, 4}, {4, 4}},
    /* 32 */ {{128, 1}, {16, 8}, {8, 2}, {4, 2}},
    /* 33 */ {{132, 1}, {44, 3}, {4, 11}, {4, 1}},
    /* 34 */ {{136, 1}, {68, 2}, {4, 17}, {4, 1}},
    /* 35 */ {{144, 1}, {72, 2}, {36, 2}, {4, 9}},
    /* 36 */ {{144, 1}, {48, 3}, {24, 2}, {12, 2}},
    /* 37 */ {{144, 1}, {48, 3}, {16, 3}, {4, 4}},
    /* 38 */ {{144, 1}, {16, 9}, {8, 2}, {4, 2}},
    /* 39 */ {{152, 1}, {76, 2}, {4, 19}, {4, 1}},
    /* 40 */ {{160, 1}, {80, 2}, {40, 2}, {4, 10}},
    /* 41 */ {{160, 1}, {80, 2}, {20, 4}, {4, 5}},
    /* 42 */ {{160, 1}, {32, 5}, {16, 2}, {4, 4}},
    /* 43 */ {{168, 1}, {84, 2}, {28, 3}, {4, 7}},
    /* 44 */ {{176, 1}, {88, 2}, {44, 2}, {4, 11}},
    /* 45 */ {{184, 1}, {92, 2}, {4, 23}, {4, 1}},
    /* 46 */ {{192, 1}, {96, 2}, {48, 2}, {4, 12}},
    /* 47 */ {{192, 1}, {96, 2}, {24, 4}, {4, 6}},
    /* 48 */ {{192, 1}, {64, 3}, {16, 4}, {4, 4}},
    /* 49 */ {{192, 1}, {24, 8}, {8, 3}, {4, 2}},
    /* 50 */ {{208, 1}, {104, 2}, {52, 2}, {4, 13}},
    /* 51 */ {{216, 1}, {108, 2}, {36, 3}, {4, 9}},
    /* 52 */ {{224, 1}, {112, 2}, {56, 2}, {4, 14}},
    /* 53 */ {{240, 1}, {120, 2}, {60, 2}, {4, 15}},
    /* 54 */ {{240, 1}, {80, 3}, {20, 4}, {4, 5}},
    /* 55 */ {{240, 1}, {48, 5}, {16, 3}, {8, 2}},
    /* 56 */ {{240, 1}, {24, 10}, {12, 2}, {4, 3}},
    /* 57 */ {{256, 1}, {128, 2}, {64, 2}, {4, 16}},
    /* 58 */ {{256, 1}, {128, 2}, {32, 4}, {4, 8}},
    /* 59 */ {{256, 1}, {16, 16}, {8, 2}, {4, 2}},
    /* 60 */ {{264, 1}, {132, 2}, {44, 3}, {4, 11}},
    /* 61 */ {{272, 1}, {136, 2}, {68, 2}, {4, 17}},
    /* 62 */ {{272, 1}, {68, 4}, {4, 17}, {4, 1}},
    /* 63 */ {{272, 1}, {16, 17}, {8, 2}, {4, 2}},
};

const char *duplex_mode[]={"FDD","TDD"};

static const uint8_t bit_reverse_table_256[] = {
    0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0, 0x08, 0x88, 0x48, 0xC8,
    0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8, 0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4,
    0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4, 0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC,
    0x3C, 0xBC, 0x7C, 0xFC, 0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
    0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA, 0x06, 0x86, 0x46, 0xC6,
    0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6, 0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE,
    0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE, 0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1,
    0x31, 0xB1, 0x71, 0xF1, 0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
    0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5, 0x0D, 0x8D, 0x4D, 0xCD,
    0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD, 0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3,
    0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3, 0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB,
    0x3B, 0xBB, 0x7B, 0xFB, 0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
    0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF};

void reverse_bits_u8(uint8_t const* in, size_t sz, uint8_t* out)
{
  DevAssert(in != NULL);
  DevAssert(out != NULL);

  for(size_t i = 0; i < sz; ++i)
    out[i] = bit_reverse_table_256[in[i]];
}

// Reverse bits implementation based on http://graphics.stanford.edu/~seander/bithacks.html
uint64_t reverse_bits(uint64_t in, int n_bits)
{
  // Reverse n_bits in uint64_t variable, example:
  // n_bits: 10
  // in:      10 0000 1111
  // return:  11 1100 0001

  AssertFatal(n_bits <= 64, "Maximum bits to reverse is 64, impossible to reverse %d bits!\n", n_bits);
  uint64_t rev_bits = 0;
  uint8_t *p = (uint8_t *)&in;
  uint8_t *q = (uint8_t *)&rev_bits;
  int n_bytes = n_bits >> 3;
  for (int n = 0; n < n_bytes; n++) {
    q[n_bytes - 1 - n] = bit_reverse_table_256[p[n]];
  }

  // Reverse remaining bits (not aligned with 8-bit)
  rev_bits = rev_bits << (n_bits % 8);
  for (int i = n_bytes * 8; i < n_bits; i++) {
    rev_bits |= ((in >> i) & 0x1) << (n_bits - i - 1);
  }
  return rev_bits;
}

static const int tables_5_3_2[5][12] = {
    {25, 52, 79, 106, 133, 160, 216, 270, -1, -1, -1, -1}, // 15 FR1
    {11, 24, 38, 51, 65, 78, 106, 133, 162, 217, 245, 273}, // 30 FR1
    {-1, 11, 18, 24, 31, 38, 51, 65, 79, 107, 121, 135}, // 60 FR1
    {66, 132, 264, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 60 FR2
    {32, 66, 132, 264, -1, -1, -1, -1, -1, -1, -1, -1} // 120FR2
};

int get_supported_band_index(int scs, frequency_range_t freq_range, int n_rbs)
{
  int scs_index = scs + freq_range;
  for (int i = 0; i < 12; i++) {
    if(n_rbs == tables_5_3_2[scs_index][i])
      return i;
  }
  return (-1); // not found
}

int get_smallest_supported_bandwidth_index(int scs, frequency_range_t frequency_range, int n_rbs)
{
  int scs_index = scs + frequency_range;
  for (int i = 0; i < 12; i++) {
    if (n_rbs <= tables_5_3_2[scs_index][i])
      return i;
  }
  return -1; // not found
}

// Table 5.2-1 NR operating bands in FR1 & FR2 (3GPP TS 38.101)
// Table 5.4.2.3-1 Applicable NR-ARFCN per operating band in FR1 & FR2 (3GPP TS 38.101)
// Notes:
// - N_OFFs for bands from 80 to 89 and band 95 is referred to UL
// - Frequencies are expressed in KHz
// - col: NR_band ul_min  ul_max  dl_min  dl_max  step  N_OFFs_DL  deltaf_raster
const nr_bandentry_t nr_bandtable[] = {
  {1,   1920000, 1980000, 2110000, 2170000, 20, 422000, 100},
  {2,   1850000, 1910000, 1930000, 1990000, 20, 386000, 100},
  {3,   1710000, 1785000, 1805000, 1880000, 20, 361000, 100},
  {5,    824000,  849000,  869000,  894000, 20, 173800, 100},
  {7,   2500000, 2570000, 2620000, 2690000, 20, 524000, 100},
  {8,    880000,  915000,  925000,  960000, 20, 185000, 100},
  {12,   699000,  716000,  729000,  746000, 20, 145800, 100},
  {13,   777000,  787000,  746000,  756000, 20, 149200, 100},
  {14,   788000,  798000,  758000,  768000, 20, 151600, 100},
  {18,   815000,  830000,  860000,  875000, 20, 172000, 100},
  {20,   832000,  862000,  791000,  821000, 20, 158200, 100},
  {24,  1627500, 1656500, 1526000, 1536000, 20, 305000, 100},
  {25,  1850000, 1915000, 1930000, 1995000, 20, 386000, 100},
  {26,   814000,  849000,  859000,  894000, 20, 171800, 100},
  {28,   703000,  758000,  758000,  813000, 20, 151600, 100},
  {29,      000,     000,  717000,  728000, 20, 143400, 100},
  {30,  2305000, 2315000, 2350000, 2360000, 20, 470000, 100},
  {34,  2010000, 2025000, 2010000, 2025000, 20, 402000, 100},
  {38,  2570000, 2620000, 2570000, 2630000, 20, 514000, 100},
  {39,  1880000, 1920000, 1880000, 1920000, 20, 376000, 100},
  {40,  2300000, 2400000, 2300000, 2400000, 20, 460000, 100},
  {41,  2496000, 2690000, 2496000, 2690000,  3, 499200,  15},
  {41,  2496000, 2690000, 2496000, 2690000,  6, 499200,  30},
  {47,  5855000, 5925000, 5855000, 5925000,  1, 790334,  15},
  {48,  3550000, 3700000, 3550000, 3700000,  1, 636667,  15},
  {48,  3550000, 3700000, 3550000, 3700000,  2, 636668,  30},
  {50,  1432000, 1517000, 1432000, 1517000, 20, 286400, 100},
  {51,  1427000, 1432000, 1427000, 1432000, 20, 285400, 100},
  {53,  2483500, 2495000, 2483500, 2495000, 20, 496700, 100},
  {65,  1920000, 2010000, 2110000, 2200000, 20, 422000, 100},
  {66,  1710000, 1780000, 2110000, 2200000, 20, 422000, 100},
  {67,      000,     000,  738000,  758000, 20, 147600, 100},
  {70,  1695000, 1710000, 1995000, 2020000, 20, 399000, 100},
  {71,   663000,  698000,  617000,  652000, 20, 123400, 100},
  {74,  1427000, 1470000, 1475000, 1518000, 20, 295000, 100},
  {75,      000,     000, 1432000, 1517000, 20, 286400, 100},
  {76,      000,     000, 1427000, 1432000, 20, 285400, 100},
  {77,  3300000, 4200000, 3300000, 4200000,  1, 620000,  15},
  {77,  3300000, 4200000, 3300000, 4200000,  2, 620000,  30},
  {78,  3300000, 3800000, 3300000, 3800000,  1, 620000,  15},
  {78,  3300000, 3800000, 3300000, 3800000,  2, 620000,  30},
  {79,  4400010, 5000000, 4400010, 5000000,  1, 693334,  15},
  {79,  4400010, 5000000, 4400010, 5000000,  2, 693334,  30},
  {80,  1710000, 1785000,     000,     000, 20, 342000, 100},
  {81,   880000,  915000,     000,     000, 20, 176000, 100},
  {82,   832000,  862000,     000,     000, 20, 166400, 100},
  {83,   703000,  748000,     000,     000, 20, 140600, 100},
  {84,  1920000, 1980000,     000,     000, 20, 384000, 100},
  {85,   698000,  716000,  728000,  746000, 20, 145600, 100},
  {86,  1710000, 1785000,     000,     000, 20, 342000, 100},
  {89,   824000,  849000,     000,     000, 20, 342000, 100},
  {90,  2496000, 2690000, 2496000, 2690000,  3, 499200,  15},
  {90,  2496000, 2690000, 2496000, 2690000,  6, 499200,  30},
  {90,  2496000, 2690000, 2496000, 2690000, 20, 499200, 100},
  {91,   832000,  862000, 1427000, 1432000, 20, 285400, 100},
  {92,   832000,  862000, 1432000, 1517000, 20, 286400, 100},
  {93,   880000,  915000, 1427000, 1432000, 20, 285400, 100},
  {94,   880000,  915000, 1432000, 1517000, 20, 286400, 100},
  {95,  2010000, 2025000,     000,     000, 20, 402000, 100},
  {96,  5925000, 7125000, 5925000, 7125000,  1, 795000,  15},
  {257,26500020,29500000,26500020,29500000,  1,2054166,  60},
  {257,26500080,29500000,26500080,29500000,  2,2054167, 120},
  {258,24250080,27500000,24250080,27500000,  1,2016667,  60},
  {258,24250080,27500000,24250080,27500000,  2,2016667, 120},
  {260,37000020,40000000,37000020,40000000,  1,2229166,  60},
  {260,37000080,40000000,37000080,40000000,  2,2229167, 120},
  {261,27500040,28350000,27500040,28350000,  1,2070833,  60},
  {261,27500040,28350000,27500040,28350000,  2,2070833, 120}
};

// synchronization raster per band tables (Rel.15)
// (38.101-1 Table 5.4.3.3-1 and 38.101-2 Table 5.4.3.3-1)
// band nb, sub-carrier spacing index, Range of gscn (First, Step size, Last)
// clang-format off
const sync_raster_t sync_raster[] = {
  {1, 0, 5279, 1, 5419},
  {2, 0, 4829, 1, 4969},
  {3, 0, 4517, 1, 4693},
  {5, 0, 2177, 1, 2230},
  {5, 1, 2183, 1, 2224},
  {7, 0, 6554, 1, 6718},
  {8, 0, 2318, 1, 2395},
  {12, 0, 1828, 1, 1858},
  {13, 0, 1871, 1, 1885},
  {14, 0, 1901, 1, 1915},
  {18, 0, 2156, 1, 2182},
  {20, 0, 1982, 1, 2047},
  {24, 0, 3818, 1, 3892},
  {24, 1, 3824, 1, 3886},
  {25, 0, 4829, 1, 4981},
  {26, 0, 2153, 1, 2230},
  {28, 0, 1901, 1, 2002},
  {29, 0, 1798, 1, 1813},
  {30, 0, 5879, 1, 5893},
  {34, 0, 5030, 1, 5056},
  {34, 1, 5036, 1, 5050},
  {38, 0, 6431, 1, 6544},
  {38, 1, 6437, 1, 6538},
  {39, 0, 4706, 1, 4795},
  {39, 1, 4712, 1, 4789},
  {40, 1, 5762, 1, 5989},
  {41, 0, 6246, 3, 6717},
  {41, 1, 6252, 3, 6714},
  {48, 1, 7884, 1, 7982},
  {50, 0, 3584, 1, 3787},
  {51, 0, 3572, 1, 3574},
  {53, 0, 6215, 1, 6232},
  {53, 1, 6221, 1, 6226},
  {65, 0, 5279, 1, 5494},
  {66, 0, 5279, 1, 5494},
  {66, 1, 5285, 1, 5488},
  {67, 0, 1850, 1, 1888},
  {70, 0, 4993, 1, 5044},
  {71, 0, 1547, 1, 1624},
  {74, 0, 3692, 1, 3790},
  {75, 0, 3584, 1, 3787},
  {76, 0, 3572, 1, 3574},
  {77, 1, 7711, 1, 8329},
  {78, 1, 7711, 1, 8051},
  {79, 1, 8480, 16, 8880},
  {85, 0, 1826, 1, 1858},
  {90, 1, 6252, 1, 6714},
  {91, 0, 3572, 1, 3574},
  {92, 0, 3584, 1, 3787},
  {93, 0, 3572, 1, 3574},
  {94, 0, 3584, 1, 3587},
  {257, 3, 22388, 1, 22558},
  {257, 4, 22390, 2, 22556},
  {258, 3, 22257, 1, 22443},
  {258, 4, 22258, 2, 22442},
  {260, 3, 22995, 1, 23166},
  {260, 4, 22996, 2, 23164},
  {261, 3, 22446, 1, 22492},
  {261, 4, 22446, 2, 22490},
};
// clang-format on

// Section 5.4.3 of 38.101-1 and -2
void check_ssb_raster(uint64_t freq, int band, int scs)
{
  int start_gscn = 0, step_gscn = 0, end_gscn = 0;
  for (int i = 0; i < sizeof(sync_raster) / sizeof(sync_raster_t); i++) {
    if (sync_raster[i].band == band && sync_raster[i].scs_index == scs) {
      start_gscn = sync_raster[i].first_gscn;
      step_gscn = sync_raster[i].step_gscn;
      end_gscn = sync_raster[i].last_gscn;
      break;
    }
  }
  AssertFatal(start_gscn != 0, "Couldn't find band %d with SCS %d\n", band, scs);
  int gscn;
  if (freq < 3000000000) {
    int N = 0;
    int M = 0;
    for (int k = 0; k < 3; k++) {
      M = (k << 1) + 1;
      if ((freq - M * 50000) % 1200000 == 0) {
        N = (freq - M * 50000) / 1200000;
        break;
      }
    }
    AssertFatal(N != 0, "SSB frequency %lu Hz not on the synchronization raster (N * 1200kHz + M * 50 kHz)\n", freq);
    gscn = (3 * N) + (M - 3) / 2;
  } else if (freq < 24250000000) {
    AssertFatal((freq - 3000000000) % 1440000 == 0,
                "SSB frequency %lu Hz not on the synchronization raster (3000 MHz + N * 1.44 MHz)\n",
                freq);
    gscn = ((freq - 3000000000) / 1440000) + 7499;
  } else {
    AssertFatal((freq - 24250080000) % 17280000 == 0,
                "SSB frequency %lu Hz not on the synchronization raster (24250.08 MHz + N * 17.28 MHz)\n",
                freq);
    gscn = ((freq - 24250080000) / 17280000) + 22256;
  }
  AssertFatal(gscn >= start_gscn && gscn <= end_gscn,
              "GSCN %d corresponding to SSB frequency %lu does not belong to GSCN range for band %d\n",
              gscn,
              freq,
              band);
  int rel_gscn = gscn - start_gscn;
  AssertFatal(rel_gscn % step_gscn == 0,
              "GSCN %d corresponding to SSB frequency %lu not in accordance with GSCN step for band %d\n",
              gscn,
              freq,
              band);
}

int get_supported_bw_mhz(frequency_range_t frequency_range, int bw_index)
{
  if (frequency_range == FR1) {
    int bandwidth_index_to_mhz[] = {5, 10, 15, 20, 25, 30, 40, 50, 60, 80, 90, 100};
    AssertFatal(bw_index >= 0 && bw_index <= sizeofArray(bandwidth_index_to_mhz),
                "Bandwidth index %d is invalid\n",
                bw_index);
    return bandwidth_index_to_mhz[bw_index];
  } else {
    int bandwidth_index_to_mhz[] = {50, 100, 200, 400};
    AssertFatal(bw_index >= 0 && bw_index <= sizeofArray(bandwidth_index_to_mhz),
                "Bandwidth index %d is invalid\n",
                bw_index);
    return bandwidth_index_to_mhz[bw_index];
  }
}

bool compare_relative_ul_channel_bw(int nr_band, int scs, int channel_bandwidth, frame_type_t frame_type)
{
  // 38.101-1 section 6.2.2
  // Relative channel bandwidth <= 4% for TDD bands and <= 3% for FDD bands
  int index = get_nr_table_idx(nr_band, scs);
  float limit = frame_type == TDD ? 0.04 : 0.03;
  float rel_bw = (float) (2 * channel_bandwidth * 1000) / (float) (nr_bandtable[index].ul_max - nr_bandtable[index].ul_min);
  return rel_bw > limit;
}

uint16_t get_band(uint64_t downlink_frequency, int32_t delta_duplex)
{
  const int64_t dl_freq_khz = downlink_frequency / 1000;
  const int32_t  delta_duplex_khz = delta_duplex / 1000;

  uint64_t center_freq_diff_khz = UINT64_MAX; // 2^64
  uint16_t current_band = 0;

  for (int ind = 0; ind < sizeofArray(nr_bandtable); ind++) {

    if (dl_freq_khz < nr_bandtable[ind].dl_min || dl_freq_khz > nr_bandtable[ind].dl_max)
      continue;

    int32_t current_offset_khz = nr_bandtable[ind].ul_min - nr_bandtable[ind].dl_min;

    if (current_offset_khz != delta_duplex_khz)
      continue;

    int64_t center_frequency_khz = (nr_bandtable[ind].dl_max + nr_bandtable[ind].dl_min) / 2;

    if (labs(dl_freq_khz - center_frequency_khz) < center_freq_diff_khz){
      current_band = nr_bandtable[ind].band;
      center_freq_diff_khz = labs(dl_freq_khz - center_frequency_khz);
    }
  }

  printf("DL frequency %"PRIu64": band %d, UL frequency %"PRIu64"\n",
        downlink_frequency, current_band, downlink_frequency+delta_duplex);

  AssertFatal(current_band != 0,
              "Can't find EUTRA band for frequency %" PRIu64 " and duplex_spacing %d\n",
              downlink_frequency,
              delta_duplex);

  return current_band;
}

int NRRIV2BW(int locationAndBandwidth,int N_RB) {
  int tmp = locationAndBandwidth/N_RB;
  int tmp2 = locationAndBandwidth%N_RB;
  if (tmp <= ((N_RB>>1)+1) && (tmp+tmp2)<N_RB) return(tmp+1);
  else                      return(N_RB+1-tmp);

}

int NRRIV2PRBOFFSET(int locationAndBandwidth,int N_RB) {
  int tmp = locationAndBandwidth/N_RB;
  int tmp2 = locationAndBandwidth%N_RB;
  if (tmp <= ((N_RB>>1)+1) && (tmp+tmp2)<N_RB) return(tmp2);
  else                      return(N_RB-1-tmp2);
}

/* TS 38.214 ch. 6.1.2.2.2 - Resource allocation type 1 for DL and UL */
int PRBalloc_to_locationandbandwidth0(int NPRB, int RBstart, int BWPsize)
{
  AssertFatal(NPRB>0 && (NPRB + RBstart <= BWPsize),
              "Illegal NPRB/RBstart Configuration (%d,%d) for BWPsize %d\n",
              NPRB, RBstart, BWPsize);

  if (NPRB <= 1 + (BWPsize >> 1))
    return (BWPsize * (NPRB - 1) + RBstart);
  else
    return (BWPsize * (BWPsize + 1 - NPRB) + (BWPsize - 1 - RBstart));
}

int PRBalloc_to_locationandbandwidth(int NPRB,int RBstart) {
  return(PRBalloc_to_locationandbandwidth0(NPRB,RBstart,275));
}

int cce_to_reg_interleaving(const int R, int k, int n_shift, const int C, int L, const int N_regs) {

  int f;  // interleaving function
  if(R==0)
    f = k;
  else {
    int c = k/R;
    int r = k % R;
    f = (r * C + c + n_shift) % (N_regs / L);
  }
  return f;
}

void get_coreset_rballoc(uint8_t *FreqDomainResource,int *n_rb,int *rb_offset) {

  uint8_t count=0, start=0, start_set=0;

  uint64_t bitmap = (((uint64_t)FreqDomainResource[0])<<37)|
    (((uint64_t)FreqDomainResource[1])<<29)|
    (((uint64_t)FreqDomainResource[2])<<21)|
    (((uint64_t)FreqDomainResource[3])<<13)|
    (((uint64_t)FreqDomainResource[4])<<5)|
    (((uint64_t)FreqDomainResource[5])>>3);

  for (int i=0; i<45; i++)
    if ((bitmap>>(44-i))&1) {
      count++;
      if (!start_set) {
        start = i;
        start_set = 1;
      }
    }
  *rb_offset = 6*start;
  *n_rb = 6*count;
}

int get_nb_periods_per_frame(uint8_t tdd_period)
{

  int nb_periods_per_frame;
  switch(tdd_period) {
    case 0:
      nb_periods_per_frame = 20; // 10ms/0p5ms
      break;

    case 1:
      nb_periods_per_frame = 16; // 10ms/0p625ms
      break;

    case 2:
      nb_periods_per_frame = 10; // 10ms/1ms
      break;

    case 3:
      nb_periods_per_frame = 8; // 10ms/1p25ms
      break;

    case 4:
      nb_periods_per_frame = 5; // 10ms/2ms
      break;

    case 5:
      nb_periods_per_frame = 4; // 10ms/2p5ms
      break;

    case 6:
      nb_periods_per_frame = 2; // 10ms/5ms
      break;

    case 7:
      nb_periods_per_frame = 1; // 10ms/10ms
      break;

    default:
      AssertFatal(1==0,"Undefined tdd period %d\n", tdd_period);
  }
  return nb_periods_per_frame;
}

void get_delta_arfcn(int i, uint32_t nrarfcn, uint64_t N_OFFs)
{
  uint32_t delta_arfcn = nrarfcn - N_OFFs;

  if(delta_arfcn % (nr_bandtable[i].step_size) != 0)
    LOG_E(NR_MAC, "nrarfcn %u is not on the channel raster for step size %lu\n", nrarfcn, nr_bandtable[i].step_size);
}

uint32_t to_nrarfcn(int nr_bandP, uint64_t dl_CarrierFreq, uint8_t scs_index, uint32_t bw)
{
  uint64_t dl_CarrierFreq_by_1k = dl_CarrierFreq / 1000;
  int bw_kHz = bw / 1000;
  uint32_t nrarfcn;
  int i = get_nr_table_idx(nr_bandP, scs_index);

  LOG_D(NR_MAC, "Searching for nr band %d DL Carrier frequency %llu bw %u\n", nr_bandP, (long long unsigned int)dl_CarrierFreq, bw);

  AssertFatal(dl_CarrierFreq_by_1k >= nr_bandtable[i].dl_min,
              "Band %d, bw %u : DL carrier frequency %llu kHz < %llu\n",
	      nr_bandP, bw, (long long unsigned int)dl_CarrierFreq_by_1k,
	      (long long unsigned int)nr_bandtable[i].dl_min);
  AssertFatal(dl_CarrierFreq_by_1k <= (nr_bandtable[i].dl_max - bw_kHz/2),
              "Band %d, dl_CarrierFreq %llu bw %u: DL carrier frequency %llu kHz > %llu\n",
	      nr_bandP, (long long unsigned int)dl_CarrierFreq,bw, (long long unsigned int)dl_CarrierFreq_by_1k,
	      (long long unsigned int)(nr_bandtable[i].dl_max - bw_kHz/2));

  int deltaFglobal = 60;
  uint32_t N_REF_Offs = 2016667;
  uint64_t F_REF_Offs_khz = 24250080;

  if (dl_CarrierFreq < 24.25e9) {
    deltaFglobal = 15;
    N_REF_Offs = 600000;
    F_REF_Offs_khz = 3000000;
  }
  if (dl_CarrierFreq < 3e9) {
    deltaFglobal = 5;
    N_REF_Offs = 0;
    F_REF_Offs_khz = 0;
  }

  // This is equation before Table 5.4.2.1-1 in 38101-1-f30
  // F_REF=F_REF_Offs + deltaF_Global(N_REF-NREF_REF_Offs)
  nrarfcn =  (((dl_CarrierFreq_by_1k - F_REF_Offs_khz) / deltaFglobal) + N_REF_Offs);
  //get_delta_arfcn(i, nrarfcn, nr_bandtable[i].N_OFFs_DL);

  return nrarfcn;
}

// This function computes the RF reference frequency from the NR-ARFCN according to 5.4.2.1 of 3GPP TS 38.104
// this function applies to both DL and UL
uint64_t from_nrarfcn(int nr_bandP, uint8_t scs_index, uint32_t nrarfcn)
{
  int deltaFglobal = 5;
  uint32_t N_REF_Offs = 0;
  uint64_t F_REF_Offs_khz = 0;
  uint64_t N_OFFs, frequency, freq_min;
  int i = get_nr_table_idx(nr_bandP, scs_index);

  if (nrarfcn > 599999 && nrarfcn < 2016667) {
    deltaFglobal = 15;
    N_REF_Offs = 600000;
    F_REF_Offs_khz = 3000000;
  }
  if (nrarfcn > 2016666 && nrarfcn < 3279166) {
    deltaFglobal = 60;
    N_REF_Offs = 2016667;
    F_REF_Offs_khz = 24250080;
  }

  int32_t delta_duplex = get_delta_duplex(nr_bandP, scs_index);

  if (delta_duplex <= 0){ // DL band >= UL band
    if (nrarfcn >= nr_bandtable[i].N_OFFs_DL){ // is TDD of FDD DL
      N_OFFs = nr_bandtable[i].N_OFFs_DL;
      freq_min = nr_bandtable[i].dl_min;
    } else {// is FDD UL
      N_OFFs = nr_bandtable[i].N_OFFs_DL + delta_duplex/deltaFglobal;
      freq_min = nr_bandtable[i].ul_min;
    }
  } else { // UL band > DL band
    if (nrarfcn >= nr_bandtable[i].N_OFFs_DL + delta_duplex / deltaFglobal){ // is FDD UL
      N_OFFs = nr_bandtable[i].N_OFFs_DL + delta_duplex / deltaFglobal;
      freq_min = nr_bandtable[i].ul_min;
    } else { // is FDD DL
      N_OFFs = nr_bandtable[i].N_OFFs_DL;
      freq_min = nr_bandtable[i].dl_min;
    }
  }

  LOG_D(NR_MAC, "Frequency from NR-ARFCN for N_OFFs %lu, duplex spacing %d KHz, deltaFglobal %d KHz\n",
        N_OFFs,
        delta_duplex,
        deltaFglobal);

  AssertFatal(nrarfcn >= N_OFFs,"nrarfcn %u < N_OFFs[%d] %llu\n", nrarfcn, nr_bandtable[i].band, (long long unsigned int)N_OFFs);
  get_delta_arfcn(i, nrarfcn, N_OFFs);

  frequency = 1000 * (F_REF_Offs_khz + (nrarfcn - N_REF_Offs) * deltaFglobal);

  LOG_D(NR_MAC, "Computing frequency (nrarfcn %llu => %llu KHz (freq_min %llu KHz, NR band %d N_OFFs %llu))\n",
        (unsigned long long)nrarfcn,
        (unsigned long long)frequency/1000,
        (unsigned long long)freq_min,
        nr_bandP,
        (unsigned long long)N_OFFs);

  return frequency;
}

int get_first_ul_slot(int nrofDownlinkSlots, int nrofDownlinkSymbols, int nrofUplinkSymbols)
{
  return (nrofDownlinkSlots + (nrofDownlinkSymbols != 0 && nrofUplinkSymbols == 0));
}

int get_dmrs_port(int nl, uint16_t dmrs_ports)
{

  if (dmrs_ports == 0) return 0; // dci 1_0
  int p = -1;
  int found = -1;
  for (int i=0; i<12; i++) { // loop over dmrs ports
    if((dmrs_ports>>i)&0x01) { // check if current bit is 1
      found++;
      if (found == nl) { // found antenna port number corresponding to current layer
        p = i;
        break;
      }
    }
  }
  AssertFatal(p>-1,"No dmrs port corresponding to layer %d found\n",nl);
  return p;
}

frame_type_t get_frame_type(uint16_t current_band, uint8_t scs_index)
{
  int32_t delta_duplex = get_delta_duplex(current_band, scs_index);
  frame_type_t current_type = delta_duplex == 0 ? TDD : FDD;
  LOG_D(NR_MAC, "NR band %d, duplex mode %s, duplex spacing = %d KHz\n", current_band, duplex_mode[current_type], delta_duplex);
  return current_type;
}

// Computes the duplex spacing (either positive or negative) in KHz
int32_t get_delta_duplex(int nr_bandP, uint8_t scs_index)
{
  int nr_table_idx = get_nr_table_idx(nr_bandP, scs_index);

  int32_t delta_duplex = (nr_bandtable[nr_table_idx].ul_min - nr_bandtable[nr_table_idx].dl_min);

  LOG_D(NR_MAC, "NR band duplex spacing is %d KHz (nr_bandtable[%d].band = %d)\n", delta_duplex, nr_table_idx, nr_bandtable[nr_table_idx].band);

  return delta_duplex;
}

// Returns the corresponding row index of the NR table
int get_nr_table_idx(int nr_bandP, uint8_t scs_index)
{
  int scs_khz = 15 << scs_index;
  int supplementary_bands[] = {29, 75, 76, 80, 81, 82, 83, 84, 86, 89, 95};
  for(int j = 0; j < sizeofArray(supplementary_bands); j++) {
    AssertFatal(nr_bandP != supplementary_bands[j],
                "Band %d is a supplementary band (%d). This is not supported yet.\n",
                nr_bandP,
                supplementary_bands[j]);
  }
  int i;
  for (i = 0; i < sizeofArray(nr_bandtable); i++) {
    if (nr_bandtable[i].band == nr_bandP && nr_bandtable[i].deltaf_raster == scs_khz)
      break;
  }

  if (i == sizeofArray(nr_bandtable)) {
    LOG_D(PHY, "Not found same deltaf_raster == scs_khz, use only band and last deltaf_raster \n");
    for(i = sizeofArray(nr_bandtable) - 1; i >= 0; i--)
       if (nr_bandtable[i].band == nr_bandP)
         break;
  }

  AssertFatal(i >= 0 && i < sizeofArray(nr_bandtable), "band is not existing: %d\n", nr_bandP);
  LOG_D(PHY,
        "NR band table index %d (Band %d, dl_min %lu, ul_min %lu)\n",
         i,
         nr_bandtable[i].band,
         nr_bandtable[i].dl_min,
         nr_bandtable[i].ul_min);

  return i;
}

int get_subband_size(int NPRB,int size) {
  // implements table  5.2.1.4-2 from 36.214
  //
  //Bandwidth part (PRBs)	Subband size (PRBs)
  // < 24	                   N/A
  //24 – 72	                   4, 8
  //73 – 144	                   8, 16
  //145 – 275	                  16, 32

  if (NPRB<24) return(1);
  if (NPRB<72) return (size==0 ? 4 : 8);
  if (NPRB<144) return (size==0 ? 8 : 16);
  if (NPRB<275) return (size==0 ? 16 : 32);
  AssertFatal(1==0,"Shouldn't get here, NPRB %d\n",NPRB);
 
}

void get_samplerate_and_bw(int mu,
                           int n_rb,
                           int8_t threequarter_fs,
                           double *sample_rate,
                           unsigned int *samples_per_frame,
                           double *tx_bw,
                           double *rx_bw) {

  if (mu == 0) {
    switch(n_rb) {
    case 270:
      if (threequarter_fs) {
        *sample_rate=92.16e6;
        *samples_per_frame = 921600;
        *tx_bw = 50e6;
        *rx_bw = 50e6;
      } else {
        *sample_rate=61.44e6;
        *samples_per_frame = 614400;
        *tx_bw = 50e6;
        *rx_bw = 50e6;
      }
      break;
    case 216:
      if (threequarter_fs) {
        *sample_rate=46.08e6;
        *samples_per_frame = 460800;
        *tx_bw = 40e6;
        *rx_bw = 40e6;
      }
      else {
        *sample_rate=61.44e6;
        *samples_per_frame = 614400;
        *tx_bw = 40e6;
        *rx_bw = 40e6;
      }
      break;
    case 160: //30 MHz
    case 133: //25 MHz
      if (threequarter_fs) {
        AssertFatal(1==0,"N_RB %d cannot use 3/4 sampling\n",n_rb);
      }
      else {
        *sample_rate=30.72e6;
        *samples_per_frame = 307200;
        *tx_bw = 20e6;
        *rx_bw = 20e6;
      }
      break;
    case 106:
      if (threequarter_fs) {
        *sample_rate=23.04e6;
        *samples_per_frame = 230400;
        *tx_bw = 20e6;
        *rx_bw = 20e6;
      }
      else {
        *sample_rate=30.72e6;
        *samples_per_frame = 307200;
        *tx_bw = 20e6;
        *rx_bw = 20e6;
      }
      break;
    case 52:
      if (threequarter_fs) {
        *sample_rate=11.52e6;
        *samples_per_frame = 115200;
        *tx_bw = 10e6;
        *rx_bw = 10e6;
      }
      else {
        *sample_rate=15.36e6;
        *samples_per_frame = 153600;
        *tx_bw = 10e6;
        *rx_bw = 10e6;
      }
      break;
    case 25:
      if (threequarter_fs) {
        *sample_rate=5.76e6;
        *samples_per_frame = 57600;
        *tx_bw = 5e6;
        *rx_bw = 5e6;
      }
      else {
        *sample_rate=7.68e6;
        *samples_per_frame = 76800;
        *tx_bw = 5e6;
        *rx_bw = 5e6;
      }
      break;
    default:
      AssertFatal(0==1,"N_RB %d not yet supported for numerology %d\n",n_rb,mu);
    }
  } else if (mu == 1) {
    switch(n_rb) {

    case 273:
      if (threequarter_fs) {
        *sample_rate=184.32e6;
        *samples_per_frame = 1843200;
        *tx_bw = 100e6;
        *rx_bw = 100e6;
      } else {
        *sample_rate=122.88e6;
        *samples_per_frame = 1228800;
        *tx_bw = 100e6;
        *rx_bw = 100e6;
      }
      break;
    case 217:
      if (threequarter_fs) {
        *sample_rate=92.16e6;
        *samples_per_frame = 921600;
        *tx_bw = 80e6;
        *rx_bw = 80e6;
      } else {
        *sample_rate=122.88e6;
        *samples_per_frame = 1228800;
        *tx_bw = 80e6;
        *rx_bw = 80e6;
      }
      break;
    case 162 :
      if (threequarter_fs) {
        AssertFatal(1==0,"N_RB %d cannot use 3/4 sampling\n",n_rb);
      }
      else {
        *sample_rate=61.44e6;
        *samples_per_frame = 614400;
        *tx_bw = 60e6;
        *rx_bw = 60e6;
      }

      break;

    case 133 :
      if (threequarter_fs) {
	AssertFatal(1==0,"N_RB %d cannot use 3/4 sampling\n",n_rb);
      }
      else {
        *sample_rate=61.44e6;
        *samples_per_frame = 614400;
        *tx_bw = 50e6;
        *rx_bw = 50e6;
      }

      break;
    case 106:
      if (threequarter_fs) {
        *sample_rate=46.08e6;
        *samples_per_frame = 460800;
        *tx_bw = 40e6;
        *rx_bw = 40e6;
      }
      else {
        *sample_rate=61.44e6;
        *samples_per_frame = 614400;
        *tx_bw = 40e6;
        *rx_bw = 40e6;
      }
     break;
    case 51:
      if (threequarter_fs) {
        *sample_rate=23.04e6;
        *samples_per_frame = 230400;
        *tx_bw = 20e6;
        *rx_bw = 20e6;
      }
      else {
        *sample_rate=30.72e6;
        *samples_per_frame = 307200;
        *tx_bw = 20e6;
        *rx_bw = 20e6;
      }
      break;
    case 24:
      if (threequarter_fs) {
        *sample_rate=11.52e6;
        *samples_per_frame = 115200;
        *tx_bw = 10e6;
        *rx_bw = 10e6;
      }
      else {
        *sample_rate=15.36e6;
        *samples_per_frame = 153600;
        *tx_bw = 10e6;
        *rx_bw = 10e6;
      }
      break;
    default:
      AssertFatal(0==1,"N_RB %d not yet supported for numerology %d\n",n_rb,mu);
    }
  } else if (mu == 3) {
    switch(n_rb) {
      case 132:
      case 128:
        if (threequarter_fs) {
          *sample_rate=184.32e6;
          *samples_per_frame = 1843200;
          *tx_bw = 200e6;
          *rx_bw = 200e6;
        } else {
          *sample_rate = 245.76e6;
          *samples_per_frame = 2457600;
          *tx_bw = 200e6;
          *rx_bw = 200e6;
        }
        break;

      case 66:
      case 64:
        if (threequarter_fs) {
          *sample_rate=92.16e6;
          *samples_per_frame = 921600;
          *tx_bw = 100e6;
          *rx_bw = 100e6;
        } else {
          *sample_rate = 122.88e6;
          *samples_per_frame = 1228800;
          *tx_bw = 100e6;
          *rx_bw = 100e6;
        }
        break;

      case 32:
        if (threequarter_fs) {
          *sample_rate=92.16e6;
          *samples_per_frame = 921600;
          *tx_bw = 50e6;
          *rx_bw = 50e6;
        } else {
          *sample_rate=61.44e6;
          *samples_per_frame = 614400;
          *tx_bw = 50e6;
          *rx_bw = 50e6;
        }
        break;

      default:
        AssertFatal(0==1,"N_RB %d not yet supported for numerology %d\n",n_rb,mu);
    }
  } else {
    AssertFatal(0 == 1,"Numerology %d not supported for the moment\n",mu);
  }
}

// from start symbol index and nb or symbols to symbol occupation bitmap in a slot
uint16_t SL_to_bitmap(int startSymbolIndex, int nrOfSymbols) {
 return ((1<<nrOfSymbols)-1)<<startSymbolIndex;
}

int get_SLIV(uint8_t S, uint8_t L) {
  return ( (uint16_t)(((L-1)<=7)? (14*(L-1)+S) : (14*(15-L)+(13-S))) );
}

void SLIV2SL(int SLIV,int *S,int *L) {

  int SLIVdiv14 = SLIV/14;
  int SLIVmod14 = SLIV%14;
  // Either SLIV = 14*(L-1) + S, or SLIV = 14*(14-L+1) + (14-1-S). Condition is 0 <= L <= 14-S
  if ((SLIVdiv14 + 1) >= 0 && (SLIVdiv14 <= 13-SLIVmod14)) {
    *L=SLIVdiv14+1;
    *S=SLIVmod14;
  } else  {
    *L=15-SLIVdiv14;
    *S=13-SLIVmod14;
  }
}

int get_ssb_subcarrier_offset(uint32_t absoluteFrequencySSB, uint32_t absoluteFrequencyPointA, int scs)
{
  // for FR1 k_SSB expressed in terms of 15kHz SCS
  // for FR2 k_SSB expressed in terms of the subcarrier spacing provided by the higher-layer parameter subCarrierSpacingCommon
  // absoluteFrequencySSB and absoluteFrequencyPointA are ARFCN
  // NR-ARFCN delta frequency is 5kHz if f < 3 GHz, 15kHz for other FR1 freq and 60kHz for FR2
  const uint32_t absolute_diff = absoluteFrequencySSB - absoluteFrequencyPointA;
  int scaling = 1;
  if (absoluteFrequencyPointA < 600000) // correspond to 3GHz
    scaling = 3;
  if (scs > 2) // FR2
    scaling <<= (scs - 2);
  int sco_limit = scs == 1 ? 24 : 12;
  int subcarrier_offset = (absolute_diff / scaling) % sco_limit;
  // 30kHz is the only case where k_SSB is expressed in terms of a different SCS (15kHz)
  // the assertion is to avoid having an offset of half a subcarrier
  if (scs == 1)
    AssertFatal(subcarrier_offset % 2 == 0, "ssb offset %d invalid for scs %d\n", subcarrier_offset, scs);
  return subcarrier_offset;
}

uint32_t get_ssb_offset_to_pointA(uint32_t absoluteFrequencySSB,
                                  uint32_t absoluteFrequencyPointA,
                                  int ssbSubcarrierSpacing,
                                  int frequency_range)
{
  // offset to pointA is expressed in terms of 15kHz SCS for FR1 and 60kHz for FR2
  // only difference wrt NR-ARFCN is delta frequency 5kHz if f < 3 GHz for ARFCN
  uint32_t absolute_diff = (absoluteFrequencySSB - absoluteFrequencyPointA);
  const int scaling_5khz = absoluteFrequencyPointA < 600000 ? 3 : 1;
  const int scaling = frequency_range == FR2 ? 1 << (ssbSubcarrierSpacing - 2) : 1 << ssbSubcarrierSpacing;
  const int scaled_abs_diff = absolute_diff / (scaling_5khz * scaling);
  // absoluteFrequencySSB is the central frequency of SSB which is made by 20RBs in total
  const int ssb_offset_point_a = ((scaled_abs_diff / 12) - 10) * scaling;
  // Offset to point A needs to be divisible by scaling
  AssertFatal(ssb_offset_point_a % scaling == 0, "PRB offset %d not valid for scs %d\n", ssb_offset_point_a, ssbSubcarrierSpacing);
  return ssb_offset_point_a;
}

static double get_start_freq(const double fc, const int nbRB, const int mu)
{
  const int scs = MU_SCS(mu) * 1000;
  return fc - ((double)nbRB / 2 * NR_NB_SC_PER_RB * scs);
}

static double get_stop_freq(const double fc, const int nbRB, const int mu)
{
  int scs = MU_SCS(mu) * 1000;
  return fc + ((double)nbRB / 2 * NR_NB_SC_PER_RB * scs);
}

static void compute_M_and_N(const int gscn, int *rM, int *rN)
{
  if (gscn > 1 && gscn < 7499) {
    for (int M = 1; M < 6; M += 2) {
      /* GSCN = 3N + (M-3) / 2
         N(int) = 2 * GSCN + 3 - M
      */
      if (((2 * gscn + 3 - M) % 6) == 0) {
        *rM = M;
        *rN = (2 * gscn + 3 - M) / 6;
        break;
      }
    }
  } else if (gscn > 7498 && gscn < 22256) {
    *rN = gscn - 7499;
  } else if (gscn > 22255 && gscn < 26638) {
    *rN = gscn - 22256;
  } else {
    LOG_E(NR_PHY, "Invalid GSCN\n");
    abort();
  }
}

// Section 5.4.3 of 38.101-1 and -2
static double get_ssref_from_gscn(const int gscn)
{
  int M, N = -1;
  compute_M_and_N(gscn, &M, &N);
  if (gscn > 1 && gscn < 7499) { // Sub 3GHz
    AssertFatal(N > 0 && N < 2500, "Invalid N\n");
    AssertFatal(M > 0 && M < 6 && (M & 0x1), "Invalid M\n");
    return (N * 1200e3 + M * 50e3);
  } else if (gscn > 7498 && gscn < 22256) {
    AssertFatal(N > -1 && N < 14757, "Invalid N\n");
    return (3000e6 + N * 1.44e6);
  } else if (gscn > 22255 && gscn < 26638) {
    AssertFatal(N > -1 && N < 4382, "Invalid N\n");
    return (24250.08e6 + N * 17.28e6);
  } else {
    LOG_E(NR_PHY, "Invalid GSCN\n");
    abort();
  }
}

static void find_gscn_to_scan(const double startFreq,
                              const double stopFreq,
                              const sync_raster_t gscn,
                              int *scanGscnStart,
                              int *scanGscnStop)
{
  const double scs = MU_SCS(gscn.scs_index) * 1e3;
  const double ssbBW = 20 * NR_NB_SC_PER_RB * scs;

  for (int g = gscn.first_gscn; g < gscn.last_gscn; g += gscn.step_gscn) {
    const double centerSSBFreq = get_ssref_from_gscn(g);
    const double startSSBFreq = centerSSBFreq - ssbBW / 2;
    if (startSSBFreq < startFreq)
      continue;

    *scanGscnStart = g;
    break;
  }
  *scanGscnStop = *scanGscnStart;

  for (int g = gscn.last_gscn; g > gscn.first_gscn; g -= gscn.step_gscn) {
    const double centerSSBFreq = get_ssref_from_gscn(g);
    const double stopSSBFreq = centerSSBFreq + ssbBW / 2 - 1;
    if (stopSSBFreq > stopFreq)
      continue;

    *scanGscnStop = g;
    break;
  }
}

static int get_ssb_first_sc(const double pointA, const double ssbCenter, const int mu)
{
  const double scs = MU_SCS(mu) * 1e3;
  const int ssbRBs = 20;
  return (int)((ssbCenter - pointA) / scs - (ssbRBs / 2 * NR_NB_SC_PER_RB));
}

/* Returns array of first SCS offset in the scanning window */
int get_scan_ssb_first_sc(const double fc, const int nbRB, const int nrBand, const int mu, nr_gscn_info_t ssbInfo[MAX_GSCN_BAND])
{
  const double startFreq = get_start_freq(fc, nbRB, mu);
  const double stopFreq = get_stop_freq(fc, nbRB, mu);

  int scanGscnStart = 0;
  int scanGscnStop = 0;
  const sync_raster_t *tmpRaster = sync_raster;
  const sync_raster_t * end=sync_raster + sizeofArray(sync_raster);
  while (tmpRaster < end && (tmpRaster->band != nrBand || tmpRaster->scs_index != mu))
    tmpRaster++;
  if (tmpRaster >= end) {
    LOG_E(PHY, "raster not found nrband=%d, mu=%d\n", nrBand, mu);
    return 0;
  }

  find_gscn_to_scan(startFreq, stopFreq, *tmpRaster, &scanGscnStart, &scanGscnStop);

  const double scs = MU_SCS(mu) * 1e3;
  const double pointA = fc - ((double)nbRB / 2 * scs * NR_NB_SC_PER_RB);
  int numGscn = 0;
  for (int g = scanGscnStart; (g <= scanGscnStop) && (numGscn < MAX_GSCN_BAND); g += tmpRaster->step_gscn) {
    ssbInfo[numGscn].ssRef = get_ssref_from_gscn(g);
    ssbInfo[numGscn].ssbFirstSC = get_ssb_first_sc(pointA, ssbInfo[numGscn].ssRef, mu);
    ssbInfo[numGscn].gscn = g;
    numGscn++;
  }

  return numGscn;
}

int get_delay_idx(int delay, int max_delay_comp)
{
  int delay_idx = max_delay_comp + delay;
  // If the measured delay is less than -MAX_DELAY_COMP, a -MAX_DELAY_COMP delay is compensated.
  delay_idx = max(delay_idx, 0);
  // If the measured delay is greater than +MAX_DELAY_COMP, a +MAX_DELAY_COMP delay is compensated.
  delay_idx = min(delay_idx, max_delay_comp << 1);
  return delay_idx;
}

void init_delay_table(uint16_t ofdm_symbol_size,
                      int max_delay_comp,
                      int max_ofdm_symbol_size,
                      c16_t delay_table[][max_ofdm_symbol_size])
{
  for (int delay = -max_delay_comp; delay <= max_delay_comp; delay++) {
    for (int k = 0; k < ofdm_symbol_size; k++) {
      double complex delay_cexp = cexp(I * (2.0 * M_PI * k * delay / ofdm_symbol_size));
      delay_table[max_delay_comp + delay][k].r = (int16_t)round(256 * creal(delay_cexp));
      delay_table[max_delay_comp + delay][k].i = (int16_t)round(256 * cimag(delay_cexp));
    }
  }
}

int set_default_nta_offset(frequency_range_t freq_range, uint32_t samples_per_subframe)
{
  // ta_offset_samples : ta_offset = samples_per_subframe : (Δf_max x N_f / 1000)
  // As described in Section 4.3.1 in 38.211

  // TODO There is no way for the UE to know about LTE-NR coexistence case
  //      as mentioned in Table 7.1.2-2 of 38.133
  //      LTE-NR coexistence means the presence of an active LTE service in the same band as NR in current deployment
  //      We assume no coexistence

  uint64_t numer = (freq_range == FR1 ? 25600 : 13792) * (uint64_t)samples_per_subframe;
  return numer / (4096 * 480);
}

void nr_timer_start(NR_timer_t *timer)
{
  timer->active = true;
  timer->counter = 0;
}

void nr_timer_stop(NR_timer_t *timer)
{
  timer->active = false;
  timer->counter = 0;
}

bool nr_timer_is_active(const NR_timer_t *timer)
{
  return timer->active;
}

bool nr_timer_tick(NR_timer_t *timer)
{
  bool expired = false;
  if (timer->active) {
    timer->counter += timer->step;
    if (timer->target == UINT_MAX) // infinite target, never expires
      return false;
    expired = nr_timer_expired(timer);
    if (expired)
      timer->active = false;
  }
  return expired;
}

bool nr_timer_expired(const NR_timer_t *timer)
{
  if (timer->target == UINT_MAX) // infinite target, never expires
    return false;
  return timer->counter >= timer->target;
}

uint32_t nr_timer_elapsed_time(const NR_timer_t *timer)
{
  return timer->counter;
}

void nr_timer_setup(NR_timer_t *timer, const uint32_t target, const uint32_t step)
{
  timer->target = target;
  timer->step = step;
  nr_timer_stop(timer);
}

unsigned short get_m_srs(int c_srs, int b_srs) {
  return srs_bandwidth_config[c_srs][b_srs][0];
}

unsigned short get_N_b_srs(int c_srs, int b_srs) {
  return srs_bandwidth_config[c_srs][b_srs][1];
}
