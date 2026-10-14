// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_data.h"
#include "rocprofvis_controller_handle.h"

namespace RocProfVis
{
namespace Controller
{

class Sample : public Handle
{
public:
    Sample(rocprofvis_controller_primitive_type_t type, uint64_t id, double timestamp);

    Sample& operator=(Sample&& other);

    virtual ~Sample() {}

    rocprofvis_controller_object_type_t GetType(void) override
    {
        return kRPVControllerObjectTypeSample;
    }

    uint64_t GetId() const { return static_cast<uint64_t>(m_timestamp); }
    rocprofvis_controller_primitive_type_t GetValueType() const
    {
        return kRPVControllerPrimitiveTypeDouble;
    }
    double                      GetTimestamp() const { return m_timestamp; }
    double                      GetEndTimestamp() const { return m_end_timestamp; }
    double                      GetValue() const { return m_data; }
    virtual uint64_t            GetNumberOfChildren() const { return 0; }
    virtual rocprofvis_result_t GetInclusiveMemoryUsage(uint64_t* value) const;

    void SetValue(double value) { m_data = value; }
    void SetEndTimestamp(double timestamp) { m_end_timestamp = timestamp; }

private:
    // SampleLOD's C ABI overrides delegate base properties to these handlers.
    friend class SampleLOD;

    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index,
                                  uint64_t* value) override;
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index,
                                  double* value) override;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index,
                                  rocprofvis_handle_t** value) override;

    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index,
                                  double value) override;

protected:
    double       m_data;
    double       m_timestamp;
    double       m_end_timestamp;
};

}
}
