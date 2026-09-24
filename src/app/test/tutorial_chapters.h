// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
#pragma once

struct ImGuiTestEngine;

namespace RocProfVis
{
namespace Tutorial
{

// Registers one test per tutorial video; each records its own lossless .mkv.
// Also keeps a copy of the samples as they are now, before any chapter opens
// them, for every chapter to start from.
void RegisterChapters(ImGuiTestEngine* engine);

}  // namespace Tutorial
}  // namespace RocProfVis
