// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_profiler_session_base.h"

#include <string>
#include <utility>
#include <vector>

namespace RocProfVis
{
namespace View
{

// View-layer owner of a single local profiler launch. Reuses the shared
// profiler C-object lifecycle from ProfilerSessionBase and launches the process
// directly via rocprofvis_profiler_launch_async. Lives independently of any
// open trace.
class ProfilerSession : public ProfilerSessionBase
{
public:
    ProfilerSession()           = default;
    ~ProfilerSession() override = default;

    // Launches a local profiler process asynchronously. On success the session
    // is registered with the AppMonitor and ProfilerStatusEvents are emitted as
    // the state changes. Any previous launch on this session is closed first.
    bool Launch(const ProfilerLaunchSpec& spec) override;
};

}  // namespace View
}  // namespace RocProfVis
