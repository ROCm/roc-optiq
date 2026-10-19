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

    struct StallReasonInfo
    {
        StallReasonKind kind;
        const char*     name;
    };

    inline static constexpr StallReasonInfo STALL_REASONS[] = {
        { StallReasonKind::kInstructionFetch, "NO_INSTRUCTION_AVAILABLE" },
        { StallReasonKind::kAluDependency, "ALU_DEPENDENCY" },
        { StallReasonKind::kWaitcnt, "WAITCNT" },
        { StallReasonKind::kInternalInstruction, "INTERNAL_INSTRUCTION" },
        { StallReasonKind::kBarrier, "BARRIER_WAIT" },
        { StallReasonKind::kArbiter, "ARBITER_NOT_WIN" },
        { StallReasonKind::kPipelineStall, "ARBITER_WIN_EX_STALL" },
        { StallReasonKind::kOtherWait, "OTHER_WAIT" },
        { StallReasonKind::kSleeping, "SLEEP" }
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
        double          bar_share = 0.0;  // Fraction of the bounded reason bar.
    };

    struct SampleCounts
    {
        uint64_t                total_count = 0;
        std::optional<uint64_t> issue_count;
        std::optional<uint64_t> stall_count;
        // Carries malformed contributors and overflow through subsequent grouping.
        bool invalid_counts = false;
    };

    struct StallReasonCounts
    {
        std::unordered_map<uint64_t, uint64_t> by_lookup;
        uint64_t                               total_count = 0;
        bool                                   overflow    = false;
    };
    typedef std::unordered_map<uint64_t, StallReasonCounts> GroupedStallReasonCounts;

    // Owns normalized instruction sampling independently of source/ISA layer arrival.
    struct PreparedSamplingData
    {
        std::unordered_map<uint64_t, SampleCounts> counts_by_instruction;
        GroupedStallReasonCounts                   reasons_by_instruction;
        std::unordered_map<uint64_t, std::string>  reason_text;
    };

    struct SamplingRow
    {
        uint64_t                 id = 0;
        SampleCounts             sample_counts;
        SamplingSummary          sampling;
        uint64_t                 unclassified_stall_count = 0;
        std::string              unclassified_stall_count_text;
        uint64_t                 stall_reason_sample_count    = 0;
        bool                     stall_reason_counts_overflow = false;
        std::vector<StallReason> stall_reasons;
    };

    struct IsaRow : SamplingRow
    {
        std::string instruction;
        uint64_t    code_object_offset = 0;
        uint64_t    source_line_id     = 0;
        uint64_t    source_file_id     = 0;
    };

    struct SourceRow : SamplingRow
    {
        std::string content;
        uint64_t    line_number = 0;
    };

    [[nodiscard]] static std::vector<IsaRow> BuildInstructions(const PcSamplingData& data,
                                                               uint64_t code_object_uuid);
    static void UpdateSourceLocations(std::vector<IsaRow>&  rows,
                                      const PcSamplingData& data);
    // Prepares owned counts and normalized reasons once for both panes.
    [[nodiscard]] static PreparedSamplingData PrepareSampling(const PcSamplingData& data);
    // Updates row sampling data and returns the hottest displayed instruction's count.
    static uint64_t                             UpdateSampling(std::vector<IsaRow>&        rows,
                                                               const PreparedSamplingData& sampling);
    [[nodiscard]] static std::vector<SourceRow> BuildSourceLines(
        const PcSamplingData& data, uint64_t source_file_uuid);
    // Aggregates mapped instructions once per line; returns the hottest source count.
    static uint64_t UpdateSourceSampling(std::vector<SourceRow>&     rows,
                                         const PcSamplingData&       data,
                                         uint64_t                    source_file_uuid,
                                         const PreparedSamplingData& sampling);
    [[nodiscard]] static SamplingSummary SummarizeSampling(const SampleCounts& counts);
    [[nodiscard]] static StallReasonKind ResolveStallReasonKind(std::string_view text);
    [[nodiscard]] static const char*     GetStallReasonName(StallReasonKind kind);
    [[nodiscard]] static double      CalculatePercentage(uint64_t value, uint64_t total);
    [[nodiscard]] static std::string FormatSampleCount(uint64_t value);

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
    };

    static bool AddSampleCount(uint64_t& total, uint64_t count);
    static void AccumulateSampleCounts(SampleCounts& total, const SampleCounts& counts);
    static void AddStallReasonCount(StallReasonCounts& counts, uint64_t lookup_uuid,
                                    uint64_t count);
    static void MergeStallReasonCounts(StallReasonCounts&       total,
                                       const StallReasonCounts& counts);

    static void UpdateSamplingRow(
        SamplingRow& row, const SampleCounts& counts, const StallReasonCounts& reasons,
        const std::unordered_map<uint64_t, std::string>& reason_text);

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
    static void RemoveIssuedSamplesFromOtherWait(
        const PcSampleState& state, const std::vector<uint64_t>& other_wait_lookup_uuids,
        StallReasonCounts& reason_counts);
    static GroupedStallReasonCounts AggregateStallReasonCountsByInstruction(
        GroupedStallReasonCounts     counts_by_sample_state,
        const SampleAggregation&     sample_aggregation,
        const std::vector<uint64_t>& other_wait_lookup_uuids);
    static std::vector<StallReason> BuildStallReasons(
        const StallReasonCounts&                         reason_counts,
        const std::unordered_map<uint64_t, std::string>& reason_text);
    static std::string ResolveStallReasonText(
        const std::unordered_map<uint64_t, std::string>& reason_text,
        uint64_t                                         lookup_uuid);
};

}  // namespace View
}  // namespace RocProfVis
