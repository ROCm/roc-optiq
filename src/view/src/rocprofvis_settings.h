// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "imgui.h"
#include <utility>
#include <vector>
namespace RocProfVis
{
namespace View
{
enum class Colors
{
    kMetaDataColor,
    kTransparent,
    kTextError,
    kTextSuccess,
    kFlameChartColor,
    kGridColor,
    kGridRed,
    kSelectionBorder,
    kSelection,
    kBoundBox,
    kFillerColor,
    kScrollBarColor,
    kHighlightChart
};
class Settings
{
public:
    static Settings& GetInstance();
    void                      SetDPI(float DPI);
    float GetDPI();
    Settings(const Settings&)                            = delete;
    Settings&                 operator=(const Settings&) = delete;
    ImU32                     GetColor(int value);
    const std::vector<ImU32>& GetColorWheel();
    void                      DarkMode();

private:
    Settings();
    ~Settings();
    std::vector<ImU32> m_color_store;
    std::vector<ImU32> m_flame_color_wheel;
    float m_DPI; 
};

}  // namespace View
}  // namespace RocProfVis
