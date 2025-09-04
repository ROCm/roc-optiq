// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_events_view.h"
#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_font_manager.h"
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
{}

EventsView::~EventsView() {}

void
EventsView::Render()
{
    ImGui::BeginChild("events_view", ImVec2(0, 0), ImGuiChildFlags_Borders);
    if(m_event_items.empty())
    {
        ImGui::TextUnformatted("No data available for the selected events.");
    }
    else
    {
        float x_button_width =
            ImGui::CalcTextSize("X").x + 2 * ImGui::GetStyle().FramePadding.x;
        for(const EventItem& item : m_event_items)
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
                    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x - x_button_width,
                                        ImGui::GetFrameHeightWithSpacing()));
                    ImGui::SameLine();
                    deselect_event = XButton();
                    ImGui::BeginChild("events_view_item", ImVec2(0, 0),
                                      ImGuiChildFlags_AutoResizeY |
                                          ImGuiChildFlags_Borders);
                    item.contents->Render();
                    ImGui::EndChild();
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
EventsView::RenderEventExtData(const event_info_t* event_data)
{
    ImGui::TextUnformatted("Event Extended Data");
    ImGui::Separator();
    if(event_data->ext_info.empty())
    {
        ImGui::TextUnformatted("No data available.");
        ImGui::NewLine();
    }
    else
    {
        if(ImGui::BeginTable("ExtDataTable", 3, TABLE_FLAGS))
        {
            ImGui::TableSetupColumn("Category");
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            clipper.Begin(event_data->ext_info.size());
            while(clipper.Step())
            {
                for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(event_data->ext_info[i].category.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(event_data->ext_info[i].name.c_str());
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(event_data->ext_info[i].value.c_str());
                }
            }
            ImGui::EndTable();
        }
    }
}

void
EventsView::RenderEventFlowInfo(const event_info_t* event_data)
{
    ImGui::NewLine();
    ImGui::TextUnformatted("Event Flow Info");
    ImGui::Separator();
    if(event_data->flow_info.empty())
    {
        ImGui::TextUnformatted("No data available.");
        ImGui::NewLine();
    }
    else
    {
        if(ImGui::BeginTable("FlowInfoTable", 5, TABLE_FLAGS))
        {
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
                        std::to_string(event_data->flow_info[i].timestamp).c_str());
                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextUnformatted(
                        std::to_string(event_data->flow_info[i].track_id).c_str());
                    ImGui::TableSetColumnIndex(4);
                    ImGui::TextUnformatted(
                        std::to_string(event_data->flow_info[i].direction).c_str());
                }
            }
            ImGui::EndTable();
        }
    }
}

void
EventsView::RenderCallStackData(const event_info_t* event_data)
{
    ImGui::TextUnformatted("Event Call Stack Data");
    ImGui::Separator();
    if(event_data->call_stack_info.empty())
    {
        ImGui::TextUnformatted("No data available.");
        ImGui::NewLine();
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
                    ImGui::TextUnformatted(event_data->call_stack_info[i].line.c_str());
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

bool
EventsView::XButton()
{
    bool clicked = false;
    ImGui::PushStyleColor(ImGuiCol_Button,
                          m_settings.GetColor(static_cast<int>(Colors::kTransparent)));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          m_settings.GetColor(static_cast<int>(Colors::kTransparent)));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          m_settings.GetColor(static_cast<int>(Colors::kTransparent)));
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
        left.m_item = std::make_shared<RocCustomWidget>([this, event_data]() {
            this->RenderEventExtData(event_data);
            this->RenderEventFlowInfo(event_data);
        });
        ;
        left.m_window_padding = ImVec2(4, 4);
        left.m_child_flags    = ImGuiChildFlags_AutoResizeY;
        LayoutItem right;
        right.m_item = std::make_shared<RocCustomWidget>(
            [this, event_data]() { this->RenderCallStackData(event_data); });
        ;
        right.m_window_padding = ImVec2(4, 4);
        right.m_child_flags    = ImGuiChildFlags_AutoResizeY;

        std::unique_ptr<HSplitContainer> container =
            std::make_unique<HSplitContainer>(left, right);
        container->SetMinLeftWidth(10.0f);
        container->SetMinRightWidth(10.0f);
        container->SetSplit(0.6f);

        m_event_items[i].header =
            "Event ID: " + std::to_string(event_data->basic_info.m_id);
        m_event_items[i].contents = std::move(container);
        m_event_items[i].info     = event_data;
    }
}

}  // namespace View
}  // namespace RocProfVis
