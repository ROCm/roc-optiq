// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_time_to_pixel.h"

#include <deque>
#include <unordered_map>

namespace RocProfVis
{
namespace View
{

inline constexpr float DEFAULT_TRACK_HEIGHT = 75.0f;

class SettingsManager;
class TrackItem;
class TrackOptions;
class CounterTrackOptions;
class QueueTrackOptions;
class TimelineTrackOptions;
class TimePixelTransform;
class TimelineSelection;
enum class Colors;

// Compare-source (A/B) badge shared by the timeline meta area and the sidebar.
void
RenderCompareSourceBadge(const TrackInfo* track_info, SettingsManager& settings);
float
CompareSourceBadgeWidth(const TrackInfo* track_info);

enum class TrackDataRequestState
{
    kIdle,
    kRequesting,
    kTimeout,
    kError
};

class Pill
{
public:
    enum Sizing : size_t
    {
        kElided,
        kCompact,
        kExtended,
        kCount
    };

    Pill(bool shown, bool active);
    ~Pill();

    void   SetLabel(const std::string& label);
    void   SetExtendedLabel(const std::string& label);
    void   SetTooltip(std::string label);
    void   SetAccentColor(size_t accent_color);
    void   SetTextColor(std::optional<Colors> color);
    void   Activate();
    void   Deactivate();
    void   Render(const ImVec2& pos, SettingsManager& settings, Sizing sizing = kCompact);
    ImVec2 Size();
    ImVec2 CompactSize();
    ImVec2 ExtSize();
    ImVec2 ElidedSize();
    bool   Visible() const;
    void   SetVisible(bool visible);

private:
    void                              CalculateSize();
    bool                              m_show_pill_label;
    bool                              m_active;
    std::optional<size_t>             m_accent_color;
    // Overrides the value text color when set; unset falls back to the default.
    std::optional<Colors>             m_text_color;
    Sizing                            m_sizing;
    std::string                       m_compact_label;
    std::string                       m_ext_label;
    std::string                       m_tooltip;
    std::array<float, Sizing::kCount> m_widths;
    float                             m_height;
    const float                       m_padding_x = 8.0f;
    const float                       m_padding_y = 2.0f;
    EventManager::SubscriptionToken   m_font_changed_token;
};

class TrackItem
{
    friend TimelineTrackOptions;
    friend TrackOptions;
    friend CounterTrackOptions;
    friend QueueTrackOptions;

public:
    TrackItem(DataProvider& dp, uint64_t id, TimelineTrackOptions& track_options,
              std::shared_ptr<TimePixelTransform> tpt,
              std::shared_ptr<TimelineSelection>  timeline_selection = nullptr);
    virtual ~TrackItem();
    void               SetID(uint64_t id);
    uint64_t           GetID() const;
    virtual float      GetTrackHeight() const;
    virtual void       Render(float width);
    virtual void       Update();
    const std::string& GetName() const;
    bool               IsInViewVertical() const;
    void               SetInViewVertical(bool in_view);

    bool IsSelected() const;

    bool IsDisplayed() const;
    void SetDisplay(bool display);

    void  SetDistanceToView(float distance);
    float GetDistanceToView() const;

    bool        TrackHeightChanged();
    static void SetSidebarSize(float sidebar_size);

    virtual bool HasData() const;
    bool         AllDataReady() const;
    virtual bool ReleaseData();
    virtual void RequestData(double min, double max, float width);
    void         RequestAnalysis();
    virtual void HandleTrackDataChanged(uint64_t request_id, uint64_t response_code);
    virtual bool HasPendingRequests() const;
    virtual bool HasPendingRequest(uint64_t request_id) const;
    virtual void UpdateMetaScaleAreaSize();
    virtual void UpdateMaxMetaScaleAreaSize();
    virtual bool IsCompactMode() const { return false; }

    TrackDataRequestState GetRequestState() const { return m_request_state; }

    bool IsMetaAreaClicked() const { return m_meta_area_clicked; }

    float GetReorderGripWidth();

    float GetMaxMetaAreaScaleWidth() { return m_max_meta_area_scale_width; }

    const TrackInfo* GetTrackInfo() const;

protected:
    virtual void  RenderMetaArea();
    virtual void  RenderMetaAreaScale();
    virtual float GetMetaAreaTrailingWidth() const;
    virtual void  RenderMetaAreaExpand();
    virtual void  RenderChart(float graph_width) = 0;
    virtual void  RenderResizeBar(const ImVec2& parent_size);
    virtual void  ExtractPointsFromData() = 0;

    void  FetchHelper();
    void  CancelPendingRequests();
    void  SetDefaultPillLabel(const TrackInfo* track_info);
    void  SetMetaAreaLabel(const TrackInfo* track_info);
    void  SetNodeColor(const TrackInfo* track_info);
    Pill* AddPill(bool shown = true, bool active = true);
    bool  HasSavedTrackHeight() const;
    float GetMetaAreaMinHeight() const;

    const TrackInfo*                    m_track_metadata;
    const AnalysisTrackStatistics*      m_track_statistics;
    bool                                m_track_statistics_dirty;
    uint64_t                            m_track_id;
    float                               m_track_content_height;
    float                               m_min_track_height;
    bool                                m_track_height_changed;
    bool                                m_is_in_view_vertical;
    float                               m_distance_to_view_y;
    std::string                         m_name;
    ImVec2                              m_metadata_padding;
    float                               m_resize_grip_thickness;
    DataProvider&                       m_data_provider;
    TrackDataRequestState               m_request_state;
    SettingsManager&                    m_settings;
    bool                                m_meta_area_clicked;
    float                               m_meta_area_scale_width;
    float                               m_max_meta_area_scale_width;
    float                               m_reorder_grip_width;
    std::shared_ptr<TimePixelTransform> m_tpt;
    std::shared_ptr<TimelineSelection>  m_timeline_selection;
    uint64_t m_chunk_duration_ns;  // Duration of each chunk in nanoseconds
    uint8_t  m_group_id_counter;   // Counter for grouping requests
    // Cancellation has already been requested for everything in
    // m_pending_requests; they stay pending until their futures resolve.
    bool     m_cancel_requested;

    std::deque<TrackRequestParams>                   m_request_queue;
    std::unordered_map<uint64_t, TrackRequestParams> m_pending_requests;
    static float                                     s_metadata_width;
    std::string                                      m_meta_area_label;
    std::string                                      m_meta_area_tooltip;

    // Node color-coding: a pill showing the node's display index, accented with
    // a color-wheel slot keyed by that index. Gated at render time by the
    // "Color-code tracks by node" preference.
    bool        m_has_node_color;
    size_t      m_node_color_index;
    size_t      m_node_display_index;
    std::string m_node_name;
    Pill*       m_node_pill;

    // User configurable options.
    std::unique_ptr<TrackOptions> m_options;
    TimelineTrackOptions&         m_timeline_track_options;

private:
    void RenderPills(ImVec2 region);

    std::vector<std::unique_ptr<Pill>> m_pills;
    bool                               m_selected;
    EventManager::SubscriptionToken    m_selected_changed_token;
};

}  // namespace View
}  // namespace RocProfVis
