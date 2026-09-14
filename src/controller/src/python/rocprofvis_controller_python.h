// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_python_runtime.h"

namespace RocProfVis
{
namespace Controller
{

class ScriptResult;

// Builds the optiq environment for one run. Anything but
// kRocProfVisPythonSuccess stops the script from running: an error with a
// Python exception set if the environment could not be finished, or
// kRocProfVisPythonCancelled if the session was cancelled before it started.
rocprofvis_python_result_t optiq_prepare_globals(void* py_dict, void* script_session);

// Undoes the interpreter-wide half of the above: takes optiq back out of
// sys.modules and empties the module, so nothing that outlives the run can
// still reach the session, the result or the controller.
void optiq_teardown_globals(void* py_dict, void* script_session);

}  // namespace Controller
}  // namespace RocProfVis
