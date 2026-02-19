// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ai_analysis_view.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_settings_manager.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace RocProfVis
{
namespace View
{

// ─── formatting helpers ──────────────────────────────────────────────────────

std::string
AiAnalysisView::FormatNs(long long ns)
{
    if(ns < 0) ns = 0;
    if(ns < 1000LL) return std::to_string(ns) + " ns";
    if(ns < 1000000LL) return std::to_string(ns / 1000) + " µs";
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
    // Match HTML theme colors
    if(priority == "HIGH") return ImVec4(1.0f, 0.27f, 0.27f, 1.0f);  // #ff4444 bright red
    if(priority == "MEDIUM") return ImVec4(1.0f, 0.55f, 0.0f, 1.0f);  // #ff8c00 orange
    if(priority == "LOW") return ImVec4(1.0f, 0.95f, 0.25f, 1.0f);  // yellow
    return ImVec4(0.33f, 0.60f, 0.93f, 1.0f);  // #5599ee blue
}

// ─── AiAnalysisView ──────────────────────────────────────────────────────────

AiAnalysisView::AiAnalysisView()
: m_loaded(false)
{
    m_widget_name = GenUniqueName("AI Analysis");
}

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

void
AiAnalysisView::Render()
{
    ImGui::BeginChild(m_widget_name.c_str(), ImVec2(0, 0), ImGuiChildFlags_None);

    if(!m_loaded)
        RenderLoadScreen();
    else
        RenderAnalysisContent();

    ImGui::EndChild();
}

// ─── load screen ─────────────────────────────────────────────────────────────

void
AiAnalysisView::RenderLoadScreen()
{
    auto& settings = SettingsManager::GetInstance();

    const float avail_w = ImGui::GetContentRegionAvail().x;
    const float avail_h = ImGui::GetContentRegionAvail().y;

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + avail_h * 0.35f);

    // Title
    const char* title = "GPU Performance Analysis";
    ImGui::SetCursorPosX((avail_w - ImGui::CalcTextSize(title).x) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.72f, 0.92f, 1.0f));
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Subtitle
    const char* sub = "Load a rocpd analysis JSON to view insights and recommendations";
    ImGui::SetCursorPosX((avail_w - ImGui::CalcTextSize(sub).x) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
    ImGui::TextUnformatted(sub);
    ImGui::PopStyleColor();

    // Error message (if any)
    if(!m_load_error.empty())
    {
        ImGui::Spacing();
        ImGui::SetCursorPosX((avail_w - ImGui::CalcTextSize(m_load_error.c_str()).x) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        ImGui::TextUnformatted(m_load_error.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Browse button
    const char* btn_label = "  Browse for JSON File...  ";
    const float btn_w     = ImGui::CalcTextSize(btn_label).x + 24.0f;
    ImGui::SetCursorPosX((avail_w - btn_w) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Button, settings.GetColor(Colors::kButton));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, settings.GetColor(Colors::kButtonHovered));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, settings.GetColor(Colors::kButtonActive));
    if(ImGui::Button(btn_label))
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
    ImGui::PopStyleColor(3);

    // Hint
    ImGui::Spacing();
    ImGui::Spacing();
    const char* hint = "Generate a JSON file with:   rocpd analyze -i trace.db --format json";
    ImGui::SetCursorPosX((avail_w - ImGui::CalcTextSize(hint).x) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
    ImGui::TextUnformatted(hint);
    ImGui::PopStyleColor();
}

// ─── analysis content ────────────────────────────────────────────────────────

void
AiAnalysisView::RenderAnalysisContent()
{
    auto& settings = SettingsManager::GetInstance();

    // Top bar: file path + reload button
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
    ImGui::TextUnformatted(m_file_path.c_str());
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 152.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, settings.GetColor(Colors::kButton));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, settings.GetColor(Colors::kButtonHovered));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, settings.GetColor(Colors::kButtonActive));
    if(ImGui::SmallButton("  Load Different File  "))
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
    ImGui::PopStyleColor(3);

    ImGui::Separator();
    ImGui::Spacing();

    // Scrollable analysis panels
    ImGui::BeginChild("ai_analysis_scroll", ImVec2(0, 0), ImGuiChildFlags_None);

    RenderOverview();
    ImGui::Spacing();
    RenderExecutionBreakdown();
    ImGui::Spacing();
    RenderRecommendations();
    ImGui::Spacing();
    RenderHotspots();
    ImGui::Spacing();
    RenderMemoryAnalysis();
    ImGui::Spacing();
    RenderHardwareCounters();
    ImGui::Spacing();
    RenderWarnings();

    ImGui::EndChild();
}

