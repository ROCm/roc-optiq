// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_handle.h"
#include "rocprofvis_ssh_uri.h"
#include "rocprofvis_ssh_fetch.h"
#include "rocprofvis_appmonitor.h"
#include <atomic>
#include <functional>
#include <memory>
#include <string>

namespace RocProfVis
{
namespace View
{

    // The phase of work an SshSession is currently driving. A session owns one
    // live connection but runs phases sequentially; only one operation is in
    // flight at a time, which the AppMonitor polls each frame.
    enum class SshOperation
    {
        None,
        Connect,
        Authenticate,
        Execute,
        Download,
        Browse,
    };

    // Non-blocking SSH session. Each Start* method kicks off one phase against
    // the (decoupled) live connection, registers a MonitorOperation with the
    // AppMonitor, and returns the assigned operation id (0 on failure). The
    // monitor polls Poll() each frame and emits RemoteStatusEvents; a listener
    // (e.g. RemoteTraceOrchestrator) advances to the next phase on completion.
    // Prompt / host-key requests are still surfaced via the *Request accessors
    // and consumed by RenderSshAuthModal independent of the event stream.
    class SshSession
    {
    public:
        SshSession(std::shared_ptr<RemoteUri> uri);
        ~SshSession();

        // Phase starters. Return the monitor operation id, or 0 on failure.
        uint64_t StartConnect();
        uint64_t StartAuthenticate();
        uint64_t StartExecute(const char* command_line = nullptr);
        uint64_t StartDownload(const char* remote_path = nullptr, const char* local_path = nullptr);
        uint64_t StartBrowsing(const char* remote_path);

        // Cancels the in-flight phase (if any) and unregisters it from the
        // monitor. The connection stays alive for further phases.
        void CancelActiveOperation();

        bool IsConnected();
        SshOperation GetActiveOperation() const { return m_active_operation; }
        uint64_t GetActiveOperationId() const { return m_active_operation_id; }

        rocprofvis_result_t SubmitPromptResponses(std::vector<std::string>& responses);
        rocprofvis_result_t CancelRequest();
        rocprofvis_result_t SubmitHostKeyDecision(HostKeyDecision decision);
        PromptRequest* GetPromptRequest() { return &m_prompt_request; };
        HostKeyRequest* GetHostKeyRequest() { return &m_host_key_request; };
        ExecutionOutput* GetExecutionOutput() { return &m_stdout; };
        FileStat* GetFileStat() { return &m_file_stat; };
        RemoteDir* GetRemoteDir() { return &m_directory; };

    private:
        // Runs the check phase for the active operation (side effects: append
        // stdout, update progress, populate prompt/host-key requests) and
        // returns the raw remote status (rocprofvis_controller_remote_status_t).
        // The latest check result is stored in m_last_result.
        uint64_t Poll();

        // Common registration: allocates a future, calls start_fn, and on
        // success registers a MonitorOperation. Returns the operation id or 0.
        uint64_t BeginOperation(SshOperation operation, MonitorOperationType monitor_type,
                std::function<rocprofvis_result_t(rocprofvis_controller_future_t*)> start_fn);

        rocprofvis_result_t AllocateConnection(const char* host, int port);
        rocprofvis_result_t StartConnection(rocprofvis_controller_future_t* future);
        rocprofvis_result_t StartAuthentication(rocprofvis_controller_future_t* future);
        rocprofvis_result_t StartAuthentication(const char* user, const char* password, const char* identity_file, const char* passphrase, rocprofvis_controller_future_t* future);
        rocprofvis_result_t StartExecution(rocprofvis_controller_future_t* future);
        rocprofvis_result_t StartExecution(const char* command_line, rocprofvis_controller_future_t* future);
        rocprofvis_result_t StartDownloadOp(rocprofvis_controller_future_t* future);
        rocprofvis_result_t StartDownloadOp(const char* remote_path, const char* local_path, rocprofvis_controller_future_t* future);
        rocprofvis_result_t StartBrowsingOp(const char* remote_path, rocprofvis_controller_future_t* future);
        rocprofvis_result_t CheckConnection();
        rocprofvis_result_t CheckAuthentication();
        rocprofvis_result_t CheckExecution();
        rocprofvis_result_t CheckDownload();
        rocprofvis_result_t CheckBrowsing();
        void FinalizeExecution();
        rocprofvis_result_t GetString(rocprofvis_handle_t* handle, rocprofvis_property_t property,
                uint64_t index, std::string& out_string);

        std::shared_ptr<RemoteUri> m_uri;
        rocprofvis_handle_t* m_connection;
        PromptRequest m_prompt_request;
        HostKeyRequest m_host_key_request;
        ExecutionOutput m_stdout;
        FileStat m_file_stat;
        RemoteDir m_directory;

        SshOperation        m_active_operation;
        uint64_t            m_active_operation_id;
        rocprofvis_result_t m_last_result;
        // Pending command/paths captured for the active operation so the start
        // happens lazily inside BeginOperation's start_fn.
        std::string         m_pending_command;
        std::string         m_pending_remote_path;
        std::string         m_pending_local_path;
    };


}  // namespace DataModel
}  // namespace RocProfVis
