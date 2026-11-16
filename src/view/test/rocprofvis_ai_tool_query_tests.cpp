// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "agenticprofiling/rocprofvis_ai_tool_query.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <vector>

using namespace RocProfVis::View;

namespace
{

// Tool arguments as the model sends them.
jt::Json
Args(const std::string& text)
{
    std::pair<jt::Json::Status, jt::Json> parsed = jt::Json::parse(text);
    REQUIRE(parsed.first == jt::Json::success);
    return parsed.second;
}

// The filter expression built from one set of arguments, and the error it gave.
std::string
Filter(const std::string& args, std::string& error)
{
    error.clear();
    return BuildAssistantFilterExpression(Args(args), error);
}

}  // namespace

TEST_CASE("No filters is an empty expression, not an error", "[assistant][query]")
{
    std::string error;
    CHECK(Filter("{}", error).empty());
    CHECK(error.empty());
}

TEST_CASE("Filters become one AND expression over quoted values", "[assistant][query]")
{
    std::string error;
    CHECK(Filter(R"({"filters":[{"column":"name","op":"=","value":"gemm"},)"
                 R"({"column":"duration","op":">=","value":1000}]})",
                 error) == "name = 'gemm' AND duration >= 1000");
    CHECK(error.empty());
    CHECK(Filter(R"({"filters":[{"column":"PID","op":"<>","value":7}]})", error) ==
          "PID != 7");
}

TEST_CASE("A value cannot end its literal and add SQL", "[assistant][query]")
{
    std::string error;
    CHECK(Filter(R"({"filters":[{"column":"name","value":"x' OR '1'='1"}]})", error) ==
          "name = 'x'' OR ''1''=''1'");
    // Control characters are dropped rather than passed through.
    CHECK(Filter("{\"filters\":[{\"column\":\"name\",\"value\":\"a\\nb\"}]}", error) ==
          "name = 'ab'");
}

TEST_CASE("contains and starts_with look for % and _ literally", "[assistant][query]")
{
    std::string error;
    CHECK(Filter(R"({"filters":[{"column":"name","op":"contains","value":"50%_done"}]})",
                 error) == R"(name LIKE '%50\%\_done%' ESCAPE '\')");
    CHECK(Filter(R"({"filters":[{"column":"name","op":"starts_with","value":"hip"}]})",
                 error) == R"(name LIKE 'hip%' ESCAPE '\')");
}

TEST_CASE("Malformed filters are refused, not dropped", "[assistant][query]")
{
    std::string error;
    CHECK(Filter(R"({"filters":{"column":"name"}})", error).empty());
    CHECK_FALSE(error.empty());

    CHECK(Filter(R"({"filters":["name"]})", error).empty());
    CHECK_FALSE(error.empty());

    // A SQL column name rather than one the rows carry.
    CHECK(Filter(R"({"filters":[{"column":"nodeId","value":1}]})", error).empty());
    CHECK(error.find("Unknown filter column") != std::string::npos);

    CHECK(Filter(R"({"filters":[{"column":"name","op":"like","value":"x"}]})", error)
              .empty());
    CHECK(error.find("Unknown filter op") != std::string::npos);

    CHECK(Filter(R"({"filters":[{"column":"name"}]})", error).empty());
    CHECK(error.find("missing value") != std::string::npos);

    CHECK(Filter(R"({"filters":[{"column":"name","value":[1]}]})", error).empty());
    CHECK_FALSE(error.empty());
}

TEST_CASE("One call carries a bounded number of filters", "[assistant][query]")
{
    std::string entries;
    for(size_t i = 0; i <= ASSISTANT_MAX_FILTERS; i++)
    {
        entries += (i > 0 ? "," : "") + std::string(R"({"column":"id","value":1})");
    }
    std::string error;
    CHECK(Filter("{\"filters\":[" + entries + "]}", error).empty());
    CHECK(error.find("Too many filters") != std::string::npos);
}

TEST_CASE("group_by, sort_order and filter columns are read from the arguments",
          "[assistant][query]")
{
    std::string error;
    CHECK(AssistantGroupByFromArgs(Args(R"({"group_by":"name"})"), error) == "name");
    CHECK(error.empty());
    CHECK(AssistantGroupByFromArgs(Args(R"({"group_by":"nodeId"})"), error).empty());
    CHECK_FALSE(error.empty());

    CHECK(AssistantSortOrderFromArgs(Args(R"({"sort_order":"ASC"})"),
                                     kRPVControllerSortOrderDescending) ==
          kRPVControllerSortOrderAscending);
    CHECK(AssistantSortOrderFromArgs(Args("{}"), kRPVControllerSortOrderDescending) ==
          kRPVControllerSortOrderDescending);

    const std::vector<std::string> expected = { "name", "PID" };
    CHECK(AssistantFilterColumns(
              Args(R"({"filters":[{"column":"name"},{"column":"PID"}]})")) == expected);
}
