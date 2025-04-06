// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprof_vis_core.h"

#include <vector>

#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/ringbuffer_sink.h"

static std::weak_ptr<spdlog::sinks::ringbuffer_sink_mt> g_rpv_log_ringbuffer;

extern "C"
{
    void rocprofvis_core_enable_log(void)
    {
        static bool is_inited = false;
        if (!is_inited)
        {
            is_inited = true;

            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::debug);

            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("log.txt", true);
            file_sink->set_level(spdlog::level::debug);

            auto buffer_sink = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(1000);
            buffer_sink->set_level(spdlog::level::debug);

            g_rpv_log_ringbuffer = buffer_sink;

            std::vector<std::shared_ptr<spdlog::sinks::sink>> sinks;
            sinks.push_back(console_sink);
            sinks.push_back(file_sink);
            sinks.push_back(buffer_sink);

            auto new_logger = std::make_shared<spdlog::logger>(std::move(std::string("rocprofvis-log")), sinks.begin(), sinks.end());
            new_logger->set_level(spdlog::level::debug);
            spdlog::details::registry::instance().initialize_logger(new_logger);

            spdlog::set_default_logger(new_logger);
            spdlog::set_level(spdlog::level::debug);
        }
    }

    void rocprofvis_core_get_log_entries(uint32_t num, void* user_ptr, rocprofvis_core_process_log_t handler)
    {
        auto sink = g_rpv_log_ringbuffer.lock();
        if(num && handler && sink)
        {
            auto array = sink->last_formatted(num);
            for (auto entry : array)
            {
                handler(user_ptr, entry.c_str());
            }
        }
    }
}
