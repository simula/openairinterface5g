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

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include "implot.h"
#include "openair1/PHY/defs_nr_UE.h"
extern "C" {
#include "openair1/PHY/TOOLS/phy_scope_interface.h"
uint64_t get_softmodem_optmask(void);
}
#include <iostream>
#include <vector>
#include <limits>
#include <algorithm>
#include <sstream>
#include <mutex>
#include <thread>
#include "executables/softmodem-bits.h"

#define MAX_OFFSETS 14
#define NR_MAX_RB 273
#define N_SC_PER_RB NR_NB_SC_PER_RB

static std::vector<int> rb_boundaries;

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

static void glfw_error_callback(int error, const char *description)
{
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

typedef struct ImScopeData {
  std::mutex write_mutex;
  scopeGraphData_t *scope_graph_data;
  bool is_data_ready;
  metadata meta;
  uint64_t time_taken_in_ns;
  uint64_t time_taken_in_ns_per_offset[MAX_OFFSETS];
  size_t data_copied_per_offset[MAX_OFFSETS];
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

MovingAverageTimer iq_procedure_timer;

static ImScopeData scope_array[EXTRA_SCOPE_TYPES];

typedef struct IQData {
  std::vector<int16_t> real;
  std::vector<int16_t> imag;
  int16_t max_iq;
  std::vector<float> power;
  float max_power;
  float timestamp;
  int nonzero_count;
  int len;
  metadata meta;

  bool TryCollect(ImScopeData *imscope_data, float time, float epsilon)
  {
    if (imscope_data->is_data_ready) {
      iq_procedure_timer.Add(imscope_data->time_taken_in_ns);
      timestamp = time;
      scopeGraphData_t *iq_header = imscope_data->scope_graph_data;
      len = iq_header->lineSz;
      real.reserve(len);
      imag.reserve(len);
      power.reserve(len);
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
      meta = imscope_data->meta;
      imscope_data->is_data_ready = false;
      return true;
    }
    return false;
  }
} IQData;

class LLRPlot {
  int len = 0;
  float timestamp = 0;
  std::vector<int16_t> llr;
  bool frozen = false;
  bool next = false;
  metadata meta;

 public:
  void Draw(float time, enum scopeDataType type, const char *label)
  {
    ImGui::BeginGroup();
    if (ImGui::Button(frozen ? "Unfreeze" : "Freeze")) {
      frozen = !frozen;
      next = false;
    }
    if (frozen) {
      ImGui::SameLine();
      ImGui::BeginDisabled(next);
      if (ImGui::Button("Load next histogram")) {
        next = true;
      }
      ImGui::EndDisabled();
    }

    ImScopeData &scope_data = scope_array[type];
    if (ImPlot::BeginPlot(label)) {
      if (!frozen || next) {
        if (scope_data.is_data_ready) {
          iq_procedure_timer.Add(scope_data.time_taken_in_ns);
          timestamp = time;
          const int16_t *tmp = (int16_t *)(scope_data.scope_graph_data + 1);
          len = scope_data.scope_graph_data->lineSz;
          llr.reserve(len);
          for (auto i = 0; i < len; i++) {
            llr[i] = tmp[i];
          }
          meta = scope_data.meta;
          scope_data.is_data_ready = false;
          if (frozen) {
            next = false;
          }
        }
      }

      ImPlot::PlotLine(label, llr.data(), len);
      ImPlot::EndPlot();
    }
    std::stringstream ss;
    if (meta.slot != -1) {
      ss << " slot: " << meta.slot;
    }
    if (meta.frame != -1) {
      ss << " frame: " << meta.frame;
    }
    if (!ss.str().empty()) {
      ImGui::Text("Data for %s", ss.str().c_str());
    }
    ImGui::Text("Data is %.2f seconds old", time - timestamp);
    ImGui::EndGroup();
  }
};

class IQHist {
 private:
  bool frozen = false;
  bool next = false;
  float range = 100;
  int num_bins = 33;
  std::string label;
  float min_nonzero_percentage = 0.9;
  float epsilon = 0.0;
  bool auto_adjust_range = true;
  int plot_type = 0;

 public:
  IQHist(const char *label_)
  {
    label = label_;
  };
  bool ShouldReadData(void)
  {
    return !frozen || next;
  }
  float GetEpsilon(void)
  {
    return epsilon;
  }
  void Draw(IQData *iq_data, float time, bool new_data)
  {
    if (new_data && frozen && next) {
      // Evaluate if new data matches filter settings
      if (((float)iq_data->nonzero_count / (float)iq_data->len) > min_nonzero_percentage) {
        next = false;
      }
    }
    ImGui::BeginGroup();
    ImGui::Checkbox("auto adjust range", &auto_adjust_range);
    if (auto_adjust_range) {
      if (range < iq_data->max_iq * 1.1) {
        range = iq_data->max_iq * 1.1;
      }
    }
    ImGui::BeginDisabled(auto_adjust_range);
    ImGui::SameLine();
    ImGui::DragFloat("Range", &range, 1, 0.1, std::numeric_limits<int16_t>::max());
    ImGui::EndDisabled();

    ImGui::DragInt("Number of bins", &num_bins, 1, 33, 101);
    if (ImGui::Button(frozen ? "Unfreeze" : "Freeze")) {
      frozen = !frozen;
      next = false;
    }

    if (frozen) {
      ImGui::SameLine();
      ImGui::BeginDisabled(next);
      if (ImGui::Button("Load next histogram")) {
        next = true;
      }
      ImGui::EndDisabled();
      ImGui::Text("Filter parameters");
      ImGui::DragFloat("%% nonzero elements", &min_nonzero_percentage, 1, 0.0, 100);
      ImGui::DragFloat("epsilon", &epsilon, 1, 0.0, 3000);
    }
    const char *items[] = {"Histogram", "Scatter", "RMS"};
    ImGui::Combo("Select plot type", &plot_type, items, sizeof(items) / sizeof(items[0]));
    if (plot_type == 0) {
      if (ImPlot::BeginPlot(label.c_str(), {(float)ImGui::GetWindowWidth() * 0.3f, (float)ImGui::GetWindowWidth() * 0.3f})) {
        ImPlot::PlotHistogram2D(label.c_str(),
                                iq_data->real.data(),
                                iq_data->imag.data(),
                                iq_data->len,
                                num_bins,
                                num_bins,
                                ImPlotRect(-range, range, -range, range));
        ImPlot::EndPlot();
      }
    } else if (plot_type == 1) {
      if (ImPlot::BeginPlot(label.c_str(), {(float)ImGui::GetWindowWidth() * 0.3f, (float)ImGui::GetWindowWidth() * 0.3f})) {
        int points_drawn = 0;
        while (points_drawn < iq_data->len) {
          // Limit the amount of data plotted with PlotScatter call (issue with vertices/draw call)
          int points_to_draw = std::min(iq_data->len - points_drawn, 16000);
          ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 1, IMPLOT_AUTO_COL, 1);
          ImPlot::PlotScatter(label.c_str(),
                              iq_data->real.data() + points_drawn,
                              iq_data->imag.data() + points_drawn,
                              points_to_draw);
          points_drawn += points_to_draw;
        }
        ImPlot::EndPlot();
      }
    } else if (plot_type == 2) {
      if (ImPlot::BeginPlot(label.c_str())) {
        ImPlot::PlotLine(label.c_str(), iq_data->power.data(), iq_data->len);
        ImPlot::EndPlot();
      }
    }
    ImGui::Text("Maximum value = %d, nonzero elements/total %d/%d", iq_data->max_iq, iq_data->nonzero_count, iq_data->len);
    ImGui::Text("Data is %.2f seconds old", time - iq_data->timestamp);
    std::stringstream ss;
    if (iq_data->meta.slot != -1) {
      ss << " slot: " << iq_data->meta.slot;
    }
    if (iq_data->meta.frame != -1) {
      ss << " frame: " << iq_data->meta.frame;
    }
    if (!ss.str().empty()) {
      ImGui::Text("Data for %s", ss.str().c_str());
    }
    ImGui::EndGroup();
  }
};

class IQSlotHeatmap {
 private:
  bool frozen = false;
  bool next = false;
  float timestamp = 0;
  std::vector<float> power;
  ImScopeData *scope_data;
  std::string label;
  int len = 0;
  float max = 0;
  float stop_at_min = 1000;

 public:
  IQSlotHeatmap(ImScopeData *scope_data_, const char *label_)
  {
    scope_data = scope_data_;
    label = label_;
  };
  // Read in the data from the sink and transform it for the use by the scope
  void ReadData(float time, int ofdm_symbol_size, int num_symbols, int first_carrier_offset, int num_rb)
  {
    auto num_sc = num_rb * NR_NB_SC_PER_RB;
    if (!frozen || next) {
      if (scope_data->is_data_ready) {
        iq_procedure_timer.Add(scope_data->time_taken_in_ns);
        uint16_t first_sc = first_carrier_offset;
        uint16_t last_sc = first_sc + num_rb * NR_NB_SC_PER_RB;
        bool wrapped = false;
        uint16_t wrapped_first_sc = 0;
        uint16_t wrapped_last_sc = 0;
        if (last_sc >= ofdm_symbol_size) {
          last_sc = ofdm_symbol_size - 1;
          wrapped = true;
          auto num_sc_left = num_sc - (last_sc - first_sc + 1);
          wrapped_last_sc = wrapped_first_sc + num_sc_left - 1;
        }
        timestamp = time;
        scopeGraphData_t *iq_header = scope_data->scope_graph_data;
        len = iq_header->lineSz;
        c16_t *source = (c16_t *)(iq_header + 1);

        power.reserve(num_sc * num_symbols);
        for (auto symbol = 0; symbol < num_symbols; symbol++) {
          int subcarrier = 0;
          for (auto sc = first_sc; sc <= last_sc; sc++) {
            auto source_index = sc + symbol * ofdm_symbol_size;
            power[subcarrier * num_symbols + symbol] = std::pow(source[source_index].r, 2) + std::pow(source[source_index].i, 2);
            subcarrier++;
          }
          if (wrapped) {
            for (auto sc = wrapped_first_sc; sc <= wrapped_last_sc; sc++) {
              auto source_index = sc + symbol * ofdm_symbol_size;
              power[subcarrier * num_symbols + symbol] = std::pow(source[source_index].r, 2) + std::pow(source[source_index].i, 2);
              subcarrier++;
            }
          }
        }
        max = *std::max_element(power.begin(), power.end());
        if (frozen && max > stop_at_min) {
          next = false;
        }
        scope_data->is_data_ready = false;
      }
    }
  }
  void Draw(float time, int ofdm_symbol_size, int num_symbols, int first_carrier_offset, int num_rb)
  {
    ReadData(time, ofdm_symbol_size, num_symbols, first_carrier_offset, num_rb);
    ImGui::BeginGroup();
    if (ImGui::Button(frozen ? "Unfreeze" : "Freeze")) {
      frozen = !frozen;
      next = false;
    }
    if (frozen) {
      ImGui::SameLine();
      ImGui::BeginDisabled(next);
      if (ImGui::Button("Load next data")) {
        next = true;
      }
      ImGui::EndDisabled();
      ImGui::Text("Filter parameters:");
      ImGui::InputFloat("Max Power minimum", &stop_at_min, 10, 100);
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Data with maximum power below that value will be discarded.");
      }
    }
    static std::vector<int> symbol_boundaries = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
    if (ImPlot::BeginPlot(label.c_str(), {(float)ImGui::GetWindowWidth() * 0.9f, 0})) {
      auto num_sc = num_rb * NR_NB_SC_PER_RB;
      ImPlot::SetupAxes("symbols", "subcarriers");
      ImPlot::SetupAxisLimits(ImAxis_X1, num_symbols, 0);
      ImPlot::SetupAxisLimits(ImAxis_Y1, num_sc, 0);
      ImPlot::PlotHeatmap(label.c_str(),
                          power.data(),
                          num_sc,
                          num_symbols,
                          0,
                          max,
                          nullptr,
                          {0, 0},
                          {(double)num_symbols, (double)num_sc});
      ImPlot::PlotInfLines("Symbol boundary", symbol_boundaries.data(), symbol_boundaries.size());
      ImPlot::PlotInfLines("RB boundary", rb_boundaries.data(), num_rb, ImPlotInfLinesFlags_Horizontal);
      ImPlot::EndPlot();
    }
    ImGui::SameLine();
    ImPlot::ColormapScale("##HeatScale", 0, max);
    ImGui::Text("Data is %.2f seconds old", time - timestamp);
    ImGui::EndGroup();
  }
};

