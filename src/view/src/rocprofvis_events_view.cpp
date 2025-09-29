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
EventsView::RenderBasicData(const event_info_t* event_data)
{
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
}

void
EventsView::RenderEventExtData(const event_info_t* event_data)
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
                for(size_t i = 0; i < event_data->ext_info.size(); ++i)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(event_data->ext_info[i].name.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(event_data->ext_info[i].value.c_str());
                }
                ImGui::EndTable();
            }
        }
    }

    ImGui::PopStyleColor(3);
}

void
EventsView::RenderEventFlowInfo(const event_info_t* event_data)
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
                            std::to_string(event_data->flow_info[i].level).c_str());
                        ImGui::TableSetColumnIndex(5);
                        ImGui::TextUnformatted(
                            std::to_string(event_data->flow_info[i].direction).c_str());
                    }
                }
                ImGui::EndTable();
            }
        }
    }
    ImGui::PopStyleColor(3);
}

void
EventsView::RenderCallStackData(const event_info_t* event_data)
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
        const event_info_t* event_data = m_data_provider.GetEventInfo(event_id);
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
