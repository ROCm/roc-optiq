#pragma once

#include "profiler_hub_interface_types.h"

#include <atomic>
#include <cstdint>
#include <future>
#include <thread>

// These types are assumed to be declared in the project's public API headers.
// Included here symbolically – replace with the real include path.
//   progress_callback_t
//   timeout_ms_t
//   status_t
//   charptr_t
//   result_t


namespace profiler_hub
{

    typedef void ( *progress_callback_t)(
        const char * file_name,
        uint64_t future_id,
        double progress_percent, 
        profiler_hub_async_status_t status, 
        const char* status_message,
        void* user_data
        );
 
    // Represents a handle to an asynchronous database operation.
    // The caller creates a future_t, optionally attaches a progress callback,
    // moves a worker std::thread into it, and later calls wait_for_completion().
    // The worker thread is responsible for calling set_promise() when it finishes
    // and show_progress() as it advances.
    class future_t
    {
    public:
        // Constructs a future with an optional progress-reporting callback.
        // @param progress_callback  – function invoked on each progress update; may be nullptr.
        // @param user_data          – opaque pointer forwarded to every callback invocation.
        future_t(progress_callback_t progress_callback,
                 void*              user_data = nullptr);

        // Joins the worker thread if it is still running, then destroys the object.
        ~future_t();

        // Non-copyable; move is intentionally left undefined – ownership is exclusive.
        future_t(const future_t&)            = delete;
        future_t& operator=(const future_t&) = delete;

        // -------------------------------------------------------------------------
        // Accessors
        // -------------------------------------------------------------------------

        // @return the progress callback supplied at construction (may be nullptr).
        progress_callback_t get_progress_callback() const
        {
            return m_progress_callback;
        }

        // @return the unique numeric identifier assigned to this future at construction.
        uint64_t get_id() const { return m_id; }

        // @return current operation progress in the range [0.0, 100.0].
        double get_progress() const { return m_progress.load(std::memory_order_relaxed); }

        // -------------------------------------------------------------------------
        // Worker thread management
        // -------------------------------------------------------------------------

        // Takes ownership of the worker thread that carries out the async operation.
        // Must be called before the first wait_for_completion() call.
        // @param thread – r-value reference to the thread to adopt.
        void set_worker(std::thread&& thread) { m_worker = std::move(thread); }

        // @return true while the worker thread is alive (joinable).
        bool is_working() const { return m_worker.joinable(); }

        // -------------------------------------------------------------------------
        // Synchronisation
        // -------------------------------------------------------------------------

        // Blocks until the worker completes or the timeout elapses.
        // On timeout the interrupted flag is set so the worker can react and exit early.
        // @param timeout_ms – maximum wait time in milliseconds.
        // @return the operation result reported by the worker, or a timeout status.
        profiler_hub_result_t wait_for_completion(uint64_t timeout_ms);

        // Called by the worker thread to deliver its final operation status.
        // Safe to call at most once; subsequent calls are silently ignored.
        // @param status – result of the completed operation.
        // @return the same status value that was stored, or an error code on failure.
        profiler_hub_result_t set_promise(profiler_hub_result_t status);


        // -------------------------------------------------------------------------
        // Interruption
        // -------------------------------------------------------------------------

        // @return true if the operation was cut short by wait_for_completion() timeout.
        bool is_interrupted() const
        {
            return m_interrupt_status.load(std::memory_order_acquire);
        }

        // Signals the worker thread that it should stop as soon as possible.
        // Idempotent – safe to call multiple times.
        void set_interrupted();

        // -------------------------------------------------------------------------
        // Progress reporting
        // -------------------------------------------------------------------------

        // Updates the stored progress value and, if a callback was supplied,
        // forwards the event to the caller.
        // @param db_name – path to the database file being processed.
        // @param step    – progress percentage of the current operation step.
        // @param action  – human-readable description of the ongoing operation.
        // @param status  – current operation status.
        void show_progress(const char* db_name,
                           double    weight,
                           const char* action,
                           profiler_hub_async_status_t  status);

    private:
        progress_callback_t    m_progress_callback;
        void*                  m_user_data;
        const uint64_t         m_id;
        std::atomic<double>    m_progress;
        std::atomic<bool>      m_interrupt_status;
        std::thread            m_worker;
        std::promise<profiler_hub_result_t> m_promise;
        std::future<profiler_hub_result_t>  m_future;
    };

}  // namespace profiler_hub