// utility structure for realtime plot
struct ScrollingBuffer {
  int MaxSize;
  int Offset;
  ImVector<ImVec2> Data;
  ScrollingBuffer(int max_size = 2000)
  {
    MaxSize = max_size;
    Offset = 0;
    Data.reserve(MaxSize);
  }
  void AddPoint(float x, float y)
  {
    if (Data.size() < MaxSize)
      Data.push_back(ImVec2(x, y));
    else {
      Data[Offset] = ImVec2(x, y);
      Offset = (Offset + 1) % MaxSize;
    }
  }
  void Erase()
  {
    if (Data.size() > 0) {
      Data.shrink(0);
      Offset = 0;
    }
  }
};

void ShowUeScope(PHY_VARS_NR_UE *ue, float t)
{
  if (ImPlot::BeginPlot("##Scrolling", ImVec2(-1, 150))) {
    static float history = 10.0f;
    ImGui::SliderFloat("History", &history, 1, 30, "%.1f s");
    static ScrollingBuffer rbs_buffer;
    static ScrollingBuffer bler;
    static ScrollingBuffer mcs;
    rbs_buffer.AddPoint(t, getKPIUE()->nofRBs);
    bler.AddPoint(t, (float)getKPIUE()->nb_nack / (float)getKPIUE()->nb_total);
    mcs.AddPoint(t, (float)getKPIUE()->dl_mcs);
    ImPlot::SetupAxes("time", "noOfRbs");
    ImPlot::SetupAxisLimits(ImAxis_X1, t - history, t, ImGuiCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, 0, NR_MAX_RB);
    ImPlot::SetupAxis(ImAxis_Y2, "bler [%]", ImPlotAxisFlags_AuxDefault);
    ImPlot::SetupAxis(ImAxis_Y3, "MCS", ImPlotAxisFlags_AuxDefault);
    ImPlot::SetAxes(ImAxis_X1, ImAxis_Y1);
    ImPlot::PlotLine("noOfRbs", &rbs_buffer.Data[0].x, &rbs_buffer.Data[0].y, rbs_buffer.Data.size(), 0, 0, 2 * sizeof(float));
    ImPlot::SetAxes(ImAxis_X1, ImAxis_Y2);
    ImPlot::PlotLine("bler", &bler.Data[0].x, &bler.Data[0].y, bler.Data.size(), 0, 0, 2 * sizeof(float));
    ImPlot::SetAxes(ImAxis_X1, ImAxis_Y3);
    ImPlot::PlotLine("mcs", &mcs.Data[0].x, &mcs.Data[0].y, mcs.Data.size(), 0, 0, 2 * sizeof(float));
    ImPlot::EndPlot();
  }
  if (ImGui::TreeNode("PDSCH IQ")) {
    static auto iq_data = new IQData();
    static auto pdsch_iq_hist = new IQHist("PDSCH IQ");
    bool new_data = false;
    if (pdsch_iq_hist->ShouldReadData()) {
      new_data = iq_data->TryCollect(&scope_array[pdschRxdataF_comp], t, pdsch_iq_hist->GetEpsilon());
    }
    pdsch_iq_hist->Draw(iq_data, t, new_data);
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("Time domain samples")) {
    static auto iq_data = new IQData();
    static auto time_domain_iq = new IQHist("Time domain samples");
    bool new_data = false;
    if (time_domain_iq->ShouldReadData()) {
      new_data = iq_data->TryCollect(&scope_array[ueTimeDomainSamples], t, time_domain_iq->GetEpsilon());
    }
    time_domain_iq->Draw(iq_data, t, new_data);
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("Time domain samples - before sync")) {
    static auto iq_data = new IQData();
    static auto time_domain_iq = new IQHist("Time domain samples - before sync");
    bool new_data = false;
    if (time_domain_iq->ShouldReadData()) {
      new_data = iq_data->TryCollect(&scope_array[ueTimeDomainSamplesBeforeSync], t, time_domain_iq->GetEpsilon());
    }
    time_domain_iq->Draw(iq_data, t, new_data);
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("Broadcast channel")) {
    ImGui::Text("RSRP %d", ue->measurements.ssb_rsrp_dBm[ue->frame_parms.ssb_index]);
    if (ImGui::TreeNode("IQ")) {
      static auto iq_data = new IQData();
      static auto broadcast_iq_hist = new IQHist("Broadcast IQ");
      bool new_data = false;
      if (broadcast_iq_hist->ShouldReadData()) {
        new_data = iq_data->TryCollect(&scope_array[ue->sl_mode ? psbchRxdataF_comp : pbchRxdataF_comp],
                                       t,
                                       broadcast_iq_hist->GetEpsilon());
      }
      broadcast_iq_hist->Draw(iq_data, t, new_data);
      ImGui::TreePop();
    }
    if (ImGui::TreeNode("CHest")) {
      static auto chest_iq_data = new IQData();
      static auto broadcast_iq_chest = new IQHist("Broadcast Chest");
      bool new_data = false;
      if (broadcast_iq_chest->ShouldReadData()) {
        new_data = chest_iq_data->TryCollect(&scope_array[ue->sl_mode ? psbchDlChEstimateTime : pbchDlChEstimateTime],
                                             t,
                                             broadcast_iq_chest->GetEpsilon());
      }
      broadcast_iq_chest->Draw(chest_iq_data, t, new_data);
      ImGui::TreePop();
    }

    if (ImGui::TreeNode("LLR")) {
      static auto llr_plot = new LLRPlot();
      llr_plot->Draw(t, ue->sl_mode ? psbchLlr : pbchLlr, "Broadcast LLR");
      ImGui::TreePop();
    }
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("RX IQ")) {
    static auto common_rx_iq_heatmap = new IQSlotHeatmap(&scope_array[commonRxdataF], "common RX IQ");
    common_rx_iq_heatmap->Draw(t,
                               ue->frame_parms.ofdm_symbol_size,
                               ue->frame_parms.symbols_per_slot,
                               ue->frame_parms.first_carrier_offset,
                               ue->frame_parms.N_RB_DL);
    ImGui::TreePop();
  }
}

