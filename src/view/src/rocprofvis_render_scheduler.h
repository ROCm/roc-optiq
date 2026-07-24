// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <chrono>

namespace RocProfVis
{
namespace View
{

// Keeps the lazy render loop awake. The app renders on demand and otherwise
// sleeps until the next OS event; anything that animates or does render-driven
// work asks here to keep rendering. Requests are level-triggered and
// self-expiring, so there is nothing to release:
//   - RequestRender(): render one more frame; call every frame while active.
//   - RequestRenderUntil() / RequestRenderForSeconds(): render until a deadline.
class RenderScheduler
{
public:
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    static RenderScheduler& GetInstance()
    {
        static RenderScheduler instance;
        return instance;
    }

    RenderScheduler(const RenderScheduler&) = delete;
    void operator=(const RenderScheduler&)  = delete;

    void RequestRender() { m_render_requested = true; }

    // Extends, never shortens, the current deadline.
    void RequestRenderUntil(TimePoint deadline)
    {
        if(deadline > m_render_deadline)
        {
            m_render_deadline = deadline;
        }
    }

    void RequestRenderForSeconds(double seconds)
    {
        RequestRenderUntil(Clock::now() + std::chrono::duration_cast<Clock::duration>(
                                              std::chrono::duration<double>(seconds)));
    }

    // Clears the per-frame request; call once at the top of each frame. The
    // deadline is not cleared - it expires on its own.
    void BeginFrame() { m_render_requested = false; }

    bool WantsRender() const
    {
        return m_render_requested || Clock::now() < m_render_deadline;
    }

private:
    RenderScheduler() = default;

    bool      m_render_requested = false;
    TimePoint m_render_deadline{};
};

}  // namespace View
}  // namespace RocProfVis
