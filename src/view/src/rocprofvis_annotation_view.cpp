// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_annotation_view.h"
#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_events.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include <imgui.h>
#include <iostream>
namespace RocProfVis
{
namespace View
{
AnnotationView::AnnotationView(DataProvider& data_provider, std::shared_ptr<AnnotationsManager> annotation_manager)
: m_annotations(annotation_manager)
, m_data_provider(data_provider)
, m_selected_note_id(-1)
{}

AnnotationView::~AnnotationView() {}

void
AnnotationView::Render()
{
    ImFont* icon_font =
        SettingsManager::GetInstance().GetFontManager().GetIconFont(FontType::kDefault);
    ImGuiStyle& style = ImGui::GetStyle();

    ImGui::BeginChild("Annotations");

    if(m_annotations->GetStickyNotes().empty())
    {
        ImGui::Text("No annotations.");
    }
    else if(ImGui::BeginTable("StickyNotesTable", 4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Title");
        ImGui::TableSetupColumn("Text");
        ImGui::TableSetupColumn("Time (ns)");

        ImGui::TableSetupColumn("Visibility");
        ImGui::TableHeadersRow();

        double      trace_start_time = m_data_provider.GetStartTime();
        const auto& time_format =
            SettingsManager::GetInstance().GetUserSettings().unit_settings.time_format;
        std::string time_label;

        for(auto& note : m_annotations->GetStickyNotes())
        {
            ImGui::TableNextRow();

            // Calculate max row height needed for highlight to cover entire row
            float id_height    = ImGui::GetTextLineHeightWithSpacing();
            float title_height = ImGui::CalcTextSize(note.GetTitle().c_str()).y +
                                 ImGui::GetStyle().ItemSpacing.y;
            float text_height = ImGui::CalcTextSize(note.GetText().c_str()).y +
                                ImGui::GetStyle().ItemSpacing.y;
            float time_height     = ImGui::GetTextLineHeightWithSpacing();
            float checkbox_height = ImGui::GetFrameHeight();

            float max_row_height = id_height;
            max_row_height       = std::max(max_row_height, title_height);
            max_row_height       = std::max(max_row_height, text_height);
            max_row_height       = std::max(max_row_height, time_height);
            max_row_height       = std::max(max_row_height, checkbox_height);

            // Title column with selection logic
            ImGui::TableNextColumn();
            bool is_selected = (note.GetID() == m_selected_note_id);
      
            if(is_selected)
            {
                ImGui::PushStyleColor(ImGuiCol_Header,
                                      ImGui::GetStyleColorVec4(ImGuiCol_TableRowBg));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                                      ImGui::GetStyleColorVec4(ImGuiCol_TableRowBg));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                                      ImGui::GetStyleColorVec4(ImGuiCol_TableRowBg));
            }

            if(ImGui::Selectable(note.GetTitle().c_str(), is_selected,
                                 ImGuiSelectableFlags_SpanAllColumns |
                                     ImGuiSelectableFlags_AllowItemOverlap,
                                 ImVec2(0, max_row_height)))
            {
                m_selected_note_id = note.GetID();
                auto event         = std::make_shared<NavigationEvent>(
                    note.GetVMinX(), note.GetVMaxX(), note.GetYOffset(), true);
                EventManager::GetInstance()->AddEvent(event);
            }

            if(is_selected)
            {
                ImGui::PopStyleColor(3);
            }

            // Text column
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", note.GetText().c_str());

            // Time column
            ImGui::TableNextColumn();
            time_label = nanosecond_to_formatted_str(note.GetTimeNs() - trace_start_time, time_format,
                                        true);
            ImGui::TextUnformatted(time_label.c_str());

            // Visibility column
            ImGui::TableNextColumn();
            bool        visible     = note.IsVisible();
            std::string checkbox_id = "##visible_" + std::to_string(note.GetID());
            ImGui::Checkbox(checkbox_id.c_str(), &visible);
            if(visible != note.IsVisible()) note.SetVisibility(visible);
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
}

}  // namespace View
}  // namespace RocProfVis