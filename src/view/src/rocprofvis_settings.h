// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "imgui.h"

#include <utility>
#include <vector>
namespace RocProfVis
{
namespace View
{

typedef struct DisplaySettings
{
    float dpi;
    bool  use_dark_mode;
    bool  dpi_based_scaling;
    int   font_size_index;
} DisplaySettings;

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
    kRulerTextColor,
    kScrubberNumberColor,
    kArrowColor,
    kBorderColor,
    kSplitterColor,
    kBgMain,
    kBgPanel,
    kAccentRed,
    kAccentRedHover,
    kAccentRedActive,
    kBorderGray,
    kTextMain,
    kTextDim,
    kScrollBg,
    kScrollGrab,
    kTableHeaderBg,
    kTableBorderStrong,
    kTableBorderLight,
    kTableRowBg,
    kTableRowBgAlt,
    kEventHighlight,
    kLineChartColor,
    kButton,
    kButtonHovered,
    kButtonActive,
    kBgWarning,
    kBgError,
    // Used to get the size of the enum, insert new colors before this line
    __kLastColor
};

enum class FontType
{
    kSmall,
    kMedium,
    kMedLarge,
    kLarge,
    // Used to get the size of the enum, insert new fonts before this line
    __kLastFont,
    kDefault = kMedium
};

class FontManager
{
public:
    FontManager();
    ~FontManager();
    FontManager(const FontManager&)                  = delete;
    FontManager&       operator=(const FontManager&) = delete;
    std::vector<float> GetFontSizes();
    ImFont*            GetIconFontByIndex(int idx);

    /*
     * Called to initialize the font manager. Should be once called after ImGui context is
     * created, but before the first frame.
     */
    bool Init();

    ImFont* GetFont(FontType font_type);
    ImFont* GetIconFont(FontType font_type);

    void    SetFontSize(int size_index);
    int     GetCurrentFontSizeIndex();
    ImFont* GetFontByIndex(int idx);
    int     GetFontSizeIndexForDPI(float dpi);

private:
    std::vector<ImFont*> m_fonts;
    std::vector<float>   m_font_sizes;
    std::vector<ImFont*> m_icon_fonts;
    std::vector<ImFont*> m_all_fonts;
    std::vector<int>     m_font_size_indices;
    std::vector<ImFont*> m_all_icon_fonts;
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

    FontManager&      GetFontManager() { return m_font_manager; }
    const ImGuiStyle& GetDefaultStyle() const { return m_default_style; }
    bool              IsDPIBasedScaling() const;
    void              SetDPIBasedScaling(bool enabled);
    DisplaySettings&  GetCurrentDisplaySettings();
    void              RestoreDisplaySettings(const DisplaySettings& settings);
    DisplaySettings&  GetInitialDisplaySettings();
    void              SetDisplaySettings(const DisplaySettings& settings);

private:
    Settings();
    ~Settings();

    void InitStyling();
    void ApplyColorStyling();

    std::vector<ImU32> m_color_store;
    std::vector<ImU32> m_flame_color_wheel;

    FontManager m_font_manager;
    ImGuiStyle  m_default_style;
    DisplaySettings
        m_display_settings_initial;  // Needed if you want to truly go back to factory
                                     // settings after changing settings and saving.
    DisplaySettings m_display_settings_current;
};

}  // namespace View
}  // namespace RocProfVis
