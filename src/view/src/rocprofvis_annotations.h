// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "imgui.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_events.h"
#include "rocprofvis_project.h"
#include "rocprofvis_stickynote.h"
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

constexpr double INVALID_TIME_NS   = std::numeric_limits<double>::lowest();
 
class AnnotationsManager;

class AnnotationsManagerProjectSettings : public ProjectSetting
{
public:
    AnnotationsManagerProjectSettings(const std::string&  project_id,
                                      AnnotationsManager& annotations_view);
    ~AnnotationsManagerProjectSettings() override;

    void ToJson() override;
    void FromJson();
    bool Valid() const override;

private:
    AnnotationsManager& m_annotations_manager;
};

class AnnotationsManagerProjectSettings;

class AnnotationsManager
{
public:
    AnnotationsManager(const std::string& project_id);
    ~AnnotationsManager();

    bool IsVisibile();
    void SetVisible(bool visible);
    void Clear();
    void SetCenter(const ImVec2& center);
    void AddSticky(double time_ns, float y_offset, const ImVec2& size,
                   const std::string& text, const std::string& title, double v_min,
                   double v_max, bool is_minimized = true);
    

    void                     ShowStickyNotePopup();
    void                     ShowStickyNoteEditPopup();
    std::vector<StickyNote>& GetStickyNotes();
    void OpenStickyNotePopup(double time_ns, float y_offset, double v_min, double v_max, ImVec2 graph_size);

private:
    std::vector<StickyNote>           m_sticky_notes;
    bool                              m_show_sticky_popup      = false;
    bool                              m_show_sticky_edit_popup = false;
    double                            m_sticky_time_ns         = 0.0;
    float                             m_sticky_y_offset        = 0.0f;
    char                              m_sticky_title[128]      = { 0 };
    char                              m_sticky_text[512]       = { 0 };
    int                               m_edit_sticky_id         = -1;
    EventManager::SubscriptionToken   m_edit_token;
    bool                              m_show_annotations;
    ImVec2                            m_visible_center;
    std::string                       m_project_id;
    AnnotationsManagerProjectSettings m_project_settings;
    double                            m_v_min_x;  
    double                            m_v_max_x;  
    int m_dragged_sticky_id; 
};

}  // namespace View
}  // namespace RocProfVis
