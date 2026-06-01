// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_handle.h"
#include "rocprofvis_ssh_uri.h"
#include "rocprofvis_ssh_fetch.h"
#include <atomic>

namespace RocProfVis
{
namespace View
{

    class SshSession
    {
    public:
        SshSession(RemoteUri* uri);
        ~SshSession();
        rocprofvis_result_t Connect();
        rocprofvis_result_t Authenticate();
        rocprofvis_result_t Execute(const char* command_line = nullptr);
        rocprofvis_result_t Download(const char* remote_path = nullptr, const char* local_path = nullptr);
        bool IsConnected();
        rocprofvis_result_t SubmitPromptResponses(std::vector<std::string>& responses);
        rocprofvis_result_t CancelRequest();
        rocprofvis_result_t SubmitHostKeyDecision(HostKeyDecision decision);
        PromptRequest* GetPromptRequest() { return &m_prompt_request; };
        HostKeyRequest* GetHostKeyRequest() { return &m_host_key_request; };
        ExecutionOutput* GetExecutionOutput() { return &m_stdout; };
        FileStat* GetFileStat() { return &m_file_stat; };
    private:
        rocprofvis_result_t AllocateConnection(const char* host, int port);
        rocprofvis_result_t StartConnection(rocprofvis_controller_future_t* future);
        rocprofvis_result_t StartAuthentication(rocprofvis_controller_future_t* future);
        rocprofvis_result_t StartAuthentication(const char* user, const char* password, const char* identity_file, const char* passphrase, rocprofvis_controller_future_t* future);
        rocprofvis_result_t StartExecution(rocprofvis_controller_future_t* future);
        rocprofvis_result_t StartExecution(const char* command_line, rocprofvis_controller_future_t* future);
        rocprofvis_result_t StartDownload(rocprofvis_controller_future_t* future);
        rocprofvis_result_t StartDownload(const char* remote_path, const char* local_path, rocprofvis_controller_future_t* future);
        rocprofvis_result_t CheckConnection();
        rocprofvis_result_t CheckAuthentication();
        rocprofvis_result_t CheckExecution();
        rocprofvis_result_t CheckDownload();
        void FinalizeExecution();
        rocprofvis_result_t GetString(rocprofvis_handle_t* handle, rocprofvis_property_t property,
                uint64_t index, std::string& out_string);

        RemoteUri* m_uri;
        rocprofvis_handle_t* m_connection;
        PromptRequest m_prompt_request;
        HostKeyRequest m_host_key_request;
        ExecutionOutput m_stdout;
        FileStat m_file_stat;
        rocprofvis_controller_arguments_t* m_args;
    };


}  // namespace DataModel
}  // namespace RocProfVis
