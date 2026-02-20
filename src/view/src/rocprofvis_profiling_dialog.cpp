// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_profiling_dialog.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_controller_ai_analysis.h"
#include "widgets/rocprofvis_dialog.h"
#include "widgets/rocprofvis_gui_helpers.h"

#include "imgui.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <thread>

namespace RocProfVis
{
namespace View
{

ProfilingDialog::ProfilingDialog()
: m_visible(false)
, m_current_screen(DialogScreen::ModeSelection)
, m_ssh_port_input(22)
, m_execution_running(false)
, m_ssh_test_running(false)
, m_execution_success(false)
, m_selected_recent_connection(0)
, m_current_stage(ProgressStage::None)
, m_selected_container_index(-1)
, m_containers_loaded(false)
, m_container_detection_running(false)
, m_slurm_nodes_input(1)
, m_slurm_ntasks_input(1)
, m_slurm_cpus_per_task_input(1)
, m_slurm_gpus_input(1)
, m_slurm_available(false)
, m_job_monitoring_active(false)
{
    m_widget_name = GenUniqueName("ProfilingDialog");

    std::memset(m_app_path_buffer, 0, sizeof(m_app_path_buffer));
    std::memset(m_app_args_buffer, 0, sizeof(m_app_args_buffer));
    std::memset(m_tool_args_buffer, 0, sizeof(m_tool_args_buffer));
    std::memset(m_output_dir_buffer, 0, sizeof(m_output_dir_buffer));
    std::memset(m_ssh_host_buffer, 0, sizeof(m_ssh_host_buffer));
    std::memset(m_ssh_user_buffer, 0, sizeof(m_ssh_user_buffer));
    std::memset(m_remote_app_buffer, 0, sizeof(m_remote_app_buffer));
    std::memset(m_remote_args_buffer, 0, sizeof(m_remote_args_buffer));
    std::memset(m_remote_output_buffer, 0, sizeof(m_remote_output_buffer));
    std::memset(m_ssh_key_path_buffer, 0, sizeof(m_ssh_key_path_buffer));
    std::memset(m_ssh_proxy_jump_buffer, 0, sizeof(m_ssh_proxy_jump_buffer));
    std::memset(m_ssh_options_buffer, 0, sizeof(m_ssh_options_buffer));
    std::memset(m_llm_api_key_buffer, 0, sizeof(m_llm_api_key_buffer));
    std::memset(m_connection_name_buffer, 0, sizeof(m_connection_name_buffer));
    std::memset(m_pre_commands_buffer, 0, sizeof(m_pre_commands_buffer));
    std::memset(m_env_var_key_buffer, 0, sizeof(m_env_var_key_buffer));
    std::memset(m_env_var_value_buffer, 0, sizeof(m_env_var_value_buffer));
    std::memset(m_container_exec_options_buffer, 0, sizeof(m_container_exec_options_buffer));
    std::memset(m_slurm_partition_buffer, 0, sizeof(m_slurm_partition_buffer));
    std::memset(m_slurm_account_buffer, 0, sizeof(m_slurm_account_buffer));
    std::memset(m_slurm_time_buffer, 0, sizeof(m_slurm_time_buffer));
    std::memset(m_slurm_job_name_buffer, 0, sizeof(m_slurm_job_name_buffer));
    std::memset(m_slurm_extra_args_buffer, 0, sizeof(m_slurm_extra_args_buffer));

    // Set default SLURM values
    std::strcpy(m_slurm_time_buffer, "01:00:00");
    std::strcpy(m_slurm_job_name_buffer, "rocprof_job");

    // Set default tool arguments with comprehensive tracing
    std::strcpy(m_tool_args_buffer, "--sys-trace --kernel-trace --memory-copy-trace");

    // Set default output directory
#ifdef _WIN32
    std::strcpy(m_output_dir_buffer, "C:\\temp\\rocprof_output");
#else
    std::strcpy(m_output_dir_buffer, "/tmp/rocprof_output");
#endif
    std::strcpy(m_remote_output_buffer, "/tmp/rocprof_output");
}

ProfilingDialog::~ProfilingDialog()
{
    if(m_execution_context)
    {
        m_execution_context->Cancel();
    }
}

void ProfilingDialog::Show()
{
    m_visible = true;
    m_current_screen = DialogScreen::ModeSelection;
    m_execution_running = false;
    m_execution_success = false;
    m_error_message.clear();
    m_progress_output.clear();

    // Detect available job schedulers
    DetectJobScheduler();

    // Pre-populate API key from settings if available and not already set
    const auto& user_settings = SettingsManager::GetInstance().GetUserSettings();

    // Only populate if the buffer is empty (user hasn't customized it yet in this session)
    if(std::strlen(m_llm_api_key_buffer) == 0)
    {
        // Default to Anthropic key based on current LLM provider
        if(m_config.llm_provider == LLMProvider::Anthropic && !user_settings.anthropic_api_key.empty())
        {
            std::strncpy(m_llm_api_key_buffer, user_settings.anthropic_api_key.c_str(), sizeof(m_llm_api_key_buffer) - 1);
        }
        else if(m_config.llm_provider == LLMProvider::OpenAI && !user_settings.openai_api_key.empty())
        {
            std::strncpy(m_llm_api_key_buffer, user_settings.openai_api_key.c_str(), sizeof(m_llm_api_key_buffer) - 1);
        }
    }
}

void ProfilingDialog::ShowWithConfig(const ProfilingConfig& config)
{
    // Copy the provided config
    m_config = config;

    // Populate UI buffers from config (ensuring null termination)
    std::strncpy(m_app_path_buffer, m_config.application_path.c_str(), sizeof(m_app_path_buffer) - 1);
    m_app_path_buffer[sizeof(m_app_path_buffer) - 1] = '\0';
    std::strncpy(m_app_args_buffer, m_config.application_args.c_str(), sizeof(m_app_args_buffer) - 1);
    m_app_args_buffer[sizeof(m_app_args_buffer) - 1] = '\0';
    std::strncpy(m_tool_args_buffer, m_config.tool_args.c_str(), sizeof(m_tool_args_buffer) - 1);
    m_tool_args_buffer[sizeof(m_tool_args_buffer) - 1] = '\0';
    std::strncpy(m_output_dir_buffer, m_config.output_directory.c_str(), sizeof(m_output_dir_buffer) - 1);
    m_output_dir_buffer[sizeof(m_output_dir_buffer) - 1] = '\0';

    std::strncpy(m_ssh_host_buffer, m_config.ssh_host.c_str(), sizeof(m_ssh_host_buffer) - 1);
    m_ssh_host_buffer[sizeof(m_ssh_host_buffer) - 1] = '\0';
    std::strncpy(m_ssh_user_buffer, m_config.ssh_user.c_str(), sizeof(m_ssh_user_buffer) - 1);
    m_ssh_user_buffer[sizeof(m_ssh_user_buffer) - 1] = '\0';
    m_ssh_port_input = m_config.ssh_port;
    std::strncpy(m_ssh_key_path_buffer, m_config.ssh_key_path.c_str(), sizeof(m_ssh_key_path_buffer) - 1);
    m_ssh_key_path_buffer[sizeof(m_ssh_key_path_buffer) - 1] = '\0';
    std::strncpy(m_ssh_proxy_jump_buffer, m_config.ssh_proxy_jump.c_str(), sizeof(m_ssh_proxy_jump_buffer) - 1);
    m_ssh_proxy_jump_buffer[sizeof(m_ssh_proxy_jump_buffer) - 1] = '\0';
    std::strncpy(m_ssh_options_buffer, m_config.ssh_options.c_str(), sizeof(m_ssh_options_buffer) - 1);
    m_ssh_options_buffer[sizeof(m_ssh_options_buffer) - 1] = '\0';
    std::strncpy(m_remote_app_buffer, m_config.remote_app_path.c_str(), sizeof(m_remote_app_buffer) - 1);
    m_remote_app_buffer[sizeof(m_remote_app_buffer) - 1] = '\0';
    std::strncpy(m_remote_args_buffer, m_config.remote_args.c_str(), sizeof(m_remote_args_buffer) - 1);
    m_remote_args_buffer[sizeof(m_remote_args_buffer) - 1] = '\0';
    std::strncpy(m_remote_output_buffer, m_config.remote_output_path.c_str(), sizeof(m_remote_output_buffer) - 1);
    m_remote_output_buffer[sizeof(m_remote_output_buffer) - 1] = '\0';

    std::strncpy(m_container_exec_options_buffer, m_config.container_exec_options.c_str(), sizeof(m_container_exec_options_buffer) - 1);
    m_container_exec_options_buffer[sizeof(m_container_exec_options_buffer) - 1] = '\0';

    std::strncpy(m_slurm_partition_buffer, m_config.slurm_partition.c_str(), sizeof(m_slurm_partition_buffer) - 1);
    m_slurm_partition_buffer[sizeof(m_slurm_partition_buffer) - 1] = '\0';
    std::strncpy(m_slurm_account_buffer, m_config.slurm_account.c_str(), sizeof(m_slurm_account_buffer) - 1);
    m_slurm_account_buffer[sizeof(m_slurm_account_buffer) - 1] = '\0';
    m_slurm_nodes_input = m_config.slurm_nodes;
    m_slurm_ntasks_input = m_config.slurm_ntasks;
    m_slurm_cpus_per_task_input = m_config.slurm_cpus_per_task;
    m_slurm_gpus_input = m_config.slurm_gpus;
    std::strncpy(m_slurm_time_buffer, m_config.slurm_time.c_str(), sizeof(m_slurm_time_buffer) - 1);
    m_slurm_time_buffer[sizeof(m_slurm_time_buffer) - 1] = '\0';
    std::strncpy(m_slurm_job_name_buffer, m_config.slurm_job_name.c_str(), sizeof(m_slurm_job_name_buffer) - 1);
    m_slurm_job_name_buffer[sizeof(m_slurm_job_name_buffer) - 1] = '\0';
    std::strncpy(m_slurm_extra_args_buffer, m_config.slurm_extra_args.c_str(), sizeof(m_slurm_extra_args_buffer) - 1);
    m_slurm_extra_args_buffer[sizeof(m_slurm_extra_args_buffer) - 1] = '\0';

    // Show the dialog starting at the appropriate config screen based on mode
    m_visible = true;
    switch(m_config.mode)
    {
        case ExecutionMode::Local:
            m_current_screen = DialogScreen::LocalConfig;
            break;
        case ExecutionMode::Remote:
            m_current_screen = DialogScreen::RemoteConfig;
            break;
        case ExecutionMode::SLURM:
            m_current_screen = DialogScreen::SLURMConfig;
            break;
    }

    m_execution_running = false;
    m_execution_success = false;
    m_error_message.clear();
    m_progress_output.clear();

    DetectJobScheduler();
}

void ProfilingDialog::Hide()
{
    m_visible = false;
}

bool ProfilingDialog::IsVisible() const
{
    return m_visible;
}

void ProfilingDialog::SetCompletionCallback(std::function<void(const std::string&, const std::string&)> callback)
{
    m_completion_callback = callback;
}

void ProfilingDialog::Render()
{
    if(!m_visible)
    {
        return;
    }

    // Open popup if not already open (must be called before BeginPopupModal)
    if(!ImGui::IsPopupOpen("Run Profiling##profiling_dialog"))
    {
        ImGui::OpenPopup("Run Profiling##profiling_dialog");
    }

    PopUpStyle popup_style;
    popup_style.CenterPopup();

    // Larger dialog to fit all UI elements with scrollbar if needed
    ImGui::SetNextWindowSize(ImVec2(850, 600), ImGuiCond_FirstUseEver);

    if(ImGui::BeginPopupModal("Run Profiling##profiling_dialog", &m_visible,
                              ImGuiWindowFlags_None))  // Allow resize and scrolling
    {
        switch(m_current_screen)
        {
            case DialogScreen::ModeSelection:
                RenderModeSelection();
                break;
            case DialogScreen::LocalConfig:
                RenderLocalConfig();
                break;
            case DialogScreen::RemoteConfig:
                RenderRemoteConfig();
                break;
            case DialogScreen::SLURMConfig:
                RenderSLURMConfig();
                break;
            case DialogScreen::Progress:
                RenderProgress();
                break;
            case DialogScreen::Complete:
                RenderComplete();
                break;
        }

        ImGui::EndPopup();
    }
}

void ProfilingDialog::RenderModeSelection()
{
    ImGui::TextUnformatted("Where would you like to run profiling?");
    ImGui::Spacing();
    ImGui::Spacing();

    int mode = static_cast<int>(m_config.mode);
    ImGui::RadioButton("Local Machine", &mode, static_cast<int>(ExecutionMode::Local));
    ImGui::RadioButton("Remote via SSH", &mode, static_cast<int>(ExecutionMode::Remote));

    // Show SLURM option if available
    if(m_slurm_available || m_config.mode == ExecutionMode::SLURM)
    {
        ImGui::RadioButton("Submit to SLURM Job Scheduler", &mode, static_cast<int>(ExecutionMode::SLURM));
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Submit profiling job to SLURM cluster (detected: sbatch available)");
        }
    }

