#pragma once
#include "rocprofvis_structs.h"
#include <string>


struct rocprofvis_graph_map_t;
struct rocprofvis_color_by_value;

class Charts
{
public:
    virtual void SetID(int id) = 0;
    virtual ~Charts() {}
    virtual float ReturnSize()                  = 0;
    virtual void  Render()                      = 0;
    virtual int   ReturnChartID()               = 0;
    virtual void  UpdateMovement(float zoom, float movement, float& min_x, float& max_x,
                                 float scale_x) = 0;
    virtual std::string GetName()               = 0;
    virtual void SetColorByValue(rocprofvis_color_by_value color_by_value_digits) = 0;
};
