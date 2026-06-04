// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_remote_profiler_session.h"
#include "rocprofvis_appmonitor.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_events.h"
#include "remote/rocprofvis_ssh_uri.h"

#include <spdlog/spdlog.h>

namespace RocProfVis
{
namespace View
{

namespace
{
bool is_terminal_profiler_state(rocprofvis_profiler_state_t state)
{
    return state == kRPVProfilerStateCompleted || state == kRPVProfilerStateFailed ||
           state == kRPVProfilerStateCancelled;
}
}  // namespace

RemoteProfilerSession::RemoteProfilerSession(
    std::shared_ptr<RemoteUri> uri, std::function<void(const std::string&)> on_open_file)
: m_uri(std::move(uri))
, m_on_open_file(std::move(on_open_file))
, m_session(nullptr)
, m_config(nullptr)
, m_profiler(nullptr)
, m_future(nullptr)
, m_profiler_op_id(0)
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
            auto* e = static_cast<RemoteStatusEvent*>(event.get());
            if(m_session && e->GetOperationId() == m_session->GetActiveOperationId())
            {
                OnRemoteStatus(e->GetOperationId(), e->GetStatus(), e->GetResult());
            }
        });

    m_profiler_status_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kProfilerStatusChanged),
        [this](std::shared_ptr<RocEvent> event)
        {
            auto* e = static_cast<ProfilerStatusEvent*>(event.get());
            if(m_profiler_op_id != 0 && e->GetOperationId() == m_profiler_op_id)
            {
                OnProfilerStatus(e->GetOperationId(), e->GetState());
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
    CloseProfiler();
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
    m_config = rocprofvis_profiler_config_alloc();
    if(m_config == nullptr)
    {
        Fail("Failed to allocate profiler config.");
        return false;
    }
    rocprofvis_profiler_config_set_type(m_config, profiler_type);
    rocprofvis_profiler_config_set_profiler_path(m_config, profiler_path.c_str());
    rocprofvis_profiler_config_set_target_executable(m_config, target_executable.c_str());
    rocprofvis_profiler_config_set_target_args(m_config, target_args.c_str());
    rocprofvis_profiler_config_set_output_directory(m_config, output_directory.c_str());
    rocprofvis_profiler_config_set_profiler_args(m_config, profiler_args.c_str());
    for(auto const& kv : env_vars)
    {
        rocprofvis_profiler_config_add_env_var(m_config, kv.first.c_str(), kv.second.c_str());
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
        CloseProfiler();
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
RemoteProfilerSession::OnRemoteStatus(uint64_t /*operation_id*/, uint64_t status,
                                      rocprofvis_result_t /*result*/)
{
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

    // Monitor the profiler op for state transitions. The status callback must
    // read the LIVE controller state (not the cached m_profiler_state, which is
    // only updated by OnProfilerStatus - that would be circular and never
    // change). Output is pulled lazily by the dialog.
    rocprofvis_profiler_t* profiler = m_profiler;
    m_profiler_op_id = AppMonitor::GetInstance()->AddOperation(
        MonitorOperationType::ProfilerSession,
        [profiler]() -> uint64_t
        {
            rocprofvis_profiler_state_t state = kRPVProfilerStateIdle;
            rocprofvis_profiler_get_state(profiler, &state);
            return static_cast<uint64_t>(state);
        },
        [this](uint64_t state) -> std::shared_ptr<RocEvent>
        {
            return std::make_shared<ProfilerStatusEvent>(
                m_profiler_op_id, static_cast<rocprofvis_profiler_state_t>(state));
        },
        [](uint64_t state) -> bool
        {
            return is_terminal_profiler_state(static_cast<rocprofvis_profiler_state_t>(state));
        });
}

void
RemoteProfilerSession::OnProfilerStatus(uint64_t /*operation_id*/, rocprofvis_profiler_state_t state)
{
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

    if(!m_uri || m_uri->GetRemoteResultPathString().empty())
    {
        // Nothing to download; the run completed but produced no fetchable trace.
        m_phase          = Phase::Done;
        m_running        = false;
        m_status_message = "Done (no trace to download).";
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

std::string
RemoteProfilerSession::GetProfilerOutput()
{
    if(m_profiler == nullptr)
    {
        return "";
    }
    uint32_t length = 0;
    rocprofvis_profiler_get_output(m_profiler, nullptr, &length);
    if(length == 0)
    {
        return "";
    }
    std::vector<char>   buffer(length + 1, '\0');
    rocprofvis_result_t result = rocprofvis_profiler_get_output(m_profiler, buffer.data(), &length);
    if(result != kRocProfVisResultSuccess)
    {
        return "";
    }
    buffer[length] = '\0';
    return std::string(buffer.data());
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

void
RemoteProfilerSession::CloseProfiler()
{
    // Stop the status poller (it captures `this`, which is about to go away).
    if(m_profiler_op_id != 0)
    {
        AppMonitor::GetInstance()->RemoveOperation(m_profiler_op_id);
        m_profiler_op_id = 0;
    }

    if(m_profiler == nullptr && m_future == nullptr && m_config == nullptr)
    {
        return;
    }

    // If the remote exec job is still running, transfer the profiler C-objects
    // to the monitor for deferred, non-blocking teardown (cancel signalled,
    // freed once the future resolves). Capture BY VALUE so the closures remain
    // valid after this session is destroyed. Mirrors ProfilerSession::Close.
    if(m_future != nullptr &&
       rocprofvis_controller_future_wait(m_future, 0) == kRocProfVisResultTimeout)
    {
        rocprofvis_profiler_config_t*   config   = m_config;
        rocprofvis_profiler_t*          profiler = m_profiler;
        rocprofvis_controller_future_t* future   = m_future;
        AppMonitor::GetInstance()->AddTeardownOp(
            MonitorOperationType::ProfilerSession,
            future,
            [profiler]() { rocprofvis_profiler_cancel(profiler); },
            [config, profiler, future]()
            {
                rocprofvis_controller_future_free(future);
                rocprofvis_profiler_free(profiler);
                rocprofvis_profiler_config_free(config);
            });
        m_future   = nullptr;
        m_profiler = nullptr;
        m_config   = nullptr;
        return;
    }

    // Future resolved (or none): no worker is using the resources; free now.
    if(m_future != nullptr)
    {
        rocprofvis_controller_future_free(m_future);
        m_future = nullptr;
    }
    if(m_profiler != nullptr)
    {
        rocprofvis_profiler_free(m_profiler);
        m_profiler = nullptr;
    }
    if(m_config != nullptr)
    {
        rocprofvis_profiler_config_free(m_config);
        m_config = nullptr;
    }
}

}  // namespace View
}  // namespace RocProfVis
