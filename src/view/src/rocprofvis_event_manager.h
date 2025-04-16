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
    using EventHandler = std::function<void(std::shared_ptr<RocEvent>)>;

    static EventManager* GetInstance();
    static void          DestroyInstance();

    bool Subscribe(int event_id, EventHandler handler);
    bool Unsubscribe(int event_id, EventHandler handler);

    void EventManager::DispatchEvents();
    void AddEvent(std::shared_ptr<RocEvent> event);

private:
    EventManager();
    ~EventManager();

    std::map<int, std::vector<EventHandler>> m_subscriptions;
    std::list<std::shared_ptr<RocEvent>>     m_event_queue;

    static EventManager* s_instance;
};

}  // namespace View
}  // namespace RocProfVis
