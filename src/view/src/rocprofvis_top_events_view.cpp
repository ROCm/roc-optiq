// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_top_events_view.h"
#include "rocprofvis_common_defs.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_utils.h"
#include "spdlog/spdlog.h"
#include "widgets/rocprofvis_gui_helpers.h"

namespace RocProfVis
{
namespace View
{

constexpr const char* TOP_EVENTS_DURATION_TOTAL_COLUMN = "DurationTotal";
constexpr const char* TOP_EVENTS_DURATION_AVG_COLUMN   = "DurationAvg";
constexpr const char* TOP_EVENTS_DURATION_MIN_COLUMN   = "DurationMin";
constexpr const char* TOP_EVENTS_DURATION_MAX_COLUMN   = "DurationMax";

TopEventsView::TopEventsView(DataProvider&                      data_provider,
                             std::shared_ptr<TimelineSelection> timeline_selection)
: m_tables(
      { std::make_unique<TopEventsTable>(
            data_provider, TableType::kAnalysisTopInstrumentedEventsTable,
            kRPVControllerTableTypeInstrumentedEvents,
            DataProvider::ANALYSIS_TOP_INSTRUMENTED_EVENTS_TABLE_REQUEST_ID,
            timeline_selection, kRocProfVisDmOperationLaunch,
            "Top Instrumented Thread Events"),
        std::make_unique<TopEventsTable>(
            data_provider, TableType::kAnalysisTopDispatchEventsTable,
            kRPVControllerTableTypeDispatchEvents,
            DataProvider::ANALYSIS_TOP_DISPATCH_EVENTS_TABLE_REQUEST_ID,
            timeline_selection, kRocProfVisDmOperationDispatch, "Top Dispatch Events"),
        std::make_unique<TopEventsTable>(
            data_provider, TableType::kAnalysisTopMemoryAllocationEventsTable,
            kRPVControllerTableTypeMemoryAllocationEvents,
            DataProvider::ANALYSIS_TOP_MEMORY_ALLOCATION_EVENTS_TABLE_REQUEST_ID,
            timeline_selection, kRocProfVisDmOperationMemoryAllocate,
            "Top Memory Allocation Events"),
        std::make_unique<TopEventsTable>(
            data_provider, TableType::kAnalysisTopMemoryCopyEventsTable,
            kRPVControllerTableTypeMemoryCopyEvents,
            DataProvider::ANALYSIS_TOP_MEMORY_COPY_EVENTS_TABLE_REQUEST_ID,
            timeline_selection, kRocProfVisDmOperationMemoryCopy,
            "Top Memory Copy Events"),
        std::make_unique<TopEventsTable>(
            data_provider, TableType::kAnalysisTopSampledEventsTable,
            kRPVControllerTableTypeSampledEvents,
            DataProvider::ANALYSIS_TOP_LAUNCH_SAMPLED_TABLE_REQUEST_ID,
            timeline_selection, kRocProfVisDmOperationLaunchSample,
            "Top Sampled Thread Events") })
{
    m_widget_name = GenUniqueName("Top Events View");
}

TopEventsView::~TopEventsView() {}

void
TopEventsView::Update()
{
    for(std::unique_ptr<TopEventsTable>& table : m_tables)
    {
        if(table)
        {
            table->Update();
        }
    }
}

void
TopEventsView::Render()
{
    const SettingsManager& settings = SettingsManager::GetInstance();
    const ImGuiStyle&      style    = settings.GetDefaultStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, style.ChildRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.WindowPadding);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgPanel));
    ImGui::PushStyleColor(ImGuiCol_Border, settings.GetColor(Colors::kBorderColor));
    ImGui::BeginChild("top_events", ImVec2(0, 0), ImGuiChildFlags_Borders);
    bool no_data = true;
    for(std::unique_ptr<TopEventsTable>& table : m_tables)
    {
        if(table)
        {
            table->Render();
            no_data &= !table->Visible();
        }
    }
    if(no_data)
    {
        CenterNextTextItem("No data available for the selected tracks.");
        ImGui::SetCursorPosY((ImGui::GetWindowHeight() - ImGui::GetTextLineHeight()) *
                             0.5f);
        ImGui::TextDisabled("No data available for the selected tracks.");
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

void
TopEventsView::HandleTrackSelectionChanged(uint64_t track_id, bool selected)
{
    for(std::unique_ptr<TopEventsTable>& table : m_tables)
    {
        if(table)
        {
            table->HandleTrackSelectionChanged(track_id, selected);
        }
    }
}

void
TopEventsView::HandleTimeRangeSelectionChanged(double start_ns, double end_ns)
{
    for(std::unique_ptr<TopEventsTable>& table : m_tables)
    {
        if(table)
        {
            table->HandleTimeRangeSelectionChanged(start_ns, end_ns);
        }
    }
}

TopEventsView::TopEventsTable::TopEventsTable(
    DataProvider& dp, TableType table_type,
    rocprofvis_controller_table_type_t request_table_type, uint64_t request_id,
    std::shared_ptr<TimelineSelection> timeline_selection,
    rocprofvis_dm_event_operation_t op, const char* header)
: MultiTrackTable(
      dp, table_type, request_table_type, request_id,
      [&dp]() -> const TablesModel& { return dp.DataModel().GetAnalysis().GetTables(); },
      [&dp]() -> TablesModel& { return dp.DataModel().GetAnalysis().GetTables(); }, false,
      timeline_selection, 2, kRPVControllerSortOrderDescending)
, m_duration_column_indices({ INVALID_UINT64_INDEX, INVALID_UINT64_INDEX,
                              INVALID_UINT64_INDEX, INVALID_UINT64_INDEX })
, m_op(op)
, m_header(header)
, m_visible(false)
{
    m_widget_name = GenUniqueName("Top Events Table");
}

TopEventsView::TopEventsTable::~TopEventsTable() {}

void
TopEventsView::TopEventsTable::Render()
{
    if(m_visible)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        ImGui::PushID(static_cast<int>(m_op));
        ImVec2 region_avail =
            ImVec2(ImGui::GetContentRegionAvail().x,
                   ImGui::GetWindowHeight() - 2.0f * style.WindowPadding.y);
        if(ImGui::CollapsingHeader(m_header, ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::SetNextWindowSize(
                ImVec2(region_avail.x,
                       std::min(region_avail.y - ImGui::GetFrameHeightWithSpacing(),
                                (Rows() + 1) * TableRowHeight() +
                                    ImGui::GetFrameHeightWithSpacing() +
                                    2.0f * style.WindowPadding.y)));
            MultiTrackTable::Render();
        }
        ImGui::PopID();
    }
}

void
TopEventsView::TopEventsTable::HandleTrackSelectionChanged(uint64_t track_id,
                                                           bool     selected)
{
    MultiTrackTable::HandleTrackSelectionChanged(track_id, selected);
    m_visible = !m_included_tracks.empty();
}

bool
TopEventsView::TopEventsTable::Visible() const
{
    return m_visible;
}

bool
TopEventsView::TopEventsTable::IncludeTrack(uint64_t track_id) const
{
    bool             include = false;
    const TrackInfo* track_info =
        m_data_provider.DataModel().GetTimeline().GetTrack(track_id);
    if(track_info)
    {
        include = track_info->operation_types.count(m_op) > 0;
    }
    return include;
}

void
TopEventsView::TopEventsTable::IndexColumns()
{
    MultiTrackTable::IndexColumns();
    const std::vector<std::string>& column_names =
        m_table_model().GetTableHeader(m_table_type);
    m_duration_column_indices = { INVALID_UINT64_INDEX, INVALID_UINT64_INDEX,
                                  INVALID_UINT64_INDEX, INVALID_UINT64_INDEX };
    for(size_t i = 0; i < column_names.size(); i++)
    {
        const std::string& col = column_names[i];
        if(col == TOP_EVENTS_DURATION_TOTAL_COLUMN)
        {
            m_duration_column_indices[kDurationTotal] = i;
        }
        else if(col == TOP_EVENTS_DURATION_AVG_COLUMN)
        {
            m_duration_column_indices[kDurationAvg] = i;
        }
        else if(col == TOP_EVENTS_DURATION_MIN_COLUMN)
        {
            m_duration_column_indices[kDurationMin] = i;
        }
        else if(col == TOP_EVENTS_DURATION_MAX_COLUMN)
        {
            m_duration_column_indices[kDurationMax] = i;
        }
    }
}

void
TopEventsView::TopEventsTable::FormatData() const
{
    const std::vector<std::vector<std::string>>& table_data =
        m_table_model().GetTableData(m_table_type);
    std::vector<FormattedColumnInfo>& formatted_column_data =
        m_table_model_mutable().GetMutableFormattedTableData(m_table_type);
    formatted_column_data.clear();
    formatted_column_data.resize(m_table_model().GetTableHeader(m_table_type).size());
    auto time_format = m_settings.GetUserSettings().unit_settings.time_format;
    for(size_t i : m_duration_column_indices)
    {
        if(i < formatted_column_data.size())
        {
            formatted_column_data[i].needs_formatting = true;
            formatted_column_data[i].formatted_row_value.resize(table_data.size());
            for(size_t row_idx = 0; row_idx < table_data.size(); row_idx++)
            {
                const std::string& raw_value = table_data[row_idx][i];
                try
                {
                    double duration_ns = std::stod(raw_value);
                    formatted_column_data[i].formatted_row_value[row_idx] =
                        nanosecond_to_formatted_str(duration_ns, time_format);
                } catch(const std::exception& e)
                {
                    spdlog::warn("Failed to format duration value '{}': {}", raw_value,
                                 e.what());
                    formatted_column_data[i].formatted_row_value[row_idx] = raw_value;
                }
            }
        }
    }
}

size_t
TopEventsView::TopEventsTable::Rows() const
{
    return m_table_model().GetTableTotalRowCount(m_table_type);
}

}  // namespace View
}  // namespace RocProfVis
