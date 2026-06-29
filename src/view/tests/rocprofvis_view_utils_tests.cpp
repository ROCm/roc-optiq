// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// Unit tests for the free functions and the CircularBuffer container in
// rocprofvis_utils.{h,cpp}. These are pure, context-free helpers, so they are
// ideal unit-test targets: deterministic output, lots of edge cases
// (zero / negative / non-finite values), and no ImGui dependency.

#include "rocprofvis_utils.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <limits>
#include <string>

using namespace RocProfVis::View;

namespace
{
constexpr double kInf = std::numeric_limits<double>::infinity();
const double     kNaN = std::numeric_limits<double>::quiet_NaN();
}  // namespace

TEST_CASE("nanosecond_to_str truncates toward zero and appends units", "[utils][time]")
{
    CHECK(nanosecond_to_str(0.0) == "0");
    CHECK(nanosecond_to_str(1500.9) == "1500");  // truncation, not rounding
    CHECK(nanosecond_to_str(42.0, true) == "42 ns");
}

TEST_CASE("nanosecond_to_us_str formats microseconds with 3 fractional digits",
          "[utils][time]")
{
    // 1500 ns = 1.500 us
    CHECK(nanosecond_to_us_str(1500.0) == "1.500");
    // remainder is zero-padded to 3 digits
    CHECK(nanosecond_to_us_str(1001.0) == "1.001");
    CHECK(nanosecond_to_us_str(999.0) == "0.999");
    // units suffix
    CHECK(nanosecond_to_us_str(2000.0, true) == "2.000 us");
    // non-finite is reported as NaN
    CHECK(nanosecond_to_us_str(kNaN) == "NaN");
    CHECK(nanosecond_to_us_str(kInf) == "NaN");
}

TEST_CASE("nanosecond_to_ms_str formats milliseconds with 6 fractional digits",
          "[utils][time]")
{
    // 2_500_000 ns = 2.500000 ms
    CHECK(nanosecond_to_ms_str(2500000.0) == "2.500000");
    CHECK(nanosecond_to_ms_str(1000001.0) == "1.000001");
    CHECK(nanosecond_to_ms_str(5000000.0, true) == "5.000000 ms");
    CHECK(nanosecond_to_ms_str(kNaN) == "NaN");
}

TEST_CASE("nanosecond_to_s_str formats seconds with 9 fractional digits", "[utils][time]")
{
    // 1_500_000_000 ns = 1.500000000 s
    CHECK(nanosecond_to_s_str(1500000000.0) == "1.500000000");
    CHECK(nanosecond_to_s_str(1000000000.0, true) == "1.000000000 s");
    CHECK(nanosecond_to_s_str(kInf) == "NaN");
}

TEST_CASE("nanosecond_to_timecode_str handles zero, sign and condensing",
          "[utils][time]")
{
    SECTION("zero is always positive and formatted by condensed flag")
    {
        CHECK(nanosecond_to_timecode_str(0.0, true) == "0.000000000");
        CHECK(nanosecond_to_timecode_str(0.0, false) == "00:00.000000000");
    }

    SECTION("condensed omits leading zero hours/minutes")
    {
        // 1.5 seconds -> seconds.nanoseconds only
        CHECK(nanosecond_to_timecode_str(1500000000.0, true) == "01.500000000");
    }

    SECTION("non-condensed always shows HH:MM:SS")
    {
        CHECK(nanosecond_to_timecode_str(1500000000.0, false) == "00:00:01.500000000");
    }

    SECTION("hours, minutes and seconds compose")
    {
        // 1h 2m 3s = 3723 s
        const double t = 3723.0 * 1000000000.0;
        CHECK(nanosecond_to_timecode_str(t, true) == "01:02:03.000000000");
    }

    SECTION("negative values keep a sign prefix when magnitude is significant")
    {
        CHECK(nanosecond_to_timecode_str(-1500000000.0, true) == "-01.500000000");
    }

    SECTION("sub-nanosecond negative magnitudes collapse to positive zero")
    {
        // |value| < 1.0 truncates to 0 ns, so no negative sign.
        CHECK(nanosecond_to_timecode_str(-0.4, true) == "0.000000000");
    }

    SECTION("non-finite inputs are reported symbolically")
    {
        CHECK(nanosecond_to_timecode_str(kNaN) == "NaN");
        CHECK(nanosecond_to_timecode_str(kInf) == "+Inf");
        CHECK(nanosecond_to_timecode_str(-kInf) == "-Inf");
    }
}

TEST_CASE("nanosecond_to_formatted_str dispatches on TimeFormat", "[utils][time]")
{
    const double t = 1500000000.0;  // 1.5 s
    CHECK(nanosecond_to_formatted_str(t, TimeFormat::kNanoseconds) == "1500000000");
    CHECK(nanosecond_to_formatted_str(t, TimeFormat::kMicroseconds) == "1500000.000");
    CHECK(nanosecond_to_formatted_str(t, TimeFormat::kMilliseconds) == "1500.000000");
    CHECK(nanosecond_to_formatted_str(t, TimeFormat::kSeconds) == "1.500000000");
    CHECK(nanosecond_to_formatted_str(t, TimeFormat::kTimecodeCondensed) == "01.500000000");
    CHECK(nanosecond_to_formatted_str(t, TimeFormat::kTimecode) == "00:00:01.500000000");
}

