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

#ifndef __PHY_TEST_TOOLS_HPP__
#define __PHY_TEST_TOOLS_HPP__

#include <vector>
#include <random>
#include <algorithm>
extern "C" {
#include "openair1/PHY/TOOLS/tools_defs.h"
}

constexpr bool is_power_of_two(uint64_t n)
{
  return n != 0 && (n & (n - 1)) == 0;
}

size_t align_up(size_t a, size_t b)
{
  return (a + b - 1) / b * b;
}

// Template adaptations for std::vector. This is needed because the avx functions expect 256 bit alignment.
template <typename T, size_t alignment>
class AlignedAllocator {
 public:
  static_assert(is_power_of_two(alignment), "Alignment should be power of 2");
  static_assert(alignment >= 8, "Alignment must be at least 8 bits");
  using value_type = T;

  AlignedAllocator() = default;

  AlignedAllocator(const AlignedAllocator &) = default;

  AlignedAllocator &operator=(const AlignedAllocator &) = default;

  template <typename U>
  struct rebind {
    using other = AlignedAllocator<U, alignment>;
  };

  T *allocate(size_t n)
  {
    size_t alignment_bytes = alignment / 8;
    void *ptr = ::aligned_alloc(alignment_bytes, align_up(n * sizeof(T), alignment_bytes));
    return static_cast<T *>(ptr);
  }

  void deallocate(T *p, size_t n)
  {
    ::free(p);
  }
};

// Using 512-aligned vector in case some functions use avx-512
template <typename T>
using AlignedAllocator512 = AlignedAllocator<T, 512>;
template <typename T>
using AlignedVector512 = std::vector<T, AlignedAllocator512<T>>;

AlignedVector512<c16_t> generate_random_c16(size_t num)
{
  std::random_device rd;
  std::mt19937 rng(rd());
  std::uniform_int_distribution<int16_t> dist(INT16_MIN, INT16_MAX);
  AlignedVector512<c16_t> vec;
  vec.resize(num);
  auto gen = [&]() { return (c16_t){dist(rng), dist(rng)}; };
  std::generate(vec.begin(), vec.end(), gen);
  return vec;
}

AlignedVector512<uint16_t> generate_random_uint16(size_t num)
{
  AlignedVector512<uint16_t> vec;
  vec.resize(num);
  auto gen = [&]() { return static_cast<uint16_t>(std::rand()); };
  std::generate(vec.begin(), vec.end(), gen);
  return vec;
}

#endif