void ShowGnbScope(PHY_VARS_gNB *gNB, float t)
{
  if (ImGui::TreeNode("RX IQ")) {
    static auto gnb_heatmap = new IQSlotHeatmap(&scope_array[gNBRxdataF], "common RX IQ");

    gnb_heatmap->Draw(t,
                      gNB->frame_parms.ofdm_symbol_size,
                      gNB->frame_parms.symbols_per_slot,
                      gNB->frame_parms.first_carrier_offset,
                      gNB->frame_parms.N_RB_UL);
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("PUSCH SLOT IQ")) {
    static auto pusch_iq = new IQData();
    static auto pusch_iq_display = new IQHist("PUSCH compensated IQ");
    bool new_data = false;
    if (pusch_iq_display->ShouldReadData()) {
      new_data = pusch_iq->TryCollect(&scope_array[gNBPuschRxIq], t, pusch_iq_display->GetEpsilon());
    }
    pusch_iq_display->Draw(pusch_iq, t, new_data);
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("PUSCH LLRs")) {
    static auto pusch_llr_plot = new LLRPlot();
    pusch_llr_plot->Draw(t, gNBPuschLlr, "PUSCH LLR");
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("Time domain samples")) {
    static auto iq_data = new IQData();
    static auto time_domain_iq = new IQHist("Time domain samples");
    bool new_data = false;
    if (time_domain_iq->ShouldReadData()) {
      new_data = iq_data->TryCollect(&scope_array[gNbTimeDomainSamples], t, time_domain_iq->GetEpsilon());
    }
    time_domain_iq->Draw(iq_data, t, new_data);
    ImGui::TreePop();
  }
}

