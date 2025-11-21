// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_view_module.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_utils.h"
#include "spdlog/spdlog.h"

using namespace RocProfVis::View;

bool
rocprofvis_view_init(std::function<void(int)> notification_callback)
{
    auto app = AppWindow::GetInstance();
    bool result = app->Init();
    if(!result)
    {
        spdlog::error("Failed initializing the Application Window");
    }
    else 
    {
        app->SetNotificationCallback(notification_callback);
    }
    return result;

}

void
rocprofvis_view_render(const rocprofvis_view_render_options_t& render_options)
{
    if(render_options ==
       rocprofvis_view_render_options_t::kRocProfVisViewRenderOption_RequestExit)
    {
        AppWindow::GetInstance()->ShowCloseConfirm();
    }
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

std::string
rocprofvis_get_application_config_path()
{
    // Get the application config path
    return get_application_config_path(true);
}
