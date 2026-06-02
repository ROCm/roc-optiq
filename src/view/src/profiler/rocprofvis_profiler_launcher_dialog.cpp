// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_profiler_launcher_dialog.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "rocprofvis_launch_shared_tabs.h"
#include "rocprofvis_rocprof_sys_backend.h"
#include "imgui.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <vector>

namespace RocProfVis
{
namespace View
{

ProfilerLauncherDialog::ProfilerLauncherDialog(AppWindow* app_window)
    : m_app_window(app_window)
    , m_profiler_session()
    , m_profiler_status_token(EventManager::InvalidSubscriptionToken)
    , m_should_open(false)
    , m_show_window(false)
    , m_is_running(false)
    , m_backend_index(0)
    , m_tool_index(0)
    , m_config()
    , m_profiler_path_override()
    , m_execution_cache()
    , m_preset_manager()
    , m_current_preset_name()
    , m_profiler_state(kRPVProfilerStateIdle)
    , m_output_text()
    , m_error_message()
    , m_auto_scroll_output(true)
{
    m_backends.push_back(std::make_unique<RocprofSysBackend>());

    m_config.profiler_id = m_backends[0]->Id();
    m_config.tool_id = m_backends[0]->GetTools()[0].id;
    m_backends[0]->LoadSettings(jt::Json());
    m_config.backend_payload = m_backends[0]->SaveSettings();

    LoadFromSettings();
    RefreshExecutionCache();

    // Listen for profiler state transitions surfaced by the AppMonitor. Filter
    // to events belonging to this dialog's session by operation id.
    m_profiler_status_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kProfilerStatusChanged),
        [this](std::shared_ptr<RocEvent> event)
        {
            auto* status_event = static_cast<ProfilerStatusEvent*>(event.get());
            if(status_event->GetOperationId() == m_profiler_session.GetOperationId())
            {
                OnProfilerStateChanged(status_event->GetState());
            }
        });
}

ProfilerLauncherDialog::~ProfilerLauncherDialog()
{
    if(m_profiler_status_token != EventManager::InvalidSubscriptionToken)
    {
        EventManager::GetInstance()->Unsubscribe(
            static_cast<int>(RocEvents::kProfilerStatusChanged), m_profiler_status_token);
    }
}

void ProfilerLauncherDialog::Show()
{
    m_should_open = true;
}

void ProfilerLauncherDialog::Render()
{
    if (m_should_open)
    {
        m_show_window = true;
        m_should_open = false;
        ImGui::SetWindowFocus("Launch Profiler");
    }

    if (!m_show_window)
    {
        return;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(1000, 700), ImGuiCond_FirstUseEver);

    bool window_open = true;
    if (ImGui::Begin("Launch Profiler", &window_open, ImGuiWindowFlags_NoScrollbar))
    {
        // Sync typed settings to backend_payload so preset save sees current values
        IProfilerBackend* backend = m_backends[m_backend_index].get();
        m_config.backend_payload = backend->SaveSettings();

        RenderToolbar();
        backend = m_backends[m_backend_index].get();
        ImGui::Separator();

        // Reserve bottom area for preview + output + buttons.
        float bottom_reserve = 280.0f;
        ImGui::BeginChild("MainPane", ImVec2(0, -bottom_reserve), true);
        RenderMainContent();
        ImGui::EndChild();

        // Warnings from backend
        auto warnings = backend->GetWarnings(m_config);
        if (!warnings.empty())
        {
            for (auto const& w : warnings)
            {
                ImVec4 color;
                const char* prefix;
                switch (w.level)
                {
                    case WarningMessage::kError:
                        color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
                        prefix = "Error: ";
                        break;
                    case WarningMessage::kWarning:
                        color = ImVec4(1.0f, 0.8f, 0.0f, 1.0f);
                        prefix = "Warning: ";
                        break;
                    default:
                        color = ImVec4(0.4f, 0.7f, 1.0f, 1.0f);
                        prefix = "Hint: ";
                        break;
                }
                ImGui::TextColored(color, "%s%s", prefix, w.text.c_str());
            }
        }

        // Command preview
        RenderCommandPreview(m_execution_cache.command_preview);

        // Launch Button row
        RenderButtonRow();

        // Output console
        if (RenderOutputConsole(m_output_text, m_error_message,
                                static_cast<int>(m_profiler_state), m_auto_scroll_output))
        {
            m_profiler_session.ClearOutput();
            m_output_text.clear();
            m_output_preamble.clear();
            m_output_epilogue.clear();
            m_process_output_raw.clear();
            m_process_output_stripped.clear();
            m_error_message.clear();
        }

    }
    ImGui::End();

    if (!window_open)
    {
        OnCloseClicked();
        m_show_window = false;
    }
}

