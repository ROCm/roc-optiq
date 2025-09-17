// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_events_view.h"
#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"

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
, m_standard_eventcard_height(ImGui::GetWindowHeight())
, m_table_expanded(true)

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
            ImGui::CalcTextSize("X").x + 2 * ImGui::GetStyle().FramePadding.x;
        for(EventItem& item : m_event_items)
        {
            if(item.info && item.contents)
            {
                bool deselect_event = false;
                ImGui::PushID(item.info->basic_info.m_id);
                ImGui::SetNextItemAllowOverlap();

                if(ImGui::CollapsingHeader(item.header.c_str(),
                                           ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::SameLine();
                    ImGui::Dummy(
                        ImVec2(ImGui::GetContentRegionAvail().x - x_button_width, 0));
                    ImGui::SameLine();
                    deselect_event = XButton();

                    ImGui::BeginChild("EventDetails", ImVec2(0, item.height), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_ResizeY);
                    item.contents->Render();
                    ImGui::EndChild();
                    
                    // Use the optimal height of the contents as the new height for the next frame
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
EventsView::RenderBasicData(const event_info_t* event_data) 
{
    float padding = m_settings.GetDefaultStyle().CellPadding.y;
    // WithPadding(padding, padding, padding, padding, [this, event_data]() {
        ImVec4 headerColor =
            ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(Colors::kSplitterColor));

        ImFont* large_font = m_settings.GetFontManager().GetFont(FontType::kLarge);

        ImGui::PushFont(large_font);

        const auto& info = event_data->basic_info;

        ImGui::TextUnformatted("ID");
        ImGui::SameLine(160);
        ImGui::Text("%llu", static_cast<unsigned long long>(info.m_id));

        ImGui::TextUnformatted("Name");
        ImGui::SameLine(160);
        ImGui::TextUnformatted(info.m_name.c_str());

        ImGui::TextUnformatted("Start Time");
        ImGui::SameLine(160);
        ImGui::Text("%.3f", info.m_start_ts);

        ImGui::TextUnformatted("Duration");
        ImGui::SameLine(160);
        ImGui::Text("%.3f", info.m_duration);

        ImGui::TextUnformatted("Level");
        ImGui::SameLine(160);
        ImGui::Text("%u", info.m_level);

        ImGui::PopFont();
    // });
}

void
EventsView::RenderEventExtData(const event_info_t* event_data)
{
    float padding = m_settings.GetDefaultStyle().CellPadding.y;
    // WithPadding(padding, padding, padding, padding, [this, event_data]() {
        ImVec4 headerColor =
            ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(Colors::kSplitterColor));

        ImGui::PushStyleColor(ImGuiCol_Header, headerColor);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, headerColor);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, headerColor);

        // --- Expandable full extended data ---
        if(ImGui::CollapsingHeader("Show More Event Extended Data",
                                   ImGuiTreeNodeFlags_DefaultOpen))
        {
            // ImGui::Separator();
            if(event_data->ext_info.empty())
            {
                ImGui::TextUnformatted("No data available.");
            }
            else
            {
                if(ImGui::BeginTable("ExtDataTable", 2, TABLE_FLAGS))
                {
                    ImGuiListClipper clipper;
                    clipper.Begin(static_cast<int>(event_data->ext_info.size()));
                    while(clipper.Step())
                    {
                        for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                        {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::TextUnformatted(event_data->ext_info[i].name.c_str());
                            ImGui::TableSetColumnIndex(1);
                            ImGui::TextUnformatted(event_data->ext_info[i].value.c_str());
                        }
                    }
                    ImGui::EndTable();
                }
            }
        }

        ImGui::PopStyleColor(3);
    // });
}

