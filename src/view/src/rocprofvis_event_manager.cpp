// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_event_manager.h"
#include "iostream"
#include "spdlog/spdlog.h"

#include <algorithm>

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

EventManager::EventManager(): m_next_token(0) {};

EventManager::~EventManager()
{
    m_event_queue.clear();
    m_subscriptions.clear();
    m_next_token = 0;
}

void
EventManager::AddEvent(std::shared_ptr<RocEvent> event)
{
    m_event_queue.push_back(event);
}

EventManager::SubscriptionToken
EventManager::Subscribe(int event_id, EventHandler handler)
{
    SubscriptionToken token = m_next_token++;
    m_subscriptions[event_id].emplace_back(token, handler);
    return token;
}

bool
EventManager::Unsubscribe(int event_id, SubscriptionToken token)
{
    bool result = false;
    auto it = m_subscriptions.find(event_id);
    if(it != m_subscriptions.end())
    {
        // Event ID found, proceed to remove the handler
        auto& handlers = it->second;

        auto handler_it =
            std::remove_if(handlers.begin(), handlers.end(),
                           [token](const auto& pair) { return pair.first == token; });
        if(handler_it != handlers.end())
        {
            spdlog::debug("Handler unsubscribed successfully for event id: {}", event_id);
            handlers.erase(handler_it, handlers.end());
            result = true;  // Successfully unsubscribed
        }
        else
        {
            // log handler does not exist
            spdlog::debug("Handler not found for event id: {} with token {}", event_id, token);
        }

        // Clean up empty vector if no more handlers exist for this event id
        if(handlers.empty())
        {
            m_subscriptions.erase(it);
        }
    }
    else
    {
        // Event ID not found, cannot unsubscribe
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
                handler.second(event);
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
