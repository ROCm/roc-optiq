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

// Sentinel for "no note is currently being dragged".
inline constexpr int INVALID_STICKY_ID = -1;

// Per-frame track layout used to anchor notes to their track.
struct TrackLayout
{
    std::function<bool(uint64_t track_id, float& out_top_y)> top_of;

    // Current height of a track, used to cut off / hide an anchor whose offset
    // no longer fits after the track is shrunk.
    std::function<bool(uint64_t track_id, float& out_height)> height_of;

    // track_at clamps to the first/last track so notes stay draggable anywhere.
    std::function<bool(float abs_y, uint64_t& out_track_id, float& out_top_y)>
        track_at;

    // Visible track viewport in content-space Y; a dragged anchor is clamped to
    // this range so it stays on-screen (auto-scroll reveals the rest).
    float view_min_y = 0.0f;
    float view_max_y = 0.0f;
};

class StickyNote
{
public:
    StickyNote(double time_ns, float y_offset, const ImVec2& size,
               const std::string& text, const std::string& title, double v_min,
               double v_max, uint64_t track_id = INVALID_TRACK_ID,
               bool is_minimized = true);

    bool Render(ImDrawList* draw_list, const ImVec2& window_position,
                std::shared_ptr<TimePixelTransform> conversion_manager,
                const TrackLayout& layout);

    // Draws a non-interactive marker, used above a track's reorder preview.
    void RenderDragGhost(ImDrawList* draw_list, const ImVec2& screen_pos) const;

    // Drags the timeline anchor marker (minimized or expanded), moving the note's
    // timeline position. The expanded window keeps its own screen position.
    bool HandleDrag(const ImVec2&                       window_position,
                    std::shared_ptr<TimePixelTransform> conversion_manager,
                    int& dragged_id, const TrackLayout& layout);
    void SetTitle(std::string title);
    void SetText(std::string text);

    // Opens the note expanded and focused for inline creation. Provisional until
    // the user types; an empty note abandoned this way is auto-discarded.
    void BeginInlineEdit();

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
    void               SetTrackHidden(bool hidden) { m_track_hidden = hidden; }
    bool               IsTrackHidden() const { return m_track_hidden; }
    bool               IsDragging() const { return m_dragging; }

    // True after the user hit the delete button; the owner sweeps these notes.
    bool               WantsDelete() const { return m_pending_delete; }

private:
    // Binds an unbound note to a track, making its offset track-relative.
    void EnsureBound(const TrackLayout& layout);

    // Absolute Y from the bound track top plus offset (raw offset if unbound).
    float ResolveAnchorY(const TrackLayout& layout) const;

    // Vertical guide at the note's time, spanning the graph height.
    void RenderTimeIndicator(ImDrawList*                         draw_list,
                             const ImVec2&                       window_position,
                             std::shared_ptr<TimePixelTransform> tpt) const;

    // Always-visible timeline anchor; clipped to clip rect when non-null.
    // Returns true if hovered.
    bool RenderAnchorMarker(ImDrawList* draw_list, const ImVec2& marker_pos,
                            const ImVec2* clip_min, const ImVec2* clip_max);

    // Floating expanded window. Returns true if hovered.
    bool RenderExpandedWindow(const ImVec2& anchor_pos);

    // True once the floating window has a real stored position.
    bool IsExpandedPlaced() const
    {
        return m_expanded_screen_pos.x >= 0.0f && m_expanded_screen_pos.y >= 0.0f;
    }

    double      m_time_ns;
    float       m_y_offset;  // Relative to m_track_id's top.
    uint64_t    m_track_id;
    int         m_id;
    ImVec2      m_size;
    std::string m_text;
    std::string m_title;
    bool        m_dragging     = false;
    bool        m_track_hidden = false;
    float       m_drag_abs_y   = 0.0f;  // Anchor Y during a drag; bound on drop.
    ImVec2      m_drag_offset  = ImVec2(0, 0);
    bool        m_is_visible;
    double      m_v_min_x;
    double      m_v_max_x;
    bool        m_is_minimized;
    bool        m_pending_delete = false;
    bool        m_request_focus  = false;
    bool        m_focus_input    = false;  // Focus the title field next frame.
    bool        m_editing_title  = false;
    bool        m_provisional    = false;  // Freshly created; discard if left empty.
    bool        m_seen_focus     = false;
    // Absolute screen position of the floating expanded window; (-1, -1) until
    // first placed. Tracked live so the window stays where the user moved it.
    // Not persisted to the project.
    ImVec2      m_expanded_screen_pos = ImVec2(-1, -1);
};

}  // namespace View
}  // namespace RocProfVis
