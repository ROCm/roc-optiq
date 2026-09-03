#pragma once

#include <cstdint>
#include <variant>
#include <profiler-hub/reader_types.hpp>

typedef void* profiler_hub_handle_t;
typedef profiler_hub_handle_t profiler_hub_trace_handle_t;
typedef profiler_hub_handle_t client_trace_handle_t;
typedef profiler_hub_handle_t profiler_hub_future_handle_t;
typedef profiler_hub_handle_t profiler_hub_value_handle_t;
typedef profiler_hub_handle_t profiler_hub_timeslice_handle_t;
typedef profiler_hub_handle_t profiler_hub_flowtrace_handle_t;
typedef profiler_hub_handle_t profiler_hub_ext_data_handle_t;
typedef profiler_hub_handle_t profiler_hub_table_handle_t;
typedef profiler_hub_handle_t profiler_hub_table_row_handle_t;
typedef profiler_hub_handle_t profiler_hub_call_stack_handle_t;
typedef const char* profiler_hub_string_t;
typedef const void** profiler_hub_optional_t;
typedef const char** profiler_hub_optional_string_t;
typedef const char** profiler_hub_search_strings_t;
typedef uint64_t* profiler_hub_optional_int_t;
typedef double* profiler_hub_optional_double_t;
typedef uint32_t profiler_hub_instance_id_t;
typedef uint32_t profiler_hub_track_id_t;
typedef uint64_t profiler_hub_event_id_t;
typedef uint32_t profiler_hub_string_id_t;
typedef uint32_t profiler_hub_event_level_t;

// Error status, the list to be updated during development
typedef enum profiler_hub_result_t {
    kProfilerHubStatusSuccess,
    kProfilerHubStatusUnknownError,
    kProfilerHubStatusNotSupported,
    kProfilerHubStatusInvalidArgument,
    kProfilerHubStatusTimeout,
    kProfilerHubStatusNotLoaded,
} profiler_hub_result_t;

typedef enum profiler_hub_async_status_t {
    kProfilerHubAsyncSuccess,
    kProfilerHubAsyncFailed,
    kProfilerHubAsyncBusy
} profiler_hub_async_status_t;

// value types, inherited from SQL types
typedef enum profiler_hub_value_type_t
{
    kPprofilerHubDataTypeUndefined = 0,
    kPprofilerHubDataTypeInt = 1,
    kPprofilerHubDataTypeDouble = 2,
    kPprofilerHubDataTypeString = 3,
    kPprofilerHubDataTypeBlob = 4,
    kPprofilerHubDataTypeNull = 5
} profiler_hub_value_type_t;

// track categories
typedef enum profiler_hub_track_category_t {

    kPprofilerHubCategoryUndefined = 0,
    kPprofilerHubCategoryRegionInstrumented,
    kPprofilerHubCategoryRegionSampled,
    kPprofilerHubCategoryKernelDispatch,
    kPprofilerHubCategoryMemoryAllocate,
    kPprofilerHubCategoryMemoryCopy,
    kPprofilerHubCategoryPerformanceCounter,
    kPprofilerHubCategoryStream,
} profiler_hub_track_category_t;

// event operations
typedef profiler_hub::reader_types::event_type_t profiler_hub_event_operation_t;

// property category will determine which info table or join read the property from
// all the info tables are supposed to be cached
typedef enum profiler_hub_property_category_t{
    kProfilerHubPropertyNode,
    kProfilerHubPropertyProcess,
    kProfilerHubPropertyAgent,
    kProfilerHubPropertyThread,
    kProfilerHubPropertyQueue,
    kProfilerHubPropertyStream,
    kProfilerHubPropertyCounter,

} profiler_hub_property_category_t;

typedef enum profiler_hub_agent_type_t{
    kProfilerHubNotAgent,
    kProfilerHubAgentCPU,
    kProfilerHubAgentGPU,
    kProfilerHubAgentNIC,
} profiler_hub_agent_type_t;

typedef enum profiler_hub_flow_direction_t
{
    kProfilerHubDirectionOutgoing,
    kProfilerHubDirectionIncoming,
} profiler_hub_flow_direction_t;    

// Database type
typedef enum profiler_hub_db_type_t {
    // not supported by profiler hub
    kDbNotSupported, 
    // supported by profiler hub
    kDbSupported,

} profiler_hub_db_type_t;
