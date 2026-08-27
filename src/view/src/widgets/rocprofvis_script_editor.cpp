// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_script_editor.h"

#include <algorithm>
#include <cfloat>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

#include "imgui.h"

#include "icons/rocprovfis_icon_defines.h"
#include "model/rocprofvis_timeline_model.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_events.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_notification_manager.h"
#include "rocprofvis_requests.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "widgets/rocprofvis_gui_helpers.h"

namespace RocProfVis
{
namespace View
{

// Compact Ask Optiq vocabulary: same cards, less chrome, so Source keeps the
// vertical room. Padding is intentionally tighter than the assistant dock.
constexpr ImVec2 SCRIPT_WINDOW_PADDING        = ImVec2(10.0f, 8.0f);
constexpr ImVec2 SCRIPT_CARD_PADDING          = ImVec2(10.0f, 6.0f);
constexpr ImVec2 SCRIPT_ITEM_SPACING          = ImVec2(8.0f, 6.0f);
constexpr ImVec2 SCRIPT_BUTTON_PADDING        = ImVec2(8.0f, 3.0f);
constexpr float  SCRIPT_HEADER_ICON_SCALE     = 0.72f;
constexpr float  SCRIPT_ACTION_ICON_SCALE     = 0.48f;
constexpr float  SCRIPT_EDITOR_INSET_ROUNDING = 5.0f;
constexpr float  SCRIPT_TWO_COLUMN_MIN_WIDTH  = 720.0f;
// Share of the workspace the Result pane starts with. Both layouts use it, so
// dragging the split wide and then narrowing the panel keeps the proportion.
constexpr float SCRIPT_DEFAULT_RESULT_RATIO = 0.34f;
constexpr float SCRIPT_SPLITTER_THICKNESS   = 6.0f;
constexpr float SCRIPT_PANE_MIN_WIDTH       = 180.0f;
constexpr float SCRIPT_PANE_MIN_HEIGHT      = 60.0f;
constexpr float  SCRIPT_DOT_RADIUS            = 2.0f;
constexpr int    SCRIPT_DOT_COUNT             = 3;
constexpr float  SCRIPT_DOT_SPACING           = 3.0f;
constexpr float  SCRIPT_DOT_SPEED             = 5.0f;

// Even-spacing sample from SCRIPTING.md so the first Run does something
// visible against a loaded system trace.
char const* const kDefaultScript =
    "track = None\n"
    "for t in optiq.selection.tracks:\n"
    "    if t.type == optiq.TRACK_TYPE_EVENTS and t.num_entries > 0:\n"
    "        track = t\n"
    "        break\n"
    "if track is None:\n"
    "    optiq.result.text('No event track in the selection')\n"
    "else:\n"
    "    events = track.events(start=optiq.selection.start, end=optiq.selection.end)\n"
    "    if len(events) < 2:\n"
    "        optiq.result.text('Need at least two events on ' + track.name)\n"
    "    else:\n"
    "        gaps = [events[i].start - events[i - 1].end for i in range(1, len(events))]\n"
    "        mean = sum(gaps) / len(gaps)\n"
    "        max_dev = max(abs(g - mean) for g in gaps)\n"
    "        even = max_dev <= (abs(mean) * 0.1)\n"
    "        optiq.result.text(track.name)\n"
    "        optiq.result.text('events=' + str(len(events)))\n"
    "        optiq.result.text('mean_gap=' + str(mean))\n"
    "        optiq.result.text('max_dev=' + str(max_dev))\n"
    "        optiq.result.text('even' if even else 'uneven')\n";

ScriptEditor::ScriptEditor(DataProvider&                      data_provider,
                           std::shared_ptr<TimelineSelection> timeline_selection)
: m_data_provider(data_provider)
, m_timeline_selection(timeline_selection)
, m_running(false)
, m_result_ratio(SCRIPT_DEFAULT_RESULT_RATIO)
, m_progress_percent(0)
, m_source(kDefaultScript)
, m_output_is_error(false)
, m_status("Ready")
, m_approval(ScriptApproval::kNone)
, m_complete_token(EventManager::InvalidSubscriptionToken)
, m_progress_token(EventManager::InvalidSubscriptionToken)
{
    m_widget_name = GenUniqueName("ScriptEditor");

    m_complete_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kScriptExecuteComplete),
        [this](std::shared_ptr<RocEvent> e) {
            std::shared_ptr<ScriptExecuteCompleteEvent> event =
                std::dynamic_pointer_cast<ScriptExecuteCompleteEvent>(e);
            // Closing any tab posts one of these from cleanup, so a script
            // running here must only be answered by its own trace.
            if(!event || m_running_source_id.empty() ||
               event->GetSourceId() != m_running_source_id)
            {
                return;
            }
            m_running          = false;
            m_progress_percent = 0;
            m_running_source_id.clear();
            if(m_approval == ScriptApproval::kRunning)
            {
                m_approval = ScriptApproval::kFinished;
            }
            m_output          = event->GetText();
            m_output_is_error = !event->Succeeded();
            if(m_output_is_error)
            {
                if(!m_output.empty())
                {
                    m_output += '\n';
                }
                m_output += event->GetError();
                m_status = "Failed";
            }
            else
            {
                m_status = "Done";
            }
        });

