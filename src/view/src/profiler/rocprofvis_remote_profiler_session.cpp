// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_remote_profiler_session.h"
#include "rocprofvis_appmonitor.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_events.h"
#include "remote/rocprofvis_ssh_uri.h"

#include <functional>
#include <spdlog/spdlog.h>

namespace RocProfVis
{
namespace View
{

RemoteProfilerSession::RemoteProfilerSession(
    std::shared_ptr<RemoteUri> uri, bool produces_trace)
: m_uri(std::move(uri))
, m_session(nullptr)
, m_produces_trace(produces_trace)
, m_remote_status_token(EventManager::InvalidSubscriptionToken)
, m_profiler_status_token(EventManager::InvalidSubscriptionToken)
, m_phase(Phase::Idle)
, m_running(false)
, m_status_message()
, m_profiler_state(kRPVProfilerStateIdle)
, m_trace_path_result(kRocProfVisResultPending)
{
    m_remote_status_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kRemoteStatusChanged),
        [this](std::shared_ptr<RocEvent> event)
        {
            auto* e = dynamic_cast<RemoteStatusEvent*>(event.get());
            if(e)
            {
                OnRemoteStatus(e->GetOperationId(), e->GetStatus(), e->GetResult());
            }
            else
            {
                spdlog::warn("Received non-RemoteStatusEvent on RemoteProfilerSession subscriber");
            }
        });

    m_profiler_status_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kProfilerStatusChanged),
        [this](std::shared_ptr<RocEvent> event)
        {
            auto* e = dynamic_cast<ProfilerStatusEvent*>(event.get());
            if(e)
            {
                OnProfilerStatus(e->GetOperationId(), e->GetState());
            }
            else
            {
                spdlog::warn("Received non-ProfilerStatusEvent on RemoteProfilerSession subscriber");
            }
        });
}

RemoteProfilerSession::~RemoteProfilerSession()
{
    if(m_remote_status_token != EventManager::InvalidSubscriptionToken)
    {
        EventManager::GetInstance()->Unsubscribe(
            static_cast<int>(RocEvents::kRemoteStatusChanged), m_remote_status_token);
    }
    if(m_profiler_status_token != EventManager::InvalidSubscriptionToken)
    {
        EventManager::GetInstance()->Unsubscribe(
            static_cast<int>(RocEvents::kProfilerStatusChanged), m_profiler_status_token);
    }

    // The remote profiler streams over the SshSession's connection on a worker
    // thread that may still be running. Hand the SshSession into the profiler
    // teardown so it (and its connection) is destroyed only AFTER the profiler
    // future resolves and the worker is joined - otherwise freeing the
    // connection here would race the worker and deadlock the join. shared_ptr
    // because the teardown is stored in a std::function. If there is no session
    // or no in-flight profiler, FreeProfilerObjects runs this immediately.
    if(m_session)
    {
        std::shared_ptr<SshSession> session = std::move(m_session);
        m_extra_teardown = [session]() mutable { session.reset(); };
    }
    Close();
    // m_session is now empty (moved into the teardown above); on any in-flight
    // SSH op, ~SshSession still hands the connection to the AppMonitor for
    // deferred, non-blocking teardown when the shared_ptr is finally released.
}

bool
RemoteProfilerSession::Launch(const ProfilerLaunchSpec& spec)
{
    if(m_running)
    {
        return false;
    }

    m_trace_path_result = kRocProfVisResultPending;

    // Build the profiler config (same fields as local) and tag it as an SSH
    // launch so the controller selects the SshProfilerExecutor.
    if(!BuildConfig(spec))
    {
        Fail("Failed to allocate profiler config.");
        return false;
    }
    if(m_uri)
    {
        rocprofvis_profiler_config_set_connection_ssh(
            m_config,
            m_uri->GetRemoteHostString().c_str(),
            m_uri->GetRemoteUserString().c_str(),
            m_uri->GetRemotePortInt(),
            m_uri->GetRemoteIdentityFileString().c_str(),
            spec.output_directory.c_str());
    }

    m_profiler = rocprofvis_profiler_alloc();
    if(m_profiler == nullptr)
    {
        Fail("Failed to allocate profiler session.");
        Close();
        return false;
    }

    // Create the SSH session and start the connect phase. The profiler launch
    // happens after authentication completes (the connection must be live).
    m_session = std::make_unique<SshSession>(m_uri);
    if(!m_session->IsConnected())
    {
        Fail("Failed to create SSH session.");
        return false;
    }

    m_status_message = "Connecting...";
    m_phase          = Phase::Connecting;
    m_running        = true;
    m_profiler_state = kRPVProfilerStateIdle;

    if(m_session->StartConnect() == 0)
    {
        Fail("SSH connection could not be started.");
        return false;
    }
    return true;
}

