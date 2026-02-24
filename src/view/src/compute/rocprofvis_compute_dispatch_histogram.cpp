// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_dispatch_histogram.h"
#include "rocprofvis_compute_selection.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_settings_manager.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>

namespace RocProfVis
{
namespace View
{

static const float BAR_WIDTH        = 10.0f;
static const float BAR_SPACING      = 2.0f;
static const float TOOLBAR_HEIGHT   = 30.0f;
static const float HISTOGRAM_HEIGHT = 200.0f;

static SettingsManager&
Settings()
{
    return SettingsManager::GetInstance();
}

DispatchHistogramView::DispatchHistogramView(
    DataProvider& data_provider, std::shared_ptr<ComputeSelection> compute_selection)
: m_data_provider(data_provider)
, m_compute_selection(compute_selection)
, m_selected_gpu_id(0)
, m_gpu_ids_dirty(true)
{}

DispatchHistogramView::~DispatchHistogramView() {}

void
DispatchHistogramView::RefreshGpuIds()
{
    m_gpu_ids.clear();
    const auto& dispatch_map = m_data_provider.ComputeModel().GetDispatches();
    for(const auto& [gpu_id, dispatches] : dispatch_map)
    {
        m_gpu_ids.push_back(gpu_id);
    }
    if(!m_gpu_ids.empty())
    {
        bool current_valid = std::find(m_gpu_ids.begin(), m_gpu_ids.end(),
                                       m_selected_gpu_id) != m_gpu_ids.end();
        if(!current_valid)
        {
            m_selected_gpu_id = m_gpu_ids.front();
        }
    }
    m_gpu_ids_dirty = false;
}

void
DispatchHistogramView::Render()
{
    if(m_gpu_ids_dirty)
    {
        RefreshGpuIds();
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, Settings().GetColor(Colors::kBgPanel));
    ImGui::PushStyleColor(ImGuiCol_Border, Settings().GetColor(Colors::kBorderGray));

    ImGui::BeginChild("DispatchHistogram",
                       ImVec2(0, TOOLBAR_HEIGHT + HISTOGRAM_HEIGHT + 10.0f),
                       ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);

    RenderToolbar();
    ImGui::Separator();
    RenderBars();

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
}

void
DispatchHistogramView::RenderToolbar()
{
    uint32_t kernel_id   = m_compute_selection->GetSelectedKernel();
    uint32_t workload_id = m_compute_selection->GetSelectedWorkload();

    ImGui::Text("Dispatch Timeline");
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    ImGui::Text("GPU:");
    ImGui::SameLine();

    for(size_t i = 0; i < m_gpu_ids.size(); i++)
    {
        ImGui::SameLine();
        char label[16];
        snprintf(label, sizeof(label), "%u", m_gpu_ids[i]);
        bool selected = (m_gpu_ids[i] == m_selected_gpu_id);
        if(selected)
        {
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  Settings().GetColor(Colors::kAccentRed));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                  Settings().GetColor(Colors::kAccentRedHover));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                  Settings().GetColor(Colors::kAccentRedActive));
        }
        if(ImGui::SmallButton(label))
        {
            m_selected_gpu_id = m_gpu_ids[i];
        }
        if(selected)
        {
            ImGui::PopStyleColor(3);
        }
    }

    if(kernel_id != ComputeSelection::INVALID_SELECTION_ID &&
       workload_id != ComputeSelection::INVALID_SELECTION_ID)
    {
        const KernelInfo* ki =
            m_data_provider.ComputeModel().GetKernelInfo(workload_id, kernel_id);
        if(ki)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Kernel: %s",
                               ki->name.c_str());
        }
    }
}

