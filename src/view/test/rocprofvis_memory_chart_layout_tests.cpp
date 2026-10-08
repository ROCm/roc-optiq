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

TEST_CASE("metric_name is parsed and optional", "[memory_chart]")
{
    const std::string json = R"({
        "version": 1,
        "blocks": [
            { "id": "l2", "content": [
                { "metric": "3.1.52", "metric_name": "L2 Hit", "title": "Hit" },
                { "metric": "3.1.1", "title": "No Name" }
            ]}
        ],
        "arrows": [
            { "from": "l2", "to": "l2", "metric": "3.1.16", "metric_name": "Flat Read" },
            { "from": "l2", "to": "l2" }
        ]
    })";

    MemChartLayout layout;
    std::string    error;
    REQUIRE(MemChartLayout::ParseFromString(json, layout, &error));
    REQUIRE(layout.blocks[0].content.size() == 2);
    CHECK(layout.blocks[0].content[0].metric.name == "3.1.52");
    CHECK(layout.blocks[0].content[0].metric.metric_name == "L2 Hit");
    CHECK(layout.blocks[0].content[0].title == "Hit");
    CHECK(layout.blocks[0].content[1].metric.name == "3.1.1");
    CHECK(layout.blocks[0].content[1].metric.metric_name.empty());
    REQUIRE(layout.arrows.size() == 2);
    CHECK(layout.arrows[0].metric.metric_name == "Flat Read");
    CHECK_FALSE(layout.arrows[1].metric.valid);
    CHECK(layout.arrows[1].metric.metric_name.empty());
}

TEST_CASE("Remap replaces category-3 ids by metric name", "[memory_chart]")
{
    const std::string json = R"({
        "version": 1,
        "blocks": [
            { "id": "l2", "content": [
                { "metric": "3.1.1", "metric_name": "L2 Hit" },
                { "metric": "9.9.9", "metric_name": "Shared" },
                { "metric": "3.1.5", "metric_name": "" },
                { "metric": "3.1.6", "metric_name": "Missing" }
            ], "children": [
                { "id": "child", "content": [
                    { "metric": "3.1.7", "metric_name": "Dup" }
                ]}
            ]}
        ],
        "arrows": [
            { "from": "l2", "to": "child", "metric": "1.1.1", "metric_name": "L2 Hit" },
            { "from": "l2", "to": "child" }
        ]
    })";

    MemChartLayout layout;
    std::string    error;
    REQUIRE(MemChartLayout::ParseFromString(json, layout, &error));

    MemChartMetricNameIndex index;
    index.Add("L2 Hit", 3, 1, 52);
    index.Add("L2 Hit", 17, 1, 9);
    index.Add("Shared", 2, 1, 21);
    index.Add("Shared", 3, 1, 99);
    index.Add("Dup", 3, 1, 1);
    index.Add("Dup", 3, 1, 2);

    std::vector<MemChartRemapFailure> failures;
    layout.RemapMetricIds(index, failures);

    CHECK(layout.blocks[0].content[0].metric.name == "3.1.52");
    CHECK(layout.blocks[0].content[1].metric.name == "3.1.99");
    CHECK(layout.blocks[0].content[2].metric.name == "3.1.5");
    CHECK(layout.blocks[0].content[3].metric.name == "3.1.6");
    CHECK(layout.blocks[0].children[0].content[0].metric.name == "3.1.7");
    CHECK(layout.arrows[0].metric.name == "3.1.52");

    REQUIRE(failures.size() == 3);
    CHECK(failures[0].template_id == "3.1.5");
    CHECK(failures[0].status == MemChartRemapStatus::kEmptyName);
    CHECK(failures[1].template_id == "3.1.6");
    CHECK(failures[1].metric_name == "Missing");
    CHECK(failures[1].status == MemChartRemapStatus::kNoMatch);
    CHECK(failures[2].template_id == "3.1.7");
    CHECK(failures[2].status == MemChartRemapStatus::kAmbiguous);
    REQUIRE(failures[2].match_ids.size() == 2);
    CHECK(failures[2].match_ids[0] == "3.1.1");
    CHECK(failures[2].match_ids[1] == "3.1.2");
}
