// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_profiler_launch_orchestrator.h"
#include "rocprofvis_appwindow.h"
#include "remote/rocprofvis_ssh_uri.h"

#include <spdlog/spdlog.h>
#include <utility>

namespace RocProfVis
{
namespace View
{

ProfilerLaunchOrchestrator::ProfilerLaunchOrchestrator(AppWindow* app_window)
: m_app_window(app_window)
, m_profiler_session()
, m_remote_session(nullptr)
, m_profiler_status_token(EventManager::InvalidSubscriptionToken)
, m_remote_uri(nullptr)
, m_profiler_state(kRPVProfilerStateIdle)
, m_is_running(false)
, m_is_remote(false)
, m_auto_load_trace(true)
, m_parse_trace(nullptr)
, m_raw_output()
, m_output_dirty(false)
, m_trace_path()
, m_launch_error()
{
    // Listen for profiler state transitions surfaced by the AppMonitor. Filter
    // to events belonging to this orchestrator's LOCAL session by operation id
    // (the remote session manages its own profiler op internally).
    m_profiler_status_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kProfilerStatusChanged),
        [this](std::shared_ptr<RocEvent> event)
        {
            auto* status_event = static_cast<ProfilerStatusEvent*>(event.get());
            if(status_event->GetOperationId() == m_profiler_session.GetOperationId())
            {
                OnProfilerStateChanged(status_event->GetState());
            }
        });
}

ProfilerLaunchOrchestrator::~ProfilerLaunchOrchestrator()
{
    // Destroy the remote session (which owns the monitored SshSession) before
    // releasing our shared RemoteUri reference. The orchestrator holds its own
    // shared_ptr, so the deferred SSH teardown stays valid regardless of the
    // owning dialog's lifetime.
    m_remote_session.reset();

    if(m_profiler_status_token != EventManager::InvalidSubscriptionToken)
    {
        EventManager::GetInstance()->Unsubscribe(
            static_cast<int>(RocEvents::kProfilerStatusChanged), m_profiler_status_token);
    }
}

bool ProfilerLaunchOrchestrator::Launch(const LaunchRequest& request)
{
    // Fresh run: drop any prior remote session and reset normalized state /
    // output so stale data from a previous launch does not leak through.
    m_remote_session.reset();
    m_raw_output.clear();
    m_output_dirty   = false;
    m_trace_path.clear();
    m_launch_error.clear();
    m_profiler_state = kRPVProfilerStateIdle;
    m_is_running     = false;

    m_is_remote       = request.is_remote;
    m_auto_load_trace = request.auto_load_trace;
    m_parse_trace     = request.parse_trace;
    m_remote_uri      = request.remote_uri;

    return request.is_remote ? LaunchRemote(request) : LaunchLocal(request);
}

bool ProfilerLaunchOrchestrator::LaunchLocal(const LaunchRequest& request)
{
    bool success = m_profiler_session.Launch(
        request.profiler_type,
        request.profiler_path,
        request.target_executable,
        request.target_args,
        request.output_directory,
        request.profiler_args,
        request.env_vars);

    if(!success)
    {
        m_launch_error   = "Failed to launch profiler";
        m_profiler_state = kRPVProfilerStateFailed;
        return false;
    }

    m_is_running     = true;
    m_profiler_state = kRPVProfilerStateRunning;
    return true;
}