void
DispatchHistogramView::RenderBars()
{
    uint32_t kernel_id   = m_compute_selection->GetSelectedKernel();

    if(kernel_id == ComputeSelection::INVALID_SELECTION_ID || m_gpu_ids.empty())
    {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                           "Select a kernel to view dispatch histogram.");
        return;
    }

    const auto* gpu_dispatches =
        m_data_provider.ComputeModel().GetDispatchesByGpu(m_selected_gpu_id);
    if(!gpu_dispatches || gpu_dispatches->empty())
    {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                           "No dispatches for GPU %u.", m_selected_gpu_id);
        return;
    }

    std::vector<const DispatchInfo*> filtered;
    filtered.reserve(gpu_dispatches->size());
    for(const auto& d : *gpu_dispatches)
    {
        if(d.kernel_uuid == kernel_id)
        {
            filtered.push_back(&d);
        }
    }

    if(filtered.empty())
    {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                           "No dispatches for the selected kernel on GPU %u.",
                           m_selected_gpu_id);
        return;
    }

    uint64_t max_duration = 0;
    uint64_t min_duration = UINT64_MAX;
    for(const auto* d : filtered)
    {
        uint64_t dur = d->end_timestamp - d->start_timestamp;
        if(dur > max_duration) max_duration = dur;
        if(dur < min_duration) min_duration = dur;
    }
    if(max_duration == 0)
    {
        max_duration = 1;
    }
    uint64_t duration_range = max_duration - min_duration;
    if(duration_range == 0)
    {
        duration_range = 1;
    }

    static const float MIN_BAR_FRAC = 0.05f;

    float content_width =
        static_cast<float>(filtered.size()) * (BAR_WIDTH + BAR_SPACING) + BAR_SPACING;
    float avail_height = HISTOGRAM_HEIGHT - 4.0f;

    ImGui::BeginChild("dispatch_histogram_bars", ImVec2(0, HISTOGRAM_HEIGHT), false,
                       ImGuiWindowFlags_HorizontalScrollbar);

    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImVec2(std::max(content_width, ImGui::GetContentRegionAvail().x),
                                avail_height);

    ImGui::Dummy(ImVec2(content_width, avail_height));

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImU32 color_a = Settings().GetColor(Colors::kAccentRed);
    ImU32 color_b = Settings().GetColor(Colors::kAccentRedActive);

    int hovered_idx = -1;

    for(size_t i = 0; i < filtered.size(); i++)
    {
        const DispatchInfo* d        = filtered[i];
        uint64_t            duration = d->end_timestamp - d->start_timestamp;
        float norm = static_cast<float>(static_cast<double>(duration - min_duration) /
                                        static_cast<double>(duration_range));
        norm = MIN_BAR_FRAC + norm * (1.0f - MIN_BAR_FRAC);

        float bar_height = norm * avail_height;
        float x0 = canvas_pos.x + BAR_SPACING + i * (BAR_WIDTH + BAR_SPACING);
        float x1 = x0 + BAR_WIDTH;
        float y1 = canvas_pos.y + avail_height;
        float y0 = y1 - bar_height;

        ImU32 color = (i % 2 == 0) ? color_a : color_b;
        draw_list->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), color);

        ImVec2 mouse = ImGui::GetMousePos();
        if(mouse.x >= x0 && mouse.x < x1 && mouse.y >= canvas_pos.y &&
           mouse.y <= canvas_pos.y + avail_height)
        {
            hovered_idx = static_cast<int>(i);
        }
    }

    if(hovered_idx >= 0)
    {
        const DispatchInfo* d        = filtered[hovered_idx];
        uint64_t            duration = d->end_timestamp - d->start_timestamp;

        ImGui::BeginTooltip();
        ImGui::Text("Dispatch #%u", d->dispatch_id);
        ImGui::Text("Duration: %llu ns", static_cast<unsigned long long>(duration));
        ImGui::Text("Start:    %llu", static_cast<unsigned long long>(d->start_timestamp));
        ImGui::Text("End:      %llu", static_cast<unsigned long long>(d->end_timestamp));
        ImGui::Text("GPU:      %u", d->gpu_id);
        ImGui::EndTooltip();
    }

    ImGui::EndChild();
}

}  // namespace View
}  // namespace RocProfVis
