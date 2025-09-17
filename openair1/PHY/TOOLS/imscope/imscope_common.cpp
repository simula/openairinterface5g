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

#include "imscope_internal.h"
#include <chrono>

void copyDataThreadSafe(void *scopeData,
                        enum scopeDataType type,
                        void *dataIn,
                        int elementSz,
                        int colSz,
                        int lineSz,
                        int offset,
                        metadata *meta)
{
  ImScopeDataWrapper &scope_data = scope_array[type];

  if (scope_data.is_data_ready) {
    // data is ready, wasn't consumed yet by scope
    return;
  }

  if (scope_data.write_mutex.try_lock()) {
    auto start = std::chrono::high_resolution_clock::now();
    scopeGraphData_t *data = scope_data.data.scope_graph_data;
    int oldDataSz = data ? data->dataSize : 0;
    int newSz = elementSz * colSz * lineSz;
    if (data == NULL || oldDataSz < newSz) {
      free(data);
      scopeGraphData_t *ptr = (scopeGraphData_t *)malloc(sizeof(scopeGraphData_t) + newSz);
      if (!ptr) {
        LOG_E(PHY, "can't realloc\n");
        return;
      } else {
        data = ptr;
      }
    }

    data->elementSz = elementSz;
    data->colSz = colSz;
    data->lineSz = lineSz;
    data->dataSize = newSz;
    memcpy(((void *)(data + 1)), dataIn, newSz);
    scope_data.data.scope_graph_data = data;
    scope_data.data.meta = *meta;
    scope_data.data.time_taken_in_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count();
    scope_data.data.scope_id = type;
    scope_data.is_data_ready = true;
    scope_data.write_mutex.unlock();
  }
}

bool tryLockScopeData(enum scopeDataType type, int elementSz, int colSz, int lineSz, metadata *meta)
{
  ImScopeDataWrapper &scope_data = scope_array[type];

  if (scope_data.is_data_ready) {
    // data is ready, wasn't consumed yet by scope
    return false;
  }

  if (scope_data.write_mutex.try_lock()) {
    auto start = std::chrono::high_resolution_clock::now();
    scopeGraphData_t *data = scope_data.data.scope_graph_data;
    int oldDataSz = data ? data->dataSize : 0;
    int newSz = elementSz * colSz * lineSz;
    if (data == NULL || oldDataSz < newSz) {
      free(data);
      scopeGraphData_t *ptr = (scopeGraphData_t *)malloc(sizeof(scopeGraphData_t) + newSz);
      if (!ptr) {
        LOG_E(PHY, "can't realloc\n");
        return false;
      } else {
        data = ptr;
      }
    }

    data->elementSz = elementSz;
    data->colSz = colSz;
    data->lineSz = lineSz;
    data->dataSize = newSz;
    scope_data.data.scope_graph_data = data;
    scope_data.data.meta = *meta;
    scope_data.data.time_taken_in_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count();
    memset(scope_data.data.time_taken_in_ns_per_offset, 0, sizeof(scope_data.data.time_taken_in_ns_per_offset));
    memset(scope_data.data.data_copied_per_offset, 0, sizeof(scope_data.data.data_copied_per_offset));
    return true;
  }
  return false;
}

void copyDataUnsafeWithOffset(enum scopeDataType type, void *dataIn, size_t size, size_t offset, int copy_index)
{
  AssertFatal(copy_index < MAX_OFFSETS, "Unexpected number of copies per sink. copy_index = %d\n", copy_index);
  ImScopeDataWrapper &scope_data = scope_array[type];
  auto start = std::chrono::high_resolution_clock::now();
  scopeGraphData_t *data = scope_data.data.scope_graph_data;
  uint8_t *outptr = (uint8_t *)(data + 1);
  memcpy(&outptr[offset], dataIn, size);
  scope_data.data.time_taken_in_ns_per_offset[copy_index] =
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count();
  scope_data.data.data_copied_per_offset[copy_index] = size;
}

void unlockScopeData(enum scopeDataType type)
{
  ImScopeDataWrapper &scope_data = scope_array[type];
  size_t total_size = 0;
  for (auto i = 0; i < MAX_OFFSETS; i++) {
    scope_data.data.time_taken_in_ns += scope_data.data.time_taken_in_ns_per_offset[i];
    total_size += scope_data.data.data_copied_per_offset[i];
  }
  if (total_size != (uint64_t)scope_data.data.scope_graph_data->dataSize) {
    scope_data.data.scope_graph_data->dataSize = total_size;
    scope_data.data.scope_graph_data->lineSz =
        total_size / scope_data.data.scope_graph_data->elementSz / scope_data.data.scope_graph_data->colSz;
  }
  scope_data.data.scope_id = type;
  scope_data.is_data_ready = true;
  scope_data.write_mutex.unlock();
}

const char *scope_id_to_string(scopeDataType type) {
  switch (type) {
    case pbchDlChEstimateTime:
      return "pbchDlChEstimateTime";
    case pbchLlr:
      return "pbchLlr";
    case pbchRxdataF_comp:
      return "pbchRxdataF_comp";
    case pdcchLlr:
      return "pdcchLlr";
    case pdcchRxdataF_comp:
      return "pdcchRxdataF_comp";
    case pdschLlr:
      return "pdschLlr";
    case pdschRxdataF_comp:
      return "pdschRxdataF_comp";
    case commonRxdataF:
      return "commonRxdataF";
    case gNBRxdataF:
      return "gNBRxdataF";
    case psbchDlChEstimateTime:
      return "psbchDlChEstimateTime";
    case psbchLlr:
      return "psbchLlr";
    case psbchRxdataF_comp:
      return "psbchRxdataF_comp";
    case gNBPuschRxIq:
      return "gNBPuschRxIq";
    case gNBPuschLlr:
      return "gNBPuschLlr";
    case ueTimeDomainSamples:
      return "ueTimeDomainSamples";
    case ueTimeDomainSamplesBeforeSync:
      return "ueTimeDomainSamplesBeforeSync";
    case gNbTimeDomainSamples:
      return "gNbTimeDomainSamples";
    case pdschChanEstimates:
      return "pdschChanEstimates";
    case pdschRxdataF:
      return "pdschRxdataF";
    default:
      return "unknown scope type";
  }
}

std::ostream &operator<<(std::ostream &os, const ImScopeData &dt)
{
  os << "dataSize: " << dt.scope_graph_data->dataSize << " elementSz: " << dt.scope_graph_data->elementSz
     << " colSz: " << dt.scope_graph_data->colSz << " lineSz: " << dt.scope_graph_data->lineSz;
  os << " meta: " << dt.meta.frame << "." << dt.meta.slot
     << " scope_id: " << scope_id_to_string(static_cast<scopeDataType>(dt.scope_id));
  return os;
}
