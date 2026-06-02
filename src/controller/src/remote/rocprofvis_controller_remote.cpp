// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_remote.h"
#include <array>


namespace RocProfVis
{
namespace Controller
{
    SshClient Remote::s_ssh_client;

    rocprofvis_result_t  Remote::AllocateConnection(
        Arguments& args,
        Array& output)
    {
        std::array<char, 128> host{};
        uint32_t host_length = host.size();

        uint64_t port;
        if (kRocProfVisResultSuccess != args.GetUInt64(kRPVControllerRemoteTypePort, 0, &port))
        {
            port = 22;
        }

        if (kRocProfVisResultSuccess == args.GetString(kRPVControllerRemoteTypeHost, 0, host.data(), &host_length))
        {
            SshConnection * connection = s_ssh_client.AllocateConnection(host.data(), port);
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
        rocprofvis_result_t   error = kRocProfVisResultUnknownError;

        std::array<char, 128> password{};
        uint32_t password_length = password.size();
        std::array<char, 128> user{};
        uint32_t user_length = user.size();
        std::array<char, 1024> key_path{};
        uint32_t key_path_length = key_path.size();
        std::array<char, 128> key_passphrase{};
        uint32_t key_passphrase_length = key_passphrase.size();

        if (kRocProfVisResultSuccess == args.GetString(kRPVControllerRemoteTypeUser, 0, user.data(), &user_length) &&
            kRocProfVisResultSuccess == args.GetString(kRPVControllerRemoteTypePassword, 0, password.data(), &password_length) &&
            kRocProfVisResultSuccess == args.GetString(kRPVControllerRemoteTypeKeyPath, 0, key_path.data(), &key_path_length) &&
            kRocProfVisResultSuccess == args.GetString(kRPVControllerRemoteTypeKeyPassphrase, 0, key_passphrase.data(), &key_passphrase_length))
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
                for (int i = 0; i < num_responses; i++)
                {
                    std::array<char, 128> response{};
                    uint32_t response_length = response.size();
                    if (kRocProfVisResultSuccess == args.GetString(kRPVControllerUserResponseIndexed, i, response.data(), &response_length))
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
        rocprofvis_result_t   error     = kRocProfVisResultUnknownError;
        std::array<char, 4096> command{};
        uint32_t command_length = command.size();

        if (args.GetString(kRPVControllerRemoteTypeCommand, 0, command.data(), &command_length) == kRocProfVisResultSuccess)
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
        rocprofvis_result_t   error = kRocProfVisResultUnknownError;
        std::array<char, 128> src_path{};
        uint32_t src_path_length = src_path.size();
        std::array<char, 128> dst_path{};
        uint32_t dst_path_length = dst_path.size();
        uint64_t direction = 0;
        if (args.GetString(kRPVControllerRemoteTypeFilePathSrc, 0, src_path.data(), &src_path_length) == kRocProfVisResultSuccess &&
            args.GetString(kRPVControllerRemoteTypeFilePathDst, 0, dst_path.data(), &dst_path_length) == kRocProfVisResultSuccess &&
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

}
}
