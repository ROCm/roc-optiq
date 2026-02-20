// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_command_execution_dialog.h"
#include "rocprofvis_appwindow.h"
#include "widgets/rocprofvis_dialog.h"

#include "imgui.h"
#include <regex>

namespace RocProfVis
{
namespace View
{

CommandExecutionDialog::CommandExecutionDialog()
: m_visible(false)
, m_running(false)
, m_success(false)
{
    m_widget_name = GenUniqueName("CommandExecutionDialog");
}

CommandExecutionDialog::~CommandExecutionDialog()
{
    if(m_execution_context)
    {
        m_execution_context->Cancel();
    }
}

void CommandExecutionDialog::Show(const std::string& command)
{
    m_visible = true;
    m_command = command;
    m_output.clear();
    m_running = false;
    m_success = false;
    m_error_message.clear();
    m_trace_file_path.clear();

    ExecuteCommand();
}

void CommandExecutionDialog::Hide()
{
    m_visible = false;
}

bool CommandExecutionDialog::IsVisible() const
{
    return m_visible;
}

void CommandExecutionDialog::SetCompletionCallback(std::function<void(const std::string&)> callback)
{
    m_completion_callback = callback;
}

void CommandExecutionDialog::Render()
{
    if(!m_visible)
    {
        return;
    }

    // Open popup if not already open (must be called before BeginPopupModal)
    if(!ImGui::IsPopupOpen("Execute Command##cmd_exec_dialog"))
    {
        ImGui::OpenPopup("Execute Command##cmd_exec_dialog");
    }

    PopUpStyle popup_style;
    popup_style.CenterPopup();

    ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);

    if(ImGui::BeginPopupModal("Execute Command##cmd_exec_dialog", &m_visible,
                              ImGuiWindowFlags_NoResize))
    {
        ImGui::TextUnformatted("Command:");
        ImGui::Spacing();

        // Display command
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        ImGui::InputTextMultiline("##command", const_cast<char*>(m_command.c_str()),
                                   m_command.size() + 1,
                                   ImVec2(-1, ImGui::GetTextLineHeight() * 2),
                                   ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleColor(2);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Status
        if(m_running)
        {
            ImGui::TextUnformatted("Status: Running...");
        }
        else if(m_success)
        {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Status: Completed Successfully");
        }
        else if(!m_error_message.empty())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Status: Failed");
        }

        ImGui::Spacing();

        // Output
        ImGui::TextUnformatted("Output:");
        ImGui::BeginChild("##output", ImVec2(0, -40), true);
        ImGui::TextUnformatted(m_output.c_str());

        // Auto-scroll to bottom
        if(ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();

        ImGui::Spacing();

        // Buttons
        if(m_running)
        {
            float button_width = 100.0f;
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - button_width - 20.0f);
            if(ImGui::Button("Cancel", ImVec2(button_width, 0)))
            {
                if(m_execution_context)
                {
                    m_execution_context->Cancel();
                    m_running = false;
                    m_error_message = "Cancelled by user";
                }
            }
        }
        else
        {
            float button_width = 120.0f;
            float spacing = ImGui::GetStyle().ItemSpacing.x;

            if(m_success && !m_trace_file_path.empty())
            {
                float total_width = button_width * 2 + spacing;
                ImGui::SetCursorPosX(ImGui::GetWindowWidth() - total_width - 20.0f);

                if(ImGui::Button("View Results", ImVec2(button_width, 0)))
                {
                    if(m_completion_callback)
                    {
                        m_completion_callback(m_trace_file_path);
                    }
                    Hide();
                }

                ImGui::SameLine();
            }
            else
            {
                ImGui::SetCursorPosX(ImGui::GetWindowWidth() - button_width - 20.0f);
            }

            if(ImGui::Button("Close", ImVec2(button_width, 0)))
            {
                Hide();
            }
        }

        ImGui::EndPopup();
    }
}

void CommandExecutionDialog::ExecuteCommand()
{
    m_running = true;
    m_output = "Executing command...\n\n";

    Controller::ProcessExecutor::ExecutionOptions options;
    options.capture_output = true;
    options.timeout_seconds = 300;  // 5 minutes
    options.output_callback = [this](const std::string& line)
    {
        m_output += line;

        // Limit output size
        if(m_output.size() > 100000)
        {
            m_output = m_output.substr(m_output.size() - 50000);
        }
    };

    m_execution_context = Controller::ProcessExecutor::ExecuteAsync(
        m_command,
        options,
        [this](const Controller::ProcessExecutor::ExecutionResult& result)
        {
            OnComplete(result);
        }
    );
}

void CommandExecutionDialog::OnComplete(const Controller::ProcessExecutor::ExecutionResult& result)
{
    m_running = false;
    m_output += "\n\n=== Execution Complete ===\n";

    if(result.exit_code == 0)
    {
        m_success = true;
        m_trace_file_path = FindTraceFile(result.stdout_output);

        if(m_trace_file_path.empty())
        {
            m_output += "\nCommand completed successfully but no trace file was found in output.\n";
        }
        else
        {
            m_output += "\nTrace file: " + m_trace_file_path + "\n";
        }
    }
    else
    {
        m_success = false;
        m_error_message = result.error_message.empty()
            ? "Command failed with exit code " + std::to_string(result.exit_code)
            : result.error_message;
        m_output += "\nError: " + m_error_message + "\n";
    }
}

std::string CommandExecutionDialog::FindTraceFile(const std::string& output) const
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

}  // namespace View
}  // namespace RocProfVis