bool ProfilerLaunchOrchestrator::LaunchRemote(const LaunchRequest& request)
{
    if(!request.remote_uri)
    {
        m_launch_error   = "No SSH connection configured.";
        m_profiler_state = kRPVProfilerStateFailed;
        return false;
    }

    // Spawn a RemoteProfilerSession that drives connect -> auth -> remote launch
    // -> download -> open. Auth prompts / download progress surface via the SSH
    // popups rendered by the dialog.
    auto parse_trace = request.parse_trace;
    m_remote_session = std::make_unique<RemoteProfilerSession>(
        request.remote_uri,
        [this](const std::string& local_path)
        {
            if(m_app_window && !local_path.empty())
            {
                m_app_window->OpenFile(local_path);
            }
        },
        [parse_trace](const std::string& profiler_stdout) -> std::string
        {
            return parse_trace ? parse_trace(profiler_stdout) : std::string();
        });

    bool success = m_remote_session->Launch(
        request.profiler_type,
        request.profiler_path,
        request.target_executable,
        request.target_args,
        request.output_directory,
        request.profiler_args,
        request.env_vars);

    if(!success)
    {
        m_launch_error = "Failed to start remote profiler: " +
                         (m_remote_session ? m_remote_session->GetStatusMessage() : std::string());
        m_profiler_state = kRPVProfilerStateFailed;
        m_remote_session.reset();
        return false;
    }

    m_is_running     = true;
    m_profiler_state = kRPVProfilerStateRunning;
    return true;
}

void ProfilerLaunchOrchestrator::Cancel()
{
    if(m_remote_session)
    {
        // Tearing down the remote session cancels the in-flight phase (SSH or
        // remote profiler) via the AppMonitor's deferred teardown.
        m_remote_session.reset();
        m_profiler_state = kRPVProfilerStateCancelled;
        m_is_running     = false;
        return;
    }

    if(m_profiler_session.Cancel())
    {
        m_profiler_state = kRPVProfilerStateCancelled;
        m_is_running     = false;
    }
    else
    {
        m_launch_error = "Failed to cancel profiler";
    }
}

void ProfilerLaunchOrchestrator::Close()
{
    m_remote_session.reset();
    m_profiler_session.Close();
    m_is_running     = false;
    m_profiler_state = kRPVProfilerStateIdle;
}

void ProfilerLaunchOrchestrator::Update()
{
    if(m_remote_session)
    {
        // The remote session owns the connect -> auth -> profile -> download
        // phase machine. The orchestrator stays "running" until the whole
        // workflow finishes, then settles to the remote profiler's terminal
        // result. The transient Idle state is never mirrored back (it would
        // regress the badge to [Idle]).
        rocprofvis_profiler_state_t remote_state = m_remote_session->GetProfilerState();

        std::string remote_out = m_remote_session->GetOutput();
        if(!remote_out.empty() && remote_out != m_raw_output)
        {
            m_raw_output   = std::move(remote_out);
            m_output_dirty = true;
        }

        if(m_remote_session->IsRunning())
        {
            m_is_running     = true;
            m_profiler_state = kRPVProfilerStateRunning;
        }
        else
        {
            m_is_running = false;
            if(remote_state == kRPVProfilerStateCompleted)
            {
                m_profiler_state = kRPVProfilerStateCompleted;
                // The remote session resolved + downloaded the trace; surface the
                // local cached copy path for the view's "Trace file:" line.
                if(m_remote_uri && m_trace_path.empty())
                {
                    m_trace_path = m_remote_uri->GetLocalResultPathString();
                }
            }
            else if(remote_state == kRPVProfilerStateCancelled)
            {
                m_profiler_state = kRPVProfilerStateCancelled;
            }
            else
            {
                m_profiler_state = kRPVProfilerStateFailed;
            }
        }
        return;
    }

    // Local: profiler state transitions arrive via kProfilerStatusChanged events
    // (see OnProfilerStateChanged). Always pull output while there's an active
    // session, even after completion (output may arrive asynchronously).
    if(m_profiler_state == kRPVProfilerStateRunning ||
       m_profiler_state == kRPVProfilerStateCompleted ||
       m_profiler_state == kRPVProfilerStateFailed)
    {
        std::string new_raw = m_profiler_session.GetOutput();
        if(new_raw != m_raw_output)
        {
            m_raw_output   = std::move(new_raw);
            m_output_dirty = true;
        }
    }
}

