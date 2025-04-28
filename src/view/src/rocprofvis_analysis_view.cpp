// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_analysis_view.h"
#include "rocprofvis_data_provider.h"

using namespace RocProfVis::View;

AnalysisView::AnalysisView(DataProvider& dp) {}
AnalysisView::~AnalysisView() {}
void
AnalysisView::Render()
{
    ImGui::Text("The component is loading...");
}
