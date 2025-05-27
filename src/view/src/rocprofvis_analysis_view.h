// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "imgui.h"
#include "rocprofvis_data_provider.h"
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
    DataProvider& m_data_provider;
    
    std::shared_ptr<TabContainer> m_tab_container;
};

}  // namespace View
}  // namespace RocProfVis
