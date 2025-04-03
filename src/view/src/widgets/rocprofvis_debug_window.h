// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_widget.h"
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

class DebugWindow : public RocWidget
{
public:
    static DebugWindow* GetInstance();

    virtual void Render();
    void         AddDebugMessage(const std::string& message);
    void Reset();

private:
    DebugWindow();

    static DebugWindow* s_instance;
    std::vector<std::string> m_debug_messages;
};

}  // namespace View
}  // namespace RocProfVis