TEST_CASE("nanosecond_str_to_formatted_str parses, offsets and falls back",
          "[utils][time]")
{
    // Parse, subtract the offset, then format.
    CHECK(nanosecond_str_to_formatted_str("1500000000", 500000000.0,
                                          TimeFormat::kSeconds, false) == "1.000000000");

    SECTION("empty input is returned verbatim")
    {
        CHECK(nanosecond_str_to_formatted_str("", 0.0, TimeFormat::kSeconds, false).empty());
    }

    SECTION("unparseable input is returned verbatim")
    {
        CHECK(nanosecond_str_to_formatted_str("not-a-number", 0.0, TimeFormat::kSeconds,
                                              false) == "not-a-number");
    }
}

TEST_CASE("timeformat_sufix returns the canonical suffix per format", "[utils][time]")
{
    CHECK(timeformat_sufix(TimeFormat::kNanoseconds) == "ns");
    CHECK(timeformat_sufix(TimeFormat::kMicroseconds) == "us");
    CHECK(timeformat_sufix(TimeFormat::kMilliseconds) == "ms");
    CHECK(timeformat_sufix(TimeFormat::kSeconds) == "s");
    CHECK(timeformat_sufix(TimeFormat::kTimecode) == "timecode");
    CHECK(timeformat_sufix(TimeFormat::kTimecodeCondensed) == "timecode");
}

TEST_CASE("calculate_nice_interval snaps to 1/2/4/5/10 * scale", "[utils][axis]")
{
    using Catch::Matchers::WithinRel;

    SECTION("guards against non-positive inputs")
    {
        CHECK(calculate_nice_interval(0.0, 10) == 1.0);
        CHECK(calculate_nice_interval(-5.0, 10) == 1.0);
        CHECK(calculate_nice_interval(100.0, 0) == 1.0);
    }

    SECTION("an ideal of 1.0 stays 1.0")
    {
        // view_range / divisions = 10 / 10 = 1.0 -> multiplier 1, scale 1
        CHECK_THAT(calculate_nice_interval(10.0, 10), WithinRel(1.0, 1e-9));
    }

    SECTION("rounds up to the nearest nice multiplier")
    {
        // ideal = 1.2 -> < 1.5 -> 1.0
        CHECK_THAT(calculate_nice_interval(12.0, 10), WithinRel(1.0, 1e-9));
        // ideal = 1.8 -> < 3.0 -> 2.0
        CHECK_THAT(calculate_nice_interval(18.0, 10), WithinRel(2.0, 1e-9));
        // ideal = 3.5 -> < 4.5 -> 4.0
        CHECK_THAT(calculate_nice_interval(35.0, 10), WithinRel(4.0, 1e-9));
        // ideal = 6.0 -> < 7.0 -> 5.0
        CHECK_THAT(calculate_nice_interval(60.0, 10), WithinRel(5.0, 1e-9));
        // ideal = 8.0 -> else -> 10.0
        CHECK_THAT(calculate_nice_interval(80.0, 10), WithinRel(10.0, 1e-9));
    }

    SECTION("scales with the order of magnitude")
    {
        // ideal = 1200 -> 1000
        CHECK_THAT(calculate_nice_interval(12000.0, 10), WithinRel(1000.0, 1e-9));
    }
}

TEST_CASE("fit_graph_axis_interval returns a usable interval and count", "[utils][axis]")
{
    SECTION("zero label width yields a single interval")
    {
        FittedGraphAxisInterval fit =
            fit_graph_axis_interval(1000.0, 500.0f, 0.0f, false, 1);
        // interval_count starts at 0, clamped to 1, then +2 pad amount is added.
        CHECK(fit.interval_count == 3);
        CHECK(fit.interval_ns > 0.0);
    }

    SECTION("nice interval is non-zero for a realistic axis")
    {
        FittedGraphAxisInterval fit =
            fit_graph_axis_interval(100000.0, 800.0f, 80.0f, true, 2);
        CHECK(fit.interval_ns > 0.0);
        CHECK(fit.interval_count >= 2);
    }
}