    m_config.mode = static_cast<ExecutionMode>(mode);

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    float button_width = 100.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float total_width = button_width * 2 + spacing;
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - total_width - 20.0f);

    if(ImGui::Button("Cancel", ImVec2(button_width, 0)))
    {
        Hide();
    }

    ImGui::SameLine();

    if(ImGui::Button("Next", ImVec2(button_width, 0)))
    {
        if(m_config.mode == ExecutionMode::Local)
        {
            m_current_screen = DialogScreen::LocalConfig;
        }
        else if(m_config.mode == ExecutionMode::Remote)
        {
            m_current_screen = DialogScreen::RemoteConfig;
        }
        else if(m_config.mode == ExecutionMode::SLURM)
        {
            m_current_screen = DialogScreen::SLURMConfig;
        }
    }
}

void ProfilingDialog::RenderLocalConfig()
{
    ImGui::TextUnformatted("Local Profiling Configuration");
    ImGui::Separator();
    ImGui::Spacing();

    // Application path
    ImGui::TextUnformatted("Application:");
    ImGui::SameLine(150);
    ImGui::PushItemWidth(-80);
    ImGui::InputText("##app_path", m_app_path_buffer, sizeof(m_app_path_buffer));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if(ImGui::Button("Browse...##app"))
    {
        AppWindow::GetInstance()->ShowOpenFileDialog(
            "Select Application",
            {{"All Files", {"*"}}},
            "",
            [this](const std::string& path)
            {
                if(!path.empty())
                {
                    std::strncpy(m_app_path_buffer, path.c_str(), sizeof(m_app_path_buffer) - 1);
                }
            }
        );
    }

    // Arguments
    ImGui::TextUnformatted("Arguments:");
    ImGui::SameLine(150);
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##app_args", m_app_args_buffer, sizeof(m_app_args_buffer));
    ImGui::PopItemWidth();

    ImGui::Spacing();

    // Profiling tool selection
    ImGui::TextUnformatted("Profiling Tool:");
    ImGui::SameLine(150);
    // Only rocprofv3 is supported for now (rocprof-compute and rocprof-sys disabled)
    const char* tools[] = {"rocprofv3"};
    int current_tool = 0;  // Always rocprofv3
    ImGui::Combo("##tool", &current_tool, tools, IM_ARRAYSIZE(tools));
    m_config.tool = ProfilingTool::RocProfV3;

    ImGui::Spacing();

    // Tool arguments (e.g., --sys-trace, --hip-trace)
    ImGui::TextUnformatted("Tool Arguments:");
    ImGui::SameLine(150);
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##tool_args", m_tool_args_buffer, sizeof(m_tool_args_buffer));
    ImGui::PopItemWidth();
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Examples: --sys-trace, --hip-trace, --kernel-trace");
    }

    ImGui::Spacing();

    // Output directory
    ImGui::TextUnformatted("Output Directory:");
    ImGui::SameLine(150);
    ImGui::PushItemWidth(-80);
    ImGui::InputText("##output_dir", m_output_dir_buffer, sizeof(m_output_dir_buffer));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if(ImGui::Button("Browse...##out"))
    {
        AppWindow::GetInstance()->ShowOpenFileDialog(
            "Select Output Directory",
            {{"All Files", {"*"}}},
            "",
            [this](const std::string& path)
            {
                if(!path.empty())
                {
                    std::strncpy(m_output_dir_buffer, path.c_str(), sizeof(m_output_dir_buffer) - 1);
                }
            }
        );
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Container options
    ImGui::TextUnformatted("Container Options:");
    ImGui::Spacing();

    ImGui::Checkbox("Run in Container", &m_config.use_container);
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Execute profiling inside a Docker/Podman/Singularity container");
    }

    if(m_config.use_container)
    {
        ImGui::Indent(20.0f);

        // Detect containers button
        if(ImGui::Button("Detect Containers"))
        {
            DetectContainers();
        }
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Scan for running Docker/Podman containers");
        }

        ImGui::SameLine();
        if(m_containers_loaded)
        {
            ImGui::TextDisabled("(%zu containers found)", m_detected_containers.size());
        }

        // Container selection dropdown
        if(!m_detected_containers.empty())
        {
            ImGui::Spacing();
            ImGui::TextUnformatted("Select Container:");
            ImGui::SameLine(150);

            // Build combo items
            std::vector<std::string> container_labels;
            for(const auto& container : m_detected_containers)
            {
                std::string runtime_str;
                switch(container.runtime)
                {
                    case ContainerRuntime::Docker: runtime_str = "[Docker]"; break;
                    case ContainerRuntime::Podman: runtime_str = "[Podman]"; break;
                    case ContainerRuntime::Singularity: runtime_str = "[Singularity]"; break;
                    default: runtime_str = "[Unknown]"; break;
                }

                container_labels.push_back(runtime_str + " " + container.name + " (" + container.image + ")");
            }

            // Create char* array for ImGui
            std::vector<const char*> container_labels_cstr;
            for(const auto& label : container_labels)
            {
                container_labels_cstr.push_back(label.c_str());
            }

            ImGui::PushItemWidth(-1);
            if(ImGui::Combo("##container_select", &m_selected_container_index,
                           container_labels_cstr.data(), container_labels_cstr.size()))
            {
                // Update config when selection changes
                if(m_selected_container_index >= 0 && m_selected_container_index < static_cast<int>(m_detected_containers.size()))
                {
                    const auto& selected = m_detected_containers[m_selected_container_index];
                    m_config.container_runtime = selected.runtime;
                    m_config.container_id = selected.id;
                    m_config.container_image = selected.image;
                }
            }
            ImGui::PopItemWidth();
        }
        else if(m_containers_loaded)
        {
            ImGui::Spacing();
            ImGui::TextDisabled("No running containers detected. Click 'Detect Containers' to scan.");
        }

        // Manual container specification
        ImGui::Spacing();
        ImGui::TextUnformatted("Or specify manually:");

        ImGui::TextUnformatted("Runtime:");
        ImGui::SameLine(150);
        const char* runtimes[] = {"Docker", "Podman", "Singularity"};
        int runtime_idx = static_cast<int>(m_config.container_runtime) - 1; // -1 because None is 0
        if(runtime_idx < 0) runtime_idx = 0;
        if(ImGui::Combo("##manual_runtime", &runtime_idx, runtimes, IM_ARRAYSIZE(runtimes)))
        {
            m_config.container_runtime = static_cast<ContainerRuntime>(runtime_idx + 1);
        }

        ImGui::TextUnformatted("Container ID/Name:");
        ImGui::SameLine(150);
        char manual_container_id[256] = {0};
        std::strncpy(manual_container_id, m_config.container_id.c_str(), sizeof(manual_container_id) - 1);
        ImGui::PushItemWidth(-1);
        if(ImGui::InputText("##manual_container_id", manual_container_id, sizeof(manual_container_id)))
        {
            m_config.container_id = manual_container_id;
        }
        ImGui::PopItemWidth();
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Container ID, name, or .sif file path for Singularity");
        }

        ImGui::TextUnformatted("Exec Options:");
        ImGui::SameLine(150);
        ImGui::PushItemWidth(-1);
        ImGui::InputText("##container_exec_options", m_container_exec_options_buffer, sizeof(m_container_exec_options_buffer));
        ImGui::PopItemWidth();
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Additional options for container exec (e.g., --privileged, -e VAR=value)");
        }

        ImGui::Unindent(20.0f);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // AI Analysis enable/disable checkbox
    ImGui::Checkbox("Run AI Analysis", &m_config.run_ai_analysis);
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(
            "Enable AI-powered performance analysis with rocpd analyze.\n"
            "When enabled, generates insights and recommendations.\n"
            "Disable to skip analysis and only collect trace data.");
    }

    // Only show LLM options if AI analysis is enabled
    if(m_config.run_ai_analysis)
    {
        ImGui::Spacing();

        // AI Analysis LLM options
        ImGui::TextUnformatted("AI Analysis LLM:");
        ImGui::SameLine(150);
        const char* llm_providers[] = {"Local (No LLM)", "Anthropic Claude", "OpenAI GPT"};
        int current_llm = static_cast<int>(m_config.llm_provider);
        if(ImGui::Combo("##llm_provider", &current_llm, llm_providers, IM_ARRAYSIZE(llm_providers)))
        {
            LLMProvider old_provider = m_config.llm_provider;
            m_config.llm_provider = static_cast<LLMProvider>(current_llm);

            // Auto-populate API key from settings when provider changes
            const auto& user_settings = SettingsManager::GetInstance().GetUserSettings();
            if(m_config.llm_provider == LLMProvider::Anthropic && !user_settings.anthropic_api_key.empty())
            {
                std::strncpy(m_llm_api_key_buffer, user_settings.anthropic_api_key.c_str(), sizeof(m_llm_api_key_buffer) - 1);
            }
            else if(m_config.llm_provider == LLMProvider::OpenAI && !user_settings.openai_api_key.empty())
            {
                std::strncpy(m_llm_api_key_buffer, user_settings.openai_api_key.c_str(), sizeof(m_llm_api_key_buffer) - 1);
            }
            else if(m_config.llm_provider == LLMProvider::Local)
            {
                // Clear the buffer when switching to Local
                m_llm_api_key_buffer[0] = '\0';
            }
        }
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Select LLM provider for AI-powered analysis insights");
        }

        // Show API key field only for Anthropic or OpenAI
        if(m_config.llm_provider != LLMProvider::Local)
        {
            ImGui::TextUnformatted("API Key:");
            ImGui::SameLine(150);
            ImGui::PushItemWidth(-1);
            ImGui::InputText("##llm_api_key", m_llm_api_key_buffer, sizeof(m_llm_api_key_buffer), ImGuiInputTextFlags_Password);
            ImGui::PopItemWidth();
            if(ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(m_config.llm_provider == LLMProvider::Anthropic
                    ? "Anthropic API key (get from https://console.anthropic.com/)"
                    : "OpenAI API key (get from https://platform.openai.com/api-keys)");
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Buttons
    float button_width = 100.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float total_width = button_width * 2 + spacing;
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - total_width - 20.0f);

    if(ImGui::Button("< Back", ImVec2(button_width, 0)))
    {
        m_current_screen = DialogScreen::ModeSelection;
    }

    ImGui::SameLine();

    bool can_run = std::strlen(m_app_path_buffer) > 0 && std::strlen(m_output_dir_buffer) > 0;
    ImGui::BeginDisabled(!can_run);
    if(ImGui::Button("Run", ImVec2(button_width, 0)))
    {
        m_config.application_path = m_app_path_buffer;
        m_config.application_args = m_app_args_buffer;
        m_config.tool_args = m_tool_args_buffer;
        m_config.output_directory = m_output_dir_buffer;
        m_config.llm_api_key = m_llm_api_key_buffer;
        ExecuteProfiling();
    }
    ImGui::EndDisabled();
}

