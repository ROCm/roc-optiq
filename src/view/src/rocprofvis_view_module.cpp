// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_view_module.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "widgets/rocprofvis_image_helpers.h"
#include "spdlog/spdlog.h"
#ifdef ROCPROFVIS_ENABLE_AGENTIC_PROFILING
#include "agenticprofiling/rocprofvis_ai_assistant.h"
#endif

using namespace RocProfVis::View;

bool
rocprofvis_view_init(std::function<void(int)>                 notification_callback,
                     rocprofvis_view_file_dialog_preference_t file_dialog_pref)
{
    auto app = AppWindow::GetInstance();
    app->SetFileDialogPreference(file_dialog_pref);
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
    if(static_cast<int>(render_options) &
       static_cast<int>(rocprofvis_view_render_options_t::kRocProfVisViewRenderOption_RequestExit))
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
rocprofvis_view_open_files(const std::vector<std::string>& file_paths)
{
    for(const std::string& path : file_paths)
    {
        AppWindow::GetInstance()->OpenFile(path);
    }
}

void
rocprofvis_view_set_fullscreen_state(bool is_fullscreen)
{
    AppWindow::GetInstance()->SetFullscreenState(is_fullscreen);
}

void
rocprofvis_view_set_texture_backend(
    rocprofvis_view_create_texture_rgba32_t create_texture,
    rocprofvis_view_destroy_texture_t       destroy_texture,
    void*                                  user_data)
{
    GuiTexture::SetBackend(create_texture, destroy_texture, user_data);
}

std::string
rocprofvis_get_application_config_path()
{
    // Get the application config path
    return get_application_config_path(true);
}

std::string
rocprofvis_get_application_log_path()
{
    return get_application_log_path(true);
}

bool
rocprofvis_view_is_remote_display_session()
{
    return is_remote_display_session();
}

bool
rocprofvis_view_wants_continuous_render()
{
    return AppWindow::GetInstance()->WantsContinuousRender();
}

#ifdef ROCPROFVIS_ENABLE_AGENTIC_PROFILING

void
rocprofvis_view_start_assistant_batch(const std::string& question,
                                      const std::string& output_path,
                                      bool explain_first, uint32_t timeout_seconds)
{
    AssistantBatchRequest request;
    request.question        = question;
    request.output_path     = output_path;
    request.explain_first   = explain_first;
    request.timeout_seconds = timeout_seconds;
    AssistantPanel::GetInstance()->StartBatch(request);
}

rocprofvis_view_assistant_batch_state_t
rocprofvis_view_assistant_batch_state()
{
    switch(AssistantPanel::GetInstance()->BatchState())
    {
        case AssistantBatchState::kRunning: return kRocProfVisAssistantBatch_Running;
        case AssistantBatchState::kDone: return kRocProfVisAssistantBatch_Done;
        case AssistantBatchState::kFailed: return kRocProfVisAssistantBatch_Failed;
        case AssistantBatchState::kInactive: break;
    }
    return kRocProfVisAssistantBatch_Inactive;
}

#else

// Refusing has to be remembered, or the caller polls an inactive state forever
// waiting for a run that was never going to happen.
static bool g_assistant_batch_refused = false;

void
rocprofvis_view_start_assistant_batch(const std::string& question,
                                      const std::string& output_path,
                                      bool explain_first, uint32_t timeout_seconds)
{
    (void) question;
    (void) output_path;
    (void) explain_first;
    (void) timeout_seconds;
    g_assistant_batch_refused = true;
    spdlog::error("This build has no assistant. Rebuild with "
                  "-DROCPROFVIS_ENABLE_AGENTIC_PROFILING=ON to use --ask.");
}

rocprofvis_view_assistant_batch_state_t
rocprofvis_view_assistant_batch_state()
{
    return g_assistant_batch_refused ? kRocProfVisAssistantBatch_Failed
                                     : kRocProfVisAssistantBatch_Inactive;
}

#endif  // ROCPROFVIS_ENABLE_AGENTIC_PROFILING

bool
rocprofvis_view_get_drag_repair_enabled()
{
    return SettingsManager::GetInstance().GetUserSettings().linux_drag_repair;
}

void
rocprofvis_view_set_drag_repair_enabled(bool enabled)
{
    SettingsManager& settings = SettingsManager::GetInstance();
    UserSettings     previous = settings.GetUserSettings();
    if(previous.linux_drag_repair == enabled)
    {
        return;
    }

    settings.GetUserSettings().linux_drag_repair = enabled;
    settings.ApplyUserSettings(previous, true);
}
