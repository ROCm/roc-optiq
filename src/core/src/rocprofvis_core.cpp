// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

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
#include "spdlog/sinks/base_sink.h"

#include <mutex>
#include <string>
#include <vector>

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

    std::vector<std::string> Formatted()
    {
        std::lock_guard<Mutex> lock(spdlog::sinks::base_sink<Mutex>::mutex_);
        std::vector<std::string> ret;
        ret.reserve(m_elements.size());
        for (auto& entry : m_elements)
        {
            spdlog::memory_buf_t formatted;
            spdlog::sinks::base_sink<Mutex>::formatter_->format(entry, formatted);
            ret.push_back(SPDLOG_BUF_TO_STRING(formatted));
        }
        m_elements.clear();
        return ret;
    }

protected:
    void sink_it_(const spdlog::details::log_msg &msg) override
    {
        m_elements.push_back(spdlog::details::log_msg_buffer{msg});
    }
    void flush_() override {}

private:
    std::vector<spdlog::details::log_msg_buffer> m_elements;
};

using BufferSinkMT = BufferSink<std::mutex>;
}
}

static std::weak_ptr<RocProfVis::Core::BufferSinkMT> g_rpv_log_ringbuffer;

extern "C"
{
    void rocprofvis_core_enable_log(const char* log_path)
    {
        static bool is_inited = false;
        if (!is_inited)
        {
            is_inited = true;

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

            auto new_logger = std::make_shared<spdlog::logger>(std::move(std::string("rocprofvis-log")), sinks.begin(), sinks.end());
            new_logger->set_level(spdlog::level::debug);
            spdlog::details::registry::instance().initialize_logger(new_logger);

            spdlog::set_default_logger(new_logger);
            spdlog::set_level(spdlog::level::debug);
        }
    }

    void rocprofvis_core_get_log_entries(void* user_ptr, rocprofvis_core_process_log_t handler)
    {
        auto sink = g_rpv_log_ringbuffer.lock();
        if(handler && sink)
        {
            auto array = sink->Formatted();
            for (auto entry : array)
            {
                handler(user_ptr, entry.c_str());
            }
        }
    }
}
