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

// Bridge between SSH worker threads (producers) and the UI (observer).
//
// Status / ownership contract:
//   - Worker/controller threads OWN all status transitions and the payload
//     buffers (m_stdout, m_remote_file_stat, m_remote_dir_info). They set
//     m_ssh_status to the value they want observed.
//   - Reading kRPVControllerRemoteStatus from the UI is a pure, NON-MUTATING
//     observation. The UI never advances the status; no worker thread ever
//     blocks waiting for the UI to observe a status.
//   - A terminal status (Completed / Failed) is latched: once set it stays set
//     until Reset() (used for connection reuse), so it can never be missed.
//   - The ONLY UI-blocking dependency is the prompt / host-key rendezvous
//     (AskPrompts / AskHostKey) which blocks the worker on m_worker_cv until
//     the UI submits responses / a decision (or cancels). That is a genuine
//     data dependency, separate from the status channel.
class SshBridge : public Handle
{
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

    std::atomic<rocprofvis_controller_remote_status_t>   m_ssh_status;
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