// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_enums.h"

#include <string>

namespace RocProfVis
{
namespace Controller
{
namespace ProfilerTool
{

/*
 * Identity and discovery for the profiler binaries the launcher can run.
 *
 * A launch names its tool with rocprofvis_profiler_tool_t; this is the only
 * place that knows what that enum is called on disk and where to look for it.
 * Callers never supply the name of an executable. The one thing a caller may
 * say is which *directory* to look in, for a ROCm install in a non-standard
 * location - and because the filename still comes from the table below, no
 * configuration file can name an arbitrary program to run.
 *
 * These are free functions rather than methods so the UI can resolve a tool for
 * a command preview or a pre-flight check without allocating a session.
 */

/*
 * The executable name for `tool`, without any directory and without a platform
 * suffix (ResolvePath adds ".exe" on Windows). Returns nullptr for
 * kRPVProfilerToolNone and for any value outside the enum.
 */
char const* GetBinaryName(rocprofvis_profiler_tool_t tool);

/*
 * Resolves `tool` to an absolute path to an existing executable.
 *
 * If `tool_directory` is non-empty it is the only place searched: the tool must
 * be at <tool_directory>/<name>, and if it is not, resolution fails rather than
 * falling through to the default search. That strictness is the point - someone
 * configures a directory precisely because they have more than one ROCm install,
 * so quietly running the other one is the confusion they were trying to end.
 * The directory must be absolute; a relative one would mean different things to
 * this process and to a child given its own working directory.
 *
 * With no directory configured, the default search is
 * $ROCM_PATH/bin/<name> (defaulting to /opt/rocm on non-Windows), then each
 * $PATH entry in order. Resolving to a full path here, rather than leaving a
 * bare name for execvp, means what runs is decided (and can be shown to the
 * user) before the process is created, and a "not found" is reported as such
 * instead of surfacing later as exit code 127.
 *
 * Returns kRocProfVisResultToolNotFound if no candidate exists,
 * kRocProfVisResultInvalidArgument for an unknown tool or a relative directory,
 * and writes the resolved path to out_path on success.
 */
rocprofvis_result_t ResolvePath(rocprofvis_profiler_tool_t tool,
                                std::string const&         tool_directory,
                                std::string&               out_path);

} // namespace ProfilerTool
} // namespace Controller
} // namespace RocProfVis
