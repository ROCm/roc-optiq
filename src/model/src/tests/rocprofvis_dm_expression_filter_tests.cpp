// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_db_expression_filter.h"
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <unordered_map>

using RocProfVis::DataModel::FilterExpression;

static bool
matches(const std::string& filter, double value)
{
    std::unordered_map<std::string, FilterExpression::Value> row = { { "x", value } };
    return FilterExpression::Parse(filter).Evaluate(row);
}

TEST_CASE("Modulo with a whole-number divisor", "[expression_filter]")
{
    REQUIRE(matches("x % 3 == 1", 7.0));
    REQUIRE_FALSE(matches("x % 3 == 0", 7.0));
}

TEST_CASE("Modulo with a fractional divisor keeps the fraction", "[expression_filter]")
{
    REQUIRE(matches("x % 0.5 == 0", 3.0));
    REQUIRE(matches("x % 0.5 == 0.25", 3.25));
    REQUIRE(matches("x % 2 == 1.5", 7.5));
}

TEST_CASE("Modulo with negative operands follows the sign of the dividend",
          "[expression_filter]")
{
    REQUIRE(matches("x % 3 == -1", -7.0));
    REQUIRE(matches("x % -3 == 1", 7.0));
}

TEST_CASE("Modulo by zero evaluates to zero", "[expression_filter]")
{
    REQUIRE(matches("x % 0 == 0", 7.0));
}
