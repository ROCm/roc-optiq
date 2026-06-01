#include "rocprofvis_controller_ssh_bridge.h"
#include "rocprofvis_core_assert.h"

namespace RocProfVis
{
namespace Controller
{

SshBridge::SshBridge()
    : Handle(__kRPVControllerRemotePropertiesFirst,
             __kRPVControllerRemotePropertiesLast)
 
{
    m_ssh_status.now = m_ssh_status.next = kRPVControllerSshIdle;
}

rocprofvis_controller_object_type_t SshBridge::GetType()
{
    return kRPVSshBridge; 
}

void SshBridge::SetStatus(rocprofvis_controller_remote_status_t status)
{
    m_ssh_status.now = m_ssh_status.next = status;
}

//----------------------------------------------------------
// Actions (called by Remote / SSH layer)
//----------------------------------------------------------


void SshBridge::AddStdOut(const char* buffer, uint64_t count, bool last)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stdout.append(buffer, count);
    m_ssh_status.now = kRPVControllerSshExecuteStdOut;
    m_ssh_status.next = last ? kRPVControllerSshCompleted : kRPVControllerSshExecuting;
}

void SshBridge::SaveError(const std::string& err)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_error_str = err;
    m_ssh_status.next = kRPVControllerSshFailed;
}

void SshBridge::SetFileStat(std::string name, uint64_t size, uint64_t time, uint64_t downloaded)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_remote_file_stat.name = std::move(name);
    m_remote_file_stat.size = size;
    m_remote_file_stat.time = time;
    m_remote_file_stat.downloaded = downloaded;

    m_ssh_status.now = kRPVControllerSshDownloadStarted;
    m_ssh_status.next = m_remote_file_stat.size == m_remote_file_stat.downloaded ? kRPVControllerSshCompleted : kRPVControllerSshDownloading;
}

void SshBridge::SetDownloaded(uint64_t size)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_remote_file_stat.downloaded += size;
    m_ssh_status.now = kRPVControllerSshDownloadProgress;
    m_ssh_status.next = kRPVControllerSshDownloading;
}

//----------------------------------------------------------
// Property handlers
//----------------------------------------------------------

rocprofvis_result_t SshBridge::GetUInt64(
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
        case kRPVControllerRemoteFileSize:
            *value = m_remote_file_stat.size;
            return kRocProfVisResultSuccess;

        case kRPVControllerRemoteFileTime:
            *value = m_remote_file_stat.time;
            return kRocProfVisResultSuccess;

        case kRPVControllerRemoteDownloaded:
            *value = m_remote_file_stat.downloaded;
            return kRocProfVisResultSuccess;

        case kRPVControllerRemoteStatus:
            *value  = m_ssh_status.now;
            m_ssh_status.now = m_ssh_status.next.load();
            return kRocProfVisResultSuccess;

        case kRPVControllerRemoteUserPromptType:
            if (std::holds_alternative<PromptRequest>(m_pending))
            {
                *value = kRPVControllerUserPromptTypeGeneric;
                return kRocProfVisResultSuccess;
            }
            if (std::holds_alternative<HostKeyRequest>(m_pending))
            {
                *value = kRPVControllerUserPromptTypeHostKey;
                return kRocProfVisResultSuccess;
            }
            return kRocProfVisResultNotLoaded;

        case kRPVControllerRemoteUserGenericNumPrompts:
            if (auto* req = std::get_if<PromptRequest>(&m_pending))
            {
                *value = req->prompts.size();
                return kRocProfVisResultSuccess;
            }
            return kRocProfVisResultNotLoaded;

        case kRPVControllerRemoteUserGenericPromptEchoIndexed:
            if (auto* req = std::get_if<PromptRequest>(&m_pending))
            {
                if (index < req->prompts.size())
                {
                    *value = req->prompts[index].echo;
                    return kRocProfVisResultSuccess;
                }
            }
            return kRocProfVisResultNotLoaded;

        case kRPVControllerRemoteUserHostKeyPromptPort:
            if (auto* req = std::get_if<HostKeyRequest>(&m_pending))
            {
                *value = req->port;
                return kRocProfVisResultSuccess;
            }
            return kRocProfVisResultNotLoaded;

        case kRPVControllerRemoteUserHostKeyPromptState:
            if (auto* req = std::get_if<HostKeyRequest>(&m_pending))
            {
                *value = static_cast<uint64_t>(req->state);
                return kRocProfVisResultSuccess;
            }
            return kRocProfVisResultNotLoaded;

        default:
            return kRocProfVisResultInvalidArgument;
    }
}

