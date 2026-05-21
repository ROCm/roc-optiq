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

constexpr ImGuiTabBarFlags TABLE_FLAGS = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                         ImGuiTableFlags_Resizable |
                                         ImGuiTableFlags_SizingStretchProp;

EventsView::EventsView(DataProvider&                      dp,
                       std::shared_ptr<TimelineSelection> timeline_selection)
: m_data_provider(dp)
, m_settings(SettingsManager::GetInstance())
, m_timeline_selection(timeline_selection)
, m_event_item_id(0)
, m_context_menu_flow_index(-1)
, m_context_menu_flow_column(-1)
{}

EventsView::~EventsView() {}

void
EventsView::Render()
{
    ImGui::BeginChild("events_view", ImVec2(0, 0), ImGuiChildFlags_None);
    if(m_event_items.empty())
    {
        ImGui::TextUnformatted("No data available for the selected events.");
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

                if(ImGui::CollapsingHeader(item.header.c_str(),
                                           ImGuiTreeNodeFlags_DefaultOpen))
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
                    ImGui::Separator();

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
    ImGui::EndChild();
}

bool
EventsView::RenderBasicData(const EventInfo* event_data)
{
    ImVec4 headerColor =
        ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(Colors::kSplitterColor));

    ImFont* large_font = m_settings.GetFontManager().GetFont(FontType::kLarge);

    ImGui::PushFont(large_font);

    const auto& info = event_data->basic_info;

    ImGui::TextUnformatted("ID");

    const uint64_t& db_id = event_data->basic_info.id.bitfield.event_id;
    ImGui::SameLine(160);
    CopyableTextUnformatted(std::to_string(db_id).c_str(), "ID", DATA_COPIED_NOTIFICATION,
                            false, true);

    ImGui::TextUnformatted("Name");
    ImGui::SameLine(160);
    CopyableTextUnformatted(info.name.c_str(), "Name", DATA_COPIED_NOTIFICATION, false,
                            true);

    double trace_start_time = m_data_provider.DataModel().GetTimeline().GetStartTime();
    const auto& time_format = m_settings.GetUserSettings().unit_settings.time_format;

    ImGui::TextUnformatted("Start Time");
    ImGui::SameLine(160);
    std::string label = nanosecond_to_formatted_str(info.start_ts - trace_start_time,
                                                    time_format, true);
    CopyableTextUnformatted(label.c_str(), "Start_time", DATA_COPIED_NOTIFICATION, false,
                            true);

    ImGui::TextUnformatted("Duration");
    ImGui::SameLine(160);
    label = nanosecond_to_formatted_str(info.duration, time_format, true);
    CopyableTextUnformatted(label.c_str(), "Duration", DATA_COPIED_NOTIFICATION, false,
                            true);

#ifdef ROCPROFVIS_DEVELOPER_MODE
    ImGui::TextUnformatted("Level");
    ImGui::SameLine(160);
    CopyableTextUnformatted(std::to_string(info.level).c_str(), "Level",
                            DATA_COPIED_NOTIFICATION,
                            false, true);
#endif

    ImGui::PopFont();
    return true;
}

bool
EventsView::RenderEventExtData(const EventInfo* event_data)
{
    if(event_data->ext_info.empty())
    {
        return false;
    }

    ImVec4 headerColor =
        ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(Colors::kSplitterColor));

    ImGui::PushStyleColor(ImGuiCol_Header, headerColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, headerColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, headerColor);

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

    ImVec4 headerColor =
        ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(Colors::kSplitterColor));

    ImGui::PushStyleColor(ImGuiCol_Header, headerColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, headerColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, headerColor);

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
                        ImGui::Separator();
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

    ImVec4 headerColor =
        ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(Colors::kSplitterColor));

    ImGui::PushStyleColor(ImGuiCol_Header, headerColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, headerColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, headerColor);

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

    ImVec4 headerColor =
        ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(Colors::kSplitterColor));

    ImGui::PushStyleColor(ImGuiCol_Header, headerColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, headerColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, headerColor);

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
                if(this->RenderBasicData(event_data))
                {
                    ImGui::NewLine();
                }
                this->RenderEventExtData(event_data);
            });
            left->m_window_padding = default_style.WindowPadding;
            left->m_child_flags =
                ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding;

            LayoutItem::Ptr right = std::make_shared<LayoutItem>();
            right->m_item = std::make_shared<RocCustomWidget>([this, event_data]() {
                if(this->RenderArgumentData(event_data))
                {
                    ImGui::NewLine();
                }
                if(this->RenderEventFlowInfo(event_data))
                {
                    ImGui::NewLine();
                }
                this->RenderCallStackData(event_data);
            });
            right->m_window_padding = default_style.WindowPadding;
            right->m_child_flags =
                ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding;

            std::unique_ptr<HSplitContainer> container =
                std::make_unique<HSplitContainer>(left, right);
            container->SetMinLeftWidth(10.0f);
            container->SetMinRightWidth(10.0f);
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
