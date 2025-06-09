// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_compute_widget.h"
#include "rocprofvis_core_assert.h"
#include "implot.h"
#include <regex>

namespace RocProfVis
{
namespace View
{

constexpr ImVec2 ITEM_SPACING_DEFAULT = ImVec2(8, 4);
constexpr ImPlotColormap PLOT_COLOR_MAP = ImPlotColormap_Plasma;
constexpr float PLOT_COLOR_MAP_WIDTH = 0.1f;
constexpr double PLOT_BAR_SIZE = 0.67;
constexpr double TABLE_THRESHOLD_HIGH = 80;
constexpr double TABLE_THRESHOLD_MID = 50;
constexpr ImU32 TABLE_COLOR_HIGH = IM_COL32(255, 18, 10, 255);
constexpr ImU32 TABLE_COLOR_MID = IM_COL32(255, 169, 10, 255);
constexpr ImU32 TABLE_COLOR_SEARCH = IM_COL32(0, 255, 0, 255);
constexpr ImVec4 TABLE_COLOR_SEARCH_TEXT = ImVec4(0, 0, 0, 1);

ComputeTable::ComputeTable(std::shared_ptr<ComputeDataProvider2> data_provider, rocprofvis_controller_compute_table_types_t type)
: m_data_provider(data_provider)
, m_type(type)
, m_model(nullptr)
, m_id("")
{
    ROCPROFVIS_ASSERT(m_data_provider);
    m_id = GenUniqueName("");
}

void ComputeTable::Update()
{
    m_model = m_data_provider->GetTableModel(m_type);
}

void ComputeTable::Render()
{
    if (m_model)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ITEM_SPACING_DEFAULT);
        ImGui::SeparatorText(m_model->m_title.c_str());
        ImGui::PopStyleVar(); 
        std::vector<std::vector<ComputeTableCellModel>>& cells = m_model->m_cells;
        if (ImGui::BeginTable(m_id.c_str(), cells[0].size(), ImGuiTableFlags_Borders | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortTristate))
        {
            for (std::string& c : m_model->m_column_names)
            {
                ImGui::TableSetupColumn(c.c_str());
            }
            ImGui::TableHeadersRow();
                
            ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs();
            if (sort_specs && sort_specs->SpecsDirty)
            {
                int sort_column;
                rocprofvis_controller_sort_order_t sort_order;
                if (sort_specs > 0 && sort_specs->Specs)
                {
                    sort_column = sort_specs->Specs->ColumnIndex;
                    sort_order = (sort_specs->Specs->SortDirection == ImGuiSortDirection_Ascending) ? kRPVControllerSortOrderAscending : kRPVControllerSortOrderDescending;
                }
                else
                {
                    sort_column = 0;
                    sort_order = kRPVControllerSortOrderNone;
                }
                sort_specs->SpecsDirty = false;
            }

            for (int r = 0; r < cells.size(); r ++)
            {
                ImGui::TableNextRow();
                for (int c = 0; c < cells[r].size(); c ++)
                {
                    ImGui::TableSetColumnIndex(c);
                        
                    ComputeTableCellModel& cell = cells[r][c];
                    if (cell.m_highlight)
                    {
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, TABLE_COLOR_SEARCH);
                    }
                    else if (cell.m_colorize && cell.m_metric)
                    {
                        if (cell.m_metric->m_value > TABLE_THRESHOLD_HIGH)
                        {
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, TABLE_COLOR_HIGH);
                        }
                        else if (cell.m_metric->m_value > TABLE_THRESHOLD_MID)
                        {
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, TABLE_COLOR_MID);
                        }
                    }

                    ImGui::TextColored(cell.m_highlight ? TABLE_COLOR_SEARCH_TEXT : ImGui::GetStyleColorVec4(ImGuiCol_Text), cell.m_value.c_str());
                }
            }
            ImGui::EndTable();
        }
    }
}

void ComputeTable::Search(const std::string& term)
{
    if (m_model)
    {
        std::regex exp(term, std::regex_constants::icase);
        for (std::vector<ComputeTableCellModel>& row : m_model->m_cells)
        {            
            bool match = !term.empty() && !(term.length() == 1 && term == " ") && std::regex_search(row[0].m_value, exp);
            for (ComputeTableCellModel& cell : row)
            {
                cell.m_highlight = match;
            }
        }
    }
}

}  // namespace View
}  // namespace RocProfVis