    m_progress_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kRequestProgressUpdate),
        [this](std::shared_ptr<RocEvent> e) {
            std::shared_ptr<RequestProgressUpdateEvent> event =
                std::dynamic_pointer_cast<RequestProgressUpdateEvent>(e);
            if(!event || event->GetRequestType() != RequestType::kExecuteScript ||
               m_running_source_id.empty() ||
               event->GetSourceId() != m_running_source_id)
            {
                return;
            }
            m_progress_percent = event->GetProgressPercent();
            if(!event->GetMessage().empty())
            {
                m_status = event->GetMessage();
            }
        });
}

ScriptEditor::~ScriptEditor()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kScriptExecuteComplete), m_complete_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kRequestProgressUpdate), m_progress_token);
}

void
ScriptEditor::ProposeScript(const std::string& source)
{
    m_source = source;
    m_file_path.clear();
    m_output.clear();
    m_output_is_error  = false;
    m_progress_percent = 0;
    m_running          = false;
    m_approval         = ScriptApproval::kPending;
    m_status           = "Waiting for you";
}

ScriptApproval
ScriptEditor::ProposalState() const
{
    return m_approval;
}

void
ScriptEditor::ClearProposal()
{
    m_approval = ScriptApproval::kNone;
}

void
ScriptEditor::Reject()
{
    m_approval = ScriptApproval::kRejected;
    m_status   = "Not run";
}

