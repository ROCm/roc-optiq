// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_memory_chart_model.h"

#include "json.h"

namespace RocProfVis
{
namespace View
{

namespace
{

std::string
ReadString(jt::Json& node, const char* key, const std::string& fallback = "")
{
    if(node.isObject() && node.contains(key))
    {
        jt::Json& value = node[key];
        if(value.isString())
        {
            return value.getString();
        }
    }
    return fallback;
}

int32_t
ReadInt(jt::Json& node, const char* key, int32_t fallback)
{
    if(node.isObject() && node.contains(key))
    {
        jt::Json& value = node[key];
        if(value.isNumber())
        {
            return static_cast<int32_t>(value.getNumber());
        }
    }
    return fallback;
}

uint32_t
ReadUint(jt::Json& node, const char* key, uint32_t fallback)
{
    if(node.isObject() && node.contains(key))
    {
        jt::Json& value = node[key];
        if(value.isNumber())
        {
            return static_cast<uint32_t>(value.getNumber());
        }
    }
    return fallback;
}

// A metric reference is the metric's full dotted id ("category.table.entry").
MemChartMetricRef
ParseMetricRef(jt::Json& value)
{
    MemChartMetricRef ref;
    if(value.isString())
    {
        ref.name  = value.getString();
        ref.valid = !ref.name.empty();
    }
    return ref;
}

MemChartMetricRef
ReadMetricRef(jt::Json& node, const char* key)
{
    if(node.isObject() && node.contains(key))
    {
        return ParseMetricRef(node[key]);
    }
    return MemChartMetricRef{};
}

MemChartArrowDir
ParseDirection(const std::string& text)
{
    if(text == "backward" || text == "2to1" || text == "reverse") return MemChartArrowDir::kBackward;
    if(text == "both" || text == "bi" || text == "bidirectional") return MemChartArrowDir::kBoth;
    return MemChartArrowDir::kForward;
}

// Content is an array whose elements are either a bare metric ref (a dotted-id
// string) or an object { "metric": <ref>, "title": <string> }.
MemChartContentItem
ParseContentItem(jt::Json& element)
{
    MemChartContentItem item;
    if(element.isObject())
    {
        item.metric   = ReadMetricRef(element, "metric");
        item.title    = ReadString(element, "title");
        item.category = ReadString(element, "category");
    }
    else
    {
        item.metric = ParseMetricRef(element);
    }
    return item;
}

MemChartBlock
ParseBlock(jt::Json& node)
{
    MemChartBlock block;
    block.id     = ReadUint(node, "id", 0);
    block.column = ReadInt(node, "column", 0);
    block.order  = ReadInt(node, "order", -1);
    block.title  = ReadString(node, "title");

    if(node.isObject() && node.contains("content"))
    {
        jt::Json& content = node["content"];
        if(content.isArray())
        {
            for(jt::Json& element : content.getArray())
            {
                block.content.push_back(ParseContentItem(element));
            }
        }
    }

    // Nested blocks: a block with children is a container box.
    if(node.isObject() && node.contains("children"))
    {
        jt::Json& children = node["children"];
        if(children.isArray())
        {
            for(jt::Json& child : children.getArray())
            {
                block.children.push_back(ParseBlock(child));
            }
        }
    }
    return block;
}

MemChartArrow
ParseArrow(jt::Json& node)
{
    MemChartArrow arrow;
    arrow.from      = ReadUint(node, "from", 0);
    arrow.to        = ReadUint(node, "to", 0);
    arrow.direction = ParseDirection(ReadString(node, "direction", "forward"));
    arrow.metric    = ReadMetricRef(node, "metric");
    arrow.title     = ReadString(node, "title");
    arrow.category  = ReadString(node, "category");
    return arrow;
}

}  // namespace

bool
MemChartLayout::ParseFromString(const std::string& json_text, MemChartLayout& out,
                                std::string* error)
{
    std::pair<jt::Json::Status, jt::Json> parsed = jt::Json::parse(json_text);
    if(parsed.first != jt::Json::success)
    {
        if(error)
        {
            *error = jt::Json::StatusToString(parsed.first);
        }
        return false;
    }

    jt::Json& root = parsed.second;
    if(!root.isObject())
    {
        if(error)
        {
            *error = "root is not an object";
        }
        return false;
    }

    MemChartLayout layout;
    layout.version = ReadInt(root, "version", 1);

    if(root.contains("blocks"))
    {
        jt::Json& blocks = root["blocks"];
        if(blocks.isArray())
        {
            for(jt::Json& block_json : blocks.getArray())
            {
                layout.blocks.push_back(ParseBlock(block_json));
            }
        }
    }

    if(root.contains("arrows"))
    {
        jt::Json& arrows = root["arrows"];
        if(arrows.isArray())
        {
            for(jt::Json& arrow_json : arrows.getArray())
            {
                layout.arrows.push_back(ParseArrow(arrow_json));
            }
        }
    }

    if(layout.blocks.empty())
    {
        if(error)
        {
            *error = "no blocks defined";
        }
        return false;
    }

    out = std::move(layout);
    return true;
}

namespace
{
MemChartBlock*
FindInBlocks(std::vector<MemChartBlock>& blocks, uint32_t id)
{
    for(MemChartBlock& block : blocks)
    {
        if(block.id == id)
        {
            return &block;
        }
        MemChartBlock* found = FindInBlocks(block.children, id);
        if(found)
        {
            return found;
        }
    }
    return nullptr;
}
}  // namespace

MemChartBlock*
MemChartLayout::FindBlock(uint32_t id)
{
    return FindInBlocks(blocks, id);
}

const MemChartBlock*
MemChartLayout::FindBlock(uint32_t id) const
{
    return FindInBlocks(const_cast<std::vector<MemChartBlock>&>(blocks), id);
}

}  // namespace View
}  // namespace RocProfVis