void ProfilingDialog::RenderRemoteConfig()
{
    ImGui::TextUnformatted("Remote Profiling Configuration");
    ImGui::Separator();
    ImGui::Spacing();

    // Recent connections dropdown
    auto& recent_connections = SettingsManager::GetInstance().GetInternalSettings().recent_connections;
    if(!recent_connections.empty())
    {
        ImGui::TextUnformatted("Recent Connections:");
        ImGui::SameLine(150);

        // Build combo items
        std::vector<std::string> connection_names;
        connection_names.push_back("-- New Connection --");
        for(const auto& conn : recent_connections)
        {
            connection_names.push_back(conn.name);
        }

        // Create combo label
        const char* preview = m_selected_recent_connection == 0
            ? connection_names[0].c_str()
            : connection_names[m_selected_recent_connection].c_str();

        if(ImGui::BeginCombo("##recent_conn", preview))
        {
            for(int i = 0; i < static_cast<int>(connection_names.size()); i++)
            {
                bool is_selected = (m_selected_recent_connection == i);
                if(ImGui::Selectable(connection_names[i].c_str(), is_selected))
                {
                    m_selected_recent_connection = i;

                    // Load connection details if not "New Connection"
                    if(i > 0)
                    {
                        auto it = recent_connections.begin();
                        std::advance(it, i - 1);
                        const auto& conn = *it;

                        std::strncpy(m_ssh_host_buffer, conn.ssh_host.c_str(), sizeof(m_ssh_host_buffer) - 1);
                        std::strncpy(m_ssh_user_buffer, conn.ssh_user.c_str(), sizeof(m_ssh_user_buffer) - 1);
                        m_ssh_port_input = conn.ssh_port;
                        std::strncpy(m_remote_app_buffer, conn.remote_app_path.c_str(), sizeof(m_remote_app_buffer) - 1);
                        std::strncpy(m_remote_output_buffer, conn.remote_output_path.c_str(), sizeof(m_remote_output_buffer) - 1);
                    }
                }
                if(is_selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Spacing();
    }

    // Connection name (optional)
    ImGui::TextUnformatted("Connection Name:");
    ImGui::SameLine(150);
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##connection_name", m_connection_name_buffer, sizeof(m_connection_name_buffer));
    ImGui::PopItemWidth();
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Optional: Name this connection for easy recall (e.g., 'Lab Server', 'Dev Machine')");
    }

    ImGui::Spacing();

    // SSH connection details
    ImGui::TextUnformatted("Host:");
    ImGui::SameLine(150);
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##ssh_host", m_ssh_host_buffer, sizeof(m_ssh_host_buffer));
    ImGui::PopItemWidth();

    ImGui::TextUnformatted("User:");
    ImGui::SameLine(150);
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##ssh_user", m_ssh_user_buffer, sizeof(m_ssh_user_buffer));
    ImGui::PopItemWidth();

    ImGui::TextUnformatted("Port:");
    ImGui::SameLine(150);
    ImGui::PushItemWidth(100);  // Fixed width for port number
    ImGui::InputInt("##ssh_port", &m_ssh_port_input);
    ImGui::PopItemWidth();

    // SSH Key Path (optional)
    ImGui::TextUnformatted("SSH Key Path:");
    ImGui::SameLine(150);
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##ssh_key_path", m_ssh_key_path_buffer, sizeof(m_ssh_key_path_buffer));
    ImGui::PopItemWidth();
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Optional: Path to SSH private key file (e.g., ~/.ssh/id_rsa)\nLeave empty to use default SSH agent or default keys");
    }

    // ProxyJump/Bastion Host (optional)
    ImGui::TextUnformatted("Jump Host:");
    ImGui::SameLine(150);
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##ssh_proxy_jump", m_ssh_proxy_jump_buffer, sizeof(m_ssh_proxy_jump_buffer));
    ImGui::PopItemWidth();
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(
            "Optional: Bastion/jump host for SSH tunneling\n"
            "Format: user@hostname or user@hostname:port\n"
            "Examples:\n"
            "  user@bastion.example.com\n"
            "  jumpuser@10.0.0.1:2222\n"
            "  bastion.corp.net (uses same user as main connection)");
    }

    // Custom SSH Options (optional)
    ImGui::TextUnformatted("SSH Options:");
    ImGui::SameLine(150);
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##ssh_options", m_ssh_options_buffer, sizeof(m_ssh_options_buffer));
    ImGui::PopItemWidth();
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(
            "Optional: Additional SSH options\n"
            "Examples:\n"
            "  -o StrictHostKeyChecking=no\n"
            "  -o ConnectTimeout=30\n"
            "  -o ServerAliveInterval=60");
    }

    ImGui::Spacing();

    // Test Connection button on its own line
    if(ImGui::Button("Test Connection", ImVec2(150, 0)))
    {
        TestSSHConnection();
    }

    if(!m_ssh_test_result.empty())
    {
        ImGui::SameLine();
        ImGui::TextColored(
            m_ssh_test_result.find("Success") != std::string::npos
                ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f)
                : ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
            "%s", m_ssh_test_result.c_str()
        );

        // Show detailed diagnostics in a collapsible section
        if(!m_ssh_test_details.empty())
        {
            ImGui::Spacing();
            if(ImGui::TreeNode("Connection Details"))
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
                ImGui::TextWrapped("%s", m_ssh_test_details.c_str());
                ImGui::PopStyleColor();
                ImGui::TreePop();
            }
        }
    }

    ImGui::Spacing();

    // Remote application path
    ImGui::TextUnformatted("Application Path (on remote):");
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##remote_app", m_remote_app_buffer, sizeof(m_remote_app_buffer));
    ImGui::PopItemWidth();

    ImGui::TextUnformatted("Arguments:");
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##remote_args", m_remote_args_buffer, sizeof(m_remote_args_buffer));
    ImGui::PopItemWidth();

    ImGui::Spacing();

    // Profiling tool
    ImGui::TextUnformatted("Profiling Tool:");
    ImGui::SameLine(150);
    // Only rocprofv3 is supported for now (rocprof-compute and rocprof-sys disabled)
    const char* tools[] = {"rocprofv3"};
    int current_tool = 0;  // Always rocprofv3
    ImGui::Combo("##remote_tool", &current_tool, tools, IM_ARRAYSIZE(tools));
    m_config.tool = ProfilingTool::RocProfV3;

    ImGui::Spacing();

    // Tool arguments (e.g., --sys-trace, --hip-trace)
    ImGui::TextUnformatted("Tool Arguments:");
    ImGui::SameLine(150);
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##remote_tool_args", m_tool_args_buffer, sizeof(m_tool_args_buffer));
    ImGui::PopItemWidth();
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Examples: --sys-trace, --hip-trace, --kernel-trace");
    }

    ImGui::Spacing();

    ImGui::TextUnformatted("Output Path (on remote):");
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##remote_output", m_remote_output_buffer, sizeof(m_remote_output_buffer));
    ImGui::PopItemWidth();

    ImGui::Spacing();
    ImGui::Checkbox("Copy results back automatically", &m_config.copy_results_back);

    ImGui::Spacing();

    // AI Analysis enable/disable checkbox
    ImGui::Checkbox("Run AI Analysis", &m_config.run_ai_analysis);
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(
            "Enable AI-powered performance analysis with rocpd analyze.\n"
            "When enabled, generates insights and recommendations.\n"
            "Disable to skip analysis and only collect trace data.");
    }

    // Only show LLM options if AI analysis is enabled
    if(m_config.run_ai_analysis)
    {
        ImGui::Spacing();

        // AI Analysis LLM options
        ImGui::TextUnformatted("AI Analysis LLM:");
        ImGui::SameLine(150);
        const char* llm_providers[] = {"Local (No LLM)", "Anthropic Claude", "OpenAI GPT"};
        int current_llm = static_cast<int>(m_config.llm_provider);
        if(ImGui::Combo("##remote_llm_provider", &current_llm, llm_providers, IM_ARRAYSIZE(llm_providers)))
        {
            m_config.llm_provider = static_cast<LLMProvider>(current_llm);

            // Auto-populate API key from settings when provider changes
            const auto& user_settings = SettingsManager::GetInstance().GetUserSettings();
            if(m_config.llm_provider == LLMProvider::Anthropic && !user_settings.anthropic_api_key.empty())
            {
                std::strncpy(m_llm_api_key_buffer, user_settings.anthropic_api_key.c_str(), sizeof(m_llm_api_key_buffer) - 1);
            }
            else if(m_config.llm_provider == LLMProvider::OpenAI && !user_settings.openai_api_key.empty())
            {
                std::strncpy(m_llm_api_key_buffer, user_settings.openai_api_key.c_str(), sizeof(m_llm_api_key_buffer) - 1);
            }
            else if(m_config.llm_provider == LLMProvider::Local)
            {
                // Clear the buffer when switching to Local
                m_llm_api_key_buffer[0] = '\0';
            }
        }
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Select LLM provider for AI-powered analysis insights");
        }

        // Show API key field only for Anthropic or OpenAI
        if(m_config.llm_provider != LLMProvider::Local)
        {
            ImGui::TextUnformatted("API Key:");
            ImGui::SameLine(150);
            ImGui::PushItemWidth(-1);
            ImGui::InputText("##remote_llm_api_key", m_llm_api_key_buffer, sizeof(m_llm_api_key_buffer), ImGuiInputTextFlags_Password);
            ImGui::PopItemWidth();
            if(ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(m_config.llm_provider == LLMProvider::Anthropic
                    ? "Anthropic API key (get from https://console.anthropic.com/)"
                    : "OpenAI API key (get from https://platform.openai.com/api-keys)");
            }
        }
    }

    ImGui::Spacing();

    // Advanced Options (collapsing header)
    if(ImGui::CollapsingHeader("Advanced Options", ImGuiTreeNodeFlags_None))
    {
        ImGui::Spacing();

        // Custom Pre-Commands
        ImGui::TextUnformatted("Pre-Profiling Commands:");
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Custom shell commands to run before profiling.\n"
                "Examples:\n"
                "  module load rocm\n"
                "  export ROCM_PATH=/opt/rocm\n"
                "  source /etc/profile.d/modules.sh\n"
                "\nCommands are executed with: bash -l -c 'commands'");
        }
        ImGui::PushItemWidth(-1);
        ImGui::InputTextMultiline("##pre_commands", m_pre_commands_buffer, sizeof(m_pre_commands_buffer),
                                   ImVec2(-1, 80), ImGuiInputTextFlags_None);
        ImGui::PopItemWidth();

        ImGui::Spacing();

        // Custom Environment Variables
        ImGui::TextUnformatted("Custom Environment Variables:");
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Add custom environment variables for profiling session");
        }

        // Display existing env vars
        if(!m_config.environment_vars.empty())
        {
            ImGui::Indent();
            std::vector<std::string> keys_to_remove;
            for(const auto& [key, value] : m_config.environment_vars)
            {
                ImGui::PushID(key.c_str());
                ImGui::TextUnformatted(key.c_str());
                ImGui::SameLine();
                ImGui::Text("= %s", value.c_str());
                ImGui::SameLine();
                if(ImGui::SmallButton("Remove"))
                {
                    keys_to_remove.push_back(key);
                }
                ImGui::PopID();
            }
            ImGui::Unindent();

            // Remove marked keys
            for(const auto& key : keys_to_remove)
            {
                m_config.environment_vars.erase(key);
            }

            ImGui::Spacing();
        }

        // Add new env var
        ImGui::Indent();
        ImGui::TextUnformatted("Add Variable:");
        ImGui::PushItemWidth(200);
        ImGui::InputText("##env_key", m_env_var_key_buffer, sizeof(m_env_var_key_buffer));
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::TextUnformatted("=");
        ImGui::SameLine();
        ImGui::PushItemWidth(300);
        ImGui::InputText("##env_value", m_env_var_value_buffer, sizeof(m_env_var_value_buffer));
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if(ImGui::Button("Add##env_var"))
        {
            if(std::strlen(m_env_var_key_buffer) > 0)
            {
                m_config.environment_vars[m_env_var_key_buffer] = m_env_var_value_buffer;
                m_env_var_key_buffer[0] = '\0';
                m_env_var_value_buffer[0] = '\0';
            }
        }
        ImGui::Unindent();

        ImGui::Spacing();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Buttons
    float button_width = 100.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float total_width = button_width * 2 + spacing;
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - total_width - 20.0f);

    if(ImGui::Button("< Back", ImVec2(button_width, 0)))
    {
        m_current_screen = DialogScreen::ModeSelection;
    }

    ImGui::SameLine();

    bool can_run = std::strlen(m_ssh_host_buffer) > 0 &&
                   std::strlen(m_ssh_user_buffer) > 0 &&
                   std::strlen(m_remote_app_buffer) > 0;
    ImGui::BeginDisabled(!can_run);
    if(ImGui::Button("Run", ImVec2(button_width, 0)))
    {
        m_config.ssh_host = std::string(m_ssh_user_buffer) + "@" + std::string(m_ssh_host_buffer);
        m_config.ssh_port = m_ssh_port_input;
        m_config.ssh_key_path = m_ssh_key_path_buffer;
        m_config.ssh_proxy_jump = m_ssh_proxy_jump_buffer;
        m_config.ssh_options = m_ssh_options_buffer;
        m_config.remote_app_path = m_remote_app_buffer;
        m_config.remote_args = m_remote_args_buffer;
        m_config.remote_output_path = m_remote_output_buffer;
        m_config.llm_api_key = m_llm_api_key_buffer;
        m_config.pre_commands = m_pre_commands_buffer;
        ExecuteProfiling();
    }
    ImGui::EndDisabled();
}

