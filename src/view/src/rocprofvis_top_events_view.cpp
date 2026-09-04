// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_top_events_view.h"
#include "rocprofvis_common_defs.h"
#include "rocprofvis_compare_panes.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_utils.h"
#include "spdlog/spdlog.h"
#include "widgets/rocprofvis_gui_helpers.h"

#include <algorithm>
#include <string>

namespace RocProfVis
{
namespace View
{

constexpr const char* TOP_EVENTS_DURATION_TOTAL_COLUMN = "DurationTotal";
constexpr const char* TOP_EVENTS_DURATION_AVG_COLUMN   = "DurationAvg";
constexpr const char* TOP_EVENTS_DURATION_MIN_COLUMN   = "DurationMin";
constexpr const char* TOP_EVENTS_DURATION_MAX_COLUMN   = "DurationMax";

constexpr const char* NO_DATA_TEXT = "No data available for the selected tracks.";

// The tables open sorted on the total duration column, biggest first.
constexpr uint64_t TOTAL_DURATION_SORT_COLUMN = 2;

// One event category: the pooled table takes the shared request id, the compare
// pair packs a client id into its own so the two fetches stay apart.
struct CategoryDescription
{
    TableType                          table_type[COMPARE_SOURCE_COUNT];
    rocprofvis_controller_table_type_t request_table_type;
    RequestType                        request_type;
    uint64_t                           request_id;
    rocprofvis_dm_event_operation_t    op;
    const char*                        header;
};

TopEventsView::TopEventsView(DataProvider&                      data_provider,
                             std::shared_ptr<TimelineSelection> timeline_selection)
: m_data_provider(data_provider)
, m_compare_mode(false)
{
    // The categories in display order. Kept local because the request ids are
    // DataProvider constants, which are not ready before main() runs.
    const CategoryDescription descriptions[CATEGORY_COUNT] = {
        { { TableType::kAnalysisTopInstrumentedEventsTable,
            TableType::kAnalysisTopInstrumentedEventsTableB },
          kRPVControllerTableTypeInstrumentedEvents,
          RequestType::kFetchAnalysisTopEventsTable,
          DataProvider::ANALYSIS_TOP_INSTRUMENTED_EVENTS_TABLE_REQUEST_ID,
          kRocProfVisDmOperationLaunch,
          "Top Instrumented Thread Events" },
        { { TableType::kAnalysisTopDispatchEventsTable,
            TableType::kAnalysisTopDispatchEventsTableB },
          kRPVControllerTableTypeDispatchEvents,
          RequestType::kFetchAnalysisTopDispatchEventsTable,
          DataProvider::ANALYSIS_TOP_DISPATCH_EVENTS_TABLE_REQUEST_ID,
          kRocProfVisDmOperationDispatch,
          "Top Dispatch Events" },
        { { TableType::kAnalysisTopMemoryAllocationEventsTable,
            TableType::kAnalysisTopMemoryAllocationEventsTableB },
          kRPVControllerTableTypeMemoryAllocationEvents,
          RequestType::kFetchAnalysisTopMemoryAllocationEventsTable,
          DataProvider::ANALYSIS_TOP_MEMORY_ALLOCATION_EVENTS_TABLE_REQUEST_ID,
          kRocProfVisDmOperationMemoryAllocate,
          "Top Memory Allocation Events" },
        { { TableType::kAnalysisTopMemoryCopyEventsTable,
            TableType::kAnalysisTopMemoryCopyEventsTableB },
          kRPVControllerTableTypeMemoryCopyEvents,
          RequestType::kFetchAnalysisTopMemoryCopyEventsTable,
          DataProvider::ANALYSIS_TOP_MEMORY_COPY_EVENTS_TABLE_REQUEST_ID,
          kRocProfVisDmOperationMemoryCopy,
          "Top Memory Copy Events" },
        { { TableType::kAnalysisTopSampledEventsTable,
            TableType::kAnalysisTopSampledEventsTableB },
          kRPVControllerTableTypeSampledEvents,
          RequestType::kFetchAnalysisTopLaunchSampleEventsTable,
          DataProvider::ANALYSIS_TOP_LAUNCH_SAMPLED_TABLE_REQUEST_ID,
          kRocProfVisDmOperationLaunchSample,
          "Top Sampled Thread Events" },
    };

    m_widget_name  = GenUniqueName("Top Events View");
    m_compare_mode = IsCompareTrace(data_provider.DataModel());

    for(size_t i = 0; i < m_categories.size(); i++)
    {
        const CategoryDescription& description = descriptions[i];
        Category&                  category    = m_categories[i];
        category.header                        = description.header;

        if(!m_compare_mode)
        {
            category.tables[COMPARE_SOURCE_A] = std::make_unique<TopEventsTable>(
                data_provider, description.table_type[COMPARE_SOURCE_A],
                description.request_table_type, description.request_id,
                timeline_selection, description.op, description.header);
            continue;
        }

        for(size_t source = 0; source < COMPARE_SOURCE_COUNT; source++)
        {
            category.tables[source] = std::make_unique<TopEventsTable>(
                data_provider, description.table_type[source],
                description.request_table_type,
                RequestIdBuilder::MakeClientRequestId(description.request_type,
                                                      COMPARE_CLIENT_ID[source]),
                timeline_selection, description.op, description.header, source);
            category.tables[source]->SetDisplaySummary(false);
            category.tables[source]->SetHeaderRenderer(
                [this, source]() { RenderSourceTitle(source); });
        }

        // Either header can drive the sort, so sync both ways.
        for(size_t source = 0; source < COMPARE_SOURCE_COUNT; source++)
        {
            const size_t peer = (source + 1) % COMPARE_SOURCE_COUNT;
            category.tables[source]->SetSortSyncCallback(
                [&category, peer](const MultiTrackTable& src) {
                    category.tables[peer]->ApplySharedSortFrom(src);
                });
        }
    }
}

TopEventsView::~TopEventsView() {}

void
TopEventsView::Update()
{
    for(Category& category : m_categories)
    {
        for(std::unique_ptr<TopEventsTable>& table : category.tables)
        {
            if(table)
            {
                table->Update();
            }
        }
    }
}

void
TopEventsView::RenderSourceTitle(size_t source_index)
{
    const CompareSourceInfo* source =
        m_data_provider.DataModel().GetCompareSource(source_index);
    if(source)
    {
        RenderCompareCardTitle(*source, SettingsManager::GetInstance());
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
    for(Category& category : m_categories)
    {
        no_data &= !AnyVisible(category);
        if(m_compare_mode)
        {
            RenderCategory(category);
        }
        else if(category.tables[COMPARE_SOURCE_A])
        {
            category.tables[COMPARE_SOURCE_A]->Render();
        }
    }
    if(no_data)
    {
        CenterNextTextItem(NO_DATA_TEXT);
        ImGui::SetCursorPosY((ImGui::GetWindowHeight() - ImGui::GetTextLineHeight()) *
                             0.5f);
        ImGui::TextDisabled("%s", NO_DATA_TEXT);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

bool
TopEventsView::AnyVisible(const Category& category)
{
    bool visible = false;
    for(const std::unique_ptr<TopEventsTable>& table : category.tables)
    {
        visible = visible || (table && table->Visible());
    }
    return visible;
}

void
TopEventsView::RenderCategory(Category& category)
{
    if(!AnyVisible(category))
    {
        return;
    }

    // The category header is drawn once, then both sources' cards below it.
    ImGui::PushID(category.header);
    if(ImGui::CollapsingHeader(category.header, ImGuiTreeNodeFlags_DefaultOpen))
    {
        const ImGuiStyle& style = SettingsManager::GetInstance().GetDefaultStyle();
        float             content_height = 0.0f;
        for(std::unique_ptr<TopEventsTable>& table : category.tables)
        {
            if(table)
            {
                content_height = std::max(content_height, table->ContentHeight());
            }
        }
        // The cards carry a source title that the pooled tables do not have.
        content_height += ImGui::GetFrameHeightWithSpacing();

        const float region_height =
            ImGui::GetWindowHeight() - 2.0f * style.WindowPadding.y;
        const ImVec2 card_size(
            (ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) * COMPARE_EVEN_SPLIT,
            std::min(region_height - ImGui::GetFrameHeightWithSpacing(), content_height));

        for(size_t source = 0; source < category.tables.size(); source++)
        {
            if(!category.tables[source])
            {
                continue;
            }
            if(source > COMPARE_SOURCE_A)
            {
                ImGui::SameLine();
            }
            ImGui::PushID(static_cast<int>(source));
            category.tables[source]->RenderCard(card_size);
            ImGui::PopID();
        }
    }
    ImGui::PopID();
}

void
TopEventsView::HandleTrackSelectionChanged(uint64_t track_id, bool selected)
{
    for(Category& category : m_categories)
    {
        for(std::unique_ptr<TopEventsTable>& table : category.tables)
        {
            if(table)
            {
                table->HandleTrackSelectionChanged(track_id, selected);
            }
        }
    }
}

void
TopEventsView::HandleTimeRangeSelectionChanged(double start_ns, double end_ns)
{
    for(Category& category : m_categories)
    {
        for(std::unique_ptr<TopEventsTable>& table : category.tables)
        {
            if(table)
            {
                table->HandleTimeRangeSelectionChanged(start_ns, end_ns);
            }
        }
    }
}

TopEventsView::TopEventsTable::TopEventsTable(
    DataProvider& dp, TableType table_type,
    rocprofvis_controller_table_type_t request_table_type, uint64_t request_id,
    std::shared_ptr<TimelineSelection> timeline_selection,
    rocprofvis_dm_event_operation_t op, const char* header,
    std::optional<uint64_t> source_file_id)
: MultiTrackTable(
      dp, table_type, request_table_type, request_id,
      [&dp]() -> const TablesModel& { return dp.DataModel().GetAnalysis().GetTables(); },
      [&dp]() -> TablesModel& { return dp.DataModel().GetAnalysis().GetTables(); },
      timeline_selection, kNone, TOTAL_DURATION_SORT_COLUMN,
      kRPVControllerSortOrderDescending, "", "", source_file_id)
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
        const float region_height =
            ImGui::GetWindowHeight() - 2.0f * style.WindowPadding.y;
        if(ImGui::CollapsingHeader(m_header, ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::SetNextWindowSize(
                ImVec2(ImGui::GetContentRegionAvail().x,
                       std::min(region_height - ImGui::GetFrameHeightWithSpacing(),
                                ContentHeight())));
            MultiTrackTable::Render();
        }
        ImGui::PopID();
    }
}

float
TopEventsView::TopEventsTable::ContentHeight() const
{
    const ImGuiStyle& style = SettingsManager::GetInstance().GetDefaultStyle();
    return (Rows() + 1) * TableRowHeight() + ImGui::GetFrameHeightWithSpacing() +
           2.0f * style.WindowPadding.y;
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
        if(include && m_source_file_id.has_value())
        {
            include = track_info->file_id == m_source_file_id.value();
        }
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
