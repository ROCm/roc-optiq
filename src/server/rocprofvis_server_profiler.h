// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_profiler.h"

#include <cstdint>
#include <functional>
#include <string>

namespace jt
{
class Json;
}

namespace RocProfVis
{
namespace Server
{

/**
 * @brief One profiler run a client started on the server's machine.
 *
 * Runs the tool through the controller's profiler API, exactly as a local
 * launch from the UI does, and forwards its output and state as messages. The
 * client finds the trace path in the output and opens it from the server, so
 * a trace recorded on the GPU machine never has to be copied anywhere.
 */
class ServerProfilerRun
{
public:
    using SendMessageFunction = std::function<bool(jt::Json& message)>;

    ServerProfilerRun(uint64_t run_id, SendMessageFunction send);
    ~ServerProfilerRun();

    ServerProfilerRun(const ServerProfilerRun&)            = delete;
    ServerProfilerRun& operator=(const ServerProfilerRun&) = delete;

    // Starts the run described by a profiler_launch message.
    bool Launch(jt::Json& message, std::string& error);
    void Cancel();
    // Forwards new output and state. False once the run has ended and said so.
    bool Update();

private:
    void SendOutput();
    void SendState(const std::string& error);

    uint64_t                        m_run_id;
    SendMessageFunction             m_send;
    rocprofvis_profiler_config_t*   m_config;
    rocprofvis_profiler_t*          m_profiler;
    rocprofvis_controller_future_t* m_future;
    rocprofvis_profiler_state_t     m_state;
    size_t                          m_sent_output;
};

}  // namespace Server
}  // namespace RocProfVis
