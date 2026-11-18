// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// The Ask Optiq panel's drawing half: the dock and its splitter, the
// transcript and its cards, the composer, the context pill, the activity
// strip, and the toolbar button that opens it. Everything here reads panel
// state and emits ImGui; the turn machinery that fills that state lives in
// rocprofvis_ai_assistant.cpp, and the two only meet through AssistantPanel
// members.

#include "rocprofvis_ai_assistant.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "imgui.h"
#include "textselect.hpp"

#include "icons/rocprovfis_icon_defines.h"
#include "model/rocprofvis_summary_model.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_render_scheduler.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_notification_manager.h"

namespace RocProfVis
{
namespace View
{

namespace
{

constexpr float  ASSISTANT_MIN_WIDTH      = 440.0f;
constexpr float  ASSISTANT_MAX_WIDTH      = 900.0f;
constexpr float  ASSISTANT_SPLITTER_WIDTH = 6.0f;
// Same padding the remote/profiler dialogs use.
constexpr ImVec2 ASSISTANT_WINDOW_PADDING = ImVec2(14.0f, 12.0f);
constexpr ImVec2 ASSISTANT_CARD_PADDING   = ImVec2(14.0f, 10.0f);
constexpr float  ASSISTANT_SEND_WIDTH     = 80.0f;
// Composer row: input, Send, and three icon buttons (copy, export, clear),
// with four gaps between the five items.
constexpr float  ASSISTANT_COMPOSER_MIN_INPUT_WIDTH = 80.0f;
constexpr int    ASSISTANT_COMPOSER_ICON_BUTTONS    = 3;
constexpr int    ASSISTANT_COMPOSER_GAP_COUNT       = 4;
constexpr float  ASSISTANT_DOT_RADIUS     = 2.5f;
constexpr int    ASSISTANT_DOT_COUNT      = 3;
constexpr float  ASSISTANT_DOT_SPACING    = 4.0f;
constexpr float  ASSISTANT_DOT_SPEED      = 5.0f;

constexpr size_t   ASSISTANT_CHART_MAX_BINS        = 64;
constexpr size_t   ASSISTANT_CHART_MIN_BINS        = 16;
constexpr float    ASSISTANT_CHART_PX_PER_BIN      = 5.0f;
constexpr size_t   ASSISTANT_CHART_ROWS            = 5;
constexpr float    ASSISTANT_CHART_HEIGHT          = 52.0f;
constexpr float    ASSISTANT_CHART_ROW_HEIGHT      = 12.0f;
constexpr float    ASSISTANT_CHART_ROW_GAP         = 2.0f;
constexpr float    ASSISTANT_CHART_MIN_ALPHA       = 0.12f;
// The context pill under the title. Its ring turns to a warning, then an error,
// as the history nears the point where older tool results are shortened.
constexpr float ASSISTANT_CONTEXT_WARN_FILL   = 0.70f;
constexpr float ASSISTANT_CONTEXT_HOT_FILL    = 0.90f;
constexpr float ASSISTANT_CONTEXT_RING_STROKE = 2.0f;
// Smallest arc drawn for a conversation that is not empty, so a short chat
// still reads as started rather than as an empty ring.
constexpr float ASSISTANT_CONTEXT_MIN_ARC    = 0.04f;
constexpr float ASSISTANT_CONTEXT_CHIP_PAD_X = 8.0f;
constexpr float ASSISTANT_CONTEXT_CHIP_PAD_Y = 2.0f;
// Space reserved under the pill. The title's line height leaves room above its
// glyphs that the pill's hard edge does not, so without this the header looks
// heavier at the bottom than at the top.
constexpr float ASSISTANT_CONTEXT_CHIP_BELOW    = 2.5f;
constexpr float ASSISTANT_CONTEXT_CHIP_GAP      = 5.0f;
constexpr float ASSISTANT_CONTEXT_DIVIDER_GAP   = 8.0f;
constexpr float ASSISTANT_CONTEXT_TIP_WIDTH     = 300.0f;
constexpr float ASSISTANT_CONTEXT_TIP_BAR_H     = 6.0f;
constexpr float ASSISTANT_CONTEXT_TIP_NUM_WIDTH = 72.0f;
// imgui.h does not publish IM_PI.
constexpr float ASSISTANT_PI = 3.14159265f;

// Groups digits so a token count can be read at a glance. 12481 -> "12,481".
std::string
FormatCount(size_t value)
{
    const std::string digits = std::to_string(value);
    std::string       out;
    out.reserve(digits.size() + digits.size() / 3);
    for(size_t i = 0; i < digits.size(); ++i)
    {
        if(i > 0 && (digits.size() - i) % 3 == 0)
        {
            out += ',';
        }
        out += digits[i];
    }
    return out;
}

// Compact count for the pill: 386, 2.2k, 45.1k, 120k, 1.2M. Full counts stay in
// the tooltip.
std::string
FormatTokensShort(size_t tokens)
{
    if(tokens < 1000)
    {
        return std::to_string(tokens);
    }
    const char* suffix = "k";
    double      value  = static_cast<double>(tokens) / 1000.0;
    if(tokens >= 999500)
    {
        suffix = "M";
        value  = static_cast<double>(tokens) / 1000000.0;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), value >= 100.0 ? "%.0f" : "%.1f", value);
    std::string text = buf;
    if(text.size() > 2 && text.compare(text.size() - 2, 2, ".0") == 0)
    {
        text.resize(text.size() - 2);
    }
    return text + suffix;
}

// How full the kept history is, from 0 to 1.
float
ContextFill(size_t used_chars, size_t budget_chars)
{
    if(budget_chars == 0)
    {
        return 0.0f;
    }
    return std::min(1.0f,
                    static_cast<float>(used_chars) / static_cast<float>(budget_chars));
}

ImU32
ContextFillColor(float fill)
{
    SettingsManager& settings = SettingsManager::GetInstance();
    if(fill >= ASSISTANT_CONTEXT_HOT_FILL)
    {
        return settings.GetColor(Colors::kTextError);
    }
    if(fill >= ASSISTANT_CONTEXT_WARN_FILL)
    {
        return settings.GetColor(Colors::kTextWarning);
    }
    return settings.GetColor(Colors::kAccent);
}

// Percent of the history budget, never showing a started chat as 0%.
std::string
FormatFillPercent(float fill)
{
    if(fill <= 0.0f)
    {
        return "0%";
    }
    const long percent = std::lround(fill * 100.0f);
    return percent < 1 ? std::string("<1%") : std::to_string(percent) + "%";
}

// Text that sits right-aligned in the space left on the current line or cell.
void
RightAlignedText(const std::string& text, ImU32 color)
{
    const float width = ImGui::CalcTextSize(text.c_str()).x;
    const float avail = ImGui::GetContentRegionAvail().x;
    if(avail > width)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - width);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopStyleColor();
}

// Breaks a button label onto extra lines so a long follow-up still fits.
std::string
WrapButtonLabel(const std::string& text, float wrap_width)
{
    if(text.empty() || wrap_width <= 1.0f)
    {
        return text;
    }
    if(ImGui::CalcTextSize(text.c_str()).x <= wrap_width)
    {
        return text;
    }

    std::string out;
    std::string line;
    size_t      i = 0;
    while(i < text.size())
    {
        const size_t space = text.find(' ', i);
        const size_t next  = (space == std::string::npos) ? text.size() : space;
        const std::string word = text.substr(i, next - i);
        const std::string candidate = line.empty() ? word : line + " " + word;
        if(!line.empty() && ImGui::CalcTextSize(candidate.c_str()).x > wrap_width)
        {
            if(!out.empty())
            {
                out += "\n";
            }
            out += line;
            line = word;
        }
        else
        {
            line = candidate;
        }
        i = (next == text.size()) ? next : next + 1;
    }
    if(!line.empty())
    {
        if(!out.empty())
        {
            out += "\n";
        }
        out += line;
    }
    return out.empty() ? text : out;
}

std::vector<std::string>
SplitMessageLines(const std::string& text)
{
    std::vector<std::string> lines;
    size_t                   start = 0;
    do
    {
        const size_t end = text.find('\n', start);
        if(end == std::string::npos)
        {
            lines.emplace_back(text, start, text.size() - start);
            break;
        }
        lines.emplace_back(text, start, end - start);
        start = end + 1;
    } while(start <= text.size());
    return lines;
}

}  // namespace

