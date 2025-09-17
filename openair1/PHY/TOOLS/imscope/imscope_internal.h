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

#ifndef IMSCOPE_INTERNAL_H
#define IMSCOPE_INTERNAL_H

#include <mutex>
#include <fstream>
#include <vector>
#include <fstream>
#include "openair1/PHY/defs_nr_UE.h"
extern "C" {
#include "../phy_scope_interface.h"
}

#define MAX_OFFSETS 14

const char *scope_id_to_string(scopeDataType type);

typedef struct ImScopeData {
  scopeGraphData_t *scope_graph_data;
  metadata meta;
  uint64_t time_taken_in_ns;
  uint64_t time_taken_in_ns_per_offset[MAX_OFFSETS];
  size_t data_copied_per_offset[MAX_OFFSETS];
  int scope_id;

  void dump_to_file(std::ofstream& stream) {
    stream.write((char*)scope_graph_data, sizeof(*scope_graph_data));
    stream.write((char*)&meta, sizeof(meta));
    stream.write((char*)&scope_id, sizeof(scope_id));
    char *source = (char *)(scope_graph_data + 1);
    stream.write(source, scope_graph_data->dataSize);
  }

  void read_from_file(std::ifstream& stream) {
    scopeGraphData_t temp;
    stream.read((char*)&temp, sizeof(temp));
    stream.read((char*)&meta, sizeof(meta));
    stream.read((char*)&scope_id, sizeof(scope_id));
    scope_graph_data = static_cast<scopeGraphData_t *>(malloc(temp.dataSize + sizeof(*scope_graph_data)));
    *scope_graph_data = temp;
    char *dest = (char *)(scope_graph_data + 1);
    stream.read(dest, temp.dataSize);
  }

  friend std::ostream& operator<<(std::ostream& os, const ImScopeData& dt);
} ImScopeData;

typedef struct MovingAverageTimer {
  uint64_t sum = 0;
  float average = 0;
  float last_update_time = 0;
  void UpdateAverage(float time)
  {
    if (time > last_update_time + 1) {
      float new_average = sum / (float)((time - last_update_time) / 1000);
      average = 0.95 * average + 0.05 * new_average;
      sum = 0;
    }
  }
  void Add(uint64_t ns)
  {
    sum += ns;
  }
} MovingAverageTimer;

typedef struct ImScopeDataWrapper {
  std::mutex write_mutex;
  bool is_data_ready;
  ImScopeData data;
} ImScopeDataWrapper;

typedef struct IQData {
  std::vector<int16_t> real;
  std::vector<int16_t> imag;
  int16_t max_iq;
  std::vector<float> power;
  float max_power;
  float timestamp;
  int nonzero_count;
  int len = 0;
  metadata meta;
  int scope_id;

  bool TryCollect(ImScopeDataWrapper *imscope_data_wrapper, float time, float epsilon, MovingAverageTimer &iq_procedure_timer)
  {
    if (imscope_data_wrapper->is_data_ready) {
      Collect(&imscope_data_wrapper->data, time, epsilon);
      iq_procedure_timer.Add(imscope_data_wrapper->data.time_taken_in_ns);
      imscope_data_wrapper->is_data_ready = false;
      return true;
    }
    return false;
  }

  void Collect(ImScopeData *scope_data, float time, float epsilon) {
    timestamp = time;
    scopeGraphData_t *iq_header = scope_data->scope_graph_data;
    len = iq_header->lineSz;
    real.resize(len);
    imag.resize(len);
    power.resize(len);
    c16_t *source = (c16_t *)(iq_header + 1);
    max_iq = 0;
    nonzero_count = 0;
    for (auto i = 0; i < len; i++) {
      real[i] = source[i].r;
      imag[i] = source[i].i;
      max_iq = std::max(max_iq, (int16_t)std::abs(source[i].r));
      max_iq = std::max(max_iq, (int16_t)std::abs(source[i].i));
      nonzero_count = std::abs(source[i].r) > epsilon || std::abs(source[i].i) > epsilon ? nonzero_count + 1 : nonzero_count;
      power[i] = std::sqrt(std::pow(source[i].r, 2) + std::pow(source[i].i, 2));
    }
    meta = scope_data->meta;
    scope_id = scope_data->scope_id;
  }
} IQData;

extern ImScopeDataWrapper scope_array[EXTRA_SCOPE_TYPES];

void copyDataThreadSafe(void *scopeData,
                        enum scopeDataType type,
                        void *dataIn,
                        int elementSz,
                        int colSz,
                        int lineSz,
                        int offset,
                        metadata *meta);

bool tryLockScopeData(enum scopeDataType type, int elementSz, int colSz, int lineSz, metadata *meta);

void copyDataUnsafeWithOffset(enum scopeDataType type, void *dataIn, size_t size, size_t offset, int copy_index);

void unlockScopeData(enum scopeDataType type);

void dumpScopeData(int slot, int frame, const char *cause_string);

void *imscope_record_thread(void *data_void_ptr);
void *imscope_thread(void *data_void_ptr);

#endif
