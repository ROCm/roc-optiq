// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "json.h"
#include "widgets/rocprofvis_widget.h"
#include <string>
#include <memory>

namespace RocProfVis
{
namespace View
{

class CommandExecutionDialog;

/// Displays a rocpd AI analysis JSON file (produced by `rocpd analyze --format json`)
/// as a modern, standalone dashboard window with light/dark theme support.
class AiAnalysisView : public RocWidget
{
public:
    AiAnalysisView();
    void Render() override;

    /// Load and parse a rocpd analysis JSON file.  May be called from the file-dialog callback.
    void LoadFile(const std::string& path);

    /// Show the AI Analysis window
    void Show() { m_show_window = true; }

    /// Check if window is open
    bool IsOpen() const { return m_show_window; }

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

    // Card-based rendering helpers
    enum class CardStatus { Success, Warning, Critical, Info };
    void BeginCard(const char* title, CardStatus status, const char* icon = nullptr);
    void EndCard();
    void RenderMetricPill(const char* label, const char* value, ImVec4 color);
    void RenderProgressChart(const char* label, float percentage, ImVec4 color, const char* tooltip = nullptr);

    void ExecuteRecommendationCommand(const std::string& command, int recommendation_index);
    std::string ParseToolArgsFromCommand(const std::string& command);

    static std::string FormatNs(long long ns);
    static std::string FormatBytes(double bytes);
    ImVec4 PriorityColor(const std::string& priority);

    bool        m_loaded;
    bool        m_use_dark_theme;
    bool        m_show_window;
    std::string m_file_path;
    std::string m_load_error;
    jt::Json    m_data;

    // Expandable section states
    std::map<std::string, bool> m_section_expanded;
    std::string m_current_card_title;  // Track current card for EndCard

    std::shared_ptr<CommandExecutionDialog> m_command_execution_dialog;

    // Theme colors
    struct ThemeColors {
        ImVec4 background;
        ImVec4 card_bg;
        ImVec4 text_primary;
        ImVec4 text_secondary;
        ImVec4 border;
        ImVec4 success;
        ImVec4 warning;
        ImVec4 critical;
        ImVec4 info;
    };

    ThemeColors GetCurrentTheme() const;
};

}  // namespace View
}  // namespace RocProfVis
