// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ssh_access.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_core_assert.h"

namespace RocProfVis
{
namespace View
{
	rocprofvis_result_t Ssh::Connect(RemoteUri * uri)
	{
        rocprofvis_result_t result = kRocProfVisResultUnknownError;

        uint64_t prop_count = 0;
        rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
        ROCPROFVIS_ASSERT(future);

        rocprofvis_controller_arguments_t* args =
            rocprofvis_controller_arguments_alloc();
        ROCPROFVIS_ASSERT(args != nullptr);
        rocprofvis_controller_array_t* output =
            rocprofvis_controller_array_alloc(1);
        ROCPROFVIS_ASSERT(output != nullptr);

        result = rocprofvis_controller_set_string(args, kRPVControllerRemoteTypeHost, 0, uri->GetRemoteHostString().c_str());
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_uint64(args, kRPVControllerRemoteTypePort, 0, uri->GetRemotePortInt());
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        if (result == kRocProfVisResultSuccess)
        {
            result = rocprofvis_controller_remote_connect_async(future, args, output);
            if (result == kRocProfVisResultSuccess)
            {
                result = rocprofvis_controller_future_wait(future, FLT_MAX);
            }
        }

        rocprofvis_controller_future_free(future);

        if (result == kRocProfVisResultSuccess)
        {
            rocprofvis_result_t result = rocprofvis_controller_get_uint64(
                output, kRPVControllerArrayNumEntries, 0, &prop_count);
            if (prop_count != 1 ||
                kRocProfVisResultSuccess != (result = rocprofvis_controller_get_object(output,
                    kRPVControllerArrayEntryIndexed,
                    0, &m_connection_handle)))
            {
                result = kRocProfVisResultFailedSshCommunication;
            }
        }

        rocprofvis_controller_array_free(output);
        rocprofvis_controller_arguments_free(args);

        return result;
	}

