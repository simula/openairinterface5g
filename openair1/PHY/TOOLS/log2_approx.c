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

#include <simde/x86/avx512/lzcnt.h>

unsigned char log2_approx(unsigned int x)
{
  const unsigned int round_val = 3037000499U; // 2^31.5
  if (!x)
    return 0;

  int l2 = simde_x_clz32(x);
  if (x > (round_val >> l2))
    l2 = 32 - l2;
  else
    l2 = 31 - l2;

  return l2;
}


unsigned char factor2(unsigned int x)
{
  unsigned char l2 = 0;
  int i;
  for (i = 0; i < 31; i++)
    if ((x & (1 << i)) != 0)
      break;

  l2 = i;
  return l2;
}

unsigned char log2_approx64(unsigned long long int x)
{
  const unsigned long long round_val = 13043817825332782212ULL; // 2^63.5
  if (!x)
    return 0;

  int l2 = simde_x_clz64(x);
  if (x > (round_val >> l2))
    l2 = 64 - l2;
  else
    l2 = 63 - l2;

  return l2;
}

