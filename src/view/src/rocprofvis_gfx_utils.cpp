// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_gfx_utils.h"

#include "imgui_internal.h"

namespace RocProfVis
{
namespace View
{
namespace GfxUtils
{

float
SmoothStep(float t)
{
    const float clamped = ImSaturate(t);
    return clamped * clamped * (3.0f - 2.0f * clamped);
}

ImU32
LerpColor(ImU32 a, ImU32 b, float t)
{
    return ImGui::ColorConvertFloat4ToU32(
        ImLerp(ImGui::ColorConvertU32ToFloat4(a), ImGui::ColorConvertU32ToFloat4(b), t));
}

}  // namespace GfxUtils
}  // namespace View
}  // namespace RocProfVis
