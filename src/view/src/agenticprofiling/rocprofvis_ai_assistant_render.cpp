// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// The Ask Optiq panel's drawing half: the dock and its splitter, the
// transcript and its cards, the composer, the activity strip, and the toolbar
// button that opens it. Everything here reads panel state and emits ImGui; the
// turn machinery that fills that state lives in rocprofvis_ai_assistant.cpp,
// and the two only meet through AssistantPanel members.

#include "rocprofvis_ai_assistant.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "imgui.h"

#include "icons/rocprovfis_icon_defines.h"
#include "model/rocprofvis_summary_model.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_render_scheduler.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "widgets/rocprofvis_gui_helpers.h"

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

    const float close_width = ImGui::GetFrameHeight();
    ImGui::BeginGroup();
    PanelIcon(ICON_COMPASS, Colors::kAccent, &settings);
    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
    ImGui::BeginGroup();
    ImGui::PushFont(nullptr, settings.GetFontManager().GetFontSize(FontSize::kMedLarge));
    ImGui::TextUnformatted("Ask Optiq");
    ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
    if(!configured)
    {
        ImGui::TextUnformatted("Not configured");
    }
    else if(!provider->model.empty())
    {
        ImGui::TextUnformatted(provider->model.c_str());
    }
    else
    {
        ImGui::TextUnformatted("Ready");
    }
    ImGui::PopStyleColor();
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
            SetTooltipStyled("%s\n%s",
                             provider->model.empty() ? "(no model set)"
                                                     : provider->model.c_str(),
                             provider->endpoint_url.c_str());
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

    if(!configured)
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextWarning));
        ImGui::TextWrapped("Set the URL, model, and key in Edit > Preferences > Assistant.");
        ImGui::PopStyleColor();
    }

    EndPanelCard();
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
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextError));
        ImGui::TextWrapped("%s", line.text.c_str());
        ImGui::PopStyleColor();
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
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextWrapped("%s", line.text.c_str());
    ImGui::PopTextWrapPos();
    EndPanelCard();
    ImGui::PopID();
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
        std::max(80.0f, ImGui::GetContentRegionAvail().x - ASSISTANT_SEND_WIDTH -
                            icon_size - gap * 2.0f);

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
    ImGui::BeginDisabled(m_lines.empty() && m_input.empty() && !Busy());
    if(IconButton(ICON_TRASH_CAN, settings.GetFontManager().GetFont(FontType::kIcon),
                  ImVec2(icon_size, icon_size), "Clear the conversation", false,
                  ImVec2(0.0f, 0.0f), settings.GetColor(Colors::kButton),
                  settings.GetColor(Colors::kButtonHovered),
                  settings.GetColor(Colors::kButtonActive)))
    {
        ResetTurn();
        m_lines.clear();
        m_input.clear();
        m_conversation.clear();
        m_next_steps.clear();
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
