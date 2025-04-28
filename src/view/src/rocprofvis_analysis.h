// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "imgui.h"
#include "rocprofvis_data_provider.h"

namespace RocProfVis
{
namespace View
{

class Analysis
{
public:
    Analysis(DataProvider& dp);
    ~Analysis();
    void Render();

private:
};

}  // namespace View
}  // namespace RocProfVis
