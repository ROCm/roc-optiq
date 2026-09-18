// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

// Scripted driving of the assistant, for evaluation runs. The panel's turn loop
// already advances from Update() rather than Render(), so a run needs no input
// events and no visible panel: it presses the same two buttons a person would
// and writes what came back. The types live here rather than in the panel
// header so the view module can start a run without including the widget.

#include <cstdint>
#include <string>

namespace RocProfVis
{
namespace View
{

// How long a whole run may take before it is abandoned. Sized for a full
// self-directed investigation - twenty tool rounds, each of which may wait on a
// query over a large trace - rather than for one question.
constexpr uint32_t ASSISTANT_BATCH_DEFAULT_TIMEOUT_SECONDS = 900;

// What one scripted run should do.
struct AssistantBatchRequest
{
    std::string question;
    std::string output_path;
    // Run Explain this view first, so the question lands as a follow-up in the
    // same conversation. This is what the panel's own button does, and a run
    // that skips it is asking a different question of a different context.
    bool explain_first = false;
    // Zero takes ASSISTANT_BATCH_DEFAULT_TIMEOUT_SECONDS.
    uint32_t timeout_seconds = 0;
};

// Where a run has got to. Polled by the app shell, which owns the exit.
enum class AssistantBatchState : uint32_t
{
    kInactive,
    kRunning,
    kDone,
    kFailed
};

}  // namespace View
}  // namespace RocProfVis