rocprofvis_result_t SshBridge::GetString(
    rocprofvis_property_t property,
    uint64_t index,
    char* value,
    uint32_t* length)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    switch (property)
    {
        case kRPVControllerRemoteExecuteStdOut:
        {
            std::string tmp = m_stdout;
            auto result = GetStdStringImpl(value, length, tmp);

            if (value != nullptr)
                m_stdout.clear();

            return result;
        }

        case kRPVControllerRemoteLastError:
            return GetStdStringImpl(value, length, m_error_str);

        case kRPVControllerRemoteFileName:
            return GetStdStringImpl(value, length, m_remote_file_stat.name);

        case kRPVControllerRemoteUserGenericPromptName:
            if (auto* req = std::get_if<PromptRequest>(&m_pending))
                return GetStdStringImpl(value, length, req->name);
            return kRocProfVisResultNotLoaded;

        case kRPVControllerRemoteUserGenericPromptInstruction:
            if (auto* req = std::get_if<PromptRequest>(&m_pending))
                return GetStdStringImpl(value, length, req->instruction);
            return kRocProfVisResultNotLoaded;

        case kRPVControllerRemoteUserGenericPromptTextIndexed:
            if (auto* req = std::get_if<PromptRequest>(&m_pending))
                return GetStdStringImpl(value, length, req->prompts[index].text);
            return kRocProfVisResultNotLoaded;

        case kRPVControllerRemoteUserHostKeyPromptHost:
            if (auto* req = std::get_if<HostKeyRequest>(&m_pending))
                return GetStdStringImpl(value, length, req->host);
            return kRocProfVisResultNotLoaded;

        case kRPVControllerRemoteUserHostKeyPromptFinderprint:
            if (auto* req = std::get_if<HostKeyRequest>(&m_pending))
                return GetStdStringImpl(value, length, req->fingerprint_sha256_b64);
            return kRocProfVisResultNotLoaded;

        case kRPVControllerRemoteUserHostKeyPromptEncryptType:
            if (auto* req = std::get_if<HostKeyRequest>(&m_pending))
                return GetStdStringImpl(value, length, req->key_type);
            return kRocProfVisResultNotLoaded;

        default:
            return kRocProfVisResultInvalidArgument;
    }
}

std::optional<std::vector<std::string>>
SshBridge::AskPrompts(const PromptRequest& req)
{
    std::unique_lock<std::mutex> lk(m_mutex);
    m_pending = req;
    m_ssh_status.now = kRPVControllerSshAuthRequest;
    m_ssh_status.next = kRPVControllerSshAuthenticating;
    m_prompt_responses.reset();
    m_worker_cv.wait(lk, [&] { return m_prompt_responses.has_value() || m_cancelled; });
    m_pending = std::monostate{};
    if(m_cancelled)
    {
        return std::nullopt;
    }
    auto out = std::move(*m_prompt_responses);
    m_prompt_responses.reset();
    return out;
}

std::optional<HostKeyDecision>
SshBridge::AskHostKey(const HostKeyRequest& req)
{
    std::unique_lock<std::mutex> lk(m_mutex);
    m_pending = req;
    m_ssh_status.now = kRPVControllerSshAuthRequest;
    m_ssh_status.next = kRPVControllerSshAuthenticating;
    m_hostkey_decision.reset();
    m_worker_cv.wait(lk, [&] { return m_hostkey_decision.has_value() || m_cancelled; });
    m_pending = std::monostate{};
    if(m_cancelled)
    {
        return std::nullopt;
    }
    auto d = *m_hostkey_decision;
    m_hostkey_decision.reset();
    return d;
}

void SshBridge::SubmitPromptResponses(std::vector<std::string> responses)
{
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if(!std::holds_alternative<PromptRequest>(m_pending)) return;
        m_prompt_responses = std::move(responses);
    }
    m_worker_cv.notify_all();
}

void SshBridge::SubmitHostKeyDecision(HostKeyDecision decision)
{
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if(!std::holds_alternative<HostKeyRequest>(m_pending)) return;
        m_hostkey_decision = decision;
    }
    m_worker_cv.notify_all();
}

void SshBridge::Cancel()
{
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_cancelled = true;
    }
    m_worker_cv.notify_all();
}

void SshBridge::Reset()
{
    std::lock_guard<std::mutex> lk(m_mutex);
    m_pending = std::monostate{};
    m_cancelled = false;
    m_prompt_responses.reset();
    m_hostkey_decision.reset();
}

} // namespace Controller
} // namespace RocProfVis
