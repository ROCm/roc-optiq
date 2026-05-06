// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
//
// Small helper used by the database loaders that fan out work across
// multiple std::threads. The native build keeps the original concurrent
// behavior; the WebAssembly build (which is currently single-threaded and
// compiled with -fno-exceptions) instead runs the work synchronously
// because constructing a std::thread there throws and aborts.
//
// Usage: replace
//
//     threads.emplace_back(task, arg1, arg2);
//
// with
//
//     DispatchOrRunInline(threads, task, arg1, arg2);

#pragma once

#include <cstdint>
#include <thread>
#include <utility>

#ifdef __EMSCRIPTEN__
#    include <emscripten.h>
#endif

namespace RocProfVis
{
namespace DataModel
{

template <typename Container, typename Func, typename... Args>
inline void DispatchOrRunInline(Container& threads, Func task, Args&&... args)
{
#ifdef __EMSCRIPTEN__
    (void) threads;
    task(std::forward<Args>(args)...);
#else
    threads.emplace_back(std::move(task), std::forward<Args>(args)...);
#endif
}

inline void YieldToBrowser()
{
#ifdef __EMSCRIPTEN__
    emscripten_sleep(0);
#endif
}

inline void YieldToBrowserEvery(uint32_t count, uint32_t interval = 4096)
{
#ifdef __EMSCRIPTEN__
    if(interval > 0 && (count % interval) == 0)
    {
        YieldToBrowser();
    }
#else
    (void) count;
    (void) interval;
#endif
}

}  // namespace DataModel
}  // namespace RocProfVis
