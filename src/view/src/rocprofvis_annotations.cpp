#include "rocprofvis_annotations.h"

namespace RocProfVis
{
namespace View
{

AnnotationsView::AnnotationsView() {}
AnnotationsView::~AnnotationsView() {}

void
AnnotationsView::AddSticky(double time_ns, float y_offset, const ImVec2& size,
                           const std::string& text)
{
    m_sticky_notes.emplace_back(time_ns, y_offset, size, text);
}

bool
AnnotationsView::Render(ImDrawList* draw_list, const ImVec2& window_position,
                        double v_min_x, double pixels_per_ns)
{
    bool movement_drag   = false;
    bool movement_resize = false;

    for(auto& note : m_sticky_notes)
    {
       
        movement_drag |= note.HandleDrag(window_position, v_min_x, pixels_per_ns);
         movement_resize |= note.HandleResize(window_position, v_min_x, pixels_per_ns);

        note.Render(draw_list, window_position, v_min_x, pixels_per_ns);
    }
    return movement_drag || movement_resize;
}
void
AnnotationsView::ShowStickyNoteMenu(const ImVec2& window_position,
                                    const ImVec2& graph_size, double v_min_x,
                                    double v_max_x)
{
    ImVec2 mouse_pos = ImGui::GetMousePos();
    ImVec2 rel_mouse_pos =
        ImVec2(mouse_pos.x - window_position.x, mouse_pos.y - window_position.y);

    if(ImGui::IsMouseClicked(ImGuiMouseButton_Right) &&
       ImGui::IsMouseHoveringRect(
           window_position,
           ImVec2(window_position.x + graph_size.x, window_position.y + graph_size.y)))
    {
        ImGui::OpenPopup("StickyNoteContextMenu");
    }

    if(ImGui::BeginPopup("StickyNoteContextMenu"))
    {
        if(ImGui::MenuItem("Add Sticky"))
        {
            float x_in_chart = rel_mouse_pos.x;
            m_sticky_time_ns =
                v_min_x + (x_in_chart / graph_size.x) * (v_max_x - v_min_x);
            m_sticky_y_offset   = rel_mouse_pos.y;
            m_sticky_title[0]   = '\0';
            m_sticky_text[0]    = '\0';
            m_show_sticky_popup = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void
AnnotationsView::ShowStickyNotePopup()
{
    if(m_show_sticky_popup)
    {
        ImGui::OpenPopup("StickyNoteInputPopup");
        if(ImGui::BeginPopupModal("StickyNoteInputPopup", nullptr,
                                  ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Sticky Note");
            ImGui::InputText("Title", m_sticky_title, IM_ARRAYSIZE(m_sticky_title));
            ImGui::InputTextMultiline("Text", m_sticky_text, IM_ARRAYSIZE(m_sticky_text),
                                      ImVec2(250, 80));
            ImGui::Spacing();
            if(ImGui::Button("Save"))
            {
                std::string content = std::string(m_sticky_title);
                if(strlen(m_sticky_title) > 0 && strlen(m_sticky_text) > 0)
                    content += "\n";
                content += m_sticky_text;
                AddSticky(m_sticky_time_ns, m_sticky_y_offset, ImVec2(180, 80), content);
                m_show_sticky_popup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(ImGui::Button("Cancel"))
            {
                m_show_sticky_popup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
}

std::vector<StickyNote>&
AnnotationsView::GetStickyNotes()
{
    return m_sticky_notes;
}

}  // namespace View
}  // namespace RocProfVis
