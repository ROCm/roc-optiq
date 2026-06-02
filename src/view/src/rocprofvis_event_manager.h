// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_events.h"

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace RocProfVis
{
namespace View
{

class EventManager
{
public:
    using EventHandler      = std::function<void(std::shared_ptr<RocEvent>)>;
    using SubscriptionToken = size_t;
    static const SubscriptionToken InvalidSubscriptionToken = static_cast<SubscriptionToken>(-1);

    static EventManager* GetInstance();
    static void          DestroyInstance();

    // Main thread only.
    SubscriptionToken Subscribe(int event_id, EventHandler handler);
    bool              Unsubscribe(int event_id, SubscriptionToken token);

    // Main thread only. Drains the pending queue and invokes handlers.
    void DispatchEvents();

    // Thread-safe. Queues an event for the next DispatchEvents() on the main thread.
    void AddEvent(std::shared_ptr<RocEvent> event);

private:
    EventManager();
    ~EventManager();
    size_t m_next_token;
    std::map<int, std::vector<std::pair<SubscriptionToken, EventHandler>>>
                                         m_subscriptions;
    std::mutex                           m_queue_mutex;
    std::list<std::shared_ptr<RocEvent>> m_event_queue;

    static EventManager* s_instance;
};

}  // namespace View
}  // namespace RocProfVis
