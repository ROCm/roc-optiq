// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ssh_session.h"
#include "rocprofvis_appmonitor.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_events.h"
#include <cfloat>
#include <memory>

namespace RocProfVis
{
namespace View
{

    SshSession::SshSession(RemoteUri* uri)
    : m_uri(uri)
    , m_connection(nullptr)
    , m_active_operation(SshOperation::None)
    , m_active_operation_id(0)
    , m_last_result(kRocProfVisResultSuccess)
    {
        if (uri)
        {
            AllocateConnection(uri->GetRemoteHostString().c_str(), uri->GetRemotePortInt());
        }
       
    }

    SshSession::~SshSession() {
        CancelActiveOperation();
        if (m_connection)
        {
            rocprofvis_controller_ssh_connection_free(m_connection);
        }
    }

    // Drives the active phase's check work each frame and returns the raw remote
    // status. Called by the AppMonitor via the registered status callback.
    uint64_t SshSession::Poll()
    {
        switch (m_active_operation)
        {
            case SshOperation::Connect:      m_last_result = CheckConnection(); break;
            case SshOperation::Authenticate: m_last_result = CheckAuthentication(); break;
            case SshOperation::Execute:      m_last_result = CheckExecution(); break;
            case SshOperation::Download:     m_last_result = CheckDownload(); break;
            case SshOperation::None:
            default:
                return kRPVControllerSshIdle;
        }

        uint64_t remote_status = kRPVControllerSshIdle;
        if (m_connection)
        {
            rocprofvis_controller_get_uint64(m_connection, kRPVControllerRemoteStatus, 0, &remote_status);
        }

        bool terminal =
            remote_status == kRPVControllerSshCompleted || remote_status == kRPVControllerSshFailed;
        if (terminal)
        {
            // Flush buffered stdout once an execute phase finishes, mirroring the
            // old blocking path's post-loop FinalizeExecution().
            if (m_active_operation == SshOperation::Execute)
            {
                FinalizeExecution();
            }
            // The monitor will remove this op (it waits on then frees the
            // future). Clear our in-flight markers so the next phase can start;
            // the monitor-owned future and operation id are no longer ours to
            // touch. The emitted event captured the id by value, so resetting it
            // here does not affect the terminal event being built this frame.
            m_active_operation = SshOperation::None;
        }
        return remote_status;
    }

    uint64_t SshSession::BeginOperation(SshOperation operation, MonitorOperationType monitor_type,
            std::function<rocprofvis_result_t(rocprofvis_controller_future_t*)> start_fn)
    {
        if (!m_connection)
        {
            return 0;
        }

        // Only one phase may be in flight per session. Callers must let the
        // active operation reach a terminal state (or explicitly cancel it)
        // before starting the next one.
        if (m_active_operation != SshOperation::None)
        {
            ROCPROFVIS_ASSERT_MSG(false, "SshSession: an operation is already in flight");
            return 0;
        }

        rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
        ROCPROFVIS_ASSERT(future);

        rocprofvis_result_t result = start_fn(future);
        if (result != kRocProfVisResultSuccess)
        {
            rocprofvis_controller_future_free(future);
            return 0;
        }

        m_active_operation = operation;
        m_last_result      = kRocProfVisResultPending;

        // Holds the operation id captured by value into the event factory, so
        // emitted events carry the correct id even after Poll() clears the
        // in-flight markers on terminal status.
        auto id_holder = std::make_shared<uint64_t>(0);

        // Register with the monitor. The status callback drives Poll() each
        // frame; the factory emits a RemoteStatusEvent carrying this session's
        // operation id and the latest check result; terminal status removes the
        // op (the monitor waits on then frees the future). cancel_fn signals the
        // bridge so an in-flight phase can unblock and resolve its future.
        rocprofvis_handle_t* connection = m_connection;
        m_active_operation_id = AppMonitor::GetInstance()->AddOperation(
            monitor_type,
            [this]() -> uint64_t { return Poll(); },
            [this, id_holder](uint64_t status) -> std::shared_ptr<RocEvent>
            {
                return std::make_shared<RemoteStatusEvent>(
                    *id_holder, static_cast<uint32_t>(status), m_last_result);
            },
            [](uint64_t status) -> bool
            {
                return status == kRPVControllerSshCompleted || status == kRPVControllerSshFailed;
            },
            [connection]() { rocprofvis_controller_remote_cancel_prompt(connection); },
            future);

        *id_holder = m_active_operation_id;
        return m_active_operation_id;
    }

    uint64_t SshSession::StartConnect()
    {
        return BeginOperation(SshOperation::Connect, MonitorOperationType::SshConnection,
            [this](rocprofvis_controller_future_t* future) { return StartConnection(future); });
    }

    uint64_t SshSession::StartAuthenticate()
    {
        if (!m_uri)
        {
            return 0;
        }
        return BeginOperation(SshOperation::Authenticate, MonitorOperationType::SshAuthentication,
            [this](rocprofvis_controller_future_t* future) { return StartAuthentication(future); });
    }

