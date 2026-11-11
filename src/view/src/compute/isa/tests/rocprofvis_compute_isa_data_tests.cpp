// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "compute/isa/rocprofvis_compute_isa_data.h"

#include <algorithm>
#include <limits>

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
        { 1,
          { { 1, 1, { { 10, std::nullopt, 0x40, "s_nop 0" },
                      { 20, std::nullopt, 0x44, "s_endpgm" } } } } }
    };
    return data;
}

TEST_CASE("ISA mnemonic ranges preserve operands, spacing, and comments", "[isa]")
{
    struct Case
    {
        const char* instruction;
        const char* mnemonic;
    };
    const Case cases[] = { { "v_add_f32 v0, v1, v2", "v_add_f32" },
                           { "  v_add_f32_e32\tv0, v1, v2 ; keep ## and %",
                             "v_add_f32_e32" },
                           { "\ts_waitcnt vmcnt(0) lgkmcnt(0)", "s_waitcnt" },
                           { "s_endpgm", "s_endpgm" },
                           { "s_nop 0 ; v_add_f32 is a comment", "s_nop" },
                           { "", "" },
                           { " \t", "" },
                           { "; standalone comment", "" },
                           { "# standalone comment", "" },
                           { ".LBB0_1:", "" },
                           { "label:", "" },
                           { ".long 0x1234", "" } };
    for(const Case& entry : cases)
    {
        INFO(entry.instruction);
        PcSamplingData data = make_isa_data();
        data.code_objects[0].kernel_symbols[0].instruction_lines[0].instruction =
            entry.instruction;
        const std::vector<IsaDataBuilder::IsaRow> rows =
            IsaDataBuilder::BuildInstructions(data, 1);
        REQUIRE(rows.size() == 2);
        const IsaDataBuilder::IsaRow& row = rows[0];
        CHECK(row.instruction == entry.instruction);
        REQUIRE(row.mnemonic_begin <= row.mnemonic_end);
        REQUIRE(row.mnemonic_end <= row.instruction.size());
        CHECK(row.instruction.substr(row.mnemonic_begin,
                                     row.mnemonic_end - row.mnemonic_begin) ==
              entry.mnemonic);
    }
}

