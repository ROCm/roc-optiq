// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_profiler_launcher_dialog.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_settings_manager.h"
#include "imgui.h"
#include <cstdio>
#include <cstring>

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
    , m_profiler_type_index(0)
    , m_profiler_state(kRPVProfilerStateIdle)
    , m_output_text()
    , m_error_message()
    , m_auto_scroll_output(true)
{
    // Initialize input buffers
    std::memset(m_profiler_path, 0, sizeof(m_profiler_path));
    std::memset(m_target_executable, 0, sizeof(m_target_executable));
    std::memset(m_target_args, 0, sizeof(m_target_args));
    std::memset(m_output_directory, 0, sizeof(m_output_directory));
    std::memset(m_profiler_args, 0, sizeof(m_profiler_args));

    // Load settings from SettingsManager
    SettingsManager& settings = SettingsManager::Get();
    ProfilerSettings& profiler_settings = settings.GetProfilerSettings();

    if (profiler_settings.profiler_path != "")
    {
        std::snprintf(
            m_profiler_path,
            sizeof(m_profiler_path),
            "%s",
            profiler_settings.profiler_path.c_str());
    }

    if (profiler_settings.profiler_output_directory != "")
    {
        std::snprintf(
            m_output_directory,
            sizeof(m_output_directory),
            "%s",
            profiler_settings.profiler_output_directory.c_str());
    }
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
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

    bool window_open = true;
    if (ImGui::Begin("Launch Profiler", &window_open, ImGuiWindowFlags_NoScrollbar))
    {
        // Profiler type selection
        ImGui::Text("Profiler Type:");
        ImGui::SameLine();
        const char* profiler_types[] = {
            "ROCm Systems Profiler (Sample)",
            "ROCm Systems Profiler (Instrument)",
            "ROCm Compute Profiler (v2)",
            "ROCm Compute Profiler (v3)"
        };
        ImGui::Combo("##ProfilerType", &m_profiler_type_index, profiler_types, IM_ARRAYSIZE(profiler_types));

        ImGui::Separator();

        // Profiler path (optional - defaults to system PATH)
        ImGui::Text("Profiler Path (optional):");
        ImGui::InputText("##ProfilerPath", m_profiler_path, sizeof(m_profiler_path));
        ImGui::SameLine();
        if (ImGui::Button("Browse##Profiler"))
        {
            OnBrowseProfilerPath();
        }
        if (std::strlen(m_profiler_path) == 0)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("(will use system PATH)");
        }

        // Target executable
        ImGui::Text("Target Executable:");
        ImGui::InputText("##TargetExec", m_target_executable, sizeof(m_target_executable));
        ImGui::SameLine();
        if (ImGui::Button("Browse##Target"))
        {
            OnBrowseTargetExecutable();
        }

        // Target arguments
        ImGui::Text("Target Arguments:");
        ImGui::InputText("##TargetArgs", m_target_args, sizeof(m_target_args));

        // Output directory
        ImGui::Text("Output Directory:");
        ImGui::InputText("##OutputDir", m_output_directory, sizeof(m_output_directory));
        ImGui::SameLine();
        if (ImGui::Button("Browse##Output"))
        {
            OnBrowseOutputDirectory();
        }

        // Profiler arguments
        ImGui::Text("Profiler Arguments:");
        ImGui::InputText("##ProfilerArgs", m_profiler_args, sizeof(m_profiler_args));

        ImGui::Separator();

        // Output section
        ImGui::Text("Profiler Output:");

        // State indicator
        const char* state_text = "Idle";
        ImVec4 state_color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);

        switch (m_profiler_state)
        {
            case kRPVProfilerStateRunning:
                state_text = "Running";
                state_color = ImVec4(0.0f, 0.8f, 0.0f, 1.0f);
                break;
            case kRPVProfilerStateCompleted:
                state_text = "Completed";
                state_color = ImVec4(0.0f, 0.6f, 1.0f, 1.0f);
                break;
            case kRPVProfilerStateFailed:
                state_text = "Failed";
                state_color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
                break;
            case kRPVProfilerStateCancelled:
                state_text = "Cancelled";
                state_color = ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
                break;
            default:
                break;
        }

        ImGui::SameLine();
        ImGui::TextColored(state_color, "[%s]", state_text);

        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &m_auto_scroll_output);

        // Output text area
        ImGuiWindowFlags output_flags = ImGuiWindowFlags_HorizontalScrollbar;
        ImGui::BeginChild("OutputText", ImVec2(0, -40), true, output_flags);

        if (!m_error_message.empty())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", m_error_message.c_str());
            ImGui::Separator();
        }

        ImGui::TextUnformatted(m_output_text.c_str());

        if (m_auto_scroll_output && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();

        // Button row
        ImGui::Separator();

        bool can_launch = !m_is_running && (m_profiler_state == kRPVProfilerStateIdle || m_profiler_state == kRPVProfilerStateCompleted || m_profiler_state == kRPVProfilerStateFailed || m_profiler_state == kRPVProfilerStateCancelled);
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
    ImGui::End();

    if (!window_open)
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
        UpdateOutput();
    }
}

