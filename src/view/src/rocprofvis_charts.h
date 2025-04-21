#pragma once
#include "rocprofvis_raw_track_data.h"
// #include "rocprofvis_structs.h"
#include "rocprofvis_view_structs.h"

namespace RocProfVis
{
namespace View
{

class Charts
{
public:
    Charts(int id, std::string name, float zoom, float movement, double& min_x,
           double& max_x, float scale_x);

    virtual void SetID(int id);
    virtual ~Charts() {}
    virtual float GetTrackHeight();
    virtual void  Render();
    virtual int   ReturnChartID();
    virtual void  UpdateMovement(
         float zoom, float movement, double& min_x, double& max_x, float scale_x,
         float m_scroll_position) = 0;  // movement should be double?
    virtual const std::string& GetName();

    virtual void  SetColorByValue(rocprofvis_color_by_value_t color_by_value_digits) = 0;
    virtual bool  GetVisibility();
    virtual float GetMovement() = 0;
    virtual std::tuple<double, double> GetMinMax();
    virtual bool                       SetRawData(const RawTrackData* raw_data) = 0;

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
    bool        m_is_chart_visible;
    float       m_metadata_width;
    std::string m_name;
};

}  // namespace View
}  // namespace RocProfVis
