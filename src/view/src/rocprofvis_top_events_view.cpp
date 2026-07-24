// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_top_events_view.h"
#include "rocprofvis_common_defs.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_track_item.h"
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

constexpr size_t   COMPARE_SOURCE_A_INDEX = 0;
constexpr size_t   COMPARE_SOURCE_B_INDEX = 1;
constexpr uint64_t COMPARE_CLIENT_ID_A    = 1;
constexpr uint64_t COMPARE_CLIENT_ID_B    = 2;
constexpr float    COMPARE_TITLE_GAP      = 2.0f;

TopEventsView::TopEventsView(DataProvider&                      data_provider,
                             std::shared_ptr<TimelineSelection> timeline_selection)
: m_data_provider(data_provider)
, m_compare_mode(false)
{
    m_widget_name = GenUniqueName("Top Events View");

    const CompareSourceInfo* source_a =
        data_provider.DataModel().GetCompareSource(COMPARE_SOURCE_A_INDEX);
    const CompareSourceInfo* source_b =
        data_provider.DataModel().GetCompareSource(COMPARE_SOURCE_B_INDEX);
    m_compare_mode = source_a && source_b;

    struct Desc
    {
        TableType                          type_a;
        TableType                          type_b;
        rocprofvis_controller_table_type_t request_type;
        RequestType                        client_request_type;
        uint64_t                           request_id_a;
        rocprofvis_dm_event_operation_t    op;
        const char*                        header;
    };

    const Desc descs[5] = {
        { TableType::kAnalysisTopInstrumentedEventsTable,
          TableType::kAnalysisTopInstrumentedEventsTableB,
          kRPVControllerTableTypeInstrumentedEvents,
          RequestType::kFetchAnalysisTopEventsTable,
          DataProvider::ANALYSIS_TOP_INSTRUMENTED_EVENTS_TABLE_REQUEST_ID,
          kRocProfVisDmOperationLaunch, "Top Instrumented Thread Events" },
        { TableType::kAnalysisTopDispatchEventsTable,
          TableType::kAnalysisTopDispatchEventsTableB,
          kRPVControllerTableTypeDispatchEvents,
          RequestType::kFetchAnalysisTopDispatchEventsTable,
          DataProvider::ANALYSIS_TOP_DISPATCH_EVENTS_TABLE_REQUEST_ID,
          kRocProfVisDmOperationDispatch, "Top Dispatch Events" },
        { TableType::kAnalysisTopMemoryAllocationEventsTable,
          TableType::kAnalysisTopMemoryAllocationEventsTableB,
          kRPVControllerTableTypeMemoryAllocationEvents,
          RequestType::kFetchAnalysisTopMemoryAllocationEventsTable,
          DataProvider::ANALYSIS_TOP_MEMORY_ALLOCATION_EVENTS_TABLE_REQUEST_ID,
          kRocProfVisDmOperationMemoryAllocate, "Top Memory Allocation Events" },
        { TableType::kAnalysisTopMemoryCopyEventsTable,
          TableType::kAnalysisTopMemoryCopyEventsTableB,
          kRPVControllerTableTypeMemoryCopyEvents,
          RequestType::kFetchAnalysisTopMemoryCopyEventsTable,
          DataProvider::ANALYSIS_TOP_MEMORY_COPY_EVENTS_TABLE_REQUEST_ID,
          kRocProfVisDmOperationMemoryCopy, "Top Memory Copy Events" },
        { TableType::kAnalysisTopSampledEventsTable,
          TableType::kAnalysisTopSampledEventsTableB, kRPVControllerTableTypeSampledEvents,
          RequestType::kFetchAnalysisTopLaunchSampleEventsTable,
          DataProvider::ANALYSIS_TOP_LAUNCH_SAMPLED_TABLE_REQUEST_ID,
          kRocProfVisDmOperationLaunchSample, "Top Sampled Thread Events" },
    };

    for(size_t i = 0; i < m_categories.size(); i++)
    {
        const Desc& desc          = descs[i];
        m_categories[i].header    = desc.header;
        if(m_compare_mode)
        {
            m_categories[i].table_a = std::make_unique<TopEventsTable>(
                data_provider, desc.type_a, desc.request_type,
                RequestIdBuilder::MakeClientRequestId(desc.client_request_type,
                                                      COMPARE_CLIENT_ID_A),
                timeline_selection, desc.op, desc.header, COMPARE_SOURCE_A_INDEX);
            m_categories[i].table_b = std::make_unique<TopEventsTable>(
                data_provider, desc.type_b, desc.request_type,
                RequestIdBuilder::MakeClientRequestId(desc.client_request_type,
                                                      COMPARE_CLIENT_ID_B),
                timeline_selection, desc.op, desc.header, COMPARE_SOURCE_B_INDEX);
            m_categories[i].table_a->SetDisplaySummary(false);
            m_categories[i].table_b->SetDisplaySummary(false);
            m_categories[i].table_a->SetHeaderRenderer(
                [this]() { RenderSourceBadge(COMPARE_SOURCE_A_INDEX); });
            m_categories[i].table_b->SetHeaderRenderer(
                [this]() { RenderSourceBadge(COMPARE_SOURCE_B_INDEX); });
        }
        else
        {
            m_categories[i].table_a = std::make_unique<TopEventsTable>(
                data_provider, desc.type_a, desc.request_type, desc.request_id_a,
                timeline_selection, desc.op, desc.header);
        }
    }
}