void
AssistantPanel::RenderActivityChart(uint64_t track_id)
{
    const AssistantToolContext context = MakeToolContext();
    if(context.data_provider == nullptr)
    {
        return;
    }

    // Bin count follows the column width, so bars stay legible when the dock is
    // dragged narrow.
    const float  avail_width = ImGui::GetContentRegionAvail().x;
    const size_t bin_count   = static_cast<size_t>(
        std::clamp(avail_width / ASSISTANT_CHART_PX_PER_BIN,
                     static_cast<float>(ASSISTANT_CHART_MIN_BINS),
                     static_cast<float>(ASSISTANT_CHART_MAX_BINS)));

    const std::vector<double> bins =
        GetAssistantActivityBins(context, track_id, bin_count);
    if(bins.empty())
    {
        return;
    }

    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();
    const ImU32       bg       = settings.GetColor(Colors::kBgFrame);
    const ImU32       bar      = settings.GetColor(Colors::kLineChartColor);
    const ImU32       border   = settings.GetColor(Colors::kBorderColor);
    const ImVec4      accent =
        ImGui::ColorConvertU32ToFloat4(settings.GetColor(Colors::kAccent));

    ImDrawList*  draw  = ImGui::GetWindowDrawList();
    const float  width = ImGui::GetContentRegionAvail().x;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float  bar_width = width / static_cast<float>(bins.size());

    draw->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + ASSISTANT_CHART_HEIGHT),
                        bg);
    for(size_t i = 0; i < bins.size(); ++i)
    {
        const float height =
            static_cast<float>(bins[i]) * (ASSISTANT_CHART_HEIGHT - 2.0f);
        if(height <= 0.0f)
        {
            continue;
        }
        const float x0 = origin.x + bar_width * static_cast<float>(i);
        const float y1 = origin.y + ASSISTANT_CHART_HEIGHT - 1.0f;
        draw->AddRectFilled(ImVec2(x0, y1 - height),
                            ImVec2(x0 + std::max(1.0f, bar_width - 1.0f), y1), bar);
    }
    draw->AddRect(origin, ImVec2(origin.x + width, origin.y + ASSISTANT_CHART_HEIGHT),
                  border);
    ImGui::Dummy(ImVec2(width, ASSISTANT_CHART_HEIGHT));

    if(track_id != INVALID_UINT64_INDEX)
    {
        ImGui::TextDisabled("Track %llu", static_cast<unsigned long long>(track_id));
        return;
    }

    const std::vector<AssistantActivityRow> rows =
        GetAssistantActivityRows(context, bin_count, ASSISTANT_CHART_ROWS);
    if(rows.empty())
    {
        return;
    }

    ImGui::Spacing();

    // Size the gutter to the widest id present; a fixed width would eat a chunk
    // of a narrow column.
    float label_width = 0.0f;
    for(const AssistantActivityRow& row : rows)
    {
        char measure[32];
        std::snprintf(measure, sizeof(measure), "t%llu",
                      static_cast<unsigned long long>(row.track_id));
        label_width = std::max(label_width, ImGui::CalcTextSize(measure).x);
    }
    label_width += style.ItemInnerSpacing.x * 2.0f;

    const float strip_width = std::max(1.0f, width - label_width);
    const ImU32 label_color = settings.GetColor(Colors::kTextDim);

    // Rows butt up against each other so they read as one strip; default item
    // spacing would band them.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(0.0f, ASSISTANT_CHART_ROW_GAP));
    for(const AssistantActivityRow& row : rows)
    {
        if(row.bins.empty())
        {
            continue;
        }

        const ImVec2 row_origin = ImGui::GetCursorScreenPos();
        const float  row_height =
            std::max(ASSISTANT_CHART_ROW_HEIGHT, ImGui::GetTextLineHeight());

        ImGui::PushID(static_cast<int>(row.track_id));
        ImGui::InvisibleButton("##minimap_row", ImVec2(width, row_height));
        ImGui::PopID();
        if(ImGui::IsItemHovered())
        {
            SetTooltipStyled("%s", row.name.c_str());
        }

        char label[32];
        std::snprintf(label, sizeof(label), "t%llu",
                      static_cast<unsigned long long>(row.track_id));
        draw->AddText(row_origin, label_color, label);

        // Fill the lane first, so idle stretches read as gaps rather than holes.
        const float x_start = row_origin.x + label_width;
        draw->AddRectFilled(ImVec2(x_start, row_origin.y),
                            ImVec2(x_start + strip_width, row_origin.y + row_height),
                            bg);

        const float cell_width = strip_width / static_cast<float>(row.bins.size());
        for(size_t i = 0; i < row.bins.size(); ++i)
        {
            const float value = static_cast<float>(row.bins[i]);
            if(value <= 0.0f)
            {
                continue;
            }
            ImVec4 cell = accent;
            cell.w = ASSISTANT_CHART_MIN_ALPHA + value * (1.0f - ASSISTANT_CHART_MIN_ALPHA);
            const float x0 = x_start + cell_width * static_cast<float>(i);
            draw->AddRectFilled(ImVec2(x0, row_origin.y),
                                ImVec2(x0 + std::max(1.0f, cell_width),
                                       row_origin.y + row_height),
                                ImGui::ColorConvertFloat4ToU32(cell));
        }
    }
    ImGui::PopStyleVar();

    ImGui::TextDisabled("Busiest tracks. Hover for names.");
}

