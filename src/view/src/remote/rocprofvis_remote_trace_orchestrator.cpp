// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_remote_trace_orchestrator.h"
#include "rocprofvis_ssh_uri.h"
#include "rocprofvis_events.h"
#include "rocprofvis_controller_enums.h"

#include <spdlog/spdlog.h>

namespace RocProfVis
{
namespace View
{

RemoteTraceOrchestrator::RemoteTraceOrchestrator(
    std::shared_ptr<RemoteUri> uri, std::function<void(const std::string&)> on_open_file)
: m_uri(std::move(uri))
, m_on_open_file(std::move(on_open_file))
, m_on_search_results(nullptr)
, m_search_command()
, m_session(nullptr)
, m_status_token(EventManager::InvalidSubscriptionToken)
, m_phase(Phase::Idle)
, m_running(false)
, m_authenticated(false)
, m_status_message()
{
    m_status_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kRemoteStatusChanged),
        [this](std::shared_ptr<RocEvent> event)
        {
            auto* status_event = dynamic_cast<RemoteStatusEvent*>(event.get());
            if(status_event == nullptr)
            {
                spdlog::warn("Received non-RemoteStatusEvent on RemoteTraceOrchestrator "
                             "subscriber");
                return;
            }
            // Only react to events from this orchestrator's session's current
            // in-flight phase.
            if(m_session &&
               status_event->GetOperationId() == m_session->GetActiveOperationId())
            {
                OnRemoteStatus(status_event->GetStatus(), status_event->GetResult());
            }
        });
}

RemoteTraceOrchestrator::~RemoteTraceOrchestrator()
{
    if(m_status_token != EventManager::InvalidSubscriptionToken)
    {
        EventManager::GetInstance()->Unsubscribe(
            static_cast<int>(RocEvents::kRemoteStatusChanged), m_status_token);
    }
}

bool
RemoteTraceOrchestrator::Start()
{
    if(m_running)
    {
        return false;
    }

    m_session = std::make_unique<SshSession>(m_uri);
    if(!m_session->IsConnected())
    {
        Fail("Failed to create SSH session.");
        return false;
    }

    m_authenticated  = false;
    m_status_message = "Connecting...";
    m_phase          = Phase::Connecting;
    m_running        = true;
    m_task           = Phase::Executing;

    if(m_session->StartConnect() == 0)
    {
        Fail("SSH connection could not be started.");
        return false;
    }
    return true;
}

bool
RemoteTraceOrchestrator::StartBrowsing()
{
    if(m_running)
    {
        return false;
    }

    m_session = std::make_unique<SshSession>(m_uri);
    if(!m_session->IsConnected())
    {
        Fail("Failed to create SSH session.");
        return false;
    }

    m_authenticated  = false;
    m_status_message = "Connecting...";
    m_phase          = Phase::Connecting;
    m_running        = true;
    m_task           = Phase::Browsing;

    if(m_session->StartConnect() == 0)
    {
        Fail("SSH connection could not be started.");
        return false;
    }
    return true;
}

bool
RemoteTraceOrchestrator::BrowsePath()
{
    if(m_running)
    {
        return false;
    }

    // Reuse an already connected + authenticated session: skip connect / auth
    // and browse directly on the live connection. This is the fast path for
    // folder-to-folder navigation.
    if(m_session && m_authenticated && m_session->IsConnected())
    {
        m_running = true;
        m_task    = Phase::Browsing;
        Browse();
        return m_phase != Phase::Failed;
    }

    // No live authenticated session yet (first browse, or the previous one was
    // torn down): run the full connect -> authenticate -> browse pipeline.
    return StartBrowsing();
}

bool
RemoteTraceOrchestrator::SearchPath(const std::string& command)
{
    if(m_running)
    {
        return false;
    }

    m_search_command = command;

    // Reuse a live authenticated session, mirroring the BrowsePath() fast path.
    if(m_session && m_authenticated && m_session->IsConnected())
    {
        m_running = true;
        m_task    = Phase::Searching;
        RunSearch();
        return m_phase != Phase::Failed;
    }

    // Otherwise connect + authenticate first, then search.
    m_session = std::make_unique<SshSession>(m_uri);
    if(!m_session->IsConnected())
    {
        Fail("Failed to create SSH session.");
        return false;
    }

    m_authenticated  = false;
    m_status_message = "Connecting...";
    m_phase          = Phase::Connecting;
    m_running        = true;
    m_task           = Phase::Searching;

    if(m_session->StartConnect() == 0)
    {
        Fail("SSH connection could not be started.");
        return false;
    }
    return true;
}

