#include "rocprofvis_events_view.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_utils.h"
#include "spdlog/spdlog.h"
#include <memory>

namespace RocProfVis
{
namespace View
{

EventsView::EventsView(DataProvider& dp)
: m_data_provider(dp)
, m_new_event_token(-1)
{
    // Subscribe to new event data
    auto new_event_data_handler = [this](std::shared_ptr<RocEvent> e) {
        this->HandleNewEventData(e);
    };
    m_new_event_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kNewEventData), new_event_data_handler);

    // Subscribe to navigation events (e.g., from sidebar)
    auto scroll_to_event_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<ScrollToEventByNameEvent>(e);
        if(evt)
        {
            this->ScrollToEventByName(evt->GetEventName());
        }
    };
    EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kHandleUserEventNavigationEvent),
        scroll_to_event_handler);
}

EventsView::~EventsView()
{
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kNewEventData),
                                             m_new_event_token);
    // Unsubscribe from other events if you have tokens for them
}

void
EventsView::HandleNewEventData(std::shared_ptr<RocEvent> e)
{
    if(!e)
    {
        spdlog::debug("Null event, cannot process new event data");
        return;
    }
    // Implement your event data handling logic here
}

void
EventsView::ScrollToEventByName(const std::string& name)
{
    // Implement logic to scroll to the event with the given name
    // For example, find the event in your data and update the view position
}

}  // namespace View
}  // namespace RocProfVis