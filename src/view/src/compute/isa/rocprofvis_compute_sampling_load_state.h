// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

namespace RocProfVis
{
namespace View
{

enum class SamplingLoadState : uint32_t
{
    kReady,
    kLoading,
    kLoadingDetails,
    kTotalsOnly,
    kFailed
};

struct SamplingLoadStates
{
    SamplingLoadState sampling     = SamplingLoadState::kLoading;
    SamplingLoadState issue_states = SamplingLoadState::kLoading;

    [[nodiscard]] bool CanRetry() const
    {
        return sampling == SamplingLoadState::kFailed ||
               sampling == SamplingLoadState::kTotalsOnly ||
               issue_states == SamplingLoadState::kFailed;
    }
};

// Completion is independent of the combined request result, including empty results.
struct SamplingLoadStatus
{
    bool sample_states_loaded       = false;
    bool stall_details_loaded       = false;
    bool instruction_samples_loaded = false;

    [[nodiscard]] SamplingLoadStates Resolve(bool loading, bool failed) const
    {
        SamplingLoadStates states;
        if(sample_states_loaded && stall_details_loaded)
        {
            states.sampling = SamplingLoadState::kReady;
        }
        else if(sample_states_loaded)
        {
            states.sampling = loading ? SamplingLoadState::kLoadingDetails
                                      : SamplingLoadState::kTotalsOnly;
        }
        else if(failed)
        {
            states.sampling = SamplingLoadState::kFailed;
        }
        if(sample_states_loaded && instruction_samples_loaded)
        {
            states.issue_states = SamplingLoadState::kReady;
        }
        else if(failed)
        {
            states.issue_states = SamplingLoadState::kFailed;
        }
        return states;
    }
};

}  // namespace View
}  // namespace RocProfVis
