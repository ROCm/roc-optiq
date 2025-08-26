#include "rocprofvis_annotations.h"
#include "rocprofvis_events.h"
#include "rocprofvis_settings.h"
#include <random>
#include <sstream>

namespace RocProfVis
{
namespace View
{
int
AnnotationsView::GetUniqueId()
{
    // Each annotation gets its own unique ID.
    static std::mt19937_64                    rng{ std::random_device{}() };
    static std::uniform_int_distribution<int> dist;
    return dist(rng);
}

AnnotationsView::AnnotationsView()
{
    auto sticky_note_handler = [this](std::shared_ptr<RocEvent> e) {
        m_show_sticky_edit_popup = true;
        auto evt                 = std::dynamic_pointer_cast<StickyNoteEvent>(e);
        if(evt)
        {
            m_edit_sticky_index = evt->GetID();
            strncpy(m_sticky_text, evt->GetText().c_str(), sizeof(m_sticky_text) - 1);
            m_sticky_text[sizeof(m_sticky_text) - 1] = '\0';
            strncpy(m_sticky_title, evt->GetTitle().c_str(), sizeof(m_sticky_title) - 1);
            m_sticky_title[sizeof(m_sticky_title) - 1] = '\0';
        }
    };
    m_edit_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kStickyNoteEdited), sticky_note_handler);
}
AnnotationsView::~AnnotationsView() {}

void
AnnotationsView::AddSticky(double time_ns, float y_offset, const ImVec2& size,
                           const std::string& text, const std::string& title)
{
    m_sticky_notes.emplace_back(time_ns, y_offset, size, text, title, GetUniqueId());
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
AnnotationsView::ShowStickyNoteEditPopup()
{
    using namespace RocProfVis::View;

    if(!m_show_sticky_edit_popup || m_edit_sticky_index < 0) return;

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
    if(ImGui::BeginPopupModal("EditStickyNote", nullptr,
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

        bool save_clicked = ImGui::Button("Save", ImVec2(70, 0));
        ImGui::SameLine();
        bool cancel_clicked = ImGui::Button("Cancel", ImVec2(70, 0));
        ImGui::SameLine();

        bool delete_clicked = ImGui::Button("Delete", ImVec2(70, 0));
        ImGui::PopStyleColor(3);

        if(save_clicked)
        {
            for(auto& note : m_sticky_notes)
            {
                if(note.GetID() == m_edit_sticky_index)
                {
                    note.SetText(std::string(m_sticky_text));
                    note.SetTitle(std::string(m_sticky_title));
                    m_edit_sticky_index      = -1;
                    m_show_sticky_edit_popup = false;
                    ImGui::CloseCurrentPopup();
                    break;
                }
            }
        }
        if(cancel_clicked)
        {
            m_show_sticky_edit_popup = false;
            m_edit_sticky_index      = -1;
            ImGui::CloseCurrentPopup();
        }
        if(delete_clicked)
        {
            int count = 0;
            for(auto& note : m_sticky_notes)
            {
                if(note.GetID() == m_edit_sticky_index)
                {
                    m_sticky_notes.erase(m_sticky_notes.begin() + count);

                    break;
                }
                count++;
            }
            m_show_sticky_edit_popup = false;
            m_edit_sticky_index      = -1;
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
    using namespace RocProfVis::View;

    if(!m_show_sticky_popup) return;

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