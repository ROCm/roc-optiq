// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "compute/isa/rocprofvis_compute_isa_data.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace RocProfVis
{
namespace View
{

static PcSamplingData
make_isa_data()
{
    PcSamplingData data;
    data.code_objects = {
        { 1, { { 1, 1, { { 10, 0x40, "s_nop 0" }, { 20, 0x44, "s_endpgm" } } } } }
    };
    return data;
}

TEST_CASE("ISA progress distinguishes missing, invalid, and genuine zero counts", "[isa]")
{
    using Availability = IsaDataBuilder::SamplingAvailability;
    struct Case
    {
        IsaDataBuilder::SampleCounts counts;
        Availability                 expected;
    };
    const Case cases[] = { { { 100, std::nullopt, std::nullopt },
                             Availability::kMissing },
                           { { 100, 0, std::nullopt }, Availability::kMissing },
                           { { 0, std::nullopt, std::nullopt }, Availability::kMissing },
                           { { 100, 80, 60 }, Availability::kInvalid },
                           { { 100, 101, 0 }, Availability::kInvalid },
                           { { 100, 0, 101 }, Availability::kInvalid },
                           { { 100, 101, std::nullopt }, Availability::kInvalid },
                           { { 100, std::nullopt, 101 }, Availability::kInvalid },
                           { { 0, 1, 0 }, Availability::kInvalid },
                           { { 0, 0, 1 }, Availability::kInvalid },
                           { { 100, 80, 20 }, Availability::kValid },
                           { { 100, 50, 20 }, Availability::kValid },
                           { { 0, 0, 0 }, Availability::kValid },
                           { { UINT64_MAX, UINT64_MAX, 0 }, Availability::kValid },
                           { { UINT64_MAX, UINT64_MAX, 1 }, Availability::kInvalid } };
    for(const Case& entry : cases)
    {
        INFO("total samples: " << entry.counts.total_count);
        const IsaDataBuilder::SamplingSummary summary =
            IsaDataBuilder::SummarizeSampling(entry.counts);
        CHECK(summary.availability == entry.expected);
        if(entry.expected != Availability::kValid)
        {
            CHECK(summary.stall_percent == 0.0);
            CHECK(summary.issue_percent == 0.0);
        }
    }
    const IsaDataBuilder::SamplingSummary summary =
        IsaDataBuilder::SummarizeSampling({ 100, 50, 20 });
    CHECK(summary.stall_percent == 20.0);
    CHECK(summary.issue_percent == 50.0);
    CHECK(summary.unclassified_count == 30);
}

TEST_CASE("ISA aggregation preserves NULL progress across sample states", "[isa]")
{
    PcSamplingData data                      = make_isa_data();
    data.pc_sample_states                    = { { 1, 10, 100, 80, 20 },
                                                 { 2, 10, 50, std::nullopt, 10 },
                                                 { 3, 20, 40, 0, 40 },
                                                 { 4, 99, 500, 500, 0 } };
    std::vector<IsaDataBuilder::IsaRow> rows = IsaDataBuilder::BuildInstructions(data, 1);
    const uint64_t                      hottest_instruction_samples =
        IsaDataBuilder::UpdateSampling(rows, IsaDataBuilder::PrepareSampling(data));
    REQUIRE(rows.size() == 2);
    CHECK(rows[0].sample_counts.total_count == 150);
    CHECK_FALSE(rows[0].sample_counts.issue_count.has_value());
    CHECK(rows[0].sample_counts.stall_count == 30);
    CHECK(rows[0].sampling.availability ==
          IsaDataBuilder::SamplingAvailability::kMissing);
    CHECK(rows[1].sampling.availability == IsaDataBuilder::SamplingAvailability::kValid);
    CHECK(rows[1].sampling.stall_percent == 100.0);
    CHECK(hottest_instruction_samples == 150);
    CHECK(IsaDataBuilder::BuildInstructions(data, 99).empty());
}

TEST_CASE("Issued observations are removed from OTHER_WAIT only when totals prove it",
          "[isa]")
{
    PcSamplingData data                 = make_isa_data();
    data.pc_sample_states               = { { 1, 10, 100, 80, 20 } };
    data.pc_sample_stall_reason_lookups = { { 1, "OTHER_WAIT" },
                                            { 2, "ALU_DEPENDENCY" } };
    data.pc_sample_stall_reasons        = { { 1, 1, 85 }, { 1, 2, 15 } };
    SECTION("Canonical OTHER_WAIT")
    {
        // Use the canonical data prepared above.
    }
    SECTION("SDK-prefixed OTHER alias")
    {
        data.pc_sample_stall_reason_lookups[0].text =
            "ROCPROFILER_PC_SAMPLING_INSTRUCTION_NOT_ISSUED_REASON_OTHER";
    }
    SECTION("Already stall-only reasons remain unchanged")
    {
        data.pc_sample_stall_reasons[0].count = 5;
    }
    SECTION("Issued observations can span equivalent lookup entries")
    {
        data.pc_sample_stall_reasons[0].count = 30;
        data.pc_sample_stall_reason_lookups.push_back({ 3, "OTHER" });
        data.pc_sample_stall_reasons.push_back({ 1, 3, 55 });
    }
    std::vector<IsaDataBuilder::IsaRow> rows = IsaDataBuilder::BuildInstructions(data, 1);
    IsaDataBuilder::UpdateSampling(rows, IsaDataBuilder::PrepareSampling(data));
    REQUIRE(rows[0].stall_reasons.size() == 2);
    CHECK(rows[0].stall_reason_sample_count == 20);
    CHECK(rows[0].stall_reasons[0].text == "ALU_DEPENDENCY");
    CHECK(rows[0].stall_reasons[0].count == 15);
    CHECK(rows[0].stall_reasons[1].kind == IsaDataBuilder::StallReasonKind::kOtherWait);
    CHECK(rows[0].stall_reasons[1].count == 5);
    CHECK(rows[0].unclassified_stall_count == 0);
}

TEST_CASE("Reason mismatches preserve measured stall percentage and bound bar shares",
          "[isa]")
{
    PcSamplingData data                 = make_isa_data();
    data.pc_sample_states               = { { 1, 10, 100, 80, 20 } };
    data.pc_sample_stall_reason_lookups = { { 1, "ALU_DEPENDENCY" }, { 2, "WAITCNT" } };
    data.pc_sample_stall_reasons        = { { 1, 1, 30 }, { 1, 2, 10 } };
    std::vector<IsaDataBuilder::IsaRow> rows = IsaDataBuilder::BuildInstructions(data, 1);
    IsaDataBuilder::UpdateSampling(rows, IsaDataBuilder::PrepareSampling(data));
    CHECK(rows[0].sampling.stall_percent == 20.0);
    CHECK(rows[0].stall_reason_sample_count == 40);
    CHECK(rows[0].stall_reasons[0].bar_share == Catch::Approx(0.75));
    CHECK(rows[0].stall_reasons[1].bar_share == Catch::Approx(0.25));
    CHECK(rows[0].unclassified_stall_count == 0);

    data.pc_sample_stall_reasons = { { 1, 1, 5 } };
    IsaDataBuilder::UpdateSampling(rows, IsaDataBuilder::PrepareSampling(data));
    REQUIRE(rows[0].stall_reasons.size() == 1);
    CHECK(rows[0].unclassified_stall_count == 15);
    CHECK(rows[0].unclassified_stall_count_text == "15");
    CHECK(rows[0].stall_reasons[0].bar_share == Catch::Approx(0.25));

    data.pc_sample_stall_reasons.clear();
    IsaDataBuilder::UpdateSampling(rows, IsaDataBuilder::PrepareSampling(data));
    CHECK(rows[0].stall_reasons.empty());
    CHECK(rows[0].unclassified_stall_count == 20);
}

TEST_CASE("Reason names, ordering, and formatted counts are prepared consistently",
          "[isa]")
{
    PcSamplingData data                 = make_isa_data();
    data.pc_sample_states               = { { 1, 10, 2469134, 0, 2469134 } };
    data.pc_sample_stall_reason_lookups = { { 1, "WAITCNT" }, { 2, "ALU_DEPENDENCY" } };
    data.pc_sample_stall_reasons        = { { 1, 1, 1234567 }, { 1, 2, 1234567 } };
    std::vector<IsaDataBuilder::IsaRow> rows = IsaDataBuilder::BuildInstructions(data, 1);
    IsaDataBuilder::UpdateSampling(rows, IsaDataBuilder::PrepareSampling(data));
    REQUIRE(rows[0].stall_reasons.size() == 2);
    CHECK(rows[0].stall_reasons[0].text == "ALU_DEPENDENCY");
    CHECK(rows[0].stall_reasons[1].text == "WAITCNT");
    CHECK(rows[0].stall_reasons[0].count_text == "1,234,567");
    CHECK(IsaDataBuilder::FormatSampleCount(0) == "0");
    CHECK(IsaDataBuilder::FormatSampleCount(UINT64_MAX) == "18,446,744,073,709,551,615");
    CHECK(IsaDataBuilder::ResolveStallReasonKind(
              "ROCPROFILER_PC_SAMPLING_INSTRUCTION_NOT_ISSUED_REASON_OTHER_WAIT") ==
          IsaDataBuilder::StallReasonKind::kOtherWait);

    data.pc_sample_stall_reasons = { { 1, 99, 7 } };
    IsaDataBuilder::UpdateSampling(rows, IsaDataBuilder::PrepareSampling(data));
    REQUIRE(rows[0].stall_reasons.size() == 1);
    CHECK(rows[0].stall_reasons[0].text == "Unknown stall reason (lookup ID 99)");
    CHECK(rows[0].stall_reasons[0].kind == IsaDataBuilder::StallReasonKind::kUnknown);
}

TEST_CASE("Layer updates keep instruction storage and unrelated row data intact", "[isa]")
{
    PcSamplingData data                      = make_isa_data();
    data.pc_sample_states                    = { { 1, 10, 100, 80, 20 } };
    data.instruction_source_lines            = { { 10, 101, 11, 1 },
                                                 { 10, 102, 12, 0 },
                                                 { 10, 103, 13, 0 } };
    std::vector<IsaDataBuilder::IsaRow> rows = IsaDataBuilder::BuildInstructions(data, 1);
    const IsaDataBuilder::IsaRow*       storage = rows.data();
    IsaDataBuilder::UpdateSourceLocations(rows, data);
    CHECK(rows[0].source_line_id == 102);
    CHECK(rows[0].source_file_id == 12);
    IsaDataBuilder::UpdateSampling(rows, IsaDataBuilder::PrepareSampling(data));
    CHECK(rows.data() == storage);
    CHECK(rows[0].source_line_id == 102);
    CHECK(rows[0].instruction == "s_nop 0");
    CHECK(rows[0].code_object_offset == 0x40);
    CHECK(rows[1].sampling.availability ==
          IsaDataBuilder::SamplingAvailability::kMissing);

    data.instruction_source_lines.clear();
    IsaDataBuilder::UpdateSourceLocations(rows, data);
    CHECK(rows.data() == storage);
    CHECK(rows[0].source_line_id == 0);
    CHECK(rows[0].sampling.stall_percent == 20.0);

    const uint64_t hottest_instruction_samples = IsaDataBuilder::UpdateSampling(rows, {});
    CHECK(rows.data() == storage);
    CHECK(rows[0].sample_counts.total_count == 0);
    CHECK(rows[0].stall_reasons.empty());
    CHECK(hottest_instruction_samples == 0);
}

static PcSamplingData
make_source_sampling_data()
{
    PcSamplingData data                 = make_isa_data();
    data.source_files                   = { { 11,
                                              "first.cpp",
                                              { { 101, 1, "first();" },
                                                { 102, 2, "second();" },
                                                { 103, 3, "// no samples" },
                                                { 104, 0, "unknown" } } },
                                            { 12, "other.cpp", { { 201, 1, "other();" } } } };
    data.instruction_source_lines       = { { 10, 102, 11, 1 }, { 10, 101, 11, 0 },
                                            { 10, 102, 11, 0 }, { 20, 101, 11, 0 },
                                            { 30, 102, 11, 0 }, { 40, 201, 12, 0 },
                                            { 50, 103, 11, 0 }, { 60, 101, 11, 0 } };
    data.pc_sample_states               = { { 1, 10, 100, 80, 20 },
                                            { 2, 20, 50, 10, 40 },
                                            { 3, 30, 75, 25, 50 },
                                            { 4, 40, 1000, 500, 500 },
                                            { 5, 999, 5000, 5000, 0 } };
    data.pc_sample_stall_reason_lookups = { { 1, "OTHER_WAIT" },
                                            { 2, "ALU_DEPENDENCY" },
                                            { 3, "WAITCNT" } };
    data.pc_sample_stall_reasons        = {
        { 1, 1, 85 }, { 1, 2, 15 }, { 2, 2, 30 }, { 2, 3, 10 }, { 3, 3, 50 }
    };
    return data;
}

TEST_CASE("Source sampling combines primary ISA mappings and normalizes before summing",
          "[isa][source]")
{
    PcSamplingData                         data = make_source_sampling_data();
    std::vector<IsaDataBuilder::SourceRow> rows =
        IsaDataBuilder::BuildSourceLines(data, 11);
    REQUIRE(rows.size() == 3);
    CHECK(rows[0].content == "first();");
    CHECK(rows[0].id == 101);
    CHECK(rows[0].line_number == 1);
    CHECK(IsaDataBuilder::BuildSourceLines(data, 99).empty());

    const uint64_t hottest = IsaDataBuilder::UpdateSourceSampling(
        rows, data, 11, IsaDataBuilder::PrepareSampling(data));
    CHECK(hottest == 150);
    CHECK(rows[0].sample_counts.total_count == 150);
    CHECK(rows[0].sample_counts.issue_count == 90);
    CHECK(rows[0].sample_counts.stall_count == 60);
    CHECK(rows[0].sampling.availability == IsaDataBuilder::SamplingAvailability::kValid);
    CHECK(rows[0].sampling.issue_percent == 60.0);
    CHECK(rows[0].sampling.stall_percent == 40.0);
    CHECK(rows[0].stall_reason_sample_count == 60);
    REQUIRE(rows[0].stall_reasons.size() == 3);
    CHECK(rows[0].stall_reasons[0].text == "ALU_DEPENDENCY");
    CHECK(rows[0].stall_reasons[0].count == 45);
    CHECK(rows[0].stall_reasons[0].count_text == "45");
    CHECK(rows[0].stall_reasons[0].bar_share == Catch::Approx(0.75));
    CHECK(rows[0].stall_reasons[1].text == "WAITCNT");
    CHECK(rows[0].stall_reasons[1].count == 10);
    CHECK(rows[0].stall_reasons[2].text == "OTHER_WAIT");
    CHECK(rows[0].stall_reasons[2].count == 5);
    CHECK(rows[1].sample_counts.total_count == 75);
    CHECK(rows[2].sample_counts.total_count == 0);
    CHECK(rows[2].sampling.availability ==
          IsaDataBuilder::SamplingAvailability::kMissing);

    // Another file, duplicate mappings, and instructions without mappings cannot inflate
    // the selected source file's totals or its hottest-line scale.
    auto other_rows = IsaDataBuilder::BuildSourceLines(data, 12);
    CHECK(IsaDataBuilder::UpdateSourceSampling(
              other_rows, data, 12, IsaDataBuilder::PrepareSampling(data)) == 1000);
    REQUIRE(other_rows.size() == 1);
    CHECK(other_rows[0].sample_counts.total_count == 1000);
}

TEST_CASE("Source sampling preserves missing counts and genuine zero observations",
          "[isa][source]")
{
    PcSamplingData data = make_source_sampling_data();
    SECTION("Missing issued counts") { data.pc_sample_states[1].issue_count.reset(); }
    SECTION("Missing stalled counts") { data.pc_sample_states[1].stall_count.reset(); }
    SECTION("Host-trap observations")
    {
        data.pc_sample_states[1].issue_count.reset();
        data.pc_sample_states[1].stall_count.reset();
    }
    data.pc_sample_states.push_back({ 6, 50, 0, 0, 0 });
    auto rows = IsaDataBuilder::BuildSourceLines(data, 11);
    IsaDataBuilder::UpdateSourceSampling(rows, data, 11,
                                         IsaDataBuilder::PrepareSampling(data));
    CHECK(rows[0].sample_counts.total_count == 150);
    CHECK(rows[0].sampling.availability ==
          IsaDataBuilder::SamplingAvailability::kMissing);
    CHECK(rows[0].sample_counts.issue_count.has_value() ==
          data.pc_sample_states[1].issue_count.has_value());
    CHECK(rows[0].sample_counts.stall_count.has_value() ==
          data.pc_sample_states[1].stall_count.has_value());
    CHECK(rows[2].sampling.availability == IsaDataBuilder::SamplingAvailability::kValid);
    CHECK(rows[2].sampling.issue_percent == 0.0);
    CHECK(rows[2].sampling.stall_percent == 0.0);
}

TEST_CASE("Source aggregation cannot mask malformed instruction counts or overflow",
          "[isa][source]")
{
    PcSamplingData data   = make_source_sampling_data();
    data.pc_sample_states = { { 1, 10, 100, 80, 60 }, { 2, 20, 100, 0, 0 } };
    SECTION("A valid aggregate does not repair an invalid contributor")
    {
        // Aggregate counts (200/80/60) pass the arithmetic check on their own.
    }
    SECTION("Counts cannot wrap while summing source lines")
    {
        data.pc_sample_states = { { 1, 10, UINT64_MAX, UINT64_MAX, 0 },
                                  { 2, 20, 1, 0, 0 } };
    }
    auto           rows    = IsaDataBuilder::BuildSourceLines(data, 11);
    const uint64_t hottest = IsaDataBuilder::UpdateSourceSampling(
        rows, data, 11, IsaDataBuilder::PrepareSampling(data));
    CHECK(hottest >= 200);
    CHECK(rows[0].sampling.availability ==
          IsaDataBuilder::SamplingAvailability::kInvalid);
    CHECK(rows[0].sampling.issue_percent == 0.0);
    CHECK(rows[0].sampling.stall_percent == 0.0);
    for(const IsaDataBuilder::StallReason& reason : rows[0].stall_reasons)
    {
        CHECK(reason.bar_share == 0.0);
    }
}

TEST_CASE("Source sampling refreshes cached lines when sampling arrives or clears",
          "[isa][source]")
{
    PcSamplingData                   data    = make_source_sampling_data();
    auto                             rows    = IsaDataBuilder::BuildSourceLines(data, 11);
    const IsaDataBuilder::SourceRow* storage = rows.data();
    IsaDataBuilder::UpdateSourceSampling(rows, data, 11,
                                         IsaDataBuilder::PrepareSampling(data));
    data.pc_sample_states.push_back({ 6, 10, 20, 20, 0 });
    CHECK(IsaDataBuilder::UpdateSourceSampling(
              rows, data, 11, IsaDataBuilder::PrepareSampling(data)) == 170);
    CHECK(rows.data() == storage);
    CHECK(rows[0].sample_counts.issue_count == 110);
    CHECK(rows[0].sample_counts.stall_count == 60);
    CHECK(rows[0].content == "first();");
    CHECK(rows[0].line_number == 1);
    CHECK(IsaDataBuilder::UpdateSourceSampling(rows, {}, 11, {}) == 0);
    CHECK(rows.data() == storage);
    CHECK(rows[0].sample_counts.total_count == 0);
    CHECK_FALSE(rows[0].sample_counts.issue_count.has_value());
    CHECK(rows[0].stall_reasons.empty());
    CHECK(rows[0].stall_reason_sample_count == 0);
    CHECK(rows[0].unclassified_stall_count == 0);
    CHECK(rows[0].sampling.availability ==
          IsaDataBuilder::SamplingAvailability::kMissing);
}

TEST_CASE("Malformed sample states remain invalid after instruction and source grouping",
          "[isa][source]")
{
    PcSamplingData data   = make_source_sampling_data();
    data.pc_sample_states = { { 1, 10, 100, 80, 60 }, { 2, 10, 100, 0, 0 } };
    SECTION("Aggregate arithmetic alone looks valid")
    {
        // 200 total, 80 issued, and 60 stalled hides the malformed first state.
    }
    SECTION("Missing progress cannot discard an earlier invalid contributor")
    {
        data.pc_sample_states[1].issue_count.reset();
    }
    SECTION("Contributor order does not change validity")
    {
        const PcSampleState first = data.pc_sample_states[0];
        data.pc_sample_states[0]  = data.pc_sample_states[1];
        data.pc_sample_states[1]  = first;
    }
    const auto prepared = IsaDataBuilder::PrepareSampling(data);
    auto       isa      = IsaDataBuilder::BuildInstructions(data, 1);
    auto       source   = IsaDataBuilder::BuildSourceLines(data, 11);
    IsaDataBuilder::UpdateSampling(isa, prepared);
    IsaDataBuilder::UpdateSourceSampling(source, data, 11, prepared);
    for(const IsaDataBuilder::SamplingRow* row :
        { static_cast<const IsaDataBuilder::SamplingRow*>(&isa[0]),
          static_cast<const IsaDataBuilder::SamplingRow*>(&source[0]) })
    {
        CHECK(row->sample_counts.total_count == 200);
        CHECK(row->sample_counts.stall_count == 60);
        CHECK(row->sample_counts.invalid_counts);
        CHECK(row->sampling.availability ==
              IsaDataBuilder::SamplingAvailability::kInvalid);
        CHECK(row->sampling.issue_percent == 0.0);
        CHECK(row->sampling.stall_percent == 0.0);
        for(const IsaDataBuilder::StallReason& reason : row->stall_reasons)
        {
            CHECK(reason.bar_share == 0.0);
        }
    }
}

TEST_CASE("Instruction count overflow is bounded and stays invalid in source totals",
          "[isa][source]")
{
    PcSamplingData data   = make_source_sampling_data();
    data.pc_sample_states = { { 1, 10, UINT64_MAX, 0, 0 }, { 2, 10, 1, 0, 0 } };
    SECTION("Total overflow")
    {
        // Use the total-only overflow inputs prepared above.
    }
    SECTION("Issued overflow")
    {
        data.pc_sample_states[0].issue_count = UINT64_MAX;
        data.pc_sample_states[1].issue_count = 1;
    }
    SECTION("Stalled overflow")
    {
        data.pc_sample_states[0].stall_count = UINT64_MAX;
        data.pc_sample_states[1].stall_count = 1;
    }
    const auto prepared = IsaDataBuilder::PrepareSampling(data);
    auto       isa      = IsaDataBuilder::BuildInstructions(data, 1);
    auto       source   = IsaDataBuilder::BuildSourceLines(data, 11);
    CHECK(IsaDataBuilder::UpdateSampling(isa, prepared) == UINT64_MAX);
    CHECK(IsaDataBuilder::UpdateSourceSampling(source, data, 11, prepared) == UINT64_MAX);
    CHECK(isa[0].sample_counts.total_count == UINT64_MAX);
    CHECK(isa[0].sampling.availability == IsaDataBuilder::SamplingAvailability::kInvalid);
    CHECK(source[0].sampling.availability == isa[0].sampling.availability);
}

TEST_CASE(
    "Prepared sampling is owned and reusable across panes and fresh source mappings",
    "[isa][source]")
{
    PcSamplingData data     = make_source_sampling_data();
    const auto     prepared = IsaDataBuilder::PrepareSampling(data);
    auto           isa      = IsaDataBuilder::BuildInstructions(data, 1);
    auto           source   = IsaDataBuilder::BuildSourceLines(data, 11);
    auto           other    = IsaDataBuilder::BuildSourceLines(data, 12);

    // The prepared data owns its values and normalized reason names, with no pointers
    // into the sample-state or reason vectors that later layer updates can replace.
    data.pc_sample_states.clear();
    data.pc_sample_stall_reasons.clear();
    data.pc_sample_stall_reason_lookups.clear();
    CHECK(IsaDataBuilder::UpdateSampling(isa, prepared) == 100);
    CHECK(IsaDataBuilder::UpdateSourceSampling(source, data, 11, prepared) == 150);
    CHECK(IsaDataBuilder::UpdateSourceSampling(other, data, 12, prepared) == 1000);
    REQUIRE(isa[0].stall_reasons.size() == 2);
    CHECK(isa[0].stall_reasons[1].text == "OTHER_WAIT");
    CHECK(isa[0].stall_reasons[1].count == 5);
    CHECK(source[0].stall_reason_sample_count == 60);
    IsaDataBuilder::UpdateSampling(isa, prepared);
    IsaDataBuilder::UpdateSourceSampling(source, data, 11, prepared);
    CHECK(isa[0].stall_reasons[1].count == 5);
    CHECK(source[0].stall_reason_sample_count == 60);

    // Correlations may arrive after sampling; they must use the cached samples with
    // their current source mappings rather than mappings frozen in the prepared data.
    data.instruction_source_lines = { { 10, 101, 11, 0 }, { 20, 102, 11, 0 } };
    CHECK(IsaDataBuilder::UpdateSourceSampling(source, data, 11, prepared) == 100);
    CHECK(source[0].sample_counts.total_count == 100);
    CHECK(source[1].sample_counts.total_count == 50);
    CHECK(IsaDataBuilder::UpdateSampling(isa, {}) == 0);
    CHECK(IsaDataBuilder::UpdateSourceSampling(source, data, 11, {}) == 0);
    CHECK(source[0].sampling.availability ==
          IsaDataBuilder::SamplingAvailability::kMissing);
}

}  // namespace View
}  // namespace RocProfVis
