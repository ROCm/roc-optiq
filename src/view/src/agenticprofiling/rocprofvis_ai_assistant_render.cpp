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
#include "rocprofvis_settings_panel.h"
#include "rocprofvis_timeline_selection.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_notification_manager.h"
#include "widgets/rocprofvis_tab_container.h"

namespace RocProfVis
{
namespace View
{

namespace
{

constexpr float  ASSISTANT_MIN_WIDTH      = 440.0f;
constexpr float  ASSISTANT_MAX_WIDTH      = 900.0f;
constexpr float  ASSISTANT_SPLITTER_WIDTH = 6.0f;
// The panel never takes more than this share of the window, so a narrow window
// still leaves the main view usable.
constexpr float  ASSISTANT_MAX_WINDOW_SHARE = 0.5f;
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
// The arrow after the model name when the endpoint lists others, as a fraction
// of the pill's text height, and the rows its list shows before scrolling.
constexpr float       ASSISTANT_MODEL_ARROW_SCALE     = 0.5f;
constexpr float       ASSISTANT_MODEL_ARROW_GAP       = 5.0f;
constexpr int         ASSISTANT_MODEL_MENU_ROWS       = 12;
constexpr float       ASSISTANT_MODEL_MENU_MIN_WIDTH  = 240.0f;
constexpr float       ASSISTANT_MODEL_MENU_MAX_SCREEN = 0.75f;
constexpr const char* ASSISTANT_MODEL_POPUP           = "##assistant_models";
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

constexpr const char* ASSISTANT_LINK_POPUP = "##assistant_links";

// Header buttons are compact so the title row is barely taller than the title,
// and the lines under it sit close enough to read as its subtitle. An elided
// header line shows in full in a tooltip this many glyphs wide.
constexpr ImVec2 ASSISTANT_HEADER_BUTTON_PADDING = ImVec2(8.0f, 3.0f);
constexpr float  ASSISTANT_HEADER_LINE_GAP       = 4.0f;
constexpr float  ASSISTANT_HEADER_TOOLTIP_GLYPHS = 32.0f;

// A dim header line starting at x, elided to the card's edge with its full
// text in a tooltip. ElidedText draws into a child window with a fixed name,
// so each line needs its own id.
void
HeaderContextLine(const char* id, const std::string& text, float x)
{
    ImGui::SetCursorPosX(x);
    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_Text,
                          SettingsManager::GetInstance().GetColor(Colors::kTextDim));
    ElidedText(text.c_str(), std::max(1.0f, ImGui::GetContentRegionAvail().x),
               ImGui::GetFontSize() * ASSISTANT_HEADER_TOOLTIP_GLYPHS);
    ImGui::PopStyleColor();
    ImGui::PopID();
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

// A trace the link menu can offer: open, loaded, and read as one trace.
bool
CanLinkTrace(const AssistantToolContext& context)
{
    return context.data_provider != nullptr && !context.is_compute &&
           !context.is_compare &&
           context.data_provider->GetState() == ProviderState::kReady;
}

bool
CanAskTrace(const AssistantToolContext& context)
{
#ifdef ROCPROFVIS_ENABLE_AGENTIC_COMPUTE
    return context.data_provider != nullptr && !context.is_compare;
#else
    return context.data_provider != nullptr && !context.is_compute && !context.is_compare;
#endif
}

TraceLinkMember
LinkMemberFor(const AssistantToolContext& context)
{
    TraceLinkMember member;
    member.path        = context.project_id;
    member.label       = context.trace_name;
    member.fingerprint = AssistantTraceFingerprint(context);
    return member;
}

void
LinkTraces(const AssistantToolContext& baseline, const AssistantToolContext& target)
{
    NotificationManager& notifications = NotificationManager::GetInstance();
    switch(AssistantTraceLinks().Link(LinkMemberFor(baseline), LinkMemberFor(target)))
    {
        case TraceLinkResult::kLinked:
        {
            notifications.Show("Linked " + target.trace_name + " to its baseline " +
                                   baseline.trace_name,
                               NotificationLevel::Success);
            break;
        }
        case TraceLinkResult::kLinkedNotSaved:
        {
            notifications.Show(
                "Linked for this session only: the link could not be saved",
                NotificationLevel::Warning);
            break;
        }
        case TraceLinkResult::kAlreadyLinked:
        {
            notifications.Show("Those traces are already linked",
                               NotificationLevel::Info);
            break;
        }
        case TraceLinkResult::kInvalid:
        {
            notifications.Show("A trace cannot be linked to itself",
                               NotificationLevel::Warning);
            break;
        }
    }
}

// Where the model name in the header leads, set up or not.
void
OpenAssistantSettings()
{
    AppWindow* app = AppWindow::GetInstance();
    if(app != nullptr)
    {
        app->ShowSettings(SettingsPanel::Assistant);
    }
}

}  // namespace

void
AssistantPanel::RenderActivityChart(const std::string& project_id, uint64_t track_id)
{
    // The card's frame and title are already drawn, so an empty chart says why
    // rather than leaving a blank box.
    const AssistantToolContext context = MakeAssistantToolContext(project_id);
    if(context.data_provider == nullptr)
    {
        // Closing a trace drops its own conversation, so a chart that has lost
        // its trace was read from a linked one.
        ImGui::TextDisabled("This chart's trace is no longer open.");
        return;
    }

    // A linked trace that was closed and opened again has no minimap until it
    // has loaded, which is not the same as having no activity.
    if(context.data_provider->GetState() == ProviderState::kLoading)
    {
        ImGui::TextDisabled("This chart's trace is still loading.");
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
        // Empty means the trace has no minimap, or this track has no row in it.
        // Only the overview chart is the whole trace.
        ImGui::TextDisabled(track_id == INVALID_UINT64_INDEX
                                ? "No activity to show for this trace."
                                : "No activity to show for this track.");
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
        if(GetInstance()->m_visible)
        {
            SetTooltipStyled("Close the assistant.");
        }
        else
        {
            SetTooltipStyled("Open the assistant. Configure the URL and key in "
                             "Edit > Preferences > Assistant.");
        }
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
    return m_visible ? DockWidth() + ASSISTANT_SPLITTER_WIDTH : 0.0f;
}

// m_dock_width keeps the width the user dragged to, so it comes back once the
// window is wide enough again.
float
AssistantPanel::DockWidth() const
{
    return std::min(m_dock_width,
                    ImGui::GetMainViewport()->WorkSize.x * ASSISTANT_MAX_WINDOW_SHARE);
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
        // Dragging left widens the panel, so the delta is negated. It starts from
        // the width on screen, so a panel the window has squeezed answers at once.
        m_dock_width = std::clamp(DockWidth() - ImGui::GetIO().MouseDelta.x,
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
        // Reopening asks for the model list again, in case the endpoint could
        // not be reached the last time.
        m_models_requested = false;
        return;
    }

    // The tab bar has drawn by now, so this is the trace the user is looking at
    // this frame rather than the one Update() saw.
    m_view_project_id = CurrentProjectId();

    // A width persisted below the current minimum would otherwise stick until
    // the splitter was dragged.
    m_dock_width = std::max(m_dock_width, ASSISTANT_MIN_WIDTH);

    SettingsManager& settings = SettingsManager::GetInstance();

    RenderSplitter();
    ImGui::SameLine(0.0f, 0.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ASSISTANT_WINDOW_PADDING);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 10.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgMain));
    // Only the transcript scrolls; the header and composer are pinned. ImGui
    // drops WindowPadding on a borderless child unless asked to keep it.
    ImGui::BeginChild("##assistant_dock", ImVec2(DockWidth(), 0.0f),
                      ImGuiChildFlags_AlwaysUseWindowPadding,
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
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(style.ItemSpacing.x, ASSISTANT_HEADER_LINE_GAP));

