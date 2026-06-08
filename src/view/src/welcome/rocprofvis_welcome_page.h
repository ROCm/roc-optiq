// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/rocprofvis_flex_container.h"

#include <functional>
#include <string>

namespace RocProfVis
{
namespace View
{

// Landing page shown when no traces are open. Renders the hero text and then a
// FlexContainer laid out as two cards: the left card holds the Start and Recent
// sections, the right card holds Resources. The layout collapses to a single
// stacked column when the window is narrow. File-open routing stays in the
// owner (AppWindow); WelcomePage just invokes the callbacks below.
class WelcomePage
{
public:
    using OpenFileFn       = std::function<void()>;
    using OpenRecentFileFn = std::function<void(const std::string& file_path)>;

    WelcomePage(OpenFileFn on_open_file, OpenRecentFileFn on_open_recent);

    void Render();

private:
    void RenderHeader();
    void RenderLeftCard();       // Start + Recent sections in a single box
    void RenderResourcesCard();  // Resources box

    OpenFileFn       m_on_open_file;
    OpenRecentFileFn m_on_open_recent;

    // Two-card body layout; left = Start + Recent, right = Resources.
    FlexContainer m_flex;
    // Set while rendering the Recent list, acted on after the children are torn
    // down (opening a file can immediately replace this view).
    std::string m_recent_file_to_open;
};

}  // namespace View
}  // namespace RocProfVis