void
EventsView::RenderEventFlowInfo(const event_info_t* event_data)
{
    float padding = m_settings.GetDefaultStyle().CellPadding.y;
    // WithPadding(padding, padding, padding, padding, [this, event_data]() {
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
                if(ImGui::BeginTable("FlowInfoTable", 5, TABLE_FLAGS))
                {
                    m_table_expanded = true;
                    ImGui::TableSetupColumn("ID");
                    ImGui::TableSetupColumn("Name");
                    ImGui::TableSetupColumn("Timestamp");
                    ImGui::TableSetupColumn("Track ID");
                    ImGui::TableSetupColumn("Direction");
                    ImGui::TableHeadersRow();

                    ImGuiListClipper clipper;
                    clipper.Begin(event_data->flow_info.size());
                    while(clipper.Step())
                    {
                        for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                        {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::TextUnformatted(
                                std::to_string(event_data->flow_info[i].id).c_str());
                            ImGui::TableSetColumnIndex(1);
                            ImGui::TextUnformatted(event_data->flow_info[i].name.c_str());
                            ImGui::TableSetColumnIndex(2);
                            ImGui::TextUnformatted(
                                std::to_string(event_data->flow_info[i].timestamp)
                                    .c_str());
                            ImGui::TableSetColumnIndex(3);
                            ImGui::TextUnformatted(
                                std::to_string(event_data->flow_info[i].track_id)
                                    .c_str());
                            ImGui::TableSetColumnIndex(4);
                            ImGui::TextUnformatted(
                                std::to_string(event_data->flow_info[i].direction)
                                    .c_str());
                        }
                    }
                    ImGui::EndTable();
                }
                else
                {
                    m_table_expanded = false;
                }
            }
        }
        ImGui::PopStyleColor(3);
    // });
}

void
EventsView::RenderCallStackData(const event_info_t* event_data)
{
    float padding = m_settings.GetDefaultStyle().CellPadding.y;
//    WithPadding(padding, padding, padding, padding, [this, event_data]() {
        ImVec4 headerColor = ImGui::ColorConvertU32ToFloat4(
            m_settings.GetColor(Colors::kSplitterColor));  // Use your desired color enum

        ImGui::PushStyleColor(ImGuiCol_Header, headerColor);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, headerColor);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, headerColor);

        if(ImGui::CollapsingHeader("Event Call Stack Data",
                                   ImGuiTreeNodeFlags_DefaultOpen))
        {
            if(event_data->call_stack_info.empty())
            {
                ImGui::TextUnformatted("No data available.");
            }
            else
            {
                if(ImGui::BeginTable("CallStackTable", 3, TABLE_FLAGS))
                {
                    ImGui::TableSetupColumn("Line");
                    ImGui::TableSetupColumn("Function");
                    ImGui::TableSetupColumn("Arguments");
                    ImGui::TableHeadersRow();

                    ImGuiListClipper clipper;
                    clipper.Begin(event_data->call_stack_info.size());
                    while(clipper.Step())
                    {
                        for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                        {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::TextUnformatted(
                                event_data->call_stack_info[i].line.c_str());
                            ImGui::TableSetColumnIndex(1);
                            ImGui::TextUnformatted(
                                event_data->call_stack_info[i].function.c_str());
                            ImGui::TableSetColumnIndex(2);
                            ImGui::TextUnformatted(
                                event_data->call_stack_info[i].arguments.c_str());
                        }
                    }
                    ImGui::EndTable();
                }
            }
        }
        ImGui::PopStyleColor(3);
    // });
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
    ImGui::PushFont(m_settings.GetFontManager().GetIconFont(FontType::kDefault));
    clicked = ImGui::SmallButton(ICON_X_CIRCLED);
    ImGui::PopFont();
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
EventsView::HandleEventSelectionChanged()
{
    std::vector<uint64_t> selected_event_ids;
    m_timeline_selection->GetSelectedEvents(selected_event_ids);
    m_event_items.resize(selected_event_ids.size());
    for(int i = 0; i < selected_event_ids.size(); i++)
    {
        const event_info_t* event_data =
            m_data_provider.GetEventInfo(selected_event_ids[i]);

        LayoutItem left;
        left.m_item = std::make_shared<RocCustomWidget>(
            [this, event_data]() {
                this->RenderBasicData(event_data);
                ImGui::NewLine();
                this->RenderEventExtData(event_data); 
            });
        left.m_window_padding = ImVec2(4, 4);
        left.m_child_flags = ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding;

        LayoutItem right;
        right.m_item = std::make_shared<RocCustomWidget>(
            [this, event_data]() { 
                this->RenderEventFlowInfo(event_data);
                ImGui::NewLine();
                this->RenderCallStackData(event_data);
            });
        right.m_window_padding = ImVec2(4, 4);
        right.m_child_flags = ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding;

        std::unique_ptr<HSplitContainer> container =
            std::make_unique<HSplitContainer>(left, right);
        container->SetMinLeftWidth(10.0f);
        container->SetMinRightWidth(10.0f);
        container->SetSplit(0.5f);

        m_event_items[i].header =
            "Event ID: " + std::to_string(event_data->basic_info.m_id);
        m_event_items[i].contents = std::move(container);
        m_event_items[i].info     = event_data;
    }
}

}  // namespace View
}  // namespace RocProfVis
