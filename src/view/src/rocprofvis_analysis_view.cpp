// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_analysis_view.h"
#include "rocprofvis_data_provider.h"

using namespace RocProfVis::View;

AnalysisView::AnalysisView(DataProvider& dp)
: m_data_provider(dp)
{
    m_widget_name = GenUniqueName("Analysis View");

    m_tab_container = std::make_shared<TabContainer>();

    TabItem tab_item;
    tab_item.m_label     = "Event Details";
    tab_item.m_id        = "event_details";
    tab_item.m_can_close = false;
    tab_item.m_widget    = std::make_shared<RocCustomWidget>([this]() { this->RenderEventTable(); });
    m_tab_container->AddTab(tab_item);

    tab_item.m_label     = "Sample Details";
    tab_item.m_id        = "sample_details";
    tab_item.m_can_close = false;
    tab_item.m_widget    = std::make_shared<RocCustomWidget>([this]() { this->RenderSampleTable(); });
    m_tab_container->AddTab(tab_item);

    m_tab_container->SetAllowToolTips(false);
    m_tab_container->SetActiveTab(0);
}

AnalysisView::~AnalysisView() {}

void
AnalysisView::Update()
{
    m_tab_container->Update();
}

void
AnalysisView::Render()
{
    m_tab_container->Render();
}

void
AnalysisView::RenderEventTable()
{
    ImGui::BeginChild("Event Data", ImVec2(0, 0), true);

    const std::vector<std::vector<std::string>>& table_data =
        m_data_provider.GetEventTableData();
    const std::vector<std::string>& column_names = m_data_provider.GetEventTableHeader();

    if(table_data.size() > 0)
    {
        ImGui::Text("Total Events: %d", static_cast<int>(table_data.size()));
        ImGui::BeginTable("Event Data Table", column_names.size(), ImGuiTableFlags_RowBg);
        for(const auto& col : column_names)
        {
            ImGui::TableSetupColumn(col.c_str());
        }
        ImGui::TableHeadersRow();
        for(const auto& row : table_data)
        {
            ImGui::TableNextRow();
            int column = 0;
            for(const auto& col : row)
            {
                ImGui::TableSetColumnIndex(column);
                ImGui::Text(col.c_str());
                column++;
            }
        }
        ImGui::EndTable();
    }
    else
    {
        ImGui::Text("No Event Data Available");
    }
    ImGui::EndChild();
}

void AnalysisView::RenderSampleTable()
{
    ImGui::BeginChild("Sample Data", ImVec2(0, 0), true);

    const std::vector<std::vector<std::string>>& table_data =
        m_data_provider.GetSampleTableData();
    const std::vector<std::string>& column_names = m_data_provider.GetSampleTableHeader();

    if(table_data.size() > 0)
    {
        ImGui::Text("Total Samples: %d", static_cast<int>(table_data.size()));
        ImGui::BeginTable("Sample Data Table", column_names.size(), ImGuiTableFlags_RowBg);
        for(const auto& col : column_names)
        {
            ImGui::TableSetupColumn(col.c_str());
        }
        ImGui::TableHeadersRow();
        for(const auto& row : table_data)
        {
            ImGui::TableNextRow();
            int column = 0;
            for(const auto& col : row)
            {
                ImGui::TableSetColumnIndex(column);
                ImGui::Text(col.c_str());
                column++;
            }
        }
        ImGui::EndTable();
    }
    else
    {
        ImGui::Text("No Sample Data Available");
    }
    ImGui::EndChild();
}