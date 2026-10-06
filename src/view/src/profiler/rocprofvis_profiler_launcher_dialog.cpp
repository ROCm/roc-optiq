// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_profiler_launcher_dialog.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "rocprofvis_launch_shared_tabs.h"
#include "rocprofvis_rocprof_sys_backend.h"
// TEMPORARY (remote/SSH): remove guard when remote graduates.
#ifdef ROCPROFVIS_ENABLE_REMOTE
#include "remote/rocprofvis_ssh_auth_modal.h"
#endif
#include "widgets/rocprofvis_widget.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_notification_manager.h"
#include "icons/rocprovfis_icon_defines.h"
#include "imgui.h"
#include <cfloat>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <vector>

namespace RocProfVis
{
namespace View
{

namespace
{
// Configure-view split: the form and the command-preview panel are divided by a
// draggable splitter, seeded to a 3:2 (form:preview) ratio on first open and
// clamped so neither side collapses.
constexpr float kSplitterWidth       = 8.0f;
constexpr float kMinPreviewWidth     = 300.0f;
constexpr float kMinFormWidth        = 320.0f;
constexpr float kInitialPreviewRatio = 2.0f / 5.0f;

constexpr ImVec2 LAUNCHER_WINDOW_PADDING = ImVec2(16.0f, 12.0f);
constexpr ImVec2 LAUNCHER_FRAME_PADDING  = ImVec2(8.0f, 4.0f);
constexpr ImVec2 LAUNCHER_ITEM_SPACING   = ImVec2(8.0f, 10.0f);
constexpr float  LAUNCHER_GROUP_GAP      = 16.0f;

// Gap between the configuration form's cards and its scrollbar.
constexpr float FORM_GUTTER = 8.0f;

// Both views end with the same footer, and Close and the primary action keep
// these widths in each, so switching views never moves those buttons.
constexpr float FOOTER_BUTTON_WIDTH  = 120.0f;
constexpr float FOOTER_PRIMARY_WIDTH = 160.0f;

// Separates the parts of the run summary and status lines.
constexpr const char* SUMMARY_SEPARATOR = "  \xC2\xB7  ";

// Height of the footer: its divider with a gap either side, and one row of
// buttons. Each view's content stops this far above the bottom. The divider's
// thickness counts toward layout, as ImGui::Separator() takes it.
float FooterHeight()
{
    const ImGuiStyle& style = ImGui::GetStyle();
    return style.ItemSpacing.y * 2.0f + std::max(style.SeparatorSize, 1.0f) +
           ImGui::GetFrameHeight();
}

// A secondary footer button: a common minimum width so short labels line up,
// wider only when the label needs it.
float FooterButtonWidth(const char* label)
{
    return std::max(ButtonWidth(label), FOOTER_BUTTON_WIDTH);
}

// Where a row of footer buttons `width` wide starts so that it ends at the right
// edge. Call at the start of the footer's line.
float FooterActionsX(float width)
{
    return ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - width);
}

// One dim line cut to `width`, with the full text on hover when it was cut.
void FittedDimText(std::string const& text, float width)
{
    const std::string shown = ElideWithEllipsis(text, width, text.size());
    ImGui::PushStyleColor(ImGuiCol_Text, SettingsManager::Get().GetColor(Colors::kTextDim));
    ImGui::TextUnformatted(shown.c_str());
    ImGui::PopStyleColor();
    if (shown != text && BeginItemTooltipStyled())
    {
        ImGui::TextUnformatted(text.c_str());
        EndTooltipStyled();
    }
}

// The target fields that appear in the launched command.
bool SameCommandTarget(TargetSpec const& a, TargetSpec const& b)
{
    return a.executable == b.executable && a.arguments == b.arguments &&
           a.working_directory == b.working_directory &&
           a.output_directory == b.output_directory;
}

// "5.3 s", "2 min 05 s", "1 h 02 min": a bare seconds count stops being
// readable once a run takes minutes.
std::string FormatElapsed(double seconds)
{
    constexpr int SECONDS_PER_MINUTE = 60;
    constexpr int SECONDS_PER_HOUR   = 3600;

    const int whole = static_cast<int>(seconds);
    char      buf[32];
    if (whole < SECONDS_PER_MINUTE)
    {
        std::snprintf(buf, sizeof(buf), "%.1f s", seconds);
    }
    else if (whole < SECONDS_PER_HOUR)
    {
        std::snprintf(buf, sizeof(buf), "%d min %02d s", whole / SECONDS_PER_MINUTE,
                      whole % SECONDS_PER_MINUTE);
    }
    else
    {
        std::snprintf(buf, sizeof(buf), "%d h %02d min", whole / SECONDS_PER_HOUR,
                      (whole % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE);
    }
    return buf;
}
}  // namespace

ProfilerLauncherDialog::ProfilerLauncherDialog(AppWindow* app_window)
    : m_app_window(app_window)
    , m_orchestrator(app_window)
#ifdef ROCPROFVIS_ENABLE_REMOTE
    , m_remote_uri(std::make_shared<RemoteUri>())
    , m_connection_store(SshConnectionStore::GetInstance())
    , m_ssh_settings_dialog(nullptr)
    , m_remote_show_progress_popup(false)
    , m_remote_last_progress()
#endif
    , m_should_open(false)
    , m_show_window(false)
    , m_show_run_view(false)
    , m_show_advanced_window(false)
    , m_preview_width(420.0f)
    , m_preview_width_initialized(false)
    , m_arg_input()
    , m_env_name_input()
    , m_env_value_input()
    , m_run_start_time(0.0)
    , m_run_end_time(0.0)
    , m_last_seen_state(kRPVProfilerStateIdle)
    , m_backend_index(0)
    , m_config()
    , m_execution_cache()
    , m_preset_manager()
    , m_current_preset_name()
    , m_output_text()
    , m_error_message()
    , m_auto_scroll_output(true)
{
    m_backends.push_back(std::make_unique<RocprofSysBackend>());

    m_config.profiler_id = m_backends[0]->Id();
    SyncToolWithBackend();
    m_backends[0]->LoadSettings(jt::Json());
    m_config.backend_payload = m_backends[0]->SaveSettings();

    LoadFromSettings();
    RefreshExecutionCache();

#ifdef ROCPROFVIS_ENABLE_REMOTE
    SyncSelectedConnection();
#endif

    // Run orchestration (sessions, profiler-state events, teardown) is owned by
    // m_orchestrator; the dialog only authors config and renders.
}

ProfilerLauncherDialog::~ProfilerLauncherDialog() = default;

void ProfilerLauncherDialog::Show()
{
    m_should_open = true;
    m_execution_cache_dirty = true;
    // Closing only hid the window; any prior run is still on the orchestrator.
    // Reopen on the configuration screen so "View Last Run" (or "View Run") is
    // on the button row. The run view is one click away and the capture is not
    // cancelled.
    m_show_run_view = false;
#ifdef ROCPROFVIS_ENABLE_REMOTE
    SyncSelectedConnection();
#endif
}

void ProfilerLauncherDialog::Render()
{
    if (m_should_open)
    {
        m_show_window = true;
        m_should_open = false;
        ImGui::SetWindowFocus("Launch Profiler");
    }

    if (!m_show_window)
    {
#ifdef ROCPROFVIS_ENABLE_REMOTE
        // Auth is rendered by AppWindow; the download-progress popup is ours
        // and must still appear if the user hid the launcher mid-transfer.
        RenderRemotePopups();
#endif
        return;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(1000, 700), ImGuiCond_FirstUseEver);

    // AppWindow renders us inside its main-window scope, which zeroes
    // WindowPadding / ItemSpacing / WindowRounding for the flush main layout.
    // Restore the app's standard style so this dialog matches the rest of the
    // app (WindowPadding must be set before Begin() to take effect).
    const ImGuiStyle& def = SettingsManager::Get().GetDefaultStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LAUNCHER_WINDOW_PADDING);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, def.WindowRounding);

