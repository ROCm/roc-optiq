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
    std::shared_ptr<RemoteUri> uri, std::function<void(const std::string&)> on_open_file,
    std::function<std::string(const std::string&)> parse_trace_path)
: m_uri(std::move(uri))
, m_on_open_file(std::move(on_open_file))
, m_parse_trace_path(std::move(parse_trace_path))
, m_session(nullptr)
, m_remote_status_token(EventManager::InvalidSubscriptionToken)
, m_profiler_status_token(EventManager::InvalidSubscriptionToken)
, m_phase(Phase::Idle)
, m_running(false)
, m_status_message()
, m_profiler_state(kRPVProfilerStateIdle)
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
    Close();
    // m_session's destructor hands any in-flight SSH op (and the connection) to
    // the AppMonitor for deferred, non-blocking teardown.
}

bool
RemoteProfilerSession::Launch(rocprofvis_profiler_type_t profiler_type,
                              const std::string&         profiler_path,
                              const std::string&         target_executable,
                              const std::string&         target_args,
                              const std::string&         output_directory,
                              const std::string&         profiler_args,
                              const std::vector<std::pair<std::string, std::string>>& env_vars)
{
    if(m_running)
    {
        return false;
    }

    // Build the profiler config (same fields as local) and tag it as an SSH
    // launch so the controller selects the SshProfilerExecutor.
    if(!BuildConfig(profiler_type, profiler_path, target_executable, target_args,
                    output_directory, profiler_args, env_vars))
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
            output_directory.c_str());
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
            case Phase::Downloading:
                Fail("Trace download failed. Check the remote output path.");
                break;
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

        case Phase::Downloading:
            m_phase          = Phase::Done;
            m_running        = false;
            m_status_message = "Done.";
            if(m_uri && m_on_open_file)
            {
                m_on_open_file(m_uri->GetLocalResultPathString());
            }
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
    if(m_profiler_op_id == 0 || operation_id != m_profiler_op_id)
    {
        return;
    }

    m_profiler_state = state;

    if(state == kRPVProfilerStateCompleted)
    {
        // Profiler done and the exec channel is closed; the connection is idle
        // again, so it is safe to start the download on the SshSession.
        StartDownload();
    }
    else if(state == kRPVProfilerStateFailed)
    {
        Fail("Remote profiler failed.");
    }
    else if(state == kRPVProfilerStateCancelled)
    {
        Fail("Remote profiler cancelled.");
    }
}

void
RemoteProfilerSession::StartDownload()
{
    // The monitor reaps the (now terminal) profiler op on its own; drop our
    // reference so further status callbacks are ignored.
    m_profiler_op_id = 0;

    if(!m_uri)
    {
        Fail("No remote URI; cannot download trace.");
        return;
    }

    // Deduce the remote trace path from the profiler's captured stdout (the
    // profiler reports the file it produced). This replaces the old manual
    // "Remote output database" field.
    if(m_parse_trace_path)
    {
        std::string parsed = m_parse_trace_path(GetOutput());
        if(!parsed.empty())
        {
            m_uri->GetRemoteResultPath() = parsed;
        }
    }

    if(m_uri->GetRemoteResultPathString().empty())
    {
        // The run completed but we could not determine a trace path to fetch.
        Fail("Could not determine remote trace path from profiler output.");
        return;
    }

    m_status_message =
        std::string("Downloading (") + m_uri->GetRemoteResultPathString() + ")";
    m_phase = Phase::Downloading;
    if(m_session->StartDownload() == 0)
    {
        Fail("Trace download could not be started.");
    }
}

void
RemoteProfilerSession::Fail(const std::string& message)
{
    spdlog::warn("[remote-profiler] {}", message);
    m_status_message = message;
    m_phase          = Phase::Failed;
    m_running        = false;
    if(m_profiler_state == kRPVProfilerStateRunning || m_profiler_state == kRPVProfilerStateIdle)
    {
        m_profiler_state = kRPVProfilerStateFailed;
    }
}

}  // namespace View
}  // namespace RocProfVis
