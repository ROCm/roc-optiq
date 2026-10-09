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
    const IsaDataBuilder::SamplingTotals totals =
        IsaDataBuilder::UpdateSampling(rows, data);
    REQUIRE(rows.size() == 2);
    CHECK(rows[0].sample_counts.total_count == 150);
    CHECK_FALSE(rows[0].sample_counts.issue_count.has_value());
    CHECK(rows[0].sample_counts.stall_count == 30);
    CHECK(rows[0].sampling.availability ==
          IsaDataBuilder::SamplingAvailability::kMissing);
    CHECK(rows[1].sampling.availability == IsaDataBuilder::SamplingAvailability::kValid);
    CHECK(rows[1].sampling.stall_percent == 100.0);
    CHECK(totals.kernel_total_samples == 690);
    CHECK(totals.hottest_instruction_samples == 150);
    CHECK(totals.stall_data_available);
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
    SECTION("Canonical OTHER_WAIT") {}
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
    IsaDataBuilder::UpdateSampling(rows, data);
    REQUIRE(rows[0].stall_reasons.size() == 2);
    CHECK(rows[0].stall_reason_sample_count == 20);
    CHECK(rows[0].stall_reasons[0].text == "ALU_DEPENDENCY");
    CHECK(rows[0].stall_reasons[0].count == 15);
    CHECK(rows[0].stall_reasons[0].share == 75.0);
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
    IsaDataBuilder::UpdateSampling(rows, data);
    CHECK(rows[0].sampling.stall_percent == 20.0);
    CHECK(rows[0].stall_reason_sample_count == 40);
    CHECK(rows[0].stall_reasons[0].share == 150.0);
    CHECK(rows[0].stall_reasons[0].bar_share == Catch::Approx(0.75));
    CHECK(rows[0].stall_reasons[1].bar_share == Catch::Approx(0.25));
    CHECK(rows[0].unclassified_stall_count == 0);

    data.pc_sample_stall_reasons = { { 1, 1, 5 } };
    IsaDataBuilder::UpdateSampling(rows, data);
    REQUIRE(rows[0].stall_reasons.size() == 1);
    CHECK(rows[0].unclassified_stall_count == 15);
    CHECK(rows[0].unclassified_stall_count_text == "15");
    CHECK(rows[0].stall_reasons[0].bar_share == Catch::Approx(0.25));

    data.pc_sample_stall_reasons.clear();
    IsaDataBuilder::UpdateSampling(rows, data);
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
    IsaDataBuilder::UpdateSampling(rows, data);
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
    IsaDataBuilder::UpdateSampling(rows, data);
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
    IsaDataBuilder::UpdateSampling(rows, data);
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

    const IsaDataBuilder::SamplingTotals cleared =
        IsaDataBuilder::UpdateSampling(rows, {});
    CHECK(rows.data() == storage);
    CHECK(rows[0].sample_counts.total_count == 0);
    CHECK(rows[0].stall_reasons.empty());
    CHECK_FALSE(cleared.stall_data_available);
}

}  // namespace View
}  // namespace RocProfVis
