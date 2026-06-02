// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_types.h"
#include "rocprofvis_c_interface_types.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_events.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace RocProfVis
{
namespace View
{

enum class MonitorOperationType
{
    SshConnection,
    SshAuthentication,
    ProfilerSession,
    FileTransfer,
};

// A single long-running operation tracked by the AppMonitor. The monitor is
// deliberately generic: it does not know how to read a session's status, so
// each operation supplies a status callback and an event factory. When the
// status callback reports a value different from the previous frame, the
// monitor builds an event via the factory and queues it on the EventManager
// for dispatch on the main thread.
struct MonitorOperation
{
    // Reads the current status code of the operation (e.g. profiler state, or
    // rocprofvis_controller_remote_status_t for SSH). Returns the raw status as
    // a uint64_t so the same struct serves every operation type.
    using StatusFn = std::function<uint64_t()>;

    // Builds the event to queue when the status changes. Receives the new
    // status so a single factory can emit the appropriate typed event.
    using EventFactory = std::function<std::shared_ptr<RocEvent>(uint64_t status)>;

    // Reports whether the operation has reached a terminal state and should be
    // removed from the monitor after its final event is queued.
    using IsTerminalFn = std::function<bool(uint64_t status)>;

    // Optional. Invoked by the monitor when an operation is asked to cancel
    // before its future has resolved. It must signal the underlying work to
    // stop so the bound future can resolve promptly (e.g. cancel an SSH prompt
    // wait). The monitor never blocks on the future; it reaps the operation in
    // a later Update() once the future has resolved.
    using CancelFn = std::function<void()>;

    // Frees one worker-touched resource. Closures MUST capture raw resource
    // pointers BY VALUE (never the owning session's `this`) so they remain
    // valid after the session is destroyed. The monitor runs an operation's
    // teardowns in registration order (FIFO) only after its future has
    // resolved, so no resource is freed while a background job still uses it.
    using TeardownFn = std::function<void()>;

    uint64_t             operation_id;    // unique id of the operation
    MonitorOperationType operation_type;  // type of operation
    StatusFn             status_fn;        // reads the current status (run every frame)
    EventFactory         event_factory;    // builds the status-changed event
    IsTerminalFn         is_terminal_fn;   // detects terminal status
    CancelFn             cancel_fn;        // signals the work to stop (optional)
    // Teardown closures run FIFO when the operation is reaped (after its future
    // resolves). Typically [0] frees the future; later entries free additional
    // resources transferred via AppendTeardown (e.g. an SSH connection).
    std::vector<TeardownFn> teardowns;
    // Optional future used only to probe completion (future_wait(.,0)). The
    // future itself is freed by a teardown closure, not directly by the monitor.
    rocprofvis_controller_future_t* future;
    uint64_t             last_status;      // last observed status
    bool                 has_status;       // whether last_status is valid yet
    // True once a cancel has been requested. While cancelling, the monitor stops
    // polling status / emitting events and only waits (non-blocking) for the
    // future to resolve so it can be reaped.
    bool                 cancelling;
};

// Singleton that monitors the status of ongoing operations such as SSH
// connections and profiler sessions. Each frame Update() polls every tracked
// operation; on a status change it queues an event through the EventManager so
// listeners can react (advance a state machine, refresh UI, etc.).
class AppMonitor
{
public:
    static AppMonitor* GetInstance();
    static void        DestroyInstance();

    // Registers a new operation to monitor. Returns the assigned operation id.
    // status_fn is required; event_factory, is_terminal_fn, cancel_fn and
    // teardown_fn may be empty. teardown_fn is the initial teardown closure
    // (typically frees the bound future); it runs - together with any closures
    // added later via AppendTeardown - only after the future resolves.
    uint64_t AddOperation(MonitorOperationType            type,
                          MonitorOperation::StatusFn      status_fn,
                          MonitorOperation::EventFactory  event_factory,
                          MonitorOperation::IsTerminalFn  is_terminal_fn,
                          MonitorOperation::CancelFn      cancel_fn = nullptr,
                          rocprofvis_controller_future_t* future = nullptr,
                          MonitorOperation::TeardownFn    teardown_fn = nullptr);

    // Transfers ownership of an additional resource teardown to the monitor so
    // it is freed after the operation's future resolves. Returns true if the
    // operation exists (the closure was queued); false if it has already been
    // reaped, in which case the caller must free the resource itself directly
    // (safe, because reaping only happens after the future resolves). This is
    // the atomic handoff used by sessions destroyed while an op is in flight.
    bool AppendTeardown(uint64_t operation_id, MonitorOperation::TeardownFn teardown_fn);

    // Registers a fire-and-forget teardown-only operation for resources whose
    // bound future is still in flight. The monitor signals cancel_fn (once),
    // then frees the resources via teardown_fn after the future resolves - all
    // non-blocking, across frames. Use this when an owner is being destroyed but
    // a background job may still be using its resources. teardown_fn must free
    // the future too and capture all resources by value.
    void AddTeardownOp(MonitorOperationType             type,
                       rocprofvis_controller_future_t*  future,
                       MonitorOperation::CancelFn       cancel_fn,
                       MonitorOperation::TeardownFn     teardown_fn);

    // Requests cancellation / removal of the given operation. Safe to call with
    // an unknown id. Never blocks the main thread:
    //   - If the bound future has already resolved (or there is none), the
    //     operation's teardowns run and it is removed immediately.
    //   - Otherwise cancel_fn is invoked (once) to signal the work to stop and
    //     the operation enters a "cancelling" state; a later Update() reaps it
    //     once the future resolves. The background job is never left with a
    //     dangling future / resource.
    void RemoveOperation(uint64_t operation_id);

    // True if the operation is still tracked (either active or cancelling).
    bool HasOperation(uint64_t operation_id) const;

    // True if any operation is still tracked (active or cancelling). Used by the
    // shutdown gate to defer process exit until the monitor has drained.
    bool HasPendingOperations() const;

    // Main thread only. Polls every tracked operation and queues status-change
    // events. Terminal / cancelling operations are reaped once their future
    // resolves.
    void Update();

private:
    AppMonitor();
    ~AppMonitor();

    // Non-blocking probe: true if op has no future or its future has resolved.
    bool FutureResolved(MonitorOperation& op);

    // Runs op's teardown closures in order (frees future + transferred
    // resources). Must only be called once FutureResolved(op) is true.
    void ReapOperation(MonitorOperation& op);

    uint64_t                              m_next_operation_id;
    std::map<uint64_t, MonitorOperation>  m_operations;

    static AppMonitor* s_instance;
};

}  // namespace View
}  // namespace RocProfVis
