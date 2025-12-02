// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_utils.h"
#include "rocprofvis_split_containers.h"
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

    void ClearTransient();
    void SetMaxMessageCount(int count);
    void ClearPersitent();
    
private:
    DebugWindow();
    static void Process(void* user_ptr, char const* log);

    void RenderNav();
    void RenderPersitent();
    void RenderTransient();

    static DebugWindow*         s_instance;
    std::vector<std::string>    m_transient_debug_messages;
    CircularBuffer<std::string> m_persitent_debug_messages;

    std::shared_ptr<VFixedContainer> m_main_container;
    std::shared_ptr<VSplitContainer> m_v_spilt_container;
    std::shared_ptr<HSplitContainer> m_h_spilt_container;

    std::string m_split_button_label;
    bool m_do_h_split;
};

}  // namespace View
}  // namespace RocProfVis