// ─── overview ────────────────────────────────────────────────────────────────

void
AiAnalysisView::RenderOverview()
{
    auto& settings = SettingsManager::GetInstance();

    // Dark theme header matching HTML --bg2: #16161f
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.09f, 0.09f, 0.12f, 1.0f));  // #16161f
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.12f, 0.12f, 0.18f, 1.0f));  // #1e1e2d
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.14f, 0.14f, 0.21f, 1.0f));  // #242436
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.87f, 0.88f, 0.94f, 1.0f));  // #dde0f0 light text
    if(!ImGui::CollapsingHeader("Overview", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::PopStyleColor(4);
        return;
    }
    ImGui::PopStyleColor(4);

    ImGui::Indent();

    jt::Json& summary   = m_data["summary"];
    jt::Json& prof_info = m_data["profiling_info"];
    jt::Json& meta      = m_data["metadata"];

    // Overall assessment text
    if(summary.isObject() && summary["overall_assessment"].isString())
    {
        // Use default app text color (already light on dark background)
        ImGui::TextWrapped("%s", summary["overall_assessment"].getString().c_str());
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
    }

    // Bottleneck + confidence badges
    if(summary.isObject())
    {
        std::string bottleneck = summary["primary_bottleneck"].isString()
                                     ? summary["primary_bottleneck"].getString()
                                     : "unknown";
        double confidence =
            summary["confidence"].isNumber() ? summary["confidence"].getNumber() : 0.0;

        ImGui::Text("Primary Bottleneck:");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.33f, 0.60f, 0.93f, 1.0f));  // #5599ee blue - bright
        ImGui::TextUnformatted(bottleneck.c_str());
        ImGui::PopStyleColor();

        ImGui::SameLine(0.0f, 50.0f);
        ImGui::Text("Confidence:");
        ImGui::SameLine();
        ImVec4 conf_color = (confidence >= 0.75) ? ImVec4(0.27f, 0.87f, 0.40f, 1.0f)  // #44dd66 green - bright
                            : (confidence >= 0.50) ? ImVec4(1.0f, 0.55f, 0.0f, 1.0f)  // #ff8c00 orange - bright
                                                   : ImVec4(0.70f, 0.70f, 0.70f, 1.0f);  // light gray
        ImGui::PushStyleColor(ImGuiCol_Text, conf_color);
        ImGui::Text("%.0f%%", confidence * 100.0);
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Spacing();
    }

    // Key findings
    if(summary.isObject() && summary["key_findings"].isArray() &&
       !summary["key_findings"].getArray().empty())
    {
        ImGui::Spacing();
        ImGui::TextUnformatted("Key Findings:");
        ImGui::Spacing();
        for(jt::Json& finding : summary["key_findings"].getArray())
        {
            if(finding.isString())
            {
                ImGui::BulletText("%s", finding.getString().c_str());
            }
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();

    // Metadata row
    if(meta.isObject())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
        std::string ts  = meta["analysis_timestamp"].isString()
                              ? meta["analysis_timestamp"].getString()
                              : "";
        std::string ver = meta["analysis_version"].isString()
                              ? meta["analysis_version"].getString()
                              : "";
        std::string db =
            meta["database_file"].isString() ? meta["database_file"].getString() : "";

        if(!ts.empty()) ImGui::Text("Analyzed:  %s", ts.c_str());
        if(!ver.empty())
        {
            ImGui::SameLine(0.0f, 20.0f);
            ImGui::Text("Schema v%s", ver.c_str());
        }
        if(!db.empty()) ImGui::Text("Source:    %s", db.c_str());
        ImGui::PopStyleColor();
    }

    // Profiling tier + GPUs
    if(prof_info.isObject())
    {
        if(prof_info["analysis_tier"].isNumber())
            ImGui::Text("Analysis Tier: %lld",
                        static_cast<long long>(prof_info["analysis_tier"].getLong()));

        if(prof_info["gpus"].isArray())
        {
            for(jt::Json& gpu : prof_info["gpus"].getArray())
            {
                if(!gpu.isObject()) continue;
                std::string name =
                    gpu["name"].isString() ? gpu["name"].getString() : "Unknown GPU";
                std::string arch =
                    gpu["architecture"].isString() ? gpu["architecture"].getString() : "";
                if(arch.empty())
                    ImGui::BulletText("GPU: %s", name.c_str());
                else
                    ImGui::BulletText("GPU: %s  (%s)", name.c_str(), arch.c_str());
            }
        }
    }

    ImGui::Unindent();
}

