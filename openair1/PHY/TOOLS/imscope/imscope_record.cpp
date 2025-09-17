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
#include "openair1/PHY/TOOLS/phy_scope_interface.h"
#include <sys/stat.h>
#include <errno.h>
#include <atomic>
#include <new>

#ifdef __cpp_lib_hardware_interference_size
    using std::hardware_constructive_interference_size;
    using std::hardware_destructive_interference_size;
#else
    // 64 bytes on x86-64 │ L1_CACHE_BYTES │ L1_CACHE_SHIFT │ __cacheline_aligned │ ...
    constexpr std::size_t hardware_constructive_interference_size = 64;
    constexpr std::size_t hardware_destructive_interference_size = 64;
#endif

#define NUM_CACHED_DATA 8
// Set maximum number of kilobytes written per recording session to 1GB. This is a safeguard against
// recoding thread writing too many files and exhausting disk space.
#define MAX_KBYTES_WRITTEN_PER_SESSION (1000 * 1000)

ImScopeDataWrapper scope_array[EXTRA_SCOPE_TYPES];
typedef struct alignas(hardware_destructive_interference_size) ImScopeDumpInstruction {
  int slot;
  int frame;
  const char *cause;
} ImScopeDumpInstruction;

std::atomic<ImScopeDumpInstruction> dump_instruction;
ImScopeDumpInstruction void_instruction({-1, -1, nullptr});

extern "C" void imscope_record_autoinit(void *dataptr)
{
  AssertFatal(IS_SOFTMODEM_5GUE ||  IS_SOFTMODEM_GNB,
              "Scope cannot find NRUE or GNB context");
  LOG_A(PHY, "Imscope recording started\n");

  for (auto i = 0U; i < EXTRA_SCOPE_TYPES; i++) {
    scope_array[i].is_data_ready = false;
    scope_array[i].data.scope_graph_data = nullptr;
    scope_array[i].data.meta = {-1, -1};
  }

  dump_instruction.exchange(void_instruction);

  scopeData_t *scope = (scopeData_t *)calloc(1, sizeof(scopeData_t));
  scope->copyData = copyDataThreadSafe;
  scope->tryLockScopeData = tryLockScopeData;
  scope->copyDataUnsafeWithOffset = copyDataUnsafeWithOffset;
  scope->unlockScopeData = unlockScopeData;
  scope->copyData = copyDataThreadSafe;
  scope->dumpScopeData = dumpScopeData;
  if (IS_SOFTMODEM_GNB) {
    scopeParms_t *scope_params = (scopeParms_t *)dataptr;
    scope_params->gNB->scopeData = scope;
    scope_params->ru->scopeData = scope;
  } else {
    PHY_VARS_NR_UE *ue = (PHY_VARS_NR_UE *)dataptr;
    ue->scopeData = scope;
  }

  pthread_t thread;
  threadCreate(&thread, imscope_record_thread, dataptr, (char *)"imscope_record", -1, sched_get_priority_min(SCHED_RR));
}

void dumpScopeData(int slot, int frame, const char *cause_string)
{
  ImScopeDumpInstruction to_dump = {slot, frame, cause_string};
  dump_instruction.compare_exchange_weak(void_instruction, to_dump);
}

void *imscope_record_thread(void *data_void_ptr)
{
  const char *path = "imscope-dump";
  struct stat info;
  bool has_dir = false;
  if (stat(path, &info) == 0) {
    if (info.st_mode & S_IFDIR) {
      has_dir = true;
    }
  }
  if (has_dir) {
    LOG_A(PHY, "ImScope recorder thread starts, recording data to %s\n", path);
  } else {
    LOG_A(PHY, "ImScope recorder thread starts, recording data to cwd\n");
  }

  std::string program_name(program_invocation_name);
  size_t last_slash = program_name.find_last_of("/");
  if (last_slash != std::string::npos) {
    program_name = program_name.substr(last_slash + 1);
  }

  ImScopeData scope_cache[EXTRA_SCOPE_TYPES][NUM_CACHED_DATA];
  memset(scope_cache, 0, sizeof(scope_cache));
  uint scope_cache_index[EXTRA_SCOPE_TYPES] = {0};
  float kbytes_written = 0.0f;
  while (kbytes_written < MAX_KBYTES_WRITTEN_PER_SESSION) {
    ImScopeDumpInstruction dump = dump_instruction.load(std::memory_order::memory_order_relaxed);
    if (dump.slot != -1) {
      std::string filename;
      if (has_dir) {
        filename += std::string(path) + "/";
      }
      filename += program_name + std::string("-") + dump.cause + std::string("-");
      filename += std::to_string(dump.frame) + std::string("-") + std::to_string(dump.slot) + std::string(".imscope");
      std::ofstream outf(filename, std::ios_base::out | std::ios_base::binary);
      if (!outf) {
        LOG_E(PHY, "Could not open file %s for writing\n", filename.c_str());
        break;
      }
      for (auto i = 0U; i < EXTRA_SCOPE_TYPES; i++) {
        for (auto j = 0U; j < NUM_CACHED_DATA; j++) {
          ImScopeData &scope_data = scope_cache[i][j];
          if (scope_data.meta.frame == dump.frame && scope_data.meta.slot == dump.slot) {
            scope_data.dump_to_file(outf);
          }
        }
      }
      long pos = outf.tellp();
      kbytes_written += pos / 1000.0f;
      outf.close();
      LOG_D(PHY, "ImScope done writing to %s\n", filename.c_str());
      dump_instruction.exchange(void_instruction);
    }
    for (auto i = 0U; i < EXTRA_SCOPE_TYPES; i++) {
      ImScopeDataWrapper &scope_data = scope_array[i];
      if (scope_data.is_data_ready) {
        uint cache_index = scope_cache_index[i]++ % NUM_CACHED_DATA;
        ImScopeData tmp = scope_cache[i][cache_index];
        scope_cache[i][cache_index] = scope_data.data;
        scope_data.data = tmp;
        scope_data.is_data_ready = false;
      }
    }
  }
  LOG_E(PHY, "ImScope recorder thread exits!\n");
  return nullptr;
}
