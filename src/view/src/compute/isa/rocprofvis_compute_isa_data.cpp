// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_isa_data.h"

#include <algorithm>
#include <utility>

namespace RocProfVis
{
namespace View
{

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
    aggregation.sample_state_by_uuid.reserve(data.pc_sample_states.size());
    for(const PcSampleState& state : data.pc_sample_states)
    {
        SampleCounts& counts =
            aggregation.counts_by_instruction
                .try_emplace(state.instruction_uuid, SampleCounts{ 0, 0, 0 })
                .first->second;
        AccumulateSampleCounts(
            counts, { state.total_count, state.issue_count, state.stall_count });
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

IsaDataBuilder::GroupedStallReasonCounts
IsaDataBuilder::GroupStallReasonCountsBySampleState(const PcSamplingData& data)
{
    GroupedStallReasonCounts counts_by_sample_state;
    counts_by_sample_state.reserve(data.pc_sample_states.size());
    for(const PcSampleStallReason& reason : data.pc_sample_stall_reasons)
    {
        counts_by_sample_state[reason.pc_sample_state_uuid]
                              [reason.pc_sample_stall_reason_lookup_uuid] += reason.count;
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

uint64_t
IsaDataBuilder::SumStallReasonCounts(const StallReasonCounts& reason_counts)
{
    uint64_t total = 0;
    for(const auto& [lookup_uuid, count] : reason_counts)
    {
        (void) lookup_uuid;
        total += count;
    }
    return total;
}

void
IsaDataBuilder::RemoveIssuedSamplesFromOtherWait(
    const PcSampleState& state, const std::vector<uint64_t>& other_wait_lookup_uuids,
    StallReasonCounts& reason_counts)
{
    if(!state.issue_count || !state.stall_count || other_wait_lookup_uuids.empty())
    {
        return;
    }

    const uint64_t issue_count = *state.issue_count;
    const bool     reasons_include_issued_samples =
        issue_count > 0 && issue_count <= state.total_count &&
        *state.stall_count == state.total_count - issue_count &&
        SumStallReasonCounts(reason_counts) == state.total_count;
    if(!reasons_include_issued_samples)
    {
        return;
    }

    uint64_t other_wait_count = 0;
    for(uint64_t lookup_uuid : other_wait_lookup_uuids)
    {
        const auto reason_it = reason_counts.find(lookup_uuid);
        if(reason_it != reason_counts.end())
        {
            other_wait_count += reason_it->second;
        }
    }
    if(other_wait_count < issue_count)
    {
        return;
    }
    uint64_t remaining_issued = issue_count;
    for(uint64_t lookup_uuid : other_wait_lookup_uuids)
    {
        const auto reason_it = reason_counts.find(lookup_uuid);
        if(reason_it == reason_counts.end())
        {
            continue;
        }
        const uint64_t removed = std::min(reason_it->second, remaining_issued);
        reason_it->second -= removed;
        remaining_issued -= removed;
        if(reason_it->second == 0)
        {
            reason_counts.erase(reason_it);
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

        for(const auto& [lookup_uuid, count] : reason_counts)
        {
            counts_by_instruction[state.instruction_uuid][lookup_uuid] += count;
        }
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
    const std::unordered_map<uint64_t, std::string>& reason_text,
    uint64_t&                                        classified_sample_count)
{
    std::vector<StallReason> stall_reasons;
    stall_reasons.reserve(reason_counts.size());
    for(const auto& [lookup_uuid, reason_count] : reason_counts)
    {
        StallReason reason;
        reason.text       = ResolveStallReasonText(reason_text, lookup_uuid);
        reason.count      = reason_count;
        reason.kind       = ResolveStallReasonKind(reason.text);
        reason.count_text = FormatSampleCount(reason.count);
        stall_reasons.push_back(std::move(reason));
        classified_sample_count += reason_count;
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
    constexpr std::string_view REASON_NAMES[] = {
        "NO_INSTRUCTION_AVAILABLE", "ALU_DEPENDENCY", "WAITCNT",
        "INTERNAL_INSTRUCTION",     "BARRIER_WAIT",   "ARBITER_NOT_WIN",
        "ARBITER_WIN_EX_STALL",     "OTHER_WAIT",     "SLEEP"
    };
    static_assert(sizeof(REASON_NAMES) / sizeof(REASON_NAMES[0]) ==
                  static_cast<uint32_t>(StallReasonKind::kUnknown));
    if(text == "OTHER")
    {
        return StallReasonKind::kOtherWait;
    }
    for(uint32_t i = 0; i < static_cast<uint32_t>(StallReasonKind::kUnknown); ++i)
    {
        if(text == REASON_NAMES[i])
        {
            return static_cast<StallReasonKind>(i);
        }
    }
    return StallReasonKind::kUnknown;
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
    for(const KernelSymbol& kernel_symbol : code_object->kernel_symbols)
    {
        for(const InstructionLine& instruction : kernel_symbol.instruction_lines)
        {
            IsaRow row;
            row.instruction        = instruction.instruction;
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
    sampling.counts_by_instruction = std::move(aggregation.counts_by_instruction);
    return sampling;
}

uint64_t
IsaDataBuilder::UpdateSampling(std::vector<IsaRow>&        rows,
                               const PreparedSamplingData& sampling)
{
    const StallReasonCounts empty_reasons;
    uint64_t                hottest_instruction_samples = 0;
    for(IsaRow& row : rows)
    {
        const auto counts_it  = sampling.counts_by_instruction.find(row.id);
        const auto reasons_it = sampling.reasons_by_instruction.find(row.id);
        UpdateSamplingRow(
            row,
            counts_it != sampling.counts_by_instruction.end() ? counts_it->second
                                                              : SampleCounts{},
            reasons_it != sampling.reasons_by_instruction.end() ? reasons_it->second
                                                                : empty_reasons,
            sampling.reason_text);
        hottest_instruction_samples =
            std::max(hottest_instruction_samples, row.sample_counts.total_count);
    }
    return hottest_instruction_samples;
}

void
IsaDataBuilder::UpdateSamplingRow(
    SamplingRow& row, const SampleCounts& counts, const StallReasonCounts& reasons,
    const std::unordered_map<uint64_t, std::string>& reason_text)
{
    row.sample_counts             = counts;
    row.sampling                  = SummarizeSampling(counts);
    row.stall_reason_sample_count = 0;
    row.unclassified_stall_count  = 0;
    row.unclassified_stall_count_text.clear();
    row.stall_reasons =
        BuildStallReasons(reasons, reason_text, row.stall_reason_sample_count);
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
        StallReasonCounts reasons;
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
        const auto reasons = prepared.reasons_by_instruction.find(instruction_uuid);
        if(reasons != prepared.reasons_by_instruction.end())
        {
            for(const auto& [lookup_uuid, count] : reasons->second)
            {
                sampling.reasons[lookup_uuid] += count;
            }
        }
    }
    uint64_t hottest_source_samples = 0;
    for(SourceRow& row : rows)
    {
        const SourceSampling& sampling = sampling_by_line.at(row.id);
        UpdateSamplingRow(row, sampling.has_samples ? sampling.counts : SampleCounts{},
                          sampling.reasons, prepared.reason_text);
        hottest_source_samples =
            std::max(hottest_source_samples, row.sample_counts.total_count);
    }
    return hottest_source_samples;
}

}  // namespace View
}  // namespace RocProfVis
