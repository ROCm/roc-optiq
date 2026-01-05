// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_data_provider.h"
#include "rocprofvis_project.h"
#include <deque>
#include <unordered_map>
#include "rocprofvis_time_to_pixel.h"

namespace RocProfVis
{
namespace View
{

class SettingsManager;
class TrackItem;
class TimePixelTransform;

enum class TrackDataRequestState
{
    kIdle,
    kRequesting,
    kTimeout,
    kError
};

class TrackProjectSettings : public ProjectSetting
{
public:
    TrackProjectSettings(const std::string& project_id, TrackItem& track_item);
    ~TrackProjectSettings() override;
    void ToJson() override;
    bool Valid() const override;

    float Height() const;

private:
    TrackItem& m_track_item;
};

class TrackItem
{
public:
    TrackItem(DataProvider& dp, uint64_t id, std::string name,
              std::shared_ptr<TimePixelTransform> tpt);
    virtual ~TrackItem() {}
    void               SetID(uint64_t id);
    uint64_t           GetID();
    virtual float      GetTrackHeight();
    virtual void       Render(float width);
    virtual void       Update();
    const std::string& GetName();
    void               RenderPillLabel(ImVec2 container_size);
    bool IsInViewVertical();
    void SetInViewVertical(bool in_view);

    bool IsSelected() const;
    void SetSelected(bool selected);

    void  SetDistanceToView(float distance);
    float GetDistanceToView();

 
    bool        TrackHeightChanged();
    static void SetSidebarSize(float sidebar_size);

    virtual bool  HasData();
    virtual bool  ReleaseData();
    virtual void  RequestData(double min, double max, float width);
    virtual bool  HandleTrackDataChanged(uint64_t request_id, uint64_t response_code);
    virtual bool  HasPendingRequests() const;
    virtual float CalculateNewMetaAreaSize();
    virtual bool  IsCompactMode() const { return false; }

    TrackDataRequestState GetRequestState() const { return m_request_state; }

    bool IsMetaAreaClicked() const { return m_meta_area_clicked; }

    float GetReorderGripWidth();

    float GetMetaAreaScaleWidth() { return m_meta_area_scale_width; }
    void  UpdateMaxMetaAreaSize(float newSize);


protected:
    virtual void RenderMetaArea();
    virtual void RenderMetaAreaScale()          = 0;
    virtual void RenderMetaAreaOptions()        = 0;
    virtual void RenderMetaAreaExpand();
    virtual void RenderChart(float graph_width) = 0;
    virtual void RenderResizeBar(const ImVec2& parent_size);
    virtual bool ExtractPointsFromData() = 0;

    void FetchHelper();

    uint64_t              m_id;
    float                 m_track_height;
    float                 m_track_content_height;
    float                 m_track_default_height; //TODO: It should be a constant, we don't need store it for each track
    float                 m_min_track_height;
    bool                  m_track_height_changed;
    bool                  m_is_in_view_vertical;
    float                 m_distance_to_view_y;
    std::string           m_name;
    ImVec2                m_metadata_padding;
    float                 m_resize_grip_thickness;
    DataProvider&         m_data_provider;
    TrackDataRequestState m_request_state;
    SettingsManager&      m_settings;
    bool                  m_meta_area_clicked;
    float                 m_meta_area_scale_width;
    bool                  m_selected;
    float                 m_reorder_grip_width;
    std::shared_ptr<TimePixelTransform> m_tpt;
    uint64_t m_chunk_duration_ns;  // Duration of each chunk in nanoseconds
    uint8_t  m_group_id_counter;   // Counter for grouping requests

    std::deque<TrackRequestParams>                   m_request_queue;
    std::unordered_map<uint64_t, TrackRequestParams> m_pending_requests;
    static float                                     s_metadata_width;
    bool                                             m_show_pill_label;
    std::string                                      m_pill_label;

private:
    TrackProjectSettings m_track_project_settings;
};

}  // namespace View
}  // namespace RocProfVis
