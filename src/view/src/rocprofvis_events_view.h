// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "widgets/rocprofvis_split_containers.h"
#include <string>
#include <list>

namespace RocProfVis
{
namespace View
{

class DataProvider;
class SettingsManager;
class TimelineSelection;
struct event_info_t;

class EventsView : public RocWidget
{
public:
    EventsView(DataProvider& dp, std::shared_ptr<TimelineSelection> timeline_selection);
    ~EventsView();
    void Render() override;

    void HandleEventSelectionChanged(const uint64_t event_id, const bool selected);

private:
    struct EventItem
    {
        int                              id;
        uint64_t                         event_id; // Info is deleted upon deselection so this must be cached seperately.
        std::string                      header;
        std::unique_ptr<HSplitContainer> contents;
        const event_info_t*              info;
        float                            height;

        bool operator==(const EventItem& other) const
        {
            return event_id == other.event_id;
        }
    };

    void RenderBasicData(const event_info_t* event_data);
    void RenderEventExtData(const event_info_t* event_data);
    void RenderEventFlowInfo(const event_info_t* event_data);
    void RenderCallStackData(const event_info_t* event_data);
    bool XButton();

    DataProvider&                            m_data_provider;
    SettingsManager&                         m_settings;
    std::shared_ptr<TimelineSelection>       m_timeline_selection;
    std::list<EventItem>                     m_event_items;
    int                                      m_event_item_id;
};

}  // namespace View
}  // namespace RocProfVis