    bool window_open = true;
    bool visible     = ImGui::Begin("Launch Profiler", &window_open,
                                    ImGuiWindowFlags_NoScrollbar);
    if (visible)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, LAUNCHER_FRAME_PADDING);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, LAUNCHER_ITEM_SPACING);

        // The dialog is a small two-step wizard: author the run (configure), then
        // watch it (run). Splitting them keeps the configuration uncluttered and
        // gives the live output the whole window while a run is in flight.
        if (m_show_run_view)
        {
            RenderRunView();
        }
        else
        {
            RenderConfigureView();
        }

        ImGui::PopStyleVar(2);  // FramePadding, ItemSpacing
    }
    ImGui::End();
    ImGui::PopStyleVar(2);  // WindowPadding, WindowRounding

    // The Advanced Options window is a separate floating window (only relevant
    // while configuring).
    if (!m_show_run_view)
    {
        RenderAdvancedWindow();
    }

#ifdef ROCPROFVIS_ENABLE_REMOTE
    // SSH settings dialog, auth prompts and download progress (rendered outside
    // the main window scope, mirroring SshTestDialog).
    RenderRemotePopups();
#endif

    if (!window_open)
    {
        Hide();
    }
}

void ProfilerLauncherDialog::RenderConfigureView()
{
    // Sync typed settings to backend_payload so preset save sees current values
    IProfilerBackend* backend = m_backends[m_backend_index].get();
    m_config.backend_payload = backend->SaveSettings();

#ifdef ROCPROFVIS_ENABLE_REMOTE
    // Keep the launch profile's SSH connection reference in sync with the
    // currently selected connection so saved profiles reference it.
    m_config.ssh_connection_ref = m_selected_connection_id;
#endif

    RenderToolbar();
    backend = m_backends[m_backend_index].get();

    // Reserve exactly the height the bottom block (warnings + error + footer)
    // needs, so it stays pinned to the bottom with no dead space.
    auto warnings = backend->GetWarnings(m_config);

    const ImGuiStyle& style          = ImGui::GetStyle();
    const float       line_h         = ImGui::GetTextLineHeightWithSpacing();
    float             bottom_reserve = FooterHeight() + warnings.size() * line_h;
    if (!m_error_message.empty())
    {
        float wrap_w = ImGui::GetContentRegionAvail().x;
        bottom_reserve += ImGui::CalcTextSize(m_error_message.c_str(), nullptr, false,
                                              wrap_w).y + style.ItemSpacing.y;
    }

    // Layout only: the cards inside paint their own surfaces over the window.
    ImGui::BeginChild("MainPane", ImVec2(0, -bottom_reserve), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoBackground);
    RenderMainContent();
    ImGui::EndChild();

    // Warnings from backend
    if (!warnings.empty())
    {
        SettingsManager& settings = SettingsManager::Get();
        for (auto const& w : warnings)
        {
            Colors      color_id;
            const char* prefix;
            switch (w.level)
            {
                case WarningMessage::kError:
                    color_id = Colors::kTextError;
                    prefix   = "Error: ";
                    break;
                case WarningMessage::kWarning:
                    color_id = Colors::kTextWarning;
                    prefix   = "Warning: ";
                    break;
                default:
                    color_id = Colors::kAccent;
                    prefix   = "Hint: ";
                    break;
            }
            ImVec4 color = ImGui::ColorConvertU32ToFloat4(settings.GetColor(color_id));
            ImGui::TextColored(color, "%s%s", prefix, w.text.c_str());
        }
    }

    // Pre-launch validation / immediate launch errors surface here since the
    // output console (which normally shows them) lives in the run view.
    if (!m_error_message.empty())
    {
        SettingsManager& settings = SettingsManager::Get();
        ImVec4 err_color =
            ImGui::ColorConvertU32ToFloat4(settings.GetColor(Colors::kTextError));
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(err_color, "%s", m_error_message.c_str());
        ImGui::PopTextWrapPos();
    }

    // Launch button row
    RenderButtonRow();
}

