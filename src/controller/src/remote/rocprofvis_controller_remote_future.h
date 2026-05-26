#pragma once

#include "rocprofvis_controller_handle.h"
#include "remote/rocprofvis_controller_remote_structs.h"
#include "rocprofvis_c_interface.h"

#include <atomic>
#include <mutex>
#include <string>

namespace RocProfVis
{
namespace Controller
{

class FutureRemote : public Handle
{
public:
    FutureRemote();

    rocprofvis_controller_object_type_t GetType() final;

    // Property handlers
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property,
                                 uint64_t index,
                                 uint64_t* value) final;

    rocprofvis_result_t GetString(rocprofvis_property_t property,
                                 uint64_t index,
                                 char* value,
                                 uint32_t* length) final;

    // Behaviour
    rocprofvis_result_t Wait();

    // Actions (called by Future / SSH layer)
    void AskPrompts(const rocprofvis_controller_user_prompt_t& req);
    void AddStdOut(const char* buffer, uint64_t count);
    void SaveError(const std::string& err);
    void SetFileStat(std::string name, uint64_t size, uint64_t time, uint64_t downloaded);
    void SetDownloaded(uint64_t size);

private:
    std::atomic<rocprofvis_controller_remote_callback_t> m_user_callback_type;
    rocprofvis_controller_user_prompt_t m_user_prompt_req;

    std::string m_stdout;
    std::string m_error_str;

    FileStat m_remote_file_stat;

    std::mutex m_mutex;
};

} // namespace Controller
} // namespace RocProfVis