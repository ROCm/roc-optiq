// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_analysis.h"
#include "rocprofvis_data_provider.h"

using namespace RocProfVis::View;

Analysis::Analysis(DataProvider& dp) {}
Analysis::~Analysis() {}
void
Analysis::Render()
{
    ImGui::Text("The component is loading...");
}
