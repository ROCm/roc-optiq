// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_remote.h"

#include <string>
#include <utility>
#include <vector>

namespace RocProfVis
{
namespace Controller
{
namespace
{

constexpr uint64_t DEFAULT_SSH_PORT = 22;

// Reads a string argument of any length: a size query, then an exact-fit copy.
bool ReadString(Arguments& args, rocprofvis_property_t property, uint64_t index,
                std::string& out)
{
    uint32_t length = 0;
    if(args.GetString(property, index, nullptr, &length) != kRocProfVisResultSuccess)
    {
        return false;
    }
    out.assign(length, '\0');
    return length == 0 ||
           args.GetString(property, index, out.data(), &length) == kRocProfVisResultSuccess;
}

// Latches the bridge's terminal status for a finished job and maps the
// transport outcome to the job's result.
rocprofvis_result_t FinishJob(SshConnection& connection, SshClient::Result result)
{
    const bool ok = result == SshClient::Result::Success;
    connection.GetSshBridge()->SetStatus(ok ? kRPVControllerSshCompleted
                                            : kRPVControllerSshFailed);
    return ok ? kRocProfVisResultSuccess : kRocProfVisResultFailedSshCommunication;
}

}  // namespace

SshClient Remote::s_ssh_client;

rocprofvis_result_t Remote::AllocateConnection(Arguments& args, Array& output)
{
    uint64_t port = 0;
    if(args.GetUInt64(kRPVControllerRemoteTypePort, 0, &port) != kRocProfVisResultSuccess)
    {
        port = DEFAULT_SSH_PORT;
    }

    std::string host;
    if(ReadString(args, kRPVControllerRemoteTypeHost, 0, host))
    {
        SshConnection* connection =
            s_ssh_client.AllocateConnection(host, static_cast<int>(port));
        if(connection)
        {
            output.SetObject(kRPVControllerArrayEntryIndexed, 0,
                             (rocprofvis_handle_t*) connection);
            return kRocProfVisResultSuccess;
        }
    }
    return kRocProfVisResultInvalidArgument;
}

rocprofvis_result_t Remote::DeleteConnection(SshConnection& connection)
{
    s_ssh_client.DeleteConnection(&connection);
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t Remote::AsyncConnect(Future& future, SshConnection& connection)
{
    future.Set(JobSystem::Get().IssueJob(
        [&connection](Future* future) -> rocprofvis_result_t {
            return FinishJob(connection, s_ssh_client.Connect(&connection, future));
        },
        &future));
    return future.IsValid() ? kRocProfVisResultSuccess : kRocProfVisResultUnknownError;
}

rocprofvis_result_t Remote::AsyncAuthenticate(Future& future, SshConnection& connection,
                                              Arguments& args)
{
    std::string user;
    std::string password;
    std::string key_path;
    std::string key_passphrase;
    if(!ReadString(args, kRPVControllerRemoteTypeUser, 0, user) ||
       !ReadString(args, kRPVControllerRemoteTypePassword, 0, password) ||
       !ReadString(args, kRPVControllerRemoteTypeKeyPath, 0, key_path) ||
       !ReadString(args, kRPVControllerRemoteTypeKeyPassphrase, 0, key_passphrase))
    {
        return kRocProfVisResultInvalidArgument;
    }

    future.Set(JobSystem::Get().IssueJob(
        [&connection, user, password, key_path, key_passphrase](
            Future* future) -> rocprofvis_result_t {
            return FinishJob(connection,
                             s_ssh_client.Authenticate(&connection, user, password, key_path,
                                                       key_passphrase, future));
        },
        &future));
    return future.IsValid() ? kRocProfVisResultSuccess : kRocProfVisResultInvalidArgument;
}

rocprofvis_result_t Remote::SubmitPromptResponses(SshConnection& connection, Arguments& args)
{
    if(!connection.GetSshBridge())
    {
        return kRocProfVisResultInvalidArgument;
    }
    uint64_t num_responses = 0;
    if(args.GetUInt64(kRPVControllerUserNumResponses, 0, &num_responses) ==
       kRocProfVisResultSuccess)
    {
        // One entry per prompt, even if a read fails, so answers stay aligned
        // with the prompts they belong to.
        std::vector<std::string> responses(num_responses);
        for(uint64_t i = 0; i < num_responses; i++)
        {
            ReadString(args, kRPVControllerUserResponseIndexed, i, responses[i]);
        }
        connection.GetSshBridge()->SubmitPromptResponses(std::move(responses));
    }
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t Remote::SubmitHostKeyDecision(SshConnection& connection, uint64_t decision)
{
    if(!connection.GetSshBridge())
    {
        return kRocProfVisResultInvalidArgument;
    }
    connection.GetSshBridge()->SubmitHostKeyDecision(static_cast<HostKeyDecision>(decision));
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t Remote::CancelPrompt(SshConnection& connection)
{
    if(!connection.GetSshBridge())
    {
        return kRocProfVisResultInvalidArgument;
    }
    connection.GetSshBridge()->Cancel();
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t Remote::Reset(SshConnection& connection)
{
    if(!connection.GetSshBridge())
    {
        return kRocProfVisResultInvalidArgument;
    }
    connection.GetSshBridge()->Reset();
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t Remote::AsyncExecute(Future& future, SshConnection& connection,
                                         Arguments& args)
{
    std::string command;
    if(!ReadString(args, kRPVControllerRemoteTypeCommand, 0, command))
    {
        return kRocProfVisResultInvalidArgument;
    }

    future.Set(JobSystem::Get().IssueJob(
        [&connection, command](Future* future) -> rocprofvis_result_t {
            return FinishJob(connection,
                             s_ssh_client.ExecuteCommand(&connection, command, future));
        },
        &future));
    return future.IsValid() ? kRocProfVisResultSuccess : kRocProfVisResultInvalidArgument;
}

rocprofvis_result_t Remote::AsyncTransfer(Future& future, SshConnection& connection,
                                          Arguments& args)
{
    std::string src_path;
    std::string dst_path;
    uint64_t    direction = 0;
    if(!ReadString(args, kRPVControllerRemoteTypeFilePathSrc, 0, src_path) ||
       !ReadString(args, kRPVControllerRemoteTypeFilePathDst, 0, dst_path) ||
       args.GetUInt64(kRPVControllerRemoteTypeDirection, 0, &direction) !=
           kRocProfVisResultSuccess)
    {
        return kRocProfVisResultInvalidArgument;
    }

    future.Set(JobSystem::Get().IssueJob(
        [&connection, src_path, dst_path, direction](Future* future) -> rocprofvis_result_t {
            // Only remote-to-local (direction 0) transfers are implemented.
            if(direction != 0)
            {
                return kRocProfVisResultNotSupported;
            }
            return FinishJob(connection,
                             s_ssh_client.DownloadFile(&connection, src_path, dst_path, future));
        },
        &future));
    return future.IsValid() ? kRocProfVisResultSuccess : kRocProfVisResultInvalidArgument;
}

rocprofvis_result_t Remote::AsyncRemoteDirectory(Future& future, SshConnection& connection,
                                                 Arguments& args)
{
    std::string path;
    if(!ReadString(args, kRPVControllerRemoteTypeFilePathDst, 0, path))
    {
        return kRocProfVisResultInvalidArgument;
    }

    future.Set(JobSystem::Get().IssueJob(
        [&connection, path](Future* future) -> rocprofvis_result_t {
            return FinishJob(connection,
                             s_ssh_client.BrowseRemoteDirectory(&connection, path, future));
        },
        &future));
    return future.IsValid() ? kRocProfVisResultSuccess : kRocProfVisResultInvalidArgument;
}

}  // namespace Controller
}  // namespace RocProfVis