void
ScriptEditor::Render()
{
    SettingsManager& settings = SettingsManager::GetInstance();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SCRIPT_WINDOW_PADDING);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, SCRIPT_ITEM_SPACING);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgMain));
    // The toolbar is pinned; each workspace pane owns its own scrolling.
    ImGui::BeginChild("##script_tab", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    RenderHeaderCard();
    RenderProposalBanner();

    // Details panels are normally much wider than they are tall. Spend that
    // width on a source/result workspace, and stack only when genuinely narrow.
    const ImVec2 workspace_size = ImGui::GetContentRegionAvail();
    const bool   columns        = workspace_size.x >= SCRIPT_TWO_COLUMN_MIN_WIDTH;
    const float  extent         = columns ? workspace_size.x : workspace_size.y;
    const float  min_pane =
        columns ? SCRIPT_PANE_MIN_WIDTH : SCRIPT_PANE_MIN_HEIGHT;

    // Columns are laid out with SameLine(0, 0), but stacking pays the item
    // spacing twice, above and below the handle.
    const float spacing =
        columns ? 0.0f : SCRIPT_ITEM_SPACING.y * 2.0f;

    // Neither pane may be dragged shut. The upper bound is written as a max so a
    // panel too small to hold both minimums still yields a usable number rather
    // than an inverted range.
    const float panes =
        std::max(1.0f, extent - SCRIPT_SPLITTER_THICKNESS - spacing);
    const float result = std::clamp(panes * m_result_ratio, std::min(min_pane, panes),
                                    std::max(min_pane, panes - min_pane));
    const float source = std::max(1.0f, panes - result);

    if(columns)
    {
        RenderSource(ImVec2(source, workspace_size.y));
        ImGui::SameLine(0.0f, 0.0f);
        RenderSplitter(true, extent, workspace_size.y);
        ImGui::SameLine(0.0f, 0.0f);
        RenderOutput(ImVec2(0.0f, workspace_size.y));
    }
    else
    {
        RenderSource(ImVec2(0.0f, source));
        RenderSplitter(false, extent, workspace_size.x);
        RenderOutput(ImVec2(0.0f, result));
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

// Drag handle between Source and Result, the same shape as the Ask Optiq dock
// splitter. Both layouts write the same ratio, so the split the user chose
// survives the panel getting narrow enough to flip to stacked and back.
void
ScriptEditor::RenderSplitter(bool columns, float split_extent, float cross_length)
{
    SettingsManager& settings = SettingsManager::GetInstance();
    const ImVec2     origin   = ImGui::GetCursorScreenPos();
    const ImVec2     size     = columns
                                    ? ImVec2(SCRIPT_SPLITTER_THICKNESS, cross_length)
                                    : ImVec2(cross_length, SCRIPT_SPLITTER_THICKNESS);

    ImGui::InvisibleButton("##script_splitter", size);
    const bool hovered = ImGui::IsItemHovered();
    const bool active  = ImGui::IsItemActive();
    if(hovered || active)
    {
        ImGui::SetMouseCursor(columns ? ImGuiMouseCursor_ResizeEW
                                      : ImGuiMouseCursor_ResizeNS);
    }
    if(active && split_extent > 0.0f)
    {
        // Result is the second pane either way, so dragging towards it shrinks
        // it and the delta is subtracted.
        const float delta =
            columns ? ImGui::GetIO().MouseDelta.x : ImGui::GetIO().MouseDelta.y;
        m_result_ratio = std::clamp(m_result_ratio - delta / split_extent, 0.0f, 1.0f);
    }

    ImGui::GetWindowDrawList()->AddRectFilled(
        origin, ImVec2(origin.x + size.x, origin.y + size.y),
        settings.GetColor(hovered || active ? Colors::kAccent
                                            : Colors::kSplitterColor));
}

// What the script is, in one dim line: where it came from, or what is loaded.
std::string
ScriptEditor::SubtitleText() const
{
    if(!m_file_path.empty())
    {
        return m_file_path;
    }
    switch(m_approval)
    {
    case ScriptApproval::kPending: return "Written by Ask Optiq";
    case ScriptApproval::kRejected: return "Declined - not run";
    case ScriptApproval::kRunning:
    case ScriptApproval::kFinished: return "Ran from Ask Optiq";
    case ScriptApproval::kFailedToStart: return "Could not be started";
    case ScriptApproval::kNone: break;
    }
    return "Python analysis over this trace";
}

void
ScriptEditor::RenderHeaderCard()
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();
    ImFont*           icon_font =
        settings.GetFontManager().GetFont(FontType::kIcon);

    BeginPanelCard("##script_header", PanelCardTone::kFrame, SCRIPT_CARD_PADDING, true,
                   &settings);

    const bool awaiting_decision = m_approval == ScriptApproval::kPending;
    const float gap              = style.ItemInnerSpacing.x;
    const float header_icon_size =
        ImGui::GetFontSize() * SCRIPT_HEADER_ICON_SCALE;

    // One compact row: title + dim subtitle share the line with the actions.
    ImGui::AlignTextToFramePadding();
    PanelIcon(awaiting_decision ? ICON_COMPASS : ICON_EDIT,
              awaiting_decision ? Colors::kAccent : Colors::kTextDim, &settings,
              header_icon_size);
    ImGui::SameLine(0.0f, gap);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Script");
    ImGui::SameLine(0.0f, gap * 2.0f);
    ImGui::AlignTextToFramePadding();
    PanelFieldLabel(SubtitleText().c_str(), false, &settings);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, SCRIPT_BUTTON_PADDING);
    const float button_height = ImGui::GetFrameHeight();
    const float icon_size     = button_height;
    const float action_icon_font_size =
        ImGui::GetFontSize() * SCRIPT_ACTION_ICON_SCALE;
    const float run_width =
        ImGui::CalcTextSize("Run").x + SCRIPT_BUTTON_PADDING.x * 2.0f + 4.0f;
    const float second_width =
        ImGui::CalcTextSize(awaiting_decision ? "Reject" : "Cancel").x +
        SCRIPT_BUTTON_PADDING.x * 2.0f;
    const float actions_gap = 6.0f;
    const float actions_width =
        run_width + second_width + icon_size * 2.0f + actions_gap * 3.0f;

    ImGui::SameLine(0.0f, 0.0f);
    const float leftover = ImGui::GetContentRegionAvail().x - actions_width;
    if(leftover > 0.0f)
    {
        ImGui::Dummy(ImVec2(leftover, 0.0f));
        ImGui::SameLine(0.0f, 0.0f);
    }

    const bool can_run = CanRun();
    if(!can_run)
    {
        ImGui::BeginDisabled();
    }
    if(AccentButton("Run", ImVec2(run_width, button_height), &settings))
    {
        Run();
    }
    if(!can_run)
    {
        ImGui::EndDisabled();
    }
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        if(m_running)
        {
            SetTooltipStyled("A script is already running");
        }
        else if(awaiting_decision)
        {
            SetTooltipStyled("Run the script Ask Optiq wrote, against this trace");
        }
        else
        {
            SetTooltipStyled("Run against this trace (wait for it to finish loading)");
        }
    }

    ImGui::SameLine(0.0f, actions_gap);
    if(awaiting_decision)
    {
        if(ColoredButton("Reject", settings.GetColor(Colors::kBgFrame),
                         settings.GetColor(Colors::kButtonHovered),
                         settings.GetColor(Colors::kButtonActive),
                         settings.GetColor(Colors::kTextMain),
                         "Do not run it. Ask Optiq is told you declined and carries "
                         "on without it.",
                         ImVec2(second_width, button_height)))
        {
            Reject();
        }
    }
    else
    {
        if(!m_running)
        {
            ImGui::BeginDisabled();
        }
        if(ColoredButton("Cancel", settings.GetColor(Colors::kBgFrame),
                         settings.GetColor(Colors::kButtonHovered),
                         settings.GetColor(Colors::kButtonActive),
                         settings.GetColor(Colors::kTextMain), nullptr,
                         ImVec2(second_width, button_height)))
        {
            Cancel();
        }
        if(!m_running)
        {
            ImGui::EndDisabled();
        }
    }

    ImGui::SameLine(0.0f, actions_gap);
    if(IconButton(ICON_FOLDER, icon_font, ImVec2(icon_size, icon_size), "Open a .py file",
                  false, ImVec2(0.0f, 0.0f), settings.GetColor(Colors::kButton),
                  settings.GetColor(Colors::kButtonHovered),
                  settings.GetColor(Colors::kButtonActive), "##script_load",
                  action_icon_font_size))
    {
        LoadFromFile();
    }
    ImGui::SameLine(0.0f, actions_gap);
    if(IconButton(ICON_DOCUMENT, icon_font, ImVec2(icon_size, icon_size),
                  "Save this script to a .py file", false, ImVec2(0.0f, 0.0f),
                  settings.GetColor(Colors::kButton),
                  settings.GetColor(Colors::kButtonHovered),
                  settings.GetColor(Colors::kButtonActive), "##script_save",
                  action_icon_font_size))
    {
        SaveToFile();
    }
    ImGui::PopStyleVar();

    EndPanelCard();
}

