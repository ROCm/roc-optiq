// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_future.h"
#include "rocprofvis_controller_arguments.h"
#include "rocprofvis_controller_array.h"
#include "rocprofvis_controller_ssh_client.h"

namespace RocProfVis
{
namespace Controller
{

    class Remote
    {
     public:

    static rocprofvis_result_t  AllocateConnection(
        Arguments& args,
        Array& output);

    static rocprofvis_result_t DeleteConnection(
        SshConnection& connection);

    static rocprofvis_result_t AsyncConnect(
        Future& future,
        SshConnection& connection);

    static rocprofvis_result_t AsyncAuthenticate(
        Future& future,
        SshConnection& connection,
        Arguments& args);

    static rocprofvis_result_t AsyncExecute(
        Future& future,
        SshConnection& connection,
        Arguments& args);

    static rocprofvis_result_t AsyncTransfer(
        Future& future,
        SshConnection& connection,
        Arguments& args);

    static rocprofvis_result_t AsyncRemoteDirectory(
        Future& future,
        SshConnection& connection,
        Arguments& args);

    static rocprofvis_result_t SubmitPromptResponses(
        SshConnection& connection,
        Arguments& args);

    static rocprofvis_result_t SubmitHostKeyDecision(
        SshConnection& connection, 
        uint64_t decision);

    static rocprofvis_result_t CancelPrompt(
        SshConnection& connection);

    static rocprofvis_result_t Reset(
        SshConnection& connection);

    private:
        static SshClient s_ssh_client;

    };
}
}
