// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#pragma once
#include "widgets/rocprofvis_widget.h"
#include <string>
#include <vector>

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

    void HandleEventSelectionChanged();

private:
    struct EventItem
    {
        std::string                      header;
        std::unique_ptr<HSplitContainer> contents;
        const event_info_t*              info;
        float                            height;
    };

    void RenderBasicData(const event_info_t* event_data);
    void RenderEventExtData(const event_info_t* event_data);
    void RenderEventFlowInfo(const event_info_t* event_data);
    void RenderCallStackData(const event_info_t* event_data);
    bool XButton();

    DataProvider&                      m_data_provider;
    SettingsManager&                   m_settings;
    std::shared_ptr<TimelineSelection> m_timeline_selection;
    std::vector<EventItem>             m_event_items;
    float                              m_standard_eventcard_height;
    bool                               m_table_expanded;
    bool                               m_table_was_expanded;
};

}  // namespace View
}  // namespace RocProfVis