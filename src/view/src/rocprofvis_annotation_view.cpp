// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_annotation_view.h"
#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_settings_manager.h"
#include <imgui.h>
#include <iostream>

namespace RocProfVis
{
namespace View
{
AnnotationView::AnnotationView(std::shared_ptr<AnnotationsManager> annotation_manager)
: m_annotations(annotation_manager)
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
        ImGui::Text("No notes");
    }
    else if(ImGui::BeginTable("StickyNotesTable", 5,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Title");
        ImGui::TableSetupColumn("Text");
        ImGui::TableSetupColumn("Time (ns)");
     
        ImGui::TableSetupColumn("Visibility");
        ImGui::TableHeadersRow();

     for(auto& note : m_annotations->GetStickyNotes())
        {
            ImGui::TableNextRow();

            // First column: Selectable, shows ID and highlights row
            ImGui::TableNextColumn();
            bool        is_selected   = (note.GetID() == m_selected_note_id);
            std::string selectable_id = "##select_" + std::to_string(note.GetID());
            if(ImGui::Selectable(selectable_id.c_str(), is_selected,
                                 ImGuiSelectableFlags_SpanAllColumns))
            {
                m_selected_note_id = note.GetID();
            }
            ImGui::SameLine(0, 0);  // Place ID text next to the selectable highlight
            ImGui::Text("%d", note.GetID());

            // Other columns
            ImGui::TableNextColumn();
            ImGui::Text("%s", note.GetTitle().c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", note.GetText().c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", note.GetTimeNs());
         
            ImGui::TableNextColumn();
            bool        visible     = note.IsVisible();
            std::string checkbox_id = "##visible_" + std::to_string(note.GetID());
            if(ImGui::Checkbox(checkbox_id.c_str(), &visible))
            {
                note.SetVisibility(visible);
            }
        }


        ImGui::EndTable();
    }

    ImGui::EndChild();
}

}  // namespace View
}  // namespace RocProfVis