void ProfilingDialog::RenderProgress()
{
    ImGui::TextUnformatted("Running Profiling...");
    ImGui::Separator();
    ImGui::Spacing();

    // Render progress stages visualization
    RenderProgressStages();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Progress output
    ImGui::TextUnformatted("Output:");
    ImGui::BeginChild("##progress_output", ImVec2(0, -40), true);
    ImGui::TextUnformatted(m_progress_output.c_str());

    // Auto-scroll to bottom
    if(ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();

    ImGui::Spacing();

    // Cancel button (only when running)
    if(m_execution_running || m_job_monitoring_active)
    {
        float button_width = 100.0f;
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - button_width - 20.0f);
        if(ImGui::Button("Cancel", ImVec2(button_width, 0)))
        {
            if(m_config.mode == ExecutionMode::SLURM && m_job_monitoring_active)
            {
                CancelSLURMJob();
                m_current_screen = DialogScreen::Complete;
            }
            else if(m_execution_context)
            {
                m_execution_context->Cancel();
                m_execution_running = false;
                m_error_message = "Execution cancelled by user";
                m_current_screen = DialogScreen::Complete;
            }
        }
    }

    // Show SLURM job status if monitoring
    if(m_config.mode == ExecutionMode::SLURM && m_job_monitoring_active && !m_job_status.empty())
    {
        ImGui::Spacing();
        ImGui::Text("Job Status: %s", m_job_status.c_str());
    }
}

void ProfilingDialog::RenderComplete()
{
    if(m_execution_success)
    {
        ImGui::TextUnformatted("Profiling Completed Successfully!");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Trace file: %s", m_result_trace_path.c_str());
        ImGui::Spacing();

        // Show the full execution log
        ImGui::TextUnformatted("Full Log:");
        ImGui::BeginChild("##success_log", ImVec2(0, -60), true);
        ImGui::TextUnformatted(m_progress_output.c_str());

        // Auto-scroll to bottom
        if(ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();

        ImGui::Spacing();

        float button_width = 120.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float total_width = button_width * 2 + spacing;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - total_width) * 0.5f);

        if(ImGui::Button("View Results", ImVec2(button_width, 0)))
        {
            if(m_completion_callback)
            {
                spdlog::info("View Results button clicked");
                spdlog::info("  Trace path: {}", m_result_trace_path);
                spdlog::info("  AI JSON path: {}", m_ai_analysis_json_path);
                m_completion_callback(m_result_trace_path, m_ai_analysis_json_path);
            }
            Hide();
        }

        ImGui::SameLine();

        if(ImGui::Button("Close", ImVec2(button_width, 0)))
        {
            Hide();
        }
    }
    else
    {
        ImGui::TextUnformatted("Profiling Failed");
        ImGui::Separator();
        ImGui::Spacing();

        // Show the full execution log
        ImGui::TextUnformatted("Full Log:");
        ImGui::BeginChild("##error_log", ImVec2(0, -60), true);
        ImGui::TextUnformatted(m_progress_output.c_str());

        // Add error message at the end if present
        if(!m_error_message.empty())
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "ERROR:");
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", m_error_message.c_str());
        }

        // Auto-scroll to bottom to show the error
        if(ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();

        ImGui::Spacing();

        float button_width = 100.0f;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - button_width) * 0.5f);

        if(ImGui::Button("Close", ImVec2(button_width, 0)))
        {
            Hide();
        }
    }
}

void ProfilingDialog::ExecuteProfiling()
{
    m_current_screen = DialogScreen::Progress;
    m_execution_running = true;
    m_progress_output.clear();
    m_execution_success = false;
    m_error_message.clear();
    m_current_stage = ProgressStage::Profiling;

    std::string command = GetToolCommand();

    // Wrap command for container if needed (only for local mode)
    if(m_config.mode == ExecutionMode::Local && m_config.use_container)
    {
        // Store exec options from buffer to config before wrapping
        m_config.container_exec_options = std::string(m_container_exec_options_buffer);
        command = WrapCommandForContainer(command);
    }

    // Display the command being executed
    m_progress_output = "=== Executing Command ===\n";
    m_progress_output += command + "\n\n";
    m_progress_output += "=== Output ===\n";

    Controller::ProcessExecutor::ExecutionOptions options;
    if(m_config.mode == ExecutionMode::Local)
    {
        options.working_directory = m_config.output_directory;
    }
    options.capture_output = true;
    options.timeout_seconds = 300;  // 5 minutes timeout

    // Real-time output streaming
    options.output_callback = [this](const std::string& line)
    {
        m_progress_output += line;

        // Limit output size
        if(m_progress_output.size() > 100000)
        {
            // Keep last 50k characters
            m_progress_output = m_progress_output.substr(m_progress_output.size() - 50000);
        }
    };

    // Execute command
    if(m_config.mode == ExecutionMode::Local)
    {
        m_execution_context = Controller::ProcessExecutor::ExecuteAsync(
            command,
            options,
            [this](const Controller::ProcessExecutor::ExecutionResult& result)
            {
                OnProfilingComplete(result);
            }
        );
    }
    else
    {
        m_execution_context = Controller::ProcessExecutor::ExecuteRemoteAsync(
            m_config.ssh_host,
            command,
            options,
            m_config.ssh_port,
            [this](const Controller::ProcessExecutor::ExecutionResult& result)
            {
                OnProfilingComplete(result);
            },
            m_config.ssh_key_path,
            m_config.ssh_proxy_jump,
            m_config.ssh_options
        );
    }
}

void ProfilingDialog::OnProfilingComplete(const Controller::ProcessExecutor::ExecutionResult& result)
{
    m_execution_running = false;
    m_progress_output += "\n\n=== Profiling Complete ===\n";
    m_progress_output += "Exit code: " + std::to_string(result.exit_code) + "\n";

    if(!result.stdout_output.empty())
    {
        m_progress_output += "Output:\n" + result.stdout_output + "\n";
    }

    if(!result.stderr_output.empty())
    {
        m_progress_output += "Errors:\n" + result.stderr_output + "\n";
    }

    if(result.exit_code == 0)
    {
        // After profiling, merge the traces into a single file
        m_current_stage = ProgressStage::Merging;
        MergeTraces();
        return; // MergeTraces will continue the workflow
    }
    else
    {
        m_execution_success = false;

        // Provide helpful error messages for common exit codes
        if(result.exit_code == 127)
        {
            std::string tool_name = GetToolName();
            m_error_message = "Command not found: " + tool_name + "\n\n"
                "The profiling tool '" + tool_name + "' is not installed or not in your PATH.\n\n"
                "Please install ROCm profiling tools:\n"
                "- On Linux: Install rocprofiler-sdk package\n"
                "- Ensure /opt/rocm/bin is in your PATH\n"
                "- Or specify the full path to the tool";
        }
        else
        {
            m_error_message = result.error_message.empty()
                ? "Profiling command failed with exit code " + std::to_string(result.exit_code)
                : result.error_message;
        }
        m_current_screen = DialogScreen::Complete;
    }
}

void ProfilingDialog::MergeTraces()
{
    m_progress_output += "\n\n=== Merging Traces ===\n";
    m_execution_running = true;

    // Determine output directory
    std::string output_dir = m_config.mode == ExecutionMode::Local
        ? std::string(m_output_dir_buffer)
        : std::string(m_remote_output_buffer);

    // First, list the directory contents to see what files were created
    // rocprofv3 creates subdirectories by hostname, so we need to list recursively
    std::string list_cmd;
    if(m_config.mode == ExecutionMode::Local)
    {
#ifdef _WIN32
        list_cmd = "dir \"" + output_dir + "\" /s /b";
#else
        list_cmd = "find \"" + output_dir + "\" -name '*.db' -type f";
#endif
    }
    else
    {
        list_cmd = "find \"" + output_dir + "\" -name '*.db' -type f";
    }

    m_progress_output += "Listing trace files in output directory:\n";
    m_progress_output += "Command: " + list_cmd + "\n";

    // Build rocpd merge command
    // This will merge all trace files in the directory into a single merged.db file
    // rocpd is a Python module, so use python3 -m rocpd
    // rocprofv3 creates files in <output_dir>/<hostname>/*.db, so we need to use the glob pattern
    std::string merge_cmd = list_cmd + " && python3 -m rocpd merge -i \"" + output_dir + "/*/*.db\" -o " + output_dir + "/merged.db";

    m_progress_output += "Merge command: python3 -m rocpd merge -i \"" + output_dir + "/*/*.db\" -o " + output_dir + "/merged.db\n";

    Controller::ProcessExecutor::ExecutionOptions options;
    options.capture_output = true;
    options.timeout_seconds = 120;  // 2 minutes for merging

    // Set PYTHONPATH for rocpd (local execution only)
    if(m_config.mode == ExecutionMode::Local)
    {
#ifdef _WIN32
        // Windows: Check common Python installation paths
        std::vector<std::string> python_paths = {
            "C:\\Program Files\\ROCm\\lib\\python3.12\\site-packages",
            "C:\\Program Files\\ROCm\\lib\\python3.11\\site-packages",
            "C:\\Program Files\\ROCm\\lib\\python3.10\\site-packages",
            "C:\\ROCm\\lib\\python3.10\\site-packages"
        };
        for(const auto& path : python_paths)
        {
            if(std::filesystem::exists(path))
            {
                options.environment["PYTHONPATH"] = path;
                break;
            }
        }
#else
        // Linux/macOS: Check /opt/rocm
        std::vector<std::string> python_versions = {"3.12", "3.11", "3.10", "3.9", "3.8"};
        for(const auto& ver : python_versions)
        {
            std::string path = "/opt/rocm/lib/python" + ver + "/site-packages";
            if(std::filesystem::exists(path))
            {
                options.environment["PYTHONPATH"] = path;
                break;
            }
        }
#endif
    }

    // Real-time output streaming
    options.output_callback = [this](const std::string& line)
    {
        m_progress_output += line;

        // Limit output size
        if(m_progress_output.size() > 100000)
        {
            m_progress_output = m_progress_output.substr(m_progress_output.size() - 50000);
        }
    };

    // Execute merge command
    if(m_config.mode == ExecutionMode::Local)
    {
        m_execution_context = Controller::ProcessExecutor::ExecuteAsync(
            merge_cmd,
            options,
            [this](const Controller::ProcessExecutor::ExecutionResult& result)
            {
                OnMergeComplete(result);
            }
        );
    }
    else
    {
        // Remote execution
        m_execution_context = Controller::ProcessExecutor::ExecuteRemoteAsync(
            m_config.ssh_host,
            merge_cmd,
            options,
            m_config.ssh_port,
            [this](const Controller::ProcessExecutor::ExecutionResult& result)
            {
                OnMergeComplete(result);
            },
            m_config.ssh_key_path,
            m_config.ssh_proxy_jump,
            m_config.ssh_options
        );
    }
}

