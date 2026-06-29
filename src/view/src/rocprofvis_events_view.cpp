// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_events_view.h"
#include "icons/rocprovfis_icon_defines.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_utils.h"

#include <array>
#include <string_view>
#include <vector>

namespace RocProfVis
{
namespace View
{

constexpr ImGuiTableFlags TABLE_FLAGS = ImGuiTableFlags_BordersOuter |
                                        ImGuiTableFlags_RowBg |
                                        ImGuiTableFlags_Resizable |
                                        ImGuiTableFlags_SizingStretchProp;

constexpr int kFlowColumnCount      = 6;
constexpr int kCallStackColumnCount = 5;
constexpr int kArgColumnCount       = 4;
constexpr int kBasicColumnCount     = 2;
constexpr int kExtColumnCount       = 2;

namespace
{

void
PushSectionHeaderStyle(SettingsManager& settings)
{
    ImGui::PushStyleColor(ImGuiCol_Header, settings.GetColor(Colors::kBgFrame));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                          settings.GetColor(Colors::kButtonHovered));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                          settings.GetColor(Colors::kButtonActive));
}

}  // namespace

bool
EventsView::FlowHighlightState::IsValid() const
{
    return flow_event_id != TimelineSelection::INVALID_SELECTION_ID;
}

void
EventsView::FlowHighlightState::Reset()
{
    owner_event_id = TimelineSelection::INVALID_SELECTION_ID;
    flow_event_id  = TimelineSelection::INVALID_SELECTION_ID;
    flow_track_id  = TimelineSelection::INVALID_SELECTION_ID;
}

EventsView::EventsView(DataProvider&                      dp,
                       std::shared_ptr<TimelineSelection> timeline_selection)
: m_data_provider(dp)
, m_settings(SettingsManager::GetInstance())
, m_timeline_selection(timeline_selection)
, m_event_item_id(0)
{
    static_assert(CallStackHoverState::kInvalidId ==
                      TimelineSelection::INVALID_SELECTION_ID,
                  "CallStackHoverState invalid sentinel must match "
                  "TimelineSelection::INVALID_SELECTION_ID");
    m_flow_hover.Reset();
    m_frame_flow_hover.Reset();
}

EventsView::~EventsView()
{
    if(m_callstack_hover.frame_event_id != TimelineSelection::INVALID_SELECTION_ID)
    {
        m_timeline_selection->UnhighlightTrackEvent(m_callstack_hover.frame_track_id,
                                                    m_callstack_hover.frame_event_id);
    }
    if(m_flow_hover.IsValid())
    {
        m_timeline_selection->UnhighlightTrackEvent(m_flow_hover.flow_track_id,
                                                    m_flow_hover.flow_event_id);
    }
}