void ProfilerLauncherDialog::RenderRunView()
{
    // The configure view's frame: one line where its toolbar sits, a card
    // filling the middle, and the same footer, so switching views moves nothing.
    ImGui::AlignTextToFramePadding();
    FittedDimText(BuildRunSummary(), ImGui::GetContentRegionAvail().x);

    BeginCodePanel("RunOutput", ImVec2(0.0f, -FooterHeight()));

    // Collapse the local profiler state / remote workflow phase into a single
    // badge so remote runs show "Connecting", "Downloading", etc. (and only show
    // "Completed" once the trace is local), with the phase detail beside it.
    std::string        status_label = "Idle";
    ConsoleStatusLevel status_level = ConsoleStatusLevel::kIdle;
    std::string        status_detail;
    ComputeConsoleStatus(status_label, status_level, status_detail);

    // Status header: a colored pill + a live elapsed timer.
    SettingsManager& settings = SettingsManager::Get();
    Colors           pill_color_id;
    switch (status_level)
    {
        case ConsoleStatusLevel::kSuccess: pill_color_id = Colors::kTextSuccess; break;
        case ConsoleStatusLevel::kError:   pill_color_id = Colors::kTextError;   break;
        case ConsoleStatusLevel::kRunning: pill_color_id = Colors::kAccent;      break;
        default:                           pill_color_id = Colors::kTextDim;     break;
    }
    const float spacing   = ImGui::GetStyle().ItemSpacing.x;
    const float actions_w = CheckboxWidth("Auto-scroll") + ButtonWidth("Copy") +
                            ButtonWidth("Clear") + spacing * 2.0f;
    const float status_end_x =
        ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - actions_w - spacing;
    StatusPill(status_label.c_str(), settings.GetColor(pill_color_id));

    std::string info;
    if (m_run_start_time > 0.0)
    {
        double end     = (m_run_end_time > 0.0) ? m_run_end_time : ImGui::GetTime();
        double elapsed = end - m_run_start_time;
        info           = "Elapsed " + FormatElapsed(elapsed);
    }
    if (!status_detail.empty())
    {
        info += (info.empty() ? "" : SUMMARY_SEPARATOR) + status_detail;
    }
    if (!info.empty())
    {
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        FittedDimText(info, status_end_x - ImGui::GetCursorPosX());
    }

    SameLineRightAligned(actions_w);
    ImGui::Checkbox("Auto-scroll##Output", &m_auto_scroll_output);
    ImGui::SameLine();
    if (ImGui::Button("Copy##OutputCopy"))
    {
        ImGui::SetClipboardText(m_output_text.c_str());
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear##OutputClear"))
    {
        m_output_text.clear();
        m_output_preamble.clear();
        m_output_epilogue.clear();
        m_process_output_raw.clear();
        m_process_output_stripped.clear();
        m_error_message.clear();
    }
    RenderCodeBox("OutputText", m_output_text, m_auto_scroll_output);
    EndCodePanel();

    RenderRunButtonRow();
}

std::string ProfilerLauncherDialog::BuildRunSummary() const
{
    std::ostringstream ss;

    if (m_backend_index >= 0 && m_backend_index < static_cast<int>(m_backends.size()))
    {
        ss << m_backends[m_backend_index]->DisplayName();
        std::string const tool_name = CurrentToolDisplayName();
        if (!tool_name.empty())
        {
            ss << " (" << tool_name << ")";
        }
    }

    if (!m_config.target.executable.empty())
    {
        ss << SUMMARY_SEPARATOR << m_config.target.executable;
        if (!m_config.target.arguments.empty())
        {
            ss << " " << m_config.target.arguments;
        }
    }

#ifdef ROCPROFVIS_ENABLE_REMOTE
    if (IsSshMode())
    {
        constexpr int DEFAULT_SSH_PORT = 22;
        ss << SUMMARY_SEPARATOR << m_remote_uri->GetRemoteUserString() << "@"
           << m_remote_uri->GetRemoteHostString();
        if (m_remote_uri->GetRemotePortInt() != DEFAULT_SSH_PORT)
        {
            ss << ":" << m_remote_uri->GetRemotePortString();
        }
    }
    else
    {
        ss << SUMMARY_SEPARATOR << "This machine";
    }
#else
    ss << SUMMARY_SEPARATOR << "This machine";
#endif

    return ss.str();
}

void ProfilerLauncherDialog::RenderToolbar()
{
    // Combos fit their widest entry, so a long profiler name is never cut off.
    auto fit_combo_width = [](std::vector<std::string> const& labels) {
        float widest = 0.0f;
        for (auto const& label : labels)
        {
            widest = std::max(widest, ImGui::CalcTextSize(label.c_str()).x);
        }
        return widest + ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.x * 2.0f;
    };

    // Profiler selector
    std::vector<std::string> backend_names;
    for (auto const& entry : m_backends)
    {
        backend_names.emplace_back(entry->DisplayName());
    }
    PanelFieldLabel("Profiler");
    ImGui::SameLine();
    ImGui::PushItemWidth(fit_combo_width(backend_names));
    if (ImGui::BeginCombo("##ProfilerBackend",
                          m_backends[m_backend_index]->DisplayName()))
    {
        for (size_t i = 0; i < m_backends.size(); i++)
        {
            bool selected = (static_cast<int>(i) == m_backend_index);
            if (ImGui::Selectable(m_backends[i]->DisplayName(), selected))
            {
                SwitchBackend(static_cast<int>(i));
            }
        }
        // Placeholders for the profilers this launcher is designed to grow into.
        // Disabled until their backends land; listed so the intended scope
        // (rocprof-sys, rocprof-compute, rocprofv3) is visible.
        ImGui::BeginDisabled();
        ImGui::Selectable("ROCm Compute Profiler (coming soon)", false);
        ImGui::Selectable("rocprofv3 (coming soon)", false);
        ImGui::EndDisabled();
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    ImGui::SameLine(0.0f, LAUNCHER_GROUP_GAP);

    // Tool selector
    IProfilerBackend const* backend = m_backends[m_backend_index].get();
    auto tools = backend->GetTools();
    std::vector<std::string> tool_names;
    for (auto const& option : tools)
    {
        tool_names.push_back(option.display_name);
    }
    PanelFieldLabel("Tool");
    ImGui::SameLine();
    ImGui::PushItemWidth(fit_combo_width(tool_names));
    if (ImGui::BeginCombo("##ToolSelector", CurrentToolDisplayName().c_str()))
    {
        for (auto const& option : tools)
        {
            if (ImGui::Selectable(option.display_name.c_str(), option.tool == m_config.tool))
            {
                m_config.tool = option.tool;
                m_execution_cache_dirty = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    ImGui::SameLine(0.0f, LAUNCHER_GROUP_GAP);

    // Saved launch profiles (Optiq JSON presets)
    std::string load_name = RenderSavedProfileBar(
        m_preset_manager, m_config.profiler_id,
        m_current_preset_name, m_config, backend, m_app_window);

    if (!load_name.empty())
    {
        LaunchConfig loaded;
        if (m_preset_manager.LoadPreset(load_name, m_config.profiler_id, loaded))
        {
            m_config = loaded;
            m_execution_cache_dirty = true;
            m_backends[m_backend_index]->LoadSettings(m_config.backend_payload);
            SyncToolWithBackend();
            backend = m_backends[m_backend_index].get();
#ifdef ROCPROFVIS_ENABLE_REMOTE
            // Resolve the profile's referenced SSH connection (if any) so the
            // remote section reflects the saved connection.
            if (!m_config.ssh_connection_ref.empty() &&
                m_connection_store.Get(m_config.ssh_connection_ref) != nullptr)
            {
                m_selected_connection_id = m_config.ssh_connection_ref;
                ApplySelectedConnection();
            }
#endif
        }
    }
}

void ProfilerLauncherDialog::RenderMainContent()
{
    IProfilerBackend const* backend = m_backends[m_backend_index].get();

    // Backend tabs are split into "general" (shown here) and "advanced" (opened
    // in a separate window), so the common path stays clean.
    auto tabs = backend->GetTabs(m_config.tool);
    std::vector<TabDescriptor const*> general_tabs;
    std::vector<TabDescriptor const*> advanced_tabs;
    for (auto const& tab : tabs)
    {
        (tab.advanced ? advanced_tabs : general_tabs).push_back(&tab);
    }

    // The configuration form goes on the left; the live command preview fills a
    // dedicated full-height panel on the right, with a draggable splitter to
    // adjust the preview width.
    const float avail = ImGui::GetContentRegionAvail().x;

    // Seed the split on first layout; afterwards the width follows the splitter.
    if (!m_preview_width_initialized && avail > 0.0f)
    {
        m_preview_width             = (avail - kSplitterWidth) * kInitialPreviewRatio;
        m_preview_width_initialized = true;
    }

    float max_preview = avail - kSplitterWidth - kMinFormWidth;
    if (max_preview < kMinPreviewWidth)
    {
        max_preview = kMinPreviewWidth;
    }
    m_preview_width  = std::clamp(m_preview_width, kMinPreviewWidth, max_preview);
    float left_w     = avail - kSplitterWidth - m_preview_width;

    // --- Left: the configuration form (scrolls if it overflows) ---
    // A slim, trackless scrollbar with a gutter before it, so it neither crowds
    // the cards nor runs into the splitter. The form starts one gutter early,
    // inside the window padding, so its cards stay aligned with the toolbar.
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() - FORM_GUTTER);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(FORM_GUTTER, 0.0f));
    PushSlimScrollbarStyle();
    ImGui::BeginChild("cfg_form", ImVec2(left_w + FORM_GUTTER, 0.0f),
                      ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoBackground);
    PopSlimScrollbarStyle();
    ImGui::PopStyleVar();
#ifdef ROCPROFVIS_ENABLE_REMOTE
    // Where to run first: pick the machine before the paths.
    BeginLaunchCard("card_connection");
    LaunchCardHeader(ICON_CHAIN, "Where to run");
    RenderRemoteSection();
    RenderToolResolutionNotice();
    EndLaunchCard();
#else
    RenderToolResolutionNotice();
#endif

    BeginLaunchCard("card_target");
    LaunchCardHeader(ICON_OPEN, "Target",
                     "The program to profile and where its results go");
#ifdef ROCPROFVIS_ENABLE_REMOTE
    // In remote (SSH) mode the Target Browse buttons open the shared remote file
    // browser (same UI as the "Open Remote Trace" dialog). Local mode keeps the
    // OS file/path dialogs, so the callbacks are only wired when targeting SSH.
    // The connection is chosen under "Where to run", never in the browser, so
    // Browse stays disabled until one is set up.
    std::function<void()> on_browse_program;
    std::function<void()> on_browse_output;
    if (IsSshMode() && HasRemoteConnection())
    {
        on_browse_program = [this]()
        {
            ApplySelectedConnection();
            EnsureRemoteFileBrowser();
            m_remote_file_browser->Open(
                m_config.target.executable, RemoteFileBrowser::PickMode::kFile,
                [this](const std::string& path) { m_config.target.executable = path; });
        };
        on_browse_output = [this]()
        {
            ApplySelectedConnection();
            EnsureRemoteFileBrowser();
            m_remote_file_browser->Open(
                m_config.target.output_directory, RemoteFileBrowser::PickMode::kDirectory,
                [this](const std::string& path) { m_config.target.output_directory = path; });
        };
    }
    RenderTargetSection(m_config.target, m_config.connection, m_app_window,
                        on_browse_program, on_browse_output);
#else
    RenderTargetSection(m_config.target, m_config.connection, m_app_window);
#endif
    EndLaunchCard();

    BeginLaunchCard("card_general");
    LaunchCardHeader(ICON_CHART_BAR, "Profiling Options");
    if (general_tabs.size() == 1)
    {
        m_execution_cache_dirty |= general_tabs[0]->render_fn();
    }
    else if (!general_tabs.empty())
    {
        if (ImGui::BeginTabBar("GeneralTabs"))
        {
            for (auto const* tab : general_tabs)
            {
                if (ImGui::BeginTabItem(tab->display_name.c_str()))
                {
                    m_execution_cache_dirty |= tab->render_fn();
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    }
    if (!advanced_tabs.empty())
    {
        ImGui::Spacing();
        if (ImGui::Button("Advanced Options..."))
        {
            m_show_advanced_window = true;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Sampling, ROCm domains, Perfetto, parallelism, logging");
        }
    }
    EndLaunchCard();

    ImGui::EndChild();

    // --- Draggable splitter to resize the preview panel ---
    SettingsManager& settings = SettingsManager::Get();
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, settings.GetColor(Colors::kSplitterColor));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, settings.GetColor(Colors::kAccent));
    ImGui::Button("##cfg_splitter", ImVec2(kSplitterWidth, ImGui::GetContentRegionAvail().y));
    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    if (ImGui::IsItemActive())
    {
        // Dragging right widens the form (narrows the preview) and vice versa.
        m_preview_width -= ImGui::GetIO().MouseDelta.x;
    }
    ImGui::PopStyleColor(3);
    ImGui::SameLine(0.0f, 0.0f);

    // --- Right: full-height command preview panel ---
    BeginCodePanel("cfg_preview", ImVec2(0.0f, 0.0f));
    RenderCommandPreview(m_execution_cache.command_preview);
    EndCodePanel();
}

void ProfilerLauncherDialog::RenderToolResolutionNotice()
{
    SettingsManager& settings = SettingsManager::Get();

    if (!m_execution_cache.resolve_error.empty())
    {
        // Said here rather than only on Launch, because "the profiler is not
        // installed" is not something the user should discover by pressing a
        // button. The command preview still shows the bare tool name, so
        // this line is what explains why it has no path.
        constexpr float NOTICE_ROUNDING     = 6.0f;
        constexpr float NOTICE_FILL_ALPHA   = 0.10f;
        constexpr float NOTICE_BORDER_ALPHA = 0.45f;
        const ImU32     warning = settings.GetColor(Colors::kTextWarning);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ApplyAlpha(warning, NOTICE_FILL_ALPHA));
        ImGui::PushStyleColor(ImGuiCol_Border, ApplyAlpha(warning, NOTICE_BORDER_ALPHA));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, NOTICE_ROUNDING);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, settings.GetDefaultStyle().WindowPadding);
        ImGui::BeginChild("tool_notice", ImVec2(0.0f, 0.0f),
                          ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders |
                              ImGuiChildFlags_AlwaysUseWindowPadding,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::TextWrapped("%s", m_execution_cache.resolve_error.c_str());
#ifdef ROCPROFVIS_ENABLE_REMOTE
        // The usual way out is a machine that has ROCm, so offer it in place.
        if (AccentButton("Run on a remote machine", ImVec2(0.0f, 0.0f), &settings))
        {
            m_config.connection     = ConnectionType::kSsh;
            m_execution_cache_dirty = true;
        }
#endif
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
        return;
    }

    // A tool directory travels with the profile, so a profile from elsewhere can
    // arrive with one set. Showing it means an imported profile cannot quietly
    // run a different build of the tool than the user expects.
    if (!m_config.tool_directory.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextWarning));
        ImGui::TextWrapped("Using tools from %s%s", m_config.tool_directory.c_str(),
                           IsSshMode() ? " on the remote host" : "");
        ImGui::PopStyleColor();
    }
}

