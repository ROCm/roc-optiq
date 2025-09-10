// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_events_view.h"
#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_custom_widgets.h"
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
                if(ImGui::CollapsingHeader(item.header.c_str()))
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
    RocProfVisCustomWidget::WithPadding(
        16.0f, 16.0f, 16.0f, 16.0f, m_standard_eventcard_height, [this, event_data]() {
            ImVec4 headerColor = ImGui::ColorConvertU32ToFloat4(
                m_settings.GetColor(Colors::kSplitterColor));

            ImGui::PushStyleColor(ImGuiCol_Header, headerColor);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, headerColor);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, headerColor);

            ImGui::TextUnformatted("Event Preview");
            ImGui::Separator();

            const auto& info     = event_data->basic_info;
            std::string thread   = "";
            std::string process  = "";
            std::string category = "";

            for(const auto& ext : event_data->ext_info)
            {
                if(ext.name == "category") category = ext.value;
                if(ext.name == "thread") thread = ext.value;
                if(ext.name == "process") process = ext.value;
            }

            if(ImGui::BeginTable("EventPreviewTable", 2, TABLE_FLAGS))
            {
                ImGui::TableSetupColumn("Field");
                ImGui::TableSetupColumn("Value");
                ImGui::TableHeadersRow();

                ImFont* large_font =
                    m_settings.GetFontManager().GetIconFont(FontType::kLarge);

                // Name
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Name");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(info.m_name.c_str());

                // Category
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Category");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(category.c_str());

                // Start Time
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Start Time");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.3f", info.m_start_ts);

                // Absolute Time
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Absolute Time");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.3f", info.m_start_ts);

                // Duration
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Duration");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.3f", info.m_duration);

                // Thread
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Thread");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(thread.c_str());

                // Process
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Process");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(process.c_str());

                ImGui::EndTable();
            }

            ImGui::Dummy(ImVec2(10, 10));
            // --- Expandable full extended data ---
            if(ImGui::CollapsingHeader("Show More Event Extended Data"))
            {
                ImGui::Separator();
                if(event_data->ext_info.empty())
                {
                    ImGui::TextUnformatted("No data available.");
                }
                else
                {
                    float parent_width = ImGui::GetContentRegionAvail().x;
                    ImGui::BeginChild("ExtDataTableChild",
                                      ImVec2(parent_width, m_standard_eventcard_height),
                                      ImGuiChildFlags_Borders);

                    if(ImGui::BeginTable("ExtDataTable", 2, TABLE_FLAGS))
                    {
                        ImGui::TableSetupColumn("Name");
                        ImGui::TableSetupColumn("Value");
                        ImGui::TableHeadersRow();

                        ImGuiListClipper clipper;
                        clipper.Begin(static_cast<int>(event_data->ext_info.size()));
                        while(clipper.Step())
                        {
                            for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                            {
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                ImGui::TextUnformatted(
                                    event_data->ext_info[i].name.c_str());
                                ImGui::TableSetColumnIndex(1);
                                ImGui::TextUnformatted(
                                    event_data->ext_info[i].value.c_str());
                            }
                        }
                        ImGui::EndTable();
                    }
                    ImGui::EndChild();
                }
            }

            ImGui::PopStyleColor(3);
        });
}

void
EventsView::RenderEventFlowInfo(const event_info_t* event_data)
{
    RocProfVisCustomWidget::WithPadding(
        16.0f, 16.0f, 16.0f, 16.0f, m_standard_eventcard_height, [this, event_data]() {
            ImVec4 headerColor = ImGui::ColorConvertU32ToFloat4(
                m_settings.GetColor(Colors::kSplitterColor));

            ImGui::PushStyleColor(ImGuiCol_Header, headerColor);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, headerColor);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, headerColor);

            if(ImGui::CollapsingHeader("Flow Extended Data"))
            {
                if(event_data->flow_info.empty())
                {
                    ImGui::TextUnformatted("No data available.");
                }
                else
                {
                    // Set a fixed width for the child region (e.g., 700.0f)
                    float parent_width = ImGui::GetContentRegionAvail().x;
                    ImGui::BeginChild("FlowInfoTableChild", ImVec2(parent_width, 400.0f),
                                      ImGuiChildFlags_Borders);

                    // Optionally, set the next item width to match the child
                    ImGui::SetNextItemWidth(parent_width);

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
                                ImGui::TextUnformatted(
                                    event_data->flow_info[i].name.c_str());
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
                    ImGui::EndChild();
                }
            }
            ImGui::PopStyleColor(3);
        });
}

void
EventsView::RenderCallStackData(const event_info_t* event_data)
{
    RocProfVisCustomWidget::WithPadding(
        16.0f, 16.0f, 16.0f, 16.0f, m_standard_eventcard_height, [this, event_data]() {
            ImVec4 headerColor = ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(
                Colors::kSplitterColor));  // Use your desired color enum

            ImGui::PushStyleColor(ImGuiCol_Header, headerColor);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, headerColor);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, headerColor);

            if(ImGui::CollapsingHeader("Event Call Stack Data"))
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
        });
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
            [this, event_data]() { this->RenderEventExtData(event_data); });
        ;
        left.m_window_padding = ImVec2(4, 4);
        left.m_child_flags    = ImGuiChildFlags_AutoResizeY;
        LayoutItem right;
        right.m_item = std::make_shared<RocCustomWidget>([this, event_data]() {
            this->RenderEventFlowInfo(event_data);

            this->RenderCallStackData(event_data);
        });
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
