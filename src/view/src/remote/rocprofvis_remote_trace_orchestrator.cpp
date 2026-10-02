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
    std::shared_ptr<RemoteUri> uri, std::function<void(const std::string&)> on_result)
: m_uri(std::move(uri))
, m_on_result(std::move(on_result))
, m_session(nullptr)
, m_status_token(EventManager::InvalidSubscriptionToken)
, m_phase(Phase::Idle)
, m_task(Task::kConnect)
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
                OnRemoteStatus(status_event->GetStatus());
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
RemoteTraceOrchestrator::Connect()
{
    return Run(Task::kConnect);
}

bool
RemoteTraceOrchestrator::BrowsePath()
{
    return Run(Task::kBrowse);
}

bool
RemoteTraceOrchestrator::DownloadPath()
{
    return Run(Task::kDownload);
}

void
RemoteTraceOrchestrator::SetOnResult(std::function<void(const std::string&)> on_result)
{
    m_on_result = std::move(on_result);
}

bool
RemoteTraceOrchestrator::Run(Task task)
{
    if(m_running)
    {
        return false;
    }
    m_task    = task;
    m_running = true;

    if(m_session && m_authenticated && m_session->IsConnected())
    {
        RunTask();
        return m_phase != Phase::Failed;
    }

    m_session       = std::make_unique<SshSession>(m_uri);
    m_authenticated = false;
    if(!m_session->IsConnected())
    {
        Fail("Could not create the SSH connection.");
        return false;
    }
    m_status_message = "Connecting...";
    m_phase          = Phase::Connecting;
    if(m_session->StartConnect() == 0)
    {
        Fail("Could not start connecting.");
        return false;
    }
    return true;
}

void
RemoteTraceOrchestrator::RunTask()
{
    switch(m_task)
    {
        case Task::kBrowse:
        {
            const std::string path = m_uri ? m_uri->GetRemoteBrowsingPathString() : "";
            m_status_message       = "Listing " + path;
            m_phase                = Phase::Browsing;
            if(path.empty() || m_session->StartBrowsing(path.c_str()) == 0)
            {
                Fail("Could not list the folder.");
            }
            break;
        }
        case Task::kDownload:
        {
            const std::string path = m_uri ? m_uri->GetRemoteResultPathString() : "";
            m_status_message       = "Downloading " + path;
            m_phase                = Phase::Downloading;
            if(path.empty() || m_session->StartDownload() == 0)
            {
                Fail("Could not start the download.");
            }
            break;
        }
        case Task::kConnect:
        default:
            Succeed(std::string());
            break;
    }
}

void
RemoteTraceOrchestrator::OnRemoteStatus(uint64_t status)
{
    if(status == kRPVControllerSshFailed)
    {
        switch(m_phase)
        {
            case Phase::Connecting:
                Fail("Could not reach the server. Check the host, port, and network.");
                break;
            case Phase::Authenticating:
                Fail("Sign-in failed. Check the user name, password, or key.");
                break;
            case Phase::Downloading: Fail("The download failed."); break;
            case Phase::Browsing:    Fail("Could not list this folder."); break;
            default:                 Fail("The SSH operation failed."); break;
        }
        return;
    }

    // Intermediate statuses (auth prompts, progress) are read from the session
    // by the UI directly.
    if(status != kRPVControllerSshCompleted)
    {
        return;
    }

    switch(m_phase)
    {
        case Phase::Connecting:
            m_status_message = "Signing in...";
            m_phase          = Phase::Authenticating;
            if(m_session->StartAuthenticate() == 0)
            {
                Fail("Could not start signing in.");
            }
            break;
        case Phase::Authenticating:
            m_authenticated = true;
            RunTask();
            break;
        case Phase::Downloading: Succeed(m_uri->GetLocalResultPathString()); break;
        case Phase::Browsing:    Succeed(m_uri->GetRemoteBrowsingPathString()); break;
        default: break;
    }
}

void
RemoteTraceOrchestrator::Succeed(const std::string& result)
{
    m_phase          = Phase::Done;
    m_running        = false;
    m_status_message = "Done.";
    if(m_on_result && m_task != Task::kConnect)
    {
        m_on_result(result);
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
    // task to build a fresh session rather than reusing a dead one.
    m_authenticated = false;
}

}  // namespace View
}  // namespace RocProfVis
