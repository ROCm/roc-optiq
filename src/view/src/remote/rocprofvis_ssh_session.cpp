// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ssh_session.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_core_assert.h"
#include <cfloat>

namespace RocProfVis
{
namespace View
{

    SshSession::SshSession(RemoteUri* uri) : m_connection(nullptr), m_uri(uri), m_args(nullptr)
    {
        if (uri)
        {
            AllocateConnection(uri->GetRemoteHostString().c_str(), uri->GetRemotePortInt());
        }
       
    }

    SshSession::~SshSession() {
        if (m_connection)
        {
            rocprofvis_controller_ssh_connection_free(m_connection);
        }
    }

    rocprofvis_result_t SshSession::Connect()
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (m_connection)
        {
            rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
            ROCPROFVIS_ASSERT(future);

            result = StartConnection(future);
            if (result == kRocProfVisResultSuccess)
            {
                while (kRocProfVisResultTimeout == rocprofvis_controller_future_wait(future, 0))
                {
                    result = CheckConnection();
                }
                result = CheckConnection();
            }
            rocprofvis_controller_future_free(future);
        }
        return result;
    }

    rocprofvis_result_t SshSession::Authenticate()
    {
        // Default to unknown error until all required authentication steps succeed.
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (m_connection && m_uri)
        {
            // Allocate async completion primitive used by the controller call.
            rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
            ROCPROFVIS_ASSERT(future);
            m_args = rocprofvis_controller_arguments_alloc();
            ROCPROFVIS_ASSERT(m_args != nullptr);

            result = StartAuthentication(future);

            if (result == kRocProfVisResultSuccess)
            {
                while (kRocProfVisResultTimeout == rocprofvis_controller_future_wait(future, 0))
                {
                    result = CheckAuthentication();
                }
                result = CheckAuthentication();
            }
            rocprofvis_controller_arguments_free(m_args);
            rocprofvis_controller_future_free(future);
        }
        return result;
    }

