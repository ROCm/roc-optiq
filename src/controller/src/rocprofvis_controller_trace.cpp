// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_trace.h"
#include "rocprofvis_controller_id.h"
#include "rocprofvis_core.h"

namespace RocProfVis
{
namespace Controller
{

static IdGenerator<Trace> s_trace_id;

Trace::Trace(uint32_t first_prop_index, uint32_t last_prop_index, const std::string& filename)
: Handle(first_prop_index, last_prop_index)
, m_id(s_trace_id.GetNextId())
, m_dm_handle(nullptr)
, m_trace_file(filename)
{}

Trace::~Trace()
{
    if(m_dm_handle)
        rocprofvis_dm_delete_trace(m_dm_handle);
}

}
}
