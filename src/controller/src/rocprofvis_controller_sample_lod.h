// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_sample.h"
#include <vector>

namespace RocProfVis
{
namespace Controller
{

class SampleLOD : public Sample
{
    void CalculateChildValues(void);

public:
    SampleLOD(rocprofvis_controller_primitive_type_t type, uint64_t id, double timestamp);

    SampleLOD(rocprofvis_controller_primitive_type_t type, uint64_t id, double timestamp,
              std::vector<Sample*>& children);

    SampleLOD& SampleLOD::operator=(SampleLOD&& other);

    virtual ~SampleLOD();

    rocprofvis_controller_object_type_t GetType(void) final;

    size_t GetNumChildren();

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index,
                                  uint64_t* value) final;
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index,
                                  double* value) final;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index,
                                  rocprofvis_handle_t** value) final;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index,
                                  char* value, uint32_t* length) final;

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index,
                                  uint64_t value) final;
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index,
                                  double value) final;
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index,
                                  rocprofvis_handle_t* value) final;
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index,
                                  char const* value, uint32_t length) final;

private:
    std::vector<Sample*> m_children;
    double               m_child_min;
    double               m_child_mean;
    double               m_child_median;
    double               m_child_max;
    double               m_child_min_timestamp;
    double               m_child_max_timestamp;
};

}
}