    const AssistantProvider* provider = settings.GetActiveAssistantProvider();
    const bool configured = provider != nullptr && !provider->endpoint_url.empty();
    // An empty Model field sends the default, so name what is actually sent.
    const std::string model = !configured ? std::string()
                              : provider->model.empty()
                                  ? std::string(ASSISTANT_DEFAULT_MODEL)
                                  : provider->model;

    // With the key the chat request would use, so the list is what it can reach.
    if(configured && !m_models_requested)
    {
        AssistantProvider endpoint = *provider;
        ApplyAssistantEndpointDefaults(endpoint);
        std::string token;
        settings.GetAssistantToken(endpoint.name, token);
        m_model_catalog.Request(endpoint.endpoint_url, token);
        m_models_requested = true;
    }

    const AssistantToolContext view     = MakeAssistantToolContext(m_view_project_id);
    const bool                 can_link = CanLinkTrace(view);

    // Top row: identity on the left, link and close at the right edge.
    // Aligning to the frame padding centers the icon and title on the buttons.
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ASSISTANT_HEADER_BUTTON_PADDING);
    ImGui::AlignTextToFramePadding();
    PanelIcon(ICON_SPARKLE, Colors::kAccent, &settings);
    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
    const float title_x = ImGui::GetCursorPosX();
    ImGui::PushFont(nullptr, settings.GetFontManager().GetFontSize(FontSize::kMedLarge));
    ImGui::TextUnformatted("Ask Optiq");
    ImGui::PopFont();
    const bool title_hovered = ImGui::IsItemHovered();

