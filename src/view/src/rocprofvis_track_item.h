#pragma once
#include "rocprofvis_data_provider.h"
#include "rocprofvis_raw_track_data.h"
#include "rocprofvis_settings.h"
#include "rocprofvis_view_structs.h"

#include <deque>

namespace RocProfVis
{
namespace View
{

enum class TrackDataRequestState
{
    kIdle,
    kRequesting,
    kTimeout,
    kError
};

class TrackItem
{
public:
    TrackItem(DataProvider& dp, uint64_t id, std::string name, float zoom, double time_offset_ns,
              double& min_x, double& max_x, double scale_x);

    virtual ~TrackItem() {}
    void               SetID(uint64_t id);
    uint64_t           GetID();
    virtual float      GetTrackHeight();
    virtual void       Render(float width);
    void               Update();
    const std::string& GetName();

    virtual void UpdateMovement(float zoom, double time_offset_ns, double& min_x, double& max_x,
                                double scale_x,
                                float m_scroll_position);

    virtual void SetColorByValue(rocprofvis_color_by_value_t color_by_value_digits) = 0;
    bool         IsInViewVertical();
    void         SetInViewVertical(bool in_view);

    bool IsSelected() const;
    void SetSelected(bool selected);

    void  SetDistanceToView(float distance);
    float GetDistanceToView();

    virtual std::tuple<double, double> GetMinMax();
    virtual bool                       HandleTrackDataChanged() = 0;
    
    bool        GetResizeStatus();
    static void SetSidebarSize(int sidebar_size);

    virtual bool HasData()     = 0;
    virtual void ReleaseData() = 0;
    virtual void RequestData(double min, double max, float width);

    TrackDataRequestState GetRequestState() const { return m_request_state; }

    bool IsMetaAreaClicked() const { return m_meta_area_clicked; }

    float GetReorderGripWidth();


protected:
    virtual void RenderMetaArea();
    virtual void RenderMetaAreaScale(ImVec2& container_size);
    virtual void RenderChart(float graph_width) = 0;
    virtual void RenderResizeBar(const ImVec2& parent_size);

    void FetchHelper();

    float                 m_zoom;
    double                m_time_offset_ns;
    double                m_min_x;
    double                m_max_x;
    double                m_scale_x;
    uint64_t              m_id;
    float                 m_track_height;
    float                 m_track_content_height;
    float                 m_min_track_height;
    bool                  m_is_in_view_vertical;
    float                 m_distance_to_view_y;
    std::string           m_name;
    ImVec2                m_metadata_padding;
    float                 m_resize_grip_thickness;
    bool                  m_is_resize;
    DataProvider&         m_data_provider;
    TrackDataRequestState m_request_state;
    Settings&             m_settings;
    bool                  m_meta_area_clicked;
    float                 m_meta_area_scale_width;
    bool                  m_selected;
    float                 m_reorder_grip_width;
    
    uint64_t m_chunk_duration_ns = 0;  // Duration of each chunk in nanoseconds
    uint64_t m_group_id_counter = 0;  // Counter for grouping requests

    std::deque<TrackRequestParams> m_request_queue;

    static float s_metadata_width;
};

}  // namespace View
}  // namespace RocProfVis
