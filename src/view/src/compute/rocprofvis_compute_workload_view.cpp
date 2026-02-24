// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_workload_view.h"
#include "imgui.h"
#include "model/compute/rocprofvis_compute_data_model.h"
#include "rocprofvis_compute_selection.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_events.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_split_containers.h"

namespace RocProfVis
{
namespace View
{

ComputeWorkloadView::ComputeWorkloadView(
    DataProvider& data_provider, std::shared_ptr<ComputeSelection> compute_selection)
: RocWidget()
, m_data_provider(data_provider)
, m_compute_selection(compute_selection)
, m_workload_info(nullptr)
{
    CreateLayout();
}

ComputeWorkloadView::~ComputeWorkloadView() {}

void
ComputeWorkloadView::Update()
{}

void
ComputeWorkloadView::CreateLayout()
{
    auto            default_style = SettingsManager::GetInstance().GetDefaultStyle();
    LayoutItem::Ptr left          = std::make_shared<LayoutItem>();
    left->m_item                  = std::make_shared<RocCustomWidget>([this]() {
        if(m_workload_info) this->RenderSystemInfo(*m_workload_info);
    });
    left->m_window_padding        = default_style.WindowPadding;
    left->m_child_flags =
        ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding;

    LayoutItem::Ptr right   = std::make_shared<LayoutItem>();
    right->m_item           = std::make_shared<RocCustomWidget>([this]() {
        if(m_workload_info) this->RenderProfilingConfig(*m_workload_info);
    });
    right->m_window_padding = default_style.WindowPadding;
    right->m_child_flags =
        ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding;

    m_content_container = std::make_unique<HSplitContainer>(left, right);
    m_content_container->SetMinLeftWidth(10.0f);
    m_content_container->SetMinRightWidth(10.0f);
    m_content_container->SetSplit(0.5f);
}
void
ComputeWorkloadView::Render()
{
    uint32_t workload_id = m_compute_selection->GetSelectedWorkload();
    m_workload_info      = m_data_provider.ComputeModel().GetWorkload(workload_id);

    const char* title_label = "Workload Information";
    CenterNextTextItem(title_label);
    // use larger font for workload name
    SettingsManager& settings   = SettingsManager::GetInstance();
    ImFont*          large_font = settings.GetFontManager().GetFont(FontType::kLarge);
    ImGui::PushFont(large_font);
    ImGui::TextUnformatted(title_label);
    ImGui::PopFont();

    if(m_workload_info)
    {
        if(ImGui::BeginChild("info", ImVec2(0, 0),
                             ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY))
        {
            if(m_content_container)
            {
                m_content_container->Render();
            }
        }
        ImGui::EndChild();
    }
    else
    {
        const char* label = "Workload Information Unavailable";
        RenderUnavailableMessage(label);
    }
}

void
ComputeWorkloadView::RenderUnavailableMessage(const char* label)
{
    ImGui::Separator();
    ImGui::NewLine();

    ImGui::BeginDisabled();
    CenterNextTextItem(label);
    ImGui::TextUnformatted(label);
    ImGui::EndDisabled();
}

void
ComputeWorkloadView::RenderSystemInfo(const WorkloadInfo& workload_info)
{
    if(ImGui::BeginChild("sys_info", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AutoResizeY))
    {
        ImGui::Text("System Information");

        // Check if system info is available and valid before rendering
        // Validity check: Ensure there are exactly 2 columns and both columns have the
        // same number of rows
        if(workload_info.system_info.size() == 2 &&
           workload_info.system_info[0].size() > 0 &&
           workload_info.system_info[0].size() == workload_info.system_info[1].size())
        {
            if(ImGui::BeginTable("", static_cast<int>(workload_info.system_info.size()),
                                 ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders))
            {
                for(size_t i = 0; i < workload_info.system_info[0].size(); i++)
                {
                    ImGui::PushID(i);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    CopyableTextUnformatted(workload_info.system_info[0][i].c_str(), "",
                                            COPY_DATA_NOTIFICATION, false, true);

                    ImGui::TableNextColumn();
                    CopyableTextUnformatted(workload_info.system_info[1][i].c_str(), "",
                                            COPY_DATA_NOTIFICATION, false, true);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }
        else
        {
            const char* label = "System Information Unavailable";
            RenderUnavailableMessage(label);
        }
    }
    ImGui::EndChild();
}

void
ComputeWorkloadView::RenderProfilingConfig(const WorkloadInfo& workload_info)
{
    ImGui::SameLine();
    if(ImGui::BeginChild("config", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY))
    {
        // Check if profiling config is available and valid before rendering
        // Validity check: Ensure there are exactly 2 columns and both columns have the
        // same number of rows
        if(workload_info.profiling_config.size() == 2 &&
           workload_info.profiling_config[0].size() > 0 &&
           workload_info.profiling_config[0].size() ==
               workload_info.profiling_config[1].size())
        {
            ImGui::Text("Profiling Configuration");
            if(ImGui::BeginTable("",
                                 static_cast<int>(workload_info.profiling_config.size()),
                                 ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders))
            {
                for(size_t i = 0; i < workload_info.profiling_config[0].size(); i++)
                {
                    ImGui::PushID(i);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    CopyableTextUnformatted(workload_info.profiling_config[0][i].c_str(),
                                            "", COPY_DATA_NOTIFICATION, false, true);
                    ImGui::TableNextColumn();
                    CopyableTextUnformatted(workload_info.profiling_config[1][i].c_str(),
                                            "", COPY_DATA_NOTIFICATION, false, true);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }
        else
        {
            const char* label = "Profiling Configuration Unavailable";
            RenderUnavailableMessage(label);
        }
        ImGui::EndChild();
    }
}

}  // namespace View
}  // namespace RocProfVis
