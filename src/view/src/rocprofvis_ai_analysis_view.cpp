// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ai_analysis_view.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_command_execution_dialog.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_dialog.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace RocProfVis
{
namespace View
{

// ═══════════════════════════════════════════════════════════════════════════
// CONSTRUCTOR & THEME
// ═══════════════════════════════════════════════════════════════════════════

AiAnalysisView::AiAnalysisView()
: m_loaded(false)
, m_use_dark_theme(true)  // Kept for compatibility, but GetCurrentTheme() uses SettingsManager
, m_show_window(false)
{
    m_widget_name = GenUniqueName("AI Performance Insights");

    // Initialize all sections - use actual card titles
    m_section_expanded["Analysis Summary"] = true;  // Open by default
    m_section_expanded["Execution Time Breakdown"] = false;
    m_section_expanded["Optimization Recommendations"] = false;
    m_section_expanded["Top Kernel Hotspots"] = false;
    m_section_expanded["Memory Transfer Analysis"] = false;
    m_section_expanded["Hardware Performance Metrics"] = false;
    m_section_expanded["Warnings & Errors"] = false;
}

AiAnalysisView::ThemeColors
AiAnalysisView::GetCurrentTheme() const
{
    ThemeColors theme;

    // Sync with SettingsManager theme
    bool use_dark = SettingsManager::GetInstance().GetUserSettings().display_settings.use_dark_mode;

    if(use_dark)
    {
        // Dark theme - NVIDIA Nsight inspired with excellent readability
        theme.background     = ImVec4(0.11f, 0.11f, 0.13f, 1.0f);  // Dark charcoal
        theme.card_bg        = ImVec4(0.16f, 0.16f, 0.19f, 1.0f);  // Lighter card background
        theme.text_primary   = ImVec4(0.95f, 0.95f, 0.96f, 1.0f);  // High contrast white
        theme.text_secondary = ImVec4(0.70f, 0.70f, 0.73f, 1.0f);  // Medium gray
        theme.border         = ImVec4(0.30f, 0.30f, 0.35f, 1.0f);  // Subtle borders
        theme.success        = ImVec4(0.20f, 0.80f, 0.40f, 1.0f);  // Green
        theme.warning        = ImVec4(1.00f, 0.70f, 0.00f, 1.0f);  // Orange
        theme.critical       = ImVec4(0.95f, 0.30f, 0.30f, 1.0f);  // Red
        theme.info           = ImVec4(0.30f, 0.70f, 1.00f, 1.0f);  // Blue
    }
    else
    {
        // Light theme - Intel VTune inspired with clean presentation
        theme.background     = ImVec4(0.95f, 0.95f, 0.96f, 1.0f);  // Light gray background
        theme.card_bg        = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);  // Pure white cards
        theme.text_primary   = ImVec4(0.13f, 0.13f, 0.13f, 1.0f);  // Dark text
        theme.text_secondary = ImVec4(0.40f, 0.40f, 0.40f, 1.0f);  // Medium gray
        theme.border         = ImVec4(0.82f, 0.82f, 0.82f, 1.0f);  // Light borders
        theme.success        = ImVec4(0.13f, 0.59f, 0.25f, 1.0f);  // Green
        theme.warning        = ImVec4(0.95f, 0.55f, 0.00f, 1.0f);  // Orange
        theme.critical       = ImVec4(0.85f, 0.13f, 0.13f, 1.0f);  // Red
        theme.info           = ImVec4(0.13f, 0.50f, 0.85f, 1.0f);  // Blue
    }

    return theme;
}

// ═══════════════════════════════════════════════════════════════════════════
// FILE LOADING
// ═══════════════════════════════════════════════════════════════════════════

void
AiAnalysisView::LoadFile(const std::string& path)
{
    m_load_error.clear();
    m_loaded = false;

    std::ifstream file(path);
    if(!file.is_open())
    {
        m_load_error = "Cannot open file: " + path;
        return;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    auto parsed = jt::Json::parse(content);
    if(parsed.first != jt::Json::success)
    {
        m_load_error = "Failed to parse JSON: " + path;
        return;
    }

    if(!parsed.second.isObject() || !parsed.second["schema_version"].isString())
    {
        m_load_error = "Not a valid rocpd analysis file (missing schema_version).";
        return;
    }

    m_data      = parsed.second;
    m_file_path = path;
    m_loaded    = true;
}

// ═══════════════════════════════════════════════════════════════════════════
// FORMATTING HELPERS
// ═══════════════════════════════════════════════════════════════════════════

std::string
AiAnalysisView::FormatNs(long long ns)
{
    if(ns < 0) ns = 0;
    if(ns < 1000LL) return std::to_string(ns) + " ns";
    if(ns < 1000000LL) return std::to_string(ns / 1000) + " us";
    if(ns < 1000000000LL) return std::to_string(ns / 1000000) + " ms";
    return std::to_string(ns / 1000000000LL) + " s";
}

std::string
AiAnalysisView::FormatBytes(double bytes)
{
    if(bytes < 1024.0) return std::to_string((int)bytes) + " B";
    if(bytes < 1024.0 * 1024.0) return std::to_string((int)(bytes / 1024.0)) + " KB";
    if(bytes < 1024.0 * 1024.0 * 1024.0)
        return std::to_string((int)(bytes / 1048576.0)) + " MB";
    return std::to_string((int)(bytes / 1073741824.0)) + " GB";
}

ImVec4
AiAnalysisView::PriorityColor(const std::string& priority)
{
    auto theme = GetCurrentTheme();
    if(priority == "HIGH") return theme.critical;
    if(priority == "MEDIUM") return theme.warning;
    if(priority == "LOW") return theme.info;
    return theme.text_secondary;
}

// ═══════════════════════════════════════════════════════════════════════════
// MAIN RENDER
// ═══════════════════════════════════════════════════════════════════════════

void
AiAnalysisView::Render()
{
    if(!m_show_window) return;

    // Standard window size
    ImGui::SetNextWindowSize(ImVec2(900, 700), ImGuiCond_FirstUseEver);

    // Center the window on first appearance
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_None;

    if(ImGui::Begin("AI Performance Insights", &m_show_window, window_flags))
    {
        if(!m_loaded)
            RenderLoadScreen();
        else
            RenderAnalysisContent();
    }
    ImGui::End();

    // Command execution dialog
    if(m_command_execution_dialog)
    {
        m_command_execution_dialog->Render();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// LOAD SCREEN
// ═══════════════════════════════════════════════════════════════════════════

void
AiAnalysisView::RenderLoadScreen()
{
    auto theme = GetCurrentTheme();

    const float avail_w = ImGui::GetContentRegionAvail().x;
    const float avail_h = ImGui::GetContentRegionAvail().y;

    // Center content vertically
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + avail_h * 0.3f);

    // Title - centered
    ImGui::SetCursorPosX((avail_w - ImGui::CalcTextSize("AI Performance Insights").x * 1.3f) * 0.5f);
    ImGui::SetWindowFontScale(1.3f);
    ImGui::TextUnformatted("AI Performance Insights");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Spacing();

    // Subtitle - centered
    const char* sub = "Load analysis JSON to view insights";
    ImGui::SetCursorPosX((avail_w - ImGui::CalcTextSize(sub).x) * 0.5f);
    ImGui::TextDisabled("%s", sub);

    // Error message
    if(!m_load_error.empty())
    {
        ImGui::Spacing();
        ImGui::SetCursorPosX((avail_w - ImGui::CalcTextSize(m_load_error.c_str()).x) * 0.5f);
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", m_load_error.c_str());
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Browse button - centered
    const float btn_w = 200.0f;
    ImGui::SetCursorPosX((avail_w - btn_w) * 0.5f);

    if(ImGui::Button("Open Analysis JSON", ImVec2(btn_w, 0)))
    {
        FileFilter filter;
        filter.m_name       = "JSON Analysis Files";
        filter.m_extensions = {"json"};
        AppWindow::GetInstance()->ShowOpenFileDialog(
            "Open rocpd Analysis JSON", {filter}, "",
            [this](std::string path) {
                if(!path.empty()) LoadFile(path);
            });
    }

    // Hint
    ImGui::Spacing();
    ImGui::Spacing();

    const char* hint = "Generate with: rocpd analyze -i trace.db --format json -o analysis.json";
    ImGui::SetCursorPosX((avail_w - ImGui::CalcTextSize(hint).x) * 0.5f);
    ImGui::TextDisabled("%s", hint);
}

// ═══════════════════════════════════════════════════════════════════════════
// MAIN CONTENT
// ═══════════════════════════════════════════════════════════════════════════

void
AiAnalysisView::RenderAnalysisContent()
{
    // ===== TOOLBAR =====
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("File:");
    ImGui::SameLine();
    ImGui::TextUnformatted(m_file_path.c_str());

    // Right-aligned load button
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 80.0f);
    if(ImGui::Button("Load...", ImVec2(80.0f, 0)))
    {
        FileFilter filter;
        filter.m_name       = "JSON Analysis Files";
        filter.m_extensions = {"json"};
        AppWindow::GetInstance()->ShowOpenFileDialog(
            "Open rocpd Analysis JSON", {filter}, "",
            [this](std::string path) {
                if(!path.empty()) LoadFile(path);
            });
    }

    ImGui::Separator();
    ImGui::Spacing();

    // ===== SCROLLABLE CONTENT =====
    ImGui::BeginChild("##content", ImVec2(0, 0), ImGuiChildFlags_None);

    RenderOverview();
    RenderExecutionBreakdown();
    RenderRecommendations();
    RenderHotspots();
    RenderMemoryAnalysis();
    RenderHardwareCounters();
    RenderWarnings();

    ImGui::EndChild();
}

// ═══════════════════════════════════════════════════════════════════════════
// SECTION HELPERS (using CollapsingHeader - standard ImGui pattern)
// ═══════════════════════════════════════════════════════════════════════════

void
AiAnalysisView::BeginCard(const char* title, CardStatus status, const char* icon)
{
    auto theme = GetCurrentTheme();
    m_current_card_title = title;

    // Status color for the header
    ImVec4 status_color;
    switch(status)
    {
        case CardStatus::Success: status_color = theme.success; break;
        case CardStatus::Warning: status_color = theme.warning; break;
        case CardStatus::Critical: status_color = theme.critical; break;
        case CardStatus::Info:
        default: status_color = theme.info; break;
    }

    // Style the collapsing header
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(status_color.x * 0.3f, status_color.y * 0.3f, status_color.z * 0.3f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(status_color.x * 0.4f, status_color.y * 0.4f, status_color.z * 0.4f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(status_color.x * 0.5f, status_color.y * 0.5f, status_color.z * 0.5f, 0.7f));

    // Use CollapsingHeader - the standard ImGui collapsible section
    bool& is_open = m_section_expanded[title];
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_None;
    if(is_open)
        flags |= ImGuiTreeNodeFlags_DefaultOpen;

    is_open = ImGui::CollapsingHeader(title, flags);

    ImGui::PopStyleColor(3);

    if(is_open)
    {
        ImGui::Indent();
    }
}

void
AiAnalysisView::EndCard()
{
    if(m_section_expanded[m_current_card_title])
    {
        ImGui::Unindent();
    }
    ImGui::Spacing();
}

void
AiAnalysisView::RenderMetricPill(const char* label, const char* value, ImVec4 color)
{
    auto theme = GetCurrentTheme();

    // Simple inline metric display
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_secondary);
    ImGui::Text("%s:", label);
    ImGui::PopStyleColor();
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::Text("%s", value);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::Text(" "); // Spacer
    ImGui::SameLine();
}

void
AiAnalysisView::RenderProgressChart(const char* label, float percentage, ImVec4 color, const char* tooltip)
{
    auto theme = GetCurrentTheme();

    // Compact label
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_primary);
    ImGui::Text("%s", label);
    ImGui::PopStyleColor();

    if(tooltip && ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", tooltip);
    }

    ImGui::SameLine(200.0f);

    char overlay[64];
    std::snprintf(overlay, sizeof(overlay), "%.1f%%", percentage);

    // Check global theme preference
    bool use_dark = SettingsManager::GetInstance().GetUserSettings().display_settings.use_dark_mode;
    ImVec4 bar_bg = use_dark ? ImVec4(0.18f, 0.18f, 0.22f, 1.0f)
                             : ImVec4(0.87f, 0.87f, 0.87f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, bar_bg);
    ImGui::ProgressBar(percentage / 100.0f, ImVec2(-1.0f, 0.0f), overlay);
    ImGui::PopStyleColor(2);
}

// ═══════════════════════════════════════════════════════════════════════════
// SECTION: OVERVIEW - Dashboard-style KPI Cards
// ═══════════════════════════════════════════════════════════════════════════

void
AiAnalysisView::RenderOverview()
{
    auto theme = GetCurrentTheme();

    jt::Json& summary   = m_data["summary"];
    jt::Json& prof_info = m_data["profiling_info"];
    jt::Json& meta      = m_data["metadata"];
    jt::Json& exec_bd   = m_data["execution_breakdown"];

    // Card status from confidence
    CardStatus card_status = CardStatus::Info;
    double confidence = summary.isObject() && summary["confidence"].isNumber()
                        ? summary["confidence"].getNumber() : 0.0;

    if(confidence >= 0.75)
        card_status = CardStatus::Success;
    else if(confidence >= 0.50)
        card_status = CardStatus::Warning;
    else
        card_status = CardStatus::Info;

    BeginCard("Analysis Summary", card_status, nullptr);

    if(!m_section_expanded["Analysis Summary"])
    {
        EndCard();
        return;
    }

    // ═══ KPI GRID - Visual metrics ═══
    // Get values from JSON
    std::string bottleneck = summary.isObject() && summary["primary_bottleneck"].isString()
                                 ? summary["primary_bottleneck"].getString()
                                 : "Unknown";

    double kernel_pct = exec_bd.isObject() && exec_bd["kernel_time_pct"].isNumber()
                            ? exec_bd["kernel_time_pct"].getNumber() : 0.0;
    long long total_ns = exec_bd.isObject() && exec_bd["total_runtime_ns"].isNumber()
                             ? exec_bd["total_runtime_ns"].getLong() : 0LL;
    int analysis_tier = prof_info.isObject() && prof_info["analysis_tier"].isNumber()
                            ? (int)prof_info["analysis_tier"].getLong() : 1;

    bool use_dark = SettingsManager::GetInstance().GetUserSettings().display_settings.use_dark_mode;

    // KPI Cards in a 2x2 grid
    const float avail_width = ImGui::GetContentRegionAvail().x;
    const float kpi_width = (avail_width - ImGui::GetStyle().ItemSpacing.x) / 2.0f;
    const float kpi_height = 75.0f;  // Taller for centered content

    // Helper lambda for rendering KPI card - centered text, larger value
    auto RenderKpiCard = [&](const char* id, const char* label, const char* value,
                             const char* sublabel, ImVec4 accent_color) {
        // Card background - subtle tint based on accent
        ImVec4 card_bg = use_dark
            ? ImVec4(accent_color.x * 0.12f, accent_color.y * 0.12f, accent_color.z * 0.12f, 0.6f)
            : ImVec4(accent_color.x * 0.1f, accent_color.y * 0.1f, accent_color.z * 0.1f, 0.4f);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, card_bg);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);

        ImGui::BeginChild(id, ImVec2(kpi_width, kpi_height), ImGuiChildFlags_Borders);

        // Top accent bar
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetWindowPos();
        draw_list->AddRectFilled(p, ImVec2(p.x + kpi_width, p.y + 4.0f),
            ImGui::ColorConvertFloat4ToU32(accent_color), 8.0f, ImDrawFlags_RoundCornersTop);

        // Center everything
        float content_height = ImGui::GetTextLineHeight() * 2.5f;  // Approximate
        float start_y = (kpi_height - content_height) * 0.5f;
        ImGui::SetCursorPosY(start_y);

        // Label - centered, smaller
        ImVec4 label_color = use_dark ? ImVec4(0.65f, 0.65f, 0.7f, 1.0f) : ImVec4(0.35f, 0.35f, 0.4f, 1.0f);
        float label_width = ImGui::CalcTextSize(label).x;
        ImGui::SetCursorPosX((kpi_width - label_width) * 0.5f);
        ImGui::TextColored(label_color, "%s", label);

        // Value - centered, large and bold
        ImGui::SetWindowFontScale(1.5f);
        float value_width = ImGui::CalcTextSize(value).x;
        ImGui::SetCursorPosX((kpi_width - value_width) * 0.5f);
        ImGui::TextColored(accent_color, "%s", value);
        ImGui::SetWindowFontScale(1.0f);

        // Sublabel - centered if exists
        if(sublabel && sublabel[0])
        {
            float sub_width = ImGui::CalcTextSize(sublabel).x;
            ImGui::SetCursorPosX((kpi_width - sub_width) * 0.5f);
            ImGui::TextColored(label_color, "%s", sublabel);
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    };

    // Row 1: Bottleneck and Confidence
    {
        // Bottleneck color
        ImVec4 bn_color = theme.info;
        if(bottleneck == "memory" || bottleneck == "Memory")
            bn_color = theme.warning;
        else if(bottleneck == "compute" || bottleneck == "Compute")
            bn_color = theme.success;
        else if(bottleneck == "latency" || bottleneck == "Latency")
            bn_color = ImVec4(0.65f, 0.35f, 0.85f, 1.0f);  // Purple

        RenderKpiCard("kpi_bn", "BOTTLENECK", bottleneck.c_str(), "", bn_color);

        ImGui::SameLine();

        // Confidence
        ImVec4 conf_color = (confidence >= 0.75) ? theme.success
                            : (confidence >= 0.50) ? theme.warning
                                                   : theme.critical;
        char conf_str[32];
        std::snprintf(conf_str, sizeof(conf_str), "%.0f%%", confidence * 100.0);
        RenderKpiCard("kpi_conf", "CONFIDENCE", conf_str, "", conf_color);
    }

    ImGui::Spacing();

    // Row 2: Kernel Time and Runtime
    {
        ImVec4 kernel_color = (kernel_pct >= 60.0) ? theme.success
                            : (kernel_pct >= 35.0) ? theme.warning
                                                   : theme.critical;
        char kernel_str[32];
        std::snprintf(kernel_str, sizeof(kernel_str), "%.1f%%", kernel_pct);
        RenderKpiCard("kpi_kernel", "KERNEL TIME", kernel_str, "GPU compute", kernel_color);

        ImGui::SameLine();

        RenderKpiCard("kpi_runtime", "RUNTIME", FormatNs(total_ns).c_str(), "total", theme.info);
    }

    ImGui::Spacing();

    // ═══ KEY FINDINGS - Brief list ═══
    if(summary.isObject() && summary["key_findings"].isArray() &&
       !summary["key_findings"].getArray().empty())
    {
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextDisabled("Key Findings:");

        for(jt::Json& finding : summary["key_findings"].getArray())
        {
            if(finding.isString())
            {
                ImGui::BulletText("%s", finding.getString().c_str());
            }
        }
    }

    // ═══ METADATA FOOTER ═══
    if(meta.isObject() || prof_info.isObject())
    {
        ImGui::Spacing();
        ImGui::Separator();

        // Inline metadata using pills style
        ImGui::TextDisabled("Tier %d", analysis_tier);

        // GPU info
        if(prof_info.isObject() && prof_info["gpus"].isArray())
        {
            for(jt::Json& gpu : prof_info["gpus"].getArray())
            {
                if(!gpu.isObject()) continue;
                std::string name = gpu["name"].isString() ? gpu["name"].getString() : "Unknown GPU";
                ImGui::SameLine();
                ImGui::TextDisabled(" | GPU: %s", name.c_str());
            }
        }

        // Timestamp
        if(meta.isObject() && meta["analysis_timestamp"].isString())
        {
            ImGui::SameLine();
            ImGui::TextDisabled(" | %s", meta["analysis_timestamp"].getString().c_str());
        }
    }

    EndCard();
}

// ═══════════════════════════════════════════════════════════════════════════
// SECTION: EXECUTION BREAKDOWN
// ═══════════════════════════════════════════════════════════════════════════

void
AiAnalysisView::RenderExecutionBreakdown()
{
    auto theme = GetCurrentTheme();

    jt::Json& bd = m_data["execution_breakdown"];
    if(!bd.isObject()) return;

    // Determine status
    double kernel_pct = bd["kernel_time_pct"].isNumber() ? bd["kernel_time_pct"].getNumber() : 0.0;
    double memcpy_pct = bd["memcpy_time_pct"].isNumber() ? bd["memcpy_time_pct"].getNumber() : 0.0;
    double api_pct = bd["api_overhead_pct"].isNumber() ? bd["api_overhead_pct"].getNumber() : 0.0;

    CardStatus card_status = CardStatus::Success;
    if(memcpy_pct > 25.0 || api_pct > 15.0 || kernel_pct < 40.0)
        card_status = CardStatus::Warning;
    if(memcpy_pct > 40.0 || api_pct > 30.0 || kernel_pct < 25.0)
        card_status = CardStatus::Critical;

    BeginCard("Execution Time Breakdown", card_status, nullptr);

    if(!m_section_expanded["Execution Time Breakdown"])
    {
        EndCard();
        return;
    }

    long long total_ns = bd["total_runtime_ns"].isNumber() ? bd["total_runtime_ns"].getLong() : 0LL;

    // Total runtime
    ImGui::TextDisabled("Total Runtime:");
    ImGui::SameLine();
    ImGui::TextColored(theme.info, "%s", FormatNs(total_ns).c_str());
    ImGui::Spacing();

    // Time breakdown values
    double kernel_pct_val = bd["kernel_time_pct"].isNumber() ? bd["kernel_time_pct"].getNumber() : 0.0;
    double memcpy_pct_val = bd["memcpy_time_pct"].isNumber() ? bd["memcpy_time_pct"].getNumber() : 0.0;
    double api_pct_val = bd["api_overhead_pct"].isNumber() ? bd["api_overhead_pct"].getNumber() : 0.0;
    double idle_pct_val = bd["idle_time_pct"].isNumber() ? bd["idle_time_pct"].getNumber() : 0.0;

    // Colors for each segment
    ImVec4 kernel_color = theme.info;
    ImVec4 memcpy_color = theme.warning;
    ImVec4 api_color = ImVec4(0.60f, 0.40f, 0.80f, 1.0f);  // Purple
    ImVec4 idle_color = theme.text_secondary;

    // ═══ STACKED BAR CHART ═══
    // Visual horizontal bar showing time distribution
    const float bar_height = 28.0f;
    const float bar_width = ImGui::GetContentRegionAvail().x;
    ImVec2 bar_pos = ImGui::GetCursorScreenPos();

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Background
    bool use_dark = SettingsManager::GetInstance().GetUserSettings().display_settings.use_dark_mode;
    ImU32 bg_color = use_dark ? IM_COL32(40, 40, 50, 255) : IM_COL32(220, 220, 230, 255);
    draw_list->AddRectFilled(bar_pos, ImVec2(bar_pos.x + bar_width, bar_pos.y + bar_height), bg_color, 4.0f);

    // Draw segments
    float x_offset = 0.0f;

    // Kernel segment
    if(kernel_pct_val > 0.0)
    {
        float seg_width = (float)(kernel_pct_val / 100.0 * bar_width);
        draw_list->AddRectFilled(
            ImVec2(bar_pos.x + x_offset, bar_pos.y),
            ImVec2(bar_pos.x + x_offset + seg_width, bar_pos.y + bar_height),
            ImGui::ColorConvertFloat4ToU32(kernel_color), 4.0f);
        x_offset += seg_width;
    }

    // Memory segment
    if(memcpy_pct_val > 0.0)
    {
        float seg_width = (float)(memcpy_pct_val / 100.0 * bar_width);
        draw_list->AddRectFilled(
            ImVec2(bar_pos.x + x_offset, bar_pos.y),
            ImVec2(bar_pos.x + x_offset + seg_width, bar_pos.y + bar_height),
            ImGui::ColorConvertFloat4ToU32(memcpy_color), 0.0f);
        x_offset += seg_width;
    }

    // API segment
    if(api_pct_val > 0.0)
    {
        float seg_width = (float)(api_pct_val / 100.0 * bar_width);
        draw_list->AddRectFilled(
            ImVec2(bar_pos.x + x_offset, bar_pos.y),
            ImVec2(bar_pos.x + x_offset + seg_width, bar_pos.y + bar_height),
            ImGui::ColorConvertFloat4ToU32(api_color), 0.0f);
        x_offset += seg_width;
    }

    // Idle segment
    if(idle_pct_val > 0.0)
    {
        float seg_width = (float)(idle_pct_val / 100.0 * bar_width);
        draw_list->AddRectFilled(
            ImVec2(bar_pos.x + x_offset, bar_pos.y),
            ImVec2(bar_pos.x + x_offset + seg_width, bar_pos.y + bar_height),
            ImGui::ColorConvertFloat4ToU32(idle_color), 4.0f);
    }

    // Reserve space for the bar
    ImGui::Dummy(ImVec2(bar_width, bar_height));
    ImGui::Spacing();

    // ═══ LEGEND ═══
    // Inline legend with colored squares
    auto RenderLegendItem = [&](const char* label, double pct, ImVec4 color) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();

        // Colored square
        float sq_size = 10.0f;
        float y_offset = (ImGui::GetTextLineHeight() - sq_size) * 0.5f;
        dl->AddRectFilled(ImVec2(pos.x, pos.y + y_offset),
                          ImVec2(pos.x + sq_size, pos.y + y_offset + sq_size),
                          ImGui::ColorConvertFloat4ToU32(color), 2.0f);

        ImGui::Dummy(ImVec2(sq_size + 4.0f, 0.0f));
        ImGui::SameLine();

        // Use theme text color for label
        ImGui::TextColored(color, "%.1f%%", pct);
        ImGui::SameLine();
        ImGui::TextColored(theme.text_primary, "%s", label);
        ImGui::SameLine(0.0f, 20.0f);
    };

    RenderLegendItem("Kernel", kernel_pct_val, kernel_color);
    RenderLegendItem("Memory", memcpy_pct_val, memcpy_color);
    RenderLegendItem("API", api_pct_val, api_color);
    RenderLegendItem("Idle", idle_pct_val, idle_color);

    ImGui::NewLine();
    ImGui::Spacing();

    // ═══ DETAILED PROGRESS BARS ═══
    RenderProgressChart("Kernel Execution", (float)kernel_pct_val, kernel_color, "Time executing GPU kernels");
    RenderProgressChart("Memory Transfers", (float)memcpy_pct_val, memcpy_color, "Data movement");
    RenderProgressChart("API Overhead", (float)api_pct_val, api_color, "ROCm API calls");
    RenderProgressChart("GPU Idle Time", (float)idle_pct_val, idle_color, "GPU waiting");

    EndCard();
}