void ProfilerLaunchOrchestrator::OnProfilerStateChanged(rocprofvis_profiler_state_t new_state)
{
    if(new_state == m_profiler_state)
    {
        return;
    }

    m_profiler_state = new_state;

    // Remote trace download + auto-load is owned by the RemoteProfilerSession;
    // local trace resolution happens here.
    if(m_remote_session)
    {
        if(new_state == kRPVProfilerStateFailed)
        {
            m_is_running = false;
        }
        return;
    }

    if(new_state == kRPVProfilerStateCompleted)
    {
        m_is_running = false;
        ResolveLocalTracePath();
        if(!m_trace_path.empty() && m_auto_load_trace && m_app_window)
        {
            m_app_window->OpenFile(m_trace_path);
        }
    }
    else if(new_state == kRPVProfilerStateFailed)
    {
        m_is_running = false;
    }
    else if(new_state == kRPVProfilerStateCancelled)
    {
        m_is_running = false;
    }
}

void ProfilerLaunchOrchestrator::ResolveLocalTracePath()
{
    // Prefer the path the profiler actually reported in its output (the backend
    // scraper). Fall back to the controller's newest-".db" filesystem scan if
    // the parser can't find one.
    m_trace_path = m_parse_trace ? m_parse_trace(m_raw_output) : std::string();
    if(m_trace_path.empty())
    {
        m_trace_path = m_profiler_session.GetTracePath();
        spdlog::warn("ProfilerLaunchOrchestrator: backend failed to parse trace path from "
                     "output; falling back to filesystem scan result '{}'",
                     m_trace_path);
    }
}

int32_t ProfilerLaunchOrchestrator::GetExitCode() const
{
    return m_profiler_session.GetExitCode();
}

SshSession* ProfilerLaunchOrchestrator::GetRemoteSshSession() const
{
    return m_remote_session ? m_remote_session->GetSession() : nullptr;
}

bool ProfilerLaunchOrchestrator::IsRemoteDownloading() const
{
    return m_remote_session && m_remote_session->IsDownloading();
}

bool ProfilerLaunchOrchestrator::ConsumeOutputDirty()
{
    bool dirty     = m_output_dirty;
    m_output_dirty = false;
    return dirty;
}

bool ProfilerLaunchOrchestrator::GetRemotePhaseBadge(std::string&        out_label,
                                                     ConsoleStatusLevel& out_level,
                                                     std::string&        out_detail) const
{
    if(!m_remote_session)
    {
        return false;
    }

    // The badge reflects the workflow phase, not just the profiler state, so the
    // user sees connect/auth/profile/download progress. The session's status
    // message (e.g. "Downloading (/path)") is the detail.
    out_detail = m_remote_session->GetStatusMessage();
    switch(m_remote_session->GetPhase())
    {
        case RemoteProfilerSession::Phase::Connecting:
            out_label = "Connecting";
            out_level = ConsoleStatusLevel::kRunning;
            break;
        case RemoteProfilerSession::Phase::Authenticating:
            out_label = "Authenticating";
            out_level = ConsoleStatusLevel::kRunning;
            break;
        case RemoteProfilerSession::Phase::Profiling:
            out_label = "Profiling";
            out_level = ConsoleStatusLevel::kRunning;
            break;
        case RemoteProfilerSession::Phase::Downloading:
            out_label = "Downloading";
            out_level = ConsoleStatusLevel::kRunning;
            break;
        case RemoteProfilerSession::Phase::Done:
            out_label = "Completed";
            out_level = ConsoleStatusLevel::kSuccess;
            break;
        case RemoteProfilerSession::Phase::Failed:
            out_label = "Failed";
            out_level = ConsoleStatusLevel::kError;
            break;
        case RemoteProfilerSession::Phase::Idle:
        default:
            out_label = "Idle";
            out_level = ConsoleStatusLevel::kIdle;
            break;
    }
    return true;
}

}  // namespace View
}  // namespace RocProfVis
