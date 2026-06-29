// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_core.h"

#include <vector>

#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/ringbuffer_sink.h"
#if defined(_WIN32)
#include "spdlog/sinks/msvc_sink.h"
#include "spdlog/sinks/wincolor_sink.h"
#endif

#include "spdlog/details/circular_q.h"
#include "spdlog/details/log_msg_buffer.h"
#include "spdlog/details/null_mutex.h"
#include "spdlog/pattern_formatter.h"
#include "spdlog/sinks/base_sink.h"

#include <mutex>
#include <string>
#include <utility>

namespace RocProfVis {
namespace Core {

template <typename Mutex>
class BufferSink final : public spdlog::sinks::base_sink<Mutex>
{
public:
    explicit BufferSink() {}

    std::vector<spdlog::details::log_msg_buffer> Raw()
    {
        std::lock_guard<Mutex> lock(spdlog::sinks::base_sink<Mutex>::mutex_);
        std::vector<spdlog::details::log_msg_buffer> output = m_elements;
        m_elements.clear();
        return output;
    }

    struct StructuredEntry
    {
        int         level;
        std::string timestamp;
        std::string message;
    };

    std::vector<StructuredEntry> Structured()
    {
        std::lock_guard<Mutex> lock(spdlog::sinks::base_sink<Mutex>::mutex_);
        // Time-only pattern with an empty end-of-line so the timestamp column is
        // clean; the message body comes straight from the record payload, so it
        // carries no level/logger prefix.
        spdlog::pattern_formatter time_formatter("%H:%M:%S.%e",
                                                 spdlog::pattern_time_type::local,
                                                 std::string());
        std::vector<StructuredEntry> ret;
        ret.reserve(m_elements.size());
        for (auto& entry : m_elements)
        {
            spdlog::memory_buf_t time_buf;
            time_formatter.format(entry, time_buf);
            ret.push_back(StructuredEntry{
                static_cast<int>(entry.level), SPDLOG_BUF_TO_STRING(time_buf),
                std::string(entry.payload.data(), entry.payload.size()) });
        }
        m_elements.clear();
        return ret;
    }

protected:
    void sink_it_(const spdlog::details::log_msg &msg) override
    {
        m_elements.push_back(spdlog::details::log_msg_buffer{msg});

        // Bound memory if nothing drains the sink (e.g. UI idle); keep newest.
        if (m_elements.size() > MAX_BUFFERED_ENTRIES)
        {
            m_elements.erase(m_elements.begin(),
                             m_elements.begin() + (m_elements.size() / 2));
        }
    }
    void flush_() override {}

private:
    // Hard cap on retained records when the buffer is not being drained.
    static constexpr size_t MAX_BUFFERED_ENTRIES = 100000;

    std::vector<spdlog::details::log_msg_buffer> m_elements;
};

using BufferSinkMT = BufferSink<std::mutex>;
}
}

static std::weak_ptr<RocProfVis::Core::BufferSinkMT> g_rpv_log_ringbuffer;
static std::string                                   g_rpv_log_path;

extern "C"
{
    void rocprofvis_core_enable_log(const char* log_path, int log_level)
    {
        static bool is_inited = false;
        if (!is_inited)
        {
            is_inited = true;

            if (log_path)
            {
                g_rpv_log_path = log_path;
            }

            std::vector<std::shared_ptr<spdlog::sinks::sink>> sinks;
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path, true);
            file_sink->set_level(spdlog::level::debug);
            sinks.push_back(file_sink);

            auto buffer_sink = std::make_shared<RocProfVis::Core::BufferSinkMT>();
            buffer_sink->set_level(spdlog::level::debug);
            sinks.push_back(buffer_sink);

#if defined(_WIN32)
            auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
            msvc_sink->set_level(spdlog::level::debug);
            sinks.push_back(msvc_sink);

            auto console_sink = std::make_shared<spdlog::sinks::wincolor_stderr_sink_mt>(
                spdlog::color_mode::automatic);
            console_sink->set_level(spdlog::level::debug);
            sinks.push_back(console_sink);
#else
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::debug);
            sinks.push_back(console_sink);
#endif

            g_rpv_log_ringbuffer = buffer_sink;

            auto new_logger = std::make_shared<spdlog::logger>(std::move(std::string("roc-optiq-log")), sinks.begin(), sinks.end());
            new_logger->set_level(spdlog::level::debug);
            spdlog::details::registry::instance().initialize_logger(new_logger);

            spdlog::set_default_logger(new_logger);
            spdlog::set_level(static_cast<spdlog::level::level_enum>(log_level));
        }
    }

    void rocprofvis_core_get_log_entries_ex(void*                               user_ptr,
                                            rocprofvis_core_process_log_entry_t handler)
    {
        auto sink = g_rpv_log_ringbuffer.lock();
        if(handler && sink)
        {
            auto array = sink->Structured();
            for (auto& entry : array)
            {
                handler(user_ptr, entry.level, entry.timestamp.c_str(),
                        entry.message.c_str());
            }
        }
    }

    const char* rocprofvis_core_get_log_path(void)
    {
        return g_rpv_log_path.c_str();
    }
}
