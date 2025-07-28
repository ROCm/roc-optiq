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
    kMetaDataColorSelected,
    kMetaDataSeparator,
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
    kRulerBgColor,
    kArrowColor,
    kBorderColor,
    kSplitterColor,
    // Used to get the size of the enum, insert new colors before this line
    __kLastColor
};

enum class FontType
{
    kSmall,
    kMedium,
    kMedLarge,
    kLarge,
    kIcon,
    // Used to get the size of the enum, insert new fonts before this line
    __kLastFont,
    kDefault = kMedium
};

class FontManager
{
public:
    FontManager()                              = default;
    ~FontManager()                             = default;
    FontManager(const FontManager&)            = delete;
    FontManager& operator=(const FontManager&) = delete;

    /*
     * Called to initialize the font manager. Should be once called after ImGui context is
     * created, but before the first frame.
     */
    bool Init();

    ImFont* GetFont(FontType font_type);

private:
    std::vector<ImFont*> m_fonts;
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
    bool HorizontalRender();
    bool IsHorizontalRender();

    FontManager& GetFontManager() { return m_font_manager; }

private:
    Settings();
    ~Settings();
    std::vector<ImU32> m_color_store;
    std::vector<ImU32> m_flame_color_wheel;
    float              m_DPI;
    bool               m_use_dark_mode;
    bool               m_use_horizontal_rendering;
    FontManager        m_font_manager;
};

}  // namespace View
}  // namespace RocProfVis
