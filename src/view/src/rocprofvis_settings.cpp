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

const std::vector<ImU32>&
Settings::GetColorWheel()
{
    return m_flame_color_wheel;
}

Settings::Settings()
: color_store(15)
, m_flame_color_wheel({

      IM_COL32(0, 114, 188, 204), IM_COL32(0, 158, 115, 204), IM_COL32(240, 228, 66, 204),
      IM_COL32(204, 121, 167, 204), IM_COL32(86, 180, 233, 204),
      IM_COL32(213, 94, 0, 204), IM_COL32(0, 204, 102, 204), IM_COL32(230, 159, 0, 204),
      IM_COL32(153, 153, 255, 204), IM_COL32(255, 153, 51, 204) })

{
    color_store[static_cast<int>(Colors::kMetaDataColor)] = IM_COL32(240, 240, 240, 55);
    color_store[static_cast<int>(Colors::kTransparent)]   = IM_COL32(0, 0, 0, 0);
    color_store[static_cast<int>(Colors::kTextError)]     = IM_COL32(255, 0, 0, 255);
    color_store[static_cast<int>(Colors::kTextSuccess)]   = IM_COL32(0, 255, 0, 255);
    color_store[static_cast<int>(Colors::kFlameChartColor)] =
        IM_COL32(128, 128, 128, 255);
    color_store[static_cast<int>(Colors::kGenericBlack)]    = IM_COL32(0, 0, 0, 255);
    color_store[static_cast<int>(Colors::kGenericRed)]      = IM_COL32(255, 0, 0, 255);
    color_store[static_cast<int>(Colors::kSelectionBorder)] = IM_COL32(0, 0, 200, 255);
    color_store[static_cast<int>(Colors::kSelection)]       = IM_COL32(0, 0, 100, 80);
    color_store[static_cast<int>(Colors::kBoundBox)]       = IM_COL32(100, 100, 100, 150);
    color_store[static_cast<int>(Colors::kGenericWhite)]   = IM_COL32(255, 255, 255, 255);
    color_store[static_cast<int>(Colors::kScrollBarColor)] = IM_COL32(200, 200, 200, 255);
}

Settings::~Settings() {}

}  // namespace View
}  // namespace RocProfVis
