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

#ifndef __LAYER2_NR_MAC_CONFIG_H__
#define __LAYER2_NR_MAC_CONFIG_H__

#include <stdint.h>

typedef struct vector_s {
  int X;
  int Y;
  int Z;
} vector_t;

// Format similar to values sent in SIB19
typedef struct gnb_sat_position_update_s {
  int sfn;
  int subframe;
  uint32_t delay;
  int drift;
  uint32_t accel;
  vector_t position;
  vector_t velocity;
} gnb_sat_position_update_t;

bool nr_update_sib19(const gnb_sat_position_update_t *sat_position);

#endif /*__LAYER2_NR_MAC_CONFIG_H__*/
