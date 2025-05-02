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
    kGenericBlack,
    kGenericRed,
    kLightBlue,
    kDarkBlue,
    kBoundBox,
    kGenericWhite,
    kScrollBarColor
};
class Settings
{
public:
    static Settings& GetInstance();

    Settings(const Settings&)                            = delete;
    Settings&                 operator=(const Settings&) = delete;
    ImU32                     GetColor(int value);
    const std::vector<ImU32>& GetColorWheel();

private:
    Settings();
    ~Settings();
    std::vector<ImU32> color_store;
    std::vector<ImU32> m_flame_color_wheel;
};

}  // namespace View
}  // namespace RocProfVis