void *imscope_thread(void *data_void_ptr)
{
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit())
    return nullptr;

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
  // GL ES 2.0 + GLSL 100
  const char *glsl_version = "#version 100";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
  // GL 3.2 + GLSL 150
  const char *glsl_version = "#version 150";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required on Mac
#else
  // GL 3.0 + GLSL 130
  const char *glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
  // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

  // Create window with graphics context
  GLFWwindow *window = glfwCreateWindow(1280, 720, "imscope", nullptr, nullptr);
  if (window == nullptr)
    return nullptr;
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // For frame capping

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  // ImGui::StyleColorsLight();

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
  ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
  ImGui_ImplOpenGL3_Init(glsl_version);

  // Our state
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  for (auto i = 0U; i < NR_MAX_RB; i++) {
    rb_boundaries.push_back(i * NR_NB_SC_PER_RB);
  }

  static double last_frame_time = glfwGetTime();
  static int target_fps = 24;

  bool is_ue = (get_softmodem_optmask() & SOFTMODEM_5GUE_BIT) > 0;
  while (!glfwWindowShouldClose(window)) {
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy
    // of the mouse data.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your
    // copy of the keyboard data. Generally you may always pass all inputs to dear imgui, and hide them from your application based
    // on those two flags.
    glfwPollEvents();

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);

    static float t = 0;

    t += ImGui::GetIO().DeltaTime;
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({(float)display_w, (float)display_h});
    ImGui::Begin("NR KPI", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    if (ImGui::TreeNode("Global settings")) {
      ImGui::ShowFontSelector("Font");
      ImGui::ShowStyleSelector("ImGui Style");
      ImPlot::ShowStyleSelector("ImPlot Style");
      ImPlot::ShowColormapSelector("ImPlot Colormap");
      ImGui::SliderInt("FPS target", &target_fps, 12, 60);
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Reduces scope flickering in unfrozen mode. Can reduce impact on perfromance of the modem");
      }
      ImGui::TreePop();
    }
    iq_procedure_timer.UpdateAverage(t);
    ImGui::Text("Total time used by IQ capture procedures per milisecond: %.2f [us]/[ms]", iq_procedure_timer.average / 1000);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Total time used in PHY threads for copying out IQ data for the scope, in uS, averaged over 1 ms");
    }

    if (is_ue) {
      PHY_VARS_NR_UE *ue = (PHY_VARS_NR_UE *)data_void_ptr;
      ShowUeScope(ue, t);
    } else {
      scopeParms_t *scope_params = (scopeParms_t *)data_void_ptr;
      PHY_VARS_gNB *gNB = scope_params->gNB;
      ShowGnbScope(gNB, t);
    }
    ImGui::End();

    // For reference
    ImPlot::ShowDemoWindow();
    ImGui::ShowDemoWindow();

    // Rendering
    ImGui::Render();
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    double target_frame_time = 1.0 / target_fps;
    double delta = glfwGetTime() - last_frame_time;
    if (delta < target_frame_time) {
      std::this_thread::sleep_for(std::chrono::duration<float>(target_frame_time - delta));
    }

    glfwSwapBuffers(window);
    last_frame_time = glfwGetTime();
  }

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  return nullptr;
}

