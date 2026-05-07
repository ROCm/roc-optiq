// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

struct FlexItem
{
    static constexpr float DEFAULT_MIN_WIDTH = 500.0f;
    static constexpr float DEFAULT_HEIGHT    = 0.0f;
    static constexpr float DEFAULT_FLEX_GROW = 1.0f;

    std::string                id;
    std::shared_ptr<RocWidget> widget;
    float                      min_width = DEFAULT_MIN_WIDTH;
    float                      height    = DEFAULT_HEIGHT;
    float                      flex_grow = DEFAULT_FLEX_GROW;
    bool                       full_row  = false;
};

class FlexContainer : public RocWidget
{
public:
    static constexpr float DEFAULT_GAP = 8.0f;

    void      Render() override;
    FlexItem* GetItem(const std::string& id);

    std::vector<FlexItem> items;
    float                 gap = DEFAULT_GAP;

    // When true, items render transparent (subcomponents inside one outer
    // card) and a thin separator line is drawn between adjacent items.
    // When false (default), each item renders as its own kBgPanel white card.
    bool subcomponent_layout = false;
};

}  // namespace View
}  // namespace RocProfVis
