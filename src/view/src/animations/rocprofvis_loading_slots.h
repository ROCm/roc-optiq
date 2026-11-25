// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>

namespace RocProfVis
{
namespace View
{

class SettingsManager;

/**
 * @brief Holds the place of rows that are still on their way: a column of
 * button-sized slots, each drawn as a miniature timeline track that fills like
 * a loading bar.
 *
 * A playhead sweeps across each track and lays event blocks down behind it, the
 * blocks hold for a moment and fade, and the next sweep lays down a different
 * lane. Each slot starts a beat after the one above it, so the column reads as
 * one wave rather than several spinners.
 *
 * Slots span the content region and are one frame tall at the current
 * FramePadding, so drawn under the style a set of buttons uses, they take the
 * room those buttons will. Keeps the render loop awake while drawn.
 *
 * @param slot_count How many slots to stack.
 * @param elapsed_seconds Time since the wait began. Timing from there is what
 * makes the first sweep start on an empty track rather than partway across.
 * @param settings Theme to draw with; the shared instance when null.
 */
void
RenderLoadingSlots(size_t slot_count, float elapsed_seconds,
                   SettingsManager* settings = nullptr);

}  // namespace View
}  // namespace RocProfVis
