// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

// Relational memory-chart layout ("nodes + edges"), mirroring the shape stored
// in the compute_workload table: a set of block rows and arrow (relation) rows,
// both keyed by id and referencing metrics by their full dotted id
// ("category.table.entry").

// Direction of an arrow relative to its declared (from -> to) endpoints.
enum class MemChartArrowDir : uint8_t
{
    kForward,   // from -> to
    kBackward,  // to -> from
    kBoth,      // bidirectional
};

// A reference to a metric by its full dotted id ("category.table.entry", e.g.
// "3.1.0"), held in `name` when `valid` is true.
struct MemChartMetricRef
{
    bool        valid = false;
    std::string name;
};

// One line inside a block: a metric plus an optional label override. When no
// override is given the metric's own name is used; the unit comes from the
// metric entry. `category` (read/write/atomic/util/hit/stall/misc) selects the
// accent color; when empty the color is inferred from the label text.
struct MemChartContentItem
{
    MemChartMetricRef metric;
    std::string       title;
    std::string       category;
};

struct MemChartBlock
{
    uint32_t                         id     = 0;
    int32_t                          column = 0;   // Meaningful for top-level blocks; propagated to children at layout.
    int32_t                          order  = -1;  // Sort key within a column/parent; -1 = declaration order.
    std::string                      title;
    std::vector<MemChartContentItem> content;   // Leaf metric rows.
    std::vector<MemChartBlock>       children;  // Nested blocks: a block with children is a container box.

    // Geometry assigned during layout (local canvas space).
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;

    // X positions where arrows attach: the top-level ancestor's box edges, so a
    // connection to a nested block terminates at the outer box rather than
    // entering it. For a top-level block these are its own edges.
    float conn_left = 0.0f, conn_right = 0.0f;

    bool IsContainer() const { return !children.empty(); }

    float Right() const { return x + w; }
    float Bottom() const { return y + h; }
    float MidX() const { return x + (w * 0.5f); }
    float MidY() const { return y + (h * 0.5f); }
};

struct MemChartArrow
{
    uint32_t          from      = 0;
    uint32_t          to        = 0;
    MemChartArrowDir  direction = MemChartArrowDir::kForward;
    MemChartMetricRef metric;
    std::string       title;     // Optional label override; else the metric name.
    std::string       category;  // read/write/atomic/... selects color; else inferred.
};

// A titled box drawn around a container block's children. Populated during
// layout (one per container block).
struct MemChartGroupBox
{
    std::string title;
    float       x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
};

struct MemChartLayout
{
    int version = 1;

    std::vector<MemChartBlock> blocks;
    std::vector<MemChartArrow> arrows;

    // Parse a layout from JSON text. On success returns true and fills `out`;
    // on failure returns false and (when non-null) sets `error`.
    static bool ParseFromString(const std::string& json_text, MemChartLayout& out,
                                std::string* error);

    MemChartBlock*       FindBlock(uint32_t id);
    const MemChartBlock* FindBlock(uint32_t id) const;
};

}  // namespace View
}  // namespace RocProfVis