void
RemoteTraceOrchestrator::OnRemoteStatus(uint64_t status, rocprofvis_result_t result)
{
    if(status == kRPVControllerSshFailed)
    {
        switch(m_phase)
        {
            case Phase::Connecting:    Fail("SSH connection failed."); break;
            case Phase::Authenticating: Fail("SSH authentication failed."); break;
            case Phase::Executing:
                Fail("CLI execution failed. Check remote command syntax and try again.");
                break;
            case Phase::Downloading:
                Fail("Result database download failed. Check profiler result path and try again.");
                break;
            case Phase::Browsing:
                Fail("Remote filesystem browsing failed.");
                break;
            case Phase::Searching:
                Fail("Remote search failed.");
                break;
            default: Fail("SSH operation failed."); break;
        }
        return;
    }

    if(status != kRPVControllerSshCompleted)
    {
        // Intermediate status (auth prompt, stdout, download progress). The
        // prompt / progress UI consumes session snapshots directly; nothing to
        // advance here.
        return;
    }

    switch(m_phase)
    {
        case Phase::Connecting:    AdvanceAfterConnect(); break;
        case Phase::Authenticating: AdvanceAfterAuthenticate(); break;
        case Phase::Executing:     AdvanceAfterExecute(); break;
        case Phase::Downloading:   AdvanceAfterDownload(); break;
        case Phase::Browsing:   AdvanceAfterBrowsing(); break;
        case Phase::Searching:  AdvanceAfterSearch(); break;
        default: break;
    }
}

void
RemoteTraceOrchestrator::AdvanceAfterConnect()
{
    m_status_message = "Authenticating...";
    m_phase          = Phase::Authenticating;
    if(m_session->StartAuthenticate() == 0)
    {
        Fail("SSH authentication could not be started.");
    }
}

void
RemoteTraceOrchestrator::AdvanceAfterAuthenticate()
{
    // Authentication succeeded; the session's connection is now live and can be
    // reused by a subsequent BrowsePath() without reconnecting.
    m_authenticated = true;

    if (m_task == Phase::Executing)
    { 
        if(m_uri && !m_uri->GetRemoteCommandLineString().empty())
        {
            m_status_message =
                std::string("Executing command (") + m_uri->GetRemoteCommandLineString() + ")";
            m_phase = Phase::Executing;
            if(m_session->StartExecute() == 0)
            {
                Fail("CLI execution could not be started.");
            }
            return;
        }
        AdvanceAfterExecute();
    }
    else if (m_task == Phase::Browsing)
    {
        Browse();
    }
    else if (m_task == Phase::Searching)
    {
        RunSearch();
    }
}

void
RemoteTraceOrchestrator::AdvanceAfterExecute()
{
    if(m_uri && !m_uri->GetRemoteResultPathString().empty())
    {
        m_status_message =
            std::string("Downloading (") + m_uri->GetRemoteResultPathString() + ")";
        m_phase = Phase::Downloading;
        if(m_session->StartDownload() == 0)
        {
            Fail("Result database download could not be started.");
        }
        return;
    }
    // Nothing to download; the workflow is complete.
    m_phase          = Phase::Done;
    m_running        = false;
    m_status_message = "Done.";
}

void
RemoteTraceOrchestrator::AdvanceAfterDownload()
{
    m_phase          = Phase::Done;
    m_running        = false;
    m_status_message = "Done.";

    if(m_uri && m_on_open_file)
    {
        m_on_open_file(m_uri->GetLocalResultPathString());
    }
}

void
RemoteTraceOrchestrator::AdvanceAfterBrowsing()
{
    m_phase          = Phase::Done;
    m_running        = false;
    m_status_message = "Done.";

    if(m_uri && m_on_open_file)
    {
        m_on_open_file(m_uri->GetRemoteBrowsingPathString());
    }
}

void
RemoteTraceOrchestrator::Browse()
{
    if(m_uri && !m_uri->GetRemoteBrowsingPathString().empty())
    {
        m_status_message =
            std::string("Browsing (") + m_uri->GetRemoteBrowsingPathString() + ")";
        m_phase = Phase::Browsing;
        if(m_session->StartBrowsing(m_uri->GetRemoteBrowsingPathString().c_str()) == 0)
        {
            Fail("Remote filesystem browsing cannot be started.");
        }
        return;
    }
    // Nothing to download; the workflow is complete.
    m_phase          = Phase::Done;
    m_running        = false;
    m_status_message = "Done.";
}

void
RemoteTraceOrchestrator::RunSearch()
{
    if(m_search_command.empty())
    {
        m_phase          = Phase::Done;
        m_running        = false;
        m_status_message = "Done.";
        return;
    }

    m_status_message = "Searching...";
    m_phase          = Phase::Searching;
    if(m_session->StartExecute(m_search_command.c_str()) == 0)
    {
        Fail("Remote search could not be started.");
    }
}

void
RemoteTraceOrchestrator::AdvanceAfterSearch()
{
    // Capture the command's stdout and reset the execution-output latch so the
    // owning dialog's stdout popup is not triggered by the search command.
    std::string output;
    if(m_session)
    {
        output = m_session->GetExecutionOutput()->Get().text;
        m_session->GetExecutionOutput()->ClearUpdated();
    }

    m_phase          = Phase::Done;
    m_running        = false;
    m_status_message = "Done.";

    if(m_on_search_results)
    {
        m_on_search_results(output);
    }
}

void
RemoteTraceOrchestrator::Fail(const std::string& message)
{
    spdlog::warn("[remote-trace] {}", message);
    m_status_message = message;
    m_phase          = Phase::Failed;
    m_running        = false;
    // The live connection (if any) can no longer be trusted; force the next
    // BrowsePath() to build a fresh session rather than reusing a dead one.
    m_authenticated  = false;
}

}  // namespace View
}  // namespace RocProfVis
