// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_csv_table_view.h"
#include "imgui.h"
#include <regex>
#include <cstring>

namespace RocProfVis
{
namespace View
{

// Table color constants (matching the existing table view)
constexpr ImU32 TABLE_COLOR_HIGH = IM_COL32(255, 18, 10, 255);
constexpr ImU32 TABLE_COLOR_MID = IM_COL32(255, 169, 10, 255);
constexpr ImU32 TABLE_COLOR_SEARCH = IM_COL32(0, 255, 0, 255);
constexpr ImVec4 TABLE_COLOR_SEARCH_TEXT = ImVec4(0, 0, 0, 1);

// Tab definitions matching the existing table view categories
static const std::vector<std::pair<std::string, std::string>> TAB_CATEGORIES = {
    {"System Speed of Light", "speed_of_light"},
    {"Command Processor", "command_processor"},
    {"Workgroup Manager", "workgroup_manager"},
    {"Wavefront", "wavefront"},
    {"Instruction Mix", "instruction_mix"},
    {"Compute Pipeline", "compute_pipeline"},
    {"Local Data Store", "local_data_store"},
    {"L1 Instruction Cache", "l1_instruction_cache"},
    {"Scalar L1 Data Cache", "scalar_l1_cache"},
    {"Address Processing Unit & Data Return Path", "address_processing"},
    {"Vector L1 Data Cache", "vector_l1_cache"},
    {"L2 Cache", "l2_cache"},
    {"L2 Cache (Per Channel)", "l2_cache_per_channel"}
};

//-----------------------------------------------------------------------------
// CSVDummyTable
//-----------------------------------------------------------------------------

CSVDummyTable::CSVDummyTable(const std::string& title, const std::vector<std::string>& columns)
    : m_title(title)
    , m_columns(columns)
    , m_search_term("")
{
    m_id = GenUniqueName("CSVTable");
}

void CSVDummyTable::AddRow(const CSVTableRow& row)
{
    m_rows.push_back(row);
}

void CSVDummyTable::Search(const std::string& term)
{
    m_search_term = term;
}

void CSVDummyTable::Render()
{
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
    ImGui::SeparatorText(m_title.c_str());
    ImGui::PopStyleVar();

    if (ImGui::BeginTable(m_id.c_str(), m_columns.size(), ImGuiTableFlags_Borders))
    {
        // Setup columns
        for (const std::string& col : m_columns)
        {
            ImGui::TableSetupColumn(col.c_str());
        }
        ImGui::TableHeadersRow();

        // Render rows
        std::regex search_regex;
        bool has_search = !m_search_term.empty() && m_search_term != " ";
        if (has_search)
        {
            try
            {
                search_regex = std::regex(m_search_term, std::regex_constants::icase);
            }
            catch (...)
            {
                has_search = false;
            }
        }

        for (const CSVTableRow& row : m_rows)
        {
            bool highlight = has_search && std::regex_search(row.metric_name, search_regex);

            ImGui::TableNextRow();

            // Metric column
            ImGui::TableSetColumnIndex(0);
            if (highlight)
            {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, TABLE_COLOR_SEARCH);
            }
            ImGui::TextColored(highlight ? TABLE_COLOR_SEARCH_TEXT : ImGui::GetStyleColorVec4(ImGuiCol_Text), 
                              "%s", row.metric_name.c_str());

            // Avg column
            ImGui::TableSetColumnIndex(1);
            if (highlight)
            {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, TABLE_COLOR_SEARCH);
            }
            ImGui::TextColored(highlight ? TABLE_COLOR_SEARCH_TEXT : ImGui::GetStyleColorVec4(ImGuiCol_Text),
                              "%s", row.avg_value.c_str());

            // Unit column
            ImGui::TableSetColumnIndex(2);
            if (highlight)
            {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, TABLE_COLOR_SEARCH);
            }
            ImGui::TextColored(highlight ? TABLE_COLOR_SEARCH_TEXT : ImGui::GetStyleColorVec4(ImGuiCol_Text),
                              "%s", row.unit.c_str());

            // Peak column
            ImGui::TableSetColumnIndex(3);
            if (highlight)
            {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, TABLE_COLOR_SEARCH);
            }
            ImGui::TextColored(highlight ? TABLE_COLOR_SEARCH_TEXT : ImGui::GetStyleColorVec4(ImGuiCol_Text),
                              "%s", row.peak_value.c_str());

