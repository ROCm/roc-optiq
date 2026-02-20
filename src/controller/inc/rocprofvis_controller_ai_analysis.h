// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_process_executor.h"

#include <string>
#include <optional>
#include <functional>

namespace RocProfVis
{
namespace Controller
{

class Future;

class AIAnalysisPipeline
{
public:
    struct AnalysisOptions
    {
        std::string trace_file_path;
        std::string output_json_path;  // If empty, will be auto-generated
        bool auto_open_results = true;
        std::function<void(const std::string&)> progress_callback;
        std::string llm_provider;  // "anthropic", "openai", or empty/local
        std::string llm_api_key;
    };

    struct AnalysisResult
    {
        bool success = false;
        std::string json_file_path;
        std::string error_message;
    };

    // Run rocpd analyze on trace file asynchronously
    static std::shared_ptr<ProcessExecutor::AsyncContext> GenerateAnalysisAsync(
        const AnalysisOptions& options,
        std::function<void(const AnalysisResult&)> completion_callback = nullptr);

    // Synchronous version
    static AnalysisResult GenerateAnalysisSync(const AnalysisOptions& options);

    // Find rocpd executable
    static std::optional<std::string> FindRocpd();

    // Check if rocpd is available
    static bool IsRocpdAvailable();

private:
    static std::string BuildRocpdCommand(
        const std::string& trace_path,
        const std::string& output_path,
        const std::string& llm_provider = "",
        const std::string& api_key = "");

    static std::string GenerateOutputPath(const std::string& trace_path);
};

}  // namespace Controller
}  // namespace RocProfVis