extern "C" void imscope_autoinit(void *dataptr)
{
  AssertFatal((get_softmodem_optmask() & SOFTMODEM_5GUE_BIT) || (get_softmodem_optmask() & SOFTMODEM_GNB_BIT),
              "Scope cannot find NRUE or GNB context");

  for (auto i = 0U; i < EXTRA_SCOPE_TYPES; i++) {
    scope_array[i].is_data_ready = false;
    scope_array[i].scope_graph_data = nullptr;
    scope_array[i].meta = {-1, -1};
  }

  if (SOFTMODEM_GNB_BIT & get_softmodem_optmask()) {
    scopeParms_t *scope_params = (scopeParms_t *)dataptr;
    scopeData_t *scope = (scopeData_t *)calloc(1, sizeof(scopeData_t));
    scope->copyData = copyDataThreadSafe;
    scope->tryLockScopeData = tryLockScopeData;
    scope->copyDataUnsafeWithOffset = copyDataUnsafeWithOffset;
    scope->unlockScopeData = unlockScopeData;
    scope_params->gNB->scopeData = scope;
    scope_params->ru->scopeData = scope;
  } else {
    PHY_VARS_NR_UE *ue = (PHY_VARS_NR_UE *)dataptr;
    scopeData_t *scope = (scopeData_t *)calloc(1, sizeof(scopeData_t));
    scope->copyData = copyDataThreadSafe;
    ue->scopeData = scope;
  }
  pthread_t thread;
  threadCreate(&thread, imscope_thread, dataptr, (char *)"imscope", -1, sched_get_priority_min(SCHED_RR));
}

