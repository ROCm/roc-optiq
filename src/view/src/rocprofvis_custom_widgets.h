#pragma once
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include <imgui.h>
#include <string>
#include <functional>

namespace RocProfVis
{
namespace View
{

class RocProfVisCustomWidget
{
public:
    virtual ~RocProfVisCustomWidget() = default;
    static float WithPadding(float left, float right, float top, float bottom, float height,
                            const std::function<void()>& content);

    
};

}  // namespace View
}  // namespace RocProfVis