void ProfilingDialog::OnMergeComplete(const Controller::ProcessExecutor::ExecutionResult& result)
{
    m_execution_running = false;
    m_progress_output += "\n\n=== Merge Complete ===\n";
    m_progress_output += result.stdout_output;

    if(result.exit_code == 0)
    {
        // Set the merged trace file path
        std::string output_dir = m_config.mode == ExecutionMode::Local
            ? std::string(m_output_dir_buffer)
            : std::string(m_remote_output_buffer);

        m_result_trace_path = output_dir + "/merged.db";
        m_execution_success = true;

        // For remote execution, save the connection to recent connections
        if(m_config.mode == ExecutionMode::Remote)
        {
            // Save this connection to recent connections
            RecentConnection conn;

            // Generate connection name (use user@host if no name provided)
            if(std::strlen(m_connection_name_buffer) > 0)
            {
                conn.name = m_connection_name_buffer;
            }
            else
            {
                conn.name = std::string(m_ssh_user_buffer) + "@" + std::string(m_ssh_host_buffer);
            }

            conn.ssh_host = m_ssh_host_buffer;
            conn.ssh_user = m_ssh_user_buffer;
            conn.ssh_port = m_ssh_port_input;
            conn.remote_app_path = m_remote_app_buffer;
            conn.remote_output_path = m_remote_output_buffer;

            SettingsManager::GetInstance().AddRecentConnection(conn);
        }

        // For remote execution, run AI analysis on the remote server first (if enabled)
        if(m_config.mode == ExecutionMode::Remote)
        {
            if(m_config.run_ai_analysis)
            {
                // Run AI analysis on remote, then copy both merged.db and analysis.json back
                m_current_stage = ProgressStage::AIAnalysis;
                RunAIAnalysis();
                return; // RunAIAnalysis will trigger CopyRemoteFileBack when done
            }
            else if(m_config.copy_results_back)
            {
                // Just copy the merged.db back without analysis
                m_current_stage = ProgressStage::CopyingFiles;
                CopyRemoteFileBack();
                return;
            }
            else
            {
                // Remote execution, no copy back, no analysis
                m_current_stage = ProgressStage::Complete;
                m_current_screen = DialogScreen::Complete;
            }
        }
        else
        {
            // Local execution: run AI analysis if enabled
            if(m_config.run_ai_analysis)
            {
                m_current_stage = ProgressStage::AIAnalysis;
                RunAIAnalysis();
            }
            else
            {
                m_current_stage = ProgressStage::Complete;
                m_current_screen = DialogScreen::Complete;
            }
        }
    }
    else
    {
        m_execution_success = false;
        m_error_message = "Failed to merge trace files.\n"
            "Exit code: " + std::to_string(result.exit_code) + "\n\n"
            "Make sure rocpd is installed and in your PATH.\n"
            "Output:\n" + result.stdout_output;
        m_current_screen = DialogScreen::Complete;
    }
}

void ProfilingDialog::FindAndCopyRemoteTraceFile()
{
    m_progress_output += "\n\n=== Finding Trace File on Remote Server ===\n";

    // List all .db files in the remote output directory to find the actual trace file
    std::string remote_dir = std::string(m_remote_output_buffer);

    std::ostringstream find_cmd;
    find_cmd << "ssh";
    if(m_config.ssh_port != 22)
    {
        find_cmd << " -p " << m_config.ssh_port;
    }
    find_cmd << " -o BatchMode=yes -o StrictHostKeyChecking=no";
    find_cmd << " " << m_config.ssh_host;
    find_cmd << " \"bash -l -c 'find " << remote_dir << " -name \\\"*.db\\\" -type f -printf \\\"%p\\\\n\\\" 2>/dev/null | head -1'\"";

    std::string command = find_cmd.str();
    m_progress_output += "Searching for trace files in: " + remote_dir + "\n";
    m_progress_output += "Command: " + command + "\n\n";

    Controller::ProcessExecutor::ExecutionOptions options;
    options.capture_output = true;
    options.timeout_seconds = 30;

    m_execution_running = true;

    m_execution_context = Controller::ProcessExecutor::ExecuteAsync(
        command,
        options,
        [this, remote_dir](const Controller::ProcessExecutor::ExecutionResult& result)
        {
            m_execution_running = false;

            if(result.exit_code == 0 && !result.stdout_output.empty())
            {
                // Extract the file path (remove newlines/whitespace)
                std::string trace_file = result.stdout_output;
                trace_file.erase(trace_file.find_last_not_of(" \n\r\t") + 1);

                if(!trace_file.empty() && trace_file.find(".db") != std::string::npos)
                {
                    m_progress_output += "Found trace file: " + trace_file + "\n";
                    m_result_trace_path = trace_file;
                    m_execution_success = true;

                    // Now copy it back
                    CopyRemoteFileBack();
                }
                else
                {
                    m_execution_success = false;
                    m_error_message = "No trace file (.db) found in remote directory: " + remote_dir + "\n\n"
                        "The profiling command may have failed to create a trace file.\n"
                        "Check the profiling output above for errors.\n\n"
                        "Try running the profiling command manually on the remote server:\n"
                        "ssh " + m_config.ssh_host + " 'ls -lh " + remote_dir + "/'";
                    m_current_screen = DialogScreen::Complete;
                }
            }
            else
            {
                m_execution_success = false;
                m_error_message = "Failed to search for trace files on remote server.\n"
                    "Exit code: " + std::to_string(result.exit_code) + "\n\n";

                if(!result.stderr_output.empty())
                {
                    m_error_message += "Error: " + result.stderr_output + "\n\n";
                }

                m_error_message += "Make sure:\n"
                    "- SSH key authentication is working\n"
                    "- The remote directory exists: " + remote_dir + "\n"
                    "- You have permissions to list files in that directory";
                m_current_screen = DialogScreen::Complete;
            }
        }
    );
}

void ProfilingDialog::CopyRemoteFileBack()
{
    m_progress_output += "\n\n=== Copying Results from Remote Server ===\n";
    m_progress_output += "Remote trace path: " + m_result_trace_path + "\n";

    // Build local destination path
    std::string local_dir = std::string(m_output_dir_buffer);
    if(local_dir.empty())
    {
#ifdef _WIN32
        local_dir = "C:\\temp\\rocprof_output";
#else
        local_dir = "/tmp/rocprof_output";
#endif
    }

    // Create local directory if it doesn't exist
    std::filesystem::create_directories(local_dir);

    // Extract filename from remote path
    std::filesystem::path remote_path(m_result_trace_path);
    std::string filename = remote_path.filename().string();
    std::string local_trace_path = local_dir + "/" + filename;

    m_progress_output += "Local destination: " + local_trace_path + "\n\n";

    // Build scp command to copy merged.db (required file)
    std::ostringstream scp_cmd;
    scp_cmd << "scp";

    // Add port if not default
    if(m_config.ssh_port != 22)
    {
        scp_cmd << " -P " << m_config.ssh_port;
    }

    scp_cmd << " -o BatchMode=yes";
    scp_cmd << " -o StrictHostKeyChecking=no";

    // Copy merged.db first
    std::string output_dir = std::string(m_remote_output_buffer);
    scp_cmd << " " << m_config.ssh_host << ":\"" << output_dir << "/merged.db\"";
    scp_cmd << " \"" << local_dir << "/\"";

    std::string command = scp_cmd.str();
    m_progress_output += "Copying merged.db...\n";

    Controller::ProcessExecutor::ExecutionOptions options;
    options.capture_output = true;
    options.timeout_seconds = 300;  // 5 minutes for file transfer

    options.output_callback = [this](const std::string& line)
    {
        m_progress_output += line;
    };

    m_execution_running = true;

    m_execution_context = Controller::ProcessExecutor::ExecuteAsync(
        command,
        options,
        [this, local_trace_path, local_dir, output_dir](const Controller::ProcessExecutor::ExecutionResult& result)
        {
            m_execution_running = false;

            if(result.exit_code == 0 && std::filesystem::exists(local_trace_path))
            {
                m_progress_output += "Trace file copied successfully: " + local_trace_path + "\n";

                // Update trace path to local path
                m_result_trace_path = local_trace_path;

                // Only try to copy AI analysis JSON if AI analysis was enabled
                if(m_config.run_ai_analysis)
                {
                    // Now try to copy the AI analysis JSON (optional, may not exist)
                    m_progress_output += "\nCopying AI analysis JSON (if available)...\n";

                    std::ostringstream scp_json_cmd;
                    scp_json_cmd << "scp";

                    if(m_config.ssh_port != 22)
                    {
                        scp_json_cmd << " -P " << m_config.ssh_port;
                    }

                    scp_json_cmd << " -o BatchMode=yes";
                    scp_json_cmd << " -o StrictHostKeyChecking=no";
                    scp_json_cmd << " " << m_config.ssh_host << ":\"" << output_dir << "/merged_ai_analysis.json\"";
                    scp_json_cmd << " \"" << local_dir << "/\"";

                    std::string json_command = scp_json_cmd.str();

                    Controller::ProcessExecutor::ExecutionOptions json_options;
                    json_options.capture_output = true;
                    json_options.timeout_seconds = 60;

                    // Execute JSON copy (non-blocking, optional)
                    Controller::ProcessExecutor::ExecuteAsync(
                        json_command,
                        json_options,
                        [this, local_dir](const Controller::ProcessExecutor::ExecutionResult& json_result)
                        {
                            std::string local_analysis_json = local_dir + "/merged_ai_analysis.json";
                            spdlog::info("JSON copy callback - exit code: {}", json_result.exit_code);
                            spdlog::info("  Expected local path: {}", local_analysis_json);
                            spdlog::info("  File exists: {}", std::filesystem::exists(local_analysis_json));

                            if(json_result.exit_code == 0 && std::filesystem::exists(local_analysis_json))
                            {
                                m_ai_analysis_json_path = local_analysis_json;
                                m_progress_output += "AI analysis JSON copied: " + local_analysis_json + "\n";
                                spdlog::info("  m_ai_analysis_json_path set to: {}", m_ai_analysis_json_path);
                            }
                            else
                            {
                                m_progress_output += "Note: AI analysis JSON not available (this is OK if analysis wasn't run)\n";
                                if(json_result.exit_code != 0)
                                {
                                    spdlog::warn("  JSON copy failed: {}", json_result.stderr_output);
                                }
                            }

                            m_current_stage = ProgressStage::Complete;
                            m_current_screen = DialogScreen::Complete;
                        }
                    );
                }
                else
                {
                    // AI analysis was disabled, skip JSON copy and complete immediately
                    m_progress_output += "AI analysis was disabled, skipping JSON file copy.\n";
                    m_current_stage = ProgressStage::Complete;
                    m_current_screen = DialogScreen::Complete;
                }
            }
            else
            {
                m_execution_success = false;
                m_error_message = "Failed to copy trace file from remote server.\n"
                    "Exit code: " + std::to_string(result.exit_code) + "\n\n";

                if(!result.stderr_output.empty())
                {
                    m_error_message += "Error output:\n" + result.stderr_output + "\n\n";
                }

                m_error_message += "Attempted to copy: " + m_result_trace_path + "\n\n"
                    "Common issues:\n"
                    "- The trace file might have a different name or location\n"
                    "- Check the profiling output above to see actual file paths\n"
                    "- SSH key authentication must be set up (no password prompts)\n"
                    "- You need read permissions for the remote files\n\n"
                    "Try unchecking 'Copy results back' and manually copy the files,\n"
                    "or check the remote server to find the actual trace file location.";
                m_current_screen = DialogScreen::Complete;
            }
        }
    );
}

