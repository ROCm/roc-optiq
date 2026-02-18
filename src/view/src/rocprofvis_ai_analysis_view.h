// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "json.h"
#include "widgets/rocprofvis_widget.h"
#include <string>

namespace RocProfVis
{
namespace View
{

/// Displays a rocpd AI analysis JSON file (produced by `rocpd analyze --format json`)
/// as a structured, interactive panel inside the Analysis View tab bar.
class AiAnalysisView : public RocWidget
{
public:
    AiAnalysisView();
    void Render() override;

    /// Load and parse a rocpd analysis JSON file.  May be called from the file-dialog callback.
    void LoadFile(const std::string& path);

private:
    void RenderLoadScreen();
    void RenderAnalysisContent();

    void RenderOverview();
    void RenderExecutionBreakdown();
    void RenderRecommendations();
    void RenderHotspots();
    void RenderMemoryAnalysis();
    void RenderHardwareCounters();
    void RenderWarnings();

    static std::string FormatNs(long long ns);
    static std::string FormatBytes(double bytes);
    static ImVec4      PriorityColor(const std::string& priority);

    bool        m_loaded;
    std::string m_file_path;
    std::string m_load_error;
    jt::Json    m_data;
};

}  // namespace View
}  // namespace RocProfVis
