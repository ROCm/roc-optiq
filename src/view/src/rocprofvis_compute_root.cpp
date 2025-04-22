// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_compute_root.h"

using namespace RocProfVis::View;

constexpr char* normalization_units[] = {"Per Wave", "Per Cycle", "Per Kernel", "Per Second"};
constexpr char* kernel_names[] = {"Kernel A", "Kernel B", "Kernel C", "Kernel D", "Kernel E", "Kernel F", "Kernel G", "Kernel H", "Kernel I", "Kernel J"};
constexpr char* dispatch_IDs[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};

constexpr int normalization_combo_width = 100;
constexpr int kernel_filter_combo_width = 200;
constexpr int dispatch_filter_combo_width = 40;

ComputeRoot::ComputeRoot()
: m_compute_summary_view(nullptr)
, m_compute_block_view(nullptr)
, m_compute_table_view(nullptr)
, m_compute_data_provider(nullptr)
, m_normalization_unit(kPerWave)
, m_dispatch_filter(0)
, m_kernel_filter(0)
, m_metrics_loaded(false)
{
    m_compute_data_provider = std::make_shared<ComputeDataProvider>();

    m_compute_summary_view = std::make_shared<ComputeSummaryView>(m_compute_data_provider);
    m_compute_block_view = std::make_shared<ComputeBlockView>();
    m_compute_table_view = std::make_shared<ComputeTableView>(m_compute_data_provider);
}

ComputeRoot::~ComputeRoot() {}

void ComputeRoot::Update()
{
    if (!m_metrics_loaded)
    {
        m_compute_data_provider->LoadMetricsFromCSV();
        m_compute_summary_view->Update();
        m_metrics_loaded = true;
    }
}

void ComputeRoot::Render()
{
    RenderMenuBar();
    if(ImGui::BeginTabBar("compute_root_tab_bar", ImGuiTabBarFlags_None))
    {
        if(ImGui::BeginTabItem("Summary View") && m_compute_summary_view)
        {
            m_compute_summary_view->Render();
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Block View") && m_compute_block_view)
        {
            m_compute_block_view->Render();
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Table View") && m_compute_table_view)
        {
            m_compute_table_view->Render();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void ComputeRoot::RenderMenuBar()
{
    ImGui::BeginMenuBar();

    float menuBarWidth =  ImGui::CalcTextSize("Normalization").x + ImGui::CalcTextSize("Dispatch Filter").x + ImGui::CalcTextSize("Kernel Filter").x 
        + normalization_combo_width + dispatch_filter_combo_width + kernel_filter_combo_width + 3;
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + ImGui::GetContentRegionAvail().x - menuBarWidth, cursorPos.y));

    ImGui::Separator();
    ImGui::Text("Normalization"); 
    ImGui::SetNextItemWidth(normalization_combo_width);
    if (ImGui::BeginCombo("##Compute Normalization Unit", normalization_units[m_normalization_unit]))
    {
        for (int i = 0; i < IM_ARRAYSIZE(normalization_units); i++)
        {
            if (ImGui::Selectable(normalization_units[i], m_normalization_unit == i))
            {
                m_normalization_unit = static_cast<compute_normalization_unit_t>(i);
            }
        }
        ImGui::EndCombo();
    }
    ImGui::Separator();
    ImGui::Text("Dispatch Filter");
    ImGui::SetNextItemWidth(dispatch_filter_combo_width);
    if (ImGui::BeginCombo("##Compute Dispatch Filter", dispatch_IDs[m_dispatch_filter]))
    {
        for (int i = 0; i < IM_ARRAYSIZE(dispatch_IDs); i++)
        {
            if (ImGui::Selectable(dispatch_IDs[i], m_dispatch_filter == i))
            {
                m_dispatch_filter = i;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::Separator();
    ImGui::Text("Kernel Filter");
    ImGui::SetNextItemWidth(kernel_filter_combo_width);
    if (ImGui::BeginCombo("##Compute Kernel Filter", kernel_names[m_kernel_filter]))
    {
        for (int i = 0; i < IM_ARRAYSIZE(kernel_names); i++)
        {
            if (ImGui::Selectable(kernel_names[i], m_kernel_filter == i))
            {
                m_kernel_filter = i;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::EndMenuBar();
}