void
ScriptEditor::RenderProposalBanner()
{
    if(m_approval != ScriptApproval::kPending)
    {
        return;
    }

    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();

    BeginPanelCard("##script_proposal", PanelCardTone::kPanel, SCRIPT_CARD_PADDING, true,
                   &settings);
    ImGui::AlignTextToFramePadding();
    PanelIcon(ICON_COMPASS, Colors::kAccent, &settings,
              ImGui::GetFontSize() * SCRIPT_HEADER_ICON_SCALE);
    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextWarning));
    ImGui::TextUnformatted("Ask Optiq wrote this - Run or Reject.");
    ImGui::PopStyleColor();
    EndPanelCard();
}

void
ScriptEditor::RenderSource(const ImVec2& size)
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, PANEL_CARD_ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SCRIPT_CARD_PADDING);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 4.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgPanel));
    ImGui::PushStyleColor(ImGuiCol_Border, settings.GetColor(Colors::kPanelBorderSubtle));
    ImGui::BeginChild("##script_source_card", size, ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    PanelFieldLabel("Source", false, &settings);
    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x * 2.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
    ImGui::TextUnformatted("Python");
    ImGui::PopStyleColor();

    const float editor_height = std::max(1.0f, ImGui::GetContentRegionAvail().y);

    ImGui::PushFont(settings.GetFontManager().GetFont(FontType::kCode), 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, SCRIPT_EDITOR_INSET_ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, settings.GetColor(Colors::kBgMain));
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextMain));
    // Negative x fills the card; the inset well is the code surface.
    InputTextMultilineString("##script_source", m_source,
                             ImVec2(-FLT_MIN, editor_height),
                             ImGuiInputTextFlags_AllowTabInput);
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
    ImGui::PopFont();

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