// ═══════════════════════════════════════════════════════════════════════════
// SECTION: RECOMMENDATIONS
// ═══════════════════════════════════════════════════════════════════════════

void
AiAnalysisView::RenderRecommendations()
{
    auto theme = GetCurrentTheme();
    bool use_dark = SettingsManager::GetInstance().GetUserSettings().display_settings.use_dark_mode;

    jt::Json& recs = m_data["recommendations"];
    if(!recs.isArray() || recs.getArray().empty()) return;

    // Card status from highest priority
    CardStatus card_status = CardStatus::Info;
    for(jt::Json& rec : recs.getArray())
    {
        if(!rec.isObject()) continue;
        std::string priority = rec["priority"].isString() ? rec["priority"].getString() : "INFO";
        if(priority == "HIGH") { card_status = CardStatus::Critical; break; }
        else if(priority == "MEDIUM" && card_status != CardStatus::Critical)
            card_status = CardStatus::Warning;
    }

    BeginCard("Optimization Recommendations", card_status, nullptr);

    if(!m_section_expanded["Optimization Recommendations"])
    {
        EndCard();
        return;
    }

    int idx = 0;
    for(jt::Json& rec : recs.getArray())
    {
        if(!rec.isObject()) { ++idx; continue; }

        std::string id       = rec["id"].isString() ? rec["id"].getString() : "???";
        std::string priority = rec["priority"].isString() ? rec["priority"].getString() : "INFO";
        std::string category = rec["category"].isString() ? rec["category"].getString() : "";
        std::string issue    = rec["issue"].isString() ? rec["issue"].getString() : "";
        std::string suggest  = rec["suggestion"].isString() ? rec["suggestion"].getString() : "";
        std::string impact   = rec["estimated_impact"].isString() ? rec["estimated_impact"].getString() : "";

        ImVec4 priority_color = PriorityColor(priority);
        bool has_commands = rec["commands"].isArray() && !rec["commands"].getArray().empty();

        // ═══ COLLAPSIBLE RECOMMENDATION HEADER ═══
        // Build header string
        std::string header = "[" + priority + "] " + category;

        // TreeNode for collapsible functionality
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(priority_color.x * 0.3f, priority_color.y * 0.3f, priority_color.z * 0.3f, 0.4f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(priority_color.x * 0.4f, priority_color.y * 0.4f, priority_color.z * 0.4f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(priority_color.x * 0.5f, priority_color.y * 0.5f, priority_color.z * 0.5f, 0.6f));

        bool is_open = ImGui::TreeNodeEx(("rec_" + std::to_string(idx)).c_str(),
                                         ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth,
                                         "%s", header.c_str());

        ImGui::PopStyleColor(3);

        if(!is_open)
        {
            ++idx;
            continue;
        }

        ImGui::Indent(8.0f);

        ImGui::Spacing();
        ImGui::Spacing();

        // Readable text colors for both light and dark modes
        ImVec4 label_color = use_dark
            ? ImVec4(0.9f, 0.7f, 0.4f, 1.0f)   // Warm orange for dark mode
            : ImVec4(0.6f, 0.4f, 0.1f, 1.0f);  // Dark orange for light mode

        ImVec4 text_color = use_dark
            ? ImVec4(0.85f, 0.85f, 0.88f, 1.0f)  // Light gray for dark mode
            : ImVec4(0.15f, 0.15f, 0.18f, 1.0f); // Dark gray for light mode

        // ─── Issue ───
        if(!issue.empty())
        {
            // Bold label with good contrast
            ImGui::SetWindowFontScale(1.05f);
            ImGui::TextColored(label_color, "Issue:");
            ImGui::SetWindowFontScale(1.0f);

            ImGui::Indent(8.0f);
            ImGui::TextColored(text_color, "%s", issue.c_str());
            ImGui::Unindent(8.0f);

            ImGui::Spacing();
            ImGui::Spacing();
        }

        // ─── Solution ───
        if(!suggest.empty())
        {
            // Bold label
            ImGui::SetWindowFontScale(1.05f);
            ImGui::TextColored(label_color, "What to do:");
            ImGui::SetWindowFontScale(1.0f);

            ImGui::Indent(8.0f);
            ImGui::TextColored(text_color, "%s", suggest.c_str());
            ImGui::Unindent(8.0f);

            ImGui::Spacing();
            ImGui::Spacing();
        }

        // ─── Action Steps ───
        if(rec["actions"].isArray() && !rec["actions"].getArray().empty())
        {
            ImGui::SetWindowFontScale(1.05f);
            ImGui::TextColored(label_color, "Action Steps:");
            ImGui::SetWindowFontScale(1.0f);

            ImGui::Indent(8.0f);
            int step = 1;
            for(jt::Json& action : rec["actions"].getArray())
            {
                if(action.isString())
                {
                    ImGui::TextColored(text_color, "%d. %s", step++, action.getString().c_str());
                }
            }
            ImGui::Unindent(8.0f);

            ImGui::Spacing();
            ImGui::Spacing();
        }

        // ─── Impact ───
        if(!impact.empty())
        {
            // Green badge - brighter for visibility
            ImVec4 impact_bg = use_dark
                ? ImVec4(0.1f, 0.3f, 0.1f, 0.6f)   // Dark green tint for dark mode
                : ImVec4(0.85f, 0.95f, 0.85f, 1.0f); // Light green for light mode

            ImVec4 impact_text = use_dark
                ? ImVec4(0.5f, 0.95f, 0.5f, 1.0f)  // Bright green for dark mode
                : ImVec4(0.2f, 0.6f, 0.2f, 1.0f);  // Dark green for light mode

            ImGui::PushStyleColor(ImGuiCol_ChildBg, impact_bg);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
            ImGui::BeginChild(("impact_badge_" + std::to_string(idx)).c_str(),
                              ImVec2(-1, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);

            ImGui::Spacing();
            ImGui::TextColored(impact_text, ">> Expected Impact: %s", impact.c_str());
            ImGui::Spacing();

            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::Spacing();
        }

        // ─── Commands Section ───
        if(has_commands)
        {
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Spacing();

            ImGui::SetWindowFontScale(1.05f);
            ImGui::TextColored(theme.text_primary, "Profiling Commands:");
            ImGui::SetWindowFontScale(1.0f);

            ImGui::Spacing();

            int ci = 0;
            for(jt::Json& cmd : rec["commands"].getArray())
            {
                if(!cmd.isObject()) { ++ci; continue; }

                std::string tool = cmd["tool"].isString() ? cmd["tool"].getString() : "";
                std::string full_cmd = cmd["full_command"].isString() ? cmd["full_command"].getString() : "";
                std::string desc = cmd["description"].isString() ? cmd["description"].getString() : "";

                if(full_cmd.empty()) { ++ci; continue; }

                // ─── Tool header ───
                ImVec4 tool_color = use_dark
                    ? ImVec4(0.4f, 0.7f, 1.0f, 1.0f)   // Bright blue for dark mode
                    : ImVec4(0.2f, 0.4f, 0.8f, 1.0f);  // Dark blue for light mode

                ImVec4 desc_color = use_dark
                    ? ImVec4(0.6f, 0.6f, 0.65f, 1.0f)  // Light gray for dark mode
                    : ImVec4(0.4f, 0.4f, 0.45f, 1.0f); // Dark gray for light mode

                ImGui::TextColored(tool_color, "%s", tool.c_str());
                if(!desc.empty())
                {
                    ImGui::SameLine();
                    ImGui::TextColored(desc_color, "- %s", desc.c_str());
                }

                ImGui::Spacing();

                // ─── Command box - lighter background like HTML ───
                ImVec4 cmd_bg = use_dark
                    ? ImVec4(0.18f, 0.18f, 0.20f, 1.0f)  // Lighter gray for dark mode
                    : ImVec4(0.92f, 0.92f, 0.94f, 1.0f); // Very light gray for light mode

                ImGui::PushStyleColor(ImGuiCol_ChildBg, cmd_bg);
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
                ImGui::BeginChild(("cmd_" + std::to_string(idx) + "_" + std::to_string(ci)).c_str(),
                                  ImVec2(-1, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);

                ImGui::Spacing();
                ImGui::Indent(8.0f);

                // Command text - better color for readability
                ImVec4 cmd_color = use_dark
                    ? ImVec4(0.5f, 0.95f, 0.5f, 1.0f)   // Bright green for dark
                    : ImVec4(0.2f, 0.5f, 0.2f, 1.0f);   // Dark green for light

                ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x - 10.0f);
                ImGui::TextColored(cmd_color, "$ %s", full_cmd.c_str());
                ImGui::PopTextWrapPos();

                ImGui::Unindent(8.0f);
                ImGui::Spacing();
                ImGui::EndChild();
                ImGui::PopStyleVar();
                ImGui::PopStyleColor();

                // ─── Buttons row ───
                ImGui::Spacing();

                // Copy button
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.35f, 0.38f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.45f, 0.45f, 0.48f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                if(ImGui::Button(("Copy##copy_" + std::to_string(idx) + "_" + std::to_string(ci)).c_str()))
                {
                    ImGui::SetClipboardText(full_cmd.c_str());
                }
                ImGui::PopStyleColor(3);

                ImGui::SameLine();

                // Run button
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.9f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                if(ImGui::Button(("Run##run_" + std::to_string(idx) + "_" + std::to_string(ci)).c_str()))
                {
                    ExecuteRecommendationCommand(full_cmd, idx);
                }
                ImGui::PopStyleColor(3);

                ImGui::Spacing();
                ImGui::Spacing();
                ++ci;
            }
        }

        // Close the collapsible section
        ImGui::Unindent(8.0f);
        ImGui::TreePop();

        ImGui::Spacing();
        ++idx;
    }

    EndCard();
}

// ═══════════════════════════════════════════════════════════════════════════
// SECTION: HOTSPOTS
// ═══════════════════════════════════════════════════════════════════════════

void
AiAnalysisView::RenderHotspots()
{
    auto theme = GetCurrentTheme();

    jt::Json& hotspots = m_data["hotspots"];
    if(!hotspots.isArray() || hotspots.getArray().empty()) return;

    // Card status
    CardStatus card_status = CardStatus::Info;
    if(!hotspots.getArray().empty())
    {
        jt::Json& top = hotspots.getArray()[0];
        if(top.isObject() && top["pct_of_total"].isNumber())
        {
            double pct = top["pct_of_total"].getNumber();
            if(pct > 80.0) card_status = CardStatus::Critical;
            else if(pct > 50.0) card_status = CardStatus::Warning;
            else card_status = CardStatus::Success;
        }
    }

    BeginCard("Top Kernel Hotspots", card_status, nullptr);

    if(!m_section_expanded["Top Kernel Hotspots"])
    {
        EndCard();
        return;
    }

    ImGui::TextDisabled("Kernels consuming the most GPU time:");
    ImGui::Spacing();

    // Table - simple version
    constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                       ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;

    const size_t n = hotspots.getArray().size();
    // Minimum height to show 5 items, max 400px
    const float row_height = ImGui::GetTextLineHeightWithSpacing();
    const float min_rows = 5.0f;
    const float tbl_h = std::max(std::min(static_cast<float>(n + 1) * row_height, 400.0f),
                                 (min_rows + 1) * row_height);

    if(ImGui::BeginTable("hotspots_tbl", 5, kFlags, ImVec2(0, tbl_h)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Rank", ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("Kernel", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Calls", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("%", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableHeadersRow();

        // Rows
        for(jt::Json& k : hotspots.getArray())
        {
            if(!k.isObject()) continue;

            int rank = k["rank"].isNumber() ? (int)k["rank"].getLong() : 0;
            std::string name = k["name"].isString() ? k["name"].getString() : "";
            long long calls = k["calls"].isNumber() ? k["calls"].getLong() : 0LL;
            long long total = k["total_duration_ns"].isNumber() ? k["total_duration_ns"].getLong() : 0LL;
            double pct = k["pct_of_total"].isNumber() ? k["pct_of_total"].getNumber() : 0.0;

            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("#%d", rank);

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(name.c_str());

            ImGui::TableNextColumn();
            ImGui::Text("%lld", calls);

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(FormatNs(total).c_str());

            ImGui::TableNextColumn();
            if(pct >= 30.0)
                ImGui::TextColored(theme.critical, "%.1f%%", pct);
            else if(pct >= 15.0)
                ImGui::TextColored(theme.warning, "%.1f%%", pct);
            else
                ImGui::TextColored(theme.success, "%.1f%%", pct);
        }

        ImGui::EndTable();
    }

    EndCard();
}

// ═══════════════════════════════════════════════════════════════════════════
// SECTION: MEMORY ANALYSIS
// ═══════════════════════════════════════════════════════════════════════════

void
AiAnalysisView::RenderMemoryAnalysis()
{
    auto theme = GetCurrentTheme();

    jt::Json& mem = m_data["memory_analysis"];
    if(!mem.isObject()) return;

    BeginCard("Memory Transfer Analysis", CardStatus::Info, nullptr);

    if(!m_section_expanded["Memory Transfer Analysis"])
    {
        EndCard();
        return;
    }

    ImGui::TextDisabled("Data movement between host and device:");
    ImGui::Spacing();

    // Simple Table
    constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;

    if(ImGui::BeginTable("memory_tbl", 4, kFlags))
    {
        ImGui::TableSetupColumn("Direction", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Bytes", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("BW", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableHeadersRow();

        const char* directions[] = {"Host-to-Device", "Device-to-Host", "Device-to-Device"};
        const char* dir_symbols[] = {"H->D", "D->H", "D<->D"};

        for(int i = 0; i < 3; i++)
        {
            jt::Json& entry = mem[directions[i]];
            if(!entry.isObject()) continue;

            long long count   = entry["count"].isNumber() ? entry["count"].getLong() : 0LL;
            long long total_b = entry["total_bytes"].isNumber() ? entry["total_bytes"].getLong() : 0LL;
            double bw_gbs = entry["bandwidth_gbps"].isNumber() ? entry["bandwidth_gbps"].getNumber() : 0.0;

            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("%s", dir_symbols[i]);

            ImGui::TableNextColumn();
            ImGui::Text("%lld", count);

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(FormatBytes(static_cast<double>(total_b)).c_str());

            ImGui::TableNextColumn();
            if(bw_gbs < 5.0)
                ImGui::TextColored(theme.critical, "%.1f GB/s", bw_gbs);
            else if(bw_gbs < 15.0)
                ImGui::TextColored(theme.warning, "%.1f GB/s", bw_gbs);
            else
                ImGui::TextColored(theme.success, "%.1f GB/s", bw_gbs);
        }

        ImGui::EndTable();
    }

    EndCard();
}

// ═══════════════════════════════════════════════════════════════════════════
// SECTION: HARDWARE COUNTERS
// ═══════════════════════════════════════════════════════════════════════════

void
AiAnalysisView::RenderHardwareCounters()
{
    auto theme = GetCurrentTheme();

    jt::Json& hw = m_data["hardware_counters"];
    if(!hw.isObject()) return;

    const bool has_counters = hw["has_counters"].isBool() && hw["has_counters"].getBool();

    CardStatus card_status = CardStatus::Info;
    if(has_counters)
    {
        jt::Json& metrics = hw["metrics"];
        if(metrics.isObject() && metrics["gpu_utilization_pct"].isNumber())
        {
            double util = metrics["gpu_utilization_pct"].getNumber();
            if(util >= 70.0) card_status = CardStatus::Success;
            else if(util >= 40.0) card_status = CardStatus::Warning;
            else card_status = CardStatus::Critical;
        }
    }

    BeginCard("Hardware Performance Metrics", card_status, nullptr);

    if(!m_section_expanded["Hardware Performance Metrics"])
    {
        EndCard();
        return;
    }

    if(!has_counters)
    {
        ImGui::TextDisabled("No hardware counter data in this trace.");
        ImGui::TextDisabled("Collect with: rocprofv3 --pmc ... -- ./app");
        EndCard();
        return;
    }

    jt::Json& metrics = hw["metrics"];
    if(metrics.isObject())
    {
        ImGui::TextDisabled("Low-level GPU performance counters:");
        ImGui::Spacing();

        // GPU utilization
        if(metrics["gpu_utilization_pct"].isNumber())
        {
            double util = metrics["gpu_utilization_pct"].getNumber();
            ImVec4 util_color = util >= 70.0 ? theme.success
                                : util >= 40.0 ? theme.warning
                                               : theme.critical;

            RenderProgressChart("GPU Utilization", static_cast<float>(util), util_color, "Target: >70%");

            if(util < 70.0)
            {
                ImGui::TextColored(theme.warning, "  Low utilization - increase parallelism");
            }
        }

        // Wave occupancy
        if(metrics["avg_waves"].isNumber())
        {
            double avg_waves = metrics["avg_waves"].getNumber();
            ImGui::TextDisabled("Waves:");
            ImGui::SameLine();
            ImGui::Text("%.0f avg", avg_waves);

            if(avg_waves < 16.0)
            {
                ImGui::TextColored(theme.warning, "  Low occupancy - check register pressure");
            }
        }
    }

    EndCard();
}

// ═══════════════════════════════════════════════════════════════════════════
// SECTION: WARNINGS
// ═══════════════════════════════════════════════════════════════════════════

void
AiAnalysisView::RenderWarnings()
{
    auto theme = GetCurrentTheme();

    jt::Json& warnings = m_data["warnings"];
    jt::Json& errors   = m_data["errors"];

    const bool has_warnings = warnings.isArray() && !warnings.getArray().empty();
    const bool has_errors   = errors.isArray() && !errors.getArray().empty();

    if(!has_warnings && !has_errors) return;

    CardStatus card_status = has_errors ? CardStatus::Critical : CardStatus::Warning;

    BeginCard("Warnings & Errors", card_status, nullptr);

    if(!m_section_expanded["Warnings & Errors"])
    {
        EndCard();
        return;
    }

    // Errors
    if(has_errors)
    {
        ImGui::TextColored(theme.critical, "Errors:");
        for(jt::Json& err : errors.getArray())
        {
            if(err.isString())
            {
                ImGui::BulletText("%s", err.getString().c_str());
            }
        }
        ImGui::Spacing();
    }

    // Warnings
    if(has_warnings)
    {
        ImGui::TextColored(theme.warning, "Warnings:");
        for(jt::Json& w : warnings.getArray())
        {
            if(!w.isObject()) continue;

            std::string msg = w["message"].isString() ? w["message"].getString() : "";
            ImGui::BulletText("%s", msg.c_str());
        }
    }

    EndCard();
}

// ═══════════════════════════════════════════════════════════════════════════
// COMMAND EXECUTION
// ═══════════════════════════════════════════════════════════════════════════

void
AiAnalysisView::ExecuteRecommendationCommand(const std::string& command, int recommendation_index)
{
    std::string tool_args = ParseToolArgsFromCommand(command);

    if(tool_args.empty())
    {
        AppWindow::GetInstance()->ShowMessageDialog("Error",
            "Could not parse profiling tool arguments from command:\n\n" + command);
        return;
    }

    AppWindow::GetInstance()->ShowProfilingDialogWithRecommendation(tool_args);
}

std::string
AiAnalysisView::ParseToolArgsFromCommand(const std::string& command)
{
    // Find tool name
    size_t tool_pos = command.find("rocprofv3");
    if(tool_pos == std::string::npos) tool_pos = command.find("rocprofsys");
    if(tool_pos == std::string::npos) tool_pos = command.find("rocprof-compute");
    if(tool_pos == std::string::npos) tool_pos = command.find("rocprof-sys");
    if(tool_pos == std::string::npos) return "";

    size_t args_start = command.find(' ', tool_pos);
    if(args_start == std::string::npos) return "";

    args_start = command.find_first_not_of(' ', args_start);
    if(args_start == std::string::npos) return "";

    // Parse arguments, excluding -d, -o, --output
    std::string args = command.substr(args_start);
    std::string result;

    std::istringstream iss(args);
    std::string token;
    bool skip_next = false;

    while(iss >> token)
    {
        if(skip_next)
        {
            skip_next = false;
            continue;
        }

        if(token == "-d" || token == "-o" || token == "--output-dir" ||
           token == "--output-file" || token == "--output")
        {
            skip_next = true;
            continue;
        }

        if(token[0] == '-')
        {
            if(!result.empty()) result += " ";
            result += token;
        }
        else if(token[0] == '/' || token.find("./") != std::string::npos ||
                token.find("/app") != std::string::npos || token == "--")
        {
            break;
        }
        else if(!result.empty())
        {
            result += " " + token;
        }
    }

    return result;
}

}  // namespace View
}  // namespace RocProfVis
