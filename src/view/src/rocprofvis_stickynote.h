// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

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
               const std::string& text, const std::string& title,
               const std::string& project_id, double v_min, double v_max);

    void Render(ImDrawList* draw_list, const ImVec2& window_position, double v_min_x,
                double pixels_per_ns);
    bool HandleResize(const ImVec2& window_position, double v_min_x, double v_max_x,
                      double pixels_per_ns);

    // Drag interaction
    bool HandleDrag(const ImVec2& window_position, double v_min_x, double v_max_x, double pixels_per_ns,
                    int& dragged_id);
    void SetTitle(std::string title);
    void SetText(std::string title);

    double             GetTimeNs() const;
    float              GetYOffset() const;
    ImVec2             GetSize() const;
    const std::string& GetText() const;
    const std::string& GetTitle() const;
    void               SetVisibility(bool visible);
    bool               IsVisible() const;
    int                GetID() const;
    void               SetTimeNs(double t) { m_time_ns = t; }
    void               SetYOffset(float y) { m_y_offset = y; }
    double             GetVMinX() const;
    double             GetVMaxX() const;

private:
    double      m_time_ns;
    float       m_y_offset;
    int         m_id;
    ImVec2      m_size;
    std::string m_text;
    std::string m_title;
    std::string m_project_id;
    bool        m_dragging      = false;
    ImVec2      m_drag_offset   = ImVec2(0, 0);
    ImVec2      m_resize_offset = ImVec2(0, 0);
    bool        m_is_visible;
    double      m_v_min_x;
    double      m_v_max_x;
    bool        m_resizing = false;
};

}  // namespace View
}  // namespace RocProfVis
