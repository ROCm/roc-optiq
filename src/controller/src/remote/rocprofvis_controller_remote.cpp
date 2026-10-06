// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_remote.h"
#include <string>


namespace RocProfVis
{
namespace Controller
{
    // Reads a string argument of any length: a size query, then an exact-fit copy.
    static bool ReadString(Arguments& args, rocprofvis_property_t property, uint64_t index,
                           std::string& out)
    {
        uint32_t length = 0;
        if (args.GetString(property, index, nullptr, &length) != kRocProfVisResultSuccess)
        {
            return false;
        }
        out.assign(length, '\0');
        return length == 0 ||
               args.GetString(property, index, out.data(), &length) == kRocProfVisResultSuccess;
    }

    SshClient Remote::s_ssh_client;

    rocprofvis_result_t  Remote::AllocateConnection(
        Arguments& args,
        Array& output)
    {
        std::string host;

        uint64_t port;
        if (kRocProfVisResultSuccess != args.GetUInt64(kRPVControllerRemoteTypePort, 0, &port))
        {
            port = 22;
        }

        if (ReadString(args, kRPVControllerRemoteTypeHost, 0, host))
        {
            SshConnection * connection = s_ssh_client.AllocateConnection(host.data(), static_cast<int>(port));
            if (connection)
            {
                output.SetObject(kRPVControllerArrayEntryIndexed, 0, (rocprofvis_handle_t*)connection);
                return kRocProfVisResultSuccess;
            }
        }
        return kRocProfVisResultInvalidArgument;
    }

    rocprofvis_result_t Remote::DeleteConnection(
        SshConnection& connection)
    {
        s_ssh_client.DeleteConnection(&connection);
        return kRocProfVisResultSuccess;
    }

	rocprofvis_result_t Remote::AsyncConnect(
            Future& future,
            SshConnection& connection)
	{
        rocprofvis_result_t   error     = kRocProfVisResultUnknownError;

        future.Set(JobSystem::Get().IssueJob([&connection](Future* future) -> rocprofvis_result_t {
            std::string error;

            SshClient::Result result =  s_ssh_client.Connect(
                    &connection, 
                    future);

               if (result == SshClient::Result::Success)
                {
                   connection.GetSshBridge()->SetStatus(kRPVControllerSshCompleted);
                    return kRocProfVisResultSuccess;
                }
                else
                {
                   connection.GetSshBridge()->SetStatus(kRPVControllerSshFailed);
                   return kRocProfVisResultFailedSshCommunication;
                }
            
            }, &future));

        if(future.IsValid())
        {
            error = kRocProfVisResultSuccess;
        }

        return error;
	}

    rocprofvis_result_t Remote::AsyncAuthenticate(
        Future& future,
        SshConnection& connection,
        Arguments& args)
    {
        rocprofvis_result_t   error = kRocProfVisResultInvalidArgument;

        std::string password;
        std::string user;
        std::string key_path;
        std::string key_passphrase;

        if (ReadString(args, kRPVControllerRemoteTypeUser, 0, user) &&
            ReadString(args, kRPVControllerRemoteTypePassword, 0, password) &&
            ReadString(args, kRPVControllerRemoteTypeKeyPath, 0, key_path) &&
            ReadString(args, kRPVControllerRemoteTypeKeyPassphrase, 0, key_passphrase))
        {

            future.Set(JobSystem::Get().IssueJob([&connection, user, password, key_path, key_passphrase](Future* future) -> rocprofvis_result_t {
                std::string error;

            SshClient::Result result = s_ssh_client.Authenticate(
                &connection,
                user.data(),
                password.data(),
                key_path.data(),
                key_passphrase.data(),
                future);

            if (result == SshClient::Result::Success)
            {
                connection.GetSshBridge()->SetStatus(kRPVControllerSshCompleted);
                return kRocProfVisResultSuccess;
            }
            else
            {
                connection.GetSshBridge()->SetStatus(kRPVControllerSshFailed);
                return kRocProfVisResultFailedSshCommunication;
            }

        }, & future));

        if (future.IsValid())
        {
            error = kRocProfVisResultSuccess;
        }
    }

        return error;
    }

    rocprofvis_result_t Remote::SubmitPromptResponses(
        SshConnection& connection, 
        Arguments& args) 
    {
        if (connection.GetSshBridge())
        {
            uint64_t num_responses = 0;
            if (kRocProfVisResultSuccess == args.GetUInt64(kRPVControllerUserNumResponses, 0, &num_responses))
            {
                std::vector<std::string> responses;
                for (uint64_t i = 0; i < num_responses; i++)
                {
                    std::string response;
                    if (ReadString(args, kRPVControllerUserResponseIndexed, i, response))
                    {
                        responses.push_back(response.data());
                    }
                }
                connection.GetSshBridge()->SubmitPromptResponses(responses);
            }
            return kRocProfVisResultSuccess;
        }
        return kRocProfVisResultInvalidArgument;
    }

