// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

/**
 * @brief The arithmetic behind compare_traces, kept free of the data model so
 * it can be tested on its own.
 *
 * Every figure a run comparison quotes is worked out here rather than by the
 * model: a diff is nothing but subtraction and division, which is exactly the
 * kind of figure a model gets wrong when it computes one itself.
 */

// One event name's figures in one run.
struct RunEventFigures
{
    std::string name;
    uint64_t    total_ns = 0;
    uint64_t    calls    = 0;
};

// One event family's figures in one run: kernels, memory or thread operations.
struct RunCategoryFigures
{
    std::vector<RunEventFigures> events;
    // The summary cut the list short into an "Others" row, so a name missing
    // from the list may still have run.
    bool     truncated = false;
    uint64_t total_ns  = 0;
    uint64_t calls     = 0;
};

enum class RunDiffState : uint8_t
{
    // Listed in both runs, so the change is known, including no change at all.
    kBoth,
    // Listed in the baseline only, and the target's list is complete.
    kGone,
    // Listed in the target only, and the baseline's list is complete.
    kNew,
    // Listed in the baseline; the target keeps its figure inside "Others".
    kHiddenInTarget,
    // Listed in the target; the baseline keeps its figure inside "Others".
    kHiddenInBaseline
};

struct RunDiffRow
{
    std::string  name;
    RunDiffState state          = RunDiffState::kBoth;
    uint64_t     baseline_ns    = 0;
    uint64_t     target_ns      = 0;
    uint64_t     baseline_calls = 0;
    uint64_t     target_calls   = 0;
};

// Pairs the two runs' events by name, largest change first. Rows whose change
// is unknown go last, since there is nothing to rank them by.
std::vector<RunDiffRow>
DiffRunCategory(const RunCategoryFigures& baseline, const RunCategoryFigures& target);

// "a -> b (+d, +p%)". No percentage when the baseline is zero, because a
// change from nothing has no meaningful ratio.
std::string
FormatRunChange(uint64_t baseline, uint64_t target);

// One family's comparison as tool output: its totals, then up to limit names.
std::string
FormatRunCategory(const char* label, const RunCategoryFigures& baseline,
                  const RunCategoryFigures& target, size_t limit);

}  // namespace View
}  // namespace RocProfVis