// ─── execution breakdown ─────────────────────────────────────────────────────

void
AiAnalysisView::RenderExecutionBreakdown()
{
    // Dark theme header
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.09f, 0.09f, 0.12f, 1.0f));  // #16161f
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.12f, 0.12f, 0.18f, 1.0f));  // #1e1e2d
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.14f, 0.14f, 0.21f, 1.0f));  // #242436
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.87f, 0.88f, 0.94f, 1.0f));  // #dde0f0
    if(!ImGui::CollapsingHeader("Execution Breakdown", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::PopStyleColor(4);
        return;
    }
    ImGui::PopStyleColor(4);

    jt::Json& bd = m_data["execution_breakdown"];
    if(!bd.isObject())
    {
        ImGui::Indent();
        ImGui::TextUnformatted("No breakdown data.");
        ImGui::Unindent();
        return;
    }

    ImGui::Indent();

    long long total_ns =
        bd["total_runtime_ns"].isNumber() ? bd["total_runtime_ns"].getLong() : 0LL;
    ImGui::Text("Total Runtime:");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.27f, 0.87f, 0.40f, 1.0f));  // #44dd66 green - bright
    ImGui::Text("%s", FormatNs(total_ns).c_str());
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();

    struct BreakdownSection
    {
        const char* label;
        const char* pct_key;
        const char* ns_key;
        ImVec4      color;
    };

    // Bright colors visible on dark background
    const BreakdownSection sections[] = {
        {"Kernel Execution", "kernel_time_pct", "kernel_time_ns",
         {0.33f, 0.60f, 0.93f, 1.0f}},  // #5599ee blue - bright
        {"Memory Copies", "memcpy_time_pct", "memcpy_time_ns",
         {1.0f, 0.55f, 0.0f, 1.0f}},  // #ff8c00 orange - bright
        {"API Overhead", "api_overhead_pct", "api_overhead_ns",
         {0.60f, 0.40f, 0.80f, 1.0f}},  // #9966cc purple - bright
        {"GPU Idle", "idle_time_pct", "idle_time_ns", {0.50f, 0.50f, 0.50f, 1.0f}},  // light gray
    };

    for(const auto& s : sections)
    {
        double    pct = bd[s.pct_key].isNumber() ? bd[s.pct_key].getNumber() : 0.0;
        long long ns  = bd[s.ns_key].isNumber() ? bd[s.ns_key].getLong() : 0LL;

        // Use default text color (already light)
        ImGui::Text("%-20s", s.label);
        ImGui::SameLine(350.0f);

        char overlay[64];
        std::snprintf(overlay, sizeof(overlay), "%.1f%%  (%s)", pct,
                      FormatNs(ns).c_str());

        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, s.color);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.18f, 1.0f));  // #1e1e2d dark bg
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));  // White text on colored bar
        ImGui::ProgressBar(static_cast<float>(pct / 100.0), ImVec2(-1.0f, 50.0f),
                           overlay);
        ImGui::PopStyleColor(3);

        // Add tooltip with detailed information and guidance
        if(ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            ImGui::Text("%s", s.label);
            ImGui::Separator();
            ImGui::Text("Percentage: %.2f%%", pct);
            ImGui::Text("Time: %s", FormatNs(ns).c_str());
            ImGui::Text("Total Runtime: %s", FormatNs(total_ns).c_str());
            if(total_ns > 0)
            {
                double ratio = static_cast<double>(ns) / static_cast<double>(total_ns);
                ImGui::Text("Ratio: %.4f", ratio);
            }
            ImGui::Spacing();

            // Add guidance based on the section type
            if(strcmp(s.label, "Kernel Execution") == 0)
            {
                ImGui::TextWrapped("Time spent running GPU kernels (compute work).");
                ImGui::Spacing();
                ImGui::TextWrapped("Target: 60-80%% for compute-heavy workloads.");
                ImGui::TextWrapped("Low? Check for small kernels or launch overhead.");
            }
            else if(strcmp(s.label, "Memory Copies") == 0)
            {
                ImGui::TextWrapped("Time spent copying data between host and device.");
                ImGui::Spacing();
                ImGui::TextWrapped("Target: <15%% for optimal performance.");
                ImGui::TextWrapped("High? Use pinned memory, async copies, or reduce transfers.");
            }
            else if(strcmp(s.label, "API Overhead") == 0)
            {
                ImGui::TextWrapped("CPU time for HIP/ROCm API calls and kernel launches.");
                ImGui::Spacing();
                ImGui::TextWrapped("Target: <10%% for GPU-bound work.");
                ImGui::TextWrapped("High? Batch operations, use streams, or fuse kernels.");
            }
            else if(strcmp(s.label, "GPU Idle") == 0)
            {
                ImGui::TextWrapped("Time when GPU is idle waiting for work.");
                ImGui::Spacing();
                ImGui::TextWrapped("Target: Close to 0%%.");
                ImGui::TextWrapped("High? Overlap work with async operations or reduce sync points.");
            }

            ImGui::PopStyleColor();
            ImGui::EndTooltip();
        }

        ImGui::Spacing();
        ImGui::Spacing();
    }

    ImGui::Unindent();
}

