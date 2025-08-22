#pragma once
#include "imgui.h"
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
                   const std::string& text);
    bool Render(ImDrawList* draw_list, const ImVec2& window_position, double v_min_x,
                double pixels_per_ns);

    std::vector<StickyNote>& GetStickyNotes();

private:
    std::vector<StickyNote> m_sticky_notes;

};

}  // namespace View
}  // namespace RocProfVis
