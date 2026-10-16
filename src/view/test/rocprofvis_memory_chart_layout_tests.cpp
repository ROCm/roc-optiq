// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "compute/rocprofvis_memory_chart_layouts_generated.h"
#include "model/compute/rocprofvis_memory_chart_model.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace RocProfVis::View;

namespace
{

bool
Parses(const std::string& json, std::string& error)
{
    MemChartLayout layout;
    return MemChartLayout::ParseFromString(json, layout, &error);
}

}  // namespace

TEST_CASE("Every embedded memory-chart layout parses and resolves", "[memory_chart]")
{
    for(const MemChartEmbeddedLayout& entry : kMemChartEmbeddedLayouts)
    {
        INFO("layout: " << entry.key);
        MemChartLayout layout;
        std::string    error;
        REQUIRE(MemChartLayout::ParseFromString(entry.json, layout, &error));
        CHECK(error.empty());
        REQUIRE_FALSE(layout.blocks.empty());
        for(const MemChartArrow& arrow : layout.arrows)
        {
            INFO("arrow: " << arrow.from << " -> " << arrow.to);
            CHECK(layout.FindBlock(arrow.from) != nullptr);
            CHECK(layout.FindBlock(arrow.to) != nullptr);
        }
    }
}

TEST_CASE("String block ids connect arrows, including nested blocks", "[memory_chart]")
{
    const std::string json = R"({
        "version": 1,
        "blocks": [
            { "id": "cu", "column": 0, "title": "CU" },
            { "id": "caches", "column": 1, "title": "Caches", "children": [
                { "id": "lds", "title": "LDS" }
            ]}
        ],
        "arrows": [ { "from": "cu", "to": "lds" } ]
    })";

    MemChartLayout layout;
    std::string    error;
    REQUIRE(MemChartLayout::ParseFromString(json, layout, &error));
    REQUIRE(layout.arrows.size() == 1);
    CHECK(layout.arrows[0].from == "cu");
    CHECK(layout.arrows[0].to == "lds");
    CHECK(layout.FindBlock("lds") != nullptr);
}

TEST_CASE("Invalid block ids are rejected", "[memory_chart]")
{
    std::string error;

    SECTION("numeric id")
    {
        CHECK_FALSE(Parses(R"({ "version": 1, "blocks": [ { "id": 1 } ] })", error));
        CHECK_FALSE(error.empty());
    }
    SECTION("missing id")
    {
        CHECK_FALSE(Parses(R"({ "version": 1, "blocks": [ { "title": "L2" } ] })", error));
        CHECK_FALSE(error.empty());
    }
    SECTION("empty id")
    {
        CHECK_FALSE(Parses(R"({ "version": 1, "blocks": [ { "id": "" } ] })", error));
        CHECK_FALSE(error.empty());
    }
    SECTION("duplicate id across a nested child")
    {
        CHECK_FALSE(Parses(R"({ "version": 1, "blocks": [
            { "id": "l2" },
            { "id": "group", "children": [ { "id": "l2" } ] }
        ] })",
                           error));
        CHECK(error.find("l2") != std::string::npos);
    }
}

TEST_CASE("Arrows must name existing blocks by string id", "[memory_chart]")
{
    std::string error;

    SECTION("unknown endpoint")
    {
        CHECK_FALSE(Parses(R"({ "version": 1,
            "blocks": [ { "id": "l2" } ],
            "arrows": [ { "from": "l2", "to": "hbm" } ] })",
                           error));
        CHECK(error.find("hbm") != std::string::npos);
    }
    SECTION("numeric endpoint")
    {
        CHECK_FALSE(Parses(R"({ "version": 1,
            "blocks": [ { "id": "l2" } ],
            "arrows": [ { "from": "l2", "to": 2 } ] })",
                           error));
        CHECK_FALSE(error.empty());
    }
    SECTION("missing endpoint")
    {
        CHECK_FALSE(Parses(R"({ "version": 1,
            "blocks": [ { "id": "l2" } ],
            "arrows": [ { "from": "l2" } ] })",
                           error));
        CHECK_FALSE(error.empty());
    }
}
