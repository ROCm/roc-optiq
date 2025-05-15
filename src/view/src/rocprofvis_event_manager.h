// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_events.h"

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

    static EventManager* GetInstance();
    static void          DestroyInstance();

    SubscriptionToken Subscribe(int event_id, EventHandler handler);
    bool              Unsubscribe(int event_id, SubscriptionToken token);

    void DispatchEvents();
    void AddEvent(std::shared_ptr<RocEvent> event);

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
