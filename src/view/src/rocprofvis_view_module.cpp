// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_view_module.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_settings.h"
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
    if(Settings::GetInstance().GetDPI() != dpi)
    {
        Settings::GetInstance().SetDPI(dpi);
        int ideal_dpi_index =
            Settings::GetInstance().GetFontManager().GetFontSizeIndexForDPI(dpi);
        Settings::GetInstance().GetFontManager().SetFontSize(ideal_dpi_index);
    }
}