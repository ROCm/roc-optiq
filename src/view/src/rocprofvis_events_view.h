// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_split_containers.h"
#include <cstdint>
#include <limits>
#include <list>
#include <memory>
#include <optional>
#include <string>

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

    friend struct EventsViewTestPeer;

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

    /* Renders the selected events' cards and returns how many were drawn. In
     * compare mode source_index limits a column to one source's events.
     */
    size_t RenderEventList(std::optional<uint64_t> source_index);
    void   RenderSourceColumn(size_t source_index);

    bool XButton();

    struct CallStackHoverState
    {
        static constexpr uint64_t kInvalidId     = std::numeric_limits<uint64_t>::max();
        uint64_t                  owner_event_id = kInvalidId;
        uint64_t                  frame_event_id = kInvalidId;
        uint64_t                  frame_track_id = kInvalidId;
    };

    DataProvider&                      m_data_provider;
    SettingsManager&                   m_settings;
    std::shared_ptr<TimelineSelection> m_timeline_selection;
    std::list<EventItem>               m_event_items;
    int                                m_event_item_id;
    bool                               m_compare_mode;
    std::shared_ptr<HSplitContainer>   m_events_split;
    CellMenuTarget                     m_flow_menu;
    CellMenuTarget                     m_callstack_menu;
    CellMenuTarget                     m_arg_menu;
    CellMenuTarget                     m_basic_menu;
    CellMenuTarget                     m_ext_menu;
    FlowHighlightState                 m_flow_hover;
    FlowHighlightState                 m_frame_flow_hover;
    CallStackHoverState                m_callstack_hover;
    CallStackHoverState                m_frame_callstack_hover;
};

}  // namespace View
}  // namespace RocProfVis