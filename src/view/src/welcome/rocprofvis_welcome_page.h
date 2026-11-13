// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/rocprofvis_flex_container.h"

#include <cstddef>
#include <functional>
#include <string>

namespace RocProfVis
{
namespace View
{

// Landing page shown when no traces are open. Renders the hero text and then a
// FlexContainer laid out as two columns: the left holds the Start and Recent
// card with the Tips & tricks card under it, the right holds Resources. Side by
// side, the shorter column's last card is stretched so both end level. The
// layout collapses to a single stacked column when the window is narrow.
// File-open routing stays in the owner (AppWindow); WelcomePage just invokes
// the callbacks below.
class WelcomePage
{
public:
    using OpenFileFn       = std::function<void()>;
    using OpenRecentFileFn = std::function<void(const std::string& file_path)>;

    WelcomePage(OpenFileFn on_open_file, OpenRecentFileFn on_open_recent);

    void Render();

private:
    void RenderHeader();
    void RenderLeftCard();       // Start + Recent box, then the tips box
    void RenderResourcesCard();  // Resources box
    // Tips & tricks box, stretched by extra_height to end level with Resources.
    void RenderTipsCard(float extra_height);
    // Moves to the next or previous tip that can be shown, and records where
    // the next launch picks up.
    void StepTip(bool forward);

    OpenFileFn       m_on_open_file;
    OpenRecentFileFn m_on_open_recent;

    // Two-column body layout; left = Start + Recent and Tips, right = Resources.
    FlexContainer m_flex;
    // The tip on screen, an index into the page's tip list.
    size_t m_tip_index;
    // Each column's height last frame without the stretch that levels them:
    // [0] is the left column, [1] is Resources.
    float m_column_heights[2];
    // The stretch each column was given last frame. An auto-resizing card is
    // sized from the frame before's content, so that is the stretch its
    // measured height includes.
    float m_column_stretch[2];
    // Whether the columns sit side by side, the only time they are levelled.
    bool m_side_by_side;
    // Set while rendering the Recent list, acted on after the children are torn
    // down (opening a file can immediately replace this view).
    std::string m_recent_file_to_open;
};

}  // namespace View
}  // namespace RocProfVis
