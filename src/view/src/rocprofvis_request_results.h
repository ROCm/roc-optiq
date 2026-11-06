// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "model/rocprofvis_model_types.h"
#include "rocprofvis_controller_enums.h"
#include "rocprofvis_raw_track_data.h"
#include "rocprofvis_requests.h"

#include <cstdint>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

/*
 * Rows read out of the controller for the request types whose effect on the
 * model depends on what the model already holds: track chunks merge into the
 * loaded data, and event details fill in an EventInfo the caller created. A
 * DataProvider applies these the same way whether it read them from its own
 * controller or received them from a server.
 */

struct TrackDataResult : public RequestResultBase
{
    std::vector<TraceEvent>   events;
    std::vector<TraceCounter> samples;
};

struct EventExtDataRow
{
    uint64_t    category_enum;  // rocprofvis_event_data_category_enum_t
    uint64_t    data_type;      // rocprofvis_controller_primitive_type_t
    std::string category;
    std::string name;
    std::string value;
    bool        has_uint_value;
    uint64_t    uint_value;
    uint16_t    arg_position;
    std::string arg_type;
};

struct EventExtDataResult : public RequestResultBase
{
    std::vector<EventExtDataRow> rows;
};

struct EventFlowResult : public RequestResultBase
{
    std::vector<EventFlowData> rows;
};

struct CallStackRow
{
    std::string file;
    std::string pc;
    std::string name;
    std::string address;
    bool        has_region_id;
    uint64_t    region_id;
};

struct EventCallStackResult : public RequestResultBase
{
    std::vector<CallStackRow> rows;
};

struct MetricValueRow
{
    rocprofvis_controller_metric_source_type_t source_type;
    uint32_t                                   source_id;
    uint32_t                                   category_id;
    uint32_t                                   table_id;
    uint32_t                                   entry_id;
    std::string                                value_name;
    double                                     value;
};

struct MetricsResult : public RequestResultBase
{
    std::vector<MetricValueRow> rows;
};

}  // namespace View
}  // namespace RocProfVis
