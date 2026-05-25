// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

struct ImGuiTestEngine;

namespace RocProfVis
{
namespace View
{
namespace Test
{

int
RegisterRocOptiqUiTests(ImGuiTestEngine* engine, bool include_app_havoc = false);

}  // namespace Test
}  // namespace View
}  // namespace RocProfVis
