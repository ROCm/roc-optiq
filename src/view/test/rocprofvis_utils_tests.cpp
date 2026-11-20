// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_utils.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace RocProfVis::View;

TEST_CASE("Unit formatters print positive values unchanged", "[utils][time_format]")
{
    CHECK(nanosecond_to_str(1234567) == "1234567");
    CHECK(nanosecond_to_us_str(1234567) == "1234.567");
    CHECK(nanosecond_to_ms_str(1234567) == "1.234567");
    CHECK(nanosecond_to_s_str(1234567) == "0.001234567");
}

TEST_CASE("Unit formatters print negative values with a sign", "[utils][time_format]")
{
    CHECK(nanosecond_to_str(-1234567) == "-1234567");
    CHECK(nanosecond_to_us_str(-1234567) == "-1234.567");
    CHECK(nanosecond_to_ms_str(-1234567) == "-1.234567");
    CHECK(nanosecond_to_s_str(-1234567) == "-0.001234567");
    CHECK(nanosecond_to_s_str(-2500000000.0, true) == "-2.500000000 s");
    CHECK(nanosecond_to_formatted_str(-1500, TimeFormat::kMicroseconds) == "-1.500");
}

TEST_CASE("Unit formatters drop the sign when the value truncates to zero",
          "[utils][time_format]")
{
    CHECK(nanosecond_to_str(-0.5) == "0");
    CHECK(nanosecond_to_us_str(-0.0) == "0.000");
    CHECK(nanosecond_to_ms_str(-0.9) == "0.000000");
    CHECK(nanosecond_to_s_str(-0.5) == "0.000000000");
}

TEST_CASE("Unit formatters reject non-finite values", "[utils][time_format]")
{
    CHECK(nanosecond_to_str(std::nan("")) == "NaN");
    CHECK(nanosecond_to_us_str(-INFINITY) == "NaN");
    CHECK(nanosecond_to_ms_str(INFINITY) == "NaN");
    CHECK(nanosecond_to_s_str(std::nan("")) == "NaN");
}
