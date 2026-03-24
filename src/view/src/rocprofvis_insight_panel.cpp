// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_insight_panel.h"
#include "imgui.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_events.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include <algorithm>
#include <cmath>

namespace RocProfVis
{
namespace View
{

InsightPanel::InsightPanel()
: m_state(State::kIdle)
, m_filter_hotspot(true)
, m_filter_gpu_starvation(true)
, m_filter_cpu_blocked(true)
, m_filter_sync_barrier(true)
, m_filter_load_imbalance(true)
, m_filter_counter_anomaly(true)
, m_filter_serialisation(true)
, m_filter_critical(true)
, m_filter_warning(true)
, m_filter_info(true)
{}

void
InsightPanel::SetResults(std::vector<Insight>&& insights)
{
    m_insights = std::move(insights);
    m_state    = State::kReady;
}

void
InsightPanel::SetRunning()
{
    m_insights.clear();
    m_state = State::kRunning;
}

// ===========================================================================
// Label / color helpers
// ===========================================================================

ImU32
InsightPanel::SeverityColor(InsightSeverity sev) const
{
    SettingsManager& sm = SettingsManager::GetInstance();
    switch(sev)
    {
    case InsightSeverity::kCritical: return sm.GetColor(Colors::kBgError);
    case InsightSeverity::kWarning: return sm.GetColor(Colors::kBgWarning);
    case InsightSeverity::kInfo:
    default: return sm.GetColor(Colors::kBgSuccess);
    }
}

const char*
InsightPanel::SeverityLabel(InsightSeverity sev) const
{
    switch(sev)
    {
    case InsightSeverity::kCritical: return "CRITICAL";
    case InsightSeverity::kWarning: return "WARNING";
    case InsightSeverity::kInfo:
    default: return "INFO";
    }
}

const char*
InsightPanel::CategoryLabel(InsightCategory cat) const
{
    switch(cat)
    {
    case InsightCategory::kHotspot: return "Hotspot";
    case InsightCategory::kGpuStarvation: return "GPU Starvation";
    case InsightCategory::kCpuBlocked: return "CPU Blocked";
    case InsightCategory::kSyncBarrier: return "Sync Barrier";
    case InsightCategory::kLoadImbalance: return "Load Imbalance";
    case InsightCategory::kCounterAnomaly: return "Counter Anomaly";
    case InsightCategory::kSerialisation: return "Serialisation";
    default: return "Unknown";
    }
}

const char*
InsightPanel::CategoryIcon(InsightCategory cat) const
{
    // Simple text markers — avoids needing new icon font codepoints
    switch(cat)
    {
    case InsightCategory::kHotspot: return "[H]";
    case InsightCategory::kGpuStarvation: return "[G]";
    case InsightCategory::kCpuBlocked: return "[C]";
    case InsightCategory::kSyncBarrier: return "[S]";
    case InsightCategory::kLoadImbalance: return "[L]";
    case InsightCategory::kCounterAnomaly: return "[#]";
    case InsightCategory::kSerialisation: return "[|]";
    default: return "[?]";
    }
}

bool
InsightPanel::PassesFilter(const Insight& ins) const
{
    // Severity filter
    switch(ins.severity)
    {
    case InsightSeverity::kCritical:
        if(!m_filter_critical) return false;
        break;
    case InsightSeverity::kWarning:
        if(!m_filter_warning) return false;
        break;
    case InsightSeverity::kInfo:
        if(!m_filter_info) return false;
        break;
    }

    // Category filter
    switch(ins.category)
    {
    case InsightCategory::kHotspot:
        if(!m_filter_hotspot) return false;
        break;
    case InsightCategory::kGpuStarvation:
        if(!m_filter_gpu_starvation) return false;
        break;
    case InsightCategory::kCpuBlocked:
        if(!m_filter_cpu_blocked) return false;
        break;
    case InsightCategory::kSyncBarrier:
        if(!m_filter_sync_barrier) return false;
        break;
    case InsightCategory::kLoadImbalance:
        if(!m_filter_load_imbalance) return false;
        break;
    case InsightCategory::kCounterAnomaly:
        if(!m_filter_counter_anomaly) return false;
        break;
    case InsightCategory::kSerialisation:
        if(!m_filter_serialisation) return false;
        break;
    }

    return true;
}

// ===========================================================================
// Render
// ===========================================================================

void
InsightPanel::Render()
{
    if(m_state == State::kRunning)
    {
        float y = ImGui::GetContentRegionAvail().y * 0.5f;
        ImGui::Dummy(ImVec2(0, y - ImGui::GetFrameHeight()));
        float w     = ImGui::GetContentRegionAvail().x;
        float t     = static_cast<float>(ImGui::GetTime());
        int   dots  = (static_cast<int>(t * 3.0f)) % 4;
        char  buf[32];
        std::snprintf(buf, sizeof(buf), "Analyzing%.*s", dots, "...");
        ImVec2 sz = ImGui::CalcTextSize(buf);
        ImGui::SetCursorPosX((w - sz.x) * 0.5f);
        ImGui::TextUnformatted(buf);
        return;
    }

    if(m_state == State::kIdle)
    {
        RenderEmptyState();
        return;
    }

    // State::kReady
    RenderHeader();
    ImGui::Separator();
    RenderFilters();
    ImGui::Separator();

    if(m_insights.empty())
    {
        ImGui::Spacing();
        ImGui::TextWrapped("No bottlenecks detected. The trace appears healthy.");
        return;
    }

    // Scrollable insight list
    ImGui::BeginChild("InsightList", ImVec2(0, 0), false);
    int index = 0;
    for(const auto& ins : m_insights)
    {
        if(PassesFilter(ins))
        {
            RenderInsightCard(ins, index);
            ++index;
        }
    }
    ImGui::EndChild();
}

void
InsightPanel::RenderHeader()
{
    SettingsManager& sm = SettingsManager::GetInstance();

    int crit = 0, warn = 0, info = 0;
    for(const auto& ins : m_insights)
    {
        switch(ins.severity)
        {
        case InsightSeverity::kCritical: ++crit; break;
        case InsightSeverity::kWarning: ++warn; break;
        case InsightSeverity::kInfo: ++info; break;
        }
    }

    ImGui::Spacing();

    // Summary badges
    auto Badge = [&](const char* label, int count, ImU32 col) {
        if(count == 0) return;
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2      pos = ImGui::GetCursorScreenPos();
        char        buf[64];
        std::snprintf(buf, sizeof(buf), "%s: %d", label, count);
        ImVec2 txt_sz  = ImGui::CalcTextSize(buf);
        float  pad     = 6.0f;
        ImVec2 box_min = pos;
        ImVec2 box_max = ImVec2(pos.x + txt_sz.x + pad * 2, pos.y + txt_sz.y + pad);
        dl->AddRectFilled(box_min, box_max, col, 4.0f);
        dl->AddText(ImVec2(pos.x + pad, pos.y + pad * 0.5f),
                    sm.GetColor(Colors::kTextOnAccent), buf);
        ImGui::Dummy(ImVec2(txt_sz.x + pad * 2, txt_sz.y + pad));
        ImGui::SameLine();
    };

    Badge("Critical", crit, sm.GetColor(Colors::kBgError));
    Badge("Warning", warn, sm.GetColor(Colors::kBgWarning));
    Badge("Info", info, sm.GetColor(Colors::kBgSuccess));

    ImGui::NewLine();

    ImGui::Text("Total insights: %d", static_cast<int>(m_insights.size()));
    ImGui::Spacing();
}

void
InsightPanel::RenderFilters()
{
    ImGui::Spacing();
    ImGui::Text("Severity:");
    ImGui::SameLine();
    ImGui::Checkbox("Critical##sev", &m_filter_critical);
    ImGui::SameLine();
    ImGui::Checkbox("Warning##sev", &m_filter_warning);
    ImGui::SameLine();
    ImGui::Checkbox("Info##sev", &m_filter_info);

    ImGui::Text("Category:");
    ImGui::SameLine();
    ImGui::Checkbox("Hotspot", &m_filter_hotspot);
    ImGui::SameLine();
    ImGui::Checkbox("GPU Starv.", &m_filter_gpu_starvation);
    ImGui::SameLine();
    ImGui::Checkbox("CPU Block", &m_filter_cpu_blocked);
    ImGui::SameLine();
    ImGui::Checkbox("Sync", &m_filter_sync_barrier);

    ImGui::Text("         ");
    ImGui::SameLine();
    ImGui::Checkbox("Imbalance", &m_filter_load_imbalance);
    ImGui::SameLine();
    ImGui::Checkbox("Counter", &m_filter_counter_anomaly);
    ImGui::SameLine();
    ImGui::Checkbox("Serial", &m_filter_serialisation);

    ImGui::Spacing();
}

void
InsightPanel::RenderInsightCard(const Insight& insight, int index)
{
    SettingsManager& sm = SettingsManager::GetInstance();
    ImGui::PushID(index);

    ImGui::Spacing();
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetCursorScreenPos();
    float       w   = ImGui::GetContentRegionAvail().x;

    // Card background
    float  card_h = 0;
    ImVec2 card_start = pos;

    // Severity colour bar on the left edge
    ImU32 sev_color = SeverityColor(insight.severity);

    ImGui::BeginGroup();

    // Title line: [icon] Category — Severity
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(sev_color), "%s %s",
                       CategoryIcon(insight.category),
                       SeverityLabel(insight.severity));
    ImGui::SameLine();
    ImGui::Text("  %s", insight.title.c_str());

