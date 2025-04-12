#pragma once
#include "rocprofvis_raw_track_data.h"
//#include "rocprofvis_structs.h"
#include "rocprofvis_view_structs.h"

namespace RocProfVis
{
namespace View
{

class Charts
{
public:
    virtual void SetID(int id) = 0;
    virtual ~Charts() {}
    virtual float GetTrackHeight()                                                  = 0;
    virtual void  Render()                                                          = 0;
    virtual int   ReturnChartID()                                                   = 0;
    virtual void  UpdateMovement(float zoom, float movement, float& min_x, float& max_x,
                                 float scale_x)                                     = 0;
    virtual const std::string& GetName()                                            = 0;
    virtual void SetColorByValue(rocprofvis_color_by_value_t color_by_value_digits) = 0;

    virtual void SetRawData(RawTrackData* raw_data) = 0;
};


}  // namespace View
}  // namespace RocProfVis
