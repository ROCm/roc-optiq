#pragma once
#include "imgui.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_events.h"
#include "rocprofvis_stickynote.h"
#include <vector>

namespace RocProfVis
{
namespace View
{

class AnnotationsView
{
public:
    AnnotationsView();
    ~AnnotationsView();

    void AddSticky(double time_ns, float y_offset, const ImVec2& size,
                   const std::string& text, const std::string& title);
    bool Render(ImDrawList* draw_list, const ImVec2& window_position, double v_min_x,
                double pixels_per_ns);
    void ShowStickyNoteMenu(const ImVec2& window_position, const ImVec2& graph_size,
                            double v_min_x, double v_max_x);
    void ShowStickyNotePopup();

    void                     ShowStickyNoteEditPopup();
     std::vector<StickyNote>& GetStickyNotes();
    void                     OpenStickyNotePopup(double time_ns, float y_offset);

private:
    std::vector<StickyNote>         m_sticky_notes;
    bool                            m_show_sticky_popup      = false;
    bool                            m_show_sticky_edit_popup = false;
    double                          m_sticky_time_ns         = 0.0;
    float                           m_sticky_y_offset        = 0.0f;
    char                            m_sticky_title[128]      = { 0 };
    char                            m_sticky_text[512]       = { 0 };
    int                             m_edit_sticky_index      = -1;
    EventManager::SubscriptionToken m_edit_token;
};
}  // namespace View
}  // namespace RocProfVis