    // Description
    ImGui::PushStyleColor(ImGuiCol_Text, sm.GetColor(Colors::kTextMain));
    ImGui::TextWrapped("%s", insight.description.c_str());
    ImGui::PopStyleColor();

    // Suggestion
    ImGui::PushStyleColor(ImGuiCol_Text, sm.GetColor(Colors::kTextDim));
    ImGui::TextWrapped("Suggestion: %s", insight.suggestion.c_str());
    ImGui::PopStyleColor();

    // Score indicator
    char score_buf[64];
    std::snprintf(score_buf, sizeof(score_buf), "Impact: %.2f", insight.score);
    ImGui::Text("%s", score_buf);

    ImGui::SameLine(w - 70.0f);
    if(ImGui::Button("Go To"))
    {
        NavigateToInsight(insight);
    }
    if(ImGui::IsItemHovered())
    {
        SetTooltipStyled("Navigate to this region in the timeline");
    }

    ImGui::EndGroup();

    // Draw the severity accent bar on the left
    card_h = ImGui::GetItemRectSize().y;
    dl->AddRectFilled(card_start, ImVec2(card_start.x + 3.0f, card_start.y + card_h),
                      sev_color);

    ImGui::Separator();
    ImGui::PopID();
}

void
InsightPanel::RenderEmptyState()
{
    float y = ImGui::GetContentRegionAvail().y * 0.5f;
    ImGui::Dummy(ImVec2(0, y - ImGui::GetFrameHeight()));
    float  w  = ImGui::GetContentRegionAvail().x;
    const char* msg = "Click \"Analyze\" to detect performance bottlenecks.";
    ImVec2 sz = ImGui::CalcTextSize(msg);
    ImGui::SetCursorPosX((w - sz.x) * 0.5f);
    ImGui::TextUnformatted(msg);
}

void
InsightPanel::NavigateToInsight(const Insight& insight)
{
    double margin = (insight.time_end - insight.time_start) * 0.15;
    double v_min  = insight.time_start - margin;
    double v_max  = insight.time_end + margin;

    EventManager::GetInstance()->AddEvent(
        std::make_shared<NavigationEvent>(v_min, v_max, 0.0, false));
}

}  // namespace View
}  // namespace RocProfVis
