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
, m_session(nullptr)
, m_status_token(EventManager::InvalidSubscriptionToken)
, m_phase(Phase::Idle)
, m_running(false)
, m_status_message()
{
    m_status_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kRemoteStatusChanged),
        [this](std::shared_ptr<RocEvent> event)
        {
            auto* status_event = static_cast<RemoteStatusEvent*>(event.get());
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

    m_status_message = "Connecting...";
    m_phase          = Phase::Connecting;
    m_running        = true;

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
RemoteTraceOrchestrator::Fail(const std::string& message)
{
    spdlog::warn("[remote-trace] {}", message);
    m_status_message = message;
    m_phase          = Phase::Failed;
    m_running        = false;
}

}  // namespace View
}  // namespace RocProfVis