void ProfilerLauncherDialog::OnLaunchClicked()
{
    // Validate inputs
    if (std::strlen(m_target_executable) == 0)
    {
        m_error_message = "Error: Target executable is required";
        return;
    }

    if (std::strlen(m_output_directory) == 0)
    {
        m_error_message = "Error: Output directory is required";
        return;
    }

    // Clear previous state
    m_error_message.clear();
    m_output_text.clear();

    // Map profiler type index to enum and determine default executable name
    rocprofvis_profiler_type_t profiler_type;
    std::string default_profiler_name;

    switch (m_profiler_type_index)
    {
        case 0:
            profiler_type = kRPVProfilerTypeRocprofSysSample;
            default_profiler_name = "rocprof-sys-sample";
            break;
        case 1:
            profiler_type = kRPVProfilerTypeRocprofSysInstrument;
            default_profiler_name = "rocprof-sys-instrument";
            break;
        case 2:
            profiler_type = kRPVProfilerTypeRocprofCompute;
            default_profiler_name = "rocprof-compute";
            break;
        case 3:
            profiler_type = kRPVProfilerTypeRocprofV3;
            default_profiler_name = "rocprofv3";
            break;
        default:
            profiler_type = kRPVProfilerTypeRocprofSysSample;
            default_profiler_name = "rocprof-sys-sample";
            break;
    }

    // Use specified path or default to executable name (will be found in PATH)
    std::string profiler_path = std::strlen(m_profiler_path) > 0
        ? std::string(m_profiler_path)
        : default_profiler_name;

    // Launch profiler via DataProvider
    bool success = m_data_provider.LaunchProfiler(
        profiler_type,
        profiler_path,
        std::string(m_target_executable),
        std::string(m_target_args),
        std::string(m_output_directory),
        std::string(m_profiler_args)
    );

    if (success)
    {
        m_is_running = true;
        m_profiler_state = kRPVProfilerStateRunning;
        m_output_text = "Profiler launched...\n";

        // Save settings
        SettingsManager& settings = SettingsManager::Get();
        ProfilerSettings& profiler_settings = settings.GetProfilerSettings();
        profiler_settings.profiler_path = std::string(m_profiler_path);
        profiler_settings.profiler_output_directory = std::string(m_output_directory);
        settings.SaveProfilerSettings();
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
        m_output_text += "\nProfiler cancelled by user.\n";
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
    // Clean up profiler if still running
    if (m_is_running)
    {
        m_data_provider.CancelProfiler();
    }

    m_data_provider.CloseProfiler();
    m_is_running = false;
    m_profiler_state = kRPVProfilerStateIdle;
}

void ProfilerLauncherDialog::OnBrowseProfilerPath()
{
    if (!m_app_window)
    {
        return;
    }

    std::vector<FileFilter> filters = {
        {"Executables", {".exe"}},
        {"All Files", {".*"}}
    };

    m_app_window->ShowOpenFileDialog(
        "Choose Profiler Executable",
        filters,
        "",
        [this](const std::string& path) { OnProfilerPathSelected(path); }
    );
}

void ProfilerLauncherDialog::OnBrowseTargetExecutable()
{
    if (!m_app_window)
    {
        return;
    }

    std::vector<FileFilter> filters = {
        {"Executables", {".exe"}},
        {"All Files", {".*"}}
    };

    m_app_window->ShowOpenFileDialog(
        "Choose Target Executable",
        filters,
        "",
        [this](const std::string& path) { OnTargetExecutableSelected(path); }
    );
}

void ProfilerLauncherDialog::OnBrowseOutputDirectory()
{
    if (!m_app_window)
    {
        return;
    }

    m_app_window->ShowPathPickerDialog(
        "Choose Output Directory",
        "",
        [this](const std::string& path) { OnOutputDirectorySelected(path); }
    );
}

void ProfilerLauncherDialog::OnProfilerPathSelected(const std::string& path)
{
    std::snprintf(m_profiler_path, sizeof(m_profiler_path), "%s", path.c_str());
}

void ProfilerLauncherDialog::OnTargetExecutableSelected(const std::string& path)
{
    std::snprintf(m_target_executable, sizeof(m_target_executable), "%s", path.c_str());
}

void ProfilerLauncherDialog::OnOutputDirectorySelected(const std::string& path)
{
    std::snprintf(m_output_directory, sizeof(m_output_directory), "%s", path.c_str());
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
            m_output_text += "\nProfiler completed successfully.\n";

            // Get trace path
            std::string trace_path = m_data_provider.GetProfilerTracePath();
            if (!trace_path.empty())
            {
                m_output_text += "Trace file: " + trace_path + "\n";

                // TODO: Optionally auto-load the trace
                // m_app_window->LoadTrace(trace_path);
            }
        }
        else if (new_state == kRPVProfilerStateFailed)
        {
            m_is_running = false;
            m_output_text += "\nProfiler failed.\n";
            m_error_message = "Profiler execution failed";
        }
        else if (new_state == kRPVProfilerStateCancelled)
        {
            m_is_running = false;
        }
    }
}

void ProfilerLauncherDialog::UpdateOutput()
{
    std::string new_output = m_data_provider.GetProfilerOutput();

    if (!new_output.empty() && new_output != m_output_text)
    {
        m_output_text = new_output;
    }
}

}  // namespace View
}  // namespace RocProfVis