void ProfilingDialog::RunAIAnalysis()
{
    m_progress_output += "\n\n=== Running AI Analysis ===\n";

    // For remote mode, run rocpd analyze on the remote server
    if(m_config.mode == ExecutionMode::Remote)
    {
        // Build rocpd analyze command
        std::string output_dir = std::string(m_remote_output_buffer);
        std::string merged_db = output_dir + "/merged.db";
        std::string analysis_json = output_dir + "/merged_ai_analysis.json";

        std::ostringstream analyze_cmd;
        analyze_cmd << "rocpd analyze -i " << merged_db;
        analyze_cmd << " --format json";
        analyze_cmd << " -o " << analysis_json;

        // Add LLM options
        switch(m_config.llm_provider)
        {
            case LLMProvider::Anthropic:
                analyze_cmd << " --llm anthropic";
                if(std::strlen(m_llm_api_key_buffer) > 0)
                {
                    analyze_cmd << " --llm-api-key \"" << m_llm_api_key_buffer << "\"";
                }
                break;
            case LLMProvider::OpenAI:
                analyze_cmd << " --llm openai";
                if(std::strlen(m_llm_api_key_buffer) > 0)
                {
                    analyze_cmd << " --llm-api-key \"" << m_llm_api_key_buffer << "\"";
                }
                break;
            case LLMProvider::Local:
            default:
                // No LLM options
                break;
        }

        std::string command = analyze_cmd.str();

        // Mask API key in log output for security
        std::string log_command = command;
        if(m_config.llm_provider != LLMProvider::Local && std::strlen(m_llm_api_key_buffer) > 0)
        {
            std::string api_key = m_llm_api_key_buffer;
            size_t pos = log_command.find(api_key);
            if(pos != std::string::npos)
            {
                log_command.replace(pos, api_key.length(), "***HIDDEN***");
            }
        }
        m_progress_output += "Running: " + log_command + "\n";

        Controller::ProcessExecutor::ExecutionOptions options;
        options.capture_output = true;
        options.timeout_seconds = 600;  // 10 minutes for AI analysis
        options.output_callback = [this](const std::string& line)
        {
            m_progress_output += line;
        };

        m_execution_running = true;

        m_execution_context = Controller::ProcessExecutor::ExecuteRemoteAsync(
            m_config.ssh_host,
            command,
            options,
            m_config.ssh_port,
            [this, analysis_json](const Controller::ProcessExecutor::ExecutionResult& result)
            {
                m_execution_running = false;

                if(result.exit_code == 0)
                {
                    m_progress_output += "\nAI analysis completed successfully\n";
                    // NOTE: Don't set m_ai_analysis_json_path here - it will be set by CopyRemoteFileBack
                    // after the file is copied to the local machine. The 'analysis_json' variable
                    // contains the REMOTE path, but we need the LOCAL path for loading.
                    spdlog::info("Remote AI analysis completed: {}", analysis_json);
                }
                else
                {
                    m_progress_output += "\nAI analysis failed (exit code: " + std::to_string(result.exit_code) + ")\n";
                    if(!result.stderr_output.empty())
                    {
                        m_progress_output += "Error: " + result.stderr_output + "\n";
                    }
                }

                // Copy files back if requested
                if(m_config.copy_results_back)
                {
                    m_current_stage = ProgressStage::CopyingFiles;
                    CopyRemoteFileBack();
                }
                else
                {
                    m_current_stage = ProgressStage::Complete;
                    m_current_screen = DialogScreen::Complete;
                }
            },
            m_config.ssh_key_path,
            m_config.ssh_proxy_jump,
            m_config.ssh_options
        );
    }
    else
    {
        // Local mode: use AIAnalysisPipeline
        if(!Controller::AIAnalysisPipeline::IsRocpdAvailable())
        {
            m_progress_output += "rocpd not found - skipping AI analysis\n";
            m_current_screen = DialogScreen::Complete;
            return;
        }

        Controller::AIAnalysisPipeline::AnalysisOptions options;
        options.trace_file_path = m_result_trace_path;
        options.progress_callback = [this](const std::string& line)
        {
            m_progress_output += line;
        };

        // Set LLM options
        switch(m_config.llm_provider)
        {
            case LLMProvider::Anthropic:
                options.llm_provider = "anthropic";
                options.llm_api_key = std::string(m_llm_api_key_buffer);
                break;
            case LLMProvider::OpenAI:
                options.llm_provider = "openai";
                options.llm_api_key = std::string(m_llm_api_key_buffer);
                break;
            case LLMProvider::Local:
            default:
                // No LLM, leave empty
                break;
        }

        m_execution_running = true;

        Controller::AIAnalysisPipeline::GenerateAnalysisAsync(
            options,
            [this](const Controller::AIAnalysisPipeline::AnalysisResult& ai_result)
            {
                m_execution_running = false;

                if(ai_result.success)
                {
                    m_progress_output += "\nAI analysis completed: " + ai_result.json_file_path + "\n";
                    m_ai_analysis_json_path = ai_result.json_file_path;
                }
                else
                {
                    m_progress_output += "\nAI analysis failed: " + ai_result.error_message + "\n";
                }

                m_current_stage = ProgressStage::Complete;
                m_current_screen = DialogScreen::Complete;
            }
        );
    }
}

void ProfilingDialog::TestSSHConnection()
{
    m_ssh_test_running = true;
    m_ssh_test_result = "Testing...";
    m_ssh_test_details.clear();

    std::string ssh_host = std::string(m_ssh_user_buffer) + "@" + std::string(m_ssh_host_buffer);

    // Enhanced test command that gathers system information
    std::string test_command =
        "echo '=== Connection Successful ===' && "
        "echo 'Hostname:' $(hostname) && "
        "echo 'User:' $(whoami) && "
        "echo 'OS:' $(uname -s) && "
        "echo 'ROCm Path:' $(which rocprofv3 2>/dev/null || echo 'NOT FOUND') && "
        "echo 'rocpd:' $(which rocpd 2>/dev/null || echo 'NOT FOUND')";

    Controller::ProcessExecutor::ExecutionOptions options;
    options.capture_output = true;
    options.timeout_seconds = 10;

    auto context = Controller::ProcessExecutor::ExecuteRemoteAsync(
        ssh_host,
        test_command,
        options,
        m_ssh_port_input,
        [this, ssh_host](const Controller::ProcessExecutor::ExecutionResult& result)
        {
            m_ssh_test_running = false;

            if(result.exit_code == 0)
            {
                m_ssh_test_result = "Success!";
                m_ssh_test_details = result.stdout_output;
                spdlog::info("SSH connection test succeeded: {}", ssh_host);
            }
            else
            {
                m_ssh_test_result = "Failed";

                // Provide detailed diagnostics based on error
                std::ostringstream details;
                details << "Connection to " << ssh_host << " failed.\n\n";

                // Analyze error type
                std::string error_lower = result.error_message;
                std::transform(error_lower.begin(), error_lower.end(), error_lower.begin(), ::tolower);

                if(error_lower.find("connection refused") != std::string::npos)
                {
                    details << "Error: Connection Refused\n";
                    details << "Possible causes:\n";
                    details << "  - SSH server not running on remote host\n";
                    details << "  - Firewall blocking port " << m_ssh_port_input << "\n";
                    details << "  - Wrong port number specified\n\n";
                    details << "Troubleshooting:\n";
                    details << "  1. Verify SSH server is running: sudo systemctl status sshd\n";
                    details << "  2. Check firewall: sudo firewall-cmd --list-all\n";
                    details << "  3. Verify port in /etc/ssh/sshd_config\n";
                }
                else if(error_lower.find("connection timed out") != std::string::npos)
                {
                    details << "Error: Connection Timed Out\n";
                    details << "Possible causes:\n";
                    details << "  - Host is unreachable (network issue)\n";
                    details << "  - Incorrect hostname or IP address\n";
                    details << "  - Firewall dropping packets\n\n";
                    details << "Troubleshooting:\n";
                    details << "  1. Verify hostname: ping " << m_ssh_host_buffer << "\n";
                    details << "  2. Check network connectivity\n";
                    details << "  3. Try from command line: ssh -p " << m_ssh_port_input << " " << ssh_host << "\n";
                }
                else if(error_lower.find("permission denied") != std::string::npos ||
                        error_lower.find("authentication") != std::string::npos)
                {
                    details << "Error: Authentication Failed\n";
                    details << "Possible causes:\n";
                    details << "  - SSH key not set up or incorrect\n";
                    details << "  - Wrong username\n";
                    details << "  - Key not in authorized_keys on remote\n\n";
                    details << "Troubleshooting:\n";
                    details << "  1. Verify SSH key: ls -la ~/.ssh/\n";
                    details << "  2. Copy key to remote: ssh-copy-id " << ssh_host << "\n";
                    details << "  3. Check permissions: chmod 600 ~/.ssh/id_rsa\n";
                    details << "  4. Specify custom key in 'SSH Key Path' field above\n";
                }
                else if(error_lower.find("could not resolve hostname") != std::string::npos)
                {
                    details << "Error: Hostname Resolution Failed\n";
                    details << "Possible causes:\n";
                    details << "  - Incorrect hostname\n";
                    details << "  - DNS server unreachable\n\n";
                    details << "Troubleshooting:\n";
                    details << "  1. Verify hostname spelling\n";
                    details << "  2. Try IP address instead of hostname\n";
                    details << "  3. Check DNS: nslookup " << m_ssh_host_buffer << "\n";
                }
                else
                {
                    details << "Error: " << result.error_message << "\n\n";
                    details << "General troubleshooting:\n";
                    details << "  1. Test from terminal: ssh -p " << m_ssh_port_input << " " << ssh_host << "\n";
                    details << "  2. Check SSH config: cat ~/.ssh/config\n";
                    details << "  3. Enable verbose output: ssh -vvv\n";
                }

                if(!result.stderr_output.empty())
                {
                    details << "\nSSH Output:\n" << result.stderr_output;
                }

                m_ssh_test_details = details.str();
                spdlog::error("SSH connection test failed: {}", result.error_message);
            }
        },
        std::string(m_ssh_key_path_buffer),
        std::string(m_ssh_proxy_jump_buffer),
        std::string(m_ssh_options_buffer)
    );
}

std::string ProfilingDialog::GetToolCommand() const
{
    std::ostringstream cmd;

    // For remote mode, add pre-commands and environment variables
    if(m_config.mode == ExecutionMode::Remote)
    {
        // Start with environment variables export
        for(const auto& [key, value] : m_config.environment_vars)
        {
            cmd << "export " << key << "=\"" << value << "\"; ";
        }

        // Add custom pre-commands if specified
        if(!m_config.pre_commands.empty())
        {
            cmd << m_config.pre_commands;
            // Ensure commands end with semicolon for proper chaining
            if(m_config.pre_commands.back() != ';')
            {
                cmd << "; ";
            }
            else
            {
                cmd << " ";
            }
        }
    }

    std::string tool_name = GetToolName();
    cmd << tool_name << " ";

    // Add output directory and filename flags (required for tools to create trace files)
    std::string output_dir = m_config.mode == ExecutionMode::Local
        ? std::string(m_output_dir_buffer)
        : std::string(m_remote_output_buffer);

    // Different tools use different flags for output directory and filename
    switch(m_config.tool)
    {
        case ProfilingTool::RocProfV3:
            // rocprofv3 uses: -d <dir> to specify output directory
            // We'll merge the traces later using rocpd merge
            cmd << "-d " << output_dir << " ";
            break;
        case ProfilingTool::RocProfCompute:
            cmd << "--output-directory " << output_dir << " ";
            break;
        case ProfilingTool::RocProfSys:
            cmd << "-o " << output_dir << "/trace ";  // -o specifies output prefix
            break;
    }

    // Add tool-specific arguments (e.g., --sys-trace, --hip-trace)
    if(!m_config.tool_args.empty())
    {
        cmd << m_config.tool_args << " ";
    }

    // Add the application to profile
    if(m_config.mode == ExecutionMode::Local)
    {
        cmd << "-- " << m_config.application_path;
        if(!m_config.application_args.empty())
        {
            cmd << " " << m_config.application_args;
        }
    }
    else
    {
        cmd << "-- " << m_config.remote_app_path;
        if(!m_config.remote_args.empty())
        {
            cmd << " " << m_config.remote_args;
        }
    }

    return cmd.str();
}

std::string ProfilingDialog::GetToolName() const
{
    switch(m_config.tool)
    {
        case ProfilingTool::RocProfV3:
            return "rocprofv3";
        case ProfilingTool::RocProfCompute:
            return "rocprof-compute";
        case ProfilingTool::RocProfSys:
            return "rocprof-sys";
        default:
            return "rocprofv3";
    }
}

std::string ProfilingDialog::FindTraceFile(const std::string& output) const
{
    // Look for patterns like "Trace file: /path/to/trace.db" or "Output: trace.db"
    std::regex trace_pattern(R"((Trace file|Output|Writing to):\s*([^\s\n]+\.(db|rpd)))");
    std::smatch match;

    if(std::regex_search(output, match, trace_pattern))
    {
        return match[2].str();
    }

    return "";
}

