#pragma once
#include "imgui.h"
#include <string>

namespace RocProfVis
{
namespace View
{

class StickyNote
{
public:
    StickyNote(double time_ns, float y_offset, const ImVec2& size,
               const std::string& text, const std::string& title, int id);

    void Render(ImDrawList* draw_list, const ImVec2& window_position, double v_min_x,
                double pixels_per_ns);
    bool HandleResize(const ImVec2& window_position, double v_min_x,
                      double pixels_per_ns);

    // Drag interaction
    bool HandleDrag(const ImVec2& window_position, double v_min_x, double pixels_per_ns);
    void SetTitle(std::string title);
    void SetText(std::string title);

    double             GetTimeNs() const { return m_time_ns; }
    float              GetYOffset() const { return m_y_offset; }
    ImVec2             GetSize() const { return m_size; }
    const std::string& GetText() const { return m_text; }
    int                GetID();
    void               SetTimeNs(double t) { m_time_ns = t; }
    void               SetYOffset(float y) { m_y_offset = y; }

private:
    double      m_time_ns;
    float       m_y_offset;
    int         m_id;
    ImVec2      m_size;
    std::string m_text;
    std::string m_title;
    bool        m_dragging      = false;
    ImVec2      m_drag_offset   = ImVec2(0, 0);
    ImVec2      m_resize_offset = ImVec2(0, 0);

    bool m_resizing = false;
};

}  // namespace View
}  // namespace RocProfVis