TopEventsView::~TopEventsView() {}

void
TopEventsView::Update()
{
    for(Category& category : m_categories)
    {
        if(category.table_a)
        {
            category.table_a->Update();
        }
        if(category.table_b)
        {
            category.table_b->Update();
        }
    }
}

void
TopEventsView::RenderSourceBadge(size_t source_index)
{
    const CompareSourceInfo* source =
        m_data_provider.DataModel().GetCompareSource(source_index);
    if(!source)
    {
        return;
    }
    SettingsManager& settings = SettingsManager::GetInstance();
    RenderCompareSourceBadge(*source, settings);
    // Columns run with ItemSpacing 0, so take the badge gap from the default style.
    ImGui::SameLine(0.0f, settings.GetDefaultStyle().ItemSpacing.x * COMPARE_TITLE_GAP);
    const std::string& label = source->name.empty() ? source->path : source->name;
    ImGui::AlignTextToFramePadding();
    ElidedText(label.c_str(), ImGui::GetContentRegionAvail().x, ImGui::GetFontSize() * 24.0f,
               Alignment_Left, true);
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
    if(m_compare_mode)
    {
        const float region_height =
            ImGui::GetWindowHeight() - 2.0f * style.WindowPadding.y;
        for(Category& category : m_categories)
        {
            const bool visible = (category.table_a && category.table_a->Visible()) ||
                                 (category.table_b && category.table_b->Visible());
            no_data &= !visible;
            if(!visible)
            {
                continue;
            }
            ImGui::PushID(category.header);
            if(ImGui::CollapsingHeader(category.header, ImGuiTreeNodeFlags_DefaultOpen))
            {
                float content = 0.0f;
                if(category.table_a)
                {
                    content = std::max(content, category.table_a->ContentHeight());
                }
                if(category.table_b)
                {
                    content = std::max(content, category.table_b->ContentHeight());
                }
                const float height =
                    std::min(region_height - ImGui::GetFrameHeightWithSpacing(), content);
                const float half =
                    (ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) * 0.5f;
                if(category.table_a)
                {
                    ImGui::PushID("A");
                    category.table_a->RenderBody(ImVec2(half, height));
                    ImGui::PopID();
                }
                ImGui::SameLine();
                if(category.table_b)
                {
                    ImGui::PushID("B");
                    category.table_b->RenderBody(ImVec2(half, height));
                    ImGui::PopID();
                }
            }
            ImGui::PopID();
        }
    }
    else
    {
        for(Category& category : m_categories)
        {
            if(category.table_a)
            {
                category.table_a->Render();
                no_data &= !category.table_a->Visible();
            }
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
    for(Category& category : m_categories)
    {
        if(category.table_a)
        {
            category.table_a->HandleTrackSelectionChanged(track_id, selected);
        }
        if(category.table_b)
        {
            category.table_b->HandleTrackSelectionChanged(track_id, selected);
        }
    }
}

void
TopEventsView::HandleTimeRangeSelectionChanged(double start_ns, double end_ns)
{
    for(Category& category : m_categories)
    {
        if(category.table_a)
        {
            category.table_a->HandleTimeRangeSelectionChanged(start_ns, end_ns);
        }
        if(category.table_b)
        {
            category.table_b->HandleTimeRangeSelectionChanged(start_ns, end_ns);
        }
    }
}

TopEventsView::TopEventsTable::TopEventsTable(
    DataProvider& dp, TableType table_type,
    rocprofvis_controller_table_type_t request_table_type, uint64_t request_id,
    std::shared_ptr<TimelineSelection> timeline_selection,
    rocprofvis_dm_event_operation_t op, const char* header,
    std::optional<uint64_t> source_index)
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
, m_source_index(source_index)
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
TopEventsView::TopEventsTable::RenderBody(const ImVec2& size)
{
    // Draw even when empty so the columns stay aligned; the caller scopes the id.
    ImGui::SetNextWindowSize(size);
    MultiTrackTable::Render();
}

float
TopEventsView::TopEventsTable::ContentHeight() const
{
    const ImGuiStyle& style = SettingsManager::GetInstance().GetDefaultStyle();
    return (Rows() + 1) * TableRowHeight() + 2.0f * ImGui::GetFrameHeightWithSpacing() +
           2.0f * style.WindowPadding.y;
}

void
TopEventsView::TopEventsTable::HandleTrackSelectionChanged(uint64_t track_id, bool selected)
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
        if(include && m_source_index.has_value())
        {
            include = track_info->file_id == m_source_index.value();
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
