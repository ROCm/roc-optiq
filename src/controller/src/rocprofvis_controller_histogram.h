// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_c_interface.h"
#include "rocprofvis_controller_data.h"
#include "rocprofvis_controller_handle.h"
#include <vector>

namespace RocProfVis
{
namespace Controller
{

class HistogramController : public Handle
{
public:
    HistogramController(std::vector<uint64_t> bins) ;
    virtual ~HistogramController();

    rocprofvis_controller_object_type_t GetType(void) override;

    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index,
                                  uint64_t* value) override;
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index,
                                  double* value) override;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index,
                                  rocprofvis_handle_t** value) override;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index,
                                  char* value, uint32_t* length) override;
    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index,
                                  uint64_t value) override;
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index,
                                  double value) override;
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index,
                                  rocprofvis_handle_t* value) override;
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index,
                                  char const* value, uint32_t length) override;

private:
    std::vector<uint64_t> m_bins;

    //vactor here 
};

}  // namespace Controller
}  // namespace RocProfVis
