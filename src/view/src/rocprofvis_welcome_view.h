// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_root_view.h"
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

struct RecentFile
{
    std::string file_path;
    std::string file_name;
    std::string last_opened;  // Human-readable format
};

class WelcomeView : public RootView
{
public:
    WelcomeView();
    ~WelcomeView();

    void Update() override;
    void Render() override;

    std::shared_ptr<RocWidget> GetToolbar() override;

private:
    void RenderWelcomeContent();
    void RenderOpenTraceCard();
    void RenderRunProfilingCard();
    void RenderRecentFiles();
    void LoadRecentFiles();
    void ShowProfilingDialog();

    std::vector<RecentFile> m_recent_files;
    bool m_hover_open_trace = false;
    bool m_hover_run_profiling = false;
    bool m_show_profiling_dialog = false;
};

}  // namespace View
}  // namespace RocProfVis
