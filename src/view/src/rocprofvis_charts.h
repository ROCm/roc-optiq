#pragma once
#include "rocprofvis_raw_track_data.h"
#include "rocprofvis_view_structs.h"
#include "rocprofvis_data_provider.h"

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

class Charts
{
public:
    Charts(DataProvider &dp, int id, std::string name, float zoom, float movement, double& min_x,
           double& max_x, float scale_x);

    virtual ~Charts() {}
    void               SetID(int id);
    int                GetID();
    virtual float      GetTrackHeight();
    virtual void       Render();
    const std::string& GetName();

    virtual void UpdateMovement(float zoom, float movement, double& min_x, double& max_x,
                                float scale_x,
                                float m_scroll_position);  // movement should be double?

    virtual void SetColorByValue(rocprofvis_color_by_value_t color_by_value_digits) = 0;
    bool         IsInViewVertical();
    void         SetInViewVertical(bool in_view);

    void  SetDistanceToView(float distance);
    float GetDistanceToView();

    virtual std::tuple<double, double> GetMinMax();
    virtual bool HandleTrackDataChanged() = 0;

    virtual bool HasData() = 0;
    virtual void ReleaseData() = 0;
    virtual void RequestData();

    TrackDataRequestState GetRequestState() const { return m_request_state; }
    
protected:
    virtual void RenderMetaArea()               = 0;
    virtual void RenderChart(float graph_width) = 0;
    virtual void RenderResizeBar(const ImVec2& parent_size);

    float       m_zoom;
    double      m_movement;
    double      m_min_x;
    double      m_max_x;
    double      m_scale_x;
    int         m_id;
    float       m_track_height;
    bool        m_is_in_view_vertical;
    float       m_distance_to_view_y;
    float       m_metadata_width;
    std::string m_name;
    ImU32       m_metadata_bg_color;
    ImVec2      m_metadata_padding;
    float       m_resize_grip_thickness;
    DataProvider& m_data_provider;
    TrackDataRequestState m_request_state;
};

}  // namespace View
}  // namespace RocProfVis
