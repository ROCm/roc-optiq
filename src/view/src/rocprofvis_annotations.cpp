// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_annotations.h"
#include "json.h"
#include "rocprofvis_stickynote.h"
#include <algorithm>
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
        
        bool is_minimized = true;
        if (note_json.contains(JSON_KEY_ANNOTATION_IS_MINIMIZED) && note_json[JSON_KEY_ANNOTATION_IS_MINIMIZED].isBool()) {
            is_minimized = note_json[JSON_KEY_ANNOTATION_IS_MINIMIZED].getBool();
        }

        // Legacy projects have no track binding; load as unbound and re-anchor
        // on first render.
        uint64_t track_id = INVALID_TRACK_ID;
        if(note_json.contains(JSON_KEY_ANNOTATION_TRACK_ID) &&
           note_json[JSON_KEY_ANNOTATION_TRACK_ID].isNumber())
        {
            track_id = static_cast<uint64_t>(
                note_json[JSON_KEY_ANNOTATION_TRACK_ID].getNumber());
        }

        ImVec2 size(size_x, size_y);
        m_annotations_manager.AddSticky(time_ns, y_offset, size, text, title, v_min,
                                        v_max, track_id, is_minimized);
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
        sticky_json[JSON_KEY_ANNOTATION_TRACK_ID] =
            static_cast<double>(notes[i].GetTrackId());
        sticky_json[JSON_KEY_TIMELINE_ANNOTATION_V_MIN_X] = notes[i].GetVMinX();
        sticky_json[JSON_KEY_TIMELINE_ANNOTATION_V_MAX_X] = notes[i].GetVMaxX();
        sticky_json[JSON_KEY_ANNOTATION_IS_MINIMIZED]     = notes[i].IsMinimized();

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
, m_show_annotations(true)
{
    if(m_project_settings.Valid())
    {
        m_project_settings.FromJson();
    }
}

void
AnnotationsManager::Clear()
{
    m_sticky_notes.clear();
}

void
AnnotationsManager::AddSticky(double time_ns, float y_offset, const ImVec2& size,
                              const std::string& text, const std::string& title,
                              double v_min, double v_max, uint64_t track_id,
                              bool is_minimized)
{
    m_sticky_notes.emplace_back(time_ns, y_offset, size, text, title, v_min, v_max,
                                track_id, is_minimized);
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
AnnotationsManager::CreateStickyNote(double time_ns, float y_offset, double v_min,
                                     double v_max, ImVec2 graph_size, uint64_t track_id)
{
    constexpr ImVec2 kNewNoteSize(220.0f, 150.0f);

    double note_time = time_ns;
    float  note_y    = y_offset;
    if(time_ns == INVALID_TIME_NS)
    {
        note_time = v_min + (v_max - v_min) * 0.5;
        note_y    = y_offset + graph_size.y * 0.5f;
    }

    AddSticky(note_time, note_y, kNewNoteSize, std::string(), std::string(), v_min, v_max,
              track_id, /*is_minimized=*/false);
    if(!m_sticky_notes.empty())
    {
        m_sticky_notes.back().BeginInlineEdit();
    }
}

void
AnnotationsManager::RemoveNotesPendingDelete()
{
    m_sticky_notes.erase(
        std::remove_if(m_sticky_notes.begin(), m_sticky_notes.end(),
                       [](const StickyNote& note) { return note.WantsDelete(); }),
        m_sticky_notes.end());
}

std::vector<StickyNote>&
AnnotationsManager::GetStickyNotes()
{
    return m_sticky_notes;
}

}  // namespace View
}  // namespace RocProfVis