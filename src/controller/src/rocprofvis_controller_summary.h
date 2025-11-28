// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller_summary_metrics.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_c_interface.h"
#include <string>

namespace RocProfVis
{
namespace Controller
{

class Trace;
class Counter;
class Node;
class Processor;
class SystemTable;
class Arguments;
class Array;
class Future;

class Summary : public Handle
{
public:
    Summary(Trace* ctx);

    ~Summary();

    rocprofvis_result_t Fetch(rocprofvis_dm_trace_t dm_handle, Arguments& args, SummaryMetrics& output, Future* future);

    rocprofvis_controller_object_type_t GetType(void) final;

    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) final;

private:
    rocprofvis_result_t FetchMetrics(rocprofvis_dm_trace_t dm_handle, Future* future);
    rocprofvis_result_t FetchCounterAverage(rocprofvis_dm_trace_t dm_handle, Counter* counter, float& average, Future* future) const;
    rocprofvis_result_t FetchTopKernels(rocprofvis_dm_trace_t dm_handle, Node* node, Processor* processor, SummaryMetrics& output, Future* future) const;

    Trace* m_ctx;

    double m_start_ts;
    double m_end_ts;

    SummaryMetrics m_metrics;
    SystemTable* m_kernel_instance_table;
};

}
}
