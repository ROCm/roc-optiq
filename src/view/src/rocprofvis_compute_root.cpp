// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_compute_root.h"

namespace RocProfVis
{
namespace View
{

ComputeRoot::ComputeRoot()
: m_compute_summary_view(nullptr)
, m_compute_block_view(nullptr)
, m_compute_table_view(nullptr)
, m_compute_data_provider(nullptr)
, m_normalization_unit(kPerWave)
, m_dispatch_filter(0)
, m_kernel_filter(0)
{
    m_compute_data_provider = std::make_shared<ComputeDataProvider>();

    m_compute_summary_view = std::make_shared<ComputeSummaryView>(m_compute_data_provider);
    m_compute_block_view = std::make_shared<ComputeBlockView>(m_compute_data_provider);
    m_compute_table_view = std::make_shared<ComputeTableView>(m_compute_data_provider);
}

ComputeRoot::~ComputeRoot() {}

void ComputeRoot::Update()
{
    if (!m_compute_data_provider->MetricsLoaded())
    {
        m_compute_data_provider->LoadMetricsFromCSV();

        if (m_compute_data_provider->MetricsLoaded())
        {
            if (m_compute_summary_view)
            {
                m_compute_summary_view->Update();
            }
            if (m_compute_table_view)
            {
                m_compute_table_view->Update();
            }
            if (m_compute_block_view)
            {
                m_compute_block_view->Update();
            }
        }
    }
}

void ComputeRoot::SetMetricsPath(std::filesystem::path path)
{
    m_compute_data_provider->SetMetricsPath(path);
}

bool ComputeRoot::MetricsLoaded()
{
    return m_compute_data_provider->MetricsLoaded();
}

void ComputeRoot::Render()
{
    if (m_compute_data_provider->MetricsLoaded())
    {
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
}

}  // namespace View
}  // namespace RocProfVis