void ProfilerLauncherDialog::RenderAdvancedWindow()
{
    if (!m_show_advanced_window)
    {
        return;
    }

    IProfilerBackend const* backend = m_backends[m_backend_index].get();

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(700, 560), ImGuiCond_FirstUseEver);

    // Restore the app's standard style (AppWindow's scope zeroes window padding /
    // item spacing / rounding). WindowPadding must be set before Begin().
    const ImGuiStyle& def = SettingsManager::Get().GetDefaultStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LAUNCHER_WINDOW_PADDING);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, def.WindowRounding);

    bool open    = true;
    bool visible = ImGui::Begin("Advanced Profiling Options", &open);
    if (visible)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, LAUNCHER_FRAME_PADDING);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, LAUNCHER_ITEM_SPACING);

        ImGui::TextDisabled("Fine-grained settings, applied on top of the selected preset.");

        // Where the tools live is a property of the launch profile rather than of
        // any one backend, so it sits above the backend tabs.
        std::function<void()> on_browse_tool_dir;
#ifdef ROCPROFVIS_ENABLE_REMOTE
        if (IsSshMode() && HasRemoteConnection())
        {
            on_browse_tool_dir = [this]()
            {
                ApplySelectedConnection();
                EnsureRemoteFileBrowser();
                m_remote_file_browser->Open(
                    m_config.tool_directory, RemoteFileBrowser::PickMode::kDirectory,
                    [this](const std::string& path)
                    {
                        m_config.tool_directory = path;
                        m_execution_cache_dirty = true;
                    });
            };
        }
#endif
        if (RenderToolLocationSection(m_config.tool_directory, m_config.connection, m_app_window,
                                      m_config.tool_directory.empty() ? std::string()
                                                                      : m_execution_cache.argv0,
                                      on_browse_tool_dir))
        {
            m_execution_cache_dirty = true;
        }
        ImGui::Separator();

        auto tabs = backend->GetTabs(m_config.tool);
        if (ImGui::BeginTabBar("AdvancedWindowTabs"))
        {
            for (auto const& tab : tabs)
            {
                if (!tab.advanced)
                {
                    continue;
                }
                if (ImGui::BeginTabItem(tab.display_name.c_str()))
                {
                    std::string child_id = "adv_scroll##" + tab.id;
                    PushSlimScrollbarStyle();
                    ImGui::BeginChild(child_id.c_str(), ImVec2(0.0f, 0.0f),
                                      ImGuiChildFlags_None);
                    PopSlimScrollbarStyle();
                    m_execution_cache_dirty |= tab.render_fn();
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }
            }
            if (ImGui::BeginTabItem("Overrides"))
            {
                PushSlimScrollbarStyle();
                ImGui::BeginChild("adv_overrides_scroll", ImVec2(0.0f, 0.0f),
                                  ImGuiChildFlags_None);
                PopSlimScrollbarStyle();
                RenderArgsEnvPanel();
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::PopStyleVar(2);  // FramePadding, ItemSpacing
    }
    ImGui::End();
    ImGui::PopStyleVar(2);  // WindowPadding, WindowRounding

    if (!open)
    {
        m_show_advanced_window = false;
    }
}