    const float link_width =
        ImGui::CalcTextSize("Link runs").x + 2.0f * ASSISTANT_HEADER_BUTTON_PADDING.x;
    ImGui::PushFont(settings.GetFontManager().GetFont(FontType::kIcon), 0.0f);
    const float close_width = ImGui::CalcTextSize(ICON_X_CIRCLED).x;
    ImGui::PopFont();
    ImGui::SameLine(0.0f, 0.0f);
    const float leftover = ImGui::GetContentRegionAvail().x - link_width -
                           style.ItemInnerSpacing.x - close_width;
    if(leftover > 0.0f)
    {
        ImGui::Dummy(ImVec2(leftover, 0.0f));
        ImGui::SameLine(0.0f, 0.0f);
    }
    ImGui::BeginDisabled(!can_link);
    if(ImGui::Button("Link runs", ImVec2(link_width, 0.0f)))
    {
        ImGui::OpenPopup(ASSISTANT_LINK_POPUP);
    }
    ImGui::EndDisabled();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        SetTooltipStyled(can_link ? "Link this trace to an earlier or later run"
                                  : "Open a loaded system trace to link runs");
    }
    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
    if(XButton("##assistant_close", "Close the assistant", &settings))
    {
        m_visible = false;
    }
    ImGui::PopStyleVar();
    RenderLinkMenu();

    if(title_hovered)
    {
        if(!configured)
        {
            SetTooltipStyled("Not configured. Click the Not configured pill, or open "
                             "Edit > Preferences > Assistant.");
        }
        else
        {
            SetTooltipStyled("%s\n%s", model.c_str(), provider->endpoint_url.c_str());
        }
    }

    // The model and context use live in the pill; trace identity and links stay
    // on their own lines because both are per-tab.
    if(configured)
    {
        RenderContextChip(model, provider->endpoint_url);
    }
    else
    {
        RenderSetupChip();
    }
    if(!view.trace_name.empty())
    {
        HeaderContextLine("trace", view.trace_name, title_x);
    }

    const std::vector<TraceLink> links = AssistantTraceLinks().LinksOf(m_view_project_id);
    if(!links.empty())
    {
        std::string linked = "Linked: ";
        for(size_t i = 0; i < links.size(); ++i)
        {
            linked += (i == 0 ? "" : ", ") + links[i].other.label + " (" +
                      links[i].other.role + ")";
        }
        HeaderContextLine("linked", linked, title_x);
    }
    ImGui::PopStyleVar();

    if(!configured)
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextWarning));
        ImGui::TextWrapped("Set the URL, model, and key from the Not configured pill, "
                           "or in Edit > Preferences > Assistant.");
        ImGui::PopStyleColor();
    }

    EndPanelCard();
}

// Links the trace on screen to another open one as baseline and target, or
// removes a link it already has.
void
AssistantPanel::RenderLinkMenu()
{
    PushContextMenuStyles();
    if(ImGui::BeginPopup(ASSISTANT_LINK_POPUP))
    {
        const AssistantToolContext view = MakeAssistantToolContext(m_view_project_id);
        if(!CanLinkTrace(view))
        {
            ImGui::TextDisabled("Open a system trace to link it to another run.");
        }
        else
        {
            TraceLinkStore&              store = AssistantTraceLinks();
            const std::vector<TraceLink> links = store.LinksOf(m_view_project_id);
            if(!links.empty())
            {
                ImGui::TextDisabled("Linked runs");
                for(const TraceLink& link : links)
                {
                    const std::string label =
                        "Unlink " + link.other.label + " (" + link.other.role + ")";
                    ImGui::PushID(link.group_id.c_str());
                    if(ImGui::MenuItem(label.c_str()))
                    {
                        const TraceUnlinkResult result = store.Unlink(link.group_id);
                        if(result == TraceUnlinkResult::kUnlinkedNotSaved)
                        {
                            NotificationManager::GetInstance().Show(
                                "Unlinked for this session only: the change could "
                                "not be saved",
                                NotificationLevel::Warning);
                        }
                    }
                    ImGui::PopID();
                }
                ImGui::Separator();
            }

            ImGui::TextDisabled("Link another open trace");
            bool       offered = false;
            AppWindow* app     = AppWindow::GetInstance();
            if(app != nullptr && app->GetTabContainer())
            {
                for(const TabItem* tab : app->GetTabContainer()->GetTabs())
                {
                    if(tab == nullptr || tab->m_id == m_view_project_id)
                    {
                        continue;
                    }
                    const AssistantToolContext other =
                        MakeAssistantToolContext(tab->m_id);
                    if(!CanLinkTrace(other))
                    {
                        continue;
                    }
                    offered = true;
                    ImGui::PushID(tab->m_id.c_str());
                    if(ImGui::BeginMenu(other.trace_name.c_str()))
                    {
                        if(ImGui::MenuItem("It is the earlier run (baseline)"))
                        {
                            LinkTraces(other, view);
                        }
                        if(ImGui::MenuItem("It is the later run (target)"))
                        {
                            LinkTraces(view, other);
                        }
                        ImGui::EndMenu();
                    }
                    ImGui::PopID();
                }
            }
            if(!offered)
            {
                ImGui::TextDisabled("No other system trace is open.");
            }
        }
        ImGui::EndPopup();
    }
    PopContextMenuStyles();
}

