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
constexpr uint8_t     TABLE_MAX_ROWS                   = 6;

TopEventsView::TopEventsView(DataProvider&                      data_provider,
                             std::shared_ptr<TimelineSelection> timeline_selection)
: m_data_provider(data_provider)
{
    m_sections[0].heading = "Top Instrumented Thread Events";
    m_sections[0].table   = std::make_unique<TopEventsTable>(
        m_data_provider, TableType::kAnalysisTopInstrumentedEventsTable,
        kRPVControllerTableTypeInstrumentedEvents,
        DataProvider::ANALYSIS_TOP_INSTRUMENTED_EVENTS_TABLE_REQUEST_ID,
        timeline_selection);

    m_sections[1].heading = "Top Dispatch Events";
    m_sections[1].table   = std::make_unique<TopEventsTable>(
        m_data_provider, TableType::kAnalysisTopDispatchEventsTable,
        kRPVControllerTableTypeDispatchEvents,
        DataProvider::ANALYSIS_TOP_DISPATCH_EVENTS_TABLE_REQUEST_ID, timeline_selection);

    m_sections[2].heading = "Top Memory Allocation Events";
    m_sections[2].table   = std::make_unique<TopEventsTable>(
        m_data_provider, TableType::kAnalysisTopMemoryAllocationEventsTable,
        kRPVControllerTableTypeMemoryAllocationEvents,
        DataProvider::ANALYSIS_TOP_MEMORY_ALLOCATION_EVENTS_TABLE_REQUEST_ID,
        timeline_selection);

    m_sections[3].heading = "Top Memory Copy Events";
    m_sections[3].table   = std::make_unique<TopEventsTable>(
        m_data_provider, TableType::kAnalysisTopMemoryCopyEventsTable,
        kRPVControllerTableTypeMemoryCopyEvents,
        DataProvider::ANALYSIS_TOP_MEMORY_COPY_EVENTS_TABLE_REQUEST_ID,
        timeline_selection);

    m_sections[4].heading = "Top Launch Sample Events";
    m_sections[4].table   = std::make_unique<TopEventsTable>(
        m_data_provider, TableType::kAnalysisTopSampledEventsTable,
        kRPVControllerTableTypeSampledEvents,
        DataProvider::ANALYSIS_TOP_LAUNCH_SAMPLED_TABLE_REQUEST_ID, timeline_selection);

    m_widget_name = GenUniqueName("Top Events View");
}

TopEventsView::~TopEventsView() {}

void
TopEventsView::Update()
{
    for(Section& section : m_sections)
    {
        if(section.table)
        {
            section.table->Update();
        }
    }
}

void
TopEventsView::HandleTrackSelectionChanged()
{
    for(Section& section : m_sections)
    {
        if(section.table)
        {
            section.table->HandleTrackSelectionChanged();
        }
    }
}

void
TopEventsView::Render()
{
    ImGui::BeginChild("top_events", ImVec2(0, 0), ImGuiChildFlags_Borders);
    for(size_t i = 0; i < m_sections.size(); i++)
    {
        Section& section = m_sections[i];
        ImGui::TextUnformatted(section.heading);
        ImGui::Separator();
        if(section.table)
        {
            ImGui::PushID(static_cast<int>(i));
            ImGui::BeginChild("section",
                              ImVec2(0, (TABLE_MAX_ROWS + 1) * TableRowHeight()),
                              ImGuiChildFlags_None);
            section.table->Render();
            ImGui::EndChild();
            ImGui::PopID();
        }
        ImGui::Spacing();
    }
    ImGui::EndChild();
}

TopEventsView::TopEventsTable::TopEventsTable(
    DataProvider& dp, TableType table_type,
    rocprofvis_controller_table_type_t request_table_type, uint64_t request_id,
    std::shared_ptr<TimelineSelection> timeline_selection)
: MultiTrackTable(
      dp, table_type, request_table_type, request_id,
      [&dp]() -> const TablesModel& { return dp.DataModel().GetAnalysis().GetTables(); },
      [&dp]() -> TablesModel& { return dp.DataModel().GetAnalysis().GetTables(); }, false,
      timeline_selection, 2, kRPVControllerSortOrderDescending)
, m_duration_column_indices({ INVALID_UINT64_INDEX, INVALID_UINT64_INDEX,
                              INVALID_UINT64_INDEX, INVALID_UINT64_INDEX })
{
    m_widget_name = GenUniqueName("Top Events Table");
}

TopEventsView::TopEventsTable::~TopEventsTable() {}

void
TopEventsView::TopEventsTable::FilterSelectedTracksForTableType(
    const std::vector<uint64_t>& selected_track_ids,
    std::vector<uint64_t>&       filtered_track_ids) const
{
    const TimelineModel& tlm = m_data_provider.DataModel().GetTimeline();
    for(uint64_t track_id : selected_track_ids)
    {
        const TrackInfo* track_info = tlm.GetTrack(track_id);
        if(track_info && track_info->track_type == kRPVControllerTrackTypeEvents)
        {
            filtered_track_ids.push_back(track_id);
        }
    }
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

}  // namespace View
}  // namespace RocProfVis