    rocprofvis_result_t Remote::SubmitHostKeyDecision(
        SshConnection& connection, 
        uint64_t decision) 
    {
        if (connection.GetSshBridge())
        {
            connection.GetSshBridge()->SubmitHostKeyDecision((HostKeyDecision)decision);
            return kRocProfVisResultSuccess;
        }
        return kRocProfVisResultInvalidArgument;
    }

    rocprofvis_result_t Remote::CancelPrompt(
        SshConnection& connection)
    {
        if (connection.GetSshBridge())
        {
            connection.GetSshBridge()->Cancel();
            return kRocProfVisResultSuccess;
        }
        return kRocProfVisResultInvalidArgument;
    }

    rocprofvis_result_t Remote::Reset(
        SshConnection& connection)
    {
        if (connection.GetSshBridge())
        {
            connection.GetSshBridge()->Reset();
            return kRocProfVisResultSuccess;
        }
        return kRocProfVisResultInvalidArgument;
    }


         
    rocprofvis_result_t Remote::AsyncExecute(
        Future& future,
        SshConnection& connection,
        Arguments& args)
	{
        rocprofvis_result_t   error     = kRocProfVisResultInvalidArgument;
        std::string command;

        if (ReadString(args, kRPVControllerRemoteTypeCommand, 0, command))
        {
            future.Set(JobSystem::Get().IssueJob([&connection, command](Future* future) -> rocprofvis_result_t {
                if (SshClient::Result::Success == s_ssh_client.ExecuteCommand(&connection, command.data(), future))
                {
                    connection.GetSshBridge()->SetStatus(kRPVControllerSshCompleted);
                    return kRocProfVisResultSuccess;
                }
                else
                {
                    connection.GetSshBridge()->SetStatus(kRPVControllerSshFailed);
                    return kRocProfVisResultFailedSshCommunication;
                }

                }, &future));

            if (future.IsValid())
            {
                error = kRocProfVisResultSuccess;
            }
        }

        return error;
	}

    rocprofvis_result_t Remote::AsyncTransfer(
        Future& future,
        SshConnection& connection,
        Arguments& args)
    {
        rocprofvis_result_t   error = kRocProfVisResultInvalidArgument;
        std::string src_path;
        std::string dst_path;
        uint64_t direction = 0;
        if (ReadString(args, kRPVControllerRemoteTypeFilePathSrc, 0, src_path) &&
            ReadString(args, kRPVControllerRemoteTypeFilePathDst, 0, dst_path) &&
            args.GetUInt64(kRPVControllerRemoteTypeDirection, 0, &direction) == kRocProfVisResultSuccess)
        {

            future.Set(JobSystem::Get().IssueJob([&connection, src_path, dst_path, direction](Future* future) -> rocprofvis_result_t {
            if (direction == 0)
            {
                if (SshClient::Result::Success == s_ssh_client.DownloadFile(&connection, src_path.data(), dst_path.data(), future))
                {
                    connection.GetSshBridge()->SetStatus(kRPVControllerSshCompleted);
                    return kRocProfVisResultSuccess;
                }
                else
                {
                    connection.GetSshBridge()->SetStatus(kRPVControllerSshFailed);
                    return kRocProfVisResultFailedSshCommunication;
                }
            }
            else
            {
                return kRocProfVisResultNotSupported;
            }

            }, &future));
        }

        if (future.IsValid())
        {
            error = kRocProfVisResultSuccess;
        }

        return error;
    }

    rocprofvis_result_t Remote::AsyncRemoteDirectory(
        Future& future,
        SshConnection& connection,
        Arguments& args)
    {
        rocprofvis_result_t   error = kRocProfVisResultInvalidArgument;
        std::string path;
        if (ReadString(args, kRPVControllerRemoteTypeFilePathDst, 0, path))
        {

            future.Set(JobSystem::Get().IssueJob([&connection, path](Future* future) -> rocprofvis_result_t {
                if (SshClient::Result::Success == s_ssh_client.BrowseRemoteDirectory(&connection, path.data(), future))
                {
                    connection.GetSshBridge()->SetStatus(kRPVControllerSshCompleted);
                    return kRocProfVisResultSuccess;
                }
                else
                {
                    connection.GetSshBridge()->SetStatus(kRPVControllerSshFailed);
                    return kRocProfVisResultFailedSshCommunication;
                }

                }, &future));
        }

        if (future.IsValid())
        {
            error = kRocProfVisResultSuccess;
        }

        return error;
    }

}
}
