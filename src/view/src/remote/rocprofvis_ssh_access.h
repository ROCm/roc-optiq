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

    class Ssh
    {
    public:
        Ssh():m_connection_handle(nullptr) {};
        rocprofvis_result_t Connect(RemoteUri* uri);
        rocprofvis_result_t Authenticate(RemoteUri* uri);
        rocprofvis_result_t Execute(RemoteUri* uri);
        rocprofvis_result_t Download(RemoteUri* uri);
        bool IsConnected();
        rocprofvis_result_t Disconnect();
        rocprofvis_result_t SubmitPromptResponses(std::vector<std::string>& responses);
        rocprofvis_result_t CancelRequest();
        rocprofvis_result_t SubmitHostKeyDecision(HostKeyDecision decision);
        PromptRequest* GetPromptRequest() { return &m_prompt_request; };
        HostKeyRequest* GetHostKeyRequest() { return &m_host_key_request; };
        ExecutionOutput* GetExecutionOutput() { return &m_stdout; };
        FileStat* GetFileStat() { return &m_file_stat; };
    private:
        rocprofvis_result_t
            GetString(rocprofvis_handle_t* handle, rocprofvis_property_t property,
                uint64_t index, std::string& out_string);

        rocprofvis_handle_t* m_connection_handle;
        PromptRequest m_prompt_request;
        HostKeyRequest m_host_key_request;
        ExecutionOutput m_stdout;
        FileStat m_file_stat;
    };


}  // namespace DataModel
}  // namespace RocProfVis
