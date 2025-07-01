#pragma once
#include "rocprofvis_data_provider.h"
#include "rocprofvis_raw_track_data.h"
#include "rocprofvis_settings.h"
#include "rocprofvis_view_structs.h"

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
    TrackItem(DataProvider& dp, int id, std::string name, float zoom, float movement,
              double& min_x, double& max_x, float scale_x);

    virtual ~TrackItem() {}
    void               SetID(int id);
    int                GetID();
    virtual float      GetTrackHeight();
    virtual void       Render(float width);
    void               Update();
    const std::string& GetName();

    virtual void UpdateMovement(float zoom, float movement, double& min_x, double& max_x,
                                float scale_x,
                                float m_scroll_position);  // movement should be double?

    virtual void SetColorByValue(rocprofvis_color_by_value_t color_by_value_digits) = 0;
    bool         IsInViewVertical();
    void         SetInViewVertical(bool in_view);

    bool IsSelected() const;
    void SetSelected(bool selected);

    void  SetDistanceToView(float distance);
    float GetDistanceToView();

    virtual std::tuple<double, double> GetMinMax();
    virtual bool                       HandleTrackDataChanged() = 0;
    // virtual bool                       SetRawData(const RawTrackData* raw_data) = 0;
    bool        GetResizeStatus();
    static void SetSidebarSize(int sidebar_size);

    virtual bool HasData()     = 0;
    virtual void ReleaseData() = 0;
    virtual void RequestData(double min, double max, float width);

    TrackDataRequestState GetRequestState() const { return m_request_state; }

    bool IsMetaAreaClicked() const { return m_meta_area_clicked; }


protected:
    virtual void RenderMetaArea();
    virtual void RenderMetaAreaScale(ImVec2& container_size);
    virtual void RenderChart(float graph_width) = 0;
    virtual void RenderResizeBar(const ImVec2& parent_size);

    void FetchHelper();

    float                 m_zoom;
    double                m_movement;
    double                m_min_x;
    double                m_max_x;
    double                m_scale_x;
    int                   m_id;
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

    std::shared_ptr<TrackRequestParams> m_deferred_request;

    static float s_metadata_width;
};

}  // namespace View
}  // namespace RocProfVis
