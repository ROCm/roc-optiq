// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#ifdef ROCPROFVIS_PROFILER_HUB_ENABLED

#include "rocprofvis_c_interface_types.h"
#include <cstddef>
#include <cstdint>
#include <profiler-hub/c/profiler_hub.h>

namespace RocProfVis
{
namespace DataModel
{

class ProfilerHub;

/** Writes profiler-hub event details into an extended-data container. */
class ProfilerHubExtData
{
public:
    ProfilerHubExtData(ProfilerHub* hub, rocprofvis_dm_extdata_t container,
                       rocprofvis_dm_event_operation_t operation, uint32_t db_instance,
                       uint64_t origin_timestamp);

    bool AddTrack(uint32_t track_id, const ph_event_data_t& event);
    bool AddRegion(const ph_region_details_t& region);
    bool AddKernelDispatch(const ph_kernel_dispatch_details_t& kernel);
    bool AddMemoryCopy(const ph_memory_copy_details_t& copy);
    bool AddMemoryAlloc(const ph_memory_alloc_details_t& alloc);

private:
    struct uint_field_t
    {
        const char* name;
        uint64_t    value;
    };
    struct text_field_t
    {
        const char* name;
        const char* value;
    };
    struct optional_field_t
    {
        const char*          name;
        ph_optional_uint64_t value;
    };

    bool AddInt(const char* category, const char* name, uint64_t value);
    bool AddText(const char* category, const char* name, const char* value);
    bool AddOptional(const char* category, const char* name, ph_optional_uint64_t value);
    bool AddUints(const char* category, const uint_field_t* fields, size_t count);
    bool AddTexts(const char* category, const text_field_t* fields, size_t count);
    bool AddOptionals(const char* category, const optional_field_t* fields, size_t count);

    bool AddEventData(const ph_event_data_t& event);
    bool AddNode(const ph_detail_node_t& node);
    bool AddProcess(const ph_detail_process_t& process);
    bool AddThread(const ph_detail_thread_t& thread);
    bool AddAgent(const char* category, const ph_detail_agent_t& agent);
    bool AddStream(const ph_detail_stream_t& stream);
    bool AddQueue(const ph_detail_queue_t& queue);
    bool AddKernelSymbol(const ph_detail_kernel_symbol_t& symbol);
    bool AddCodeObject(const ph_detail_code_object_t& object);

    ProfilerHub*                    m_hub;
    rocprofvis_dm_extdata_t         m_container;
    rocprofvis_dm_event_operation_t m_operation;
    uint32_t                        m_db_instance;
    uint64_t                        m_origin;
};

}  // namespace DataModel
}  // namespace RocProfVis

#endif  // ROCPROFVIS_PROFILER_HUB_ENABLED