void
AssistantPanel::RenderToolbarButton()
{
    SettingsManager& settings = SettingsManager::GetInstance();
    ImGui::PushStyleColor(ImGuiCol_Button,
                          ImGui::ColorConvertU32ToFloat4(settings.GetColor(Colors::kBgFrame)));
    ImGui::PushStyleColor(
        ImGuiCol_ButtonHovered,
        ImGui::ColorConvertU32ToFloat4(settings.GetColor(Colors::kButtonHovered)));
    ImGui::PushStyleColor(
        ImGuiCol_ButtonActive,
        ImGui::ColorConvertU32ToFloat4(settings.GetColor(Colors::kButtonActive)));
    ImGui::PushStyleColor(ImGuiCol_Text,
                          ImGui::ColorConvertU32ToFloat4(settings.GetColor(Colors::kTextMain)));
    if(ImGui::Button("Ask Optiq"))
    {
        GetInstance()->ToggleVisible();
    }
    ImGui::PopStyleColor(4);
    if(ImGui::IsItemHovered())
    {
        SetTooltipStyled("Open the assistant. Configure the URL and key in "
                         "Edit > Preferences > Assistant.");
    }
}

// Kept for the RocWidget contract; AppWindow draws the panel through RenderDocked.
void
AssistantPanel::Render()
{
    RenderDocked();
}

float
AssistantPanel::DockedWidth() const
{
    return m_visible ? m_dock_width + ASSISTANT_SPLITTER_WIDTH : 0.0f;
}

// Drag handle between the main view and the panel, matching the topology
// sidebar on the other side of the window.
void
AssistantPanel::RenderSplitter()
{
    SettingsManager& settings = SettingsManager::GetInstance();
    const ImVec2     origin   = ImGui::GetCursorScreenPos();
    const float      height   = ImGui::GetContentRegionAvail().y;

    ImGui::InvisibleButton("##assistant_splitter",
                           ImVec2(ASSISTANT_SPLITTER_WIDTH, height));
    const bool hovered = ImGui::IsItemHovered();
    const bool active  = ImGui::IsItemActive();
    if(hovered || active)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    if(active)
    {
        // Dragging left widens the panel, so the delta is negated.
        m_dock_width = std::clamp(m_dock_width - ImGui::GetIO().MouseDelta.x,
                                  ASSISTANT_MIN_WIDTH, ASSISTANT_MAX_WIDTH);
    }

    ImGui::GetWindowDrawList()->AddRectFilled(
        origin, ImVec2(origin.x + ASSISTANT_SPLITTER_WIDTH, origin.y + height),
        settings.GetColor(hovered || active ? Colors::kAccent
                                            : Colors::kSplitterColor));
}

