// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_annotation_view.h"
#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_events.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "widgets/rocprofvis_gui_helpers.h"
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
    SettingsManager& settings = SettingsManager::GetInstance();
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,
                        settings.GetDefaultStyle().ChildRounding);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgPanel));
    ImGui::PushStyleColor(ImGuiCol_Border, settings.GetColor(Colors::kBorderColor));
    ImGui::BeginChild("Annotations", ImVec2(0, 0), ImGuiChildFlags_Borders);

    if(m_annotations->GetStickyNotes().empty())
    {
        ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().ItemSpacing.y * 0.5f));
        ImGui::TextDisabled("No annotations.");
    }
    else if(ImGui::BeginTable("StickyNotesTable", 4,
                              ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
                                  ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Title");
        ImGui::TableSetupColumn("Text");
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed,
                                ImGui::GetFontSize() * 12.0f);
        ImGui::TableSetupColumn("Visible", ImGuiTableColumnFlags_WidthFixed,
                                ImGui::GetFontSize() * 5.5f);
        ImGui::TableHeadersRow();

        const auto& time_format =
            SettingsManager::GetInstance().GetUserSettings().unit_settings.time_format;
        std::string time_label;

        for(auto& note : m_annotations->GetStickyNotes())
        {
            ImGui::PushID(note.GetID());
            const float row_height = ImGui::GetFrameHeight();
            ImGui::TableNextRow(ImGuiTableRowFlags_None, row_height);

            // Title column with selection logic.
            ImGui::TableNextColumn();
            const bool is_selected = (note.GetID() == m_selected_note_id);

            if(is_selected)
            {
                ImGui::PushStyleColor(ImGuiCol_Header,
                                      settings.GetColor(Colors::kSelection));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                                      settings.GetColor(Colors::kSelection));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                                      settings.GetColor(Colors::kSelection));
            }

            const std::string display_title =
                note.GetTitle().empty() ? "Untitled" : note.GetTitle();
            std::string note_preview = note.GetText();
            for(char& c : note_preview)
            {
                if(c == '\n' || c == '\r' || c == '\t') c = ' ';
            }
            const std::string selectable_label =
                display_title + "##sticky_note_" + std::to_string(note.GetID());

            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
            if(ImGui::Selectable(selectable_label.c_str(), is_selected,
                                 ImGuiSelectableFlags_SpanAllColumns |
                                     ImGuiSelectableFlags_AllowOverlap,
                                 ImVec2(0.0f, row_height)))
            {
                m_selected_note_id = note.GetID();
                auto event         = std::make_shared<NavigationEvent>(
                    note.GetVMinX(), note.GetVMaxX(), note.GetYOffset(), true);
                EventManager::GetInstance()->AddEvent(event);
            }
            ImGui::PopStyleVar();

            if(is_selected)
            {
                ImGui::PopStyleColor(3);
            }

            // Text column
            ImGui::TableNextColumn();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() +
                                 ImGui::GetStyle().FramePadding.y);
            ImGui::PushID("note_preview");
            ElidedText(note_preview.c_str(), ImGui::GetContentRegionAvail().x);
            ImGui::PopID();

            // Time column
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            time_label = nanosecond_to_formatted_str(note.GetTimeNs(), time_format,
                                        true);
            ImGui::TextUnformatted(time_label.c_str());

            // Visibility column
            ImGui::TableNextColumn();
            bool        visible     = note.IsVisible();
            std::string checkbox_id = "##visible_" + std::to_string(note.GetID());
            ImGui::Checkbox(checkbox_id.c_str(), &visible);
            if(visible != note.IsVisible()) note.SetVisibility(visible);

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
}

}  // namespace View
}  // namespace RocProfVis