void ProfilerLauncherDialog::RenderToolbar()
{
    // Profiler selector
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Profiler:");
    ImGui::SameLine();
    ImGui::PushItemWidth(140);
    if (ImGui::BeginCombo("##ProfilerBackend",
                          m_backends[m_backend_index]->DisplayName()))
    {
        for (size_t i = 0; i < m_backends.size(); i++)
        {
            bool selected = (static_cast<int>(i) == m_backend_index);
            if (ImGui::Selectable(m_backends[i]->DisplayName(), selected))
            {
                SwitchBackend(static_cast<int>(i));
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    VerticalSeparator();

    // Tool selector
    IProfilerBackend const* backend = m_backends[m_backend_index].get();
    auto tools = backend->GetTools();
    ImGui::Text("Tool:");
    ImGui::SameLine();
    ImGui::PushItemWidth(120);
    if (ImGui::BeginCombo("##ToolSelector",
                          tools[m_tool_index].display_name.c_str()))
    {
        for (size_t i = 0; i < tools.size(); i++)
        {
            bool selected = (static_cast<int>(i) == m_tool_index);
            if (ImGui::Selectable(tools[i].display_name.c_str(), selected))
            {
                m_tool_index = static_cast<int>(i);
                m_config.tool_id = tools[i].id;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    VerticalSeparator();

    // Profiler path override
    ImGui::Text("Profiler Path:");
    ImGui::SameLine();
    char path_buf[512];
    std::snprintf(path_buf, sizeof(path_buf), "%s", m_profiler_path_override.c_str());
    ImGui::PushItemWidth(220);
    if (ImGui::InputText("##ProfPath", path_buf, sizeof(path_buf)))
    {
        m_profiler_path_override = path_buf;
    }
    ImGui::PopItemWidth();
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Leave empty to use PATH");
    }

    VerticalSeparator();

    // Saved launch profiles (Optiq JSON presets)
    std::string load_name = RenderSavedProfileBar(
        m_preset_manager, m_config.profiler_id,
        m_current_preset_name, m_config, backend, m_app_window);

    if (!load_name.empty())
    {
        LaunchConfig loaded;
        if (m_preset_manager.LoadPreset(load_name, m_config.profiler_id, loaded))
        {
            m_config = loaded;
            m_backends[m_backend_index]->LoadSettings(m_config.backend_payload);
            backend = m_backends[m_backend_index].get();
            tools = backend->GetTools();
            for (size_t i = 0; i < tools.size(); i++)
            {
                if (tools[i].id == m_config.tool_id)
                {
                    m_tool_index = static_cast<int>(i);
                    break;
                }
            }
        }
    }
}

void ProfilerLauncherDialog::RenderMainContent()
{
    IProfilerBackend const* backend = m_backends[m_backend_index].get();

    // Target is always visible at the top, not buried in a tab.
    RenderTargetSection(m_config.target, m_config.connection, m_app_window);
    ImGui::Separator();

    if (ImGui::BeginTabBar("LaunchTabs"))
    {
        // Backend-provided tabs (Quick, Sampling, ROCm, ...)
        auto tabs = backend->GetTabs(m_config.tool_id);
        for (auto const& tab : tabs)
        {
            if (ImGui::BeginTabItem(tab.display_name.c_str()))
            {
                tab.render_fn();
                ImGui::EndTabItem();
            }
        }

        // Raw Env Vars tab (shared)
        if (ImGui::BeginTabItem("Raw Env Vars"))
        {
            RenderRawEnvVarsTab(m_config.extra_env, m_execution_cache.curated_env_vars);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

void ProfilerLauncherDialog::RenderButtonRow()
{
    ImGui::Separator();

    bool can_launch = !m_is_running &&
        (m_profiler_state == kRPVProfilerStateIdle ||
         m_profiler_state == kRPVProfilerStateCompleted ||
         m_profiler_state == kRPVProfilerStateFailed ||
         m_profiler_state == kRPVProfilerStateCancelled);
    bool can_cancel = m_is_running && m_profiler_state == kRPVProfilerStateRunning;

    if (can_launch)
    {
        if (ImGui::Button("Launch", ImVec2(120, 0)))
        {
            OnLaunchClicked();
        }
    }
    else
    {
        ImGui::BeginDisabled();
        ImGui::Button("Launch", ImVec2(120, 0));
        ImGui::EndDisabled();
    }

    ImGui::SameLine();

    if (can_cancel)
    {
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            OnCancelClicked();
        }
    }
    else
    {
        ImGui::BeginDisabled();
        ImGui::Button("Cancel", ImVec2(120, 0));
        ImGui::EndDisabled();
    }

    ImGui::SameLine();

    if (ImGui::Button("Close", ImVec2(120, 0)))
    {
        OnCloseClicked();
        m_show_window = false;
    }
}

void ProfilerLauncherDialog::Update()
{
    if (m_show_window)
    {
        RefreshExecutionCache();
    }

    // Profiler state transitions arrive via kProfilerStatusChanged events
    // (see OnProfilerStateChanged), dispatched by the AppMonitor each frame.

    // Always try to fetch output while there's an active profiler session,
    // even after the process has completed (output may arrive asynchronously)
    if (m_profiler_state == kRPVProfilerStateRunning ||
        m_profiler_state == kRPVProfilerStateCompleted ||
        m_profiler_state == kRPVProfilerStateFailed)
    {
        UpdateOutput();
    }
}

void ProfilerLauncherDialog::OnLaunchClicked()
{
    IProfilerBackend* backend = m_backends[m_backend_index].get();

    // Sync typed settings to payload before launch/validation
    m_config.backend_payload = backend->SaveSettings();

    // Validate
    std::string err = backend->Validate(m_config);
    if (!err.empty())
    {
        m_error_message = "Error: " + err;
        return;
    }

    // Clear previous state
    m_error_message.clear();
    m_output_preamble.clear();
    m_output_epilogue.clear();
    m_process_output_raw.clear();
    m_process_output_stripped.clear();
    m_output_text.clear();

    RefreshExecutionCache();

    // Determine profiler type enum for controller
    rocprofvis_profiler_type_t profiler_type = kRPVProfilerTypeRocprofSysRun;
    if (m_config.tool_id == "run" || m_config.tool_id == "sample")
    {
        profiler_type = kRPVProfilerTypeRocprofSysRun;
    }
    else if (m_config.tool_id == "instrument")
    {
        profiler_type = kRPVProfilerTypeRocprofSysInstrument;
    }

    std::string const& profiler_path = m_execution_cache.profiler_path;

    std::string const& profiler_args_str = m_execution_cache.profiler_args;

    // Launch via the profiler session; state transitions are reported through
    // kProfilerStatusChanged events handled in OnProfilerStateChanged().
    bool success = m_profiler_session.Launch(
        profiler_type,
        profiler_path,
        m_config.target.executable,
        m_config.target.arguments,
        m_config.target.output_directory,
        profiler_args_str,
        m_execution_cache.env_vars);

    if (success)
    {
        m_is_running = true;
        m_profiler_state = kRPVProfilerStateRunning;

        // Build preamble
        std::ostringstream preamble;
        preamble << m_execution_cache.command_preview << "\n\n";
        m_output_preamble = preamble.str();
        RebuildComposedOutput();

        // Save settings
        SaveToSettings();
        AddRecentTarget(m_config.target.executable);
    }
    else
    {
        m_error_message = "Failed to launch profiler";
        m_profiler_state = kRPVProfilerStateFailed;
    }
}

void ProfilerLauncherDialog::OnCancelClicked()
{
    if (m_profiler_session.Cancel())
    {
        m_output_epilogue += "\nProfiler cancelled by user.\n";
        RebuildComposedOutput();
        m_profiler_state = kRPVProfilerStateCancelled;
        m_is_running = false;
    }
    else
    {
        m_error_message = "Failed to cancel profiler";
    }
}

void ProfilerLauncherDialog::OnCloseClicked()
{
    if (m_is_running)
    {
        m_profiler_session.Cancel();
    }

    m_profiler_session.Close();
    m_is_running = false;
    m_profiler_state = kRPVProfilerStateIdle;
}

void ProfilerLauncherDialog::OnProfilerStateChanged(rocprofvis_profiler_state_t new_state)
{
    if (new_state == m_profiler_state)
    {
        return;
    }

    m_profiler_state = new_state;

    if (new_state == kRPVProfilerStateCompleted)
    {
        m_is_running = false;
        m_output_epilogue += "\nProfiler completed successfully.\n";

        std::string trace_path = m_profiler_session.GetTracePath();
        if (!trace_path.empty())
        {
            m_output_epilogue += "Trace file: " + trace_path + "\n";

            if (m_config.target.auto_load_trace && m_app_window)
            {
                m_app_window->OpenFile(trace_path);
            }
        }
        RebuildComposedOutput();
    }
    else if (new_state == kRPVProfilerStateFailed)
    {
        m_is_running = false;
        int32_t exit_code = m_profiler_session.GetExitCode();
        char exit_msg[128];
        std::snprintf(exit_msg, sizeof(exit_msg),
                      "\nProfiler failed (exit code %d).\n", exit_code);
        m_output_epilogue += exit_msg;
        RebuildComposedOutput();
        if (exit_code == 127)
        {
            m_error_message = "Profiler executable not found or could not be started (exit code 127)";
        }
        else
        {
            std::snprintf(exit_msg, sizeof(exit_msg),
                          "Profiler execution failed (exit code %d)", exit_code);
            m_error_message = exit_msg;
        }
    }
    else if (new_state == kRPVProfilerStateCancelled)
    {
        m_is_running = false;
    }
}

void ProfilerLauncherDialog::UpdateOutput()
{
    std::string new_raw = m_profiler_session.GetOutput();
    if (new_raw != m_process_output_raw)
    {
        m_process_output_raw = std::move(new_raw);
        m_process_output_stripped = strip_ansi_for_display(m_process_output_raw);
        RebuildComposedOutput();
    }
}

void ProfilerLauncherDialog::RebuildComposedOutput()
{
    m_output_text = m_output_preamble + m_process_output_stripped + m_output_epilogue;
}

void ProfilerLauncherDialog::RefreshExecutionCache()
{
    if (m_backends.empty())
    {
        m_execution_cache = ExecutionCache();
        return;
    }

    IProfilerBackend* backend = m_backends[m_backend_index].get();
    m_config.backend_payload = backend->SaveSettings();

    ExecutionCache cache;
    cache.profiler_path = GetProfilerPath();
    backend->FlattenToExecution(m_config, cache.curated_env_vars, cache.argv);

    cache.env_vars = cache.curated_env_vars;
    for (auto const& kv : m_config.extra_env)
    {
        cache.env_vars.emplace_back(kv.first, kv.second);
    }

    for (size_t i = 0; i < cache.argv.size(); i++)
    {
        if (i > 0)
        {
            cache.profiler_args += " ";
        }
        cache.profiler_args += cache.argv[i];
    }

    for (auto const& arg : m_config.extra_argv)
    {
        if (!cache.profiler_args.empty())
        {
            cache.profiler_args += " ";
        }
        cache.profiler_args += arg;
    }

    cache.command_preview = BuildCommandPreviewString(
        m_config, cache.profiler_path, cache.env_vars, cache.argv);

    m_execution_cache = std::move(cache);
}

void ProfilerLauncherDialog::SwitchBackend(int index)
{
    if (index < 0 || index >= static_cast<int>(m_backends.size()))
    {
        return;
    }
    m_backend_index = index;
    m_tool_index = 0;
    m_config.profiler_id = m_backends[index]->Id();
    m_config.tool_id = m_backends[index]->GetTools()[0].id;
    m_backends[index]->LoadSettings(jt::Json());
    m_config.backend_payload = m_backends[index]->SaveSettings();
}

void ProfilerLauncherDialog::LoadFromSettings()
{
    SettingsManager& settings = SettingsManager::Get();
    ProfilerSettings& ps = settings.GetProfilerSettings();

    m_profiler_path_override = ps.profiler_path;
    m_config.target.output_directory = ps.profiler_output_directory;
    m_config.target.auto_load_trace = ps.auto_load_trace;
    m_current_preset_name = ps.last_preset_name;

    if (!ps.last_profiler_id.empty())
    {
        for (size_t i = 0; i < m_backends.size(); i++)
        {
            if (m_backends[i]->Id() == ps.last_profiler_id)
            {
                m_backend_index = static_cast<int>(i);
                m_config.profiler_id = ps.last_profiler_id;
                break;
            }
        }
    }
}

void ProfilerLauncherDialog::SaveToSettings()
{
    SettingsManager& settings = SettingsManager::Get();
    ProfilerSettings& ps = settings.GetProfilerSettings();

    ps.profiler_path = m_profiler_path_override;
    ps.profiler_output_directory = m_config.target.output_directory;
    ps.auto_load_trace = m_config.target.auto_load_trace;
    ps.last_preset_name = m_current_preset_name;
    ps.last_profiler_id = m_config.profiler_id;
    settings.SaveProfilerSettings();
}

void ProfilerLauncherDialog::AddRecentTarget(std::string const& exe)
{
    if (exe.empty())
    {
        return;
    }

    SettingsManager& settings = SettingsManager::Get();
    ProfilerSettings& ps = settings.GetProfilerSettings();

    // Remove existing entry if present
    auto& recents = ps.recent_targets;
    recents.erase(
        std::remove(recents.begin(), recents.end(), exe),
        recents.end());

    // Add to front
    recents.insert(recents.begin(), exe);

    // Cap at 10
    if (recents.size() > 10)
    {
        recents.resize(10);
    }

    settings.SaveProfilerSettings();
}

std::string ProfilerLauncherDialog::GetProfilerPath() const
{
    if (!m_profiler_path_override.empty())
    {
        return m_profiler_path_override;
    }
    IProfilerBackend const* backend = m_backends[m_backend_index].get();
    return backend->GetDefaultBinary(m_config.tool_id);
}

}  // namespace View
}  // namespace RocProfVis
