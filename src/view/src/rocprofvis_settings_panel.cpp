// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_settings_panel.h"
#include <imgui.h>

namespace RocProfVis
{
namespace View
{

SettingsPanel::SettingsPanel()
{
    // Initialize settings state here
}

SettingsPanel::~SettingsPanel()
{
    // Cleanup if needed
}

void
SettingsPanel::Render()
{
    ImGui::Begin("Settings");
    // Add ImGui widgets for settings here, e.g.:
    // ImGui::Checkbox("Enable Feature", &enableFeature);
    ImGui::End();
}

}  // namespace View
}  // namespace RocProfVis