// Stands in for the context pill until an endpoint is set, so the place the
// model name will appear is also the way to set one.
void
AssistantPanel::RenderSetupChip()
{
    SettingsManager& settings = SettingsManager::GetInstance();
    const char*      label    = "Not configured";

    ImGui::PushFont(nullptr, settings.GetFontManager().GetFontSize(FontSize::kSmall));
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float chip_h = ImGui::GetTextLineHeight() + ASSISTANT_CONTEXT_CHIP_PAD_Y * 2.0f;
    const float width =
        ImGui::CalcTextSize(label).x + ASSISTANT_CONTEXT_CHIP_PAD_X * 2.0f;

    const bool pressed = ImGui::InvisibleButton(
        "##assistant_setup_chip", ImVec2(width, chip_h + ASSISTANT_CONTEXT_CHIP_BELOW));
    const bool hovered = ImGui::IsItemHovered();

    ImDrawList*  draw = ImGui::GetWindowDrawList();
    const ImVec2 chip_max(origin.x + width, origin.y + chip_h);
    draw->AddRectFilled(origin, chip_max, settings.GetColor(Colors::kBgPanel),
                        chip_h * 0.5f);
    draw->AddRect(
        origin, chip_max,
        settings.GetColor(hovered ? Colors::kBorderColor : Colors::kPanelBorderSubtle),
        chip_h * 0.5f);
    draw->AddText(ImVec2(origin.x + ASSISTANT_CONTEXT_CHIP_PAD_X,
                         origin.y + ASSISTANT_CONTEXT_CHIP_PAD_Y),
                  settings.GetColor(Colors::kTextDim), label);
    ImGui::PopFont();

    if(hovered)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        SetTooltipStyled("Open the Assistant settings to set the URL, model, and key.");
    }
    if(pressed)
    {
        OpenAssistantSettings();
    }
}

/*
 * The pill under the title: the model, a ring for how full the kept history is,
 * the conversation's size against that budget, and - once the endpoint has
 * reported any - the tokens this conversation has used. The model name opens
 * its settings. The arrow beside it, when the endpoint lists models, opens
 * that list.
 */