void ProfilerLauncherDialog::RenderArgsEnvPanel()
{
    SettingsManager& settings   = SettingsManager::Get();
    const ImU32      accent     = settings.GetColor(Colors::kAccent);
    ImGuiStyle&      style      = ImGui::GetStyle();

    auto trim = [](std::string s) -> std::string
    {
        size_t a = s.find_first_not_of(" \t");
        size_t b = s.find_last_not_of(" \t");
        return (a == std::string::npos) ? std::string() : s.substr(a, b - a + 1);
    };

    // Wrap-aware pill row: SameLine only while the next pill still fits.
    auto keep_on_row = [&](float next_label_w)
    {
        float window_x2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
        float last_x2   = ImGui::GetItemRectMax().x;
        return (last_x2 + style.ItemSpacing.x + next_label_w + 24.0f) < window_x2;
    };

    // ===== Command line arguments (lead - they read as a one-liner) =====
    LaunchSubHeader("ARGUMENTS",
                    "Passed to the profiler, one entry at a time (a flag, or a flag + value).");

    bool add_arg = false;
    ImGui::SetNextItemWidth(-90.0f);
    if (InputTextStringWithHint("##ArgInput", "e.g.  --sampling-freq 500",
                                m_arg_input, ImGuiInputTextFlags_EnterReturnsTrue))
    {
        add_arg = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Add##Arg", ImVec2(80.0f, 0.0f)))
    {
        add_arg = true;
    }
    if (add_arg)
    {
        std::string value = trim(m_arg_input);
        if (!value.empty())
        {
            m_config.extra_argv.push_back(value);
            m_arg_input.clear();
            m_execution_cache_dirty = true;
        }
    }

    if (!m_config.extra_argv.empty())
    {
        ImGui::Spacing();
        int edit_idx   = -1;
        int remove_idx = -1;
        for (size_t i = 0; i < m_config.extra_argv.size(); i++)
        {
            ImGui::PushID(static_cast<int>(i));
            PillAction act = EditablePill(m_config.extra_argv[i].c_str(), accent);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Click to edit  -  x to remove");
            }
            ImGui::PopID();

            if (act == PillAction::kEdit)   { edit_idx = static_cast<int>(i); }
            if (act == PillAction::kRemove) { remove_idx = static_cast<int>(i); }

            if (i + 1 < m_config.extra_argv.size() &&
                keep_on_row(ImGui::CalcTextSize(m_config.extra_argv[i + 1].c_str()).x))
            {
                ImGui::SameLine();
            }
        }
        if (edit_idx >= 0)
        {
            // Pick it up into the edit box; modify + Add to re-add.
            m_arg_input = m_config.extra_argv[static_cast<size_t>(edit_idx)];
            m_config.extra_argv.erase(m_config.extra_argv.begin() + edit_idx);
            m_execution_cache_dirty = true;
        }
        else if (remove_idx >= 0)
        {
            m_config.extra_argv.erase(m_config.extra_argv.begin() + remove_idx);
            m_execution_cache_dirty = true;
        }
    }

    ImGui::Spacing();

    // ===== Environment variables (name = value, grow vertically) =====
    LaunchSubHeader("ENVIRONMENT VARIABLES",
                    "Extra environment variables set for the profiler process.");

    bool add_env = false;
    ImGui::SetNextItemWidth(200.0f);
    InputTextStringWithHint("##EnvName", "NAME", m_env_name_input);
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("=");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-90.0f);
    if (InputTextStringWithHint("##EnvValue", "value", m_env_value_input,
                                ImGuiInputTextFlags_EnterReturnsTrue))
    {
        add_env = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Add##Env", ImVec2(80.0f, 0.0f)))
    {
        add_env = true;
    }
    if (add_env)
    {
        std::string name = trim(m_env_name_input);
        if (!name.empty())
        {
            m_config.extra_env[name] = m_env_value_input;
            m_env_name_input.clear();
            m_env_value_input.clear();
            m_execution_cache_dirty = true;
        }
    }

    if (!m_config.extra_env.empty())
    {
        ImGui::Spacing();
        std::vector<std::pair<std::string, std::string>> envs(
            m_config.extra_env.begin(), m_config.extra_env.end());
        std::string edit_key;
        std::string remove_key;
        for (size_t i = 0; i < envs.size(); i++)
        {
            std::string pill = envs[i].first + " = " + envs[i].second;
            ImGui::PushID(envs[i].first.c_str());
            PillAction act = EditablePill(pill.c_str(), accent);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Click to edit  -  x to remove");
            }
            ImGui::PopID();

            if (act == PillAction::kEdit)   { edit_key = envs[i].first; }
            if (act == PillAction::kRemove) { remove_key = envs[i].first; }

            if (i + 1 < envs.size())
            {
                std::string next = envs[i + 1].first + " = " + envs[i + 1].second;
                if (keep_on_row(ImGui::CalcTextSize(next.c_str()).x))
                {
                    ImGui::SameLine();
                }
            }
        }
        if (!edit_key.empty())
        {
            auto it = m_config.extra_env.find(edit_key);
            if (it != m_config.extra_env.end())
            {
                m_env_name_input        = it->first;
                m_env_value_input       = it->second;
                m_config.extra_env.erase(it);
                m_execution_cache_dirty = true;
            }
        }
        else if (!remove_key.empty())
        {
            m_config.extra_env.erase(remove_key);
            m_execution_cache_dirty = true;
        }
    }
}

#ifdef ROCPROFVIS_ENABLE_REMOTE
// TEMPORARY (remote/SSH): the SSH connection selector, settings dialog, auth
// modal, and download-progress popup. Remove this guard when remote graduates.
void ProfilerLauncherDialog::RenderRemoteSection()
{
    // Local vs. remote (SSH) execution selector. Kept here (rather than in the
    // shared RenderTargetSection) so it sits alongside the SSH connection
    // options, which need dialog-owned state (RemoteUri / SshSettingsDialog /
    // RemoteProfilerSession) to render. Laid out as design-language label/value
    // table rows so it matches the remote trace dialog.
    SettingsManager&    settings    = SettingsManager::GetInstance();
    constexpr float     LABEL_WIDTH = 96.0f;

    if (!ImGui::BeginTable("##profiler_run_on", 2, ImGuiTableFlags_SizingStretchProp))
    {
        return;
    }
    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, LABEL_WIDTH);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    PanelFieldLabel("Run on", true, &settings);

    // A two-button segmented control reads more clearly than a dropdown for a
    // binary choice.
    ImGui::TableSetColumnIndex(1);
    bool is_local = (m_config.connection == ConnectionType::kLocal);

    if (ImGui::RadioButton("This machine", is_local))
    {
        m_config.connection     = ConnectionType::kLocal;
        m_execution_cache_dirty = true;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Remote (SSH)", !is_local))
    {
        m_config.connection     = ConnectionType::kSsh;
        m_execution_cache_dirty = true;
    }

    if (IsSshMode())
    {
        // Connection chip: accent when configured, dim "Configure" when not -
        // clicking it opens the shared SSH settings dialog (matches the remote
        // trace dialog's header connection button).
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        PanelFieldLabel("Connection", true, &settings);

        ImGui::TableSetColumnIndex(1);
        const std::string host = m_remote_uri->GetRemoteHostString();
        // Prefer the connection's user-chosen name; fall back to user@host:port.
        const std::string host_chip =
            host.empty() ? std::string("Configure")
                         : m_remote_uri->GetConnection().DisplayLabel();

        ImGui::PushStyleColor(ImGuiCol_Button, settings.GetColor(Colors::kButton));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              settings.GetColor(Colors::kButtonHovered));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              settings.GetColor(Colors::kButtonActive));
        ImGui::PushStyleColor(ImGuiCol_Text, host.empty()
                                                 ? settings.GetColor(Colors::kTextDim)
                                                 : settings.GetColor(Colors::kAccent));
        if (ImGui::Button((host_chip + "##profiler_connection").c_str(),
                          ImVec2(-FLT_MIN, 0.0f)))
        {
            m_ssh_settings_dialog = std::make_unique<SshSettingsDialog>(
                m_connection_store, m_selected_connection_id,
                [this](const std::string& id)
                {
                    m_selected_connection_id = id;
                    ApplySelectedConnection();
                });
        }
        ImGui::PopStyleColor(4);
    }

    ImGui::EndTable();
}

void ProfilerLauncherDialog::RenderRemotePopups()
{
    // Render the transient SSH settings dialog; destroy once it reports closed.
    if (m_ssh_settings_dialog)
    {
        if (!m_ssh_settings_dialog->Render())
        {
            m_ssh_settings_dialog.reset();
        }
    }

    // Remote file/directory picker for the Target Browse buttons. Owns its own
    // SSH session for listing, so it is independent of the launch orchestrator.
    if (m_remote_file_browser)
    {
        m_remote_file_browser->Render();
    }

    // Auth prompts / host-key requests are rendered centrally for every live
    // session by AppWindow (RenderSshAuthModals); this dialog only owns the
    // download-progress popup below.
    SshSession* ssh_session = m_orchestrator.GetRemoteSshSession();

    // Open the download-progress popup once the workflow enters the download
    // phase. Whether/when individual FileStat progress snapshots arrive is
    // unreliable for small/fast transfers, so the popup is driven by the
    // session's phase, not by the progress reaching downloaded==size.
    bool downloading = m_orchestrator.IsRemoteDownloading();
    if (downloading && !m_remote_show_progress_popup)
    {
        m_remote_show_progress_popup = true;
        m_remote_last_progress = FileStat::Snapshot{};
        ImGui::OpenPopup("Remote Trace Download");
    }

    // Pull the latest progress snapshot when available.
    if (ssh_session)
    {
        if (auto fetch = ssh_session->GetFileStat()->ConsumeIfUpdated())
        {
            m_remote_last_progress = *fetch;
        }
    }

    // Closing on the download phase ending (rather than on downloaded == size)
    // avoids hanging open when the final progress snapshot never arrives.
    RenderRemoteDownloadPopup("Remote Trace Download", m_remote_last_progress.name.c_str(),
                              m_remote_last_progress.downloaded, m_remote_last_progress.size,
                              !downloading, m_remote_show_progress_popup);
}
#endif  // ROCPROFVIS_ENABLE_REMOTE

