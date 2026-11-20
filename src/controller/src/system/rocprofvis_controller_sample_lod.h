// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

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
    SampleLOD(uint64_t id, double timestamp);

    SampleLOD(uint64_t id, double timestamp, std::vector<Sample*>& children);

    SampleLOD& operator=(SampleLOD&& other);

    virtual ~SampleLOD() {}

    rocprofvis_controller_object_type_t GetType(void) final
    {
        return kRPVControllerObjectTypeSample;
    }

    uint64_t                          GetNumberOfChildren() const noexcept override { return m_children.size(); }
    [[nodiscard]] rocprofvis_result_t GetChild(uint64_t index, Sample** value) const;
    [[nodiscard]] rocprofvis_result_t SetChild(uint64_t index, Sample* child);
    [[nodiscard]] rocprofvis_result_t SetNumberOfChildren(uint64_t count);
    [[nodiscard]] rocprofvis_result_t GetInclusiveMemoryUsage(uint64_t* value) const override;

    double GetChildMin() const noexcept { return m_child_min; }
    double GetChildMean() const noexcept { return m_child_mean; }
    double GetChildMedian() const noexcept { return m_child_median; }
    double GetChildMax() const noexcept { return m_child_max; }
    double GetChildMinTimestamp() const noexcept { return m_child_min_timestamp; }
    double GetChildMaxTimestamp() const noexcept { return m_child_max_timestamp; }

    void SetChildMin(double value) noexcept { m_child_min = value; }
    void SetChildMean(double value) noexcept { m_child_mean = value; }
    void SetChildMedian(double value) noexcept { m_child_median = value; }
    void SetChildMax(double value) noexcept { m_child_max = value; }
    void SetChildMinTimestamp(double timestamp) noexcept { m_child_min_timestamp = timestamp; }
    void SetChildMaxTimestamp(double timestamp) noexcept { m_child_max_timestamp = timestamp; }

private:
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index,
                                  uint64_t* value) final;
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index,
                                  double* value) final;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index,
                                  rocprofvis_handle_t** value) final;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index,
                                  char* value, uint32_t* length) final
    {
        return Sample::GetString(property, index, value, length);
    }

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index,
                                  uint64_t value) final;
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index,
                                  double value) final;
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index,
                                  rocprofvis_handle_t* value) final;
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index,
                                  char const* value) final;

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
