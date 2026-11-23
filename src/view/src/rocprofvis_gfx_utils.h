// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "imgui.h"

namespace RocProfVis
{
namespace View
{
namespace GfxUtils
{

// General interpolation and colour maths for drawing code. Dear ImGui already
// has ImLerp for ImVec2 and ImVec4, ImSaturate, and ImBezierCubicCalc in
// imgui_internal.h, so only what it lacks belongs here.

/**
 * @brief The ease-in-out curve 3t^2 - 2t^3, also known as smoothstep.
 *
 * @param t Progress along the ramp. Clamped to [0, 1].
 * @return float 0 at the start, 1 at the end, and level at both, so a fade or
 * a move driven by it starts and stops gently.
 */
float
SmoothStep(float t);

/**
 * @brief Blends two packed colours channel by channel, alpha included.
 *
 * @param a The colour at t = 0.
 * @param b The colour at t = 1.
 * @param t How far from a to b. Not clamped.
 * @return ImU32 The blended colour.
 */
ImU32
LerpColor(ImU32 a, ImU32 b, float t);

}  // namespace GfxUtils
}  // namespace View
}  // namespace RocProfVis
