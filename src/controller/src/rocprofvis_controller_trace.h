// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_c_interface.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_controller_data.h"
#include "rocprofvis_controller_handle.h"
#include <mutex>
#include <string>

namespace RocProfVis
{
namespace Controller
{

class Future;

class Trace : public Handle
{
public:
    Trace(uint32_t first_prop_index, uint32_t last_prop_index, char const* const filename);

    virtual ~Trace();

    virtual rocprofvis_result_t Init() = 0;

    virtual rocprofvis_result_t Load(Future& future) = 0;

protected:
    uint64_t              m_id;
    rocprofvis_dm_trace_t m_dm_handle;
    std::mutex            m_mutex;
    Data                  m_dm_message;
    uint16_t              m_dm_progress_percent;
    std::string           m_trace_file;

    static void ProgressCallback(rocprofvis_db_filename_t         db_filename,
                                 rocprofvis_db_progress_percent_t progress_percent,
                                 rocprofvis_db_status_t           status,
                                 rocprofvis_db_status_message_t message, void* user_data);

};

}  // namespace Controller
}  // namespace RocProfVis