            // Pct of Peak column (with colorization)
            ImGui::TableSetColumnIndex(4);
            if (highlight)
            {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, TABLE_COLOR_SEARCH);
            }
            else if (!row.pct_of_peak.empty())
            {
                try
                {
                    double pct = std::stod(row.pct_of_peak);
                    if (pct > 80.0)
                    {
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, TABLE_COLOR_HIGH);
                    }
                    else if (pct > 50.0)
                    {
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, TABLE_COLOR_MID);
                    }
                }
                catch (...) {}
            }
            ImGui::TextColored(highlight ? TABLE_COLOR_SEARCH_TEXT : ImGui::GetStyleColorVec4(ImGuiCol_Text),
                              "%s", row.pct_of_peak.c_str());
        }

        ImGui::EndTable();
    }
}

//-----------------------------------------------------------------------------
// CSVTableCategory
//-----------------------------------------------------------------------------

CSVTableCategory::CSVTableCategory(const std::string& category_name)
    : m_category_name(category_name)
{
}

void CSVTableCategory::AddTable(std::shared_ptr<CSVDummyTable> table)
{
    m_tables.push_back(table);
}

void CSVTableCategory::Search(const std::string& term)
{
    for (auto& table : m_tables)
    {
        table->Search(term);
    }
}

void CSVTableCategory::Update()
{
    for (auto& table : m_tables)
    {
        table->Update();
    }
}

void CSVTableCategory::Render()
{
    ImGui::BeginChild("", ImVec2(-1, -1));
    for (auto& table : m_tables)
    {
        table->Render();
    }
    ImGui::EndChild();
}

//-----------------------------------------------------------------------------
// ComputeCSVTableView
//-----------------------------------------------------------------------------

ComputeCSVTableView::ComputeCSVTableView()
    : m_search_term("")
{
    memset(m_search_term, 0, sizeof(m_search_term));
    m_tab_container = std::make_shared<TabContainer>();
    CreateDummyTabs();
}

ComputeCSVTableView::~ComputeCSVTableView()
{
}

void ComputeCSVTableView::CreateDummyTabs()
{
    // Standard columns for most tables
    std::vector<std::string> standard_columns = {"Metric", "Avg", "Unit", "Peak", "Pct of Peak"};

    for (const auto& [tab_name, tab_id] : TAB_CATEGORIES)
    {
        auto category = std::make_shared<CSVTableCategory>(tab_name);

        // Create a dummy table for each category
        auto table = std::make_shared<CSVDummyTable>(tab_name, standard_columns);

        // Add placeholder rows (empty - user said don't fill in data)
        // The table structure is ready to receive data

        category->AddTable(table);
        m_categories.push_back(category);
        m_tab_container->AddTab(TabItem{tab_name, tab_id, category, false});
    }
}

void ComputeCSVTableView::Update()
{
    if (m_tab_container)
    {
        m_tab_container->Update();
    }
}

void ComputeCSVTableView::RenderMenuBar()
{
    ImVec2 cursor_position = ImGui::GetCursorScreenPos();
    ImVec2 content_region = ImGui::GetContentRegionAvail();
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(cursor_position.x, cursor_position.y),
        ImVec2(cursor_position.x + content_region.x,
               cursor_position.y + ImGui::GetFrameHeightWithSpacing()),
        ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg]),
        0.0f);

    ImGui::SetCursorScreenPos(ImVec2(cursor_position.x + ImGui::GetContentRegionAvail().x - 350, cursor_position.y));
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Search");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(350 - ImGui::CalcTextSize("Search").x);
    if (ImGui::InputTextWithHint("##csv_table_search_bar", "Metric Name", m_search_term, IM_ARRAYSIZE(m_search_term),
                                 ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_AutoSelectAll))
    {
        // Propagate search to all categories
        std::string search_str(m_search_term);
        for (auto& category : m_categories)
        {
            category->Search(search_str);
        }
    }
}

void ComputeCSVTableView::Render()
{
    RenderMenuBar();

    ImGui::BeginChild("csv_table_tab_bar", ImVec2(-1, -1), ImGuiChildFlags_Borders);
    m_tab_container->Render();
    ImGui::EndChild();
}

}  // namespace View
}  // namespace RocProfVis