void
AssistantPanel::RenderDocked()
{
    if(!m_visible)
    {
        return;
    }

    // A width persisted below the current minimum would otherwise stick until
    // the splitter was dragged.
    m_dock_width = std::max(m_dock_width, ASSISTANT_MIN_WIDTH);

    SettingsManager& settings = SettingsManager::GetInstance();

    RenderSplitter();
    ImGui::SameLine(0.0f, 0.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ASSISTANT_WINDOW_PADDING);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 10.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgMain));
    // Only the transcript scrolls; the header and composer are pinned.
    ImGui::BeginChild("##assistant_dock", ImVec2(m_dock_width, 0.0f),
                      ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    RenderHeaderCard();
    RenderTranscript();
    RenderComposer();

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void
AssistantPanel::RenderHeaderCard()
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();

    BeginPanelCard("##assistant_header", PanelCardTone::kFrame, ASSISTANT_CARD_PADDING,
                   true, &settings);

    const AssistantProvider* provider = settings.GetActiveAssistantProvider();
    const bool configured = provider != nullptr && !provider->endpoint_url.empty();
    // An empty Model field sends the default, so name what is actually sent.
    const std::string model = !configured ? std::string()
                              : provider->model.empty()
                                  ? std::string(ASSISTANT_DEFAULT_MODEL)
                                  : provider->model;

    const float close_width = ImGui::GetFrameHeight();
    ImGui::BeginGroup();
    PanelIcon(ICON_COMPASS, Colors::kAccent, &settings);
    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
    ImGui::BeginGroup();
    ImGui::PushFont(nullptr, settings.GetFontManager().GetFontSize(FontSize::kMedLarge));
    ImGui::TextUnformatted("Ask Optiq");
    ImGui::PopFont();
    if(!configured)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
        ImGui::TextUnformatted("Not configured");
        ImGui::PopStyleColor();
    }
    ImGui::EndGroup();
    ImGui::EndGroup();
    if(ImGui::IsItemHovered())
    {
        if(!configured)
        {
            SetTooltipStyled("Not configured. Edit > Preferences > Assistant.");
        }
        else
        {
            SetTooltipStyled("%s\n%s", model.c_str(), provider->endpoint_url.c_str());
        }
    }

    ImGui::SameLine(0.0f, 0.0f);
    const float leftover = ImGui::GetContentRegionAvail().x - close_width;
    if(leftover > 0.0f)
    {
        ImGui::Dummy(ImVec2(leftover, 0.0f));
        ImGui::SameLine(0.0f, 0.0f);
    }
    if(XButton("##assistant_close", "Close the assistant", &settings))
    {
        m_visible = false;
    }

    // Its own row rather than under the title, so it starts on the card's left
    // edge with the icon and the transcript text below.
    if(configured)
    {
        RenderContextChip(model, provider->endpoint_url);
    }
    else
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextWarning));
        ImGui::TextWrapped("Set the URL, model, and key in Edit > Preferences > Assistant.");
        ImGui::PopStyleColor();
    }

    EndPanelCard();
}

/*
 * The pill under the title: the model, a ring for how full the kept history is,
 * the conversation's size against that budget, and - once the endpoint has
 * reported any - the tokens this conversation has used.
 */
void
AssistantPanel::RenderContextChip(const std::string& model, const std::string& endpoint)
{
    const ContextLoad load = MeasureContext();
    const float       fill = ContextFill(load.history_chars, load.history_budget_chars);

    SettingsManager& settings = SettingsManager::GetInstance();
    const ImU32      main     = settings.GetColor(Colors::kTextMain);
    const ImU32      dim      = settings.GetColor(Colors::kTextDim);

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float  avail  = ImGui::GetContentRegionAvail().x;

    ImGui::PushFont(nullptr, settings.GetFontManager().GetFontSize(FontSize::kSmall));
    const float text_h   = ImGui::GetTextLineHeight();
    const float chip_h   = text_h + ASSISTANT_CONTEXT_CHIP_PAD_Y * 2.0f;
    const float ring_d   = std::max(8.0f, text_h - 3.0f);
    const float word_gap = ImGui::CalcTextSize(" ").x * 2.0f;

    const std::string used   = FormatTokensShort(load.history_tokens);
    const std::string budget = " / " + FormatTokensShort(load.history_budget_tokens);
    const std::string in_n =
        FormatTokensShort(static_cast<size_t>(m_session_prompt_tokens));
    const std::string out_n =
        FormatTokensShort(static_cast<size_t>(m_session_completion_tokens));
    const char* in_label  = " in";
    const char* out_label = " out";

    const float divider_w = ASSISTANT_CONTEXT_DIVIDER_GAP * 2.0f + 1.0f;
    const float context_w =
        ring_d + ASSISTANT_CONTEXT_CHIP_GAP + ImGui::CalcTextSize(used.c_str()).x;
    const float budget_w = ImGui::CalcTextSize(budget.c_str()).x;
    const float usage_w  = divider_w + ImGui::CalcTextSize(in_n.c_str()).x +
                          ImGui::CalcTextSize(in_label).x + word_gap +
                          ImGui::CalcTextSize(out_n.c_str()).x +
                          ImGui::CalcTextSize(out_label).x;
    const float pads = ASSISTANT_CONTEXT_CHIP_PAD_X * 2.0f;

    // A narrow dock drops the usage first, then the budget, then shortens the
    // model name. The ring and the conversation size always stay.
    std::string label       = model;
    bool        show_usage  = m_usage_reported;
    bool        show_budget = true;
    auto        total       = [&]() {
        return pads + ImGui::CalcTextSize(label.c_str()).x + divider_w + context_w +
               (show_budget ? budget_w : 0.0f) + (show_usage ? usage_w : 0.0f);
    };
    if(show_usage && total() > avail)
    {
        show_usage = false;
    }
    if(total() > avail)
    {
        show_budget = false;
    }
    if(total() > avail)
    {
        label = ElideWithEllipsis(model,
                                  std::max(0.0f, avail - (pads + divider_w + context_w)));
    }
    const float width = total();

    ImGui::InvisibleButton("##assistant_context_chip",
                           ImVec2(width, chip_h + ASSISTANT_CONTEXT_CHIP_BELOW));
    const bool hovered = ImGui::IsItemHovered();

    ImDrawList*  draw     = ImGui::GetWindowDrawList();
    const float  center_y = origin.y + chip_h * 0.5f;
    const ImVec2 chip_min(origin);
    const ImVec2 chip_max(origin.x + width, origin.y + chip_h);
    // Hover changes only the outline: the ring's track and the dividers share
    // the fill's tone, so a tinted fill would hide them.
    draw->AddRectFilled(chip_min, chip_max, settings.GetColor(Colors::kBgPanel),
                        chip_h * 0.5f);
    draw->AddRect(
        chip_min, chip_max,
        settings.GetColor(hovered ? Colors::kBorderColor : Colors::kPanelBorderSubtle),
        chip_h * 0.5f);

    const float text_y  = center_y - text_h * 0.5f;
    const ImU32 divider = settings.GetColor(Colors::kBorderColor);
    float       x       = origin.x + ASSISTANT_CONTEXT_CHIP_PAD_X;
    auto        run     = [&](const char* text, ImU32 color) {
        draw->AddText(ImVec2(x, text_y), color, text);
        x += ImGui::CalcTextSize(text).x;
    };
    auto rule = [&]() {
        x += ASSISTANT_CONTEXT_DIVIDER_GAP;
        draw->AddLine(ImVec2(x, center_y - text_h * 0.35f),
                      ImVec2(x, center_y + text_h * 0.35f), divider);
        x += 1.0f + ASSISTANT_CONTEXT_DIVIDER_GAP;
    };

    run(label.c_str(), main);
    rule();

    const ImVec2 ring_center(x + ring_d * 0.5f, center_y);
    const float  ring_r = ring_d * 0.5f - ASSISTANT_CONTEXT_RING_STROKE * 0.5f;
    draw->AddCircle(ring_center, ring_r, settings.GetColor(Colors::kBorderColor), 0,
                    ASSISTANT_CONTEXT_RING_STROKE);
    if(fill >= 0.995f)
    {
        draw->AddCircle(ring_center, ring_r, ContextFillColor(fill), 0,
                        ASSISTANT_CONTEXT_RING_STROKE);
    }
    else if(load.history_chars > 0)
    {
        // Zero radians points right, so start at the top and sweep clockwise.
        const float start = -ASSISTANT_PI * 0.5f;
        const float sweep =
            std::max(fill, ASSISTANT_CONTEXT_MIN_ARC) * ASSISTANT_PI * 2.0f;
        draw->PathArcTo(ring_center, ring_r, start, start + sweep, 0);
        draw->PathStroke(ContextFillColor(fill), ASSISTANT_CONTEXT_RING_STROKE);
    }
    x += ring_d + ASSISTANT_CONTEXT_CHIP_GAP;

    run(used.c_str(), load.history_chars > 0 ? main : dim);
    if(show_budget)
    {
        run(budget.c_str(), dim);
    }
    if(show_usage)
    {
        rule();
        run(in_n.c_str(), main);
        run(in_label, dim);
        x += word_gap;
        run(out_n.c_str(), main);
        run(out_label, dim);
    }
    ImGui::PopFont();

    if(hovered)
    {
        BeginTooltipStyled();
        RenderContextTooltip(load, fill, model, endpoint);
        EndTooltipStyled();
    }
}

