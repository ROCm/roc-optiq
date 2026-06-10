// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_events.h"
#include "rocprofvis_shared_types.h"

#include <functional>
#include <list>
#include <map>
#include <memory>
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
    static const SubscriptionToken InvalidSubscriptionToken = static_cast<SubscriptionToken>(INVALID_INDEX_64);

    static EventManager* GetInstance();
    static void          DestroyInstance();

    SubscriptionToken Subscribe(int event_id, EventHandler handler);
    bool              Unsubscribe(int event_id, SubscriptionToken token);

    void DispatchEvents();
    void AddEvent(std::shared_ptr<RocEvent> event);

    // True while events are queued for deferred dispatch (keeps lazy render on).
    bool HasPendingEvents() const { return !m_event_queue.empty(); }

private:
    EventManager();
    ~EventManager();
    size_t m_next_token;
    std::map<int, std::vector<std::pair<SubscriptionToken, EventHandler>>>
                                         m_subscriptions;
    std::list<std::shared_ptr<RocEvent>> m_event_queue;

    static EventManager* s_instance;
};

}  // namespace View
}  // namespace RocProfVis
