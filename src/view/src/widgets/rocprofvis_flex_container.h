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
    std::string                id;
    std::shared_ptr<RocWidget> widget;
    float                      min_width = 500.0f;
    float                      height    = 0.0f;
    float                      flex_grow = 1.0f;
};

class FlexContainer : public RocWidget
{
public:
    void      Render() override;
    FlexItem* GetItem(const std::string& id);

    std::vector<FlexItem> items;
    float                 gap            = 8.0f;
    float                 min_row_height = 0.0f;
};

}  // namespace View
}  // namespace RocProfVis
