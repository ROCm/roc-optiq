// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include <cstddef>
#include <string>
#include <vector>
#include "rocprofvis_c_interface.h"

namespace jt
{
class Json;
}

namespace RocProfVis
{
namespace Controller
{

class Array;
class Track;
class Callstack;
class FlowControl;

class Event : public Handle
{
public:
    Event(uint64_t id, double start_ts, double end_ts);

    Event& operator=(Event&& other);

    virtual ~Event();

    rocprofvis_controller_object_type_t GetType(void) override
    {
        return kRPVControllerObjectTypeEvent;
    }

    rocprofvis_result_t Fetch(rocprofvis_property_t property,  Array& array, rocprofvis_dm_trace_t dm_handle);

    uint64_t GetId() const { return m_id; }
    uint8_t  GetLevel() const { return m_level; }
    double   GetStartTimestamp() const { return m_start_timestamp; }
    double   GetEndTimestamp() const { return m_end_timestamp; }
    double   GetDuration() const { return GetEndTimestamp() - GetStartTimestamp(); }

    const char* GetName() const;
    const char* GetCategory() const;
    const char* GetTopCombinedName() const;
    size_t      GetNameStringIndex() const { return m_name; }
    size_t      GetCategoryStringIndex() const { return m_category; }
    size_t      GetTopCombinedNameStringIndex() const { return m_combined_top_name; }

    uint64_t            GetNumberOfChildren() const;
    rocprofvis_result_t GetChildId(uint64_t index, uint64_t* value) const;
    rocprofvis_result_t GetInclusiveMemoryUsage(uint64_t* value) const;

    void                SetLevel(uint64_t level);
    void SetStartTimestamp(double timestamp) { m_start_timestamp = timestamp; }
    void SetEndTimestamp(double timestamp) { m_end_timestamp = timestamp; }
    void SetTopCombinedNameStringIndex(size_t index) { m_combined_top_name = index; }
    rocprofvis_result_t SetName(const char* name);
    rocprofvis_result_t SetCategory(const char* category);
    rocprofvis_result_t SetNumberOfChildren(uint64_t count);
    rocprofvis_result_t SetChildId(uint64_t index, uint64_t id);

    bool IsDeletable() override
    {
        return (m_retain_counter == 0) ? true : --m_retain_counter == 0;
    }
    void IncreaseRetainCounter() override { m_retain_counter++; }

private:
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index,
                                  uint64_t* value) override;
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index,
                                  double* value) override;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index,
                                  char* value, uint32_t* length) override;
    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index,
                                  uint64_t value) override;
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index,
                                  double value) override;
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index,
                                  char const* value) override;

    Array* m_children;
    uint64_t m_id;
    double m_start_timestamp;
    double m_end_timestamp;
    size_t m_name;
    size_t m_category;
    size_t m_combined_top_name;
    uint8_t m_level;
    uint32_t m_retain_counter;

public:
    static rocprofvis_result_t FetchDataModelFlowTraceProperty(uint64_t event_id, Array& array, rocprofvis_dm_trace_t dm_trace_handle);
    static rocprofvis_result_t FetchDataModelStackTraceProperty(uint64_t event_id, Array& array, rocprofvis_dm_trace_t dm_trace_handle);
    static rocprofvis_result_t FetchDataModelExtendedDataProperty(uint64_t event_id, Array& array, rocprofvis_dm_trace_t dm_trace_handle);

private:
    static std::string FromJson(const char* key, jt::Json& json);
};

}
}