void
RemoteProfilerSession::OnRemoteStatus(uint64_t operation_id, uint64_t status,
                                      rocprofvis_result_t /*result*/)
{
    // Once the workflow has failed, ignore further status so a late/cancel event
    // cannot overwrite the original failure reason or advance the phase machine.
    if(m_phase == Phase::Failed)
    {
        return;
    }
    if(!m_session || operation_id != m_session->GetActiveOperationId())
    {
        return;
    }

    if(status == kRPVControllerSshFailed)
    {
        switch(m_phase)
        {
            case Phase::Connecting:     Fail("SSH connection failed."); break;
            case Phase::Authenticating: Fail("SSH authentication failed."); break;
            default: Fail("SSH operation failed."); break;
        }
        return;
    }

    if(status != kRPVControllerSshCompleted)
    {
        return;  // intermediate status (auth prompt, download progress, etc.)
    }

    switch(m_phase)
    {
        case Phase::Connecting:
            m_status_message = "Authenticating...";
            m_phase          = Phase::Authenticating;
            if(m_session->StartAuthenticate() == 0)
            {
                Fail("SSH authentication could not be started.");
            }
            break;

        case Phase::Authenticating:
            // Connection is live + authenticated and the SshSession is now idle;
            // launch the remote profiler on it.
            StartProfiler();
            break;

        default:
            break;
    }
}

void
RemoteProfilerSession::StartProfiler()
{
    m_status_message = "Profiling on remote host...";
    m_phase          = Phase::Profiling;

    m_future = rocprofvis_controller_future_alloc();
    if(m_future == nullptr)
    {
        Fail("Failed to allocate profiler future.");
        return;
    }

    rocprofvis_result_t result = rocprofvis_profiler_launch_remote_async(
        m_profiler, m_config, m_session->GetConnectionHandle(), m_future);
    if(result != kRocProfVisResultSuccess)
    {
        Fail("Failed to launch remote profiler.");
        return;
    }

    // Monitor the profiler op for state transitions (shared base poller reads
    // the LIVE controller state and emits ProfilerStatusEvents). Output is
    // pulled lazily by the dialog.
    RegisterProfilerMonitor();
}

void
RemoteProfilerSession::OnProfilerStatus(uint64_t operation_id, rocprofvis_profiler_state_t state)
{
    // Once failed, ignore further profiler status (e.g. the Cancelled event that
    // Fail() itself triggers) so the original failure reason is preserved.
    if(m_phase == Phase::Failed)
    {
        return;
    }
    if(m_profiler_op_id == 0 || operation_id != m_profiler_op_id)
    {
        return;
    }

    m_profiler_state = state;

    // Completed is finished by Update(), once the worker has returned.
    if(state == kRPVProfilerStateFailed)
    {
        Fail("Remote profiler failed.");
    }
    else if(state == kRPVProfilerStateCancelled)
    {
        Fail("Remote profiler cancelled.");
    }
}

void
RemoteProfilerSession::Update()
{
    // The profiler reports Completed before its worker returns; the connection
    // and the output are only final once the future has resolved.
    if(m_phase == Phase::Profiling && m_profiler_state == kRPVProfilerStateCompleted &&
       m_future != nullptr &&
       rocprofvis_controller_future_wait(m_future, 0) != kRocProfVisResultTimeout)
    {
        FinishProfiling();
    }
}

std::unique_ptr<SshSession>
RemoteProfilerSession::TakeSession()
{
    std::unique_ptr<SshSession> session;
    if(m_session && m_phase == Phase::Done)
    {
        // The worker has finished with the connection; free the profiler objects
        // before it changes hands so nothing else refers to it.
        Close();
        session = std::move(m_session);
    }
    return session;
}

void
RemoteProfilerSession::FinishProfiling()
{
    // The monitor reaps the (now terminal) profiler op on its own; drop our
    // reference so further status callbacks are ignored.
    m_profiler_op_id = 0;

    if(!m_uri)
    {
        Fail("No remote URI; cannot download trace.");
        return;
    }

    // The controller scraped the remote path as the profiler printed it. It is
    // read even when the trace is not auto-opened, so Open Trace can download it
    // later. A run not meant to write a trace has none to find. A path that
    // cannot be read leaves a completed run completed, as for a local run: the
    // trace is opened by hand. The URI outlives a run, so a missing path must
    // clear it, or the next download would fetch the previous run's file.
    std::string path;
    m_trace_path_result =
        m_produces_trace ? GetTraceFilePath(path) : kRocProfVisResultNotSupported;
    if(m_trace_path_result == kRocProfVisResultSuccess && !path.empty())
    {
        m_uri->GetRemoteResultPath() = std::move(path);
    }
    else
    {
        m_uri->GetRemoteResultPath().clear();
        if(m_trace_path_result == kRocProfVisResultSuccess)
        {
            m_trace_path_result = kRocProfVisResultNotAvailable;
        }
    }
    m_phase          = Phase::Done;
    m_running        = false;
    m_status_message = "Done.";
}

void
RemoteProfilerSession::Fail(const std::string& message)
{
    spdlog::warn("[remote-profiler] {}", message);
    m_status_message = message;
    m_phase          = Phase::Failed;
    m_running        = false;

    // Stop a still-running remote profiler so it doesn't keep executing on the
    // remote host after the UI shows Failed. Set the phase first (above) so the
    // Cancelled status this triggers is ignored by OnProfilerStatus's
    // Phase::Failed guard and does not overwrite the original failure reason.
    // In-flight SSH ops and the connection are torn down when this session is
    // destroyed (deferred, non-blocking, via the base + SshSession teardown).
    if(GetState() == kRPVProfilerStateRunning)
    {
        Cancel();
    }

    // A failure after the profiler reported Completed must not be reported as
    // completed; only a cancel keeps its own state.
    if(m_profiler_state != kRPVProfilerStateCancelled)
    {
        m_profiler_state = kRPVProfilerStateFailed;
    }
}

}  // namespace View
}  // namespace RocProfVis
