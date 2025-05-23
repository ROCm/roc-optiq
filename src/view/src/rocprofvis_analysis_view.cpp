// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_analysis_view.h"
#include "rocprofvis_data_provider.h"

using namespace RocProfVis::View;

AnalysisView::AnalysisView(DataProvider& dp)
: m_data_provider(dp)
{}

AnalysisView::~AnalysisView() {}

void
AnalysisView::Update()
{}

void
AnalysisView::Render()
{
    ImGui::BeginChild("Event Data", ImVec2(0, 0), true);

    const std::vector<std::vector<std::string>>& table_data =
        m_data_provider.GetEventTableData();
    const std::vector<std::string>& column_names =
        m_data_provider.GetEventTableHeader();
    
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
