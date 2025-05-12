// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_event_manager.h"
#include "spdlog/spdlog.h"
#include "iostream"
using namespace RocProfVis::View;

// EventManager Implementation

EventManager* EventManager::s_instance = nullptr;

EventManager*
EventManager::GetInstance()
{
    if(!s_instance)
    {
        s_instance = new EventManager();
    }
    return s_instance;
}

void
EventManager::DestroyInstance()
{
    if(s_instance)
    {
        delete s_instance;
        s_instance = nullptr;
    }
}

EventManager::EventManager() {};
EventManager::~EventManager()
{
    m_event_queue.clear();
    m_subscriptions.clear();
}

void
EventManager::AddEvent(std::shared_ptr<RocEvent> event)
{
    m_event_queue.push_back(event);
}

bool
EventManager::Subscribe(int event_id, EventHandler handler)
{
  
    bool                       result   = false;
    std::vector<EventHandler>& handlers = m_subscriptions[event_id];

    // Check if the handler is already registered
    auto it = std::find_if(
        handlers.begin(), handlers.end(),
        [&handler](const EventHandler& existing_handler) {
            return existing_handler.target<void(std::shared_ptr<RocEvent>)>() ==
                   handler.target<void(std::shared_ptr<RocEvent>)>();
        });

    if(it == handlers.end())
    {
        // Handler is not already registered, so add it
        handlers.push_back(handler);
        result = true;
    }
    else
    {
        // Handler is already registered
        spdlog::debug("This handler is already registered for event {}", event_id);
    }

    return result;
}

bool
EventManager::Unsubscribe(int event_id, EventHandler handler)
{
    bool result = false;

    auto it = m_subscriptions.find(event_id);
    if(it != m_subscriptions.end())
    {
        std::vector<EventHandler>& handlers = it->second;

        // Find and remove the handler
        auto handler_it = std::find_if(
            handlers.begin(), handlers.end(),
            [&handler](const EventHandler& existing_handler) {
                return existing_handler.target<void(std::shared_ptr<RocEvent>)>() ==
                       handler.target<void(std::shared_ptr<RocEvent>)>();
            });

        if(handler_it != handlers.end())
        {
            spdlog::debug("Handler unsubscribed successfully for event id: {}", event_id);
            handlers.erase(handler_it);
            result = true;
        }
        else
        {
            // log handler does not exist
            spdlog::debug("Handler not found for event id: {}", event_id);
        }

        // Clean up empty vector if no more handlers exist for this event id
        if(handlers.empty())
        {
            m_subscriptions.erase(it);
        }
    }
    else
    {
        // The event does not exist
        spdlog::debug("Event id {} not found, cannot unregister handler", event_id);
    }
    return result;
}

void
EventManager::DispatchEvents()
{
    while(!m_event_queue.empty())
    {
        auto event = m_event_queue.front();
        if(!event)
        {
            m_event_queue.pop_front();
            continue;  // Skip null events
        }

        auto it = m_subscriptions.find(event->GetId());
        if(it != m_subscriptions.end())
        {
            auto& handlers = it->second;
            for(auto& handler : handlers)
            {
                handler(event);
                if(!event->CanPropagate())
                {
                    break;  // Stop propagation if indicated
                }
            }
        }
        // Remove the event from the queue after processing
        m_event_queue.pop_front();
    }
}
