// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace RocProfVis
{
namespace View
{

// Relational memory-chart layout ("nodes + edges"), mirroring the shape stored
// in the compute_workload table: a set of block rows and arrow (relation) rows.
// Blocks are keyed by a unique string id (e.g. "l2", "data_fabric"), arrows name
// their endpoints by those ids, and both reference metrics by their full dotted
// id ("category.table.entry").

// Direction of an arrow relative to its declared (from -> to) endpoints.
enum class MemChartArrowDir : uint8_t
{
    kForward,   // from -> to
    kBackward,  // to -> from
    kBoth,      // bidirectional
};

// Theme-independent color slot for a content row or arrow. Resolved from the
// layout `category` (or inferred from the label). Mapped to an ImU32 via the
// chart palette, which is rebuilt on theme change.
enum class MemChartColorKind : uint8_t
{
    kNeutral = 0,
    kRead,
    kWrite,
    kAtomic,
    kUtil,
    kHit,
    kStall,
};

// Memory-chart metrics are category 3, so their dotted ids are "3.x.x".
constexpr uint32_t kMemChartMetricCategoryId = 3;

// A reference to a metric by its full dotted id ("category.table.entry", e.g.
// "3.1.0"), held in `name` when `valid` is true. `metric_name` is the database
// name used to find the real category-3 id when remapping is enabled.
struct MemChartMetricRef
{
    bool        valid = false;
    std::string name;
    std::string metric_name;
};

// Result of looking up one template metric by name.
enum class MemChartRemapStatus : uint8_t
{
    kReplaced = 0,  // `name` was replaced with the category-3 id.
    kEmptyName,     // The template metric has no metric_name.
    kNoMatch,       // No category-3 metric has that name.
    kAmbiguous,     // More than one category-3 metric has that name.
};

// A template metric whose id was left unchanged. `template_id` is the dotted id
// read from the template. `match_ids` lists the category-3 ids when ambiguous.
struct MemChartRemapFailure
{
    std::string              template_id;
    std::string              metric_name;
    MemChartRemapStatus      status = MemChartRemapStatus::kNoMatch;
    std::vector<std::string> match_ids;
};

// Category-3 metric names from the open workload. Entries in other categories
// are ignored, so a name that also exists outside category 3 still resolves to
// the single "3.x.x" row.
class MemChartMetricNameIndex
{
public:
    void Add(const std::string& name, uint32_t category_id, uint32_t table_id,
             uint32_t entry_id);

    // Clears `dotted_id` and `match_ids`. Returns kReplaced, kNoMatch, or
    // kAmbiguous. An empty name is the caller's concern (kEmptyName).
    MemChartRemapStatus Lookup(const std::string& metric_name, std::string& dotted_id,
                               std::vector<std::string>& match_ids) const;

private:
    std::unordered_map<std::string, std::vector<std::string>> m_by_name;
};

// One line inside a block. All fields optional: `title` overrides the metric's
// name, `unit` overrides the entry's unit suffix, and `category`
// (read/write/atomic/util/hit/stall/misc) sets the accent color (else inferred
// from the label).
struct MemChartContentItem
{
    MemChartMetricRef metric;
    std::string       title;
    std::string       category;
    std::string       unit;

    // Render cache: resolved label/value strings, refreshed on layout load and
    // on metric fetch (not recomputed per frame).
    std::string       cached_label;
    std::string       cached_value;
    MemChartColorKind cached_color_kind = MemChartColorKind::kNeutral;
    uint32_t          cached_color      = 0;  // Palette ImU32 for cached_color_kind.
};

struct MemChartBlock
{
    std::string                      id;           // Unique, non-empty; arrows reference blocks by it.
    int32_t                          column = 0;   // Meaningful for top-level blocks; propagated to children at layout.
    int32_t                          row    = 0;   // Grid row band: 0 = main row, <0 above it, >0 below it. Propagated to children at layout.
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
    std::string       from;      // Source block id.
    std::string       to;        // Destination block id.
    MemChartArrowDir  direction = MemChartArrowDir::kForward;
    MemChartMetricRef metric;
    std::string       title;     // Optional label override; else the metric name.
    std::string       category;  // read/write/atomic/... selects color; else inferred.

    // Render cache: resolved label/value strings, refreshed on layout load and
    // on metric fetch (not recomputed per frame).
    std::string       cached_label;
    std::string       cached_value;
    MemChartColorKind cached_color_kind = MemChartColorKind::kNeutral;
    uint32_t          cached_color      = 0;  // Palette ImU32 for cached_color_kind.

    // Endpoints resolved from `from`/`to` by the view on layout load. They point
    // into the view's own copy of the blocks and are rebuilt on every load, so
    // they must not be used from any other copy of the layout.
    const MemChartBlock* from_block = nullptr;
    const MemChartBlock* to_block   = nullptr;
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
    // on failure returns false and (when non-null) sets `error`. Fails when a
    // block's id is missing, not a string, or duplicated, or when an arrow's
    // `from`/`to` doesn't name an existing block.
    static bool ParseFromString(const std::string& json_text, MemChartLayout& out,
                                std::string* error);

    MemChartBlock*       FindBlock(const std::string& id);
    const MemChartBlock* FindBlock(const std::string& id) const;

    // Replace each metric's dotted id with the category-3 id for its
    // metric_name. Refs with no metric are skipped. Failures are appended to
    // `failures` and those template ids are left unchanged.
    void RemapMetricIds(const MemChartMetricNameIndex&     index,
                        std::vector<MemChartRemapFailure>& failures);
};

}  // namespace View
}  // namespace RocProfVis