TEST_CASE("calculate_adaptive_view_range centers and pads the item", "[utils][view]")
{
    using Catch::Matchers::WithinRel;

    SECTION("tiny items expand to the minimum visible span and stay centered")
    {
        // duration 0 -> guarded to 1 ns -> span clamped to 100us minimum.
        ViewRangeNS range = calculate_adaptive_view_range(/*start*/ 1000.0, /*dur*/ 0.0);
        const double span = range.end_ns - range.start_ns;
        CHECK_THAT(span, WithinRel(100.0 * 1000.0, 1e-6));  // 100 us in ns
        const double center = (range.start_ns + range.end_ns) * 0.5;
        // center ~= start + d*0.5, with d guarded to 1.0
        CHECK_THAT(center, WithinRel(1000.0 + 0.5, 1e-9));
    }

    SECTION("large items use modest padding (span = duration * 3)")
    {
        // duration >= 5 ms uses pad_long = 1.0 -> span = d * (1 + 2) = 3d.
        const double dur  = 10.0 * 1000.0 * 1000.0;  // 10 ms
        ViewRangeNS  range = calculate_adaptive_view_range(0.0, dur);
        const double span = range.end_ns - range.start_ns;
        CHECK_THAT(span, WithinRel(dur * 3.0, 1e-9));
    }

    SECTION("view span grows monotonically with duration")
    {
        const double small =
            calculate_adaptive_view_range(0.0, 1.0 * 1000.0).end_ns -
            calculate_adaptive_view_range(0.0, 1.0 * 1000.0).start_ns;
        const double large =
            calculate_adaptive_view_range(0.0, 20.0 * 1000.0 * 1000.0).end_ns -
            calculate_adaptive_view_range(0.0, 20.0 * 1000.0 * 1000.0).start_ns;
        CHECK(large > small);
    }
}

TEST_CASE("compact_number_format produces SI-style strings", "[utils][format]")
{
    SECTION("sub-thousand values keep adaptive precision")
    {
        CHECK(compact_number_format(0.0) == "0.00");
        CHECK(compact_number_format(5.0) == "5.00");
        CHECK(compact_number_format(42.0) == "42.0");
        CHECK(compact_number_format(123.0) == "123");
    }

    SECTION("thousands and millions get suffixes")
    {
        CHECK(compact_number_format(1000.0) == "1.00K");
        CHECK(compact_number_format(1500.0) == "1.50K");
        CHECK(compact_number_format(2000000.0) == "2.00M");
        CHECK(compact_number_format(3000000000.0) == "3.00B");
    }

    SECTION("negative values keep the sign")
    {
        CHECK(compact_number_format(-1500.0) == "-1.50K");
    }

    SECTION("non-finite values are symbolic")
    {
        CHECK(compact_number_format(kNaN) == "NaN");
        CHECK(compact_number_format(kInf) == "+Inf");
        CHECK(compact_number_format(-kInf) == "-Inf");
    }
}

TEST_CASE("get_executable_name strips the directory on either separator",
          "[utils][path]")
{
    CHECK(get_executable_name("/usr/local/bin/roc-optiq") == "roc-optiq");
    CHECK(get_executable_name("C:\\Program Files\\roc-optiq.exe") == "roc-optiq.exe");
    CHECK(get_executable_name("roc-optiq") == "roc-optiq");
    CHECK(get_executable_name("").empty());
    // Mixed separators: the last separator of either kind wins.
    CHECK(get_executable_name("/a/b\\c.exe") == "c.exe");
}

TEST_CASE("CircularBuffer respects capacity and FIFO eviction", "[utils][container]")
{
    SECTION("push evicts the oldest once full")
    {
        CircularBuffer<int> buf(3);
        CHECK(buf.IsEmpty());
        CHECK_FALSE(buf.IsFull());

        buf.Push(1);
        buf.Push(2);
        buf.Push(3);
        CHECK(buf.IsFull());
        CHECK(buf.GetContainer().size() == 3);
        CHECK(buf.GetContainer().front() == 1);

        // Overflow drops the oldest (1) and appends 4.
        buf.Push(4);
        CHECK(buf.GetContainer().size() == 3);
        CHECK(buf.GetContainer().front() == 2);
        CHECK(buf.GetContainer().back() == 4);
    }

    SECTION("pop on empty is a no-op")
    {
        CircularBuffer<int> buf(2);
        buf.Pop();  // must not crash or throw
        CHECK(buf.IsEmpty());
    }

    SECTION("resize down trims the oldest entries")
    {
        CircularBuffer<int> buf(4);
        buf.Push(10);
        buf.Push(20);
        buf.Push(30);
        buf.Resize(2);
        CHECK(buf.GetContainer().size() == 2);
        CHECK(buf.GetContainer().front() == 20);
        CHECK(buf.GetContainer().back() == 30);
        CHECK(buf.IsFull());
    }

    SECTION("clear empties the buffer")
    {
        CircularBuffer<int> buf(2);
        buf.Push(1);
        buf.Clear();
        CHECK(buf.IsEmpty());
    }

    SECTION("iteration visits items oldest-to-newest")
    {
        CircularBuffer<int> buf(3);
        buf.Push(7);
        buf.Push(8);
        buf.Push(9);
        int sum = 0;
        for(int v : buf)
        {
            sum += v;
        }
        CHECK(sum == 24);
    }
}
