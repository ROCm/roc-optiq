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
class Settings;
class TimelineSelection;
struct event_info_t;

class EventsView : public RocWidget
{
public:
    EventsView(DataProvider& dp, std::shared_ptr<TimelineSelection> selection);
    ~EventsView();
    void Render() override;

    void HandleEventSelectionChanged();

private:
    struct EventItem
    {
        std::string                      header;
        std::unique_ptr<HSplitContainer> contents;
        const event_info_t*              info;
    };

    void RenderEventExtData(const event_info_t* event_data);
    void RenderEventFlowInfo(const event_info_t* event_data);
    void RenderCallStackData(const event_info_t* event_data);
    bool XButton();

    DataProvider&                      m_data_provider;
    Settings&                          m_settings;
    std::shared_ptr<TimelineSelection> m_timeline_selection;
    std::vector<EventItem>             m_event_items;
};

}  // namespace View
}  // namespace RocProfVis