// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_ai_analysis.h"
#include "rocprofvis_controller_process_executor.h"

#include <spdlog/spdlog.h>
#include <filesystem>
#include <sstream>

namespace RocProfVis
{
namespace Controller
{

std::shared_ptr<ProcessExecutor::AsyncContext> AIAnalysisPipeline::GenerateAnalysisAsync(
    const AnalysisOptions& options,
    std::function<void(const AnalysisResult&)> completion_callback)
{
    // Validate input
    if(options.trace_file_path.empty())
    {
        spdlog::error("AIAnalysisPipeline: Trace file path is empty");
        if(completion_callback)
        {
            AnalysisResult result;
            result.success = false;
            result.error_message = "Trace file path is empty";
            completion_callback(result);
        }
        return nullptr;
    }

    if(!std::filesystem::exists(options.trace_file_path))
    {
        spdlog::error("AIAnalysisPipeline: Trace file does not exist: {}", options.trace_file_path);
        if(completion_callback)
        {
            AnalysisResult result;
            result.success = false;
            result.error_message = "Trace file does not exist: " + options.trace_file_path;
            completion_callback(result);
        }
        return nullptr;
    }

    // Find rocpd
    auto rocpd_path = FindRocpd();
    if(!rocpd_path)
    {
        spdlog::warn("AIAnalysisPipeline: rocpd not found in PATH or /opt/rocm");
        if(completion_callback)
        {
            AnalysisResult result;
            result.success = false;
            result.error_message = "rocpd executable not found. Please ensure ROCm profiler tools are installed.";
            completion_callback(result);
        }
        return nullptr;
    }

    // Generate output path if not provided
    std::string output_path = options.output_json_path;
    if(output_path.empty())
    {
        output_path = GenerateOutputPath(options.trace_file_path);
    }

    // Build command
    std::string command = BuildRocpdCommand(options.trace_file_path, output_path, options.llm_provider, options.llm_api_key);

    spdlog::info("AIAnalysisPipeline: Running analysis command: {}", command);

    // Setup execution options
    ProcessExecutor::ExecutionOptions exec_options;
    exec_options.capture_output = true;
    exec_options.timeout_seconds = 600;  // 10 minutes timeout

    // Set PYTHONPATH for rocpd Python module
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
            exec_options.environment["PYTHONPATH"] = path;
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
            exec_options.environment["PYTHONPATH"] = path;
            break;
        }
    }
#endif

    if(options.progress_callback)
    {
        exec_options.output_callback = options.progress_callback;
    }

    // Execute asynchronously
    auto context = ProcessExecutor::ExecuteAsync(
        command,
        exec_options,
        [output_path, completion_callback](const ProcessExecutor::ExecutionResult& exec_result)
        {
            AnalysisResult result;

            if(exec_result.exit_code == 0)
            {
                // Check if output file was created
                if(std::filesystem::exists(output_path))
                {
                    result.success = true;
                    result.json_file_path = output_path;
                    spdlog::info("AIAnalysisPipeline: Analysis completed successfully: {}", output_path);
                }
                else
                {
                    result.success = false;
                    result.error_message = "Analysis command succeeded but output file was not created: " + output_path;
                    spdlog::error("AIAnalysisPipeline: {}", result.error_message);
                }
            }
            else
            {
                result.success = false;
                result.error_message = exec_result.error_message.empty()
                    ? "rocpd analyze failed with exit code " + std::to_string(exec_result.exit_code)
                    : exec_result.error_message;
                spdlog::error("AIAnalysisPipeline: Analysis failed: {}", result.error_message);
            }

            if(completion_callback)
            {
                completion_callback(result);
            }
        }
    );

    return context;
}

AIAnalysisPipeline::AnalysisResult AIAnalysisPipeline::GenerateAnalysisSync(
    const AnalysisOptions& options)
{
    AnalysisResult result;

    // Validate input
    if(options.trace_file_path.empty())
    {
        result.success = false;
        result.error_message = "Trace file path is empty";
        return result;
    }

    if(!std::filesystem::exists(options.trace_file_path))
    {
        result.success = false;
        result.error_message = "Trace file does not exist: " + options.trace_file_path;
        return result;
    }

    // Find rocpd
    auto rocpd_path = FindRocpd();
    if(!rocpd_path)
    {
        result.success = false;
        result.error_message = "rocpd executable not found";
        return result;
    }

    // Generate output path if not provided
    std::string output_path = options.output_json_path;
    if(output_path.empty())
    {
        output_path = GenerateOutputPath(options.trace_file_path);
    }

    // Build command
    std::string command = BuildRocpdCommand(options.trace_file_path, output_path, options.llm_provider, options.llm_api_key);

    // Execute synchronously
    ProcessExecutor::ExecutionOptions exec_options;
    exec_options.capture_output = true;
    exec_options.timeout_seconds = 600;

    // Set PYTHONPATH for rocpd Python module
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
            exec_options.environment["PYTHONPATH"] = path;
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
            exec_options.environment["PYTHONPATH"] = path;
            break;
        }
    }
#endif

    auto exec_result = ProcessExecutor::ExecuteSync(command, exec_options);

    if(exec_result.exit_code == 0 && std::filesystem::exists(output_path))
    {
        result.success = true;
        result.json_file_path = output_path;
    }
    else
    {
        result.success = false;
        result.error_message = exec_result.error_message.empty()
            ? "rocpd analyze failed"
            : exec_result.error_message;
    }

    return result;
}

std::optional<std::string> AIAnalysisPipeline::FindRocpd()
{
    return ProcessExecutor::FindROCmTool("rocpd");
}

bool AIAnalysisPipeline::IsRocpdAvailable()
{
    return FindRocpd().has_value();
}

std::string AIAnalysisPipeline::BuildRocpdCommand(
    const std::string& trace_path,
    const std::string& output_path,
    const std::string& llm_provider,
    const std::string& api_key)
{
    std::ostringstream cmd;

    // rocpd is an executable at /opt/rocm/bin/rocpd
    cmd << "rocpd";
    cmd << " analyze";
    cmd << " -i \"" << trace_path << "\"";
    cmd << " --format json";
    cmd << " -o \"" << output_path << "\"";

    // Add LLM options if specified
    if(!llm_provider.empty() && llm_provider != "local")
    {
        cmd << " --llm " << llm_provider;
        if(!api_key.empty())
        {
            cmd << " --llm-api-key \"" << api_key << "\"";
        }
    }

    return cmd.str();
}

std::string AIAnalysisPipeline::GenerateOutputPath(const std::string& trace_path)
{
    std::filesystem::path trace_file(trace_path);
    std::filesystem::path output_dir = trace_file.parent_path();

    if(output_dir.empty())
    {
        output_dir = std::filesystem::current_path();
    }

    std::string base_name = trace_file.stem().string();
    std::filesystem::path output_file = output_dir / (base_name + "_ai_analysis.json");

    return output_file.string();
}

}  // namespace Controller
}  // namespace RocProfVis