// The pill's hover card: which endpoint answers, the history against its
// budget, what rides along on every request, and the reported token counts.
void
AssistantPanel::RenderContextTooltip(const ContextLoad& load, float fill,
                                     const std::string& model,
                                     const std::string& endpoint) const
{
    SettingsManager& settings = SettingsManager::GetInstance();
    const ImU32      main     = settings.GetColor(Colors::kTextMain);
    const ImU32      dim      = settings.GetColor(Colors::kTextDim);
    const float      width    = ASSISTANT_CONTEXT_TIP_WIDTH;
    // A tooltip sizes itself to its content, so alignment is measured against
    // this fixed width rather than against the space the window reports.
    const float start_x = ImGui::GetCursorPosX();

    ImGui::PushTextWrapPos(start_x + width);

    ImGui::TextUnformatted(model.c_str());
    ImGui::PushStyleColor(ImGuiCol_Text, dim);
    ImGui::TextWrapped("%s", endpoint.c_str());
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const std::string percent = FormatFillPercent(fill) + " used";
    ImGui::TextUnformatted("Context");
    ImGui::SameLine(start_x + width - ImGui::CalcTextSize(percent.c_str()).x);
    ImGui::PushStyleColor(ImGuiCol_Text, dim);
    ImGui::TextUnformatted(percent.c_str());
    ImGui::PopStyleColor();

    ImDrawList*  draw   = ImGui::GetWindowDrawList();
    const ImVec2 bar    = ImGui::GetCursorScreenPos();
    const float  radius = ASSISTANT_CONTEXT_TIP_BAR_H * 0.5f;
    draw->AddRectFilled(bar, ImVec2(bar.x + width, bar.y + ASSISTANT_CONTEXT_TIP_BAR_H),
                        settings.GetColor(Colors::kBorderColor), radius);
    if(load.history_chars > 0)
    {
        const float filled =
            std::min(width, std::max(ASSISTANT_CONTEXT_TIP_BAR_H, width * fill));
        draw->AddRectFilled(bar,
                            ImVec2(bar.x + filled, bar.y + ASSISTANT_CONTEXT_TIP_BAR_H),
                            ContextFillColor(fill), radius);
    }
    ImGui::Dummy(ImVec2(width, ASSISTANT_CONTEXT_TIP_BAR_H));

    const std::string about =
        FormatCount(load.history_tokens) + " of " +
        FormatCount(load.history_budget_tokens) +
        " tokens of conversation. Past that, Optiq shortens older tool results. The "
        "prompt and tool list add about " +
        FormatCount(load.overhead_tokens) + " to every request.";
    ImGui::PushStyleColor(ImGuiCol_Text, dim);
    ImGui::TextWrapped("%s", about.c_str());
    ImGui::PopStyleColor();

    // Stays in the context block. The usage table below is what the endpoint
    // reported, and the disclaimer does not cover it.
    ImGui::Spacing();
    ImGui::PushFont(nullptr, settings.GetFontManager().GetFontSize(FontSize::kSmall));
    ImGui::PushStyleColor(ImGuiCol_Text, dim);
    ImGui::TextWrapped("Context figures are estimates. Actual values may vary.");
    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextUnformatted("Token usage");
    if(!m_usage_reported)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, dim);
        ImGui::TextWrapped("%s", m_conversation.empty()
                                     ? "Counts appear after the first reply."
                                     : "This endpoint did not report token counts.");
        ImGui::PopStyleColor();
        ImGui::PopTextWrapPos();
        return;
    }

    if(ImGui::BeginTable("##assistant_usage", 3, ImGuiTableFlags_None,
                         ImVec2(width, 0.0f)))
    {
        ImGui::TableSetupColumn("##what", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##in", ImGuiTableColumnFlags_WidthFixed,
                                ASSISTANT_CONTEXT_TIP_NUM_WIDTH);
        ImGui::TableSetupColumn("##out", ImGuiTableColumnFlags_WidthFixed,
                                ASSISTANT_CONTEXT_TIP_NUM_WIDTH);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(1);
        RightAlignedText("In", dim);
        ImGui::TableSetColumnIndex(2);
        RightAlignedText("Out", dim);

        auto row = [&](const char* label, int in_tokens, int out_tokens) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, dim);
            ImGui::TextUnformatted(label);
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(1);
            RightAlignedText(FormatCount(static_cast<size_t>(in_tokens)), main);
            ImGui::TableSetColumnIndex(2);
            RightAlignedText(FormatCount(static_cast<size_t>(out_tokens)), main);
        };
        row("Last reply", m_last_prompt_tokens, m_last_completion_tokens);
        row("This conversation", m_session_prompt_tokens, m_session_completion_tokens);
        ImGui::EndTable();
    }

    std::string note = "Over " + std::to_string(m_session_rounds) +
                       (m_session_rounds == 1 ? " request" : " requests") +
                       ". Input counts the whole history again on every request.";
    if(m_last_cached_known && m_last_cached_tokens > 0)
    {
        note += " " + FormatCount(static_cast<size_t>(m_last_cached_tokens)) +
                " of the last reply's input came from the endpoint's cache.";
    }
    ImGui::PushStyleColor(ImGuiCol_Text, dim);
    ImGui::TextWrapped("%s", note.c_str());
    ImGui::PopStyleColor();

    ImGui::PopTextWrapPos();
}

