// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_launch_shared_tabs.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_gui_helpers.h"
#include "rocprofvis_controller_enums.h"
#include "imgui.h"
#include <algorithm>
#include <sstream>
#include <cstring>

namespace RocProfVis
{
namespace View
{
namespace
{

static char s_target_exe_buf[512] = {};
static char s_target_args_buf[512] = {};
static char s_working_dir_buf[512] = {};
static char s_output_dir_buf[512] = {};

static char s_new_env_name[128] = {};
static char s_new_env_value[256] = {};

static char s_save_preset_name[128] = {};

void SyncBufFromString(char* buf, size_t buf_size, std::string const& str)
{
    std::snprintf(buf, buf_size, "%s", str.c_str());
}

} // namespace

bool RenderTargetSection(TargetSpec& target, AppWindow* app_window)
{
    bool modified = false;

    // Sync on first use or when target changes (by checking if buf is empty or different)
    if (s_target_exe_buf[0] == '\0' && !target.executable.empty())
    {
        SyncBufFromString(s_target_exe_buf, sizeof(s_target_exe_buf), target.executable);
        SyncBufFromString(s_target_args_buf, sizeof(s_target_args_buf), target.arguments);
        SyncBufFromString(s_working_dir_buf, sizeof(s_working_dir_buf), target.working_directory);
        SyncBufFromString(s_output_dir_buf, sizeof(s_output_dir_buf), target.output_directory);
    }

    ImGui::Text("Target Executable:");
    if (ImGui::InputText("##TargetExe", s_target_exe_buf, sizeof(s_target_exe_buf)))
    {
        target.executable = s_target_exe_buf;
        modified = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse##TargetExe") && app_window)
    {
        app_window->ShowOpenFileDialog(
            "Choose Target Executable", {}, "",
            [&target](std::string const& path)
            {
                target.executable = path;
                SyncBufFromString(s_target_exe_buf, sizeof(s_target_exe_buf), path);
            });
    }

    ImGui::Text("Target Arguments:");
    if (ImGui::InputText("##TargetArgs", s_target_args_buf, sizeof(s_target_args_buf)))
    {
        target.arguments = s_target_args_buf;
        modified = true;
    }

    ImGui::Text("Working Directory:");
    if (ImGui::InputText("##WorkDir", s_working_dir_buf, sizeof(s_working_dir_buf)))
    {
        target.working_directory = s_working_dir_buf;
        modified = true;
    }

    ImGui::Text("Output Directory:");
    if (ImGui::InputText("##OutputDir", s_output_dir_buf, sizeof(s_output_dir_buf)))
    {
        target.output_directory = s_output_dir_buf;
        modified = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse##OutputDir") && app_window)
    {
        app_window->ShowPathPickerDialog(
            "Choose Output Directory", "",
            [&target](std::string const& path)
            {
                target.output_directory = path;
                SyncBufFromString(s_output_dir_buf, sizeof(s_output_dir_buf), path);
            });
    }

    ImGui::Checkbox("Open trace when profiling completes", &target.auto_load_trace);

    return modified;
}

void RenderConnectionSection(ConnectionSpec& connection)
{
    ImGui::Text("Connection:");
    ImGui::SameLine();

    const char* conn_types[] = {"Local", "SSH (future)"};
    int conn_idx = (connection.type == ConnectionType::kLocal) ? 0 : 1;

    ImGui::PushItemWidth(150);
    if (ImGui::Combo("##Connection", &conn_idx, conn_types, 2))
    {
        connection.type = (conn_idx == 0) ? ConnectionType::kLocal : ConnectionType::kSsh;
    }
    ImGui::PopItemWidth();

    if (connection.type == ConnectionType::kSsh)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "SSH not yet supported");
        ImGui::BeginDisabled();
        char host_buf[128] = {};
        char user_buf[64] = {};
        ImGui::InputText("Host##SSH", host_buf, sizeof(host_buf));
        ImGui::InputText("User##SSH", user_buf, sizeof(user_buf));
        ImGui::EndDisabled();
    }
}

void RenderRawEnvVarsTab(std::map<std::string, std::string>& extra_env,
                         std::vector<std::pair<std::string, std::string>> const& curated_env)
{
    ImGui::Text("Additional environment variables passed to the profiler process.");
    ImGui::Separator();

    // Table of existing entries
    std::string key_to_remove;

    if (ImGui::BeginTable("EnvVars", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 200.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##Actions", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableHeadersRow();

        for (auto& kv : extra_env)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            // Check if this shadows a curated env var
            bool shadows_curated = false;
            for (auto const& ce : curated_env)
            {
                if (ce.first == kv.first)
                {
                    shadows_curated = true;
                    break;
                }
            }

            if (shadows_curated)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "%s", kv.first.c_str());
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Overrides a curated setting above");
                }
            }
            else
            {
                ImGui::TextUnformatted(kv.first.c_str());
            }

            ImGui::TableSetColumnIndex(1);
            char val_buf[512];
            std::snprintf(val_buf, sizeof(val_buf), "%s", kv.second.c_str());
            std::string input_id = "##env_val_" + kv.first;
            ImGui::PushItemWidth(-1);
            if (ImGui::InputText(input_id.c_str(), val_buf, sizeof(val_buf)))
            {
                kv.second = val_buf;
            }
            ImGui::PopItemWidth();

            ImGui::TableSetColumnIndex(2);
            std::string rm_id = "X##rm_" + kv.first;
            if (ImGui::SmallButton(rm_id.c_str()))
            {
                key_to_remove = kv.first;
            }
        }