// ─── recommendations ─────────────────────────────────────────────────────────

void
AiAnalysisView::RenderRecommendations()
{
    auto& settings = SettingsManager::GetInstance();

    // Dark theme header
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.09f, 0.09f, 0.12f, 1.0f));  // #16161f
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.12f, 0.12f, 0.18f, 1.0f));  // #1e1e2d
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.14f, 0.14f, 0.21f, 1.0f));  // #242436
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.87f, 0.88f, 0.94f, 1.0f));  // #dde0f0
    if(!ImGui::CollapsingHeader("Optimization Recommendations",
                                ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::PopStyleColor(4);
        return;
    }
    ImGui::PopStyleColor(4);

    jt::Json& recs = m_data["recommendations"];
    if(!recs.isArray() || recs.getArray().empty())
    {
        ImGui::Indent();
        ImGui::TextUnformatted("No recommendations.");
        ImGui::Unindent();
        return;
    }

    ImGui::Indent();

    int idx = 0;
    for(jt::Json& rec : recs.getArray())
    {
        if(!rec.isObject())
        {
            ++idx;
            continue;
        }

        std::string id       = rec["id"].isString() ? rec["id"].getString() : "???";
        std::string priority = rec["priority"].isString() ? rec["priority"].getString() : "INFO";
        std::string category = rec["category"].isString() ? rec["category"].getString() : "";
        std::string issue    = rec["issue"].isString() ? rec["issue"].getString() : "";
        std::string suggest =
            rec["suggestion"].isString() ? rec["suggestion"].getString() : "";

        // Header: "[HIGH]  ROCPD-MEMCPY-001  –  Memory Transfer###rec_0"
        std::string header = "[" + priority + "]  " + id;
        if(!category.empty()) header += "  \xe2\x80\x93  " + category;  // "–" in UTF-8
        header += "###rec_" + std::to_string(idx);

        ImGui::PushStyleColor(ImGuiCol_Text, PriorityColor(priority));
        bool open = ImGui::CollapsingHeader(header.c_str());
        ImGui::PopStyleColor();

        if(open)
        {
            ImGui::Indent();

            // Issue
            if(!issue.empty())
            {
                ImGui::TextUnformatted("Issue:");
                ImGui::SameLine();
                ImGui::TextWrapped("%s", issue.c_str());
                ImGui::Spacing();
            }

            // Suggestion
            if(!suggest.empty())
            {
                ImGui::TextUnformatted("What to do:");
                ImGui::SameLine();
                ImGui::TextWrapped("%s", suggest.c_str());
                ImGui::Spacing();
            }

            // Action steps
            if(rec["actions"].isArray() && !rec["actions"].getArray().empty())
            {
                ImGui::TextUnformatted("Steps:");
                int step = 1;
                for(jt::Json& action : rec["actions"].getArray())
                {
                    if(action.isString())
                        ImGui::Text("  %d. %s", step++, action.getString().c_str());
                }
                ImGui::Spacing();
            }

            // Estimated impact
            if(rec["estimated_impact"].isString() &&
               !rec["estimated_impact"].getString().empty())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.30f, 0.82f, 0.42f, 1.0f));
                ImGui::Text("Expected Impact: %s",
                            rec["estimated_impact"].getString().c_str());
                ImGui::PopStyleColor();
                ImGui::Spacing();
            }

            // Profiling commands
            if(rec["commands"].isArray() && !rec["commands"].getArray().empty())
            {
                ImGui::TextUnformatted("Profiling Commands:");

                int ci = 0;
                for(jt::Json& cmd : rec["commands"].getArray())
                {
                    if(!cmd.isObject())
                    {
                        ++ci;
                        continue;
                    }
                    std::string tool     = cmd["tool"].isString() ? cmd["tool"].getString() : "";
                    std::string desc     = cmd["description"].isString()
                                              ? cmd["description"].getString()
                                              : "";
                    std::string full_cmd = cmd["full_command"].isString()
                                              ? cmd["full_command"].getString()
                                              : "";

                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.33f, 0.60f, 0.93f, 1.0f));  // #5599ee blue for tool
                    ImGui::BulletText("%s", tool.c_str());
                    ImGui::PopStyleColor();

                    if(!desc.empty() || !full_cmd.empty())
                    {
                        ImGui::Indent();

                        if(!desc.empty())
                        {
                            ImGui::TextWrapped("%s", desc.c_str());
                        }

                        if(!full_cmd.empty())
                        {
                            // Command box - dark background with green monospace text (like HTML)
                            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.06f, 0.08f, 1.0f));  // #0e0e14 dark bg
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.66f, 0.91f, 0.47f, 1.0f));  // #a8e878 green
                            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.17f, 0.17f, 0.27f, 1.0f));  // #2c2c44 border
                            std::string child_id = "cmd_" + std::to_string(idx) + "_" +
                                                   std::to_string(ci);
                            float cmd_h =
                                ImGui::GetTextLineHeightWithSpacing() * 1.6f;
                            ImGui::BeginChild(child_id.c_str(), ImVec2(-1.0f, cmd_h),
                                              ImGuiChildFlags_Borders);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() +
                                                 (cmd_h - ImGui::GetTextLineHeight()) *
                                                     0.5f);
                            ImGui::TextUnformatted(full_cmd.c_str());
                            ImGui::EndChild();
                            ImGui::PopStyleColor(3);
                        }

                        ImGui::Unindent();
                    }
                    ++ci;
                }
            }

            ImGui::Unindent();
        }

        ImGui::Spacing();
        ++idx;
    }

    ImGui::Unindent();
}