void ProfilerLauncherDialog::RenderButtonRow()
{
    ImGui::Separator();

    bool is_running = m_orchestrator.IsRunning();
    rocprofvis_profiler_state_t state = m_orchestrator.GetState();
    bool state_ready = !is_running &&
        (state == kRPVProfilerStateIdle ||
         state == kRPVProfilerStateCompleted ||
         state == kRPVProfilerStateFailed ||
         state == kRPVProfilerStateCancelled);

    // Live readiness: backend validation plus (for remote) a configured host.
    IProfilerBackend* backend = m_backends[m_backend_index].get();
    std::string       readiness = backend->Validate(m_config);
#ifdef ROCPROFVIS_ENABLE_REMOTE
    if (readiness.empty() && IsSshMode() &&
        (m_remote_uri->GetRemoteHostString().empty() ||
         m_remote_uri->GetRemoteUserString().empty()))
    {
        readiness = "Configure the SSH connection (host/user) to launch";
    }
#endif
    bool valid      = readiness.empty();
    bool can_launch = state_ready && valid;

    // If a run is in flight or a previous run's output is available, let the
    // user jump straight to the focused run view.
    bool has_run_view = is_running ||
        state == kRPVProfilerStateRunning ||
        state == kRPVProfilerStateCompleted ||
        state == kRPVProfilerStateFailed ||
        state == kRPVProfilerStateCancelled;
    const bool show_view_run = has_run_view && !m_output_text.empty();

    // Readiness on the left; actions on the right with the primary last, the
    // same arrangement as the run view and the app's other dialogs.
    const float spacing    = ImGui::GetStyle().ItemSpacing.x;
    const char* view_label = is_running ? "View Run" : "View Last Run";
    float       actions_w  = FOOTER_BUTTON_WIDTH + spacing + FOOTER_PRIMARY_WIDTH;
    if (show_view_run)
    {
        actions_w += FooterButtonWidth(view_label) + spacing;
    }
    const float actions_x = FooterActionsX(actions_w);

    if (!is_running)
    {
        SettingsManager& settings = SettingsManager::Get();
        ImGui::AlignTextToFramePadding();
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(valid ? Colors::kTextSuccess
                                                                     : Colors::kTextWarning));
        ElidedText(valid ? "Ready to launch" : readiness.c_str(),
                   std::max(actions_x - ImGui::GetCursorPosX() - spacing, 80.0f),
                   360.0f, Alignment_Left, true);
        ImGui::PopStyleColor();
        ImGui::SameLine();
    }
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), actions_x));

    if (show_view_run)
    {
        if (ImGui::Button(view_label, ImVec2(FooterButtonWidth(view_label), 0.0f)))
        {
            m_show_run_view = true;
        }
        ImGui::SameLine();
    }

    if (ImGui::Button("Close", ImVec2(FOOTER_BUTTON_WIDTH, 0.0f)))
    {
        Hide();
    }
    ImGui::SameLine();

    if (!can_launch)
    {
        ImGui::BeginDisabled();
    }
    if (AccentButton("Launch Profiler", ImVec2(FOOTER_PRIMARY_WIDTH, 0.0f)))
    {
        OnLaunchClicked();
    }
    if (!can_launch)
    {
        ImGui::EndDisabled();
    }
}

void ProfilerLauncherDialog::RenderRunButtonRow()
{
    ImGui::Separator();

    constexpr const char* OPEN_TRACE_LABEL = "Open Trace";
    constexpr const char* BACK_LABEL       = "Back to Configuration";

    const bool        is_running = m_orchestrator.IsRunning();
    const std::string trace_path = m_orchestrator.GetTracePath();
    // A completed local run can be opened by hand; remote runs load the
    // downloaded trace through the orchestrator.
    const bool can_open_trace = !is_running && !trace_path.empty() &&
                                m_orchestrator.GetState() == kRPVProfilerStateCompleted &&
                                !m_orchestrator.IsRemote();

    // The configure view's arrangement: actions at the right, primary last.
    const float spacing   = ImGui::GetStyle().ItemSpacing.x;
    float       actions_w = FOOTER_BUTTON_WIDTH + spacing + FOOTER_PRIMARY_WIDTH;
    if (can_open_trace)
    {
        actions_w += FooterButtonWidth(OPEN_TRACE_LABEL) + spacing;
    }
    if (!is_running)
    {
        actions_w += FooterButtonWidth(BACK_LABEL) + spacing;
    }
    ImGui::SetCursorPosX(FooterActionsX(actions_w));

    if (can_open_trace)
    {
        if (ImGui::Button(OPEN_TRACE_LABEL, ImVec2(FooterButtonWidth(OPEN_TRACE_LABEL), 0.0f)) &&
            m_app_window)
        {
            m_app_window->OpenFile(trace_path);
        }
        ImGui::SameLine();
    }
    if (!is_running)
    {
        if (ImGui::Button(BACK_LABEL, ImVec2(FooterButtonWidth(BACK_LABEL), 0.0f)))
        {
            m_show_run_view = false;
        }
        ImGui::SameLine();
    }
    if (ImGui::Button("Close", ImVec2(FOOTER_BUTTON_WIDTH, 0.0f)))
    {
        Hide();
    }
    ImGui::SameLine();
    if (AccentButton(is_running ? "Cancel" : "Run Again", ImVec2(FOOTER_PRIMARY_WIDTH, 0.0f)))
    {
        if (is_running)
        {
            OnCancelClicked();
        }
        else
        {
            OnLaunchClicked();
        }
    }
}

void ProfilerLauncherDialog::Update()
{
    if (m_show_window)
    {
        // Rebuild the (allocation-heavy) execution cache / command preview only
        // when a control reported an actual change - every widget, including the
        // backend-owned settings tabs, ORs its ImGui return value into the dirty
        // flag. The target can also change outside a widget's frame (a Browse
        // dialog's asynchronous pick), so it is compared with the previewed one.
        if (m_execution_cache_dirty || !SameCommandTarget(m_config.target, m_previewed_target))
        {
            RefreshExecutionCache();
            m_previewed_target      = m_config.target;
            m_execution_cache_dirty = false;
        }
    }

    // Pump the run engine, then reflect its normalized state into the console.
    m_orchestrator.Update();

    // Re-strip / recompose only when new profiler output arrived.
    if (m_orchestrator.ConsumeOutputDirty())
    {
        m_process_output_raw = m_orchestrator.GetRawOutput();
        m_process_output_stripped = strip_ansi_for_display(m_process_output_raw);
        RebuildComposedOutput();
    }

    // Append epilogue text once per run-state transition (the orchestrator owns
    // the state; the dialog owns the user-facing log composition).
    rocprofvis_profiler_state_t state = m_orchestrator.GetState();
    if (state != m_last_seen_state)
    {
        HandleStateTransition(state);
        m_last_seen_state = state;
    }
}

void ProfilerLauncherDialog::SyncToolWithBackend()
{
    if (m_backends.empty())
    {
        m_config.tool = kRPVProfilerToolNone;
        return;
    }

    std::vector<ToolOption> tools = m_backends[m_backend_index]->GetTools();
    if (tools.empty())
    {
        m_config.tool = kRPVProfilerToolNone;
        return;
    }

    for (auto const& option : tools)
    {
        if (option.tool == m_config.tool)
        {
            return;
        }
    }

    // Reached for a fresh config, and for a profile saved against a different
    // profiler or by a build that knew a tool this one does not. Reported rather
    // than silently substituted: the user asked for a specific tool.
    if (m_config.tool != kRPVProfilerToolNone)
    {
        spdlog::warn("Launch profile names a tool that {} does not offer; using {} instead",
                     m_backends[m_backend_index]->Id(), tools[0].display_name);
    }
    m_config.tool = tools[0].tool;
}

std::string ProfilerLauncherDialog::CurrentToolDisplayName() const
{
    if (m_backends.empty())
    {
        return std::string();
    }
    for (auto const& option : m_backends[m_backend_index]->GetTools())
    {
        if (option.tool == m_config.tool)
        {
            return option.display_name;
        }
    }
    return std::string();
}

