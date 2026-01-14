// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_trace.h"
#include "rocprofvis_controller_id.h"
#include "rocprofvis_core.h"
#include "rocprofvis_core_assert.h"

namespace RocProfVis
{
namespace Controller
{

static IdGenerator<Trace> s_trace_id;

Trace::Trace(uint32_t first_prop_index, uint32_t last_prop_index, char const* const filename)
: Handle(first_prop_index, last_prop_index)
, m_id(s_trace_id.GetNextId())
, m_dm_handle(nullptr)
, m_dm_progress_percent(0)
{
    assert (filename && strlen(filename));
    m_trace_file = filename;
}

Trace::~Trace()
{
    if(m_dm_handle)
        rocprofvis_dm_delete_trace(m_dm_handle);
}

void Trace::ProgressCallback(rocprofvis_db_filename_t db_filename,
    rocprofvis_db_progress_percent_t progress_percent,
    rocprofvis_db_status_t status, rocprofvis_db_status_message_t message, void* user_data)
{
    (void) db_filename;
    (void) status;
    ROCPROFVIS_ASSERT(user_data);
    Trace* trace = (Trace*) user_data;
    if(trace != nullptr)
    {
        std::lock_guard lock(trace->m_mutex);
        trace->m_dm_message = Data(message);
        trace->m_dm_progress_percent = progress_percent;
    }
}

}
}