// ─── hotspots ────────────────────────────────────────────────────────────────

void
AiAnalysisView::RenderHotspots()
{
    auto& settings = SettingsManager::GetInstance();

    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.20f, 0.30f, 0.50f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.25f, 0.35f, 0.55f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.30f, 0.40f, 0.60f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    if(!ImGui::CollapsingHeader("Top Kernel Hotspots", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::PopStyleColor(4);
        return;
    }
    ImGui::PopStyleColor(4);

    jt::Json& hotspots = m_data["hotspots"];
    if(!hotspots.isArray() || hotspots.getArray().empty())
    {
        ImGui::Indent();
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
        ImGui::TextUnformatted("No kernel data available.");
        ImGui::PopStyleColor();
        ImGui::Unindent();
        return;
    }

    ImGui::Indent();  // MUCH larger font for table to match UI

    constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                       ImGuiTableFlags_Resizable |
                                       ImGuiTableFlags_SizingStretchProp |
                                       ImGuiTableFlags_ScrollY;

    const float row_h = ImGui::GetTextLineHeightWithSpacing() * 2.2f;  // Even taller rows
    const size_t n    = hotspots.getArray().size();
    // Make table very large - use available space or at least 600px
    const float avail_h = ImGui::GetContentRegionAvail().y;
    const float  tbl_h = std::max(std::min(static_cast<float>(n) * row_h + row_h * 2.5f, avail_h * 0.7f), 600.0f);

    if(ImGui::BeginTable("ai_hotspots", 7, kFlags, ImVec2(0, tbl_h)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Rank", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Kernel Name", ImGuiTableColumnFlags_WidthStretch, 6.0f);
        ImGui::TableSetupColumn("Calls", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Total Time", ImGuiTableColumnFlags_WidthStretch, 1.8f);
        ImGui::TableSetupColumn("Avg Time", ImGuiTableColumnFlags_WidthStretch, 1.5f);
        ImGui::TableSetupColumn("Min Time", ImGuiTableColumnFlags_WidthStretch, 1.5f);
        ImGui::TableSetupColumn("% Total", ImGuiTableColumnFlags_WidthFixed, 110.0f);

        // Custom header with better styling
        ImGui::TableNextRow(ImGuiTableRowFlags_Headers, row_h * 0.9f);
        for(int col = 0; col < 7; col++)
        {
            ImGui::TableSetColumnIndex(col);
            const char* column_name = ImGui::TableGetColumnName(col);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            ImGui::TableHeader(column_name);
            ImGui::PopStyleColor();
        }

        for(jt::Json& k : hotspots.getArray())
        {
            if(!k.isObject()) continue;

            int         rank  = k["rank"].isNumber() ? (int)k["rank"].getLong() : 0;
            std::string name  = k["name"].isString() ? k["name"].getString() : "";
            long long   calls = k["calls"].isNumber() ? k["calls"].getLong() : 0LL;
            long long   total =
                k["total_duration_ns"].isNumber() ? k["total_duration_ns"].getLong() : 0LL;
            double    avg = k["avg_duration_ns"].isNumber() ? k["avg_duration_ns"].getNumber() : 0.0;
            long long min_d =
                k["min_duration_ns"].isNumber() ? k["min_duration_ns"].getLong() : 0LL;
            double pct = k["pct_of_total"].isNumber() ? k["pct_of_total"].getNumber() : 0.0;

            // Highlight hot kernels (>20% of total)
            if(pct >= 20.0)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                       ImGui::ColorConvertFloat4ToU32(
                                           ImVec4(0.45f, 0.20f, 0.20f, 0.60f)));

            ImGui::TableNextRow(ImGuiTableRowFlags_None, row_h * 0.85f);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));  // Dark text for table

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", rank);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(name.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%lld", calls);
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(FormatNs(total).c_str());
            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(FormatNs(static_cast<long long>(avg)).c_str());
            ImGui::TableSetColumnIndex(5);
            ImGui::TextUnformatted(FormatNs(min_d).c_str());
            ImGui::TableSetColumnIndex(6);
            // Highlight high percentage
            if(pct >= 20.0)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.80f, 0.20f, 0.20f, 1.0f));  // Dark red
                ImGui::Text("%.1f%%", pct);
                ImGui::PopStyleColor();
            }
            else
            {
                ImGui::Text("%.1f%%", pct);
            }

            ImGui::PopStyleColor();

            // Add tooltip when hovering on the kernel name
            if(ImGui::TableGetColumnIndex() == 1 && ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                ImGui::Text("Kernel: %s", name.c_str());
                ImGui::Separator();
                ImGui::Text("Rank: #%d", rank);
                ImGui::Text("Calls: %lld", calls);
                ImGui::Text("Total: %s", FormatNs(total).c_str());
                ImGui::Text("Average: %s", FormatNs(static_cast<long long>(avg)).c_str());
                ImGui::Text("Minimum: %s", FormatNs(min_d).c_str());
                ImGui::Text("%% of Total: %.2f%%", pct);
                ImGui::PopStyleColor();
                ImGui::EndTooltip();
            }
        }

        ImGui::EndTable();
    }

    ImGui::Unindent();
}

