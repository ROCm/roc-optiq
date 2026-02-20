// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_process_executor.h"
#include "widgets/rocprofvis_widget.h"

#include <string>
#include <memory>
#include <functional>

namespace RocProfVis
{
namespace View
{

class CommandExecutionDialog : public RocWidget
{
public:
    CommandExecutionDialog();
    ~CommandExecutionDialog();

    void Show(const std::string& command);
    void Hide();
    bool IsVisible() const;

    void Render() override;

    // Callback when command completes successfully with a trace file
    void SetCompletionCallback(std::function<void(const std::string& trace_path)> callback);

private:
    void ExecuteCommand();
    void OnComplete(const Controller::ProcessExecutor::ExecutionResult& result);
    std::string FindTraceFile(const std::string& output) const;

    bool m_visible;
    std::string m_command;
    std::string m_output;
    bool m_running;
    bool m_success;
    std::string m_error_message;
    std::string m_trace_file_path;

    std::shared_ptr<Controller::ProcessExecutor::AsyncContext> m_execution_context;
    std::function<void(const std::string&)> m_completion_callback;
};

}  // namespace View
}  // namespace RocProfVis
