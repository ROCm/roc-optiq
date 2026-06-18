// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "imgui.h"
#include "model/rocprofvis_common_defs.h"
#include "rocprofvis_time_to_pixel.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace RocProfVis
{
namespace View
{

class TimePixelTransform;

// Sentinel for an unbound note (e.g. legacy project).
inline constexpr uint64_t INVALID_TRACK_ID = INVALID_UINT64_INDEX;

// Per-frame track layout used to anchor notes to their track.
struct TrackLayout
{
    std::function<bool(uint64_t track_id, float& out_top_y)> top_of;

    // track_at clamps to the first/last track so notes stay draggable anywhere.
    std::function<bool(float abs_y, uint64_t& out_track_id, float& out_top_y)>
        track_at;
};

class StickyNote
{
public:
    StickyNote(double time_ns, float y_offset, const ImVec2& size,
               const std::string& text, const std::string& title,
               const std::string& project_id, double v_min, double v_max,
               uint64_t track_id = INVALID_TRACK_ID, bool is_minimized = true);

    bool Render(ImDrawList* draw_list, const ImVec2& window_position,
                std::shared_ptr<TimePixelTransform> conversion_manager,
                const TrackLayout& layout);

    // Draws a non-interactive marker, used above a track's reorder preview.
    void RenderDragGhost(ImDrawList* draw_list, const ImVec2& screen_pos) const;
    bool HandleResize(const ImVec2&                       window_position,
                      std::shared_ptr<TimePixelTransform> conversion_manager,
                      const TrackLayout&                  layout);

    // Drag interaction
    bool HandleDrag(const ImVec2&                       window_position,
                    std::shared_ptr<TimePixelTransform> conversion_manager,
                    int& dragged_id, const TrackLayout& layout);
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
    bool               IsMinimized() const { return m_is_minimized; }
    uint64_t           GetTrackId() const { return m_track_id; }

private:
    // Binds an unbound note to a track, making its offset track-relative.
    void EnsureBound(const TrackLayout& layout);

    // Absolute Y from the bound track top plus offset (raw offset if unbound).
    float ResolveAnchorY(const TrackLayout& layout) const;

    // Vertical guide at the note's time, spanning the graph height.
    void RenderTimeIndicator(ImDrawList*                         draw_list,
                             const ImVec2&                       window_position,
                             std::shared_ptr<TimePixelTransform> tpt) const;

    double      m_time_ns;
    float       m_y_offset;  // Relative to m_track_id's top.
    uint64_t    m_track_id;
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
    bool        m_is_minimized;
    ImVec2      m_expanded_screen_pos = ImVec2(-1, -1);  // Temporary position for expanded note (not saved)
};

}  // namespace View
}  // namespace RocProfVis
