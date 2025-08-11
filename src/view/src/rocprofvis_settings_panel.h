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

    void Render();  // Call this to draw the settings panel

private:
    // Add private members for settings state here
};

}  // namespace View
}  // namespace RocProfVis