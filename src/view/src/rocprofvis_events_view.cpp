// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_events_view.h"
#include "icons/rocprovfis_icon_defines.h"
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
                                                             item.info->basic_info.m_id);
                }
                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();
}

void
EventsView::RenderBasicData(const EventInfo* event_data)
{
    ImVec4 headerColor =
        ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(Colors::kSplitterColor));

    ImFont* large_font = m_settings.GetFontManager().GetFont(FontType::kLarge);

    ImGui::PushFont(large_font);

    const auto& info = event_data->basic_info;

    ImGui::TextUnformatted("ID");
    ImGui::SameLine(160);
    CopyableTextUnformatted(std::to_string(info.m_id).c_str(), "ID",
                            DATA_COPIED_NOTIFICATION, false, true);

    ImGui::TextUnformatted("Name");
    ImGui::SameLine(160);
    CopyableTextUnformatted(info.m_name.c_str(), "Name", DATA_COPIED_NOTIFICATION, false,
                            true);

    double      trace_start_time = m_data_provider.DataModel().GetTimeline().GetStartTime();
    const auto& time_format      = m_settings.GetUserSettings().unit_settings.time_format;

    ImGui::TextUnformatted("Start Time");
    ImGui::SameLine(160);
    std::string label = nanosecond_to_formatted_str(info.m_start_ts - trace_start_time,
                                                    time_format, true);
    CopyableTextUnformatted(label.c_str(), "Start_time", DATA_COPIED_NOTIFICATION, false,
                            true);

    ImGui::TextUnformatted("Duration");
    ImGui::SameLine(160);
    label = nanosecond_to_formatted_str(info.m_duration, time_format, true);
    CopyableTextUnformatted(label.c_str(), "Duration", DATA_COPIED_NOTIFICATION, false,
                            true);

#ifdef ROCPROFVIS_DEVELOPER_MODE
    ImGui::TextUnformatted("Level");
    ImGui::SameLine(160);
    CopyableTextUnformatted(std::to_string(info.m_level).c_str(), "Level",
                            DATA_COPIED_NOTIFICATION,
                            false, true);
#endif

    ImGui::PopFont();
}

void
EventsView::RenderEventExtData(const EventInfo* event_data)
{
    ImVec4 headerColor =
        ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(Colors::kSplitterColor));

    ImGui::PushStyleColor(ImGuiCol_Header, headerColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, headerColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, headerColor);

    // --- Expandable full extended data ---
    if(ImGui::CollapsingHeader("Show More Event Extended Data",
                               ImGuiTreeNodeFlags_DefaultOpen))
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
                                            std::to_string(i),
                                            COPY_DATA_NOTIFICATION, false,
                                            true);
                    ImGui::TableSetColumnIndex(1);

                    switch(event_data->ext_info[i].category_enum)
                    {
                        case kRocProfVisEventEssentialDataStart:
                        case kRocProfVisEventEssentialDataEnd:
                            offset_ns = m_data_provider.DataModel().GetTimeline().GetStartTime();
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
}

