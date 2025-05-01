// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_settings.h"
#include "imgui.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
namespace RocProfVis
{
namespace View
{


Settings&
Settings::GetInstance()
{
    static Settings instance;
    return instance;
}

ImU32
Settings::GetColor(int value)
{
    return color_store[value];
}

Settings::Settings()
: color_store(5)

{
    color_store[static_cast<int>(Colors::kMetaDataColor)] =  IM_COL32(240, 240, 240, 55);
    
    //int y = static_cast<int>(Colors::kLineColor);

    //color_store[static_cast<int>(Colors::kLineColor)] = 0;
}

Settings::~Settings() {}

}  // namespace View
}  // namespace RocProfVis
