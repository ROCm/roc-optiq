// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_profiler_launcher_dialog.h"
#include "rocprofvis_data_provider.h"
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
    , m_data_provider()
    , m_should_open(false)
    , m_show_window(false)
    , m_is_running(false)
    , m_backend_index(0)
    , m_tool_index(0)
    , m_config()
    , m_profiler_path_override()
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
}

ProfilerLauncherDialog::~ProfilerLauncherDialog()
{
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

        // Top: Preset bar
        std::string load_name = RenderPresetBar(
            m_preset_manager, m_config.profiler_id,
            m_current_preset_name, m_config, backend, m_app_window);

        if (!load_name.empty())
        {
            LaunchConfig loaded;
            if (m_preset_manager.LoadPreset(load_name, m_config.profiler_id, loaded))
            {
                m_config = loaded;
                m_backends[m_backend_index]->LoadSettings(m_config.backend_payload);
                // Find matching tool index
                auto tools = backend->GetTools();
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

        ImGui::Separator();

        // Two-column layout: reserve bottom area for preview + output + buttons
        float bottom_reserve = 280.0f;
        float left_width = 200.0f;
        ImGui::BeginChild("LeftPane", ImVec2(left_width, -bottom_reserve), true);
        RenderLeftPane();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("RightPane", ImVec2(0, -bottom_reserve), true);
        RenderRightPane();
        ImGui::EndChild();

        // Command preview
        RenderCommandPreview(backend, m_config, GetProfilerPath());

        // Output console
        RenderOutputConsole(m_output_text, m_error_message,
                           static_cast<int>(m_profiler_state), m_auto_scroll_output);

        // Button row
        RenderButtonRow();
    }
    ImGui::End();

    if (!window_open)
    {
        OnCloseClicked();
        m_show_window = false;
    }
}

void ProfilerLauncherDialog::RenderLeftPane()
{
    // Profiler selector
    ImGui::Text("Profiler:");
    ImGui::PushItemWidth(-1);
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

    ImGui::Spacing();

    // Tool selector
    IProfilerBackend const* backend = m_backends[m_backend_index].get();
    auto tools = backend->GetTools();
    ImGui::Text("Tool:");
    ImGui::PushItemWidth(-1);
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

    ImGui::Spacing();

    // Connection
    RenderConnectionSection(m_config.connection);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Profiler path override
    ImGui::Text("Profiler Path:");
    ImGui::TextDisabled("(empty = use PATH)");
    char path_buf[512];
    std::snprintf(path_buf, sizeof(path_buf), "%s", m_profiler_path_override.c_str());
    ImGui::PushItemWidth(-1);
    if (ImGui::InputText("##ProfPath", path_buf, sizeof(path_buf)))
    {
        m_profiler_path_override = path_buf;
    }
    ImGui::PopItemWidth();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Recent targets
    SettingsManager& settings = SettingsManager::Get();
    ProfilerSettings& prof_settings = settings.GetProfilerSettings();
    if (!prof_settings.recent_targets.empty())
    {
        ImGui::Text("Recent:");
        for (auto const& t : prof_settings.recent_targets)
        {
            std::string short_name = t;
            if (short_name.size() > 25)
            {
                short_name = "..." + short_name.substr(short_name.size() - 22);
            }
            if (ImGui::Selectable(short_name.c_str(), false))
            {
                m_config.target.executable = t;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", t.c_str());
            }
        }
    }
}

void ProfilerLauncherDialog::RenderRightPane()
{
    IProfilerBackend const* backend = m_backends[m_backend_index].get();

    if (ImGui::BeginTabBar("LaunchTabs"))
    {
        // Target tab (shared)
        if (ImGui::BeginTabItem("Target"))
        {
            RenderTargetSection(m_config.target, m_app_window);
            ImGui::EndTabItem();
        }

        // Backend-provided tabs
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
            std::vector<std::pair<std::string, std::string>> curated_env;
            std::vector<std::string> dummy_argv;
            backend->FlattenToExecution(m_config, curated_env, dummy_argv);
            RenderRawEnvVarsTab(m_config.extra_env, curated_env);
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
    if (m_is_running)
    {
        PollProfilerState();
    }

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

    // Build env vars and argv from backend
    std::vector<std::pair<std::string, std::string>> env_vars;
    std::vector<std::string> argv;
    backend->FlattenToExecution(m_config, env_vars, argv);

    // Merge extra_env (user raw env vars override curated)
    for (auto const& kv : m_config.extra_env)
    {
        env_vars.emplace_back(kv.first, kv.second);
    }

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

    std::string profiler_path = GetProfilerPath();

    // Build profiler_args as empty (we pass argv individually via the new API)
    // Join argv into a single string for the legacy API
    std::string profiler_args_str;
    for (size_t i = 0; i < argv.size(); i++)
    {
        if (i > 0) profiler_args_str += " ";
        profiler_args_str += argv[i];
    }

    // Launch via DataProvider (uses the existing C API path for now)
    bool success = m_data_provider.LaunchProfiler(
        profiler_type,
        profiler_path,
        m_config.target.executable,
        m_config.target.arguments,
        m_config.target.output_directory,
        profiler_args_str,
        env_vars);

    if (success)
    {
        m_is_running = true;
        m_profiler_state = kRPVProfilerStateRunning;

        // Build preamble
        std::ostringstream preamble;
        for (auto const& kv : env_vars)
        {
            if (!kv.second.empty())
            {
                preamble << kv.first << "=" << kv.second << " \\\n";
            }
        }
        preamble << "$ " << profiler_path;
        if (!profiler_args_str.empty())
        {
            preamble << " " << profiler_args_str;
        }
        if (!m_config.target.output_directory.empty())
        {
            preamble << " --output " << m_config.target.output_directory;
        }
        preamble << " -- " << m_config.target.executable;
        if (!m_config.target.arguments.empty())
        {
            preamble << " " << m_config.target.arguments;
        }
        preamble << "\n\n";
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
    if (m_data_provider.CancelProfiler())
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
        m_data_provider.CancelProfiler();
    }

    m_data_provider.CloseProfiler();
    m_is_running = false;
    m_profiler_state = kRPVProfilerStateIdle;
}

void ProfilerLauncherDialog::PollProfilerState()
{
    rocprofvis_profiler_state_t new_state = m_data_provider.GetProfilerState();

    if (new_state != m_profiler_state)
    {
        m_profiler_state = new_state;

        if (new_state == kRPVProfilerStateCompleted)
        {
            m_is_running = false;
            m_output_epilogue += "\nProfiler completed successfully.\n";

            std::string trace_path = m_data_provider.GetProfilerTracePath();
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
            int32_t exit_code = m_data_provider.GetProfilerExitCode();
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
}

void ProfilerLauncherDialog::UpdateOutput()
{
    std::string new_raw = m_data_provider.GetProfilerOutput();
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