void
AssistantPanel::RenderTranscript()
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();

    // Last frame's measured composer height; the estimate is only for frame one.
    const float composer_height =
        m_composer_height > 0.0f
            ? m_composer_height
            : ImGui::GetFrameHeight() + style.ItemSpacing.y * 2.0f +
                  ASSISTANT_CARD_PADDING.y * 2.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, PANEL_CARD_ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ASSISTANT_CARD_PADDING);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 12.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgPanel));
    ImGui::PushStyleColor(ImGuiCol_Border, settings.GetColor(Colors::kPanelBorderSubtle));
    ImGui::BeginChild("assistant_history", ImVec2(0.0f, -composer_height),
                      ImGuiChildFlags_Borders);

    if(m_lines.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextMain));
        ImGui::TextWrapped("Ask about this trace, or press Explain this view below.");
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
        ImGui::TextWrapped(
            "I read the timeline overview first, then dig into whatever looks worst.");
        ImGui::PopStyleColor();
    }

    for(size_t i = 0; i < m_lines.size(); ++i)
    {
        RenderMessageCard(i, m_lines[i]);
    }

    // Drawn from live state rather than stored, so it cannot outlive its turn.
    if(Busy() && !m_status.empty())
    {
        RenderLoadingIndicatorDots(ASSISTANT_DOT_RADIUS, ASSISTANT_DOT_COUNT,
                                   ASSISTANT_DOT_SPACING,
                                   settings.GetColor(Colors::kAccent),
                                   ASSISTANT_DOT_SPEED);
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x * 2.0f);
        PanelFieldLabel(m_status.c_str(), false, &settings);
    }

    if(m_scroll_to_bottom)
    {
        ImGui::SetScrollHereY(1.0f);
        m_scroll_to_bottom = false;
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

void
AssistantPanel::RenderMessageCard(size_t index, const ChatLine& line)
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();

    ImGui::PushID(static_cast<int>(index));

    if(line.speaker == Speaker::kStatus)
    {
        // A notice that outlived its turn, such as a failed request.
        PanelIcon(ICON_X_CIRCLED, Colors::kTextError, &settings);
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        RenderSelectableMessage(index, line.text, Colors::kTextError);
        ImGui::PopID();
        return;
    }

    if(line.speaker == Speaker::kChart)
    {
        BeginPanelCard("##chart_card", PanelCardTone::kPanel, ASSISTANT_CARD_PADDING,
                       true, &settings);
        PanelIcon(ICON_CHART_BAR, Colors::kAccent, &settings);
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        PanelFieldLabel("Timeline overview", false, &settings);
        RenderActivityChart(line.track_id);
        EndPanelCard();
        ImGui::PopID();
        return;
    }

    const bool user = line.speaker == Speaker::kUser;
    BeginPanelCard("##message_card",
                   user ? PanelCardTone::kFrame : PanelCardTone::kPanel,
                   ASSISTANT_CARD_PADDING, true, &settings);
    if(user)
    {
        PanelFieldLabel("You", false, &settings);
    }
    else
    {
        PanelIcon(ICON_COMPASS, Colors::kAccent, &settings);
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        PanelFieldLabel("Optiq", false, &settings);
    }
    ImGui::Spacing();
    RenderSelectableMessage(index, line.text, Colors::kTextMain);
    EndPanelCard();
    ImGui::PopID();
}

TextSelect*
AssistantPanel::MessageSelect(size_t index)
{
    if(index >= m_lines.size())
    {
        return nullptr;
    }
    if(m_message_select.size() < m_lines.size())
    {
        m_message_select.resize(m_lines.size());
    }
    if(m_message_lines.size() < m_lines.size())
    {
        m_message_lines.resize(m_lines.size());
    }
    if(m_message_select[index] == nullptr)
    {
        m_message_select[index] = std::make_unique<TextSelect>(
            [this, index](size_t line_index) {
                if(index >= m_message_lines.size() ||
                   line_index >= m_message_lines[index].size())
                {
                    return std::string_view();
                }
                return std::string_view(m_message_lines[index][line_index]);
            },
            [this, index]() {
                return index < m_message_lines.size() ? m_message_lines[index].size()
                                                      : size_t{0};
            },
            true);
    }
    return m_message_select[index].get();
}