TEST_CASE("ISA instruction colours follow type names independently of database UUIDs",
          "[isa]")
{
    using Kind = IsaDataBuilder::InstructionKind;
    struct Case
    {
        uint64_t    uuid;
        const char* text;
        Kind        kind;
        const char* name;
    };
    const Case cases[] = {
        { 1, "INTERNAL", Kind::kInternal, "INTERNAL" },
        { 2, "MATRIX", Kind::kMatrix, "MATRIX" },
        { 3, "SCALAR", Kind::kScalar, "SCALAR" },
        { 4, "BRANCH", Kind::kBranch, "BRANCH" },
        { 5, "VALU", Kind::kValu, "VALU" },
        { 6, "FLAT", Kind::kFlat, "FLAT" },
        { 6, "LDS", Kind::kLds, "LDS" },
        { 7, "BARRIER", Kind::kBarrier, "BARRIER" },
        { 6, "lds", Kind::kLds, "LDS" },
        { 7, "barrier", Kind::kBarrier, "BARRIER" },
        { 3, "scalar", Kind::kScalar, "SCALAR" },
        { 4, "branch", Kind::kBranch, "BRANCH" },
        { 7, "FUTURE_INSTRUCTION_TYPE", Kind::kUnknown, "FUTURE_INSTRUCTION_TYPE" },
        { 8, "NO_INST", Kind::kUnknown, "NO_INST" },
        { 9, "UNRECOGNIZED_TYPE", Kind::kUnknown, "UNRECOGNIZED_TYPE" },
        { 0, "", Kind::kUnknown, "Unknown / unavailable" }
    };
    for(const Case& entry : cases)
    {
        INFO(entry.text);
        PcSamplingData                data = make_isa_data();
        std::vector<InstructionLine>& instructions =
            data.code_objects[0].kernel_symbols[0].instruction_lines;
        instructions[0].instruction_type_uuid = entry.uuid;
        instructions[1].instruction_type_uuid = 901;
        data.instruction_type_lookup_map      = { { entry.uuid, entry.text },
                                                  { 901, entry.text } };
        const std::vector<IsaDataBuilder::IsaRow> rows =
            IsaDataBuilder::BuildInstructions(data, 1);
        REQUIRE(rows.size() == 2);
        CHECK(rows[0].instruction_kind == entry.kind);
        CHECK(rows[1].instruction_kind == entry.kind);
        CHECK(rows[0].instruction_type_name == entry.name);
        CHECK(rows[1].instruction_type_name == entry.name);
        CHECK(rows[0].code_object_offset == 0x40);
        data.instruction_type_lookup_map.clear();
        const std::vector<IsaDataBuilder::IsaRow> missing =
            IsaDataBuilder::BuildInstructions(data, 1);
        CHECK(missing[0].instruction_kind == Kind::kUnknown);
        CHECK(missing[1].instruction_kind == Kind::kUnknown);
        CHECK(missing[0].instruction_type_name == "Unknown / unavailable");
        CHECK(missing[1].instruction_type_name == "Unknown / unavailable");
    }
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

TEST_CASE("Canonical stall reasons support SDK aliases and preserve unknown names",
          "[isa]")
{
    using Kind                   = IsaDataBuilder::StallReasonKind;
    const Kind  expected_kinds[] = { Kind::kInstructionFetch, Kind::kAluDependency,
                                     Kind::kWaitcnt,          Kind::kInternalInstruction,
                                     Kind::kBarrier,          Kind::kArbiter,
                                     Kind::kPipelineStall,    Kind::kOtherWait,
                                     Kind::kSleeping };
    const char* expected_names[] = {
        "NO_INSTRUCTION_AVAILABLE", "ALU_DEPENDENCY", "WAITCNT",
        "INTERNAL_INSTRUCTION",     "BARRIER_WAIT",   "ARBITER_NOT_WIN",
        "ARBITER_WIN_EX_STALL",     "OTHER_WAIT",     "SLEEP"
    };
    for(size_t i = 0; i < sizeof(expected_kinds) / sizeof(expected_kinds[0]); ++i)
    {
        const std::string name = expected_names[i];
        CHECK(IsaDataBuilder::GetStallReasonName(expected_kinds[i]) == name);
        CHECK(IsaDataBuilder::ResolveStallReasonKind(name) == expected_kinds[i]);
        CHECK(IsaDataBuilder::ResolveStallReasonKind(
                  "ROCPROFILER_PC_SAMPLING_INSTRUCTION_NOT_ISSUED_REASON_" + name) ==
              expected_kinds[i]);
    }
    CHECK(IsaDataBuilder::ResolveStallReasonKind("OTHER") == Kind::kOtherWait);
    CHECK(IsaDataBuilder::ResolveStallReasonKind(
              "ROCPROFILER_PC_SAMPLING_INSTRUCTION_NOT_ISSUED_REASON_OTHER") ==
          Kind::kOtherWait);
    CHECK(IsaDataBuilder::ResolveStallReasonKind("") == Kind::kUnknown);
    CHECK(IsaDataBuilder::ResolveStallReasonKind("CUSTOM_REASON") == Kind::kUnknown);
    CHECK(IsaDataBuilder::GetStallReasonName(Kind::kUnknown) == std::string("Unknown"));
    CHECK(IsaDataBuilder::GetStallReasonName(static_cast<Kind>(UINT32_MAX)) ==
          std::string("Unknown"));
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

TEST_CASE("Percentage display preserves zero and identifies unavailable measurements",
          "[isa][sample-metrics]")
{
    using Availability = IsaDataBuilder::SamplingAvailability;
    struct Case
    {
        IsaDataBuilder::PercentageSummary value;
        const char*                       expected;
    };
    const Case cases[] = { { { Availability::kValid, 49.401407527783604 }, "49.4%" },
                           { { Availability::kValid, 24.710678590215675 }, "24.7%" },
                           { { Availability::kValid, 43.75 }, "43.8%" },
                           { { Availability::kValid, 100.0 }, "100.0%" },
                           { { Availability::kValid, 0.0 }, "0.0%" },
                           { { Availability::kValid, -0.0 }, "0.0%" },
                           { { Availability::kMissing, 0.0 }, u8"\u2014" },
                           { { Availability::kMissing, 49.4 }, u8"\u2014" },
                           { { Availability::kInvalid,
                               std::numeric_limits<double>::quiet_NaN() },
                             u8"\u2014" } };
    for(const Case& entry : cases)
    {
        CHECK(IsaDataBuilder::FormatSamplePercentage(entry.value) == entry.expected);
    }
}

TEST_CASE("Sampled percentages validate each metric independently of progress counts",
          "[isa][sample-metrics]")
{
    using Availability = IsaDataBuilder::SamplingAvailability;
    struct Case
    {
        std::optional<double> percent;
        Availability          expected;
    };
    const Case cases[] = {
        { std::nullopt, Availability::kMissing },
        { 0.0, Availability::kValid },
        { 49.401407527783604, Availability::kValid },
        { 100.0, Availability::kValid },
        { -0.1, Availability::kInvalid },
        { 100.1, Availability::kInvalid },
        { std::numeric_limits<double>::quiet_NaN(), Availability::kInvalid },
        { std::numeric_limits<double>::infinity(), Availability::kInvalid },
        { -std::numeric_limits<double>::infinity(), Availability::kInvalid }
    };
    for(const Case& entry : cases)
    {
        PcSamplingData data   = make_isa_data();
        data.pc_sample_states = {
            { 1, 10, 100, std::nullopt, std::nullopt, entry.percent, 50.0 },
            { 2, 20, 100, std::nullopt, std::nullopt, 50.0, entry.percent }
        };
        auto rows = IsaDataBuilder::BuildInstructions(data, 1);
        IsaDataBuilder::UpdateSampling(rows, IsaDataBuilder::PrepareSampling(data));
        CHECK(rows[0].sampling.availability == Availability::kMissing);
        CHECK(rows[0].wave_occupancy.availability == entry.expected);
        CHECK(rows[1].active_threads.availability == entry.expected);
        CHECK(rows[0].active_threads.availability == Availability::kValid);
        CHECK(rows[1].wave_occupancy.availability == Availability::kValid);
        CHECK(rows[0].active_threads.percent == 50.0);
        CHECK(rows[1].wave_occupancy.percent == 50.0);
        if(entry.expected == Availability::kValid)
        {
            CHECK(rows[0].wave_occupancy.percent == Catch::Approx(*entry.percent));
            CHECK(rows[1].active_threads.percent == Catch::Approx(*entry.percent));
        }
        else
        {
            CHECK(rows[0].wave_occupancy.percent == 0.0);
            CHECK(rows[1].active_threads.percent == 0.0);
        }
    }
}

TEST_CASE("Reported hotspot percentages retain database precision",
          "[isa][sample-metrics]")
{
    PcSamplingData data   = make_isa_data();
    data.pc_sample_states = { { 1, 10, 9538, 2012, 7526, 49.401407527783604, 100.0 },
                              { 2, 20, 1901, 252, 1649, 24.710678590215675, 100.0 } };
    auto rows             = IsaDataBuilder::BuildInstructions(data, 1);
    IsaDataBuilder::UpdateSampling(rows, IsaDataBuilder::PrepareSampling(data));
    CHECK(rows[0].wave_occupancy.percent == Catch::Approx(49.401407527783604));
    CHECK(rows[1].wave_occupancy.percent == Catch::Approx(24.710678590215675));
    CHECK(rows[0].active_threads.percent == 100.0);
    CHECK(rows[1].active_threads.percent == 100.0);
    CHECK(rows[0].sample_counts.total_count == 9538);
    CHECK(rows[0].sample_counts.issue_count == 2012);
    CHECK(rows[0].sample_counts.stall_count == 7526);
}

TEST_CASE("ISA and source percentages use sample weights through primary mappings",
          "[isa][source][sample-metrics]")
{
    using Availability    = IsaDataBuilder::SamplingAvailability;
    PcSamplingData data   = make_source_sampling_data();
    data.pc_sample_states = {
        { 1, 10, 100, 0, 100, 40.0, 100.0 },  { 2, 10, 300, 300, 0, 80.0, 0.0 },
        { 3, 20, 100, 100, 0, 10.0, 60.0 },   { 4, 30, 75, 0, 75, 20.0, 100.0 },
        { 5, 40, 1000, 0, 1000, 90.0, 10.0 }, { 6, 999, 5000, 5000, 0, 100.0, 100.0 }
    };
    SECTION("Valid progress counts") {}
    SECTION("Malformed progress does not invalidate independent percentages")
    {
        data.pc_sample_states[0].issue_count = 101;
    }
    const auto prepared = IsaDataBuilder::PrepareSampling(data);
    auto       isa_rows = IsaDataBuilder::BuildInstructions(data, 1);
    IsaDataBuilder::UpdateSampling(isa_rows, prepared);
    CHECK(isa_rows[0].wave_occupancy.availability == Availability::kValid);
    CHECK(isa_rows[0].wave_occupancy.percent == Catch::Approx(70.0));
    CHECK(isa_rows[0].active_threads.percent == Catch::Approx(25.0));

    // The snapshot owns its values, and source projection needs no ISA text.
    data.pc_sample_states.clear();
    data.code_objects.clear();
    auto rows = IsaDataBuilder::BuildSourceLines(data, 11);
    IsaDataBuilder::UpdateSourceSampling(rows, data, 11, prepared);
    CHECK(rows[0].wave_occupancy.availability == Availability::kValid);
    CHECK(rows[0].active_threads.availability == Availability::kValid);
    CHECK(rows[0].wave_occupancy.percent == Catch::Approx(58.0));
    CHECK(rows[0].active_threads.percent == Catch::Approx(32.0));
    CHECK(rows[1].wave_occupancy.percent == Catch::Approx(20.0));
    CHECK(rows[2].wave_occupancy.availability == Availability::kMissing);
    CHECK(rows[2].active_threads.availability == Availability::kMissing);
    auto other_rows = IsaDataBuilder::BuildSourceLines(data, 12);
    IsaDataBuilder::UpdateSourceSampling(other_rows, data, 12, prepared);
    CHECK(other_rows[0].wave_occupancy.percent == Catch::Approx(90.0));
    CHECK(other_rows[0].active_threads.percent == Catch::Approx(10.0));
}

TEST_CASE("Missing and invalid percentages propagate without masking other metrics",
          "[isa][source][sample-metrics]")
{
    using Availability                    = IsaDataBuilder::SamplingAvailability;
    PcSamplingData data                   = make_source_sampling_data();
    data.pc_sample_states                 = { { 1, 10, 100, 100, 0, 40.0, 100.0 },
                                              { 2, 10, 200, 200, 0, std::nullopt, 0.0 },
                                              { 3, 20, 100, 100, 0, 80.0, 100.0 } };
    Availability instruction_availability = Availability::kMissing;
    Availability source_availability      = Availability::kMissing;
    SECTION("Missing measurement with nonzero samples") {}
    SECTION("An invalid contributor remains invalid after averaging")
    {
        data.pc_sample_states[1].wave_occupancy_percent = -1.0;
        instruction_availability                        = Availability::kInvalid;
        source_availability                             = Availability::kInvalid;
    }
    SECTION("An invalid instruction takes precedence over a missing measurement")
    {
        data.pc_sample_states[2].wave_occupancy_percent =
            std::numeric_limits<double>::quiet_NaN();
        source_availability = Availability::kInvalid;
    }
    const auto prepared = IsaDataBuilder::PrepareSampling(data);
    auto       isa_rows = IsaDataBuilder::BuildInstructions(data, 1);
    IsaDataBuilder::UpdateSampling(isa_rows, prepared);
    CHECK(isa_rows[0].wave_occupancy.availability == instruction_availability);
    CHECK(isa_rows[0].active_threads.availability == Availability::kValid);
    CHECK(isa_rows[0].active_threads.percent == Catch::Approx(100.0 / 3.0));
    auto rows = IsaDataBuilder::BuildSourceLines(data, 11);
    IsaDataBuilder::UpdateSourceSampling(rows, data, 11, prepared);
    CHECK(rows[0].wave_occupancy.availability == source_availability);
    CHECK(rows[0].active_threads.availability == Availability::kValid);
    CHECK(rows[0].active_threads.percent == Catch::Approx(50.0));
    CHECK(rows[0].sampling.availability == Availability::kValid);
    CHECK(rows[0].sample_counts.total_count == 400);
}

TEST_CASE("Zero-sample states do not contribute to sampled percentages",
          "[isa][source][sample-metrics]")
{
    using Availability    = IsaDataBuilder::SamplingAvailability;
    PcSamplingData data   = make_source_sampling_data();
    data.pc_sample_states = { { 1, 10, 100, 100, 0, 40.0, 50.0 },
                              { 2, 10, 0, 0, 0, std::nullopt, std::nullopt },
                              { 3, 10, 0, 0, 0, -1.0,
                                std::numeric_limits<double>::quiet_NaN() },
                              { 4, 20, 0, 0, 0, 100.0, 0.0 } };
    const auto prepared   = IsaDataBuilder::PrepareSampling(data);
    auto       isa_rows   = IsaDataBuilder::BuildInstructions(data, 1);
    IsaDataBuilder::UpdateSampling(isa_rows, prepared);
    CHECK(isa_rows[0].wave_occupancy.availability == Availability::kValid);
    CHECK(isa_rows[0].wave_occupancy.percent == 40.0);
    CHECK(isa_rows[0].active_threads.availability == Availability::kValid);
    CHECK(isa_rows[0].active_threads.percent == 50.0);
    CHECK(isa_rows[1].wave_occupancy.availability == Availability::kMissing);
    CHECK(isa_rows[1].active_threads.availability == Availability::kMissing);
    auto rows = IsaDataBuilder::BuildSourceLines(data, 11);
    IsaDataBuilder::UpdateSourceSampling(rows, data, 11, prepared);
    CHECK(rows[0].wave_occupancy.percent == 40.0);
    CHECK(rows[0].active_threads.percent == 50.0);
}

TEST_CASE("Sample weight overflow invalidates percentages at either grouping stage",
          "[isa][source][sample-metrics]")
{
    using Availability    = IsaDataBuilder::SamplingAvailability;
    PcSamplingData data   = make_source_sampling_data();
    data.pc_sample_states = { { 1, 10, UINT64_MAX, UINT64_MAX, 0, 100.0, 0.0 } };
    auto isa_rows         = IsaDataBuilder::BuildInstructions(data, 1);
    IsaDataBuilder::UpdateSampling(isa_rows, IsaDataBuilder::PrepareSampling(data));
    CHECK(isa_rows[0].wave_occupancy.availability == Availability::kValid);
    CHECK(isa_rows[0].wave_occupancy.percent == 100.0);
    CHECK(isa_rows[0].active_threads.availability == Availability::kValid);
    CHECK(isa_rows[0].active_threads.percent == 0.0);
    SECTION("Overflow while merging states for one instruction")
    {
        data.pc_sample_states.push_back({ 2, 10, 1, 1, 0, 50.0, 50.0 });
    }
    SECTION("Overflow while merging instructions for one source line")
    {
        data.pc_sample_states.push_back({ 2, 20, 1, 1, 0, 50.0, 50.0 });
    }
    const auto prepared = IsaDataBuilder::PrepareSampling(data);
    IsaDataBuilder::UpdateSampling(isa_rows, prepared);
    CHECK(isa_rows[0].wave_occupancy.availability ==
          (data.pc_sample_states[1].instruction_uuid == 10 ? Availability::kInvalid
                                                           : Availability::kValid));
    auto rows = IsaDataBuilder::BuildSourceLines(data, 11);
    IsaDataBuilder::UpdateSourceSampling(rows, data, 11, prepared);
    CHECK(rows[0].wave_occupancy.availability == Availability::kInvalid);
    CHECK(rows[0].active_threads.availability == Availability::kInvalid);
}

TEST_CASE("Sampling updates clear stale percentages and reuse current source mappings",
          "[isa][source][sample-metrics]")
{
    using Availability    = IsaDataBuilder::SamplingAvailability;
    PcSamplingData data   = make_source_sampling_data();
    data.pc_sample_states = { { 1, 10, 100, 100, 0, 49.4, 100.0 } };
    const auto prepared   = IsaDataBuilder::PrepareSampling(data);
    auto       isa_rows   = IsaDataBuilder::BuildInstructions(data, 1);
    auto       rows       = IsaDataBuilder::BuildSourceLines(data, 11);
    const auto mappings   = data.instruction_source_lines;
    data.instruction_source_lines.clear();
    IsaDataBuilder::UpdateSourceSampling(rows, data, 11, prepared);
    CHECK(rows[0].wave_occupancy.availability == Availability::kMissing);
    data.instruction_source_lines = mappings;
    IsaDataBuilder::UpdateSampling(isa_rows, prepared);
    IsaDataBuilder::UpdateSourceSampling(rows, data, 11, prepared);
    CHECK(rows[0].wave_occupancy.availability == Availability::kValid);
    CHECK(rows[0].wave_occupancy.percent == Catch::Approx(49.4));
    CHECK(rows[0].active_threads.percent == 100.0);
    IsaDataBuilder::UpdateSampling(isa_rows, {});
    IsaDataBuilder::UpdateSourceSampling(rows, data, 11, {});
    CHECK(isa_rows[0].wave_occupancy.availability == Availability::kMissing);
    CHECK(isa_rows[0].active_threads.availability == Availability::kMissing);
    CHECK(rows[0].wave_occupancy.availability == Availability::kMissing);
    CHECK(rows[0].active_threads.availability == Availability::kMissing);
    CHECK(rows[0].wave_occupancy.percent == 0.0);
    CHECK(rows[0].content == "first();");
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
    CHECK(hottest == 0);
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
    CHECK(IsaDataBuilder::UpdateSampling(isa, prepared) == 0);
    CHECK(IsaDataBuilder::UpdateSourceSampling(source, data, 11, prepared) == 0);
    CHECK(isa[0].sample_counts.total_count == UINT64_MAX);
    CHECK(isa[0].sampling.availability == IsaDataBuilder::SamplingAvailability::kInvalid);
    CHECK(source[0].sampling.availability == isa[0].sampling.availability);
}

TEST_CASE("Invalid instructions do not collapse other ISA or source sample bars",
          "[isa][source]")
{
    PcSamplingData data = make_source_sampling_data();
    data.instruction_source_lines = { { 10, 101, 11, 0 }, { 20, 102, 11, 0 } };
    data.pc_sample_states = { { 1, 10, UINT64_MAX, 0, 0 },
                              { 2, 10, 1, 0, 0 },
                              { 3, 20, 100, 25, 75 } };
    SECTION("Overflowing instruction")
    {
        // Keep the overflowing total and its otherwise valid neighbor.
    }
    SECTION("Malformed progress counts")
    {
        data.pc_sample_states = { { 1, 10, 1000, 900, 200 }, { 2, 20, 100, 25, 75 } };
    }
    SECTION("Malformed counts with missing progress")
    {
        data.pc_sample_states = { { 1, 10, 1000, std::nullopt, 1001 },
                                  { 2, 20, 100, 25, 75 } };
    }
    const auto prepared = IsaDataBuilder::PrepareSampling(data);
    auto       isa      = IsaDataBuilder::BuildInstructions(data, 1);
    auto       source   = IsaDataBuilder::BuildSourceLines(data, 11);
    CHECK(IsaDataBuilder::UpdateSampling(isa, prepared) == 100);
    CHECK(IsaDataBuilder::UpdateSourceSampling(source, data, 11, prepared) == 100);
    CHECK(isa[0].sampling.availability == IsaDataBuilder::SamplingAvailability::kInvalid);
    CHECK(source[0].sampling.availability ==
          IsaDataBuilder::SamplingAvailability::kInvalid);
    CHECK(isa[1].sample_counts.total_count == 100);
    CHECK(source[1].sample_counts.total_count == 100);
    CHECK(isa[1].sampling.issue_percent == 25.0);
    CHECK(source[1].sampling.stall_percent == 75.0);
}

TEST_CASE("Source overflow does not hide another line or invalidate ISA totals",
          "[isa][source]")
{
    PcSamplingData data = make_source_sampling_data();
    data.pc_sample_states = { { 1, 10, UINT64_MAX, UINT64_MAX, 0 },
                              { 2, 20, 1, 0, 1 },
                              { 3, 30, 250, 100, 150 } };
    const auto prepared = IsaDataBuilder::PrepareSampling(data);
    auto       isa      = IsaDataBuilder::BuildInstructions(data, 1);
    auto       source   = IsaDataBuilder::BuildSourceLines(data, 11);
    CHECK(IsaDataBuilder::UpdateSampling(isa, prepared) == UINT64_MAX);
    CHECK(IsaDataBuilder::UpdateSourceSampling(source, data, 11, prepared) == 250);
    CHECK(isa[0].sampling.availability == IsaDataBuilder::SamplingAvailability::kValid);
    CHECK(source[0].sample_counts.total_count == UINT64_MAX);
    CHECK(source[0].sampling.availability ==
          IsaDataBuilder::SamplingAvailability::kInvalid);
    CHECK(source[1].sample_counts.total_count == 250);
    CHECK(source[1].sampling.availability ==
          IsaDataBuilder::SamplingAvailability::kValid);
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

static void
check_unavailable_reason_breakdown(const IsaDataBuilder::SamplingRow& row, uint64_t total,
                                   uint64_t issued, uint64_t stalled)
{
    CHECK(row.sample_counts.total_count == total);
    CHECK(row.sample_counts.issue_count == issued);
    CHECK(row.sample_counts.stall_count == stalled);
    CHECK_FALSE(row.sample_counts.invalid_counts);
    CHECK(row.sampling.availability == IsaDataBuilder::SamplingAvailability::kValid);
    CHECK(row.stall_reason_counts_overflow);
    CHECK(row.stall_reasons.empty());
    CHECK(row.stall_reason_sample_count == 0);
    CHECK(row.unclassified_stall_count == stalled);
}

TEST_CASE("Overflowing reason records cannot wrap into valid normalization totals",
          "[isa][source]")
{
    PcSamplingData data   = make_source_sampling_data();
    data.pc_sample_states = { { 1, 10, 100, 40, 60 } };
    SECTION("Repeated lookup overflows")
    {
        data.pc_sample_stall_reasons = { { 1, 2, UINT64_MAX }, { 1, 2, 1 } };
    }
    SECTION("Distinct lookups overflow the classified total")
    {
        // An unchecked sum wraps to 100 and could falsely justify subtracting issued
        // samples from OTHER_WAIT, leaving an apparently matching stalled total.
        data.pc_sample_stall_reasons = { { 1, 1, UINT64_MAX }, { 1, 2, 101 } };
    }
    const auto prepared = IsaDataBuilder::PrepareSampling(data);
    CHECK(prepared.reasons_by_instruction.at(10).overflow);
    CHECK(prepared.reasons_by_instruction.at(10).total_count == UINT64_MAX);
    auto isa    = IsaDataBuilder::BuildInstructions(data, 1);
    auto source = IsaDataBuilder::BuildSourceLines(data, 11);
    IsaDataBuilder::UpdateSampling(isa, prepared);
    IsaDataBuilder::UpdateSourceSampling(source, data, 11, prepared);
    check_unavailable_reason_breakdown(isa[0], 100, 40, 60);
    check_unavailable_reason_breakdown(source[0], 100, 40, 60);

    // A later valid update must clear the warning and restore the reason colors.
    data.pc_sample_stall_reasons = { { 1, 2, 60 } };
    const auto recovered         = IsaDataBuilder::PrepareSampling(data);
    IsaDataBuilder::UpdateSampling(isa, recovered);
    IsaDataBuilder::UpdateSourceSampling(source, data, 11, recovered);
    CHECK_FALSE(isa[0].stall_reason_counts_overflow);
    CHECK_FALSE(source[0].stall_reason_counts_overflow);
    REQUIRE(isa[0].stall_reasons.size() == 1);
    REQUIRE(source[0].stall_reasons.size() == 1);
    CHECK(isa[0].stall_reason_sample_count == 60);
    CHECK(source[0].stall_reason_sample_count == 60);
    CHECK(isa[0].stall_reasons[0].bar_share == 1.0);
    IsaDataBuilder::UpdateSampling(isa, {});
    IsaDataBuilder::UpdateSourceSampling(source, data, 11, {});
    CHECK_FALSE(isa[0].stall_reason_counts_overflow);
    CHECK_FALSE(source[0].stall_reason_counts_overflow);
}

TEST_CASE("Instruction reason merges retain overflow from sums across sample states",
          "[isa][source]")
{
    PcSamplingData data          = make_source_sampling_data();
    data.pc_sample_states        = { { 1, 10, 100, 40, 60 }, { 2, 10, 100, 40, 60 } };
    data.pc_sample_stall_reasons = { { 1, 2, UINT64_MAX }, { 2, 2, 1 } };
    SECTION("Same lookup")
    {
        // Keep both counts in the same reason bucket.
    }
    SECTION("Different lookups")
    {
        data.pc_sample_stall_reasons[1].pc_sample_stall_reason_lookup_uuid = 3;
    }
    const auto prepared = IsaDataBuilder::PrepareSampling(data);
    auto       isa      = IsaDataBuilder::BuildInstructions(data, 1);
    auto       source   = IsaDataBuilder::BuildSourceLines(data, 11);
    IsaDataBuilder::UpdateSampling(isa, prepared);
    IsaDataBuilder::UpdateSourceSampling(source, data, 11, prepared);
    check_unavailable_reason_breakdown(isa[0], 200, 80, 120);
    check_unavailable_reason_breakdown(source[0], 200, 80, 120);
}

TEST_CASE(
    "Source reason merges detect overflow across otherwise representable instructions",
    "[isa][source]")
{
    PcSamplingData data          = make_source_sampling_data();
    data.pc_sample_states        = { { 1, 10, 100, 40, 60 }, { 2, 20, 100, 40, 60 } };
    data.pc_sample_stall_reasons = { { 1, 2, UINT64_MAX }, { 2, 2, 1 } };
    const auto prepared          = IsaDataBuilder::PrepareSampling(data);
    CHECK_FALSE(prepared.reasons_by_instruction.at(10).overflow);
    CHECK_FALSE(prepared.reasons_by_instruction.at(20).overflow);
    auto isa    = IsaDataBuilder::BuildInstructions(data, 1);
    auto source = IsaDataBuilder::BuildSourceLines(data, 11);
    IsaDataBuilder::UpdateSampling(isa, prepared);
    IsaDataBuilder::UpdateSourceSampling(source, data, 11, prepared);
    CHECK_FALSE(isa[0].stall_reason_counts_overflow);
    CHECK_FALSE(isa[1].stall_reason_counts_overflow);
    check_unavailable_reason_breakdown(source[0], 200, 80, 120);
}

TEST_CASE("The maximum representable reason count remains valid at the boundary",
          "[isa][source]")
{
    PcSamplingData data          = make_source_sampling_data();
    data.pc_sample_states        = { { 1, 10, UINT64_MAX, 0, UINT64_MAX } };
    data.pc_sample_stall_reasons = { { 1, 2, UINT64_MAX }, { 1, 3, 0 } };
    const auto prepared          = IsaDataBuilder::PrepareSampling(data);
    auto       isa               = IsaDataBuilder::BuildInstructions(data, 1);
    auto       source            = IsaDataBuilder::BuildSourceLines(data, 11);
    IsaDataBuilder::UpdateSampling(isa, prepared);
    IsaDataBuilder::UpdateSourceSampling(source, data, 11, prepared);
    CHECK_FALSE(isa[0].stall_reason_counts_overflow);
    CHECK_FALSE(source[0].stall_reason_counts_overflow);
    CHECK(isa[0].stall_reason_sample_count == UINT64_MAX);
    CHECK(source[0].stall_reason_sample_count == UINT64_MAX);
    REQUIRE_FALSE(isa[0].stall_reasons.empty());
    CHECK(isa[0].stall_reasons[0].bar_share == 1.0);
}

static PcSamplingData
make_issue_sampling_data()
{
    PcSamplingData data             = make_source_sampling_data();
    data.pc_sample_states           = { { 1, 10, 100, 80, 20 }, { 2, 20, 50, 10, 40 } };
    data.instruction_sample_lookups = { { 1, "NO_INST" }, { 2, "VALU" } };
    data.instruction_samples        = { { 1, 1, 20 }, { 1, 2, 80 }, { 2, 2, 50 } };
    data.instruction_samples_loaded = true;
    return data;
}

TEST_CASE("Issue states preserve the reported barrier and VALU counts",
          "[isa][issue-states]")
{
    PcSamplingData data                                      = make_issue_sampling_data();
    data.code_objects[0].kernel_symbols[0].instruction_lines = {
        { 10, 7, 0x35C0, "s_barrier" }, { 20, 3, 0x26CC, "v_add_f64 v[0:1], v[2:3], 1.0" }
    };
    data.instruction_type_lookup_map = { { 7, "BARRIER" }, { 3, "VALU" } };
    data.pc_sample_states = { { 1, 10, 1901, 252, 1649 }, { 2, 20, 9538, 2012, 7526 } };
    data.instruction_sample_lookups.push_back({ 9, "BARRIER" });
    data.instruction_samples = { { 1, 1, 1649 }, { 1, 9, 252 }, { 2, 2, 9538 } };
    data.pc_sample_stall_reason_lookups = { { 1, "OTHER_WAIT" }, { 2, "BARRIER_WAIT" } };
    data.pc_sample_stall_reasons        = { { 1, 1, 252 }, { 1, 2, 1649 } };
    const auto prepared                 = IsaDataBuilder::PrepareSampling(data);
    auto       rows                     = IsaDataBuilder::BuildInstructions(data, 1);
    IsaDataBuilder::UpdateSampling(rows, prepared);
    REQUIRE(rows.size() == 2);
    CHECK(rows[0].instruction_kind == IsaDataBuilder::InstructionKind::kBarrier);
    CHECK(rows[0].issue_states.availability ==
          IsaDataBuilder::SamplingAvailability::kValid);
    CHECK(rows[0].issue_states.total_count == 1901);
    REQUIRE(rows[0].issue_states.states.size() == 2);
    const auto& no_inst = rows[0].issue_states.states[0];
    CHECK(no_inst.text == "NO_INST");
    CHECK(no_inst.count == 1649);
    CHECK(no_inst.count_text == "1,649");
    CHECK(no_inst.percent == Catch::Approx(1649.0 / 1901.0 * 100.0));
    CHECK(rows[0].issue_states.states[1].text == "BARRIER");
    CHECK(rows[0].issue_states.states[1].count == 252);
    CHECK(rows[0].issue_states.states[1].percent ==
          Catch::Approx(252.0 / 1901.0 * 100.0));
    CHECK(rows[0].sample_counts.issue_count == 252);
    CHECK(rows[0].sample_counts.stall_count == 1649);
    REQUIRE(rows[0].stall_reasons.size() == 1);
    CHECK(rows[0].stall_reasons[0].text == "BARRIER_WAIT");
    CHECK(rows[0].stall_reasons[0].count == 1649);
    REQUIRE(rows[1].issue_states.states.size() == 1);
    CHECK(rows[1].issue_states.states[0].text == "VALU");
    CHECK(rows[1].issue_states.states[0].count == 9538);
    CHECK(rows[1].issue_states.states[0].percent == 100.0);
    CHECK(rows[1].sample_counts.issue_count == 2012);
}

TEST_CASE("Issue states merge by instruction UUID and retain both branch outcomes",
          "[isa][issue-states]")
{
    PcSamplingData data   = make_issue_sampling_data();
    data.pc_sample_states = { { 1, 10, 100, 80, 20 },
                              { 3, 10, 50, 10, 40 },
                              { 2, 20, 50, 10, 40 } };
    data.code_objects[0].kernel_symbols[0].instruction_lines[0] = {
        10, 8, 0x40, "s_cbranch_scc1 590"
    };
    data.instruction_type_lookup_map = { { 8, "BRANCH" } };
    data.code_objects[0].kernel_symbols[0].instruction_lines[1].code_object_offset = 0x40;
    data.instruction_sample_lookups = { { 99, "BRANCH_NOT_TAKEN" },
                                        { 7, "BRANCH_TAKEN" },
                                        { 2, "VALU" } };
    data.instruction_samples = { { 1, 99, 50 },          { 1, 99, 30 }, { 1, 7, 20 },
                                 { 3, 99, 30 },          { 3, 7, 20 },  { 2, 2, 50 },
                                 { 999, 99, UINT64_MAX } };
    auto rows                = IsaDataBuilder::BuildInstructions(data, 1);
    IsaDataBuilder::UpdateSampling(rows, IsaDataBuilder::PrepareSampling(data));
    CHECK(rows[0].instruction_kind == IsaDataBuilder::InstructionKind::kBranch);
    REQUIRE(rows[0].issue_states.states.size() == 2);
    CHECK(rows[0].issue_states.total_count == 150);
    CHECK(rows[0].issue_states.states[0].text == "BRANCH_NOT_TAKEN");
    CHECK(rows[0].issue_states.states[0].count == 110);
    CHECK(rows[0].issue_states.states[0].percent == Catch::Approx(110.0 / 150.0 * 100.0));
    CHECK(rows[0].issue_states.states[1].text == "BRANCH_TAKEN");
    CHECK(rows[0].issue_states.states[1].count == 40);
    REQUIRE(rows[1].issue_states.states.size() == 1);
    CHECK(rows[1].issue_states.states[0].text == "VALU");
    CHECK(rows[1].issue_states.total_count == 50);
}

TEST_CASE("Incomplete issue distributions never publish partial counts or shares",
          "[isa][issue-states][source]")
{
    using Availability  = IsaDataBuilder::SamplingAvailability;
    PcSamplingData data = make_issue_sampling_data();
    SECTION("Fetch incomplete") { data.instruction_samples_loaded = false; }
    SECTION("No records") { data.instruction_samples.clear(); }
    SECTION("NULL count") { data.instruction_samples[0].count.reset(); }
    SECTION("Missing records for a contributing sample state")
    {
        data.pc_sample_states.push_back({ 3, 10, 10, 5, 5 });
    }
    SECTION("Recorded counts cover only part of the samples")
    {
        data.instruction_samples[0].count = 19;
    }
    SECTION("Known totals cannot imply that an unrecorded count was zero")
    {
        data.instruction_samples[0].count.reset();
        data.instruction_samples[1].count = 100;
    }
    const auto prepared = IsaDataBuilder::PrepareSampling(data);
    auto       isa      = IsaDataBuilder::BuildInstructions(data, 1);
    auto       source   = IsaDataBuilder::BuildSourceLines(data, 11);
    IsaDataBuilder::UpdateSampling(isa, prepared);
    IsaDataBuilder::UpdateSourceSampling(source, data, 11, prepared);
    const IsaDataBuilder::SamplingRow* checked_rows[] = { &isa[0], &source[0] };
    for(const IsaDataBuilder::SamplingRow* row : checked_rows)
    {
        CHECK(row->issue_states.availability == Availability::kMissing);
        CHECK(row->issue_states.states.empty());
        CHECK(row->issue_states.total_count == 0);
        CHECK(row->sampling.availability == Availability::kValid);
    }
}

TEST_CASE("Issue-state zero counts are recorded and zero-sample gaps do not contribute",
          "[isa][issue-states]")
{
    PcSamplingData data               = make_issue_sampling_data();
    data.instruction_samples[0].count = 0;
    data.instruction_samples[1].count = 100;
    uint64_t total                    = 100;
    SECTION("Measured zero with observed samples") {}
    SECTION("Measured zero with zero total samples")
    {
        total                             = 0;
        data.pc_sample_states[0]          = { 1, 10, 0, 0, 0 };
        data.instruction_samples[1].count = 0;
    }
    SECTION("An unrecorded zero-sample state")
    {
        data.pc_sample_states.push_back({ 3, 10, 0, std::nullopt, std::nullopt });
    }
    SECTION("NULL issue counts on a zero-sample state")
    {
        data.pc_sample_states.push_back({ 3, 10, 0, std::nullopt, std::nullopt });
        data.instruction_samples.push_back({ 3, 1, std::nullopt });
    }
    auto rows = IsaDataBuilder::BuildInstructions(data, 1);
    IsaDataBuilder::UpdateSampling(rows, IsaDataBuilder::PrepareSampling(data));
    const auto& issues = rows[0].issue_states;
    CHECK(issues.availability == IsaDataBuilder::SamplingAvailability::kValid);
    CHECK(issues.total_count == total);
    REQUIRE(issues.states.size() == 2);
    const auto no_inst = std::find_if(
        issues.states.begin(), issues.states.end(),
        [](const IsaDataBuilder::IssueState& state) { return state.lookup_uuid == 1; });
    REQUIRE(no_inst != issues.states.end());
    CHECK(no_inst->text == "NO_INST");
    CHECK(no_inst->count == 0);
    CHECK(no_inst->count_text == "0");
    CHECK(no_inst->percent == 0.0);
}

TEST_CASE("Issue-state labels retain captured names and deterministic unknown identities",
          "[isa][issue-states]")
{
    PcSamplingData data             = make_issue_sampling_data();
    data.instruction_sample_lookups = {
        { 1, "SDK_FUTURE_STATE" }, { 2, "DUPLICATE" }, { 3, "DUPLICATE" }, { 4, "" }
    };
    data.instruction_samples = { { 1, 4, 20 }, { 1, 3, 20 }, { 1, 2, 20 },
                                 { 1, 1, 20 }, { 1, 0, 20 }, { 2, 1, 50 } };
    auto rows                = IsaDataBuilder::BuildInstructions(data, 1);
    IsaDataBuilder::UpdateSampling(rows, IsaDataBuilder::PrepareSampling(data));
    const std::vector<uint64_t> expected = { 2, 3, 1, 0, 4 };
    REQUIRE(rows[0].issue_states.states.size() == expected.size());
    for(size_t i = 0; i < expected.size(); i++)
    {
        CHECK(rows[0].issue_states.states[i].lookup_uuid == expected[i]);
    }
    CHECK(rows[0].issue_states.states[2].text == "SDK_FUTURE_STATE");
    CHECK(rows[0].issue_states.states[3].text == "Unknown issue state (lookup ID 0)");
    CHECK(rows[0].issue_states.states[4].text == "Unknown issue state (lookup ID 4)");
    std::reverse(data.instruction_samples.begin(), data.instruction_samples.end());
    IsaDataBuilder::UpdateSampling(rows, IsaDataBuilder::PrepareSampling(data));
    for(size_t i = 0; i < expected.size(); i++)
    {
        CHECK(rows[0].issue_states.states[i].lookup_uuid == expected[i]);
    }
}

TEST_CASE("Issue-state validity is independent of malformed or missing progress counts",
          "[isa][issue-states]")
{
    PcSamplingData data = make_issue_sampling_data();
    SECTION("Host-trap-style missing progress")
    {
        data.pc_sample_states[0].issue_count.reset();
        data.pc_sample_states[0].stall_count.reset();
    }
    SECTION("Malformed progress counts") { data.pc_sample_states[0].issue_count = 101; }
    auto rows = IsaDataBuilder::BuildInstructions(data, 1);
    IsaDataBuilder::UpdateSampling(rows, IsaDataBuilder::PrepareSampling(data));
    CHECK(rows[0].sampling.availability != IsaDataBuilder::SamplingAvailability::kValid);
    CHECK(rows[0].issue_states.availability ==
          IsaDataBuilder::SamplingAvailability::kValid);
    CHECK(rows[0].issue_states.total_count == 100);
    REQUIRE(rows[0].issue_states.states.size() == 2);
    CHECK(rows[0].issue_states.states[0].count == 80);
}

TEST_CASE("Invalid issue contributors and overflow survive instruction and source merges",
          "[isa][issue-states][source]")
{
    using Availability         = IsaDataBuilder::SamplingAvailability;
    PcSamplingData data        = make_issue_sampling_data();
    bool           source_only = false;
    SECTION("Repeated lookup overflow")
    {
        data.instruction_samples = { { 1, 1, UINT64_MAX }, { 1, 1, 101 }, { 2, 2, 50 } };
    }
    SECTION("Distinct lookup overflow")
    {
        data.instruction_samples = { { 1, 1, UINT64_MAX }, { 1, 2, 101 }, { 2, 2, 50 } };
    }
    SECTION("Overflow across individually valid states of one instruction")
    {
        data.pc_sample_states    = { { 1, 10, UINT64_MAX, 0, UINT64_MAX },
                                     { 3, 10, 1, 1, 0 } };
        data.instruction_samples = { { 1, 1, UINT64_MAX }, { 3, 1, 1 } };
    }
    SECTION("Overflow across individually valid instructions of one source line")
    {
        source_only              = true;
        data.pc_sample_states    = { { 1, 10, UINT64_MAX, 0, UINT64_MAX },
                                     { 2, 20, 1, 1, 0 } };
        data.instruction_samples = { { 1, 1, UINT64_MAX }, { 2, 1, 1 } };
    }
    SECTION("Opposite per-state mismatches cannot cancel")
    {
        data.pc_sample_states    = { { 1, 10, 50, 30, 20 }, { 3, 10, 50, 30, 20 } };
        data.instruction_samples = { { 1, 1, 60 }, { 3, 1, 40 } };
    }
    SECTION("Known excess takes precedence over a NULL contributor")
    {
        data.instruction_samples = { { 1, 1, 101 },
                                     { 1, 2, std::nullopt },
                                     { 2, 2, 50 } };
    }
    SECTION("Positive issue count on a zero-sample state")
    {
        data.pc_sample_states.push_back({ 3, 10, 0, 0, 0 });
        data.instruction_samples.push_back({ 3, 1, 1 });
    }
    const auto prepared = IsaDataBuilder::PrepareSampling(data);
    auto       isa      = IsaDataBuilder::BuildInstructions(data, 1);
    auto       source   = IsaDataBuilder::BuildSourceLines(data, 11);
    IsaDataBuilder::UpdateSampling(isa, prepared);
    IsaDataBuilder::UpdateSourceSampling(source, data, 11, prepared);
    CHECK(isa[0].issue_states.availability ==
          (source_only ? Availability::kValid : Availability::kInvalid));
    if(!source_only)
    {
        CHECK(isa[0].issue_states.states.empty());
        CHECK(isa[0].issue_states.total_count == 0);
    }
    CHECK(source[0].issue_states.availability == Availability::kInvalid);
    CHECK(source[0].issue_states.states.empty());
    CHECK(source[0].issue_states.total_count == 0);
}

TEST_CASE("The largest representable issue count remains valid",
          "[isa][issue-states][source]")
{
    PcSamplingData data      = make_issue_sampling_data();
    data.pc_sample_states    = { { 1, 10, UINT64_MAX, 0, UINT64_MAX } };
    data.instruction_samples = { { 1, 1, UINT64_MAX }, { 1, 2, 0 } };
    const auto prepared      = IsaDataBuilder::PrepareSampling(data);
    auto       isa           = IsaDataBuilder::BuildInstructions(data, 1);
    auto       source        = IsaDataBuilder::BuildSourceLines(data, 11);
    IsaDataBuilder::UpdateSampling(isa, prepared);
    IsaDataBuilder::UpdateSourceSampling(source, data, 11, prepared);
    const IsaDataBuilder::SamplingRow* checked_rows[] = { &isa[0], &source[0] };
    for(const IsaDataBuilder::SamplingRow* row : checked_rows)
    {
        CHECK(row->issue_states.availability ==
              IsaDataBuilder::SamplingAvailability::kValid);
        CHECK(row->issue_states.total_count == UINT64_MAX);
        REQUIRE(row->issue_states.states.size() == 2);
        CHECK(row->issue_states.states[0].count == UINT64_MAX);
        CHECK(row->issue_states.states[0].percent == 100.0);
        CHECK(row->issue_states.states[1].count == 0);
    }
}

TEST_CASE("Source issue aggregation retains missing contributors and owned snapshots",
          "[isa][issue-states][source]")
{
    using Availability      = IsaDataBuilder::SamplingAvailability;
    PcSamplingData data     = make_issue_sampling_data();
    const auto     prepared = IsaDataBuilder::PrepareSampling(data);
    data.instruction_samples.clear();
    data.instruction_sample_lookups.clear();
    auto isa    = IsaDataBuilder::BuildInstructions(data, 1);
    auto source = IsaDataBuilder::BuildSourceLines(data, 11);
    IsaDataBuilder::UpdateSampling(isa, prepared);
    IsaDataBuilder::UpdateSourceSampling(source, data, 11, prepared);
    CHECK(source[0].issue_states.availability == Availability::kValid);
    CHECK(source[0].issue_states.total_count == 150);
    REQUIRE(source[0].issue_states.states.size() == 2);
    CHECK(source[0].issue_states.states[0].text == "VALU");
    CHECK(source[0].issue_states.states[0].count == 130);
    CHECK(source[0].issue_states.states[0].percent ==
          Catch::Approx(130.0 / 150.0 * 100.0));
    CHECK(source[1].issue_states.availability == Availability::kMissing);

    data.instruction_source_lines[1].source_line_uuid = 102;
    IsaDataBuilder::UpdateSourceSampling(source, data, 11, prepared);
    CHECK(source[0].issue_states.total_count == 50);
    REQUIRE(source[0].issue_states.states.size() == 1);
    CHECK(source[0].issue_states.states[0].text == "VALU");
    CHECK(source[1].issue_states.total_count == 100);

    data = make_issue_sampling_data();
    data.instruction_samples.pop_back();
    const auto incomplete = IsaDataBuilder::PrepareSampling(data);
    IsaDataBuilder::UpdateSampling(isa, incomplete);
    IsaDataBuilder::UpdateSourceSampling(source, data, 11, incomplete);
    CHECK(isa[0].issue_states.availability == Availability::kValid);
    CHECK(isa[1].issue_states.availability == Availability::kMissing);
    CHECK(source[0].issue_states.availability == Availability::kMissing);
    CHECK(source[0].issue_states.states.empty());

    IsaDataBuilder::UpdateSampling(isa, {});
    IsaDataBuilder::UpdateSourceSampling(source, data, 11, {});
    CHECK(isa[0].issue_states.availability == Availability::kMissing);
    CHECK(isa[0].issue_states.states.empty());
    CHECK(isa[0].issue_states.total_count == 0);
    CHECK(source[0].issue_states.availability == Availability::kMissing);
    CHECK(source[0].issue_states.states.empty());
    CHECK(source[0].issue_states.total_count == 0);
}

}  // namespace View
}  // namespace RocProfVis
