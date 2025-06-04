// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "imgui.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

class AnalysisView : public RocWidget
{
public:
    AnalysisView(DataProvider& dp);
    ~AnalysisView();
    void Render() override;
    void Update() override;

private:
    void RenderEventTable();
    void RenderSampleTable();

    void HandleTimelineSelectionChanged(std::shared_ptr<RocEvent> e);

    DataProvider& m_data_provider;

    std::shared_ptr<TabContainer> m_tab_container;
    int m_max_displayed_rows;  // Maximum number of rows to display in the table
    EventManager::SubscriptionToken m_time_line_selection_changed_token;
};

}  // namespace View
}  // namespace RocProfVis