// ─── memory analysis ─────────────────────────────────────────────────────────

void
AiAnalysisView::RenderMemoryAnalysis()
{
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.20f, 0.30f, 0.50f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.25f, 0.35f, 0.55f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.30f, 0.40f, 0.60f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    if(!ImGui::CollapsingHeader("Memory Transfer Analysis",
                                ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::PopStyleColor(4);
        return;
    }
    ImGui::PopStyleColor(4);

    jt::Json& mem = m_data["memory_analysis"];
    if(!mem.isObject())
    {
        ImGui::Indent();
        ImGui::TextUnformatted("No memory transfer data.");
        ImGui::Unindent();
        return;
    }

    ImGui::Indent();

    constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                       ImGuiTableFlags_Resizable |
                                       ImGuiTableFlags_SizingStretchProp;

    const float row_h = ImGui::GetTextLineHeightWithSpacing() * 2.0f;

    if(ImGui::BeginTable("ai_mem", 6, kFlags, ImVec2(0, row_h * 7.5f)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Direction", ImGuiTableColumnFlags_WidthStretch, 2.8f);
        ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Total Bytes", ImGuiTableColumnFlags_WidthStretch, 1.5f);
        ImGui::TableSetupColumn("Total Time", ImGuiTableColumnFlags_WidthStretch, 1.5f);
        ImGui::TableSetupColumn("Avg Size", ImGuiTableColumnFlags_WidthStretch, 1.5f);
        ImGui::TableSetupColumn("Bandwidth", ImGuiTableColumnFlags_WidthStretch, 1.5f);

        // Custom header with better styling
        ImGui::TableNextRow(ImGuiTableRowFlags_Headers, row_h * 0.9f);
        for(int col = 0; col < 6; col++)
        {
            ImGui::TableSetColumnIndex(col);
            const char* column_name = ImGui::TableGetColumnName(col);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            ImGui::TableHeader(column_name);
            ImGui::PopStyleColor();
        }

        const char* directions[] = {"Host-to-Device", "Device-to-Host", "Device-to-Device",
                                    "Peer-to-Peer", "Unknown"};

        for(const char* dir : directions)
        {
            jt::Json& entry = mem[dir];
            if(!entry.isObject()) continue;

            long long count =
                entry["count"].isNumber() ? entry["count"].getLong() : 0LL;
            long long total_b =
                entry["total_bytes"].isNumber() ? entry["total_bytes"].getLong() : 0LL;
            long long total_t = entry["total_duration_ns"].isNumber()
                                    ? entry["total_duration_ns"].getLong()
                                    : 0LL;
            double avg_b  = entry["avg_bytes"].isNumber() ? entry["avg_bytes"].getNumber() : 0.0;
            double bw_gbs = entry["bandwidth_gbps"].isNumber()
                                ? entry["bandwidth_gbps"].getNumber()
                                : 0.0;

            ImGui::TableNextRow(ImGuiTableRowFlags_None, row_h * 0.85f);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));  // Dark text for table

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(dir);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%lld", count);
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(FormatBytes(static_cast<double>(total_b)).c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(FormatNs(total_t).c_str());
            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(FormatBytes(avg_b).c_str());
            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%.2f GB/s", bw_gbs);

            ImGui::PopStyleColor();
        }

        ImGui::EndTable();
    }

    ImGui::Unindent();
}

// ─── hardware counters ────────────────────────────────────────────────────────

void
AiAnalysisView::RenderHardwareCounters()
{
    auto& settings = SettingsManager::GetInstance();

    jt::Json& hw = m_data["hardware_counters"];
    if(!hw.isObject()) return;

    const bool has_counters =
        hw["has_counters"].isBool() && hw["has_counters"].getBool();
    const ImGuiTreeNodeFlags flags =
        has_counters ? ImGuiTreeNodeFlags_DefaultOpen : 0;

    if(!ImGui::CollapsingHeader("Hardware Counters", flags)) return;

    ImGui::Indent();

    if(!has_counters)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
        ImGui::TextUnformatted("No hardware counter data in this trace.");
        ImGui::Spacing();
        ImGui::TextUnformatted(
            "Collect counters with:   rocprofv3 --pmc GRBM_COUNT GRBM_GUI_ACTIVE "
            "SQ_WAVES -- ./app");
        ImGui::PopStyleColor();
        ImGui::Unindent();
        return;
    }

    jt::Json& metrics = hw["metrics"];
    if(metrics.isObject())
    {
        // GPU utilization bar
        if(metrics["gpu_utilization_pct"].isNumber())
        {
            double util = metrics["gpu_utilization_pct"].getNumber();

            ImGui::Text("GPU Utilization:");
            if(ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                ImGui::Text("GPU Utilization");
                ImGui::Separator();
                ImGui::TextWrapped("Percentage of time the GPU was actively executing work.");
                ImGui::Spacing();
                ImGui::TextWrapped("Why it matters:");
                ImGui::BulletText("High utilization (>70%%) = GPU is kept busy");
                ImGui::BulletText("Low utilization (<70%%) = GPU is waiting or underutilized");
                ImGui::Spacing();
                ImGui::TextWrapped("How to improve:");
                ImGui::BulletText("Launch more work or larger kernels");
                ImGui::BulletText("Reduce CPU-GPU synchronization");
                ImGui::BulletText("Overlap compute with memory transfers");
                ImGui::PopStyleColor();
                ImGui::EndTooltip();
            }
            ImGui::SameLine(190.0f);

            char overlay[32];
            std::snprintf(overlay, sizeof(overlay), "%.1f%%", util);

            ImVec4 bar_color = util >= 70.0 ? ImVec4(0.20f, 0.80f, 0.32f, 1.0f)
                                            : ImVec4(0.90f, 0.42f, 0.12f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, bar_color);
            ImGui::ProgressBar(static_cast<float>(util / 100.0), ImVec2(-1.0f, 18.0f),
                               overlay);
            ImGui::PopStyleColor();

            if(ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                ImGui::Text("Current: %.1f%%", util);
                if(util >= 70.0)
                    ImGui::TextWrapped("Good! GPU is well-utilized.");
                else
                    ImGui::TextWrapped("Low - GPU is underutilized. See recommendations below.");
                ImGui::PopStyleColor();
                ImGui::EndTooltip();
            }

            if(util < 70.0)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.42f, 0.12f, 1.0f));
                ImGui::TextUnformatted(
                    "  Low GPU utilization — consider increasing parallelism or "
                    "hiding latency with more waves.");
                ImGui::PopStyleColor();
            }
            ImGui::Spacing();
        }

        // Wave occupancy
        if(metrics["avg_waves"].isNumber())
        {
            double avg_waves = metrics["avg_waves"].getNumber();
            double max_waves =
                metrics["max_waves"].isNumber() ? metrics["max_waves"].getNumber() : avg_waves;
            double min_waves =
                metrics["min_waves"].isNumber() ? metrics["min_waves"].getNumber() : avg_waves;
            ImGui::Text("Wave Count:  avg %.0f   max %.0f   min %.0f", avg_waves,
                        max_waves, min_waves);

            if(ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                ImGui::Text("Wave Occupancy");
                ImGui::Separator();
                ImGui::TextWrapped("Number of wavefronts (groups of 64 threads) executing simultaneously per SIMD unit.");
                ImGui::Spacing();
                ImGui::TextWrapped("Why it matters:");
                ImGui::BulletText("Higher waves = better GPU utilization");
                ImGui::BulletText("AMD GPUs can typically run 16-32 waves per CU");
                ImGui::BulletText("Low waves (<16) = resources are limiting parallelism");
                ImGui::Spacing();
                ImGui::TextWrapped("Common limiters:");
                ImGui::BulletText("Too many registers per thread");
                ImGui::BulletText("Too much LDS (local shared memory) per workgroup");
                ImGui::BulletText("Workgroup size too small");
                ImGui::Spacing();
                ImGui::TextWrapped("How to improve:");
                ImGui::BulletText("Reduce register pressure (use fewer variables)");
                ImGui::BulletText("Reduce LDS usage or increase workgroup size");
                ImGui::BulletText("Use rocprof-compute to analyze occupancy limits");
                ImGui::PopStyleColor();
                ImGui::EndTooltip();
            }

            if(avg_waves < 16.0)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.58f, 0.10f, 1.0f));
                ImGui::TextUnformatted(
                    "  Low wave occupancy — check register usage and LDS allocation.");
                ImGui::PopStyleColor();
            }
        }
    }

    ImGui::Unindent();
}