void
AssistantPanel::RenderContextChip(const std::string& model, const std::string& endpoint)
{
    Conversation&     conversation = ViewConversation();
    const ContextLoad load         = MeasureContext();
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
        FormatTokensShort(static_cast<size_t>(conversation.session_prompt_tokens));
    const std::string out_n =
        FormatTokensShort(static_cast<size_t>(conversation.session_completion_tokens));
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

    // An arrow after the name means the pill opens the endpoint's model list.
    // Dimmed while an answer is on its way, which keeps a turn on one model.
    const bool listed =
        !m_model_catalog.Models().empty() && m_model_catalog.Url() == endpoint;
    const bool  can_pick = listed && !Busy();
    const float arrow_w  = text_h * ASSISTANT_MODEL_ARROW_SCALE;
    const float picker_w = listed ? ASSISTANT_MODEL_ARROW_GAP + arrow_w : 0.0f;

    // A narrow dock drops the usage first, then the budget, then shortens the
    // model name. The ring and the conversation size always stay.
    std::string label       = model;
    bool        show_usage  = conversation.usage_reported;
    bool        show_budget = true;
    auto        total       = [&]() {
        return pads + ImGui::CalcTextSize(label.c_str()).x + picker_w + divider_w +
               context_w + (show_budget ? budget_w : 0.0f) +
               (show_usage ? usage_w : 0.0f);
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
        label = ElideWithEllipsis(
            model, std::max(0.0f, avail - (pads + picker_w + divider_w + context_w)));
    }
    const float width = total();

    const bool pressed = ImGui::InvisibleButton(
        "##assistant_context_chip", ImVec2(width, chip_h + ASSISTANT_CONTEXT_CHIP_BELOW));
    const bool hovered = ImGui::IsItemHovered();
    // The name opens Assistant settings. The arrow, drawn only when this
    // endpoint listed models, opens that list. The rest of the pill reports
    // and has its own hover card. Enter or Space on the focused pill has no
    // pointer over the name, but it is still an activation of the name.
    const float name_right =
        origin.x + ASSISTANT_CONTEXT_CHIP_PAD_X + ImGui::CalcTextSize(label.c_str()).x;
    const float name_hit_right =
        name_right + (listed ? 0.0f : ASSISTANT_CONTEXT_DIVIDER_GAP);
    const float mouse_x    = ImGui::GetIO().MousePos.x;
    const bool  over_name  = hovered && mouse_x < name_hit_right;
    const bool  over_arrow = listed && hovered && mouse_x >= name_right &&
                             mouse_x <= name_right + picker_w;
    if(over_name || (can_pick && over_arrow))
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    const bool mouse_click = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
    if(pressed && can_pick && over_arrow && mouse_click)
    {
        ImGui::OpenPopup(ASSISTANT_MODEL_POPUP);
    }
    else if(pressed && (over_name || !mouse_click))
    {
        OpenAssistantSettings();
    }

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
    if(listed)
    {
        x += ASSISTANT_MODEL_ARROW_GAP;
        const float top    = center_y - arrow_w * 0.25f;
        const float bottom = center_y + arrow_w * 0.25f;
        draw->AddTriangleFilled(ImVec2(x, top), ImVec2(x + arrow_w, top),
                                ImVec2(x + arrow_w * 0.5f, bottom),
                                can_pick ? main : dim);
        x += arrow_w;
    }
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

    if(hovered && !ImGui::IsPopupOpen(ASSISTANT_MODEL_POPUP))
    {
        BeginTooltipStyled();
        RenderContextTooltip(conversation, load, fill, model, endpoint);
        EndTooltipStyled();
    }
    RenderModelMenu(ImVec2(chip_min.x, chip_max.y + ASSISTANT_CONTEXT_CHIP_BELOW), model,
                    endpoint);
}

float
AssistantPanel::ModelNameWidth()
{
    ModelWidthCache& cache     = m_model_width_cache;
    ImFont* const    font      = ImGui::GetFont();
    const float      font_size = ImGui::GetFontSize();
    if(cache.revision != m_model_catalog.Revision() || cache.font != font ||
       cache.font_size != font_size)
    {
        cache.width = 0.0f;
        for(const std::string& name : m_model_catalog.Models())
        {
            cache.width = std::max(cache.width, ImGui::CalcTextSize(name.c_str()).x);
        }
        cache.revision  = m_model_catalog.Revision();
        cache.font      = font;
        cache.font_size = font_size;
    }
    return cache.width;
}

// The model list under the pill, with the model in use checked. A pick is
// saved like any setting, and the next request goes to it.
void
AssistantPanel::RenderModelMenu(const ImVec2& anchor, const std::string& model,
                                const std::string& endpoint)
{
    if(!ImGui::IsPopupOpen(ASSISTANT_MODEL_POPUP))
    {
        return;
    }
    const ImGuiStyle& style = ImGui::GetStyle();
    const float       max_h =
        ImGui::GetTextLineHeightWithSpacing() * ASSISTANT_MODEL_MENU_ROWS +
        style.WindowPadding.y * 2.0f;
    const float text_w = ModelNameWidth();
    const float max_w =
        ImGui::GetMainViewport()->WorkSize.x * ASSISTANT_MODEL_MENU_MAX_SCREEN;
    const float popup_w =
        std::min(max_w, std::max(ASSISTANT_MODEL_MENU_MIN_WIDTH,
                                 text_w + style.WindowPadding.x * 2.0f));
    ImGui::SetNextWindowPos(anchor, ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(popup_w, 0.0f), ImVec2(popup_w, max_h));
    PushContextMenuStyles();
    if(ImGui::BeginPopup(ASSISTANT_MODEL_POPUP, ImGuiWindowFlags_HorizontalScrollbar))
    {
        if(m_model_catalog.Url() != endpoint || m_model_catalog.Models().empty())
        {
            ImGui::CloseCurrentPopup();
        }
        // The names are drawn over the rows, not as their labels, so the row
        // width alone sets how far the menu scrolls: the menu's full width, or
        // the widest name when that is wider.
        const float row_w = std::max(text_w, ImGui::GetContentRegionAvail().x);
        for(size_t i = 0;
            m_model_catalog.Url() == endpoint && i < m_model_catalog.Models().size(); ++i)
        {
            const std::string& name     = m_model_catalog.Models()[i];
            const bool         selected = name == model;
            ImGui::PushID(static_cast<int>(i));
            const ImVec2 text_pos = ImGui::GetCursorScreenPos();
            const bool   picked =
                ImGui::Selectable("##model", selected, ImGuiSelectableFlags_None,
                                  ImVec2(row_w, ImGui::GetTextLineHeight()));
            ImGui::GetWindowDrawList()->AddText(
                text_pos, ImGui::GetColorU32(ImGuiCol_Text), name.c_str());
            if(picked && !selected)
            {
                SettingsManager::GetInstance().SetActiveAssistantModel(name);
            }
            if(selected)
            {
                ImGui::SetItemDefaultFocus();
            }
            ImGui::PopID();
        }
        ImGui::EndPopup();
    }
    PopContextMenuStyles();
}

