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
#include "widgets/rocprofvis_notification_manager.h"

namespace RocProfVis
{
namespace View
{

constexpr ImGuiTableFlags TABLE_FLAGS = ImGuiTableFlags_BordersOuter |
                                        ImGuiTableFlags_RowBg |
                                        ImGuiTableFlags_Resizable |
                                        ImGuiTableFlags_SizingStretchProp;

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
, m_context_menu_flow_index(-1)
, m_context_menu_flow_column(-1)
{
    m_flow_hover.Reset();
    m_frame_flow_hover.Reset();
}

EventsView::~EventsView()
{
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
    m_frame_flow_hover.Reset();
    if(m_event_items.empty())
    {
        ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().ItemSpacing.y * 0.5f));
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

    if(ImGui::BeginTable("EventSummaryTable", 2,
                         ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
    {
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed,
                                ImGui::GetFontSize() * 8.5f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        const auto row = [&](const char* label, const char* value, const char* id) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", label);
            ImGui::TableNextColumn();
            CopyableTextUnformatted(value, id, DATA_COPIED_NOTIFICATION, false, true);
        };

        row("ID", db_id_label.c_str(), "ID");
        row("Name", info.name.c_str(), "Name");
        row("Start", start_label.c_str(), "Start_time");
        row("Duration", duration_label.c_str(), "Duration");

#ifdef ROCPROFVIS_DEVELOPER_MODE
        std::string level_label = std::to_string(info.level);
        row("Level", level_label.c_str(), "Level");
#endif
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
            if(ImGui::BeginTable("ExtDataTable", 2, TABLE_FLAGS))
            {
                ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                double     offset_ns = 0;
                TimeFormat time_format =
                    m_settings.GetUserSettings().unit_settings.time_format;

                for(size_t i = 0; i < event_data->ext_info.size(); ++i)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    CopyableTextUnformatted(event_data->ext_info[i].name.c_str(),
                                            std::to_string(i), COPY_DATA_NOTIFICATION,
                                            false, true);
                    ImGui::TableSetColumnIndex(1);

                    switch(event_data->ext_info[i].category_enum)
                    {
                        case kRocProfVisEventEssentialDataStart:
                        case kRocProfVisEventEssentialDataEnd:
                            offset_ns =
                                m_data_provider.DataModel().GetTimeline().GetStartTime();
                        case kRocProfVisEventEssentialDataDuration:
                        {
                            CopyableTextUnformatted(nanosecond_str_to_formatted_str(
                                                        event_data->ext_info[i].value,
                                                        offset_ns, time_format, true)
                                                        .c_str(),
                                                    std::to_string(i),
                                                    COPY_DATA_NOTIFICATION, false, true);
                            offset_ns = 0;
                            break;
                        }
                        default:
                            CopyableTextUnformatted(event_data->ext_info[i].value.c_str(),
                                                    std::to_string(i),
                                                    COPY_DATA_NOTIFICATION, false, true);
                            break;
                    }
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
            if(ImGui::BeginTable("FlowInfoTable", 6, TABLE_FLAGS))
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

                auto flow_cell = [&](int col, const char* text, const char* id, int row)
                {
                    if(col > 0)
                        ImGui::TableSetColumnIndex(col);
                    else
                        ImGui::SameLine();
                    CopyableTextUnformatted(text, id, COPY_DATA_NOTIFICATION,
                                            false, false);
                    if(ImGui::IsItemClicked(ImGuiMouseButton_Right))
                    {
                        m_context_menu_flow_index  = row;
                        m_context_menu_flow_column = col;
                        ImGui::OpenPopup("##FlowContextMenu");
                    }
                };

                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(event_data->flow_info.size()));
                while(clipper.Step())
                {
                    for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                    {
                        const auto& flow = event_data->flow_info[i];
                        std::string row_str = std::to_string(i);

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

                        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
                        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
                        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));
                        bool row_clicked = ImGui::Selectable(
                            ("##flow_sel_" + row_str).c_str(), false,
                            ImGuiSelectableFlags_SpanAllColumns |
                                ImGuiSelectableFlags_AllowOverlap,
                            ImVec2(0.0f, 0.0f));
                        bool row_hovered =
                            row_clicked ||
                            ImGui::IsItemHovered(
                                ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
                                ImGuiHoveredFlags_AllowWhenOverlappedByItem);
                        if(ImGui::IsItemClicked(ImGuiMouseButton_Right))
                        {
                            m_context_menu_flow_index  = i;
                            m_context_menu_flow_column = 0;
                            ImGui::OpenPopup("##FlowContextMenu");
                        }
                        ImGui::PopStyleColor(3);

                        std::string id_str        = std::to_string(flow.id.bitfield.event_id);
                        std::string timestamp_str = nanosecond_to_formatted_str(
                            flow.start_timestamp - trace_start_time, time_format, true);
                        std::string track_str     = std::to_string(flow.track_id);
                        std::string level_str     = std::to_string(flow.level);
                        std::string dir_str       = std::to_string(flow.direction);

                        flow_cell(0, id_str.c_str(),        ("##id_" + row_str).c_str(), i);
                        flow_cell(1, flow.name.c_str(),     ("##name_" + row_str).c_str(), i);
                        flow_cell(2, timestamp_str.c_str(), ("##ts_" + row_str).c_str(), i);
                        flow_cell(3, track_str.c_str(),     ("##tid_" + row_str).c_str(), i);
                        flow_cell(4, level_str.c_str(),     ("##lvl_" + row_str).c_str(), i);
                        flow_cell(5, dir_str.c_str(),       ("##dir_" + row_str).c_str(), i);

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
                    }
                }

