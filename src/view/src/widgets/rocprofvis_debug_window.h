// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "../rocprofvis_utils.h"
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
    void         AddPersitentDebugMessage(const std::string& message);

    void Reset();

private:
    DebugWindow();
    static void Process(void* user_ptr, char const* log);

    static DebugWindow*         s_instance;
    std::vector<std::string>    m_transient_debug_messages;
    CircularBuffer<std::string> m_persitent_debug_messages;
};

}  // namespace View
}  // namespace RocProfVis