void
EventsView::RenderEventFlowInfo(const EventInfo* event_data)
{
    ImVec4 headerColor =
        ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(Colors::kSplitterColor));

    ImGui::PushStyleColor(ImGuiCol_Header, headerColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, headerColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, headerColor);

    if(ImGui::CollapsingHeader("Flow Extended Data", ImGuiTreeNodeFlags_DefaultOpen))
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

                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(event_data->flow_info.size()));
                while(clipper.Step())
                {
                    for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        CopyableTextUnformatted(
                            std::to_string(event_data->flow_info[i].id).c_str(),
                            "##id_" + std::to_string(i), COPY_DATA_NOTIFICATION, false,
                            true);
                        ImGui::TableSetColumnIndex(1);
                        CopyableTextUnformatted(
                            event_data->flow_info[i].name.c_str(),
                                                "##name_" + std::to_string(i),
                                                COPY_DATA_NOTIFICATION, false, true);
                        ImGui::TableSetColumnIndex(2);
                        std::string timestamp_label = nanosecond_to_formatted_str(
                            event_data->flow_info[i].start_timestamp - trace_start_time,
                            time_format, true);
                        CopyableTextUnformatted(timestamp_label.c_str(),
                                                "##start_timestamp_" + std::to_string(i),
                                                COPY_DATA_NOTIFICATION, false,
                                                true);
                        ImGui::TableSetColumnIndex(3);
                        CopyableTextUnformatted(
                            std::to_string(event_data->flow_info[i].track_id).c_str(),
                            "##track_id_" + std::to_string(i),
                            COPY_DATA_NOTIFICATION, false, true);
                        ImGui::TableSetColumnIndex(4);
                        CopyableTextUnformatted(
                            std::to_string(event_data->flow_info[i].level).c_str(),
                            "##level_" + std::to_string(i), COPY_DATA_NOTIFICATION, false,
                            true);
                        ImGui::TableSetColumnIndex(5);
                        CopyableTextUnformatted(
                            std::to_string(event_data->flow_info[i].direction).c_str(),
                            "##direction_" + std::to_string(i),
                            COPY_DATA_NOTIFICATION, false, true);
                    }
                }
                ImGui::EndTable();
            }
        }
    }
    ImGui::PopStyleColor(3);
}

void
EventsView::RenderCallStackData(const EventInfo* event_data)
{
    ImVec4 headerColor =
        ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(Colors::kSplitterColor));

    ImGui::PushStyleColor(ImGuiCol_Header, headerColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, headerColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, headerColor);

    if(ImGui::CollapsingHeader("Event Call Stack Data", ImGuiTreeNodeFlags_DefaultOpen))
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
                        CopyableTextUnformatted(
                            event_data->call_stack_info[i].pc.c_str(), "",
                            COPY_DATA_NOTIFICATION, false, true);
                        ImGui::PopID();
                    }
                }
                ImGui::EndTable();
            }
        }
    }
    ImGui::PopStyleColor(3);
}

bool
EventsView::XButton()
{
    bool clicked = false;
    ImGui::PushStyleColor(ImGuiCol_Button, m_settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          m_settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          m_settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleVarX(ImGuiStyleVar_FramePadding, 0);
    ImGui::PushFont(m_settings.GetFontManager().GetIconFont(FontType::kDefault));
    clicked = ImGui::SmallButton(ICON_X_CIRCLED);
    ImGui::PopFont();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        m_settings.GetDefaultStyle().WindowPadding);
    if(ImGui::BeginItemTooltip())
    {
        ImGui::TextUnformatted("Unselect Event");
        ImGui::EndTooltip();
    }
    ImGui::PopStyleVar();
    return clicked;
}

void
EventsView::HandleEventSelectionChanged(const uint64_t event_id, const bool selected)
{
    if(selected)
    {
        const EventInfo* event_data = m_data_provider.DataModel().GetEvents().GetEvent(event_id);
        if(event_data)
        {
            auto       default_style = m_settings.GetDefaultStyle();
            LayoutItem::Ptr left          = std::make_shared<LayoutItem>();
            left->m_item = std::make_shared<RocCustomWidget>([this, event_data]() {
                this->RenderBasicData(event_data);
                ImGui::NewLine();
                this->RenderEventExtData(event_data);
            });
            left->m_window_padding = default_style.WindowPadding;
            left->m_child_flags =
                ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding;

            LayoutItem::Ptr right = std::make_shared<LayoutItem>();
            right->m_item = std::make_shared<RocCustomWidget>([this, event_data]() {
                this->RenderEventFlowInfo(event_data);
                ImGui::NewLine();
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

            m_event_items.emplace_front(
                EventItem{ m_event_item_id++, event_data->basic_info.m_id,
                           "Event ID: " + std::to_string(event_data->basic_info.m_id),
                           std::move(container), event_data, 0.0f });
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