                auto style = m_settings.GetDefaultStyle();
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.WindowPadding);
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, style.ItemSpacing);
                if(ImGui::BeginPopup("##FlowContextMenu"))
                {
                    if(m_context_menu_flow_index >= 0 &&
                       m_context_menu_flow_index <
                           static_cast<int>(event_data->flow_info.size()))
                    {
                        const auto& ctx_flow =
                            event_data->flow_info[m_context_menu_flow_index];

                        if(ImGui::MenuItem("Go To Event"))
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

                        if(ImGui::MenuItem("Copy Row Data"))
                        {
                            std::string row_text;
                            for(int c = 0; c < 6; c++)
                            {
                                if(c > 0) row_text += '\t';
                                row_text += cells[c];
                            }
                            ImGui::SetClipboardText(row_text.c_str());
                        }
                        if(ImGui::MenuItem("Copy Cell Data"))
                        {
                            int col = m_context_menu_flow_column;
                            if(col >= 0 && col < 6)
                                ImGui::SetClipboardText(cells[col].c_str());
                        }
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopStyleVar(2);

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
            if(ImGui::BeginTable("CallStackTable", 4, TABLE_FLAGS))
            {
                ImGui::TableSetupColumn("Address");
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("File");
                ImGui::TableSetupColumn("PC");
                ImGui::TableHeadersRow();

                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(event_data->call_stack_info.size()));
                while(clipper.Step())
                {
                    for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                    {
                        ImGui::PushID(i);
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        CopyableTextUnformatted(
                            event_data->call_stack_info[i].address.c_str(), "",
                            COPY_DATA_NOTIFICATION, false, true);
                        ImGui::TableSetColumnIndex(1);
                        CopyableTextUnformatted(
                            event_data->call_stack_info[i].name.c_str(), "",
                            COPY_DATA_NOTIFICATION, false, true);
                        ImGui::TableSetColumnIndex(2);
                        CopyableTextUnformatted(
                            event_data->call_stack_info[i].file.c_str(), "",
                            COPY_DATA_NOTIFICATION, false, true);
                        ImGui::TableSetColumnIndex(3);
                        CopyableTextUnformatted(event_data->call_stack_info[i].pc.c_str(),
                                                "", COPY_DATA_NOTIFICATION, false, true);
                        ImGui::PopID();
                    }
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
            if(ImGui::BeginTable("EventArgTable", 4, TABLE_FLAGS))
            {
                ImGui::TableSetupColumn("Pos");
                ImGui::TableSetupColumn("Type");
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Value");
                ImGui::TableHeadersRow();
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(event_data->args.size()));
                while(clipper.Step())
                {
                    for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                    {
                        ImGui::PushID(i);
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        CopyableTextUnformatted(
                            std::to_string(event_data->args[i].position).c_str(), "",
                            COPY_DATA_NOTIFICATION, false, true);
                        ImGui::TableSetColumnIndex(1);
                        CopyableTextUnformatted(event_data->args[i].data_type.c_str(), "",
                                                COPY_DATA_NOTIFICATION, false, true);
                        ImGui::TableSetColumnIndex(2);
                        CopyableTextUnformatted(event_data->args[i].name.c_str(), "",
                                                COPY_DATA_NOTIFICATION, false, true);
                        ImGui::TableSetColumnIndex(3);
                        CopyableTextUnformatted(event_data->args[i].value.c_str(), "",
                                                COPY_DATA_NOTIFICATION, false, true);
                        ImGui::PopID();
                    }
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
            left->m_bg_color       = m_settings.GetColor(Colors::kBgPanel);
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
            right->m_bg_color       = m_settings.GetColor(Colors::kBgPanel);
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