void ProfilingDialog::RenderProgressStages()
{
    // Define the stages
    struct Stage
    {
        const char* name;
        ProgressStage stage;
        const char* icon;
    };

    std::vector<Stage> stages;

    // Add stages based on mode and configuration
    stages.push_back({"Profiling", ProgressStage::Profiling, "1"});
    stages.push_back({"Merging", ProgressStage::Merging, "2"});

    // Only show AI Analysis stage if enabled for this profiling session
    if(m_config.run_ai_analysis)
    {
        stages.push_back({"AI Analysis", ProgressStage::AIAnalysis, "3"});
    }

    // Show copying stage for remote execution with copy back enabled
    if(m_config.mode == ExecutionMode::Remote && m_config.copy_results_back)
    {
        stages.push_back({"Copying Files", ProgressStage::CopyingFiles,
                         m_config.run_ai_analysis ? "4" : "3"});
    }

    // Calculate layout
    float window_width = ImGui::GetContentRegionAvail().x;
    float stage_width = (window_width - 20.0f * (stages.size() - 1)) / stages.size();

    ImGui::BeginGroup();

    // Draw stages
    for(size_t i = 0; i < stages.size(); ++i)
    {
        if(i > 0)
        {
            ImGui::SameLine(0, 20.0f);
        }

        ImGui::BeginGroup();

        // Determine stage state
        bool is_current = (m_current_stage == stages[i].stage);
        bool is_completed = (static_cast<int>(m_current_stage) > static_cast<int>(stages[i].stage)) &&
                            m_current_stage != ProgressStage::None;
        bool is_pending = !is_current && !is_completed;

        // Draw stage indicator (circle with number)
        ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        float circle_radius = 20.0f;
        ImVec2 circle_center = ImVec2(cursor_pos.x + stage_width * 0.5f, cursor_pos.y + circle_radius);

        // Draw connector line to next stage
        if(i < stages.size() - 1)
        {
            ImVec2 line_start = ImVec2(circle_center.x + circle_radius, circle_center.y);
            ImVec2 line_end = ImVec2(circle_center.x + stage_width * 0.5f + 20.0f - circle_radius, circle_center.y);
            ImU32 line_color = is_completed ? IM_COL32(0, 200, 0, 255) : IM_COL32(100, 100, 100, 255);
            draw_list->AddLine(line_start, line_end, line_color, 3.0f);
        }

        // Draw circle
        ImU32 circle_color;
        ImU32 text_color;

        if(is_current)
        {
            // Current stage: bright blue with animation
            float pulse = (float)(ImGui::GetTime() - (int)ImGui::GetTime());
            int intensity = 150 + (int)(pulse * 100);
            circle_color = IM_COL32(0, intensity, 255, 255);
            text_color = IM_COL32(255, 255, 255, 255);

            // Draw outer ring for current stage
            draw_list->AddCircle(circle_center, circle_radius + 3.0f, IM_COL32(0, 150, 255, 150), 32, 2.0f);
        }
        else if(is_completed)
        {
            // Completed: green
            circle_color = IM_COL32(0, 200, 0, 255);
            text_color = IM_COL32(255, 255, 255, 255);
        }
        else
        {
            // Pending: gray
            circle_color = IM_COL32(100, 100, 100, 255);
            text_color = IM_COL32(200, 200, 200, 255);
        }

        draw_list->AddCircleFilled(circle_center, circle_radius, circle_color);

        // Draw checkmark for completed stages or number for others
        if(is_completed)
        {
            // Draw checkmark
            ImVec2 check_start = ImVec2(circle_center.x - 8, circle_center.y);
            ImVec2 check_mid = ImVec2(circle_center.x - 2, circle_center.y + 6);
            ImVec2 check_end = ImVec2(circle_center.x + 8, circle_center.y - 6);
            draw_list->AddLine(check_start, check_mid, IM_COL32(255, 255, 255, 255), 3.0f);
            draw_list->AddLine(check_mid, check_end, IM_COL32(255, 255, 255, 255), 3.0f);
        }
        else
        {
            // Draw stage number
            ImVec2 text_size = ImGui::CalcTextSize(stages[i].icon);
            ImVec2 text_pos = ImVec2(circle_center.x - text_size.x * 0.5f, circle_center.y - text_size.y * 0.5f);
            draw_list->AddText(text_pos, text_color, stages[i].icon);
        }

        // Add invisible button for spacing
        ImGui::InvisibleButton(("##stage_" + std::to_string(i)).c_str(), ImVec2(stage_width, circle_radius * 2));

        // Draw stage name below circle
        const char* stage_name = stages[i].name;
        ImVec2 name_size = ImGui::CalcTextSize(stage_name);
        ImVec2 name_pos = ImVec2(stage_width * 0.5f - name_size.x * 0.5f, 0);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + name_pos.x);
        ImGui::TextColored(is_current ? ImVec4(0.3f, 0.7f, 1.0f, 1.0f) :
                          is_completed ? ImVec4(0.0f, 0.8f, 0.0f, 1.0f) :
                          ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                          "%s", stage_name);

        ImGui::EndGroup();
    }

    ImGui::EndGroup();
}

void ProfilingDialog::DetectContainers()
{
    if(m_container_detection_running)
    {
        return;
    }

    m_container_detection_running = true;
    m_detected_containers.clear();

    spdlog::info("Detecting containers...");

    // Detect Docker containers
    DetectDockerContainers();

    // Detect Podman containers
    DetectPodmanContainers();

    // Detect Singularity containers
    DetectSingularityContainers();

    m_containers_loaded = true;
    m_container_detection_running = false;

    spdlog::info("Container detection complete. Found {} containers", m_detected_containers.size());
}

void ProfilingDialog::DetectDockerContainers()
{
    // Check if docker is available
    auto docker_path = Controller::ProcessExecutor::FindExecutableInPath("docker");
    if(!docker_path)
    {
        spdlog::debug("Docker not found in PATH");
        return;
    }

    // List running containers
    std::string command = "docker ps --format \"{{.ID}}|{{.Names}}|{{.Image}}|{{.Status}}\"";

    Controller::ProcessExecutor::ExecutionOptions options;
    options.capture_output = true;
    options.timeout_seconds = 5;

    auto result = Controller::ProcessExecutor::ExecuteSync(command, options);

    if(result.exit_code != 0)
    {
        spdlog::warn("Failed to list Docker containers: {}", result.error_message);
        return;
    }

    // Parse output
    std::istringstream stream(result.stdout_output);
    std::string line;

    while(std::getline(stream, line))
    {
        if(line.empty())
            continue;

        // Split by |
        std::istringstream line_stream(line);
        std::string id, name, image, status;

        std::getline(line_stream, id, '|');
        std::getline(line_stream, name, '|');
        std::getline(line_stream, image, '|');
        std::getline(line_stream, status, '|');

        ContainerInfo info;
        info.id = id;
        info.name = name;
        info.image = image;
        info.status = status;
        info.runtime = ContainerRuntime::Docker;

        m_detected_containers.push_back(info);
        spdlog::debug("Found Docker container: {} ({})", name, id);
    }
}

void ProfilingDialog::DetectPodmanContainers()
{
    // Check if podman is available
    auto podman_path = Controller::ProcessExecutor::FindExecutableInPath("podman");
    if(!podman_path)
    {
        spdlog::debug("Podman not found in PATH");
        return;
    }

    // List running containers
    std::string command = "podman ps --format \"{{.ID}}|{{.Names}}|{{.Image}}|{{.Status}}\"";

    Controller::ProcessExecutor::ExecutionOptions options;
    options.capture_output = true;
    options.timeout_seconds = 5;

    auto result = Controller::ProcessExecutor::ExecuteSync(command, options);

    if(result.exit_code != 0)
    {
        spdlog::warn("Failed to list Podman containers: {}", result.error_message);
        return;
    }

    // Parse output
    std::istringstream stream(result.stdout_output);
    std::string line;

    while(std::getline(stream, line))
    {
        if(line.empty())
            continue;

        // Split by |
        std::istringstream line_stream(line);
        std::string id, name, image, status;

        std::getline(line_stream, id, '|');
        std::getline(line_stream, name, '|');
        std::getline(line_stream, image, '|');
        std::getline(line_stream, status, '|');

        ContainerInfo info;
        info.id = id;
        info.name = name;
        info.image = image;
        info.status = status;
        info.runtime = ContainerRuntime::Podman;

        m_detected_containers.push_back(info);
        spdlog::debug("Found Podman container: {} ({})", name, id);
    }
}

void ProfilingDialog::DetectSingularityContainers()
{
    // Singularity doesn't have running containers in the same way
    // Instead, we can check for .sif files in common locations
    // For now, we'll skip automatic detection of Singularity containers
    // Users can manually specify container paths if needed
    spdlog::debug("Singularity container auto-detection not implemented (use manual path entry)");
}

std::string ProfilingDialog::WrapCommandForContainer(const std::string& command) const
{
    if(!m_config.use_container)
    {
        return command;
    }

    std::ostringstream wrapped_cmd;

    switch(m_config.container_runtime)
    {
        case ContainerRuntime::Docker:
        {
            wrapped_cmd << "docker exec";

            // Add custom exec options if provided
            if(!m_config.container_exec_options.empty())
            {
                wrapped_cmd << " " << m_config.container_exec_options;
            }

            // Add container ID/name
            wrapped_cmd << " " << m_config.container_id;

            // Add command
            wrapped_cmd << " bash -c '" << command << "'";
            break;
        }

        case ContainerRuntime::Podman:
        {
            wrapped_cmd << "podman exec";

            // Add custom exec options if provided
            if(!m_config.container_exec_options.empty())
            {
                wrapped_cmd << " " << m_config.container_exec_options;
            }

            // Add container ID/name
            wrapped_cmd << " " << m_config.container_id;

            // Add command
            wrapped_cmd << " bash -c '" << command << "'";
            break;
        }

        case ContainerRuntime::Singularity:
        {
            // For Singularity, container_id is the .sif file path
            wrapped_cmd << "singularity exec";

            // Add custom exec options if provided
            if(!m_config.container_exec_options.empty())
            {
                wrapped_cmd << " " << m_config.container_exec_options;
            }

            // Add container image path
            wrapped_cmd << " " << m_config.container_id;

            // Add command
            wrapped_cmd << " " << command;
            break;
        }

        case ContainerRuntime::None:
        default:
            return command;
    }

    spdlog::debug("Wrapped command for container: {}", wrapped_cmd.str());
    return wrapped_cmd.str();
}

void ProfilingDialog::DetectJobScheduler()
{
    // Check for SLURM
    auto sbatch_path = Controller::ProcessExecutor::FindExecutableInPath("sbatch");
    if(sbatch_path)
    {
        m_config.scheduler = JobScheduler::SLURM;
        m_slurm_available = true;
        spdlog::info("SLURM detected: {}", *sbatch_path);
    }
    else
    {
        m_slurm_available = false;
        spdlog::debug("SLURM not detected (sbatch not found in PATH)");
    }

    // TODO: Add detection for PBS, LSF if needed in future
}