        ImGui::EndTable();
    }

    if (!key_to_remove.empty())
    {
        extra_env.erase(key_to_remove);
    }

    // Add new entry
    ImGui::Separator();
    ImGui::Text("Add:");
    ImGui::SameLine();
    ImGui::PushItemWidth(180);
    ImGui::InputText("##NewEnvName", s_new_env_name, sizeof(s_new_env_name));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Text("=");
    ImGui::SameLine();
    ImGui::PushItemWidth(200);
    ImGui::InputText("##NewEnvValue", s_new_env_value, sizeof(s_new_env_value));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("+##AddEnv"))
    {
        if (std::strlen(s_new_env_name) > 0)
        {
            extra_env[s_new_env_name] = s_new_env_value;
            s_new_env_name[0] = '\0';
            s_new_env_value[0] = '\0';
        }
    }
}

void RenderCommandPreview(
    IProfilerBackend const* backend,
    LaunchConfig const& config,
    std::string const& profiler_path)
{
    ImGui::Text("Command Preview:");
    ImGui::Separator();

    std::vector<std::pair<std::string, std::string>> env_vars;
    std::vector<std::string> argv;

    if (backend)
    {
        backend->FlattenToExecution(config, env_vars, argv);
    }

    // Merge extra_env
    for (auto const& kv : config.extra_env)
    {
        env_vars.emplace_back(kv.first, kv.second);
    }

    // Build display string
    std::ostringstream preview;

    for (auto const& kv : env_vars)
    {
        if (!kv.second.empty())
        {
            preview << kv.first << "=" << kv.second << " \\\n";
        }
    }

    preview << profiler_path;
    for (auto const& arg : argv)
    {
        preview << " " << arg;
    }

    // Add extra_argv
    for (auto const& arg : config.extra_argv)
    {
        preview << " " << arg;
    }

    if (!config.target.output_directory.empty())
    {
        preview << " --output " << config.target.output_directory;
    }
    if (!config.target.executable.empty())
    {
        preview << " -- " << config.target.executable;
    }
    if (!config.target.arguments.empty())
    {
        preview << " " << config.target.arguments;
    }

    std::string preview_str = preview.str();

    ImGuiWindowFlags flags = ImGuiWindowFlags_HorizontalScrollbar;
    ImGui::BeginChild("CmdPreview", ImVec2(0, 80), true, flags);
    ImGui::TextUnformatted(preview_str.c_str());
    ImGui::EndChild();

    if (ImGui::Button("Copy Command##CmdPreview"))
    {
        ImGui::SetClipboardText(preview_str.c_str());
    }
}

void RenderOutputConsole(
    std::string const& output_text,
    std::string const& error_message,
    int profiler_state,
    bool& auto_scroll)
{
    ImGui::Text("Output:");

    const char* state_text = "Idle";
    ImVec4 state_color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);

    switch (profiler_state)
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
    ImGui::Checkbox("Auto-scroll##Output", &auto_scroll);

    ImGui::SameLine();
    if (ImGui::Button("Copy##OutputCopy"))
    {
        std::string clip;
        if (!error_message.empty())
        {
            clip = error_message + "\n\n";
        }
        clip += output_text;
        ImGui::SetClipboardText(clip.c_str());
    }

    ImGuiWindowFlags output_flags = ImGuiWindowFlags_HorizontalScrollbar;
    float output_height = std::max(ImGui::GetContentRegionAvail().y - 30.0f, 60.0f);
    ImGui::BeginChild("OutputText", ImVec2(0, output_height), true, output_flags);

    if (!error_message.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", error_message.c_str());
        ImGui::Separator();
    }

    ImGui::TextUnformatted(output_text.c_str());

    if (auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
}

std::string RenderPresetBar(
    LaunchPresetManager& preset_mgr,
    std::string const& profiler_id,
    std::string& current_preset_name,
    LaunchConfig& config,
    IProfilerBackend const* backend,
    AppWindow* /*app_window*/)
{
    std::string load_name;

    std::vector<PresetInfo> presets = preset_mgr.ListPresets(profiler_id);

    ImGui::Text("Preset:");
    ImGui::SameLine();

    // Combo for selecting preset
    ImGui::PushItemWidth(150);
    if (ImGui::BeginCombo("##PresetCombo", current_preset_name.empty() ? "(none)" : current_preset_name.c_str()))
    {
        if (ImGui::Selectable("(none)", current_preset_name.empty()))
        {
            current_preset_name.clear();
        }
        for (auto const& p : presets)
        {
            bool selected = (p.name == current_preset_name);
            if (ImGui::Selectable(p.name.c_str(), selected))
            {
                load_name = p.name;
                current_preset_name = p.name;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    ImGui::SameLine();
    if (ImGui::Button("Save##Preset"))
    {
        if (!current_preset_name.empty())
        {
            preset_mgr.SavePreset(current_preset_name, config, backend);
        }
        else
        {
            ImGui::OpenPopup("SavePresetPopup");
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Save As...##Preset"))
    {
        ImGui::OpenPopup("SavePresetPopup");
    }

    ImGui::SameLine();
    if (ImGui::Button("Delete##Preset"))
    {
        if (!current_preset_name.empty())
        {
            preset_mgr.DeletePreset(current_preset_name, profiler_id);
            current_preset_name.clear();
        }
    }

    // Save-As popup
    if (ImGui::BeginPopup("SavePresetPopup"))
    {
        ImGui::Text("Preset name:");
        ImGui::InputText("##SaveName", s_save_preset_name, sizeof(s_save_preset_name));
        if (ImGui::Button("OK##SavePreset") && std::strlen(s_save_preset_name) > 0)
        {
            current_preset_name = s_save_preset_name;
            preset_mgr.SavePreset(current_preset_name, config, backend);
            s_save_preset_name[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel##SavePreset"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    return load_name;
}

} // namespace View
} // namespace RocProfVis