void ProfilerLauncherDialog::OnLaunchClicked()
{
    IProfilerBackend* backend = m_backends[m_backend_index].get();

    // Sync typed settings to payload before launch/validation
    m_config.backend_payload = backend->SaveSettings();

    // Validate
    std::string err = backend->Validate(m_config);
    if (!err.empty())
    {
        m_error_message = "Error: " + err;
        return;
    }

    const bool is_remote = IsSshMode();

#ifndef ROCPROFVIS_ENABLE_REMOTE
    // A config/preset can carry connection == kSsh even in a build without
    // remote support (the two feature flags are independent). Don't silently run
    // it locally - block with a clear reason.
    if (m_config.connection == ConnectionType::kSsh)
    {
        m_error_message = "This configuration targets a remote (SSH) host, but this build "
                          "was compiled without remote support.";
        return;
    }
#endif

#ifdef ROCPROFVIS_ENABLE_REMOTE
    // Remote precheck: bind the selected connection and require host/user.
    if (is_remote)
    {
        ApplySelectedConnection();
        if (m_remote_uri->GetRemoteHostString().empty() ||
            m_remote_uri->GetRemoteUserString().empty())
        {
            m_error_message = "Configure the SSH connection (host/user) before launching.";
            return;
        }
    }
#endif

    // Clear previous state
    m_error_message.clear();
    m_output_preamble.clear();
    m_output_epilogue.clear();
    m_process_output_raw.clear();
    m_process_output_stripped.clear();
    m_output_text.clear();

    // Re-resolve rather than reuse the memo: the user may have installed ROCm or
    // corrected the tool directory since it was taken, and a launch is the one
    // place where an out-of-date answer would be acted on rather than displayed.
    RefreshToolPath(true);
    RefreshExecutionCache();

    // A tool that cannot be found is reported here rather than left to fail
    // mid-launch. resolve_error is only ever set for a local run, since remote
    // resolution happens on the other host (RefreshExecutionCache).
    if (!m_execution_cache.resolve_error.empty())
    {
        m_error_message = "Error: " + m_execution_cache.resolve_error;
        return;
    }
    if (m_execution_cache.tool == kRPVProfilerToolNone)
    {
        m_error_message = "Error: no profiler tool selected";
        return;
    }

    // Assemble the run request from the config / execution cache, then hand off
    // to the orchestrator. The backend scraper resolves the produced trace path
    // from the profiler's stdout (local and remote).
    ProfilerLaunchOrchestrator::LaunchRequest request;
    request.spec.tool              = m_execution_cache.tool;
    request.spec.tool_directory    = m_config.tool_directory;
    request.spec.output_directory  = m_config.target.output_directory;
    request.spec.working_directory = m_config.target.working_directory;
    request.spec.profiler_argv     = m_execution_cache.argv;
    request.spec.env_vars          = m_execution_cache.env_vars;
    request.is_remote              = is_remote;
    request.auto_load_trace        = m_config.target.auto_load_trace;
#ifdef ROCPROFVIS_ENABLE_REMOTE
    request.remote_uri        = is_remote ? m_remote_uri : nullptr;
#endif
    request.parse_trace       = [backend](const std::string& profiler_stdout) -> std::string
    {
        return backend ? backend->ParseTraceOutputPath(profiler_stdout) : std::string();
    };

#ifdef ROCPROFVIS_ENABLE_REMOTE
    m_remote_show_progress_popup = false;
#endif
    m_last_seen_state            = kRPVProfilerStateIdle;

    bool success = m_orchestrator.Launch(request);
    if (success)
    {
        // Build the command-preview preamble for the console.
        std::ostringstream preamble;
        preamble << m_execution_cache.command_preview << "\n\n";
        m_output_preamble = preamble.str();
        m_last_seen_state = kRPVProfilerStateRunning;
        RebuildComposedOutput();

        // Swap to the focused run view so the live output owns the window.
        m_show_run_view        = true;
        m_show_advanced_window = false;
        m_run_start_time       = ImGui::GetTime();
        m_run_end_time         = 0.0;

        SaveToSettings();
        AddRecentTarget(m_config.target.executable);
    }
    else
    {
        m_error_message = m_orchestrator.GetLaunchError();
    }
}

void ProfilerLauncherDialog::OnCancelClicked()
{
    // Cancelling sets the orchestrator state to Cancelled; the epilogue line is
    // appended on the next Update() transition (HandleStateTransition).
    m_orchestrator.Cancel();
}

void ProfilerLauncherDialog::Hide()
{
    // Do not Close() the orchestrator and do not reset m_last_seen_state: the
    // run (and its console) must still be there when the window comes back, and
    // resetting the seen state would replay the completion epilogue.
    m_show_window          = false;
    m_show_run_view        = false;
    m_show_advanced_window = false;
}

void ProfilerLauncherDialog::HandleStateTransition(rocprofvis_profiler_state_t new_state)
{
    const bool is_remote = m_orchestrator.IsRemote();

    // Freeze the elapsed-time readout when the run reaches a terminal state.
    if (new_state == kRPVProfilerStateCompleted ||
        new_state == kRPVProfilerStateFailed ||
        new_state == kRPVProfilerStateCancelled)
    {
        m_run_end_time = ImGui::GetTime();
    }

    if (new_state == kRPVProfilerStateCompleted)
    {
        m_output_epilogue += "\nProfiling finished.\n";
        std::string trace_path = m_orchestrator.GetTracePath();
        if (!trace_path.empty())
        {
            m_output_epilogue += "Trace file: " + trace_path + "\n";
        }
        RebuildComposedOutput();
    }
    else if (new_state == kRPVProfilerStateFailed)
    {
        if (is_remote)
        {
            // Surface the specific remote failure reason (SSH connect/auth
            // failure, "could not determine remote trace path", etc.) instead of
            // a generic line with no cause.
            std::string remote_msg = m_orchestrator.GetRemoteStatusMessage();
            m_error_message = remote_msg.empty() ? std::string("Remote profiler failed.")
                                                 : remote_msg;
        }
        else
        {
            constexpr int32_t COMMAND_NOT_FOUND_EXIT_CODE = 127;
            int32_t           exit_code = m_orchestrator.GetExitCode();
            m_error_message =
                exit_code == COMMAND_NOT_FOUND_EXIT_CODE
                    ? std::string("The profiler could not be started (exit code 127).")
                    : "The profiler exited with code " + std::to_string(exit_code) + ".";
        }
        m_output_epilogue += "\nProfiling failed: " + m_error_message + "\n";
        RebuildComposedOutput();
    }
    else if (new_state == kRPVProfilerStateCancelled)
    {
        m_output_epilogue += "\nProfiling cancelled.\n";
        RebuildComposedOutput();
    }

    // Closing the launcher only hid the window; the run kept going. Tell the
    // user it settled, because they are not looking at the console.
    if (!m_show_window)
    {
        if (new_state == kRPVProfilerStateCompleted)
        {
            std::string trace_path = m_orchestrator.GetTracePath();
            NotificationManager::GetInstance().Show(
                trace_path.empty() ? "Profiler finished"
                                   : ("Profiler finished: " + trace_path),
                NotificationLevel::Success);
        }
        else if (new_state == kRPVProfilerStateFailed)
        {
            NotificationManager::GetInstance().Show(
                m_error_message.empty() ? "Profiler failed" : m_error_message,
                NotificationLevel::Error);
        }
        else if (new_state == kRPVProfilerStateCancelled)
        {
            NotificationManager::GetInstance().Show("Profiler cancelled",
                                                    NotificationLevel::Info);
        }
    }
}

void ProfilerLauncherDialog::RebuildComposedOutput()
{
    m_output_text = m_output_preamble + m_process_output_stripped + m_output_epilogue;
}

void ProfilerLauncherDialog::ComputeConsoleStatus(std::string&        out_label,
                                                  ConsoleStatusLevel& out_level,
                                                  std::string&        out_detail) const
{
    out_label  = "Idle";
    out_level  = ConsoleStatusLevel::kIdle;
    out_detail.clear();

#ifdef ROCPROFVIS_ENABLE_REMOTE
    // Remote: the badge reflects the workflow phase (connect/auth/profile/
    // download), provided by the orchestrator. Returns false when not remote.
    if (m_orchestrator.GetRemotePhaseBadge(out_label, out_level, out_detail))
    {
        return;
    }
#endif

    // Local: map the profiler process state directly.
    switch (m_orchestrator.GetState())
    {
        case kRPVProfilerStateRunning:
            out_label = "Running";
            out_level = ConsoleStatusLevel::kRunning;
            break;
        case kRPVProfilerStateCompleted:
            out_label = "Completed";
            out_level = ConsoleStatusLevel::kSuccess;
            break;
        case kRPVProfilerStateFailed:
            out_label  = "Failed";
            out_level  = ConsoleStatusLevel::kError;
            out_detail = m_error_message;
            break;
        case kRPVProfilerStateCancelled:
            out_label = "Cancelled";
            out_level = ConsoleStatusLevel::kIdle;
            break;
        default:
            out_label = "Idle";
            out_level = ConsoleStatusLevel::kIdle;
            break;
    }
}