    rocprofvis_result_t Ssh::Authenticate(RemoteUri * uri)
    {
        rocprofvis_result_t result = kRocProfVisResultUnknownError;

        uint64_t prop_count = 0;
        rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
        ROCPROFVIS_ASSERT(future);

        rocprofvis_controller_arguments_t* args =
            rocprofvis_controller_arguments_alloc();
        ROCPROFVIS_ASSERT(args != nullptr);

        result = rocprofvis_controller_set_string(args, kRPVControllerRemoteTypeUser, 0, uri->GetRemoteUserString().c_str());
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_string(args, kRPVControllerRemoteTypePassword, 0, uri->GetRemotePasswordString().c_str());
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_string(args, kRPVControllerRemoteTypeKeyPath, 0, uri->GetRemoteIdentityFileString().c_str());
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_string(args, kRPVControllerRemoteTypeKeyPassphrase, 0, uri->GetRemoteIdentityFileString().c_str());
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        if (result == kRocProfVisResultSuccess)
        {
            result = rocprofvis_controller_remote_authenticate_async(future, m_connection_handle, args);
            if (result == kRocProfVisResultSuccess)
            {

                result = kRocProfVisResultPending;
                while (result != kRocProfVisResultSuccess && result != kRocProfVisResultFailedSshCommunication)
                {
                    result = rocprofvis_controller_future_wait(future, 0);
                    if (kRocProfVisResultSshCommunicationCallback == result)
                    {
                        uint64_t callback_type;
                        rocprofvis_result_t callback_result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureRemoteCallbackType, 0, &callback_type);
                        if (kRocProfVisResultSuccess == callback_result)
                        {
                            if (kRPVControllerSshCallbackAuthRequest == callback_type)
                            {
                                uint64_t prompt_type = 0;
                                rocprofvis_result_t prompt_result = rocprofvis_controller_get_uint64(
                                    future, kRPVControllerFutureUserPromptType, 0, &prompt_type);
                                if (kRocProfVisResultSuccess != prompt_result)
                                {
                                    break;
                                }
                                if (prompt_type == kRPVControllerUserPromptTypeGeneric)
                                {
                                    std::string name;
                                    std::string instruction;
                                    uint64_t num_prompts = 0;
                                    std::vector<PromptItem> prompts;
                                    prompt_result = GetString(future, kRPVControllerFutureUserGenericPromptName, 0, name);
                                    if (kRocProfVisResultSuccess != prompt_result)
                                    {
                                        break;
                                    }
                                    prompt_result = GetString(future, kRPVControllerFutureUserGenericPromptInstruction, 0, instruction);
                                    if (kRocProfVisResultSuccess != prompt_result)
                                    {
                                        break;
                                    }
                                    prompt_result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureUserGenericNumPrompts, 0, &num_prompts);
                                    if (kRocProfVisResultSuccess != prompt_result)
                                    {
                                        break;
                                    }
                                    for (int i = 0; i < num_prompts; i++)
                                    {
                                        std::string prompt;
                                        prompt_result = GetString(future, kRPVControllerFutureUserGenericPromptTextIndexed, i, prompt);
                                        if (kRocProfVisResultSuccess != prompt_result)
                                        {
                                            break;
                                        }
                                        uint64_t echo = 0;
                                        prompt_result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureUserGenericPromptEchoIndexed, i, &echo);
                                        if (kRocProfVisResultSuccess != prompt_result)
                                        {
                                            break;
                                        }
                                        prompts.push_back({ prompt,(bool)echo });
                                    }
                                    if (kRocProfVisResultSuccess != prompt_result)
                                    {
                                        break;
                                    }
                                    else
                                    {
                                        m_prompt_request.update(name, instruction, prompts);
                                    }
                                }
                                else
                                    if (prompt_type == kRPVControllerUserPromptTypeHostKey)
                                    {
                                        std::string host;
                                        uint64_t port, state;
                                        std::string fingerprint_sha256_b64;
                                        std::string key_type;
                                        prompt_result = GetString(future, kRPVControllerFutureUserHostKeyPromptHost, 0, host);
                                        if (kRocProfVisResultSuccess != prompt_result)
                                        {
                                            break;
                                        }
                                        prompt_result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureUserHostKeyPromptPort, 0, &port);
                                        if (kRocProfVisResultSuccess != prompt_result)
                                        {
                                            break;
                                        }
                                        prompt_result = GetString(future, kRPVControllerFutureUserHostKeyPromptFinderprint, 0, fingerprint_sha256_b64);
                                        if (kRocProfVisResultSuccess != prompt_result)
                                        {
                                            break;
                                        }
                                        prompt_result = GetString(future, kRPVControllerFutureUserHostKeyPromptEncryptType, 0, key_type);
                                        if (kRocProfVisResultSuccess != prompt_result)
                                        {
                                            break;
                                        }
                                        prompt_result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureUserHostKeyPromptState, 0, &state);
                                        if (kRocProfVisResultSuccess != prompt_result)
                                        {
                                            break;
                                        }
                                        else
                                        {
                                            m_host_key_request.update(host, port, fingerprint_sha256_b64, key_type, (HostKeyState)state);
                                        }
                                    }

                                if (kRocProfVisResultSuccess != prompt_result)
                                {
                                    rocprofvis_controller_remote_cancel_prompt(m_connection_handle);
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }

        rocprofvis_controller_future_free(future);
        rocprofvis_controller_arguments_free(args);

        return result;
    }

    rocprofvis_result_t Ssh::Execute(RemoteUri * uri)
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (!uri->GetRemoteCommandLineString().empty())
        {
            rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
            ROCPROFVIS_ASSERT(future);
            rocprofvis_controller_arguments_t* args =
                rocprofvis_controller_arguments_alloc();
            ROCPROFVIS_ASSERT(args != nullptr);

            result = rocprofvis_controller_set_string(args, kRPVControllerRemoteTypeCommand, 0, uri->GetRemoteCommandLineString().c_str());
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            if (result == kRocProfVisResultSuccess)
            {
                result = rocprofvis_controller_remote_execute_async(future, m_connection_handle, args);
                if (result == kRocProfVisResultSuccess)
                {
                    result = kRocProfVisResultPending;
                    while (result != kRocProfVisResultSuccess && result != kRocProfVisResultFailedSshCommunication)
                    {
                        result = rocprofvis_controller_future_wait(future, 0);
                        if (kRocProfVisResultSshCommunicationCallback == result)
                        {
                            uint64_t callback_type;
                            rocprofvis_result_t callback_result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureRemoteCallbackType, 0, (uint64_t*)&callback_type);
                            if (kRocProfVisResultSuccess == callback_result)
                            {
                                if (kRPVControllerSshCallbackExecuteStdOut == callback_type)
                                {
                                    std::string out;
                                    rocprofvis_result_t stdout_result = GetString(future, kRPVControllerFutureRemoteExecuteStdOut, 0, out);
                                    if (kRocProfVisResultSuccess != stdout_result)
                                    {
                                        break;
                                    }
                                    else
                                    {
                                        m_stdout.append(out);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            m_stdout.finish();
            rocprofvis_controller_future_free(future);
            rocprofvis_controller_arguments_free(args);
        }
        return result;
    }

    rocprofvis_result_t Ssh::Download(RemoteUri * uri)
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (!uri->GetRemoteResultPathString().empty())
        {
            rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
            ROCPROFVIS_ASSERT(future);
            rocprofvis_controller_arguments_t* args =
                rocprofvis_controller_arguments_alloc();
            ROCPROFVIS_ASSERT(args != nullptr);

            std::string remote_result_path = uri->GetRemoteResultPathString();

            result = rocprofvis_controller_set_string(args, kRPVControllerRemoteTypeFilePathSrc, 0, remote_result_path.c_str());
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            std::string local_path = uri->GetLocalResultPathString();

            result = rocprofvis_controller_set_string(args, kRPVControllerRemoteTypeFilePathDst, 0, local_path.data());
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            uint64_t direction = 0; //download

            result = rocprofvis_controller_set_uint64(args, kRPVControllerRemoteTypeDirection, 0, direction);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            if (result == kRocProfVisResultSuccess)
            {
                result = rocprofvis_controller_remote_transfer_async(future, m_connection_handle, args);
                if (result == kRocProfVisResultSuccess)
                {
                    result = kRocProfVisResultPending;
                    while (result != kRocProfVisResultSuccess && result != kRocProfVisResultFailedSshCommunication)
                    {
                        result = rocprofvis_controller_future_wait(future, 0);
                        if (kRocProfVisResultSshCommunicationCallback == result)
                        {
                            uint64_t callback_type;
                            rocprofvis_result_t callback_result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureRemoteCallbackType, 0, &callback_type);
                            if (kRocProfVisResultSuccess == callback_result)
                            {
                                if (kRPVControllerSshCallbackDownloadSarted == callback_type)
                                {
                                    std::string name;
                                    uint64_t size, time, downloaded_bytes;
                                    rocprofvis_result_t progress_result = GetString(future, kRPVControllerFutureRemoteFileName, 0, name);
                                    if (kRocProfVisResultSuccess != progress_result)
                                    {
                                        break;
                                    }
                                    progress_result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureRemoteFileSize, 0, &size);
                                    if (kRocProfVisResultSuccess != progress_result)
                                    {
                                        break;
                                    }
                                    progress_result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureRemoteFileTime, 0, &time);
                                    if (kRocProfVisResultSuccess != progress_result)
                                    {
                                        break;
                                    }
                                    progress_result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureRemoteDownloaded, 0, &downloaded_bytes);
                                    if (kRocProfVisResultSuccess != progress_result)
                                    {
                                        break;
                                    }
                                    m_file_stat.update(name, size, time, downloaded_bytes);
                                } else
                                if (kRPVControllerSshCallbackDownloadProgress == callback_type)
                                {
                                    uint64_t downloaded_bytes;
                                    rocprofvis_result_t progress_result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureRemoteDownloaded, 0, &downloaded_bytes);
                                    if (kRocProfVisResultSuccess != progress_result)
                                    {
                                        break;
                                    }
                                    m_file_stat.set_downloaded(downloaded_bytes);
                                }
                            }
                        }
                    }
                }
            }

            rocprofvis_controller_future_free(future);
        }
        return result;
    }

    bool Ssh::IsConnected()
    {
        return m_connection_handle != nullptr;
    }

    rocprofvis_result_t Ssh::Disconnect()
    {
        return rocprofvis_controller_remote_disconnect(m_connection_handle);
    }

    rocprofvis_result_t Ssh::SubmitPromptResponses(std::vector<std::string>& responses)
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

            result = rocprofvis_controller_remote_submit_responses(m_connection_handle, args);
        }
        return result;
    }

    rocprofvis_result_t Ssh::CancelRequest() 
    {
        return rocprofvis_controller_remote_cancel_prompt(m_connection_handle);
    }

    rocprofvis_result_t Ssh::SubmitHostKeyDecision(HostKeyDecision decision) 
    {
        return rocprofvis_controller_remote_submit_hostkey_decision(m_connection_handle, (uint64_t)decision);
    }

    rocprofvis_result_t
        Ssh::GetString(rocprofvis_handle_t* handle, rocprofvis_property_t property,
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
