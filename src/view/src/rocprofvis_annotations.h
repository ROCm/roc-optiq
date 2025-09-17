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
class AnnotationsView;

class AnnotationsViewProjectSettings : public ProjectSetting
{
public:
    AnnotationsViewProjectSettings(const std::string& project_id,
                                   AnnotationsView&   annotations_view);
    ~AnnotationsViewProjectSettings() override;

    void ToJson() override;
    void FromJson();
    bool Valid() const override;

private:
    AnnotationsView& m_annotations_view;
};

class AnnotationsViewProjectSettings;

class AnnotationsView
{
public:
    AnnotationsView(const std::string& project_id);
    ~AnnotationsView();

    bool IsVisibile();
    void SetVisible(bool visible);

    void Clear();

    void AddSticky(double time_ns, float y_offset, const ImVec2& size,
                   const std::string& text, const std::string& title);
    bool Render(ImDrawList* draw_list, const ImVec2& window_position, double v_min_x,
                double pixels_per_ns, ImVec2 current_center);
    void ShowStickyNoteMenu(const ImVec2& window_position, const ImVec2& graph_size,
                            double v_min_x, double v_max_x, float scroll_y);
    void ShowStickyNotePopup();
    void ShowStickyNoteEditPopup();
    std::vector<StickyNote>& GetStickyNotes();
    void OpenStickyNotePopup(double time_ns /* = -1.0 */, float y_offset /* = -1.0f */);

private:
    std::vector<StickyNote>         m_sticky_notes;
    bool                            m_show_sticky_popup      = false;
    bool                            m_show_sticky_edit_popup = false;
    double                          m_sticky_time_ns         = 0.0;
    float                           m_sticky_y_offset        = 0.0f;
    char                            m_sticky_title[128]      = { 0 };
    char                            m_sticky_text[512]       = { 0 };
    int                             m_edit_sticky_id         = -1;
    EventManager::SubscriptionToken m_edit_token;
    bool                            m_show_annotations;
    ImVec2                          m_visible_center;
    std::string                     m_project_id;
    AnnotationsViewProjectSettings  m_project_settings;
    int m_dragged_sticky_id;  // ID of the sticky note currently being dragged
};

}  // namespace View
}  // namespace RocProfVis