void ProfilerLauncherDialog::RefreshToolPath(bool force)
{
    bool const ssh = IsSshMode();
    if (!force && m_tool_path.populated && m_tool_path.tool == m_config.tool &&
        m_tool_path.ssh == ssh && m_tool_path.directory == m_config.tool_directory)
    {
        return;
    }

    m_tool_path.tool      = m_config.tool;
    m_tool_path.directory = m_config.tool_directory;
    m_tool_path.ssh       = ssh;
    m_tool_path.populated = true;
    m_tool_path.error.clear();

    std::string const& tool_directory = m_config.tool_directory;
    if (ssh)
    {
        // The tool is on the remote host, so this machine's filesystem says
        // nothing about it - resolving here would report a local install (or its
        // absence) for a command that runs elsewhere. Show exactly what the
        // remote will run, which is what ProfilerConfig::ResolveToolPathRemote
        // composes: <tool_directory>/<name>, or the bare name for the remote
        // $PATH. Joined with '/' because the remote is addressed as POSIX.
        std::string const name = GetToolBinaryName(m_config.tool);
        m_tool_path.argv0 = tool_directory.empty()
                                ? name
                                : (tool_directory.back() == '/' ? tool_directory + name
                                                                : tool_directory + "/" + name);
    }
    else
    {
        m_tool_path.argv0 =
            ResolveToolPath(m_config.tool, tool_directory, m_tool_path.error);
        if (m_tool_path.argv0.empty())
        {
            // Keep the preview a command line rather than "<not found>"; the
            // notice above it carries the reason.
            m_tool_path.argv0 = GetToolBinaryName(m_config.tool);
        }
    }
}

void ProfilerLauncherDialog::RefreshExecutionCache()
{
    if (m_backends.empty())
    {
        m_execution_cache = ExecutionCache();
        return;
    }

    IProfilerBackend* backend = m_backends[m_backend_index].get();
    m_config.backend_payload = backend->SaveSettings();

    ExecutionCache cache;
    cache.tool = m_config.tool;

    RefreshToolPath(false);
    cache.argv0         = m_tool_path.argv0;
    cache.resolve_error = m_tool_path.error;

    backend->FlattenToExecution(m_config, cache.curated_env_vars, cache.argv);

    cache.env_vars = cache.curated_env_vars;
    for (auto const& kv : m_config.extra_env)
    {
        cache.env_vars.emplace_back(kv.first, kv.second);
    }

    cache.command_preview =
        BuildCommandPreviewString(cache.argv0, cache.env_vars, cache.argv);

    m_execution_cache = std::move(cache);
}

void ProfilerLauncherDialog::SwitchBackend(int index)
{
    if (index < 0 || index >= static_cast<int>(m_backends.size()))
    {
        return;
    }
    m_backend_index = index;
    m_config.profiler_id = m_backends[index]->Id();
    // Not carried across: a tool belongs to one profiler, so the new backend's
    // default is the only sensible selection.
    m_config.tool = kRPVProfilerToolNone;
    SyncToolWithBackend();
    m_backends[index]->LoadSettings(jt::Json());
    m_config.backend_payload = m_backends[index]->SaveSettings();
    m_execution_cache_dirty = true;
}

void ProfilerLauncherDialog::LoadFromSettings()
{
    SettingsManager& settings = SettingsManager::Get();
    ProfilerSettings& ps = settings.GetProfilerSettings();

    m_config.target.output_directory = ps.profiler_output_directory;
    m_config.target.auto_load_trace = ps.auto_load_trace;
    m_current_preset_name = ps.last_preset_name;
#ifdef ROCPROFVIS_ENABLE_REMOTE
    m_selected_connection_id = ps.last_ssh_connection_id;
#endif

    if (!ps.last_profiler_id.empty())
    {
        for (size_t i = 0; i < m_backends.size(); i++)
        {
            if (m_backends[i]->Id() == ps.last_profiler_id)
            {
                m_backend_index = static_cast<int>(i);
                m_config.profiler_id = ps.last_profiler_id;
                break;
            }
        }
    }

    // Rehydrate the remembered profile's contents, not just its name (mirrors
    // the combo-select load path in RenderToolbar()).
    if (!m_current_preset_name.empty())
    {
        LaunchConfig loaded;
        if (m_preset_manager.LoadPreset(m_current_preset_name,
                                        m_config.profiler_id, loaded))
        {
            m_config = loaded;
            m_execution_cache_dirty = true;
            m_backends[m_backend_index]->LoadSettings(m_config.backend_payload);
            SyncToolWithBackend();
#ifdef ROCPROFVIS_ENABLE_REMOTE
            // A ref to a since-deleted connection is left unbound rather than
            // remapped onto whichever connection is selected.
            if (!m_config.ssh_connection_ref.empty() &&
                m_connection_store.Get(m_config.ssh_connection_ref) != nullptr)
            {
                m_selected_connection_id = m_config.ssh_connection_ref;
            }
#endif
        }
        else
        {
            m_current_preset_name.clear();  // profile was renamed/deleted
        }
    }
}

void ProfilerLauncherDialog::SaveToSettings()
{
    SettingsManager& settings = SettingsManager::Get();
    ProfilerSettings& ps = settings.GetProfilerSettings();

    ps.profiler_output_directory = m_config.target.output_directory;
    ps.auto_load_trace = m_config.target.auto_load_trace;
    ps.last_preset_name = m_current_preset_name;
    ps.last_profiler_id = m_config.profiler_id;
#ifdef ROCPROFVIS_ENABLE_REMOTE
    ps.last_ssh_connection_id = m_selected_connection_id;
#endif
    settings.SaveProfilerSettings();
}

#ifdef ROCPROFVIS_ENABLE_REMOTE
void ProfilerLauncherDialog::SyncSelectedConnection()
{
    if(m_connection_store.Get(m_selected_connection_id) == nullptr && !m_connection_store.Empty())
    {
        m_selected_connection_id = m_connection_store.List().front().id;
    }
    ApplySelectedConnection();
}

void ProfilerLauncherDialog::ApplySelectedConnection()
{
    const SshConnectionConfig* cfg = m_connection_store.Get(m_selected_connection_id);
    if(cfg)
    {
        m_remote_uri->SetConnection(*cfg);
    }
    else
    {
        m_remote_uri->SetConnection(SshConnectionConfig());
    }
}

bool ProfilerLauncherDialog::HasRemoteConnection() const
{
    return !m_remote_uri->GetRemoteHostString().empty() &&
           !m_remote_uri->GetRemoteUserString().empty();
}

void ProfilerLauncherDialog::EnsureRemoteFileBrowser()
{
    if (!m_remote_file_browser)
    {
        m_remote_file_browser = std::make_unique<RemoteFileBrowser>(m_remote_uri);
    }
}
#endif  // ROCPROFVIS_ENABLE_REMOTE

void ProfilerLauncherDialog::AddRecentTarget(std::string const& exe)
{
    if (exe.empty())
    {
        return;
    }

    SettingsManager& settings = SettingsManager::Get();
    ProfilerSettings& ps = settings.GetProfilerSettings();

    // Remove existing entry if present
    auto& recents = ps.recent_targets;
    recents.erase(
        std::remove(recents.begin(), recents.end(), exe),
        recents.end());

    // Add to front
    recents.insert(recents.begin(), exe);

    // Cap at 10
    if (recents.size() > 10)
    {
        recents.resize(10);
    }

    settings.SaveProfilerSettings();
}

}  // namespace View
}  // namespace RocProfVis
