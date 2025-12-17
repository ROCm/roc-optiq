// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_annotations.h"
#include "json.h"
#include "rocprofvis_events.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_stickynote.h"
#include "widgets/rocprofvis_widget.h"
#include <algorithm>
#include <cstring>
#include <vector>
namespace RocProfVis
{
namespace View
{
AnnotationsManagerProjectSettings::AnnotationsManagerProjectSettings(
    const std::string& project_id, AnnotationsManager& annotations_view)
: ProjectSetting(project_id)
, m_annotations_manager(annotations_view)

{}

AnnotationsManagerProjectSettings::~AnnotationsManagerProjectSettings() {}

void
AnnotationsManagerProjectSettings::FromJson()
{
    m_annotations_manager.Clear();
    std::vector<jt::Json>& annotation_vec =
        m_settings_json[JSON_KEY_ANNOTATIONS].getArray();

    for(auto& note_json : annotation_vec)
    {
        double time_ns = note_json[JSON_KEY_ANNOTATION_TIME_NS].getNumber();
        float  y_offset =
            static_cast<float>(note_json[JSON_KEY_ANNOTATION_Y_OFFSET].getNumber());
        float size_x =
            static_cast<float>(note_json[JSON_KEY_ANNOTATION_SIZE_X].getNumber());
        float size_y =
            static_cast<float>(note_json[JSON_KEY_ANNOTATION_SIZE_Y].getNumber());
        std::string text  = note_json[JSON_KEY_ANNOTATION_TEXT].getString();
        std::string title = note_json[JSON_KEY_ANNOTATION_TITLE].getString();
        double      v_min = static_cast<double>(
            note_json[JSON_KEY_TIMELINE_ANNOTATION_V_MIN_X].getNumber());
        double v_max = static_cast<double>(
            note_json[JSON_KEY_TIMELINE_ANNOTATION_V_MAX_X].getNumber());

        ImVec2 size(size_x, size_y);
        m_annotations_manager.AddSticky(time_ns, y_offset, size, text, title, v_min,
                                        v_max);
    }
}

void
AnnotationsManagerProjectSettings::ToJson()
{
    const std::vector<StickyNote>& notes  = m_annotations_manager.GetStickyNotes();
    m_settings_json[JSON_KEY_ANNOTATIONS] = jt::Json();

    for(size_t i = 0; i < notes.size(); ++i)
    {
        jt::Json sticky_json;
        sticky_json[JSON_KEY_ANNOTATION_TIME_NS]          = notes[i].GetTimeNs();
        sticky_json[JSON_KEY_ANNOTATION_Y_OFFSET]         = notes[i].GetYOffset();
        sticky_json[JSON_KEY_ANNOTATION_SIZE_X]           = notes[i].GetSize().x;
        sticky_json[JSON_KEY_ANNOTATION_SIZE_Y]           = notes[i].GetSize().y;
        sticky_json[JSON_KEY_ANNOTATION_TEXT]             = notes[i].GetText();
        sticky_json[JSON_KEY_ANNOTATION_TITLE]            = notes[i].GetTitle();
        sticky_json[JSON_KEY_ANNOTATION_ID]               = notes[i].GetID();
        sticky_json[JSON_KEY_TIMELINE_ANNOTATION_V_MIN_X] = notes[i].GetVMinX();
        sticky_json[JSON_KEY_TIMELINE_ANNOTATION_V_MAX_X] = notes[i].GetVMaxX();

        m_settings_json[JSON_KEY_ANNOTATIONS][i] = sticky_json;
    }
}

bool
AnnotationsManagerProjectSettings::Valid() const
{
    // Check that "annotations" exists and is an array
    if(!m_settings_json.contains(JSON_KEY_ANNOTATIONS) ||
       !m_settings_json[JSON_KEY_ANNOTATIONS].isArray())
        return false;

    auto annotations = m_settings_json[JSON_KEY_ANNOTATIONS];

    for(auto& note_json : annotations.getArray())
    {
        if(!note_json.contains(JSON_KEY_ANNOTATION_TIME_NS) ||
           !note_json[JSON_KEY_ANNOTATION_TIME_NS].isNumber())
            return false;
        if(!note_json.contains(JSON_KEY_ANNOTATION_Y_OFFSET) ||
           !note_json[JSON_KEY_ANNOTATION_Y_OFFSET].isNumber())
            return false;
        if(!note_json.contains(JSON_KEY_ANNOTATION_SIZE_X) ||
           !note_json[JSON_KEY_ANNOTATION_SIZE_X].isNumber())
            return false;
        if(!note_json.contains(JSON_KEY_ANNOTATION_SIZE_Y) ||
           !note_json[JSON_KEY_ANNOTATION_SIZE_Y].isNumber())
            return false;
        if(!note_json.contains(JSON_KEY_ANNOTATION_TEXT) ||
           !note_json[JSON_KEY_ANNOTATION_TEXT].isString())
            return false;
        if(!note_json.contains(JSON_KEY_ANNOTATION_TITLE) ||
           !note_json[JSON_KEY_ANNOTATION_TITLE].isString())
            return false;
        if(!note_json.contains(JSON_KEY_TIMELINE_ANNOTATION_V_MIN_X) ||
           !note_json[JSON_KEY_TIMELINE_ANNOTATION_V_MIN_X].isNumber())
            return false;
        if(!note_json.contains(JSON_KEY_TIMELINE_ANNOTATION_V_MAX_X) ||
           !note_json[JSON_KEY_TIMELINE_ANNOTATION_V_MAX_X].isNumber())
            return false;
    }
    return true;
}

AnnotationsManager::AnnotationsManager(const std::string& project_id)
: m_project_settings(project_id, *this)
, m_project_id(project_id)
, m_show_annotations(true)
, m_visible_center(0.0f, 0.0f)
, m_dragged_sticky_id(-1)
{
    if(m_project_settings.Valid())
    {
        m_project_settings.FromJson();
    }

    auto sticky_note_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<StickyNoteEvent>(e);
        if(evt && evt->GetSourceId() == m_project_id)
        {
            m_show_sticky_edit_popup = true;
            m_edit_sticky_id         = evt->GetNoteId();

            for(int i = 0; i < m_sticky_notes.size(); i++)
            {
                if(m_sticky_notes[i].GetID() == m_edit_sticky_id)
                {
                    const std::string& title = m_sticky_notes[i].GetTitle();
                    size_t len_title = std::min(title.size(), sizeof(m_sticky_title) - 1);
                    std::memcpy(m_sticky_title, title.c_str(), len_title);
                    m_sticky_title[len_title] = '\0';
                    // Copy the text string to m_sticky_text safely
                    const std::string& text = m_sticky_notes[i].GetText();
                    size_t len_text = std::min(text.size(), sizeof(m_sticky_text) - 1);
                    std::memcpy(m_sticky_text, text.c_str(), len_text);
                    m_sticky_text[len_text] = '\0';
                    break;
                }
            }
        }
    };
    m_edit_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kStickyNoteEdited), sticky_note_handler);
}
AnnotationsManager::~AnnotationsManager()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kStickyNoteEdited), m_edit_token);
}

