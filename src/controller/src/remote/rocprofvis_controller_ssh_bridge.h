#pragma once

#include "rocprofvis_controller_handle.h"
#include "remote/rocprofvis_controller_remote_structs.h"
#include "rocprofvis_c_interface.h"

#include <atomic>
#include <mutex>
#include <string>
#include <condition_variable>
#include <optional>
#include <variant>
#include <vector>

namespace RocProfVis
{
namespace Controller
{

class SshBridge : public Handle
{
    struct SshStatus
    {
        std::atomic<rocprofvis_controller_remote_status_t> now;
        std::atomic<rocprofvis_controller_remote_status_t> next;
    };
public:
    SshBridge();

    rocprofvis_controller_object_type_t GetType() final;

    // Property handlers
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property,
                                 uint64_t index,
                                 uint64_t* value) final;

    rocprofvis_result_t GetString(rocprofvis_property_t property,
                                 uint64_t index,
                                 char* value,
                                 uint32_t* length) final;

    // Status
    void SetStatus(rocprofvis_controller_remote_status_t status);

    // Actions (called by SSH layer)
    void AddStdOut(const char* buffer, uint64_t count);
    void SaveError(const std::string& err);
    void SetFileStat(std::string name, uint64_t size, uint64_t time, uint64_t downloaded);
    void SetDownloaded(uint64_t size);
    void SetFileInfo(std::string name, uint64_t size, uint64_t time, uint64_t attrs);

    std::optional<std::vector<std::string>> AskPrompts(const PromptRequest& req);
    std::optional<HostKeyDecision>          AskHostKey(const HostKeyRequest& req);

    // UI-side.
    void SubmitPromptResponses(std::vector<std::string> responses);
    void SubmitHostKeyDecision(HostKeyDecision decision);
    void Cancel();
    void Reset();
    void Clear();

private:

    SshStatus                                            m_ssh_status;
    rocprofvis_controller_user_prompt_t                  m_pending;
    std::string                                          m_stdout;
    std::string                                          m_error_str;
    std::vector<FileStat>                                m_remote_file_stat;
    std::vector<FileStat>                                m_remote_dir_info;
    std::mutex                                           m_mutex;
    std::condition_variable                              m_worker_cv;
    bool                                                 m_cancelled = false;
    std::optional<std::vector<std::string>>              m_prompt_responses;
    std::optional<HostKeyDecision>                       m_hostkey_decision;
};


} // namespace Controller
} // namespace RocProfVis