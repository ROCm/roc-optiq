// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_isa_data.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <utility>

namespace RocProfVis
{
namespace View
{

static_assert(std::size(IsaDataBuilder::STALL_REASONS) ==
              static_cast<size_t>(IsaDataBuilder::StallReasonKind::kUnknown));
static_assert(std::size(IsaDataBuilder::INSTRUCTION_TYPES) ==
              static_cast<size_t>(IsaDataBuilder::InstructionKind::kUnknown));

const CodeObjectStore*
IsaDataBuilder::FindCodeObject(const PcSamplingData& data, uint64_t code_object_uuid)
{
    for(const CodeObjectStore& code_object : data.code_objects)
    {
        if(code_object.code_object_uuid == code_object_uuid)
        {
            return &code_object;
        }
    }
    return nullptr;
}

std::unordered_map<uint64_t, IsaDataBuilder::SourceLocation>
IsaDataBuilder::BuildSourceLocations(const PcSamplingData& data)
{
    std::unordered_map<uint64_t, SourceLocation> source_locations;
    for(const InstructionSourceLine& dep : data.instruction_source_lines)
    {
        if(dep.frame_index == 0)
        {
            // Keep the first mapping per instruction_uuid; duplicates at frame_index==0
            // are unexpected but harmless — the first entry in the data is authoritative.
            source_locations.emplace(
                dep.instruction_uuid,
                SourceLocation{ dep.source_line_uuid, dep.source_file_uuid });
        }
    }
    return source_locations;
}

IsaDataBuilder::SampleAggregation
IsaDataBuilder::AggregateSampleCounts(const PcSamplingData& data)
{
    SampleAggregation aggregation;
    aggregation.counts_by_instruction.reserve(data.pc_sample_states.size());
    aggregation.metrics_by_instruction.reserve(data.pc_sample_states.size());
    aggregation.sample_state_by_uuid.reserve(data.pc_sample_states.size());
    for(const PcSampleState& state : data.pc_sample_states)
    {
        SampleCounts& counts =
            aggregation.counts_by_instruction
                .try_emplace(state.instruction_uuid, SampleCounts{ 0, 0, 0 })
                .first->second;
        AccumulateSampleCounts(
            counts, { state.total_count, state.issue_count, state.stall_count });
        AccumulateSampleMetrics(
            aggregation.metrics_by_instruction[state.instruction_uuid],
            { MakeSamplePercentage(state.total_count, state.wave_occupancy_percent),
              MakeSamplePercentage(state.total_count, state.active_thread_percent) });
        aggregation.sample_state_by_uuid.emplace(state.pc_sample_state_uuid, &state);
    }
    return aggregation;
}

bool
IsaDataBuilder::AddSampleCount(uint64_t& total, uint64_t count)
{
    if(count > UINT64_MAX - total)
    {
        total = UINT64_MAX;
        return false;
    }
    total += count;
    return true;
}

void
IsaDataBuilder::AccumulateSampleCounts(SampleCounts& total, const SampleCounts& counts)
{
    total.invalid_counts |=
        SummarizeSampling(counts).availability == SamplingAvailability::kInvalid;
    total.invalid_counts |= !AddSampleCount(total.total_count, counts.total_count);
    const auto add_progress = [&total](std::optional<uint64_t>&       accumulated,
                                       const std::optional<uint64_t>& value) {
        if(accumulated && value)
        {
            total.invalid_counts |= !AddSampleCount(*accumulated, *value);
        }
        else
        {
            accumulated.reset();
        }
    };
    add_progress(total.issue_count, counts.issue_count);
    add_progress(total.stall_count, counts.stall_count);
}

IsaDataBuilder::SamplePercentage
IsaDataBuilder::MakeSamplePercentage(uint64_t                     sample_count,
                                     const std::optional<double>& percent)
{
    SamplePercentage value;
    // States without observations cannot contribute or invalidate a measured mean.
    if(sample_count > 0)
    {
        value.sample_count = sample_count;
        value.missing      = !percent.has_value();
        if(percent)
        {
            value.invalid =
                !std::isfinite(*percent) || *percent < 0.0 || *percent > MAX_PERCENTAGE;
            if(!value.invalid)
            {
                value.weighted_sum = static_cast<double>(sample_count) * *percent;
            }
        }
    }
    return value;
}

void
IsaDataBuilder::AccumulateSamplePercentage(SamplePercentage&       total,
                                           const SamplePercentage& value)
{
    total.invalid |= value.invalid;
    total.invalid |= !AddSampleCount(total.sample_count, value.sample_count);
    total.missing |= value.missing;
    total.weighted_sum += value.weighted_sum;
}

void
IsaDataBuilder::AccumulateSampleMetrics(SampleMetrics& total, const SampleMetrics& value)
{
    AccumulateSamplePercentage(total.wave_occupancy, value.wave_occupancy);
    AccumulateSamplePercentage(total.active_threads, value.active_threads);
}

IsaDataBuilder::PercentageSummary
IsaDataBuilder::SummarizePercentage(const SamplePercentage& value)
{
    PercentageSummary summary;
    if(value.invalid)
    {
        summary.availability = SamplingAvailability::kInvalid;
    }
    else if(value.sample_count > 0 && !value.missing)
    {
        summary.availability = SamplingAvailability::kValid;
        // Only bound rounding error after every contributing measurement was validated.
        summary.percent =
            std::clamp(value.weighted_sum / static_cast<double>(value.sample_count), 0.0,
                       MAX_PERCENTAGE);
    }
    return summary;
}

std::unordered_map<uint64_t, std::string>
IsaDataBuilder::BuildStallReasonText(const PcSamplingData& data)
{
    std::unordered_map<uint64_t, std::string> text_by_lookup;
    text_by_lookup.reserve(data.pc_sample_stall_reason_lookups.size());
    for(const PcSampleStallReasonLookup& lookup : data.pc_sample_stall_reason_lookups)
    {
        text_by_lookup.emplace(lookup.pc_sample_stall_reason_lookup_uuid, lookup.text);
    }
    return text_by_lookup;
}

void
IsaDataBuilder::AddStallReasonCount(StallReasonCounts& counts, uint64_t lookup_uuid,
                                    uint64_t count)
{
    counts.overflow |= !AddSampleCount(counts.by_lookup[lookup_uuid], count);
    counts.overflow |= !AddSampleCount(counts.total_count, count);
}

void
IsaDataBuilder::MergeStallReasonCounts(StallReasonCounts&       total,
                                       const StallReasonCounts& counts)
{
    total.overflow |= counts.overflow;
    for(const auto& [lookup_uuid, count] : counts.by_lookup)
    {
        AddStallReasonCount(total, lookup_uuid, count);
    }
}

IsaDataBuilder::GroupedStallReasonCounts
IsaDataBuilder::GroupStallReasonCountsBySampleState(const PcSamplingData& data)
{
    GroupedStallReasonCounts counts_by_sample_state;
    counts_by_sample_state.reserve(data.pc_sample_states.size());
    for(const PcSampleStallReason& reason : data.pc_sample_stall_reasons)
    {
        AddStallReasonCount(counts_by_sample_state[reason.pc_sample_state_uuid],
                            reason.pc_sample_stall_reason_lookup_uuid, reason.count);
    }
    return counts_by_sample_state;
}

std::vector<uint64_t>
IsaDataBuilder::FindOtherWaitLookupUuids(
    const std::unordered_map<uint64_t, std::string>& reason_text)
{
    std::vector<uint64_t> lookup_uuids;
    for(const auto& [lookup_uuid, text] : reason_text)
    {
        if(ResolveStallReasonKind(text) == StallReasonKind::kOtherWait)
        {
            lookup_uuids.push_back(lookup_uuid);
        }
    }
    std::sort(lookup_uuids.begin(), lookup_uuids.end());
    return lookup_uuids;
}

void
IsaDataBuilder::RemoveIssuedSamplesFromOtherWait(
    const PcSampleState& state, const std::vector<uint64_t>& other_wait_lookup_uuids,
    StallReasonCounts& reason_counts)
{
    if(reason_counts.overflow || !state.issue_count || !state.stall_count ||
       other_wait_lookup_uuids.empty())
    {
        return;
    }

    const uint64_t issue_count = *state.issue_count;
    const bool     reasons_include_issued_samples =
        issue_count > 0 && issue_count <= state.total_count &&
        *state.stall_count == state.total_count - issue_count &&
        reason_counts.total_count == state.total_count;
    if(!reasons_include_issued_samples)
    {
        return;
    }

    uint64_t other_wait_count = 0;
    for(uint64_t lookup_uuid : other_wait_lookup_uuids)
    {
        const auto reason_it = reason_counts.by_lookup.find(lookup_uuid);
        if(reason_it != reason_counts.by_lookup.end())
        {
            if(!AddSampleCount(other_wait_count, reason_it->second))
            {
                reason_counts.overflow = true;
                return;
            }
        }
    }
    if(other_wait_count < issue_count)
    {
        return;
    }
    uint64_t remaining_issued = issue_count;
    for(uint64_t lookup_uuid : other_wait_lookup_uuids)
    {
        const auto reason_it = reason_counts.by_lookup.find(lookup_uuid);
        if(reason_it == reason_counts.by_lookup.end())
        {
            continue;
        }
        const uint64_t removed = std::min(reason_it->second, remaining_issued);
        reason_it->second -= removed;
        reason_counts.total_count -= removed;
        remaining_issued -= removed;
        if(reason_it->second == 0)
        {
            reason_counts.by_lookup.erase(reason_it);
        }
        if(remaining_issued == 0)
        {
            break;
        }
    }
}

IsaDataBuilder::GroupedStallReasonCounts
IsaDataBuilder::AggregateStallReasonCountsByInstruction(
    GroupedStallReasonCounts     counts_by_sample_state,
    const SampleAggregation&     sample_aggregation,
    const std::vector<uint64_t>& other_wait_lookup_uuids)
{
    GroupedStallReasonCounts counts_by_instruction;
    for(auto& [sample_state_uuid, reason_counts] : counts_by_sample_state)
    {
        const auto state_it =
            sample_aggregation.sample_state_by_uuid.find(sample_state_uuid);
        if(state_it == sample_aggregation.sample_state_by_uuid.end())
        {
            continue;
        }

        const PcSampleState& state = *state_it->second;
        if(!state.issue_count || !state.stall_count)
        {
            continue;
        }

        // Stochastic profiles store an issued sample's irrelevant stall reason as
        // OTHER_WAIT. Only normalize states whose totals prove that reasons include
        // issued samples, preserving profiles that already store stall-only reasons.
        RemoveIssuedSamplesFromOtherWait(state, other_wait_lookup_uuids, reason_counts);

        MergeStallReasonCounts(counts_by_instruction[state.instruction_uuid],
                               reason_counts);
    }
    return counts_by_instruction;
}

IsaDataBuilder::GroupedStallReasonCounts
IsaDataBuilder::BuildStallReasonCounts(
    const PcSamplingData& data, const SampleAggregation& sample_aggregation,
    const std::unordered_map<uint64_t, std::string>& reason_text)
{
    return AggregateStallReasonCountsByInstruction(
        GroupStallReasonCountsBySampleState(data), sample_aggregation,
        FindOtherWaitLookupUuids(reason_text));
}

std::string
IsaDataBuilder::ResolveStallReasonText(
    const std::unordered_map<uint64_t, std::string>& reason_text, uint64_t lookup_uuid)
{
    const auto text_it = reason_text.find(lookup_uuid);
    if(text_it != reason_text.end() && !text_it->second.empty())
    {
        return text_it->second;
    }
    return "Unknown stall reason (lookup ID " + std::to_string(lookup_uuid) + ")";
}

std::vector<IsaDataBuilder::StallReason>
IsaDataBuilder::BuildStallReasons(
    const StallReasonCounts&                         reason_counts,
    const std::unordered_map<uint64_t, std::string>& reason_text)
{
    std::vector<StallReason> stall_reasons;
    stall_reasons.reserve(reason_counts.by_lookup.size());
    for(const auto& [lookup_uuid, reason_count] : reason_counts.by_lookup)
    {
        StallReason reason;
        reason.text       = ResolveStallReasonText(reason_text, lookup_uuid);
        reason.count      = reason_count;
        reason.kind       = ResolveStallReasonKind(reason.text);
        reason.count_text = FormatSampleCount(reason.count);
        stall_reasons.push_back(std::move(reason));
    }
    std::sort(stall_reasons.begin(), stall_reasons.end(),
              [](const StallReason& lhs, const StallReason& rhs) {
                  if(lhs.count != rhs.count)
                  {
                      return lhs.count > rhs.count;
                  }
                  return lhs.text < rhs.text;
              });
    return stall_reasons;
}

void
IsaDataBuilder::AddIssueStateCount(IssueStateCounts& counts, uint64_t lookup_uuid,
                                   uint64_t count)
{
    counts.invalid |= !AddSampleCount(counts.by_lookup[lookup_uuid], count);
    counts.invalid |= !AddSampleCount(counts.total_count, count);
}

void
IsaDataBuilder::MergeIssueStateCounts(IssueStateCounts&       total,
                                      const IssueStateCounts& counts)
{
    total.missing |= counts.missing;
    total.invalid |= counts.invalid;
    total.invalid |= !AddSampleCount(total.sample_count, counts.sample_count);
    for(const auto& [lookup_uuid, count] : counts.by_lookup)
    {
        AddIssueStateCount(total, lookup_uuid, count);
    }
}

IsaDataBuilder::GroupedIssueStateCounts
IsaDataBuilder::BuildIssueStateCounts(const PcSamplingData&    data,
                                      const SampleAggregation& aggregation)
{
    GroupedIssueStateCounts counts_by_state;
    counts_by_state.reserve(aggregation.sample_state_by_uuid.size());
    for(const InstructionSample& sample : data.instruction_samples)
    {
        if(aggregation.sample_state_by_uuid.count(sample.pc_sample_state_uuid) == 0)
        {
            continue;
        }
        IssueStateCounts& counts = counts_by_state[sample.pc_sample_state_uuid];
        if(sample.count)
        {
            AddIssueStateCount(counts, sample.instruction_sample_lookup_uuid,
                               *sample.count);
        }
        else
        {
            counts.missing = true;
        }
    }
    GroupedIssueStateCounts counts_by_instruction;
    counts_by_instruction.reserve(aggregation.counts_by_instruction.size());
    for(const auto& [state_uuid, state] : aggregation.sample_state_by_uuid)
    {
        IssueStateCounts& counts = counts_by_state[state_uuid];
        counts.sample_count      = state->total_count;
        counts.invalid |= counts.total_count > state->total_count;
        // Validate before merging: an excess in one state cannot fill a gap in another.
        // Unrecorded zero-sample states do not invalidate observed distributions.
        counts.missing = state->total_count > 0 &&
                         (counts.missing || counts.total_count < state->total_count);
        MergeIssueStateCounts(counts_by_instruction[state->instruction_uuid], counts);
    }
    return counts_by_instruction;
}

IsaDataBuilder::IssueStateSummary
IsaDataBuilder::SummarizeIssueStates(
    const IssueStateCounts&                          counts,
    const std::unordered_map<uint64_t, std::string>& text_by_lookup)
{
    IssueStateSummary summary;
    if(counts.invalid)
    {
        summary.availability = SamplingAvailability::kInvalid;
    }
    else if(!counts.missing && !counts.by_lookup.empty())
    {
        summary.availability = SamplingAvailability::kValid;
        summary.total_count  = counts.total_count;
        summary.states.reserve(counts.by_lookup.size());
        for(const auto& [lookup_uuid, count] : counts.by_lookup)
        {
            IssueState state;
            state.lookup_uuid = lookup_uuid;
            const auto text   = text_by_lookup.find(lookup_uuid);
            if(text != text_by_lookup.end() && !text->second.empty())
            {
                state.text = text->second;
            }
            else
            {
                state.text = "Unknown issue state (lookup ID " +
                             std::to_string(lookup_uuid) + ")";
            }
            state.count       = count;
            state.count_text  = FormatSampleCount(count);
            state.percent     = CalculatePercentage(count, counts.sample_count);
            summary.states.push_back(std::move(state));
        }
        std::sort(summary.states.begin(), summary.states.end(),
                  [](const IssueState& lhs, const IssueState& rhs) {
                      if(lhs.count != rhs.count)
                      {
                          return lhs.count > rhs.count;
                      }
                      if(lhs.text != rhs.text)
                      {
                          return lhs.text < rhs.text;
                      }
                      return lhs.lookup_uuid < rhs.lookup_uuid;
                  });
        for(const IssueState& state : summary.states)
        {
            if(!summary.display_text.empty())
            {
                summary.display_text += "; ";
            }
            summary.display_text += state.text + ": " + state.count_text;
        }
    }
    return summary;
}

double
IsaDataBuilder::CalculatePercentage(uint64_t value, uint64_t total)
{
    return total > 0 ? static_cast<double>(value) / static_cast<double>(total) * 100.0
                     : 0.0;
}

std::string
IsaDataBuilder::FormatSampleCount(uint64_t value)
{
    const std::string digits = std::to_string(value);
    std::string       formatted_count;
    formatted_count.reserve(digits.size() + digits.size() / 3);

    int digits_since_separator = 0;
    for(auto it = digits.rbegin(); it != digits.rend(); ++it)
    {
        if(digits_since_separator == 3)
        {
            formatted_count.push_back(',');
            digits_since_separator = 0;
        }
        formatted_count.push_back(*it);
        ++digits_since_separator;
    }
    std::reverse(formatted_count.begin(), formatted_count.end());
    return formatted_count;
}

std::string
IsaDataBuilder::FormatSamplePercentage(const PercentageSummary& value)
{
    if(value.availability != SamplingAvailability::kValid)
    {
        return u8"\u2014";
    }
    constexpr size_t TEXT_CAPACITY       = 32;
    char             text[TEXT_CAPACITY] = {};
    std::snprintf(text, sizeof(text), "%.1f%%",
                  value.percent == 0.0 ? 0.0 : value.percent);
    return text;
}

IsaDataBuilder::SamplingSummary
IsaDataBuilder::SummarizeSampling(const SampleCounts& counts)
{
    SamplingSummary summary;
    if(counts.invalid_counts ||
       (counts.issue_count && *counts.issue_count > counts.total_count) ||
       (counts.stall_count && *counts.stall_count > counts.total_count) ||
       (counts.issue_count && counts.stall_count &&
        *counts.stall_count > counts.total_count - *counts.issue_count))
    {
        summary.availability = SamplingAvailability::kInvalid;
        return summary;
    }
    if(!counts.issue_count || !counts.stall_count)
    {
        return summary;
    }
    summary.availability  = SamplingAvailability::kValid;
    summary.stall_percent = CalculatePercentage(*counts.stall_count, counts.total_count);
    summary.issue_percent = CalculatePercentage(*counts.issue_count, counts.total_count);
    summary.unclassified_count =
        counts.total_count - *counts.issue_count - *counts.stall_count;
    return summary;
}

IsaDataBuilder::StallReasonKind
IsaDataBuilder::ResolveStallReasonKind(std::string_view text)
{
    constexpr std::string_view SDK_PREFIX =
        "ROCPROFILER_PC_SAMPLING_INSTRUCTION_NOT_ISSUED_REASON_";
    if(text.substr(0, SDK_PREFIX.size()) == SDK_PREFIX)
    {
        text.remove_prefix(SDK_PREFIX.size());
    }
    if(text == "OTHER")
    {
        return StallReasonKind::kOtherWait;
    }
    for(const StallReasonInfo& reason : STALL_REASONS)
    {
        if(text == reason.name)
        {
            return reason.kind;
        }
    }
    return StallReasonKind::kUnknown;
}

const char*
IsaDataBuilder::GetStallReasonName(StallReasonKind kind)
{
    for(const StallReasonInfo& reason : STALL_REASONS)
    {
        if(kind == reason.kind)
        {
            return reason.name;
        }
    }
    return "Unknown";
}

IsaDataBuilder::InstructionKind
IsaDataBuilder::ResolveInstructionKind(std::string_view text)
{
    std::string name(text);
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    for(const InstructionTypeInfo& type : INSTRUCTION_TYPES)
    {
        if(name == type.name)
        {
            return type.kind;
        }
    }
    return InstructionKind::kUnknown;
}

const char*
IsaDataBuilder::GetInstructionTypeName(InstructionKind kind)
{
    for(const InstructionTypeInfo& type : INSTRUCTION_TYPES)
    {
        if(type.kind == kind)
        {
            return type.name;
        }
    }
    return "Unknown / unavailable";
}

std::vector<IsaDataBuilder::IsaRow>
IsaDataBuilder::BuildInstructions(const PcSamplingData& data, uint64_t code_object_uuid)
{
    std::vector<IsaRow>    rows;
    const CodeObjectStore* code_object = FindCodeObject(data, code_object_uuid);
    if(!code_object)
    {
        return rows;
    }
    std::unordered_map<uint64_t, InstructionKind> instruction_kinds;
    instruction_kinds.reserve(data.instruction_type_lookup_map.size());
    for(const auto& [uuid, text] : data.instruction_type_lookup_map)
    {
        instruction_kinds.emplace(uuid, ResolveInstructionKind(text));
    }
    for(const KernelSymbol& kernel_symbol : code_object->kernel_symbols)
    {
        for(const InstructionLine& instruction : kernel_symbol.instruction_lines)
        {
            IsaRow row;
            row.instruction = instruction.instruction;
            if(instruction.instruction_type_uuid)
            {
                const auto kind =
                    instruction_kinds.find(*instruction.instruction_type_uuid);
                if(kind != instruction_kinds.end())
                {
                    row.instruction_kind = kind->second;
                }
            }
            row.instruction_type_name = GetInstructionTypeName(row.instruction_kind);
            if(row.instruction_kind == InstructionKind::kUnknown &&
               instruction.instruction_type_uuid)
            {
                const auto type = data.instruction_type_lookup_map.find(
                    *instruction.instruction_type_uuid);
                if(type != data.instruction_type_lookup_map.end() &&
                   !type->second.empty())
                {
                    row.instruction_type_name = type->second;
                }
            }
            constexpr const char* WHITESPACE = " \t\r\n\f\v";
            const size_t          begin = row.instruction.find_first_not_of(WHITESPACE);
            if(begin != std::string::npos)
            {
                const size_t end = row.instruction.find_first_of(WHITESPACE, begin);
                const size_t token_end =
                    end == std::string::npos ? row.instruction.size() : end;
                // Labels, directives, and standalone comments are not mnemonics.
                if(row.instruction[begin] != ';' && row.instruction[begin] != '#' &&
                   row.instruction[begin] != '.' && row.instruction[token_end - 1] != ':')
                {
                    row.mnemonic_begin = begin;
                    row.mnemonic_end   = token_end;
                }
            }
            row.id                 = instruction.instruction_uuid;
            row.code_object_offset = instruction.code_object_offset;
            rows.push_back(std::move(row));
        }
    }
    return rows;
}

void
IsaDataBuilder::UpdateSourceLocations(std::vector<IsaRow>&  rows,
                                      const PcSamplingData& data)
{
    const auto source_locations = BuildSourceLocations(data);
    for(IsaRow& row : rows)
    {
        row.source_line_id     = 0;
        row.source_file_id     = 0;
        const auto location_it = source_locations.find(row.id);
        if(location_it != source_locations.end())
        {
            row.source_line_id = location_it->second.source_line_id;
            row.source_file_id = location_it->second.source_file_id;
        }
    }
}

IsaDataBuilder::PreparedSamplingData
IsaDataBuilder::PrepareSampling(const PcSamplingData& data)
{
    SampleAggregation    aggregation = AggregateSampleCounts(data);
    PreparedSamplingData sampling;
    sampling.reason_text = BuildStallReasonText(data);
    sampling.reasons_by_instruction =
        BuildStallReasonCounts(data, aggregation, sampling.reason_text);
    if(data.instruction_samples_loaded && !data.instruction_samples.empty())
    {
        for(const InstructionSampleLookup& lookup : data.instruction_sample_lookups)
        {
            sampling.issue_state_text.emplace(lookup.instruction_sample_lookup_uuid,
                                              lookup.text);
        }
        sampling.issue_states_by_instruction = BuildIssueStateCounts(data, aggregation);
    }
    sampling.counts_by_instruction  = std::move(aggregation.counts_by_instruction);
    sampling.metrics_by_instruction = std::move(aggregation.metrics_by_instruction);
    return sampling;
}

uint64_t
IsaDataBuilder::UpdateSampling(std::vector<IsaRow>&        rows,
                               const PreparedSamplingData& sampling)
{
    const StallReasonCounts empty_reasons;
    const SampleMetrics     empty_metrics;
    const IssueStateCounts  empty_issues;
    uint64_t                hottest_instruction_samples = 0;
    for(IsaRow& row : rows)
    {
        const auto counts_it  = sampling.counts_by_instruction.find(row.id);
        const auto metrics_it = sampling.metrics_by_instruction.find(row.id);
        const auto reasons_it = sampling.reasons_by_instruction.find(row.id);
        const auto issues_it  = sampling.issue_states_by_instruction.find(row.id);
        UpdateSamplingRow(
            row,
            counts_it != sampling.counts_by_instruction.end() ? counts_it->second
                                                              : SampleCounts{},
            metrics_it != sampling.metrics_by_instruction.end() ? metrics_it->second
                                                                : empty_metrics,
            reasons_it != sampling.reasons_by_instruction.end() ? reasons_it->second
                                                                : empty_reasons,
            issues_it != sampling.issue_states_by_instruction.end() ? issues_it->second
                                                                    : empty_issues,
            sampling);
        if(row.sampling.availability != SamplingAvailability::kInvalid)
        {
            hottest_instruction_samples =
                std::max(hottest_instruction_samples, row.sample_counts.total_count);
        }
    }
    return hottest_instruction_samples;
}

void
IsaDataBuilder::UpdateSamplingRow(SamplingRow& row, const SampleCounts& counts,
                                  const SampleMetrics&        metrics,
                                  const StallReasonCounts&    reasons,
                                  const IssueStateCounts&     issues,
                                  const PreparedSamplingData& prepared)
{
    row.sample_counts  = counts;
    row.sampling       = SummarizeSampling(counts);
    row.wave_occupancy = SummarizePercentage(metrics.wave_occupancy);
    row.active_threads = SummarizePercentage(metrics.active_threads);
    row.issue_states   = SummarizeIssueStates(issues, prepared.issue_state_text);
    row.stall_reason_sample_count    = 0;
    row.stall_reason_counts_overflow = reasons.overflow;
    row.unclassified_stall_count     = 0;
    row.unclassified_stall_count_text.clear();
    row.stall_reasons.clear();
    if(!reasons.overflow)
    {
        row.stall_reason_sample_count = reasons.total_count;
        row.stall_reasons             = BuildStallReasons(reasons, prepared.reason_text);
    }
    if(row.sampling.availability == SamplingAvailability::kValid)
    {
        const uint64_t stalled   = *counts.stall_count;
        const uint64_t bar_total = std::max(stalled, row.stall_reason_sample_count);
        for(StallReason& reason : row.stall_reasons)
        {
            reason.bar_share = CalculatePercentage(reason.count, bar_total) / 100.0;
        }
        row.unclassified_stall_count = stalled > row.stall_reason_sample_count
                                           ? stalled - row.stall_reason_sample_count
                                           : 0;
        row.unclassified_stall_count_text =
            FormatSampleCount(row.unclassified_stall_count);
    }
}

std::vector<IsaDataBuilder::SourceRow>
IsaDataBuilder::BuildSourceLines(const PcSamplingData& data, uint64_t source_file_uuid)
{
    std::vector<SourceRow> rows;
    for(const SourceFile& file : data.source_files)
    {
        if(file.source_file_uuid != source_file_uuid)
        {
            continue;
        }
        rows.reserve(file.source_lines.size());
        for(const SourceLine& line : file.source_lines)
        {
            if(line.line_number == 0)
            {
                continue;
            }
            SourceRow row;
            row.id          = line.source_line_uuid;
            row.line_number = line.line_number;
            row.content     = line.content;
            rows.push_back(std::move(row));
        }
        break;
    }
    return rows;
}

uint64_t
IsaDataBuilder::UpdateSourceSampling(std::vector<SourceRow>&     rows,
                                     const PcSamplingData&       data,
                                     uint64_t                    source_file_uuid,
                                     const PreparedSamplingData& prepared)
{
    if(rows.empty())
    {
        return 0;
    }
    struct SourceSampling
    {
        SampleCounts      counts{ 0, 0, 0 };
        SampleMetrics     metrics;
        StallReasonCounts reasons;
        IssueStateCounts  issues;
        bool              has_samples = false;
    };
    const auto                                   locations = BuildSourceLocations(data);
    std::unordered_map<uint64_t, SourceSampling> sampling_by_line;
    for(const SourceRow& row : rows)
    {
        sampling_by_line.emplace(row.id, SourceSampling{});
    }
    for(const auto& [instruction_uuid, counts] : prepared.counts_by_instruction)
    {
        const auto location = locations.find(instruction_uuid);
        if(location == locations.end() ||
           location->second.source_file_id != source_file_uuid)
        {
            continue;
        }
        const auto line = sampling_by_line.find(location->second.source_line_id);
        if(line == sampling_by_line.end())
        {
            continue;
        }
        SourceSampling& sampling = line->second;
        sampling.has_samples     = true;
        AccumulateSampleCounts(sampling.counts, counts);
        const auto metrics = prepared.metrics_by_instruction.find(instruction_uuid);
        if(metrics != prepared.metrics_by_instruction.end())
        {
            AccumulateSampleMetrics(sampling.metrics, metrics->second);
        }
        const auto reasons = prepared.reasons_by_instruction.find(instruction_uuid);
        if(reasons != prepared.reasons_by_instruction.end())
        {
            MergeStallReasonCounts(sampling.reasons, reasons->second);
        }
        const auto issues = prepared.issue_states_by_instruction.find(instruction_uuid);
        if(issues != prepared.issue_states_by_instruction.end())
        {
            MergeIssueStateCounts(sampling.issues, issues->second);
        }
        else if(counts.total_count > 0)
        {
            sampling.issues.missing = true;
        }
    }
    uint64_t hottest_source_samples = 0;
    for(SourceRow& row : rows)
    {
        const SourceSampling& sampling = sampling_by_line.at(row.id);
        UpdateSamplingRow(row, sampling.has_samples ? sampling.counts : SampleCounts{},
                          sampling.metrics, sampling.reasons, sampling.issues, prepared);
        if(row.sampling.availability != SamplingAvailability::kInvalid)
        {
            hottest_source_samples =
                std::max(hottest_source_samples, row.sample_counts.total_count);
        }
    }
    return hottest_source_samples;
}

}  // namespace View
}  // namespace RocProfVis
