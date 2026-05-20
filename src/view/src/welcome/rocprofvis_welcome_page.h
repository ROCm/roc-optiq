// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <functional>
#include <string>

namespace RocProfVis
{
namespace View
{

// Landing page shown when no traces are open. Renders the hero text, a Start
// tile (Open Trace action + drag-and-drop hint), a Recent files tile, and a
// Resources tile on top of a themed backdrop. File-open routing stays in the
// owner (AppWindow); WelcomePage just invokes the callbacks below.
class WelcomePage
{
public:
    using OpenFileFn       = std::function<void()>;
    using OpenRecentFileFn = std::function<void(const std::string& file_path)>;

    WelcomePage(OpenFileFn on_open_file, OpenRecentFileFn on_open_recent);

    void Render();

private:
    void RenderHeader(float content_width);
    void RenderStartTile();
    void RenderRecentTile(std::string& recent_file_to_open);
    void RenderResourcesTile();

    OpenFileFn       m_on_open_file;
    OpenRecentFileFn m_on_open_recent;
};

}  // namespace View
}  // namespace RocProfVis
