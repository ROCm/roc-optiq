// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_profiler_tool.h"

#include "spdlog/spdlog.h"

#include <cstdlib>
#include <filesystem>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace RocProfVis
{
namespace Controller
{
namespace ProfilerTool
{

namespace
{

#ifdef _WIN32
constexpr char kPathListSeparator = ';';
constexpr char const* kExecutableSuffix = ".exe";
#else
constexpr char kPathListSeparator = ':';
constexpr char const* kExecutableSuffix = "";
// Where a ROCm install lives when $ROCM_PATH says nothing. There is no
// equivalent convention on Windows, so that platform relies on $ROCM_PATH and
// $PATH alone.
constexpr char const* kDefaultRocmPath = "/opt/rocm";
#endif

// Reads an environment variable, returning empty when unset. std::getenv is used
// rather than the _dupenv_s family to keep one implementation across platforms;
// the value is copied immediately, so the returned pointer is not retained.
std::string get_env(char const* name)
{
    char const* value = std::getenv(name);
    return (value != nullptr) ? std::string(value) : std::string();
}

// True if `path` names an existing file this process may execute. The
// std::error_code overloads are used throughout because the throwing
// std::filesystem overloads are not an option in this codebase.
bool is_executable_file(std::filesystem::path const& path)
{
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec) || ec)
    {
        // Follow symlinks: is_regular_file already does, but a broken link
        // lands here and is correctly rejected.
        return false;
    }

#ifdef _WIN32
    // Windows has no execute permission bit that is meaningful here; presence
    // of the file (with its .exe suffix already applied) is the test.
    return true;
#else
    return ::access(path.c_str(), X_OK) == 0;
#endif
}

// Appends the platform's executable suffix to a bare tool name.
std::string with_suffix(char const* binary_name)
{
    return std::string(binary_name) + kExecutableSuffix;
}

} // namespace

char const* GetBinaryName(rocprofvis_profiler_tool_t tool)
{
    switch (tool)
    {
        case kRPVProfilerToolRocprofSysRun:
            return "rocprof-sys-run";
        case kRPVProfilerToolRocprofSysSample:
            return "rocprof-sys-sample";
        case kRPVProfilerToolRocprofSysInstrument:
            return "rocprof-sys-instrument";
        case kRPVProfilerToolRocprofSysAvail:
            return "rocprof-sys-avail";
        case kRPVProfilerToolRocprofCompute:
            return "rocprof-compute";
        case kRPVProfilerToolRocprofV3:
            return "rocprofv3";
        case kRPVProfilerToolNone:
        default:
            return nullptr;
    }
}

rocprofvis_result_t ResolvePath(rocprofvis_profiler_tool_t tool,
                                std::string const&         tool_directory,
                                std::string&               out_path)
{
    char const* binary_name = GetBinaryName(tool);
    if (binary_name == nullptr)
    {
        spdlog::error("ProfilerTool::ResolvePath: unknown tool {}", static_cast<int>(tool));
        return kRocProfVisResultInvalidArgument;
    }

    std::string const file_name = with_suffix(binary_name);

    if (!tool_directory.empty())
    {
        if (!std::filesystem::path(tool_directory).is_absolute())
        {
            spdlog::error("Profiler tool directory must be an absolute path, got '{}'",
                          tool_directory);
            return kRocProfVisResultInvalidArgument;
        }

        std::filesystem::path candidate = std::filesystem::path(tool_directory) / file_name;
        if (!is_executable_file(candidate))
        {
            spdlog::error("'{}' was not found in the configured tool directory '{}'", file_name,
                          tool_directory);
            return kRocProfVisResultToolNotFound;
        }
        // Deliberately no fallback to the default search: a configured directory
        // that lacks the tool is reported, not quietly replaced by whatever else
        // is installed - which would be a different build of the same tool.
        spdlog::warn("Using configured tool directory for '{}': '{}'", file_name, tool_directory);
        out_path = candidate.lexically_normal().string();
        return kRocProfVisResultSuccess;
    }

    std::string rocm_path = get_env("ROCM_PATH");
#ifndef _WIN32
    if (rocm_path.empty())
    {
        rocm_path = kDefaultRocmPath;
    }
#endif
    if (!rocm_path.empty())
    {
        std::filesystem::path candidate =
            std::filesystem::path(rocm_path) / "bin" / file_name;
        if (is_executable_file(candidate))
        {
            out_path = candidate.lexically_normal().string();
            return kRocProfVisResultSuccess;
        }
    }

    std::string const search_path = get_env("PATH");
    size_t            start       = 0;
    while (start <= search_path.size())
    {
        size_t end = search_path.find(kPathListSeparator, start);
        if (end == std::string::npos)
        {
            end = search_path.size();
        }

        std::string entry = search_path.substr(start, end - start);
        start             = end + 1;

        if (entry.empty())
        {
            continue;
        }

        std::filesystem::path candidate = std::filesystem::path(entry) / file_name;
        if (is_executable_file(candidate))
        {
            // Absolute so that what was found is what runs, even if this
            // process's working directory differs from the child's.
            std::error_code ec;
            std::filesystem::path absolute = std::filesystem::absolute(candidate, ec);
            out_path = (ec ? candidate : absolute).lexically_normal().string();
            return kRocProfVisResultSuccess;
        }
    }

    spdlog::error("Profiler tool '{}' was not found in $ROCM_PATH/bin or on $PATH", file_name);
    return kRocProfVisResultToolNotFound;
}

} // namespace ProfilerTool
} // namespace Controller
} // namespace RocProfVis
