// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

namespace RocProfVis
{
namespace Controller
{

class ScriptResult;

void optiq_prepare_globals(void* py_dict, void* script_session);

}  // namespace Controller
}  // namespace RocProfVis
