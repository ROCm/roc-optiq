#include "rocprofvis_controller_remote_future.h"
#include "rocprofvis_core_assert.h"

namespace RocProfVis
{
namespace Controller
{

FutureRemote::FutureRemote()
    : Handle(__kRPVControllerFuturePropertiesFirst,
             __kRPVControllerFuturePropertiesLast)
    , m_user_callback_type(kRPVControllerSshCallbackIdle)
    , m_user_prompt_req(std::monostate{})
{
}

rocprofvis_controller_object_type_t FutureRemote::GetType()
{
    return kRPVControllerObjectTypeFuture; 
}

//----------------------------------------------------------
// Wait (callback bridge)
//----------------------------------------------------------
rocprofvis_result_t FutureRemote::Wait()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_user_callback_type != kRPVControllerSshCallbackIdle)
    {
        return kRocProfVisResultSshCommunicationCallback;
    }

    return kRocProfVisResultSuccess;
}

//----------------------------------------------------------
// Actions (called by Future / SSH layer)
//----------------------------------------------------------

void FutureRemote::AskPrompts(const rocprofvis_controller_user_prompt_t& req)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_user_prompt_req = req;
    m_user_callback_type = kRPVControllerSshCallbackAuthRequest;
}

void FutureRemote::AddStdOut(const char* buffer, uint64_t count)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stdout.append(buffer, count);
    m_user_callback_type = kRPVControllerSshCallbackExecuteStdOut;
}

void FutureRemote::SaveError(const std::string& err)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_error_str = err;
}

void FutureRemote::SetFileStat(std::string name, uint64_t size, uint64_t time, uint64_t downloaded)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_remote_file_stat.name = std::move(name);
    m_remote_file_stat.size = size;
    m_remote_file_stat.time = time;
    m_remote_file_stat, downloaded = downloaded;

    m_user_callback_type = kRPVControllerSshCallbackDownloadSarted;
}

void FutureRemote::SetDownloaded(uint64_t size)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_remote_file_stat.downloaded += size;
    m_user_callback_type = kRPVControllerSshCallbackDownloadProgress;
}

//----------------------------------------------------------
// Property handlers
//----------------------------------------------------------

rocprofvis_result_t FutureRemote::GetUInt64(
    rocprofvis_property_t property,
    uint64_t index,
    uint64_t* value)
{
    (void)index;

    if (!value)
        return kRocProfVisResultInvalidArgument;

    std::lock_guard<std::mutex> lock(m_mutex);

    switch (property)
    {
        case kRPVControllerFutureRemoteFileSize:
            *value = m_remote_file_stat.size;
            return kRocProfVisResultSuccess;

        case kRPVControllerFutureRemoteFileTime:
            *value = m_remote_file_stat.time;
            return kRocProfVisResultSuccess;

        case kRPVControllerFutureRemoteDownloaded:
            *value = m_remote_file_stat.downloaded;
            return kRocProfVisResultSuccess;

        case kRPVControllerFutureRemoteCallbackType:
            *value = m_user_callback_type;
            m_user_callback_type = kRPVControllerSshCallbackIdle;
            return kRocProfVisResultSuccess;

        case kRPVControllerFutureUserPromptType:
            if (std::holds_alternative<PromptRequest>(m_user_prompt_req))
            {
                *value = kRPVControllerUserPromptTypeGeneric;
                return kRocProfVisResultSuccess;
            }
            if (std::holds_alternative<HostKeyRequest>(m_user_prompt_req))
            {
                *value = kRPVControllerUserPromptTypeHostKey;
                return kRocProfVisResultSuccess;
            }
            return kRocProfVisResultNotLoaded;

        case kRPVControllerFutureUserGenericNumPrompts:
            if (auto* req = std::get_if<PromptRequest>(&m_user_prompt_req))
            {
                *value = req->prompts.size();
                return kRocProfVisResultSuccess;
            }
            return kRocProfVisResultNotLoaded;

        case kRPVControllerFutureUserGenericPromptEchoIndexed:
            if (auto* req = std::get_if<PromptRequest>(&m_user_prompt_req))
            {
                if (index < req->prompts.size())
                {
                    *value = req->prompts[index].echo;
                    return kRocProfVisResultSuccess;
                }
            }
            return kRocProfVisResultNotLoaded;

        case kRPVControllerFutureUserHostKeyPromptPort:
            if (auto* req = std::get_if<HostKeyRequest>(&m_user_prompt_req))
            {
                *value = req->port;
                return kRocProfVisResultSuccess;
            }
            return kRocProfVisResultNotLoaded;

        case kRPVControllerFutureUserHostKeyPromptState:
            if (auto* req = std::get_if<HostKeyRequest>(&m_user_prompt_req))
            {
                *value = static_cast<uint64_t>(req->state);
                return kRocProfVisResultSuccess;
            }
            return kRocProfVisResultNotLoaded;

        default:
            return kRocProfVisResultInvalidArgument;
    }
}

rocprofvis_result_t FutureRemote::GetString(
    rocprofvis_property_t property,
    uint64_t index,
    char* value,
    uint32_t* length)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    switch (property)
    {
        case kRPVControllerFutureRemoteExecuteStdOut:
        {
            std::string tmp = m_stdout;
            auto result = GetStdStringImpl(value, length, tmp);

            if (value != nullptr)
                m_stdout.clear();

            return result;
        }

        case kRPVControllerFutureRemoteLastError:
            return GetStdStringImpl(value, length, m_error_str);

        case kRPVControllerFutureRemoteFileName:
            return GetStdStringImpl(value, length, m_remote_file_stat.name);

        case kRPVControllerFutureUserGenericPromptName:
            if (auto* req = std::get_if<PromptRequest>(&m_user_prompt_req))
                return GetStdStringImpl(value, length, req->name);
            return kRocProfVisResultNotLoaded;

        case kRPVControllerFutureUserGenericPromptInstruction:
            if (auto* req = std::get_if<PromptRequest>(&m_user_prompt_req))
                return GetStdStringImpl(value, length, req->instruction);
            return kRocProfVisResultNotLoaded;

        case kRPVControllerFutureUserGenericPromptTextIndexed:
            if (auto* req = std::get_if<PromptRequest>(&m_user_prompt_req))
                return GetStdStringImpl(value, length, req->prompts[index].text);
            return kRocProfVisResultNotLoaded;

        case kRPVControllerFutureUserHostKeyPromptHost:
            if (auto* req = std::get_if<HostKeyRequest>(&m_user_prompt_req))
                return GetStdStringImpl(value, length, req->host);
            return kRocProfVisResultNotLoaded;

        case kRPVControllerFutureUserHostKeyPromptFinderprint:
            if (auto* req = std::get_if<HostKeyRequest>(&m_user_prompt_req))
                return GetStdStringImpl(value, length, req->fingerprint_sha256_b64);
            return kRocProfVisResultNotLoaded;

        case kRPVControllerFutureUserHostKeyPromptEncryptType:
            if (auto* req = std::get_if<HostKeyRequest>(&m_user_prompt_req))
                return GetStdStringImpl(value, length, req->key_type);
            return kRocProfVisResultNotLoaded;

        default:
            return kRocProfVisResultInvalidArgument;
    }
}

} // namespace Controller
} // namespace RocProfVis