// The pill's hover card: which endpoint answers, the history against its
// budget, what rides along on every request, and the reported token counts.
void
AssistantPanel::RenderContextTooltip(const Conversation& conversation,
                                     const ContextLoad& load, float fill,
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
    ImGui::TextUnformatted("Click the model name to change these.");
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
    if(!conversation.usage_reported)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, dim);
        ImGui::TextWrapped("%s", conversation.messages.empty()
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
        row("Last reply", conversation.last_prompt_tokens,
            conversation.last_completion_tokens);
        row("This conversation", conversation.session_prompt_tokens,
            conversation.session_completion_tokens);
        ImGui::EndTable();
    }

    std::string note = "Over " + std::to_string(conversation.session_rounds) +
                       (conversation.session_rounds == 1 ? " request" : " requests") +
                       ". Input counts the whole history again on every request.";
    if(conversation.last_cached_known && conversation.last_cached_tokens > 0)
    {
        note += " " + FormatCount(static_cast<size_t>(conversation.last_cached_tokens)) +
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
    SettingsManager&  settings     = SettingsManager::GetInstance();
    const ImGuiStyle& style        = settings.GetDefaultStyle();
    Conversation&     conversation = ViewConversation();

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

    if(conversation.lines.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextMain));
        ImGui::TextWrapped("Ask about this trace, or press Explain this view below.");
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
        ImGui::TextWrapped(
            "I read the timeline overview first, then dig into whatever looks worst.");
        ImGui::PopStyleColor();
    }

    for(size_t i = 0; i < conversation.lines.size(); ++i)
    {
        RenderMessageCard(conversation, i, conversation.lines[i]);
    }

    // Drawn from live state rather than stored, so it cannot outlive its turn,
    // and only under the conversation the turn is writing to.
    if(Busy() && !m_status.empty() && m_turn_project_id == m_view_project_id)
    {
        RenderLoadingIndicatorDots(ASSISTANT_DOT_RADIUS, ASSISTANT_DOT_COUNT,
                                   ASSISTANT_DOT_SPACING,
                                   settings.GetColor(Colors::kAccent),
                                   ASSISTANT_DOT_SPEED);
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x * 2.0f);
        PanelFieldLabel(m_status.c_str(), false, &settings);
    }

    if(conversation.scroll_to_bottom)
    {
        ImGui::SetScrollHereY(1.0f);
        conversation.scroll_to_bottom = false;
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

void
AssistantPanel::RenderMessageCard(Conversation& conversation, size_t index,
                                  const ChatLine& line)
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();

    ImGui::PushID(static_cast<int>(index));

    if(line.speaker == Speaker::kStatus)
    {
        // A notice that outlived its turn, such as a failed request.
        PanelIcon(ICON_X_CIRCLED, Colors::kTextError, &settings);
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        RenderSelectableMessage(conversation, index, line.text, Colors::kTextError);
        ImGui::PopID();
        return;
    }

    if(line.speaker == Speaker::kChart)
    {
        const std::string chart_project =
            line.project_id.empty() ? m_view_project_id : line.project_id;
        std::string title = "Timeline overview";
        if(chart_project != m_view_project_id)
        {
            const AssistantToolContext source = MakeAssistantToolContext(chart_project);
            if(!source.trace_name.empty())
            {
                title += " of " + source.trace_name;
            }
        }
        BeginPanelCard("##chart_card", PanelCardTone::kPanel, ASSISTANT_CARD_PADDING,
                       true, &settings);
        PanelIcon(ICON_CHART_BAR, Colors::kAccent, &settings);
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        PanelFieldLabel(title.c_str(), false, &settings);
        RenderActivityChart(chart_project, line.track_id);
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
        PanelIcon(ICON_SPARKLE, Colors::kAccent, &settings);
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        PanelFieldLabel("Optiq", false, &settings);
    }
    ImGui::Spacing();
    RenderSelectableMessage(conversation, index, line.text, Colors::kTextMain);
    EndPanelCard();
    ImGui::PopID();
}

