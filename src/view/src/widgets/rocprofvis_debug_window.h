// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_split_containers.h"
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

// Developer-mode scratchpad for ad-hoc debug output (e.g. per-frame variable
// traces via AddDebugMessage). Application logs are shown in LogViewer instead.
class DebugWindow : public RocWidget
{
public:
    static DebugWindow* GetInstance();

    virtual void Render();
    void         AddDebugMessage(const std::string& message);

    void ClearTransient();

private:
    DebugWindow();

    void RenderNav();
    void RenderMessages();

    static DebugWindow*      s_instance;
    std::vector<std::string> m_transient_debug_messages;

    std::shared_ptr<VFixedContainer> m_main_container;
};

}  // namespace View
}  // namespace RocProfVis