// ─── warnings ────────────────────────────────────────────────────────────────

void
AiAnalysisView::RenderWarnings()
{
    auto& settings = SettingsManager::GetInstance();

    jt::Json& warnings = m_data["warnings"];
    jt::Json& errors   = m_data["errors"];

    const bool has_warnings =
        warnings.isArray() && !warnings.getArray().empty();
    const bool has_errors = errors.isArray() && !errors.getArray().empty();

    if(!has_warnings && !has_errors) return;

    if(!ImGui::CollapsingHeader("Warnings & Errors")) return;

    ImGui::Indent();

    if(has_errors)
    {
        for(jt::Json& err : errors.getArray())
        {
            if(err.isString())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.30f, 0.30f, 1.0f));
                ImGui::BulletText("[ERROR] %s", err.getString().c_str());
                ImGui::PopStyleColor();
            }
        }
        if(has_warnings) ImGui::Spacing();
    }

    if(has_warnings)
    {
        for(jt::Json& w : warnings.getArray())
        {
            if(!w.isObject()) continue;

            std::string sev = w["severity"].isString() ? w["severity"].getString() : "warning";
            std::string msg = w["message"].isString() ? w["message"].getString() : "";
            std::string rec =
                w["recommendation"].isString() ? w["recommendation"].getString() : "";

            ImVec4 color = (sev == "warning")
                               ? ImVec4(0.90f, 0.62f, 0.12f, 1.0f)
                               : ImGui::ColorConvertU32ToFloat4(
                                     settings.GetColor(Colors::kTextDim));
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::BulletText("[%s] %s", sev.c_str(), msg.c_str());
            ImGui::PopStyleColor();

            if(!rec.empty())
            {
                ImGui::Indent();
                ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
                ImGui::TextWrapped("Tip: %s", rec.c_str());
                ImGui::PopStyleColor();
                ImGui::Unindent();
            }
        }
    }

    ImGui::Unindent();
}

}  // namespace View
}  // namespace RocProfVis
