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
        Display,
        Units
    };

    void RenderDisplayOptions();
    void RenderUnitOptions();

    void ResetDisplayOptions();
    void ResetUnitOptions();

    bool ResetButton();

    bool                     m_should_open;
    bool                     m_settings_changed;
    bool                     m_settings_confirmed;
    Category                 m_category;
    std::list<std::string>   m_font_sizes;
    std::vector<const char*> m_font_sizes_ptr;

    SettingsManager& m_settings;
    FontManager&     m_fonts;
    UserSettings&    m_usersettings;

    const UserSettings& m_usersettings_default;
    // Copy of settings on Show().
    UserSettings m_usersettings_initial;
    UserSettings m_usersettings_previous;
    // Seperate store for font settings to keep changes isolated to preview.
    FontSettings m_font_settings;
};

}  // namespace View
}  // namespace RocProfVis