TextSelect*
AssistantPanel::MessageSelect(Conversation& conversation, size_t index)
{
    if(index >= conversation.lines.size())
    {
        return nullptr;
    }
    if(conversation.message_select.size() < conversation.lines.size())
    {
        conversation.message_select.resize(conversation.lines.size());
    }
    if(conversation.message_lines.size() < conversation.lines.size())
    {
        conversation.message_lines.resize(conversation.lines.size());
    }
    if(conversation.message_select[index] == nullptr)
    {
        // The conversation is heap-held by the map, so its address is stable for
        // as long as these callbacks, which it owns, can run.
        Conversation* owner                = &conversation;
        conversation.message_select[index] = std::make_unique<TextSelect>(
            [owner, index](size_t line_index) {
                if(index >= owner->message_lines.size() ||
                   line_index >= owner->message_lines[index].size())
                {
                    return std::string_view();
                }
                return std::string_view(owner->message_lines[index][line_index]);
            },
            [owner, index]() {
                return index < owner->message_lines.size()
                           ? owner->message_lines[index].size()
                           : size_t{ 0 };
            },
            true);
    }
    return conversation.message_select[index].get();
}

void
AssistantPanel::RenderSelectableMessage(Conversation& conversation, size_t index,
                                        const std::string& text, Colors text_color)
{
    const ImU32 color  = SettingsManager::GetInstance().GetColor(text_color);
    TextSelect* select = MessageSelect(conversation, index);
    if(select == nullptr)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextWrapped("%s", text.c_str());
        ImGui::PopStyleColor();
        return;
    }
    if(conversation.message_lines[index].empty())
    {
        conversation.message_lines[index] = SplitMessageLines(text);
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
    for(const std::string& line : conversation.message_lines[index])
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
        for(const std::unique_ptr<TextSelect>& other : conversation.message_select)
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
    const AssistantToolContext view         = MakeAssistantToolContext(m_view_project_id);
    const bool                 can_ask      = CanAskTrace(view);
    Conversation&              conversation = ViewConversation();

    const bool show_explain =
        conversation.lines.empty() && conversation.next_steps.empty() && !Busy();
    if(!show_explain && conversation.next_steps.empty())
    {
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
    ImGui::BeginDisabled(Busy() || !configured || !can_ask);

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
            if(view.is_compare)
            {
                SetTooltipStyled("Open the two traces on their own and link them; "
                                 "Ask Optiq would add a compare tab's runs together.");
            }
            else if(view.is_compute && !can_ask)
            {
                SetTooltipStyled("Ask Optiq currently reads system traces.");
            }
            else
            {
                SetTooltipStyled(
                    "Runs a self-directed investigation: timeline overview, summary, "
                    "top events, then drills into the worst offenders and moves the "
                    "timeline to what it found.");
            }
        }
        ImGui::Spacing();
        return;
    }

    const float wrap_width = std::max(
        1.0f, ImGui::GetContentRegionAvail().x - ImGui::GetStyle().FramePadding.x * 2.0f);
    int clicked = -1;
    for(size_t i = 0; i < conversation.next_steps.size(); ++i)
    {
        ImGui::PushID(static_cast<int>(i));
        const std::string label = WrapButtonLabel(
            std::to_string(i + 1) + ". " + conversation.next_steps[i], wrap_width);
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
        conversation.input = conversation.next_steps[static_cast<size_t>(clicked)];
        SendCurrentInput(false);
    }

    ImGui::Spacing();
}

std::string
AssistantPanel::BuildTranscriptText(const Conversation& conversation,
                                    const std::string&  trace_name) const
{
    std::string out = "# Ask Optiq conversation\n\n";
    if(!trace_name.empty())
    {
        out += "Trace: " + trace_name + "\n\n";
    }
    for(const ChatLine& line : conversation.lines)
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
    const Conversation& conversation = ViewConversation();
    if(conversation.lines.empty())
    {
        return;
    }
    const std::string trace_name = MakeAssistantToolContext(m_view_project_id).trace_name;
    ImGui::SetClipboardText(BuildTranscriptText(conversation, trace_name).c_str());
    NotificationManager::GetInstance().Show("Conversation copied to clipboard",
                                            NotificationLevel::Success);
}

