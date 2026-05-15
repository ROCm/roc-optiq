// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "widgets/rocprofvis_split_containers.h"
#include <cstdint>
#include <list>
#include <string>
#include <string_view>

namespace RocProfVis
{
namespace View
{

class DataProvider;
class SettingsManager;
class TimelineSelection;
struct EventInfo;

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
        int      id;
        uint64_t event_id;  // Info is deleted upon deselection so this must be cached
                            // separately.
        std::string                      header;
        std::unique_ptr<HSplitContainer> contents;
        const EventInfo*                 info;
        float                            height;

        bool operator==(const EventItem& other) const
        {
            return event_id == other.event_id;
        }
    };

    struct FlowHighlightState
    {
        uint64_t owner_event_id;
        uint64_t flow_event_id;
        uint64_t flow_track_id;

        bool IsValid() const;
        void Reset();
    };

    bool RenderBasicData(const EventInfo* event_data);
    bool RenderEventExtData(const EventInfo* event_data);
    bool RenderEventFlowInfo(const EventInfo* event_data);
    bool RenderCallStackData(const EventInfo* event_data);
    bool RenderArgumentData(const EventInfo* event_data);

    bool XButton();

    DataProvider&                            m_data_provider;
    SettingsManager&                         m_settings;
    std::shared_ptr<TimelineSelection>       m_timeline_selection;
    std::list<EventItem>                     m_event_items;
    int                                      m_event_item_id;
    int                                      m_context_menu_flow_index;
    int                                      m_context_menu_flow_column;
    FlowHighlightState                       m_flow_hover;
    FlowHighlightState                       m_frame_flow_hover;
    const std::string_view DATA_COPIED_NOTIFICATION = "Data was copied";
};

}  // namespace View
}  // namespace RocProfVis