void
AssistantPanel::RenderSelectableMessage(size_t index, const std::string& text,
                                        Colors text_color)
{
    const ImU32 color  = SettingsManager::GetInstance().GetColor(text_color);
    TextSelect* select = MessageSelect(index);
    if(select == nullptr)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextWrapped("%s", text.c_str());
        ImGui::PopStyleColor();
        return;
    }
    if(m_message_lines[index].empty())
    {
        m_message_lines[index] = SplitMessageLines(text);
    }

    // TextSelect expects a text-only window whose first glyph is at its origin.
    // Splitting on explicit newlines preserves blank paragraphs while its word
    // wrapping handles each visual line.
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const ImVec2 message_spacing = ImGui::GetStyle().ItemSpacing;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(message_spacing.x, 0.0f));
    ImGui::BeginChild("##message_text", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AutoResizeY,
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                          ImGuiWindowFlags_NoScrollWithMouse);
    // Scoped to the text so the copy menu below keeps the theme's text color.
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    for(const std::string& line : m_message_lines[index])
    {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(line.data(), line.data() + line.size());
        ImGui::PopTextWrapPos();
    }
    ImGui::PopStyleColor();

    // Only one message keeps a highlight, so Ctrl+C copies what is on screen.
    // Ctrl+C reaches only the focused window, and right-click does not focus
    // on its own. The popup flag keeps this working when the click lands while
    // another message's menu is still open.
    if(ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) &&
       (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
        ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
    {
        ImGui::SetWindowFocus();
        for(const std::unique_ptr<TextSelect>& other : m_message_select)
        {
            if(other != nullptr && other.get() != select)
            {
                other->clearSelection();
            }
        }
    }
    select->update();
    // update() copies on Ctrl+C but does not report it. The same chord in the
    // same window fires on the same press.
    bool copied_selection =
        ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_C) && select->hasSelection();

    PushContextMenuStyles();
    if(ImGui::BeginPopupContextWindow("##message_copy"))
    {
        if(IconMenuItem(ICON_COPY, "Copy", select->hasSelection()))
        {
            select->copy();
            copied_selection = true;
        }
        if(IconMenuItem(ICON_LIST, "Select all"))
        {
            select->selectAll();
        }
        ImGui::Separator();
        if(IconMenuItem(ICON_DOCUMENT, "Copy message"))
        {
            ImGui::SetClipboardText(text.c_str());
            NotificationManager::GetInstance().Show("Message copied to clipboard",
                                                    NotificationLevel::Info);
        }
        ImGui::EndPopup();
    }
    PopContextMenuStyles();
    if(copied_selection)
    {
        NotificationManager::GetInstance().Show("Selection copied to clipboard",
                                                NotificationLevel::Info);
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

// Stacked follow-ups under the transcript: Explain this view on an empty chat,
// or the model's offered next steps after a turn.
void
AssistantPanel::RenderSuggestedActions()
{
    SettingsManager& settings = SettingsManager::GetInstance();
    const AssistantProvider* provider = settings.GetActiveAssistantProvider();
    const bool configured =
        provider != nullptr && !provider->endpoint_url.empty();

    const bool show_explain = m_lines.empty() && m_next_steps.empty() && !Busy();
    if(!show_explain && m_next_steps.empty())
    {
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
    ImGui::BeginDisabled(Busy() || !configured);

    if(show_explain)
    {
        if(AccentButton("Explain this view", ImVec2(-FLT_MIN, 0.0f), &settings))
        {
            SendCurrentInput(true);
        }
        ImGui::EndDisabled();
        ImGui::PopStyleVar(2);
        if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            SetTooltipStyled(
                "Runs a self-directed investigation: timeline overview, summary, top "
                "events, then drills into the worst offenders and moves the timeline "
                "to what it found.");
        }
        ImGui::Spacing();
        return;
    }

    const float wrap_width = std::max(
        1.0f, ImGui::GetContentRegionAvail().x - ImGui::GetStyle().FramePadding.x * 2.0f);
    int clicked = -1;
    for(size_t i = 0; i < m_next_steps.size(); ++i)
    {
        ImGui::PushID(static_cast<int>(i));
        const std::string label =
            WrapButtonLabel(std::to_string(i + 1) + ". " + m_next_steps[i], wrap_width);
        bool pressed = false;
        if(i == 0)
        {
            pressed = AccentButton(label.c_str(), ImVec2(-FLT_MIN, 0.0f), &settings);
        }
        else
        {
            pressed = ColoredButton(label.c_str(), settings.GetColor(Colors::kButton),
                                    settings.GetColor(Colors::kButtonHovered),
                                    settings.GetColor(Colors::kButtonActive),
                                    settings.GetColor(Colors::kTextMain), nullptr,
                                    ImVec2(-FLT_MIN, 0.0f));
        }
        if(pressed)
        {
            clicked = static_cast<int>(i);
        }
        ImGui::PopID();
    }

    ImGui::EndDisabled();
    ImGui::PopStyleVar(2);

    if(clicked >= 0)
    {
        m_input = m_next_steps[static_cast<size_t>(clicked)];
        SendCurrentInput(false);
    }

    ImGui::Spacing();
}

std::string
AssistantPanel::BuildTranscriptText() const
{
    std::string out = "# Ask Optiq conversation\n\n";
    for(const ChatLine& line : m_lines)
    {
        switch(line.speaker)
        {
            case Speaker::kUser:
                out += "## You\n\n";
                out += line.text;
                out += "\n\n";
                break;
            case Speaker::kAssistant:
                out += "## Optiq\n\n";
                out += line.text;
                out += "\n\n";
                break;
            case Speaker::kStatus:
                out += "> Notice: ";
                out += line.text;
                out += "\n\n";
                break;
            case Speaker::kChart:
                // Charts redraw from live model data, so note one was shown
                // rather than freezing a snapshot.
                if(line.track_id == INVALID_UINT64_INDEX)
                {
                    out += "_[Timeline overview chart]_\n\n";
                }
                else
                {
                    out += "_[Activity chart for track " +
                           std::to_string(line.track_id) + "]_\n\n";
                }
                break;
        }
    }
    return out;
}

void
AssistantPanel::CopyTranscript()
{
    if(m_lines.empty())
    {
        return;
    }
    ImGui::SetClipboardText(BuildTranscriptText().c_str());
    NotificationManager::GetInstance().Show("Conversation copied to clipboard",
                                            NotificationLevel::Success);
}

void
AssistantPanel::ExportTranscript()
{
    if(m_lines.empty())
    {
        return;
    }
    AppWindow* app = AppWindow::GetInstance();
    if(app == nullptr)
    {
        return;
    }

    FileFilter md_filter;
    md_filter.m_name       = "Markdown";
    md_filter.m_extensions = { "md" };
    FileFilter txt_filter;
    txt_filter.m_name       = "Text";
    txt_filter.m_extensions = { "txt" };

    // Captured by value: the save dialog is async and the conversation may
    // change before the user picks a path.
    const std::string text = BuildTranscriptText();
    app->ShowSaveFileDialog(
        "Export Conversation", { md_filter, txt_filter }, "ask-optiq-conversation.md",
        [text](std::string path) {
            if(path.empty())
            {
                return;
            }
            std::ofstream file(path, std::ios::binary);
            if(!file.is_open())
            {
                NotificationManager::GetInstance().Show("Could not write " + path,
                                                        NotificationLevel::Error);
                return;
            }
            file.write(text.data(), static_cast<std::streamsize>(text.size()));
            // Close explicitly and check: the write may be buffered, so a full
            // disk or flush failure only surfaces here, not at is_open().
            file.close();
            if(file.fail())
            {
                NotificationManager::GetInstance().Show("Failed to write " + path,
                                                        NotificationLevel::Error);
                return;
            }
            NotificationManager::GetInstance().Show("Conversation exported to " + path,
                                                    NotificationLevel::Success);
        });
}

void
AssistantPanel::RenderComposer()
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();

    // Measured to the end of the card, so RenderTranscript can reserve exactly
    // this much on the next frame.
    const float start_y = ImGui::GetCursorPosY();

    BeginPanelCard("##assistant_composer", PanelCardTone::kFrame, ASSISTANT_CARD_PADDING,
                   true, &settings);

    RenderSuggestedActions();

    ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_None;
    if(Busy())
    {
        input_flags |= ImGuiInputTextFlags_ReadOnly;
    }

    const float gap       = style.ItemInnerSpacing.x;
    const float icon_size = ImGui::GetFrameHeight();
    const float input_width =
        std::max(ASSISTANT_COMPOSER_MIN_INPUT_WIDTH,
                 ImGui::GetContentRegionAvail().x - ASSISTANT_SEND_WIDTH -
                     icon_size * ASSISTANT_COMPOSER_ICON_BUTTONS -
                     gap * ASSISTANT_COMPOSER_GAP_COUNT);

    ImGui::SetNextItemWidth(input_width);
    InputTextStringWithHint("##assistant_input",
                            Busy() ? "Working..." : "Ask about this trace...", m_input,
                            input_flags);
    const bool submitted = ImGui::IsItemFocused() &&
                           ImGui::IsKeyPressed(ImGuiKey_Enter) &&
                           !ImGui::GetIO().KeyShift && !Busy();

    ImGui::SameLine(0.0f, gap);
    ImGui::BeginDisabled(Busy() || m_input.empty());
    const bool send = AccentButton("Send", ImVec2(ASSISTANT_SEND_WIDTH, 0.0f), &settings);
    ImGui::EndDisabled();

    ImGui::SameLine(0.0f, gap);
    ImGui::BeginDisabled(m_lines.empty());
    if(IconButton(ICON_COPY, settings.GetFontManager().GetFont(FontType::kIcon),
                  ImVec2(icon_size, icon_size), "Copy the conversation", false,
                  ImVec2(0.0f, 0.0f), settings.GetColor(Colors::kButton),
                  settings.GetColor(Colors::kButtonHovered),
                  settings.GetColor(Colors::kButtonActive)))
    {
        CopyTranscript();
    }

    ImGui::SameLine(0.0f, gap);
    // ICON_ARROW_IN_BOX reads as "import" (it is the Import-from-Basic glyph), so
    // a document icon is used here to mean "write the conversation out to a file".
    if(IconButton(ICON_DOCUMENT, settings.GetFontManager().GetFont(FontType::kIcon),
                  ImVec2(icon_size, icon_size), "Export the conversation to a file", false,
                  ImVec2(0.0f, 0.0f), settings.GetColor(Colors::kButton),
                  settings.GetColor(Colors::kButtonHovered),
                  settings.GetColor(Colors::kButtonActive)))
    {
        ExportTranscript();
    }
    ImGui::EndDisabled();

    ImGui::SameLine(0.0f, gap);
    ImGui::BeginDisabled(m_lines.empty() && m_input.empty() && !Busy());
    if(IconButton(ICON_TRASH_CAN, settings.GetFontManager().GetFont(FontType::kIcon),
                  ImVec2(icon_size, icon_size), "Clear the conversation", false,
                  ImVec2(0.0f, 0.0f), settings.GetColor(Colors::kButton),
                  settings.GetColor(Colors::kButtonHovered),
                  settings.GetColor(Colors::kButtonActive)))
    {
        ResetTurn();
        m_lines.clear();
        m_message_select.clear();
        m_message_lines.clear();
        m_input.clear();
        m_conversation.clear();
        m_next_steps.clear();
        ClearTokenUsage();
    }
    ImGui::EndDisabled();

    EndPanelCard();

    // EndChild already advanced the cursor past one ItemSpacing, which the
    // transcript's reservation has to include too.
    m_composer_height = ImGui::GetCursorPosY() - start_y +
                        settings.GetDefaultStyle().ItemSpacing.y;

    if(send || submitted)
    {
        SendCurrentInput(false);
    }
}

}  // namespace View
}  // namespace RocProfVis