void
AssistantPanel::ExportTranscript()
{
    const Conversation& conversation = ViewConversation();
    if(conversation.lines.empty())
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
    const std::string text = BuildTranscriptText(
        conversation, MakeAssistantToolContext(m_view_project_id).trace_name);
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

// Clears the conversation on screen. A turn writing to it is stopped; one
// running on another trace carries on.
void
AssistantPanel::ClearConversation()
{
    if(Busy() && m_turn_project_id == m_view_project_id)
    {
        ResetTurn();
    }
    Conversation& conversation = ViewConversation();
    conversation.lines.clear();
    conversation.message_select.clear();
    conversation.message_lines.clear();
    conversation.input.clear();
    conversation.next_steps.clear();
    ResetHistory(conversation);
    ClearTokenUsage(conversation);
}

void
AssistantPanel::RenderComposer()
{
    SettingsManager&           settings     = SettingsManager::GetInstance();
    const ImGuiStyle&          style        = settings.GetDefaultStyle();
    Conversation&              conversation = ViewConversation();
    const AssistantToolContext view         = MakeAssistantToolContext(m_view_project_id);
    const bool                 can_ask      = CanAskTrace(view);

    // Measured to the end of the card, so RenderTranscript can reserve exactly
    // this much on the next frame.
    const float start_y = ImGui::GetCursorPosY();

    BeginPanelCard("##assistant_composer", PanelCardTone::kFrame, ASSISTANT_CARD_PADDING,
                   true, &settings);

    RenderSuggestedActions();

    // One turn runs at a time, so a question on another trace waits for it,
    // and the hint says which trace it is busy with.
    const bool  busy_here      = Busy() && m_turn_project_id == m_view_project_id;
    const bool  busy_elsewhere = Busy() && !busy_here;
    std::string hint           = "Ask about this trace...";
    if(busy_elsewhere)
    {
        const std::string turn_trace = MakeToolContext().trace_name;
        hint                         = "Working on " +
               (turn_trace.empty() ? std::string("another trace") : turn_trace) + "...";
    }
    else if(busy_here)
    {
        hint = "Working...";
    }
    else if(view.is_compare)
    {
        hint = "Link the two traces from their own tabs...";
    }
    else if(view.is_compute && !can_ask)
    {
        hint = "Ask Optiq currently reads system traces...";
    }
    else if(view.data_provider == nullptr)
    {
        hint = "Open a trace...";
    }

    ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_None;
    if(Busy() || !can_ask)
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
    InputTextStringWithHint("##assistant_input", hint.c_str(), conversation.input,
                            input_flags);
    const bool submitted = ImGui::IsItemFocused() &&
                           ImGui::IsKeyPressed(ImGuiKey_Enter) &&
                           !ImGui::GetIO().KeyShift && !Busy() && can_ask;

    ImGui::SameLine(0.0f, gap);
    ImGui::BeginDisabled(Busy() || !can_ask || conversation.input.empty());
    const bool send = AccentButton("Send", ImVec2(ASSISTANT_SEND_WIDTH, 0.0f), &settings);
    ImGui::EndDisabled();

    ImGui::SameLine(0.0f, gap);
    ImGui::BeginDisabled(conversation.lines.empty());
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
    // the export icon is used here to mean "write the conversation out to a file".
    if(IconButton(ICON_EXPORT, settings.GetFontManager().GetFont(FontType::kIcon),
                  ImVec2(icon_size, icon_size), "Export the conversation to a file", false,
                  ImVec2(0.0f, 0.0f), settings.GetColor(Colors::kButton),
                  settings.GetColor(Colors::kButtonHovered),
                  settings.GetColor(Colors::kButtonActive)))
    {
        ExportTranscript();
    }
    ImGui::EndDisabled();

    ImGui::SameLine(0.0f, gap);
    ImGui::BeginDisabled(conversation.lines.empty() && conversation.input.empty() &&
                         !busy_here);
    // Clearing is the panel's only way to stop a turn, so the tooltip says so
    // while one runs here. A turn on another trace is left running.
    if(IconButton(ICON_TRASH_CAN, settings.GetFontManager().GetFont(FontType::kIcon),
                  ImVec2(icon_size, icon_size),
                  busy_here ? "Stop this answer and clear the conversation"
                            : "Clear the conversation",
                  false, ImVec2(0.0f, 0.0f), settings.GetColor(Colors::kButton),
                  settings.GetColor(Colors::kButtonHovered),
                  settings.GetColor(Colors::kButtonActive)))
    {
        ClearConversation();
    }
    ImGui::EndDisabled();

    EndPanelCard();

    // EndChild already advanced the cursor past one ItemSpacing, the same gap
    // the transcript has to leave above the composer.
    m_composer_height = ImGui::GetCursorPosY() - start_y;

    if(send || submitted)
    {
        SendCurrentInput(false);
    }
}

}  // namespace View
}  // namespace RocProfVis