void ProfilingDialog::RenderSLURMConfig()
{
    ImGui::TextUnformatted("SLURM Job Configuration");
    ImGui::Separator();
    ImGui::Spacing();

    // Application path
    ImGui::TextUnformatted("Application:");
    ImGui::SameLine(150);
    ImGui::PushItemWidth(-80);
    ImGui::InputText("##slurm_app_path", m_app_path_buffer, sizeof(m_app_path_buffer));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if(ImGui::Button("Browse...##app"))
    {
        AppWindow::GetInstance()->ShowOpenFileDialog(
            "Select Application",
            {{"All Files", {"*"}}},
            "",
            [this](const std::string& path)
            {
                if(!path.empty())
                {
                    std::strncpy(m_app_path_buffer, path.c_str(), sizeof(m_app_path_buffer) - 1);
                }
            }
        );
    }

    // Arguments
    ImGui::TextUnformatted("Arguments:");
    ImGui::SameLine(150);
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##slurm_app_args", m_app_args_buffer, sizeof(m_app_args_buffer));
    ImGui::PopItemWidth();

    ImGui::Spacing();

    // Tool arguments
    ImGui::TextUnformatted("Tool Arguments:");
    ImGui::SameLine(150);
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##slurm_tool_args", m_tool_args_buffer, sizeof(m_tool_args_buffer));
    ImGui::PopItemWidth();

    ImGui::Spacing();

    // Output directory
    ImGui::TextUnformatted("Output Directory:");
    ImGui::SameLine(150);
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##slurm_output_dir", m_output_dir_buffer, sizeof(m_output_dir_buffer));
    ImGui::PopItemWidth();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // SLURM Resource Requirements
    ImGui::TextUnformatted("SLURM Resource Requirements:");
    ImGui::Spacing();

    ImGui::TextUnformatted("Job Name:");
    ImGui::SameLine(150);
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##slurm_job_name", m_slurm_job_name_buffer, sizeof(m_slurm_job_name_buffer));
    ImGui::PopItemWidth();

    ImGui::TextUnformatted("Partition:");
    ImGui::SameLine(150);
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##slurm_partition", m_slurm_partition_buffer, sizeof(m_slurm_partition_buffer));
    ImGui::PopItemWidth();
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("SLURM partition/queue (e.g., gpu, compute)");
    }

    ImGui::TextUnformatted("Account:");
    ImGui::SameLine(150);
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##slurm_account", m_slurm_account_buffer, sizeof(m_slurm_account_buffer));
    ImGui::PopItemWidth();
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Account for billing (optional)");
    }

    ImGui::TextUnformatted("Nodes:");
    ImGui::SameLine(150);
    ImGui::PushItemWidth(100);
    ImGui::InputInt("##slurm_nodes", &m_slurm_nodes_input, 1, 10);
    ImGui::PopItemWidth();

    ImGui::TextUnformatted("Tasks:");
    ImGui::SameLine(150);
    ImGui::PushItemWidth(100);
    ImGui::InputInt("##slurm_ntasks", &m_slurm_ntasks_input, 1, 10);
    ImGui::PopItemWidth();

    ImGui::TextUnformatted("CPUs per Task:");
    ImGui::SameLine(150);
    ImGui::PushItemWidth(100);
    ImGui::InputInt("##slurm_cpus", &m_slurm_cpus_per_task_input, 1, 10);
    ImGui::PopItemWidth();

    ImGui::TextUnformatted("GPUs:");
    ImGui::SameLine(150);
    ImGui::PushItemWidth(100);
    ImGui::InputInt("##slurm_gpus", &m_slurm_gpus_input, 1, 8);
    ImGui::PopItemWidth();

    ImGui::TextUnformatted("Time Limit:");
    ImGui::SameLine(150);
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##slurm_time", m_slurm_time_buffer, sizeof(m_slurm_time_buffer));
    ImGui::PopItemWidth();
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Format: HH:MM:SS (e.g., 01:00:00 for 1 hour)");
    }

    ImGui::Spacing();

    ImGui::TextUnformatted("Extra Arguments:");
    ImGui::SameLine(150);
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##slurm_extra", m_slurm_extra_args_buffer, sizeof(m_slurm_extra_args_buffer));
    ImGui::PopItemWidth();
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Additional sbatch options (e.g., --constraint=gpu_mem:32gb)");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // AI Analysis option
    ImGui::Checkbox("Run AI Analysis", &m_config.run_ai_analysis);

    ImGui::Spacing();
    ImGui::Spacing();

    // Navigation buttons
    float button_width = 100.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float total_width = button_width * 3 + spacing * 2;
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - total_width - 20.0f);

    if(ImGui::Button("< Back", ImVec2(button_width, 0)))
    {
        m_current_screen = DialogScreen::ModeSelection;
    }

    ImGui::SameLine();

    if(ImGui::Button("Cancel", ImVec2(button_width, 0)))
    {
        Hide();
    }

    ImGui::SameLine();

    if(ImGui::Button("Submit Job", ImVec2(button_width, 0)))
    {
        // Capture config from buffers
        m_config.application_path = std::string(m_app_path_buffer);
        m_config.application_args = std::string(m_app_args_buffer);
        m_config.tool_args = std::string(m_tool_args_buffer);
        m_config.output_directory = std::string(m_output_dir_buffer);
        m_config.slurm_partition = std::string(m_slurm_partition_buffer);
        m_config.slurm_account = std::string(m_slurm_account_buffer);
        m_config.slurm_nodes = m_slurm_nodes_input;
        m_config.slurm_ntasks = m_slurm_ntasks_input;
        m_config.slurm_cpus_per_task = m_slurm_cpus_per_task_input;
        m_config.slurm_gpus = m_slurm_gpus_input;
        m_config.slurm_time = std::string(m_slurm_time_buffer);
        m_config.slurm_job_name = std::string(m_slurm_job_name_buffer);
        m_config.slurm_extra_args = std::string(m_slurm_extra_args_buffer);

        SubmitSLURMJob();
    }
}

std::string ProfilingDialog::BuildSLURMBatchScript() const
{
    std::ostringstream script;

    script << "#!/bin/bash\n";
    script << "#SBATCH --job-name=" << m_config.slurm_job_name << "\n";

    if(!m_config.slurm_partition.empty())
    {
        script << "#SBATCH --partition=" << m_config.slurm_partition << "\n";
    }

    if(!m_config.slurm_account.empty())
    {
        script << "#SBATCH --account=" << m_config.slurm_account << "\n";
    }

    script << "#SBATCH --nodes=" << m_config.slurm_nodes << "\n";
    script << "#SBATCH --ntasks=" << m_config.slurm_ntasks << "\n";
    script << "#SBATCH --cpus-per-task=" << m_config.slurm_cpus_per_task << "\n";
    script << "#SBATCH --gpus=" << m_config.slurm_gpus << "\n";
    script << "#SBATCH --time=" << m_config.slurm_time << "\n";

    // Output and error files
    std::string job_name = m_config.slurm_job_name.empty() ? "rocprof_job" : m_config.slurm_job_name;
    script << "#SBATCH --output=" << job_name << "_%j.out\n";
    script << "#SBATCH --error=" << job_name << "_%j.err\n";

    // Extra arguments
    if(!m_config.slurm_extra_args.empty())
    {
        script << m_config.slurm_extra_args << "\n";
    }

    script << "\n";

    // Load ROCm module if needed
    script << "# Load ROCm environment\n";
    script << "module load rocm 2>/dev/null || true\n";
    script << "export PATH=/opt/rocm/bin:/opt/rocm/libexec/rocprofiler:$PATH\n";
    script << "\n";

    // Change to output directory
    if(!m_config.output_directory.empty())
    {
        script << "cd " << m_config.output_directory << "\n";
        script << "\n";
    }

    // Run profiling command
    std::string tool_command = GetToolCommand();
    script << "# Run profiling\n";
    script << "echo \"Starting profiling: " << tool_command << "\"\n";
    script << tool_command << "\n";
    script << "\n";

    // Merge traces
    script << "# Merge traces\n";
    script << "rocpd merge -d " << m_config.output_directory << " -o merged_trace.db\n";
    script << "\n";

    // Run AI analysis if enabled
    if(m_config.run_ai_analysis)
    {
        script << "# Run AI analysis\n";
        script << "rocpd analyze -i merged_trace.db --format json -o ai_analysis.json\n";
        script << "\n";
    }

    script << "echo \"Profiling complete!\"\n";
    script << "echo \"Output directory: " << m_config.output_directory << "\"\n";

    return script.str();
}

void ProfilingDialog::SubmitSLURMJob()
{
    m_current_screen = DialogScreen::Progress;
    m_execution_running = true;
    m_progress_output.clear();
    m_current_stage = ProgressStage::Profiling;

    // Build batch script
    std::string batch_script = BuildSLURMBatchScript();

    // Write script to temporary file
    std::string script_path = m_config.output_directory + "/rocprof_job.sbatch";

    std::ofstream script_file(script_path);
    if(!script_file)
    {
        m_execution_success = false;
        m_error_message = "Failed to create SLURM batch script at: " + script_path;
        m_current_screen = DialogScreen::Complete;
        spdlog::error("{}", m_error_message);
        return;
    }

    script_file << batch_script;
    script_file.close();

    spdlog::info("Created SLURM batch script: {}", script_path);

    // Display script
    m_progress_output = "=== SLURM Batch Script ===\n";
    m_progress_output += batch_script + "\n\n";
    m_progress_output += "=== Submitting Job ===\n";

    // Submit job with sbatch
    std::string submit_command = "sbatch " + script_path;

    Controller::ProcessExecutor::ExecutionOptions options;
    options.capture_output = true;
    options.timeout_seconds = 30;

    m_execution_context = Controller::ProcessExecutor::ExecuteAsync(
        submit_command,
        options,
        [this, script_path](const Controller::ProcessExecutor::ExecutionResult& result)
        {
            m_execution_running = false;

            if(result.exit_code == 0)
            {
                // Parse job ID from output (format: "Submitted batch job 12345")
                std::regex job_id_regex(R"(Submitted batch job (\d+))");
                std::smatch match;
                std::string output = result.stdout_output;

                if(std::regex_search(output, match, job_id_regex) && match.size() > 1)
                {
                    m_config.slurm_job_id = match[1].str();
                    m_progress_output += "Job submitted successfully!\n";
                    m_progress_output += "Job ID: " + m_config.slurm_job_id + "\n\n";
                    m_job_status = "Submitted (ID: " + m_config.slurm_job_id + ")";

                    spdlog::info("SLURM job submitted: {}", m_config.slurm_job_id);

                    // Start monitoring job
                    m_job_monitoring_active = true;
                    MonitorSLURMJob();
                }
                else
                {
                    m_execution_success = false;
                    m_error_message = "Job submitted but failed to parse job ID from output:\n" + output;
                    m_current_screen = DialogScreen::Complete;
                    spdlog::warn("{}", m_error_message);
                }
            }
            else
            {
                m_execution_success = false;
                m_error_message = "Failed to submit SLURM job:\n" + result.error_message +
                                  "\n\nOutput:\n" + result.stdout_output;
                m_current_screen = DialogScreen::Complete;
                spdlog::error("SLURM job submission failed: {}", result.error_message);
            }
        }
    );
}

void ProfilingDialog::MonitorSLURMJob()
{
    if(!m_job_monitoring_active || m_config.slurm_job_id.empty())
    {
        return;
    }

    // Query job status with squeue
    std::string status_command = "squeue -j " + m_config.slurm_job_id + " --format=\"%T\" --noheader";

    Controller::ProcessExecutor::ExecutionOptions options;
    options.capture_output = true;
    options.timeout_seconds = 10;

    Controller::ProcessExecutor::ExecuteAsync(
        status_command,
        options,
        [this](const Controller::ProcessExecutor::ExecutionResult& result)
        {
            if(result.exit_code == 0 && !result.stdout_output.empty())
            {
                std::string status = result.stdout_output;
                // Remove whitespace
                status.erase(std::remove_if(status.begin(), status.end(), ::isspace), status.end());

                m_job_status = status + " (ID: " + m_config.slurm_job_id + ")";
                m_progress_output += "Job status: " + status + "\n";

                spdlog::debug("SLURM job {} status: {}", m_config.slurm_job_id, status);

                // Check if job is still running
                if(status == "PENDING" || status == "RUNNING" || status == "CONFIGURING")
                {
                    // Continue monitoring (check again in 5 seconds)
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    MonitorSLURMJob();
                }
                else if(status == "COMPLETED")
                {
                    m_job_monitoring_active = false;
                    m_progress_output += "\n=== Job Completed Successfully ===\n";
                    m_execution_success = true;
                    RetrieveSLURMResults();
                }
                else
                {
                    // Job failed or was cancelled
                    m_job_monitoring_active = false;
                    m_execution_success = false;
                    m_error_message = "SLURM job ended with status: " + status;
                    m_current_screen = DialogScreen::Complete;
                    spdlog::error("{}", m_error_message);
                }
            }
            else
            {
                // Job no longer in queue (might have completed)
                m_job_monitoring_active = false;
                m_progress_output += "\nJob no longer in queue. Checking for results...\n";
                RetrieveSLURMResults();
            }
        }
    );
}

void ProfilingDialog::CancelSLURMJob()
{
    if(m_config.slurm_job_id.empty())
    {
        return;
    }

    std::string cancel_command = "scancel " + m_config.slurm_job_id;

    Controller::ProcessExecutor::ExecutionOptions options;
    options.capture_output = true;

    Controller::ProcessExecutor::ExecuteAsync(
        cancel_command,
        options,
        [this](const Controller::ProcessExecutor::ExecutionResult& result)
        {
            if(result.exit_code == 0)
            {
                m_job_monitoring_active = false;
                m_progress_output += "\nJob cancelled successfully.\n";
                spdlog::info("SLURM job {} cancelled", m_config.slurm_job_id);
            }
            else
            {
                m_progress_output += "\nFailed to cancel job: " + result.error_message + "\n";
                spdlog::warn("Failed to cancel SLURM job {}: {}", m_config.slurm_job_id, result.error_message);
            }
        }
    );
}

void ProfilingDialog::RetrieveSLURMResults()
{
    m_progress_output += "Retrieving results...\n";

    // Check for merged trace file
    std::string merged_trace = m_config.output_directory + "/merged_trace.db";

    if(std::filesystem::exists(merged_trace))
    {
        m_result_trace_path = merged_trace;
        m_progress_output += "Found trace file: " + merged_trace + "\n";

        // Check for AI analysis JSON
        if(m_config.run_ai_analysis)
        {
            std::string ai_json = m_config.output_directory + "/ai_analysis.json";
            if(std::filesystem::exists(ai_json))
            {
                m_ai_analysis_json_path = ai_json;
                m_progress_output += "Found AI analysis: " + ai_json + "\n";
            }
        }

        m_execution_success = true;
        m_current_screen = DialogScreen::Complete;
        m_current_stage = ProgressStage::Complete;

        spdlog::info("SLURM job results retrieved successfully");
    }
    else
    {
        m_execution_success = false;
        m_error_message = "SLURM job completed but trace file not found at: " + merged_trace;
        m_current_screen = DialogScreen::Complete;
        spdlog::error("{}", m_error_message);
    }
}

}  // namespace View
}  // namespace RocProfVis
