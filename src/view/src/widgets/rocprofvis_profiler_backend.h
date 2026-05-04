// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_launch_config.h"
#include "json.h"
#include <string>
#include <vector>
#include <utility>
#include <functional>

namespace RocProfVis
{
namespace View
{

struct ToolOption
{
    std::string id;
    std::string display_name;
};

struct TabDescriptor
{
    std::string id;
    std::string display_name;
    std::function<void(jt::Json& payload)> render_fn;
};

/**
 * Interface for profiler-specific backends.
 * Each profiler type (rocprof-sys, rocprof-compute, rocprofv3) implements this.
 */
class IProfilerBackend
{
public:
    virtual ~IProfilerBackend() = default;

    virtual const char* Id() const = 0;
    virtual const char* DisplayName() const = 0;

    virtual std::vector<ToolOption> GetTools() const = 0;
    virtual std::string GetDefaultBinary(std::string const& tool_id) const = 0;

    virtual std::vector<TabDescriptor> GetTabs(std::string const& tool_id) const = 0;

    /**
     * Validate the config before launch. Returns empty string on success,
     * or a human-readable error message on failure.
     */
    virtual std::string Validate(LaunchConfig const& config) const = 0;

    /**
     * Convert the curated backend_payload fields into env vars and argv for the
     * profiler process. The caller merges extra_env on top afterwards.
     */
    virtual void FlattenToExecution(
        LaunchConfig const& config,
        std::vector<std::pair<std::string, std::string>>& env_out,
        std::vector<std::string>& argv_out) const = 0;

    virtual jt::Json DefaultPayload() const = 0;

    /**
     * Export the current config to a rocprof-sys .cfg format string.
     * Returns empty string if export is not supported by this backend.
     */
    virtual std::string ExportCfg(jt::Json const& payload) const = 0;
};

} // namespace View
} // namespace RocProfVis