    // if command_line is omited, the data will be taken from corresponded fields of m_uri
    rocprofvis_result_t SshSession::Execute(const char* command_line)
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (m_connection && m_uri)
        {
            rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
            ROCPROFVIS_ASSERT(future);
            m_args = rocprofvis_controller_arguments_alloc();
            ROCPROFVIS_ASSERT(m_args != nullptr);
            result = command_line ? StartExecution(command_line, future) : StartExecution(future);
            if (result == kRocProfVisResultSuccess)
            {
                while (kRocProfVisResultTimeout == rocprofvis_controller_future_wait(future, 0))
                {
                    result = CheckExecution();
                }
                result = CheckExecution();
            }
            FinalizeExecution();   
            rocprofvis_controller_arguments_free(m_args);
            rocprofvis_controller_future_free(future);

        }
        return result;
    }

    // if remote_path or local_path omited, the data will be taken from corresponded fields of m_uri
    rocprofvis_result_t SshSession::Download(const char* remote_path, const char* local_path)
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (m_connection)
        {
            rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
            ROCPROFVIS_ASSERT(future);
            m_args = rocprofvis_controller_arguments_alloc();
            ROCPROFVIS_ASSERT(m_args != nullptr);

            result = remote_path && local_path ? StartDownload(remote_path, local_path, future) : StartDownload(future);
            if (result == kRocProfVisResultSuccess)
            {
                while (kRocProfVisResultTimeout == rocprofvis_controller_future_wait(future, 0))
                {
                    result = CheckDownload();
                }
                result = CheckDownload();
            }
            rocprofvis_controller_arguments_free(m_args);
            rocprofvis_controller_future_free(future);
        }
        return result;
    }

    rocprofvis_result_t SshSession::AllocateConnection(const char* host, int port)
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (host && port > 0 && port < 65536)
        {
            uint64_t prop_count = 0;
            rocprofvis_controller_arguments_t* args =
                rocprofvis_controller_arguments_alloc();
            ROCPROFVIS_ASSERT(args != nullptr);
            rocprofvis_controller_array_t* output =
                rocprofvis_controller_array_alloc(1);
            ROCPROFVIS_ASSERT(output != nullptr);

            result = rocprofvis_controller_set_string(args, kRPVControllerRemoteTypeHost, 0, host);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            result = rocprofvis_controller_set_uint64(args, kRPVControllerRemoteTypePort, 0, port);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            if (result == kRocProfVisResultSuccess)
            {
                result = rocprofvis_controller_ssh_connection_alloc(args, output);
                if (result == kRocProfVisResultSuccess)
                {
                    result = rocprofvis_controller_get_uint64(
                        output, kRPVControllerArrayNumEntries, 0, &prop_count);
                    if (prop_count != 1 ||
                        kRocProfVisResultSuccess != (result = rocprofvis_controller_get_object(output,
                            kRPVControllerArrayEntryIndexed,
                            0, &m_connection)))
                    {
                        result = kRocProfVisResultFailedSshCommunication;
                    }
                }
            }
            rocprofvis_controller_array_free(output);
            rocprofvis_controller_arguments_free(args);
        }
        return result;
    }

    rocprofvis_result_t SshSession::StartConnection(rocprofvis_controller_future_t* future)
    {
        if (m_connection && future)
        {
            return rocprofvis_controller_remote_connect_async(future, m_connection);
        }
        return kRocProfVisResultInvalidArgument;
    }

    rocprofvis_result_t SshSession::CheckConnection()
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (m_connection)
        {
            result = kRocProfVisResultPending;
            uint64_t remote_status;
            rocprofvis_result_t remote_result = rocprofvis_controller_get_uint64(m_connection, kRPVControllerRemoteStatus, 0, &remote_status);
            if (remote_status == kRPVControllerSshCompleted)
            {
                result = kRocProfVisResultSuccess;
            }
            else if (remote_status == kRPVControllerSshFailed)
            {
                result = kRocProfVisResultFailedSshCommunication;
            }
        }
        return result;
    }

    rocprofvis_result_t SshSession::StartAuthentication(rocprofvis_controller_future_t* future)
    {
        if (m_connection && m_uri && future)
        {
            return StartAuthentication(
                m_uri->GetRemoteUserString().c_str(),
                m_uri->GetRemotePasswordString().c_str(),
                m_uri->GetRemoteIdentityFileString().c_str(),
                m_uri->GetPassphraseString().c_str(),
                future);
        }
        else
        {
            return kRocProfVisResultInvalidArgument;
        }

    }

    rocprofvis_result_t SshSession::StartAuthentication(const char* user, const char* password, const char* identity_file, const char* passphrase, rocprofvis_controller_future_t* future)
    {
        rocprofvis_result_t result = kRocProfVisResultUnknownError;

        // Populate user/password and SSH key material used by remote authentication.
        result = rocprofvis_controller_set_string(m_args, kRPVControllerRemoteTypeUser, 0, user);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_string(m_args, kRPVControllerRemoteTypePassword, 0, password);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_string(m_args, kRPVControllerRemoteTypeKeyPath, 0, identity_file);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_string(m_args, kRPVControllerRemoteTypeKeyPassphrase, 0, passphrase);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        if (result == kRocProfVisResultSuccess)
        {
            result = rocprofvis_controller_remote_authenticate_async(future, m_connection, m_args);
        }
        return result;
    }

    rocprofvis_result_t SshSession::CheckAuthentication()
    {
        auto Failure = [&]()
            {
                rocprofvis_controller_remote_cancel_prompt(m_connection);
                return kRocProfVisResultFailedSshCommunication;
            };
        rocprofvis_result_t result = kRocProfVisResultUnknownError;
        uint64_t remote_status;
        rocprofvis_result_t remote_result = rocprofvis_controller_get_uint64(m_connection, kRPVControllerRemoteStatus, 0, &remote_status);
        if (kRocProfVisResultSuccess == remote_result)
        {
            if (remote_status == kRPVControllerSshCompleted)
            {
                return kRocProfVisResultSuccess;
            }
            else if (remote_status == kRPVControllerSshFailed)
            {
                return kRocProfVisResultFailedSshCommunication;
            } else
            if (kRPVControllerSshAuthRequest == remote_status)
            {
                uint64_t prompt_type = 0;
                rocprofvis_result_t prompt_result = rocprofvis_controller_get_uint64(
                    m_connection, kRPVControllerRemoteUserPromptType, 0, &prompt_type);
                if (kRocProfVisResultSuccess == prompt_result)
                {
                    if (prompt_type == kRPVControllerUserPromptTypeGeneric)
                    {
                        std::string name;
                        std::string instruction;
                        uint64_t num_prompts = 0;
                        std::vector<PromptItem> prompts;
                        prompt_result = GetString(m_connection, kRPVControllerRemoteUserGenericPromptName, 0, name);
                        if (kRocProfVisResultSuccess != prompt_result)
                        {
                            return Failure();
                        }
                        prompt_result = GetString(m_connection, kRPVControllerRemoteUserGenericPromptInstruction, 0, instruction);
                        if (kRocProfVisResultSuccess != prompt_result)
                        {
                            return Failure();
                        }
                        prompt_result = rocprofvis_controller_get_uint64(m_connection, kRPVControllerRemoteUserGenericNumPrompts, 0, &num_prompts);
                        if (kRocProfVisResultSuccess != prompt_result)
                        {
                            return Failure();
                        }
                        for (int i = 0; i < num_prompts; i++)
                        {
                            std::string prompt;
                            prompt_result = GetString(m_connection, kRPVControllerRemoteUserGenericPromptTextIndexed, i, prompt);
                            if (kRocProfVisResultSuccess != prompt_result)
                            {
                                return Failure();
                            }
                            uint64_t echo = 0;
                            prompt_result = rocprofvis_controller_get_uint64(m_connection, kRPVControllerRemoteUserGenericPromptEchoIndexed, i, &echo);
                            if (kRocProfVisResultSuccess != prompt_result)
                            {
                                return Failure();
                            }
                            prompts.push_back({ prompt,(bool)echo });
                        }
                            
                        m_prompt_request.update(name, instruction, prompts);
                            
                    }
                    else
                    if (prompt_type == kRPVControllerUserPromptTypeHostKey)
                    {
                        std::string host;
                        uint64_t port, state;
                        std::string fingerprint_sha256_b64;
                        std::string key_type;
                        prompt_result = GetString(m_connection, kRPVControllerRemoteUserHostKeyPromptHost, 0, host);
                        if (kRocProfVisResultSuccess != prompt_result)
                        {
                            return Failure();
                        }
                        prompt_result = rocprofvis_controller_get_uint64(m_connection, kRPVControllerRemoteUserHostKeyPromptPort, 0, &port);
                        if (kRocProfVisResultSuccess != prompt_result)
                        {
                            return Failure();
                        }
                        prompt_result = GetString(m_connection, kRPVControllerRemoteUserHostKeyPromptFinderprint, 0, fingerprint_sha256_b64);
                        if (kRocProfVisResultSuccess != prompt_result)
                        {
                            return Failure();
                        }
                        prompt_result = GetString(m_connection, kRPVControllerRemoteUserHostKeyPromptEncryptType, 0, key_type);
                        if (kRocProfVisResultSuccess != prompt_result)
                        {
                            return Failure();
                        }
                        prompt_result = rocprofvis_controller_get_uint64(m_connection, kRPVControllerRemoteUserHostKeyPromptState, 0, &state);
                        if (kRocProfVisResultSuccess != prompt_result)
                        {
                            return Failure();
                        }
                        else
                        {
                            m_host_key_request.update(host, port, fingerprint_sha256_b64, key_type, (HostKeyState)state);
                        }
                    }
                }
            }
        }
        return kRocProfVisResultPending;
    }
    
    rocprofvis_result_t SshSession::StartExecution(rocprofvis_controller_future_t* future)
    {
        if (m_connection && m_uri && future && !m_uri->GetRemoteCommandLineString().empty())
        {
            return StartExecution(m_uri->GetRemoteCommandLineString().c_str(), future);
        }
        else
        {
            return kRocProfVisResultInvalidArgument;
        }
    }

    rocprofvis_result_t SshSession::StartExecution(const char* command_line, rocprofvis_controller_future_t* future)
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (m_connection && command_line && future)
        {
            m_stdout.clear_updated();
            result = rocprofvis_controller_set_string(m_args, kRPVControllerRemoteTypeCommand, 0, command_line);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            if (result == kRocProfVisResultSuccess)
            {
                result = rocprofvis_controller_remote_execute_async(future, m_connection, m_args);
            }
        }
        return result;
    }

    rocprofvis_result_t SshSession::CheckExecution()
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (m_connection)
        {
            uint64_t remote_status;
            rocprofvis_result_t remote_result = rocprofvis_controller_get_uint64(m_connection, kRPVControllerRemoteStatus, 0, (uint64_t*)&remote_status);
            if (kRocProfVisResultSuccess == remote_result)
            {
                if (remote_status == kRPVControllerSshCompleted)
                {
                    result = kRocProfVisResultSuccess;
                }
                else if (remote_status == kRPVControllerSshFailed)
                {
                    result = kRocProfVisResultFailedSshCommunication;
                } else
                if (remote_status == kRPVControllerSshExecuteStdOut)
                {
                    std::string out;
                    rocprofvis_result_t stdout_result = GetString(m_connection, kRPVControllerRemoteExecuteStdOut, 0, out);
                    if (kRocProfVisResultSuccess != stdout_result)
                    {
                        result = kRocProfVisResultFailedSshCommunication;
                    }
                    else
                    {
                        m_stdout.append(out);
                        result = kRocProfVisResultPending;
                    }
                }
            }
        }
        return result;
    }

    void SshSession::FinalizeExecution() 
    {
        m_stdout.finish();
    }


    rocprofvis_result_t SshSession::StartDownload(rocprofvis_controller_future_t* future)
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (m_connection && m_uri)
        {
            std::string remote_path = m_uri->GetRemoteResultPathString();
            std::string local_path = m_uri->GetLocalResultPathString();
            result =  StartDownload(remote_path.c_str(), local_path.c_str(), future);
        }
        return result;
    }

    rocprofvis_result_t SshSession::StartDownload(const char* remote_path, const char* local_path, rocprofvis_controller_future_t* future)
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (m_connection)
        {

            result = rocprofvis_controller_set_string(m_args, kRPVControllerRemoteTypeFilePathSrc, 0, remote_path);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);



            result = rocprofvis_controller_set_string(m_args, kRPVControllerRemoteTypeFilePathDst, 0, local_path);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            uint64_t direction = 0; //download

            result = rocprofvis_controller_set_uint64(m_args, kRPVControllerRemoteTypeDirection, 0, direction);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            if (result == kRocProfVisResultSuccess)
            {
                result = rocprofvis_controller_remote_transfer_async(future, m_connection, m_args);
            }
        }
        return result;
    }

    rocprofvis_result_t SshSession::CheckDownload()
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (m_connection)
        {
            uint64_t remote_status;
            rocprofvis_result_t remote_result = rocprofvis_controller_get_uint64(m_connection, kRPVControllerRemoteStatus, 0, &remote_status);
            if (kRocProfVisResultSuccess == remote_result)
            {
                if (remote_status == kRPVControllerSshCompleted)
                {
                    result = kRocProfVisResultSuccess;
                }
                else if (remote_status == kRPVControllerSshFailed)
                {
                    result = kRocProfVisResultFailedSshCommunication;
                } else
                if (remote_status == kRPVControllerSshDownloadStarted)
                {
                    std::string name;
                    uint64_t size, time, downloaded_bytes;
                    rocprofvis_result_t progress_result = GetString(m_connection, kRPVControllerRemoteFileName, 0, name);
                    if (kRocProfVisResultSuccess != progress_result)
                    {
                        return kRocProfVisResultFailedSshCommunication;
                    }
                    progress_result = rocprofvis_controller_get_uint64(m_connection, kRPVControllerRemoteFileSize, 0, &size);
                    if (kRocProfVisResultSuccess != progress_result)
                    {
                        return kRocProfVisResultFailedSshCommunication;
                    }
                    progress_result = rocprofvis_controller_get_uint64(m_connection, kRPVControllerRemoteFileTime, 0, &time);
                    if (kRocProfVisResultSuccess != progress_result)
                    {
                        return kRocProfVisResultFailedSshCommunication;
                    }
                    progress_result = rocprofvis_controller_get_uint64(m_connection, kRPVControllerRemoteDownloaded, 0, &downloaded_bytes);
                    if (kRocProfVisResultSuccess != progress_result)
                    {
                        return kRocProfVisResultFailedSshCommunication;
                    }
                    m_file_stat.update(name, size, time, downloaded_bytes);
                    result = kRocProfVisResultPending;
                }
                else
                if (remote_status == kRPVControllerSshDownloadProgress)
                {
                    uint64_t downloaded_bytes;
                    rocprofvis_result_t progress_result = rocprofvis_controller_get_uint64(m_connection, kRPVControllerRemoteDownloaded, 0, &downloaded_bytes);
                    if (kRocProfVisResultSuccess != progress_result)
                    {
                        return kRocProfVisResultFailedSshCommunication;
                    }
                    m_file_stat.set_downloaded(downloaded_bytes);
                    result = kRocProfVisResultPending;
                }               
            }
        }
        return result;
    }



    bool SshSession::IsConnected()
    {
        return m_connection != nullptr;
    }

    rocprofvis_result_t SshSession::SubmitPromptResponses(std::vector<std::string>& responses)
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        rocprofvis_controller_arguments_t* args =
            rocprofvis_controller_arguments_alloc();
        ROCPROFVIS_ASSERT(args != nullptr);

        result = rocprofvis_controller_set_uint64(args, kRPVControllerUserNumResponses, 0, responses.size());
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        if (result == kRocProfVisResultSuccess)
        {
            for (int i = 0; i < responses.size(); i++)
            {
                result = rocprofvis_controller_set_string(args, kRPVControllerUserResponseIndexed, 0, responses[i].c_str());
                ROCPROFVIS_ASSERT_MSG_BREAK(result == kRocProfVisResultSuccess, "Failed setting response argument");
            }

            result = rocprofvis_controller_remote_submit_responses(m_connection, args);
        }
        return result;
    }

    rocprofvis_result_t SshSession::CancelRequest() 
    {
        return rocprofvis_controller_remote_cancel_prompt(m_connection);
    }

    rocprofvis_result_t SshSession::SubmitHostKeyDecision(HostKeyDecision decision) 
    {
        return rocprofvis_controller_remote_submit_hostkey_decision(m_connection, (uint64_t)decision);
    }

    rocprofvis_result_t
        SshSession::GetString(rocprofvis_handle_t* handle, rocprofvis_property_t property,
            uint64_t index, std::string& out_string)
    {
        uint32_t length = 0;
        out_string.clear();

        rocprofvis_result_t result =
            rocprofvis_controller_get_string(handle, property, index, nullptr, &length);
        if(result != kRocProfVisResultSuccess || length == 0)
        {
            return result;
        }

        out_string.resize(length);
        result = rocprofvis_controller_get_string(
            handle, property, index, const_cast<char*>(out_string.c_str()), &length);
        return result;
    }

}  // namespace DataModel
}  // namespace RocProfVis
