// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/rocprofvis_widget.h"
#include <memory>

namespace RocProfVis
{
namespace View
{

class RootView : public RocWidget
{
public:
    RootView() {};
    virtual ~RootView() {};

    virtual std::shared_ptr<RocWidget> GetToolbar() {return nullptr;};

    virtual void RenderEditMenuOptions() {};
};

}
}
