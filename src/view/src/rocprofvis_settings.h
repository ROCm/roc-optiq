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
    kHighlightChart,
    // Used to get the size of the enum, insert new colors before this line
    __kLastColor  
};

class Settings
{
public:
    static Settings& GetInstance();
    void             SetDPI(float DPI);
    float            GetDPI();
    Settings(const Settings&)            = delete;
    Settings& operator=(const Settings&) = delete;
    ImU32     GetColor(int value) const;
    ImU32     GetColor(Colors color) const;

    const std::vector<ImU32>& GetColorWheel();

    void DarkMode();
    void LightMode();
    bool IsDarkMode() const;

private:
    Settings();
    ~Settings();
    std::vector<ImU32> m_color_store;
    std::vector<ImU32> m_flame_color_wheel;
    float              m_DPI;
    bool               m_use_dark_mode;
};

}  // namespace View
}  // namespace RocProfVis
