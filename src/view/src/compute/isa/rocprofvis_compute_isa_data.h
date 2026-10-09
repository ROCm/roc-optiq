// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "model/compute/rocprofvis_compute_model_types.h"

namespace RocProfVis
{
namespace View
{

// Builds the ISA presentation data without depending on ImGui or settings.
class IsaDataBuilder
{
public:
    enum class SamplingAvailability : uint32_t
    {
        kMissing,
        kInvalid,
        kValid
    };

    enum class StallReasonKind : uint32_t
    {
        kInstructionFetch,
        kAluDependency,
        kWaitcnt,
        kInternalInstruction,
        kBarrier,
        kArbiter,
        kPipelineStall,
        kOtherWait,
        kSleeping,
        kUnknown
    };

    struct SamplingSummary
    {
        // Missing counts differ from measured zero; invalid counts never yield shares.
        SamplingAvailability availability       = SamplingAvailability::kMissing;
        double               stall_percent      = 0.0;
        double               issue_percent      = 0.0;
        uint64_t             unclassified_count = 0;
    };

    struct StallReason
    {
        std::string     text;
        uint64_t        count = 0;
        StallReasonKind kind  = StallReasonKind::kUnknown;
        std::string     count_text;
        double          share     = 0.0;  // Percentage of measured stalled samples.
        double          bar_share = 0.0;  // Fraction of the bounded reason bar.
    };

    struct SampleCounts
    {
        uint64_t                total_count = 0;
        std::optional<uint64_t> issue_count;
        std::optional<uint64_t> stall_count;
    };

    struct IsaRow
    {
        std::string              instruction;
        uint64_t                 id                 = 0;
        uint64_t                 code_object_offset = 0;
        uint64_t                 source_line_id     = 0;
        uint64_t                 source_file_id     = 0;
        SampleCounts             sample_counts;
        SamplingSummary          sampling;
        uint64_t                 unclassified_stall_count = 0;
        std::string              unclassified_stall_count_text;
        uint64_t                 stall_reason_sample_count = 0;
        std::vector<StallReason> stall_reasons;
    };

    struct SamplingTotals
    {
        uint64_t kernel_total_samples        = 0;
        uint64_t hottest_instruction_samples = 0;
        bool     stall_data_available        = false;
    };

    static std::vector<IsaRow> BuildInstructions(const PcSamplingData& data,
                                                 uint64_t              code_object_uuid);
    static void                UpdateSourceLocations(std::vector<IsaRow>&  rows,
                                                     const PcSamplingData& data);
    static SamplingTotals      UpdateSampling(std::vector<IsaRow>&  rows,
                                              const PcSamplingData& data);
    static SamplingSummary     SummarizeSampling(const SampleCounts& counts);
    static StallReasonKind     ResolveStallReasonKind(std::string_view text);
    static double              CalculatePercentage(uint64_t value, uint64_t total);
    static std::string         FormatSampleCount(uint64_t value);

private:
    // Temporary lookup tables used by the corresponding layer update.
    struct SourceLocation
    {
        uint64_t source_line_id = 0;
        uint64_t source_file_id = 0;
    };

    struct SampleAggregation
    {
        std::unordered_map<uint64_t, SampleCounts>         counts_by_instruction;
        std::unordered_map<uint64_t, const PcSampleState*> sample_state_by_uuid;
        uint64_t                                           kernel_total_samples = 0;
    };

    typedef std::unordered_map<uint64_t, uint64_t>          StallReasonCounts;
    typedef std::unordered_map<uint64_t, StallReasonCounts> GroupedStallReasonCounts;

    static const CodeObjectStore* FindCodeObject(const PcSamplingData& data,
                                                 uint64_t              code_object_uuid);
    static std::unordered_map<uint64_t, SourceLocation> BuildSourceLocations(
        const PcSamplingData& data);
    static SampleAggregation AggregateSampleCounts(const PcSamplingData& data);
    static std::unordered_map<uint64_t, std::string> BuildStallReasonText(
        const PcSamplingData& data);
    static GroupedStallReasonCounts BuildStallReasonCounts(
        const PcSamplingData& data, const SampleAggregation& sample_aggregation,
        const std::unordered_map<uint64_t, std::string>& reason_text);
    static GroupedStallReasonCounts GroupStallReasonCountsBySampleState(
        const PcSamplingData& data);
    static std::vector<uint64_t> FindOtherWaitLookupUuids(
        const std::unordered_map<uint64_t, std::string>& reason_text);
    static uint64_t SumStallReasonCounts(const StallReasonCounts& reason_counts);
    static void     RemoveIssuedSamplesFromOtherWait(
            const PcSampleState& state, const std::vector<uint64_t>& other_wait_lookup_uuids,
            StallReasonCounts& reason_counts);
    static GroupedStallReasonCounts AggregateStallReasonCountsByInstruction(
        GroupedStallReasonCounts     counts_by_sample_state,
        const SampleAggregation&     sample_aggregation,
        const std::vector<uint64_t>& other_wait_lookup_uuids);
    static std::vector<StallReason> BuildStallReasons(
        const StallReasonCounts&                         reason_counts,
        const std::unordered_map<uint64_t, std::string>& reason_text,
        uint64_t&                                        classified_sample_count);
    static std::string ResolveStallReasonText(
        const std::unordered_map<uint64_t, std::string>& reason_text,
        uint64_t                                         lookup_uuid);
};

}  // namespace View
}  // namespace RocProfVis
