// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "imgui.h"
#include "rocprofvis_data_provider.h"

namespace RocProfVis
{
namespace View
{

class AnalysisView
{
public:
    AnalysisView(DataProvider& dp);
    ~AnalysisView();
    void Render();

private:
};

}  // namespace View
}  // namespace RocProfVis
