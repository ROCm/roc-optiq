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
    return context.data_provider != nullptr && !context.is_compute && !context.is_compare;
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

}  // namespace

void
AssistantPanel::RenderActivityChart(const std::string& project_id, uint64_t track_id)
{
    const AssistantToolContext context = MakeAssistantToolContext(project_id);
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
    ImGui::BeginChild("##assistant_dock", ImVec2(m_dock_width, 0.0f),
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

    const AssistantToolContext view     = MakeAssistantToolContext(m_view_project_id);
    const bool                 can_link = CanLinkTrace(view);

    // Top row: identity on the left, link and close at the right edge.
    // Aligning to the frame padding centers the icon and title on the buttons.
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ASSISTANT_HEADER_BUTTON_PADDING);
    ImGui::AlignTextToFramePadding();
    PanelIcon(ICON_COMPASS, Colors::kAccent, &settings);
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

    // Context under the title: model and trace, then any linked runs.
    std::string metadata = !configured               ? std::string("Not configured")
                           : provider->model.empty() ? std::string("Ready")
                                                     : provider->model;
    if(!view.trace_name.empty())
    {
        metadata += " \xC2\xB7 " + view.trace_name;
    }
    HeaderContextLine("metadata", metadata, title_x);

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
        ImGui::TextWrapped("Set the URL, model, and key in Edit > Preferences > Assistant.");
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
        PanelIcon(ICON_COMPASS, Colors::kAccent, &settings);
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
            else if(view.is_compute)
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
    else if(view.is_compute)
    {
        hint = "Ask Optiq currently reads system traces...";
    }
    else if(view.data_provider == nullptr)
    {
        hint = "Open a system trace...";
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
    ImGui::BeginDisabled(conversation.lines.empty() && conversation.input.empty() &&
                         !busy_here);
    if(IconButton(ICON_TRASH_CAN, settings.GetFontManager().GetFont(FontType::kIcon),
                  ImVec2(icon_size, icon_size), "Clear the conversation", false,
                  ImVec2(0.0f, 0.0f), settings.GetColor(Colors::kButton),
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