    // if command_line is omitted, the data will be taken from m_uri
    uint64_t SshSession::StartExecute(const char* command_line)
    {
        m_pending_command = command_line ? command_line : std::string();
        bool have_cmd = command_line != nullptr;
        return BeginOperation(SshOperation::Execute, MonitorOperationType::SshConnection,
            [this, have_cmd](rocprofvis_controller_future_t* future)
            {
                return have_cmd ? StartExecution(m_pending_command.c_str(), future)
                                : StartExecution(future);
            });
    }

    // if remote_path or local_path omitted, the data will be taken from m_uri
    uint64_t SshSession::StartDownload(const char* remote_path, const char* local_path)
    {
        m_pending_remote_path = remote_path ? remote_path : std::string();
        m_pending_local_path  = local_path ? local_path : std::string();
        bool have_paths = remote_path && local_path;
        return BeginOperation(SshOperation::Download, MonitorOperationType::FileTransfer,
            [this, have_paths](rocprofvis_controller_future_t* future)
            {
                return have_paths
                    ? StartDownloadOp(m_pending_remote_path.c_str(), m_pending_local_path.c_str(), future)
                    : StartDownloadOp(future);
            });
    }

    void SshSession::CancelActiveOperation()
    {
        // Only act while a phase is genuinely in flight. After a phase reaches a
        // terminal state, Poll() clears m_active_operation and the monitor has
        // already removed (and freed) the operation, so there is nothing to do.
        if (m_active_operation == SshOperation::None)
        {
            m_active_operation_id = 0;
            return;
        }

        if (m_active_operation == SshOperation::Execute)
        {
            FinalizeExecution();
        }

        // RemoveOperation signals the bridge (cancel_fn), waits for the bound
        // future to resolve, then frees it - so the worker is never left with a
        // dangling future.
        if (m_active_operation_id != 0)
        {
            AppMonitor::GetInstance()->RemoveOperation(m_active_operation_id);
            m_active_operation_id = 0;
        }

        m_active_operation = SshOperation::None;
    }

    rocprofvis_result_t SshSession::AllocateConnection(const char* host, int port)
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (host && port > 0 && port < 65536)
        {
            uint64_t prop_count = 0;
            rocprofvis_controller_arguments_t* args = rocprofvis_controller_arguments_alloc();
            ROCPROFVIS_ASSERT(args != nullptr);
            rocprofvis_controller_array_t* output = rocprofvis_controller_array_alloc(1);
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
        rocprofvis_controller_arguments_t* args = rocprofvis_controller_arguments_alloc();
        ROCPROFVIS_ASSERT(args != nullptr);

        // Populate user/password and SSH key material used by remote authentication.
        result = rocprofvis_controller_set_string(args, kRPVControllerRemoteTypeUser, 0, user);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_string(args, kRPVControllerRemoteTypePassword, 0, password);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_string(args, kRPVControllerRemoteTypeKeyPath, 0, identity_file);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_string(args, kRPVControllerRemoteTypeKeyPassphrase, 0, passphrase);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        if (result == kRocProfVisResultSuccess)
        {
            result = rocprofvis_controller_remote_authenticate_async(future, m_connection, args);
        }
        rocprofvis_controller_arguments_free(args);
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

            rocprofvis_controller_arguments_t* args = rocprofvis_controller_arguments_alloc();
            ROCPROFVIS_ASSERT(args != nullptr);

            result = rocprofvis_controller_set_string(args, kRPVControllerRemoteTypeCommand, 0, command_line);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            if (result == kRocProfVisResultSuccess)
            {
                result = rocprofvis_controller_remote_execute_async(future, m_connection, args);
            }
            rocprofvis_controller_arguments_free(args);
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


    rocprofvis_result_t SshSession::StartDownloadOp(rocprofvis_controller_future_t* future)
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (m_connection && m_uri)
        {
            std::string remote_path = m_uri->GetRemoteResultPathString();
            std::string local_path = m_uri->GetLocalResultPathString();
            result =  StartDownloadOp(remote_path.c_str(), local_path.c_str(), future);
        }
        return result;
    }

    rocprofvis_result_t SshSession::StartDownloadOp(const char* remote_path, const char* local_path, rocprofvis_controller_future_t* future)
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (m_connection)
        {
            rocprofvis_controller_arguments_t* args = rocprofvis_controller_arguments_alloc();
            ROCPROFVIS_ASSERT(args != nullptr);

            result = rocprofvis_controller_set_string(args, kRPVControllerRemoteTypeFilePathSrc, 0, remote_path);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            result = rocprofvis_controller_set_string(args, kRPVControllerRemoteTypeFilePathDst, 0, local_path);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            uint64_t direction = 0; //download

            result = rocprofvis_controller_set_uint64(args, kRPVControllerRemoteTypeDirection, 0, direction);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            if (result == kRocProfVisResultSuccess)
            {
                result = rocprofvis_controller_remote_transfer_async(future, m_connection, args);
            }
            rocprofvis_controller_arguments_free(args);
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
        rocprofvis_controller_arguments_t* args = rocprofvis_controller_arguments_alloc();
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

        rocprofvis_controller_arguments_free(args);

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