void
ScriptEditor::RenderOutput(const ImVec2& size)
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, PANEL_CARD_ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SCRIPT_CARD_PADDING);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 4.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgFrame));
    ImGui::PushStyleColor(ImGuiCol_Border, settings.GetColor(Colors::kPanelBorderSubtle));
    ImGui::BeginChild("##script_result_card", size, ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    PanelFieldLabel("Result", false, &settings);

    Colors status_color = Colors::kTextDim;
    if(m_running)
    {
        status_color = Colors::kAccent;
    }
    else if(m_output_is_error)
    {
        status_color = Colors::kTextError;
    }
    else if(m_status == "Done")
    {
        status_color = Colors::kTextSuccess;
    }

    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x * 2.0f);
    if(m_running)
    {
        RenderLoadingIndicatorDots(SCRIPT_DOT_RADIUS, SCRIPT_DOT_COUNT, SCRIPT_DOT_SPACING,
                                   settings.GetColor(Colors::kAccent), SCRIPT_DOT_SPEED);
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(status_color));
    ImGui::TextUnformatted(m_status.c_str());
    ImGui::PopStyleColor();
    if(m_running && m_progress_percent > 0)
    {
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
        ImGui::Text("%u%%", static_cast<unsigned>(m_progress_percent));
        ImGui::PopStyleColor();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, SCRIPT_EDITOR_INSET_ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgMain));
    ImGui::PushStyleColor(ImGuiCol_Border, settings.GetColor(Colors::kPanelBorderSubtle));
    ImGui::BeginChild("##script_result_body",
                      ImVec2(0.0f, std::max(1.0f, ImGui::GetContentRegionAvail().y)),
                      ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::PushFont(settings.GetFontManager().GetFont(FontType::kCode), 0.0f);
    if(m_output.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
        ImGui::TextWrapped("Nothing yet. Press Run to execute the script against this "
                           "trace.");
        ImGui::PopStyleColor();
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Text,
                              settings.GetColor(m_output_is_error ? Colors::kTextError
                                                                  : Colors::kTextMain));
        ImGui::TextUnformatted(m_output.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::PopFont();

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

bool
ScriptEditor::CanRun() const
{
    return !m_running && m_data_provider.GetState() == ProviderState::kReady;
}

void
ScriptEditor::Run()
{
    if(m_data_provider.GetState() != ProviderState::kReady)
    {
        m_status = "Trace still loading";
        return;
    }
    m_output_is_error = false;

    std::vector<uint64_t> track_ids;
    double                start_ts   = 0.0;
    double                end_ts     = 0.0;
    bool                  have_range = false;
    if(m_timeline_selection)
    {
        m_timeline_selection->GetSelectedTracks(track_ids);
        have_range = m_timeline_selection->GetSelectedTimeRange(start_ts, end_ts);
    }
    if(!have_range)
    {
        start_ts = m_data_provider.DataModel().GetTimeline().GetStartTime();
        end_ts   = m_data_provider.DataModel().GetTimeline().GetEndTime();
    }

    m_output.clear();
    m_output_is_error  = false;
    m_progress_percent = 0;
    if(!m_data_provider.ExecuteScript(m_source, track_ids, start_ts, end_ts))
    {
        m_status          = "Could not start";
        m_output_is_error = true;
        // An approved script that never started still has to answer the
        // assistant, or it waits out the whole approval deadline for a run
        // that was never going to happen.
        if(m_approval == ScriptApproval::kPending)
        {
            m_approval = ScriptApproval::kFailedToStart;
        }
        NotificationManager::GetInstance().Show("Could not start the script",
                                                NotificationLevel::Error);
        return;
    }
    if(m_approval == ScriptApproval::kPending)
    {
        m_approval = ScriptApproval::kRunning;
    }
    m_running           = true;
    m_running_source_id = m_data_provider.GetTraceFilePath();
    m_status            = "Running";
}

void
ScriptEditor::Cancel()
{
    if(!m_data_provider.CancelScript())
    {
        // Nothing left to cancel, so no completion event is coming and the
        // editor would sit on Running forever.
        m_running = false;
        m_running_source_id.clear();
        m_status = "Not running";
        return;
    }
    m_status = "Cancelling";
}

void
ScriptEditor::LoadFromFile()
{
    AppWindow* app = AppWindow::GetInstance();
    if(!app)
    {
        return;
    }
    FileFilter py_filter;
    py_filter.m_name       = "Python";
    py_filter.m_extensions = { "py" };
    app->ShowOpenFileDialog("Load Script", { py_filter }, m_file_path,
                            [this](std::string path) { ReadFile(path); });
}

void
ScriptEditor::SaveToFile()
{
    AppWindow* app = AppWindow::GetInstance();
    if(!app)
    {
        return;
    }
    FileFilter py_filter;
    py_filter.m_name       = "Python";
    py_filter.m_extensions = { "py" };
    app->ShowSaveFileDialog("Save Script", { py_filter }, m_file_path,
                            [this](std::string path) { WriteFile(path); });
}

void
ScriptEditor::ReadFile(const std::string& path)
{
    if(path.empty())
    {
        return;
    }
    std::ifstream file(path, std::ios::binary);
    if(!file.is_open())
    {
        m_status = "Could not open file";
        NotificationManager::GetInstance().Show("Could not open " + path,
                                                NotificationLevel::Error);
        return;
    }
    m_source.assign((std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());
    m_file_path = path;
    m_status    = "Loaded";
}

void
ScriptEditor::WriteFile(const std::string& path)
{
    if(path.empty())
    {
        return;
    }
    std::ofstream file(path, std::ios::binary);
    if(!file.is_open())
    {
        m_status = "Could not write file";
        NotificationManager::GetInstance().Show("Could not write " + path,
                                                NotificationLevel::Error);
        return;
    }
    file.write(m_source.data(), static_cast<std::streamsize>(m_source.size()));
    m_file_path = path;
    m_status    = "Saved";
}

}  // namespace View
}  // namespace RocProfVis
