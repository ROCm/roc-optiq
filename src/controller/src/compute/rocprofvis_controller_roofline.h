// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_handle.h"
#include "rocprofvis_c_interface_types.h"
#include <vector>

namespace RocProfVis
{
namespace Controller
{

class Kernel;

class Roofline : public Handle
{
public:
    Roofline();

    virtual ~Roofline();

    rocprofvis_controller_object_type_t GetType(void) final;

    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final;
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value) final;

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) final;
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index, double value) final;

    bool QueryToPropertyEnum(rocprofvis_db_compute_column_enum_t in, rocprofvis_controller_roofline_ceiling_compute_type_t& out) const;
    bool QueryToPropertyEnum(rocprofvis_db_compute_column_enum_t in, rocprofvis_controller_roofline_ceiling_bandwidth_type_t& out) const;
    bool QueryToPropertyEnum(rocprofvis_db_compute_column_enum_t in, rocprofvis_controller_roofline_kernel_intensity_type_t& out) const;
    double MinX() const;
    double MaxX() const;
    double DynamicMaxXFactor() const;

private:
    struct Point {
        double x;
        double y;
    };
    struct CeilingBandwidth
    {
        rocprofvis_controller_roofline_ceiling_bandwidth_type_t type;
        Point position;
    };
    struct CeilingCompute
    {
        rocprofvis_controller_roofline_ceiling_compute_type_t type;
        Point position;
    };
    struct CeilingRidge
    {
        rocprofvis_controller_roofline_ceiling_bandwidth_type_t bandwidth_type;
        rocprofvis_controller_roofline_ceiling_compute_type_t compute_type;
        Point position;
    };
    struct Intensity
    {
        uint32_t kernel_id;
        rocprofvis_controller_roofline_kernel_intensity_type_t type;
        Point position;
    };

    std::vector<CeilingBandwidth> m_ceilings_bandwidth;
    std::vector<CeilingCompute> m_ceilings_compute;
    std::vector<CeilingRidge> m_ceilings_ridge;
    std::vector<Intensity> m_intensities;
    
};

}
}
