// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_view_module.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_font_manager.h"
#include "spdlog/spdlog.h"

using namespace RocProfVis::View;

bool
rocprofvis_view_init()
{
    bool result = AppWindow::GetInstance()->Init();
    if(!result)
    {
        spdlog::error("Failed initializing the Application Window");
    }
    return result;
}

void
rocprofvis_view_render()
{
    AppWindow::GetInstance()->Render();
}

void
rocprofvis_view_destroy()
{
    AppWindow::GetInstance()->DestroyInstance();
}

void
rocprofvis_view_set_dpi(float dpi)
{
    SettingsManager& settings = SettingsManager::GetInstance();
    if(settings.GetUserSettings().display_settings.dpi_based_scaling)
    {
        if(settings.GetDPI() != dpi)
        {
            settings.SetDPI(dpi);
            FontManager& fonts = settings.GetFontManager();
            fonts.SetFontSize(fonts.GetDPIScaledFontIndex());
        }
    }
}

void
rocprofvis_view_open_files(const std::vector<std::string>& file_paths)
{
    for(const std::string& path : file_paths)
    {
        AppWindow::GetInstance()->OpenFile(path);
    }
}
