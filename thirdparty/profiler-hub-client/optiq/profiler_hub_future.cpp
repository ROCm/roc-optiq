#include "profiler_hub_future.hpp"
#include "profiler_hub_missing.hpp"

#include <atomic>
#include <chrono>
#include <stdexcept>

namespace profiler_hub
{

// Module-level counter – each future_t gets a unique id at construction time.
static std::atomic<uint64_t> s_next_id{1};

    // -----------------------------------------------------------------------------
    // Construction / destruction
    // -----------------------------------------------------------------------------

    future_t::future_t(progress_callback_t progress_callback, void* user_data)
    : m_progress_callback(progress_callback)
    , m_user_data(user_data)
    , m_id(s_next_id.fetch_add(1, std::memory_order_relaxed))
    , m_progress(0.0)
    , m_interrupt_status(false)
    , m_future(m_promise.get_future())
    {}

    future_t::~future_t()
    {
        // Ask a running worker to stop before we block on join.
        set_interrupted();
        if (m_worker.joinable())
            m_worker.join();
    }

    // -----------------------------------------------------------------------------
    // Synchronization
    // -----------------------------------------------------------------------------

    profiler_hub_result_t
    future_t::wait_for_completion(uint64_t timeout_ms)
    {
        const auto wait_status =
            m_future.wait_for(std::chrono::milliseconds(timeout_ms));

        if (wait_status == std::future_status::timeout)
        {
            set_interrupted();
            return profiler_hub_result_t::kProfilerHubStatusTimeout;
        }

        return m_future.get();
    }

    profiler_hub_result_t
    future_t::set_promise(profiler_hub_result_t status)
    {
        try
        {
            m_promise.set_value(status);
        }
        catch (const std::future_error&)
        {
            // Promise was already fulfilled (e.g. called twice) – report error.
            return profiler_hub_result_t::kProfilerHubStatusUnknownError;
        }
        return status;
    }

    // -----------------------------------------------------------------------------
    // Interruption
    // -----------------------------------------------------------------------------

    void
    future_t::set_interrupted()
    {
        m_interrupt_status.store(true, std::memory_order_release);
        missing_t::function("query cancellation propagation");
    }

    // -----------------------------------------------------------------------------
    // Progress reporting
    // -----------------------------------------------------------------------------

    void
    future_t::show_progress(const char* db_name,
                            double    progress_percent,
                            const char* action,
                            profiler_hub_async_status_t  status)
    {
        m_progress.store(progress_percent, std::memory_order_relaxed);

        if (m_progress_callback)
            m_progress_callback(db_name, m_id, progress_percent, status, action, nullptr);
    }

}  // namespace profiler_hub
