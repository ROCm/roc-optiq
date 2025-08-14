#pragma once

// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

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

    // Getter and setter for open state
    bool IsOpen();
    void SetOpen(bool);
    bool CustomRadioButtonHollow(const char* label, bool active);

private:
    bool m_is_open           = false;
    int  m_preview_font_size = -1;
};

}  // namespace View
}  // namespace RocProfVis