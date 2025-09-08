// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_settings_manager.h"
#include <list>

namespace RocProfVis
{
namespace View
{

class SettingsPanel
{
public:
    SettingsPanel(SettingsManager& settings);
    ~SettingsPanel();

    void Show();
    void Render();

private:
    struct FontSettings
    {
        bool dpi_scaling;
        int  size_index;
    };
    enum Category
    {
        Display
    };

    void RenderDisplayOptions();

    void ResetCategory(Category category);
    bool ResetButton();

    bool                     m_should_open;
    bool                     m_settings_changed;
    Category                 m_category;
    std::list<std::string>   m_font_sizes;
    std::vector<const char*> m_font_sizes_ptr;

    SettingsManager& m_settings;
    FontManager&     m_fonts;
    UserSettings&    m_usersettings;

    // Copy of settings on Show().
    UserSettings m_usersettings_initial;
    // Seperate store for font settings to keep changes isolated to preview.
    FontSettings m_font_settings;
};

}  // namespace View
}  // namespace RocProfVis