void copyDataThreadSafe(void *scopeData,
                        enum scopeDataType type,
                        void *dataIn,
                        int elementSz,
                        int colSz,
                        int lineSz,
                        int offset,
                        metadata *meta)
{
  ImScopeData &scope_data = scope_array[type];

  if (scope_data.is_data_ready) {
    // data is ready, wasn't consumed yet by scope
    return;
  }

  if (scope_data.write_mutex.try_lock()) {
    auto start = std::chrono::high_resolution_clock::now();
    scopeGraphData_t *data = scope_data.scope_graph_data;
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
    scope_data.scope_graph_data = data;
    scope_data.meta = *meta;
    scope_data.time_taken_in_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count();
    scope_data.is_data_ready = true;
    scope_data.write_mutex.unlock();
  }
}

bool tryLockScopeData(enum scopeDataType type, int elementSz, int colSz, int lineSz, metadata *meta)
{
  ImScopeData &scope_data = scope_array[type];

  if (scope_data.is_data_ready) {
    // data is ready, wasn't consumed yet by scope
    return false;
  }

  if (scope_data.write_mutex.try_lock()) {
    auto start = std::chrono::high_resolution_clock::now();
    scopeGraphData_t *data = scope_data.scope_graph_data;
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
    scope_data.scope_graph_data = data;
    scope_data.meta = *meta;
    scope_data.time_taken_in_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count();
    memset(scope_data.time_taken_in_ns_per_offset, 0, sizeof(scope_data.time_taken_in_ns_per_offset));
    memset(scope_data.data_copied_per_offset, 0, sizeof(scope_data.data_copied_per_offset));
    return true;
  }
  return false;
}