void
AnnotationsManager::Clear()
{
    m_sticky_notes.clear();
}

void
AnnotationsManager::AddSticky(double time_ns, float y_offset, const ImVec2& size,
                              const std::string& text, const std::string& title,
                              double v_min, double v_max)
{
    m_sticky_notes.emplace_back(time_ns, y_offset, size, text, title, m_project_id, v_min,
                                v_max);
}

bool
AnnotationsManager::IsVisibile()
{
    return m_show_annotations;
}

void
AnnotationsManager::SetVisible(bool SetVisible)
{
    m_show_annotations = SetVisible;
}

void
AnnotationsManager::ShowStickyNoteEditPopup()
{
    if(!m_show_sticky_edit_popup) return;

    SettingsManager& settings     = SettingsManager::GetInstance();
    ImU32            text_color   = settings.GetColor(Colors::kRulerTextColor);
    ImU32            button_color = settings.GetColor(Colors::kHighlightChart);

    PopUpStyle popup_style;
    popup_style.PushPopupStyles();
    popup_style.PushTitlebarColors();
    popup_style.CenterPopup();
    
    ImGui::PushStyleColor(ImGuiCol_Text, text_color);

    ImGui::OpenPopup("Edit Annotation");

    ImGui::SetNextWindowSize(ImVec2(390, 420),
                             ImGuiCond_Once);  // Initial size, user can resize
    if(ImGui::BeginPopupModal("Edit Annotation", nullptr, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::Text("Edit Annotation");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Title:");
        ImGui::SetNextItemWidth(-FLT_MIN);  // Full width
        ImGui::InputText("##StickyTitle", m_sticky_title, IM_ARRAYSIZE(m_sticky_title),
                         ImGuiInputTextFlags_AutoSelectAll);

        ImGui::Spacing();

        ImGui::Text("Text:");
        ImVec2 text_box_size = ImVec2(ImGui::GetContentRegionAvail().x,
                                      ImGui::GetContentRegionAvail().y - 60);
        ImGui::InputTextMultiline("##StickyText", m_sticky_text,
                                  IM_ARRAYSIZE(m_sticky_text), text_box_size,
                                  ImGuiInputTextFlags_AllowTabInput);

        ImGui::Spacing();

        float button_width       = 80.0f;
        float spacing            = ImGui::GetStyle().ItemSpacing.x;
        float total_button_width = button_width * 3 + spacing * 2;
        float cursor_x           = ImGui::GetContentRegionAvail().x - total_button_width;
        if(cursor_x > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursor_x);

        ImGui::PushStyleColor(ImGuiCol_Button, button_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              settings.GetColor(Colors::kHighlightChart));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              settings.GetColor(Colors::kHighlightChart));

        bool save_clicked = ImGui::Button("Save", ImVec2(button_width, 0));
        ImGui::SameLine();
        bool cancel_clicked = ImGui::Button("Cancel", ImVec2(button_width, 0));
        ImGui::SameLine();
        bool delete_clicked = ImGui::Button("Delete", ImVec2(button_width, 0));

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
    ImGui::PopStyleColor(1);  // Pop text color
    // PopUpStyle destructor will automatically pop remaining styles
}

void
AnnotationsManager::ShowStickyNotePopup()
{
    if(!m_show_sticky_popup) return;

    int count = 0;
    for(auto& note : m_sticky_notes)
    {
        if(note.GetID() == m_edit_sticky_id)
        {
            const std::string& title = note.GetTitle();
            size_t len_title = std::min(title.size(), sizeof(m_sticky_title) - 1);
            std::memcpy(m_sticky_title, title.c_str(), len_title);
            m_sticky_title[len_title] = '\0';
            // Copy the text string to m_sticky_text safely
            const std::string& text = note.GetText();
            size_t len_text = std::min(text.size(), sizeof(m_sticky_text) - 1);
            std::memcpy(m_sticky_text, text.c_str(), len_text);
            m_sticky_text[len_text] = '\0';
        }
        count++;
    }

    SettingsManager& settings     = SettingsManager::GetInstance();
    ImU32            text_color   = settings.GetColor(Colors::kRulerTextColor);
    ImU32            button_color = settings.GetColor(Colors::kHighlightChart);

    PopUpStyle popup_style;
    popup_style.PushPopupStyles();
    popup_style.PushTitlebarColors();
    popup_style.CenterPopup();
    
    ImGui::PushStyleColor(ImGuiCol_Text, text_color);

    ImGui::OpenPopup("Annotation");

    ImGui::SetNextWindowSize(ImVec2(390, 420),
                             ImGuiCond_Once);  // Initial size, user can resize
    if(ImGui::BeginPopupModal("Annotation", nullptr, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::Text("Add Annotation");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Title:");
        ImGui::SetNextItemWidth(-FLT_MIN);  // Make the input take the full width
        ImGui::InputText("##StickyTitle", m_sticky_title, IM_ARRAYSIZE(m_sticky_title),
                         ImGuiInputTextFlags_AutoSelectAll);

        ImGui::Spacing();

        ImGui::Text("Text:");
        ImVec2 text_box_size = ImVec2(ImGui::GetContentRegionAvail().x,
                                      ImGui::GetContentRegionAvail().y - 60);
        ImGui::InputTextMultiline("##StickyText", m_sticky_text,
                                  IM_ARRAYSIZE(m_sticky_text), text_box_size,
                                  ImGuiInputTextFlags_AllowTabInput);

        ImGui::Spacing();

        // Button row, right-aligned
        float button_width       = 100.0f;
        float spacing            = ImGui::GetStyle().ItemSpacing.x;
        float total_button_width = button_width * 2 + spacing;
        float cursor_x           = ImGui::GetContentRegionAvail().x - total_button_width;
        if(cursor_x > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursor_x);

        ImGui::PushStyleColor(ImGuiCol_Button, button_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              settings.GetColor(Colors::kHighlightChart));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              settings.GetColor(Colors::kHighlightChart));

        bool save_clicked = ImGui::Button("Save", ImVec2(button_width, 0));
        ImGui::SameLine();
        bool cancel_clicked = ImGui::Button("Cancel", ImVec2(button_width, 0));

        ImGui::PopStyleColor(3);

        if(save_clicked)
        {
            AddSticky(m_sticky_time_ns, m_sticky_y_offset, ImVec2(180, 80),
                      std::string(m_sticky_text), std::string(m_sticky_title), m_v_min_x,
                      m_v_max_x);
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
    ImGui::PopStyleColor(1);  // Pop text color
    // PopUpStyle destructor will automatically pop remaining styles
}

void
AnnotationsManager::OpenStickyNotePopup(double time_ns, float y_offset, double v_min,
                                        double v_max, ImVec2 graph_size)
{
    if(time_ns == INVALID_TIME_NS)
    {
        double center_time_ns  = v_min + (v_max - v_min) * 0.5;
        float  center_y_offset = y_offset + graph_size.y * 0.5f;
        m_sticky_time_ns       = center_time_ns;
        m_sticky_y_offset      = center_y_offset;
    }
    else
    {
        m_sticky_time_ns  = time_ns;
        m_sticky_y_offset = y_offset;
    }

    m_sticky_title[0]   = '\0';
    m_sticky_text[0]    = '\0';
    m_show_sticky_popup = true;
    m_v_min_x           = v_min;
    m_v_max_x           = v_max;
}

std::vector<StickyNote>&
AnnotationsManager::GetStickyNotes()
{
    return m_sticky_notes;
}

}  // namespace View
}  // namespace RocProfVis