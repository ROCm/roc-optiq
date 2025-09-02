#include "rocprofvis_annotations.h"
#include "rocprofvis_events.h"
#include "rocprofvis_settings.h"
#include <cstring>

namespace RocProfVis
{
namespace View
{

AnnotationsView::AnnotationsView()
{
    auto sticky_note_handler = [this](std::shared_ptr<RocEvent> e) {
        m_show_sticky_edit_popup = true;
        auto evt                 = std::dynamic_pointer_cast<StickyNoteEvent>(e);
        if(evt)
        {
            m_edit_sticky_id = evt->GetID();
        }
    };
    m_edit_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kStickyNoteEdited), sticky_note_handler);
}
AnnotationsView::~AnnotationsView()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kStickyNoteEdited), m_edit_token);
}

void
AnnotationsView::AddSticky(double time_ns, float y_offset, const ImVec2& size,
                           const std::string& text, const std::string& title)
{
    m_sticky_notes.emplace_back(time_ns, y_offset, size, text, title);
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
                                    double v_max_x, float scroll_y)
{
    ImVec2 mouse_pos = ImGui::GetMousePos();
    // Mouse position relative without adjusting for user scroll. 
    ImVec2 rel_mouse_pos =
        ImVec2(mouse_pos.x - window_position.x, mouse_pos.y - window_position.y);

    // Use the visible area for hover detection adjusted for user scroll 
    ImVec2 win_min = window_position;
    ImVec2 win_max = ImVec2(window_position.x + graph_size.x,
                            window_position.y + graph_size.y + scroll_y);

    if(ImGui::IsMouseClicked(ImGuiMouseButton_Right) &&
       ImGui::IsMouseHoveringRect(win_min, win_max))
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
AnnotationsView::ShowStickyNoteEditPopup()
{
    if(!m_show_sticky_edit_popup) return;

    Settings& settings     = Settings::GetInstance();
    ImU32     popup_bg     = settings.GetColor(Colors::kFillerColor);
    ImU32     border_color = settings.GetColor(Colors::kBorderColor);
    ImU32     text_color   = settings.GetColor(Colors::kRulerTextColor);
    ImU32     button_color = settings.GetColor(Colors::kHighlightChart);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, 18));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 10));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, popup_bg);
    ImGui::PushStyleColor(ImGuiCol_Border, border_color);

    ImGui::OpenPopup("Edit Sticky Note");
    if(ImGui::BeginPopupModal("Edit Sticky Note", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, text_color);

        ImGui::Text("Edit Sticky Note");
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 8));

        ImGui::Text("Title:");
        ImGui::InputText("##StickyTitle", m_sticky_title, IM_ARRAYSIZE(m_sticky_title),
                         ImGuiInputTextFlags_AutoSelectAll);
        ImGui::Dummy(ImVec2(0, 4));

        ImGui::Text("Text:");
        ImGui::InputTextMultiline("##StickyText", m_sticky_text,
                                  IM_ARRAYSIZE(m_sticky_text), ImVec2(290, 100),
                                  ImGuiInputTextFlags_AllowTabInput);

        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(0, 12));

        ImGui::PushStyleColor(ImGuiCol_Button, button_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              settings.GetColor(Colors::kHighlightChart));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              settings.GetColor(Colors::kHighlightChart));

        bool save_clicked = ImGui::Button("Save", ImVec2(80, 0));
        ImGui::SameLine();
        bool cancel_clicked = ImGui::Button("Cancel", ImVec2(80, 0));
        ImGui::SameLine();
        bool delete_clicked = ImGui::Button("Delete", ImVec2(80, 0));
        ImGui::PopStyleColor(3);

        if(save_clicked)
        {
            for(auto& note : m_sticky_notes)
            {
                if(note.GetID() == m_edit_sticky_id)
                {
                    note.SetText(std::string(m_sticky_text));
                    note.SetTitle(std::string(m_sticky_title));
                    m_edit_sticky_id         = -1;
                    m_show_sticky_edit_popup = false;
                    ImGui::CloseCurrentPopup();
                    break;
                }
            }
        }
        if(cancel_clicked)
        {
            m_show_sticky_edit_popup = false;
            m_edit_sticky_id         = -1;
            ImGui::CloseCurrentPopup();
        }
        if(delete_clicked)
        {
            int count = 0;
            for(auto& note : m_sticky_notes)
            {
                if(note.GetID() == m_edit_sticky_id)
                {
                    m_sticky_notes.erase(m_sticky_notes.begin() + count);

                    break;
                }
                count++;
            }
            m_show_sticky_edit_popup = false;
            m_edit_sticky_id         = -1;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

void
AnnotationsView::ShowStickyNotePopup()
{
    if(!m_show_sticky_popup) return;

    int count = 0;
    for(auto& note : m_sticky_notes)
    {
        if(note.GetID() == m_edit_sticky_id)
        {
            const std::string& title = note.GetTitle();
            std::strncpy(m_sticky_title, title.c_str(), sizeof(m_sticky_title) - 1);
            m_sticky_title[sizeof(m_sticky_title) - 1] = '\0';
            // Copy the text string to m_sticky_text safely
            const std::string& text = note.GetText();
            std::strncpy(m_sticky_text, text.c_str(), sizeof(m_sticky_text) - 1);
            m_sticky_text[sizeof(m_sticky_text) - 1] = '\0';
        }
        count++;
    }

    Settings& settings     = Settings::GetInstance();
    ImU32     popup_bg     = settings.GetColor(Colors::kFillerColor);
    ImU32     border_color = settings.GetColor(Colors::kBorderColor);
    ImU32     text_color   = settings.GetColor(Colors::kRulerTextColor);
    ImU32     button_color = settings.GetColor(Colors::kHighlightChart);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, 18));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 10));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, popup_bg);
    ImGui::PushStyleColor(ImGuiCol_Border, border_color);

    ImGui::OpenPopup("Annotation");
    if(ImGui::BeginPopupModal("Annotation", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, text_color);

        ImGui::Text("Add Sticky Note");
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 8));

        ImGui::Text("Title:");
        ImGui::InputText("##StickyTitle", m_sticky_title, IM_ARRAYSIZE(m_sticky_title),
                         ImGuiInputTextFlags_AutoSelectAll);
        ImGui::Dummy(ImVec2(0, 4));

        ImGui::Text("Text:");

        ImGui::InputTextMultiline("##StickyText", m_sticky_text,
                                  IM_ARRAYSIZE(m_sticky_text), ImVec2(290, 100),
                                  ImGuiInputTextFlags_AllowTabInput);

        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(0, 12));

        ImGui::PushStyleColor(ImGuiCol_Button, button_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              settings.GetColor(Colors::kHighlightChart));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              settings.GetColor(Colors::kHighlightChart));

        bool save_clicked = ImGui::Button("Save", ImVec2(100, 0));
        ImGui::SameLine();
        bool cancel_clicked = ImGui::Button("Cancel", ImVec2(100, 0));

        ImGui::PopStyleColor(3);
        if(save_clicked)
        {
            AddSticky(m_sticky_time_ns, m_sticky_y_offset, ImVec2(180, 80),
                      std::string(m_sticky_text), std::string(m_sticky_title));
            m_show_sticky_popup = false;
            ImGui::CloseCurrentPopup();
        }
        if(cancel_clicked)
        {
            m_show_sticky_popup = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

void
AnnotationsView::OpenStickyNotePopup(double time_ns, float y_offset)
{
    m_sticky_time_ns    = time_ns;
    m_sticky_y_offset   = y_offset;
    m_sticky_title[0]   = '\0';
    m_sticky_text[0]    = '\0';
    m_show_sticky_popup = true;
}

std::vector<StickyNote>&
AnnotationsView::GetStickyNotes()
{
    return m_sticky_notes;
}

}  // namespace View
}  // namespace RocProfVis