void
EventsView::Render()
{
    const ImGuiStyle& style = m_settings.GetDefaultStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, style.ChildRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.WindowPadding);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, m_settings.GetColor(Colors::kBgPanel));
    ImGui::PushStyleColor(ImGuiCol_Border, m_settings.GetColor(Colors::kBorderColor));
    ImGui::BeginChild("events_view", ImVec2(0, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
    m_frame_callstack_hover = {};
    m_frame_flow_hover.Reset();
    // Use subtler borders for nested event panes.
    ImGui::PushStyleColor(ImGuiCol_Border,
                          m_settings.GetColor(Colors::kPanelBorderSubtle));
    ImGui::PushStyleColor(ImGuiCol_TableBorderStrong,
                          m_settings.GetColor(Colors::kTableBorderOuter));
    ImGui::PushStyleColor(ImGuiCol_TableBorderLight,
                          m_settings.GetColor(Colors::kTableBorderInner));
    if(m_event_items.empty())
    {
        CenterNextTextItem("No data available for the selected events.");
        ImGui::SetCursorPosY((ImGui::GetWindowHeight() - ImGui::GetTextLineHeight()) *
                             0.5f);
        ImGui::TextDisabled("No data available for the selected events.");
    }
    else
    {
        float x_button_width =
            ImGui::CalcTextSize("X").x + ImGui::GetStyle().FramePadding.x;
        for(EventItem& item : m_event_items)
        {
            if(item.info && item.contents)
            {
                bool deselect_event = false;
                ImGui::PushID(item.id);
                ImGui::SetNextItemAllowOverlap();

                PushSectionHeaderStyle(m_settings);
                bool event_open = ImGui::CollapsingHeader(
                    item.header.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
                ImGui::PopStyleColor(3);

                if(event_open)
                {
                    ImGui::SameLine();
                    ImGui::Dummy(
                        ImVec2(ImGui::GetContentRegionAvail().x - x_button_width, 0));
                    ImGui::SameLine();
                    deselect_event = XButton();

                    ImGui::BeginChild("EventDetails", ImVec2(0, item.height),
                                      ImGuiChildFlags_None);
                    item.contents->Render();
                    ImGui::EndChild();


                    // Use the optimal height of the contents as the new height for the
                    // next frame
                    if(item.contents)
                    {
                        item.height = item.contents->GetOptimalHeight();
                    }
                }
                else
                {
                    ImGui::SameLine();
                    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x - x_button_width,
                                        ImGui::GetFrameHeightWithSpacing()));
                    ImGui::SameLine();
                    deselect_event = XButton();
                }
                if(deselect_event)
                {
                    m_timeline_selection->UnselectTrackEvent(item.info->track_id,
                                                             item.info->basic_info.id.uuid);
                }
                ImGui::PopID();
            }
        }
    }
    if(m_frame_callstack_hover.frame_event_id != m_callstack_hover.frame_event_id)
    {
        if(m_callstack_hover.frame_event_id != TimelineSelection::INVALID_SELECTION_ID)
        {
            m_timeline_selection->UnhighlightTrackEvent(m_callstack_hover.frame_track_id,
                                                        m_callstack_hover.frame_event_id);
        }
        if(m_frame_callstack_hover.frame_event_id != TimelineSelection::INVALID_SELECTION_ID)
        {
            m_timeline_selection->HighlightTrackEvent(m_frame_callstack_hover.frame_track_id,
                                                      m_frame_callstack_hover.frame_event_id);
        }
    }
    m_callstack_hover = m_frame_callstack_hover;
    if(m_frame_flow_hover.flow_event_id != m_flow_hover.flow_event_id)
    {
        if(m_flow_hover.IsValid())
        {
            m_timeline_selection->UnhighlightTrackEvent(m_flow_hover.flow_track_id,
                                                        m_flow_hover.flow_event_id);
        }
        if(m_frame_flow_hover.IsValid())
        {
            m_timeline_selection->HighlightTrackEvent(m_frame_flow_hover.flow_track_id,
                                                      m_frame_flow_hover.flow_event_id);
        }
    }
    m_flow_hover = m_frame_flow_hover;
    ImGui::PopStyleColor(3);
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

bool
EventsView::RenderBasicData(const EventInfo* event_data)
{
    const auto& info = event_data->basic_info;
    const uint64_t& db_id = event_data->basic_info.id.bitfield.event_id;
    double trace_start_time = m_data_provider.DataModel().GetTimeline().GetStartTime();
    const auto& time_format = m_settings.GetUserSettings().unit_settings.time_format;
    std::string db_id_label  = std::to_string(db_id);
    std::string start_label  = nanosecond_to_formatted_str(info.start_ts - trace_start_time,
                                                           time_format, true);
    std::string duration_label = nanosecond_to_formatted_str(info.duration,
                                                             time_format, true);

    std::vector<std::array<std::string, kBasicColumnCount>> rows = {
        { std::string("ID"), db_id_label },
        { std::string("Name"), info.name },
        { std::string("Start"), start_label },
        { std::string("Duration"), duration_label },
    };
#ifdef ROCPROFVIS_DEVELOPER_MODE
    rows.push_back({ std::string("Level"), std::to_string(info.level) });
#endif

    if(ImGui::BeginTable("EventSummaryTable", kBasicColumnCount,
                         ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
    {
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed,
                                ImGui::GetFontSize() * 8.5f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        bool open_menu = false;

        for(int i = 0; i < static_cast<int>(rows.size()); i++)
        {
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            RenderRowHitbox("##basic_sel", i, kBasicColumnCount, m_basic_menu, open_menu);

            PositionCell(0);
            ImGui::TextDisabled("%s", rows[i][0].c_str());
            CaptureCellRightClick(0, i, m_basic_menu, open_menu);

            PositionCell(1);
            CopyableTextUnformatted(rows[i][1].c_str(), "##basic_value",
                                    COPY_DATA_NOTIFICATION, false, false);
            CaptureCellRightClick(1, i, m_basic_menu, open_menu);
            ImGui::PopID();
        }

        if(open_menu)
        {
            ImGui::OpenPopup("##BasicContextMenu");
        }

        if(BeginCellContextMenu("##BasicContextMenu"))
        {
            if(m_basic_menu.row >= 0 &&
               m_basic_menu.row < static_cast<int>(rows.size()))
            {
                AddCopyRowCellMenuItems(rows[m_basic_menu.row].data(), kBasicColumnCount,
                                        m_basic_menu.column);
            }
            EndCellContextMenu();
        }

        ImGui::EndTable();
    }
    return true;
}

bool
EventsView::RenderEventExtData(const EventInfo* event_data)
{
    if(event_data->ext_info.empty())
    {
        return false;
    }

    PushSectionHeaderStyle(m_settings);

    // --- Expandable full extended data ---
    if(ImGui::CollapsingHeader("Event Extended Data", ImGuiTreeNodeFlags_None))
    {
        if(event_data->ext_info.empty())
        {
            ImGui::TextUnformatted("No data available.");
        }
        else
        {
            if(ImGui::BeginTable("ExtDataTable", kExtColumnCount, TABLE_FLAGS))
            {
                ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                TimeFormat time_format =
                    m_settings.GetUserSettings().unit_settings.time_format;

                auto format_ext_value = [&](int i) -> std::string {
                    double offset_ns = 0;
                    switch(event_data->ext_info[i].category_enum)
                    {
                        case kRocProfVisEventEssentialDataStart:
                        case kRocProfVisEventEssentialDataEnd:
                            offset_ns =
                                m_data_provider.DataModel().GetTimeline().GetStartTime();
                        case kRocProfVisEventEssentialDataDuration:
                            return nanosecond_str_to_formatted_str(
                                event_data->ext_info[i].value, offset_ns, time_format,
                                true);
                        default:
                            return event_data->ext_info[i].value;
                    }
                };

                bool open_menu = false;

                for(int i = 0; i < static_cast<int>(event_data->ext_info.size()); i++)
                {
                    ImGui::PushID(i);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    RenderRowHitbox("##ext_sel", i, kExtColumnCount, m_ext_menu, open_menu);

                    PositionCell(0);
                    CopyableTextUnformatted(event_data->ext_info[i].name.c_str(),
                                            "##ext_name", COPY_DATA_NOTIFICATION, false,
                                            false);
                    CaptureCellRightClick(0, i, m_ext_menu, open_menu);

                    PositionCell(1);
                    CopyableTextUnformatted(format_ext_value(i).c_str(), "##ext_value",
                                            COPY_DATA_NOTIFICATION, false, false);
                    CaptureCellRightClick(1, i, m_ext_menu, open_menu);
                    ImGui::PopID();
                }

                if(open_menu)
                {
                    ImGui::OpenPopup("##ExtContextMenu");
                }

                if(BeginCellContextMenu("##ExtContextMenu"))
                {
                    if(m_ext_menu.row >= 0 &&
                       m_ext_menu.row < static_cast<int>(event_data->ext_info.size()))
                    {
                        std::string cells[kExtColumnCount] = {
                            event_data->ext_info[m_ext_menu.row].name,
                            format_ext_value(m_ext_menu.row) };
                        AddCopyRowCellMenuItems(cells, kExtColumnCount, m_ext_menu.column);
                    }
                    EndCellContextMenu();
                }

                ImGui::EndTable();
            }
        }
    }

    ImGui::PopStyleColor(3);
    return true;
}

bool
EventsView::RenderEventFlowInfo(const EventInfo* event_data)
{
    if(event_data->flow_info.empty())
    {
        return false;
    }

    PushSectionHeaderStyle(m_settings);

    if(ImGui::CollapsingHeader("Flow Data", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if(event_data->flow_info.empty())
        {
            ImGui::TextUnformatted("No data available.");
        }
        else
        {
            if(ImGui::BeginTable("FlowInfoTable", kFlowColumnCount, TABLE_FLAGS))
            {
                ImGui::TableSetupColumn("ID");
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Timestamp");
                ImGui::TableSetupColumn("Track ID");
                ImGui::TableSetupColumn("Level");
                ImGui::TableSetupColumn("Direction");
                ImGui::TableHeadersRow();

                double trace_start_time =
                    m_data_provider.DataModel().GetTimeline().GetStartTime();
                const auto& time_format =
                    m_settings.GetUserSettings().unit_settings.time_format;

                const uint64_t this_owner_event_id = event_data->basic_info.id.uuid;
                const uint64_t prev_hovered_flow_event_id =
                    (m_flow_hover.owner_event_id == this_owner_event_id)
                        ? m_flow_hover.flow_event_id
                        : TimelineSelection::INVALID_SELECTION_ID;

                bool open_menu = false;

                auto flow_cell = [&](int col, const char* text, std::string_view id,
                                     int row) {
                    PositionCell(col);
                    CopyableTextUnformatted(text, id, COPY_DATA_NOTIFICATION, false,
                                            false);
                    CaptureCellRightClick(col, row, m_flow_menu, open_menu);
                };

                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(event_data->flow_info.size()));
                while(clipper.Step())
                {
                    for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                    {
                        const auto& flow = event_data->flow_info[i];

                        ImGui::PushID(i);
                        ImGui::TableNextRow();
                        if(flow.id.uuid == prev_hovered_flow_event_id)
                        {
                            ImGui::TableSetBgColor(
                                ImGuiTableBgTarget_RowBg0,
                                m_settings.GetColor(Colors::kHighlightChart));
                        }
                        else if(flow.id.uuid == this_owner_event_id)
                        {
                            ImGui::TableSetBgColor(
                                ImGuiTableBgTarget_RowBg0,
                                m_settings.GetColor(Colors::kAreaOfInterest));
                        }
                        ImGui::TableSetColumnIndex(0);
                        bool row_hovered = RenderRowHitbox("##flow_sel", i,
                                                           kFlowColumnCount, m_flow_menu,
                                                           open_menu);

                        std::string id_str        = std::to_string(flow.id.bitfield.event_id);
                        std::string timestamp_str = nanosecond_to_formatted_str(
                            flow.start_timestamp - trace_start_time, time_format, true);
                        std::string track_str     = std::to_string(flow.track_id);
                        std::string level_str     = std::to_string(flow.level);
                        std::string dir_str       = std::to_string(flow.direction);

                        flow_cell(0, id_str.c_str(),        "##flow_id", i);
                        flow_cell(1, flow.name.c_str(),     "##flow_name", i);
                        flow_cell(2, timestamp_str.c_str(), "##flow_ts", i);
                        flow_cell(3, track_str.c_str(),     "##flow_tid", i);
                        flow_cell(4, level_str.c_str(),     "##flow_lvl", i);
                        flow_cell(5, dir_str.c_str(),       "##flow_dir", i);

                        if(row_hovered &&
                           ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        {
                            m_timeline_selection->NavigateToEvent(
                                flow.track_id, flow.id.uuid,
                                static_cast<double>(flow.start_timestamp),
                                static_cast<double>(flow.end_timestamp -
                                                    flow.start_timestamp));
                        }
                        if(row_hovered)
                        {
                            m_frame_flow_hover.owner_event_id = this_owner_event_id;
                            m_frame_flow_hover.flow_event_id  = flow.id.uuid;
                            m_frame_flow_hover.flow_track_id  = flow.track_id;
                            if(flow.id.uuid == this_owner_event_id)
                            {
                                SetTooltipStyled(
                                    "This is the selected event for this flow.");
                            }
                        }
                        ImGui::PopID();
                    }
                }

                if(open_menu)
                {
                    ImGui::OpenPopup("##FlowContextMenu");
                }

                if(BeginCellContextMenu("##FlowContextMenu"))
                {
                    if(m_flow_menu.row >= 0 &&
                       m_flow_menu.row <
                           static_cast<int>(event_data->flow_info.size()))
                    {
                        const auto& ctx_flow = event_data->flow_info[m_flow_menu.row];

                        if(IconMenuItem(ICON_ARROW_FORWARD, "Go To Event"))
                        {
                            m_timeline_selection->NavigateToEvent(
                                ctx_flow.track_id, ctx_flow.id.uuid,
                                static_cast<double>(ctx_flow.start_timestamp),
                                static_cast<double>(ctx_flow.end_timestamp -
                                                    ctx_flow.start_timestamp));
                        }

                        std::string cells[] = {
                            std::to_string(ctx_flow.id.bitfield.event_id),
                            ctx_flow.name,
                            nanosecond_to_formatted_str(
                                ctx_flow.start_timestamp - trace_start_time,
                                time_format, true),
                            std::to_string(ctx_flow.track_id),
                            std::to_string(ctx_flow.level),
                            std::to_string(ctx_flow.direction)};

                        AddCopyRowCellMenuItems(cells, kFlowColumnCount,
                                                m_flow_menu.column);
                    }
                    EndCellContextMenu();
                }

                ImGui::EndTable();
            }
        }
    }
    ImGui::PopStyleColor(3);
    return true;
}

bool
EventsView::RenderCallStackData(const EventInfo* event_data)
{
    if(event_data->call_stack_info.empty())
    {
        return false;
    }

    PushSectionHeaderStyle(m_settings);

    if(ImGui::CollapsingHeader("Call Stack Data", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if(event_data->call_stack_info.empty())
        {
            ImGui::TextUnformatted("No data available.");
        }
        else
        {
            if(ImGui::BeginTable("CallStackTable", kCallStackColumnCount, TABLE_FLAGS))
            {
                ImGui::TableSetupColumn("ID");
                ImGui::TableSetupColumn("Address");
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("File");
                ImGui::TableSetupColumn("PC");
                ImGui::TableHeadersRow();

                bool open_menu = false;

                auto callstack_cell = [&](int col, const std::string& text,
                                           std::string_view col_id, int row) {
                    PositionCell(col);

                    if(text.empty())
                    {
                        ImVec4 disabled_col =
                            ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
                        ImGui::PushStyleColor(ImGuiCol_Text, disabled_col);
                        CopyableTextUnformatted("N/A", col_id, COPY_DATA_NOTIFICATION,
                                                false, false);
                        ImGui::PopStyleColor();
                    }
                    else
                    {
                        CopyableTextUnformatted(text.c_str(), col_id,
                                                COPY_DATA_NOTIFICATION, false, false);
                    }

                    CaptureCellRightClick(col, row, m_callstack_menu, open_menu);
                };

                const uint64_t this_owner_event_id = event_data->basic_info.id.uuid;
                const uint64_t prev_hovered_frame_event_id =
                    (m_callstack_hover.owner_event_id == this_owner_event_id)
                        ? m_callstack_hover.frame_event_id
                        : TimelineSelection::INVALID_SELECTION_ID;

                auto navigate_to_frame = [&](uint64_t uuid) {
                    m_timeline_selection->NavigateToEvent(event_data->track_id, uuid,
                                                          event_data->basic_info.start_ts,
                                                          event_data->basic_info.duration);
                };

                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(event_data->call_stack_info.size()));
                while(clipper.Step())
                {
                    for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                    {
                        const auto& frame    = event_data->call_stack_info[i];
                        const uint64_t uuid  = frame.id.uuid;
                        const bool     is_owner_frame = (uuid == this_owner_event_id);

                        ImGui::PushID(i);
                        ImGui::TableNextRow();
                        if(uuid == prev_hovered_frame_event_id)
                        {
                            ImGui::TableSetBgColor(
                                ImGuiTableBgTarget_RowBg0,
                                m_settings.GetColor(Colors::kHighlightChart));
                        }
                        else if(is_owner_frame)
                        {
                            ImGui::TableSetBgColor(
                                ImGuiTableBgTarget_RowBg0,
                                m_settings.GetColor(Colors::kAreaOfInterest));
                        }

                        ImGui::TableSetColumnIndex(0);
                        bool row_hovered = RenderRowHitbox("##cs_sel", i,
                                                           kCallStackColumnCount,
                                                           m_callstack_menu, open_menu);

                        callstack_cell(0, std::to_string(frame.id.bitfield.event_id),
                                       "##cs_id", i);
                        callstack_cell(1, frame.address, "##cs_addr", i);
                        callstack_cell(2, frame.name, "##cs_name", i);
                        callstack_cell(3, frame.file, "##cs_file", i);
                        callstack_cell(4, frame.pc, "##cs_pc", i);

                        if(row_hovered)
                        {
                            m_frame_callstack_hover.owner_event_id = this_owner_event_id;
                            m_frame_callstack_hover.frame_event_id = uuid;
                            m_frame_callstack_hover.frame_track_id = event_data->track_id;
                            if(is_owner_frame)
                            {
                                SetTooltipStyled(
                                    "This is the event you opened to view "
                                    "this call stack.");
                            }
                        }
                        if(row_hovered &&
                           ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        {
                            navigate_to_frame(uuid);
                        }
                        ImGui::PopID();
                    }
                }

                if(open_menu)
                {
                    ImGui::OpenPopup("##CallStackContextMenu");
                }

                if(BeginCellContextMenu("##CallStackContextMenu"))
                {
                    if(m_callstack_menu.row >= 0 &&
                       m_callstack_menu.row <
                           static_cast<int>(event_data->call_stack_info.size()))
                    {
                        const auto& ctx_frame =
                            event_data->call_stack_info[m_callstack_menu.row];

                        if(IconMenuItem(ICON_ARROW_FORWARD, "Go To Event"))
                        {
                            navigate_to_frame(ctx_frame.id.uuid);
                        }

                        std::string cells[] = {
                            std::to_string(ctx_frame.id.bitfield.event_id),
                            ctx_frame.address, ctx_frame.name, ctx_frame.file,
                            ctx_frame.pc};

                        AddCopyRowCellMenuItems(cells, kCallStackColumnCount,
                                                m_callstack_menu.column);
                    }
                    EndCellContextMenu();
                }
                ImGui::EndTable();
            }
        }
    }
    ImGui::PopStyleColor(3);
    return true;
}

bool
EventsView::RenderArgumentData(const EventInfo* event_data)
{
    if(event_data->args.empty())
    {
        return false;
    }

    PushSectionHeaderStyle(m_settings);

    if(ImGui::CollapsingHeader("Arguments", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if(event_data->args.empty())
        {
            ImGui::TextUnformatted("No data available.");
        }
        else
        {
            if(ImGui::BeginTable("EventArgTable", kArgColumnCount, TABLE_FLAGS))
            {
                ImGui::TableSetupColumn("Pos");
                ImGui::TableSetupColumn("Type");
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Value");
                ImGui::TableHeadersRow();

                bool open_menu = false;

                auto arg_cell = [&](int col, const char* text, std::string_view col_id,
                                    int row) {
                    PositionCell(col);
                    CopyableTextUnformatted(text, col_id, COPY_DATA_NOTIFICATION, false,
                                            false);
                    CaptureCellRightClick(col, row, m_arg_menu, open_menu);
                };

                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(event_data->args.size()));
                while(clipper.Step())
                {
                    for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                    {
                        ImGui::PushID(i);
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        RenderRowHitbox("##arg_sel", i, kArgColumnCount, m_arg_menu,
                                        open_menu);

                        std::string pos_str =
                            std::to_string(event_data->args[i].position);

                        arg_cell(0, pos_str.c_str(), "##arg_pos", i);
                        arg_cell(1, event_data->args[i].data_type.c_str(), "##arg_type", i);
                        arg_cell(2, event_data->args[i].name.c_str(), "##arg_name", i);
                        arg_cell(3, event_data->args[i].value.c_str(), "##arg_value", i);
                        ImGui::PopID();
                    }
                }

                if(open_menu)
                {
                    ImGui::OpenPopup("##ArgContextMenu");
                }

                if(BeginCellContextMenu("##ArgContextMenu"))
                {
                    if(m_arg_menu.row >= 0 &&
                       m_arg_menu.row < static_cast<int>(event_data->args.size()))
                    {
                        const auto& ctx_arg = event_data->args[m_arg_menu.row];

                        std::string cells[] = { std::to_string(ctx_arg.position),
                                                ctx_arg.data_type, ctx_arg.name,
                                                ctx_arg.value };

                        AddCopyRowCellMenuItems(cells, kArgColumnCount, m_arg_menu.column);
                    }
                    EndCellContextMenu();
                }

                ImGui::EndTable();
            }
        }
    }
    ImGui::PopStyleColor(3);
    return true;
}

bool
EventsView::XButton()
{
    bool clicked = RocProfVis::View::XButton(nullptr, "Unselect Event", &m_settings);
    return clicked;
}

void
EventsView::HandleEventSelectionChanged(const uint64_t event_id, const bool selected)
{
    if(selected)
    {
        const EventInfo* event_data =
            m_data_provider.DataModel().GetEvents().GetEvent(event_id);
        if(event_data)
        {
            auto            default_style = m_settings.GetDefaultStyle();
            LayoutItem::Ptr left          = std::make_shared<LayoutItem>();
            left->m_item = std::make_shared<RocCustomWidget>([this, event_data]() {
                this->RenderBasicData(event_data);
                ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().ItemSpacing.y * 0.5f));
                this->RenderEventExtData(event_data);
            });
            left->m_window_padding = default_style.WindowPadding;
            left->m_item_spacing   = default_style.ItemSpacing;
            left->m_inherit_bg_color = true;
            left->m_child_flags =
                ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders |
                ImGuiChildFlags_AlwaysUseWindowPadding;

            LayoutItem::Ptr right = std::make_shared<LayoutItem>();
            right->m_item = std::make_shared<RocCustomWidget>([this, event_data]() {
                if(this->RenderArgumentData(event_data))
                {
                    ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().ItemSpacing.y * 0.35f));
                }
                if(this->RenderEventFlowInfo(event_data))
                {
                    ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().ItemSpacing.y * 0.35f));
                }
                this->RenderCallStackData(event_data);
            });
            right->m_window_padding = default_style.WindowPadding;
            right->m_item_spacing   = default_style.ItemSpacing;
            right->m_inherit_bg_color = true;
            right->m_child_flags =
                ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders |
                ImGuiChildFlags_AlwaysUseWindowPadding;

            std::unique_ptr<HSplitContainer> container =
                std::make_unique<HSplitContainer>(left, right);
            container->SetMinLeftWidth(260.0f);
            container->SetMinRightWidth(260.0f);
            container->SetSplit(0.5f);

            // Get DB from event ID
            const uint64_t& db_id = event_data->basic_info.id.bitfield.event_id;

            m_event_items.emplace_front(
                EventItem{ m_event_item_id++, event_data->basic_info.id.uuid,
                           "Event ID: " + std::to_string(db_id), std::move(container),
                           event_data, 0.0f });
        }
    }
    else if(event_id == TimelineSelection::INVALID_SELECTION_ID)
    {
        m_event_items.clear();
    }
    else
    {
        m_event_items.remove({ 0, event_id, "", nullptr, nullptr, 0 });
    }
}

}  // namespace View
}  // namespace RocProfVis
