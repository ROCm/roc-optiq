// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include <rocprofvis_settings.h>
#include <string>
namespace RocProfVis
{
namespace View
{

class SettingsPanel
{
public:
    SettingsPanel();
    ~SettingsPanel();

    void Render();
    void SaveModifiedSettingsToJson(const std::string& filename);
    void LoadModifiedSettingsFromJson(const std::string& filename);
    // Getter and setter for open state
    bool IsOpen();
    void SetOpen(bool);

private:
    bool            m_is_open           = false;
    int             m_preview_font_size = -1;
    DisplaySettings m_display_settings_initial;
    DisplaySettings m_display_settings_modified;
};

}  // namespace View
}  // namespace RocProfVis