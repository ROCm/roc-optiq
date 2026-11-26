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
    // Reads a string argument at its full length: a null buffer asks for the
    // length first, so no value is cut short.
    rocprofvis_result_t get_string_argument(Arguments& args, rocprofvis_property_t property,
                                            uint64_t index, std::string& value)
    {
        uint32_t            length = 0;
        rocprofvis_result_t result = args.GetString(property, index, nullptr, &length);
        if (result == kRocProfVisResultSuccess)
        {
            value.resize(length);
            if (length > 0)
            {
                result = args.GetString(property, index, value.data(), &length);
                value.resize(length);
            }
        }
        return result;
    }
}  // namespace

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

        if (kRocProfVisResultSuccess == get_string_argument(args, kRPVControllerRemoteTypeHost, 0, host))
        {
            SshConnection * connection = s_ssh_client.AllocateConnection(host.c_str(), static_cast<int>(port));
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

        if (kRocProfVisResultSuccess == get_string_argument(args, kRPVControllerRemoteTypeUser, 0, user) &&
            kRocProfVisResultSuccess == get_string_argument(args, kRPVControllerRemoteTypePassword, 0, password) &&
            kRocProfVisResultSuccess == get_string_argument(args, kRPVControllerRemoteTypeKeyPath, 0, key_path) &&
            kRocProfVisResultSuccess == get_string_argument(args, kRPVControllerRemoteTypeKeyPassphrase, 0, key_passphrase))
        {

            future.Set(JobSystem::Get().IssueJob([&connection, user = std::move(user), password = std::move(password),
                                                  key_path = std::move(key_path),
                                                  key_passphrase = std::move(key_passphrase)](Future* future) -> rocprofvis_result_t {
                std::string error;

            SshClient::Result result = s_ssh_client.Authenticate(
                &connection,
                user.c_str(),
                password.c_str(),
                key_path.c_str(),
                key_passphrase.c_str(),
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
                    if (kRocProfVisResultSuccess == get_string_argument(args, kRPVControllerUserResponseIndexed, i, response))
                    {
                        responses.push_back(std::move(response));
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

        if (get_string_argument(args, kRPVControllerRemoteTypeCommand, 0, command) == kRocProfVisResultSuccess)
        {
            future.Set(JobSystem::Get().IssueJob([&connection, command = std::move(command)](Future* future) -> rocprofvis_result_t {
                if (SshClient::Result::Success == s_ssh_client.ExecuteCommand(&connection, command.c_str(), future))
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
        if (get_string_argument(args, kRPVControllerRemoteTypeFilePathSrc, 0, src_path) == kRocProfVisResultSuccess &&
            get_string_argument(args, kRPVControllerRemoteTypeFilePathDst, 0, dst_path) == kRocProfVisResultSuccess &&
            args.GetUInt64(kRPVControllerRemoteTypeDirection, 0, &direction) == kRocProfVisResultSuccess)
        {

            future.Set(JobSystem::Get().IssueJob([&connection, src_path = std::move(src_path), dst_path = std::move(dst_path),
                                                  direction](Future* future) -> rocprofvis_result_t {
            if (direction == 0)
            {
                SshClient::Result download_result = s_ssh_client.DownloadFile(&connection, src_path.c_str(), dst_path.c_str(), future);
                if (SshClient::Result::Success == download_result)
                {
                    connection.GetSshBridge()->SetStatus(kRPVControllerSshCompleted);
                    return kRocProfVisResultSuccess;
                }
                else if (SshClient::Result::Cancelled == download_result)
                {
                    connection.GetSshBridge()->SetStatus(kRPVControllerSshCancelled);
                    return kRocProfVisResultCancelled;
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
        if (get_string_argument(args, kRPVControllerRemoteTypeFilePathDst, 0, path) == kRocProfVisResultSuccess)
        {

            future.Set(JobSystem::Get().IssueJob([&connection, path = std::move(path)](Future* future) -> rocprofvis_result_t {
                if (SshClient::Result::Success == s_ssh_client.BrowseRemoteDirectory(&connection, path.c_str(), future))
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