void copyDataUnsafeWithOffset(enum scopeDataType type, void *dataIn, size_t size, size_t offset, int copy_index)
{
  AssertFatal(copy_index < MAX_OFFSETS, "Unexpected number of copies per sink. copy_index = %d\n", copy_index);
  ImScopeData &scope_data = scope_array[type];
  auto start = std::chrono::high_resolution_clock::now();
  scopeGraphData_t *data = scope_data.scope_graph_data;
  uint8_t *outptr = (uint8_t *)(data + 1);
  memcpy(&outptr[offset], dataIn, size);
  scope_data.time_taken_in_ns_per_offset[copy_index] =
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count();
  scope_data.data_copied_per_offset[copy_index] = size;
}

void unlockScopeData(enum scopeDataType type)
{
  ImScopeData &scope_data = scope_array[type];
  size_t total_size = 0;
  for (auto i = 0; i < MAX_OFFSETS; i++) {
    scope_data.time_taken_in_ns += scope_data.time_taken_in_ns_per_offset[i];
    total_size += scope_data.data_copied_per_offset[i];
  }
  if (total_size != (uint64_t)scope_data.scope_graph_data->dataSize) {
    LOG_E(PHY, "Scope is missing data - not all data that was expected was copied - possibly missed copyDataUnsafeWithOffset call\n");
  }
  scope_data.is_data_ready = true;
  scope_data.write_mutex.unlock();
}
