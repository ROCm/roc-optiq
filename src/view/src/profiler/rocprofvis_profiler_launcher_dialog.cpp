// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_profiler_launcher_dialog.h"
#include "rocprofvis_appwindow.h"
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
constexpr float kSplitterWidth       = 6.0f;
constexpr float kMinPreviewWidth     = 300.0f;
constexpr float kMinFormWidth        = 320.0f;
constexpr float kInitialPreviewRatio = 2.0f / 5.0f;

// A filled, accent-colored primary button used for the main call-to-action
// (Launch / Cancel) so it stands out from the neutral secondary buttons.
bool AccentButton(const char* label, const ImVec2& size)
{
    SettingsManager& settings = SettingsManager::Get();
    ImGui::PushStyleColor(ImGuiCol_Button, settings.GetColor(Colors::kAccent));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, settings.GetColor(Colors::kAccentHover));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, settings.GetColor(Colors::kAccentActive));
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextOnAccent));
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(4);
    return clicked;
}
}  // namespace

ProfilerLauncherDialog::ProfilerLauncherDialog(AppWindow* app_window)
    : m_app_window(app_window)
    , m_orchestrator(app_window)
#ifdef ROCPROFVIS_ENABLE_REMOTE
    , m_remote_uri(std::make_shared<RemoteUri>())
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
    , m_tool_index(0)
    , m_config()
    , m_profiler_path_override()
    , m_execution_cache()
    , m_preset_manager()
    , m_current_preset_name()
    , m_output_text()
    , m_error_message()
    , m_auto_scroll_output(true)
{
    m_backends.push_back(std::make_unique<RocprofSysBackend>());

    m_config.profiler_id = m_backends[0]->Id();
    m_config.tool_id = m_backends[0]->GetTools()[0].id;
    m_backends[0]->LoadSettings(jt::Json());
    m_config.backend_payload = m_backends[0]->SaveSettings();

    LoadFromSettings();
    RefreshExecutionCache();

#ifdef ROCPROFVIS_ENABLE_REMOTE
    m_connection_store.Load();
    if(m_connection_store.Get(m_selected_connection_id) == nullptr && !m_connection_store.Empty())
    {
        m_selected_connection_id = m_connection_store.List().front().id;
    }
    ApplySelectedConnection();
#endif

    // Run orchestration (sessions, profiler-state events, teardown) is owned by
    // m_orchestrator; the dialog only authors config and renders.
}

ProfilerLauncherDialog::~ProfilerLauncherDialog() = default;

void ProfilerLauncherDialog::Show()
{
    m_should_open = true;
    m_execution_cache_dirty = true;
    // Always reopen on the configuration screen; a prior run (if any) was torn
    // down on close.
    if (!m_orchestrator.IsRunning())
    {
        m_show_run_view = false;
    }
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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, def.WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, def.WindowRounding);

    bool window_open = true;
    bool visible     = ImGui::Begin("Launch Profiler", &window_open,
                                    ImGuiWindowFlags_NoScrollbar);
    if (visible)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, def.ItemSpacing);

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

        ImGui::PopStyleVar(1);
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
        OnCloseClicked();
        m_show_window          = false;
        m_show_run_view        = false;
        m_show_advanced_window = false;
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
    ImGui::Separator();

    // Live "this run" summary: chips that update as options are toggled. Great
    // for a quick read of exactly what will be collected.
    {
        std::vector<std::string> tags;
#ifdef ROCPROFVIS_ENABLE_REMOTE
        tags.push_back(IsSshMode() ? "Remote (SSH)" : "Local");
#else
        tags.push_back("Local");
#endif
        auto tools = backend->GetTools();
        if (m_tool_index >= 0 && m_tool_index < static_cast<int>(tools.size()))
        {
            tags.push_back(tools[m_tool_index].display_name);
        }
        std::vector<std::string> backend_tags = backend->GetSummaryTags(m_config);
        tags.insert(tags.end(), backend_tags.begin(), backend_tags.end());
        RenderConfigChips("This run:", tags);
    }
    ImGui::Spacing();

    // Reserve exactly the height the bottom block (warnings + error + separator
    // + buttons) needs, so it stays pinned to the bottom with no dead space.
    auto warnings = backend->GetWarnings(m_config);

    const ImGuiStyle& style  = ImGui::GetStyle();
    const float       line_h = ImGui::GetTextLineHeightWithSpacing();
    float bottom_reserve = style.ItemSpacing.y            // gap after the form
                         + style.ItemSpacing.y + 1.0f     // separator line + gap to buttons
                         + ImGui::GetFrameHeight();        // button row (no trailing spacing)
    bottom_reserve += warnings.size() * line_h;
    if (!m_error_message.empty())
    {
        float wrap_w = ImGui::GetContentRegionAvail().x;
        bottom_reserve += ImGui::CalcTextSize(m_error_message.c_str(), nullptr, false,
                                              wrap_w).y + style.ItemSpacing.y;
    }

    ImGui::BeginChild("MainPane", ImVec2(0, -bottom_reserve), ImGuiChildFlags_None);
    RenderMainContent();
    ImGui::EndChild();

    // Warnings from backend
    if (!warnings.empty())
    {
        for (auto const& w : warnings)
        {
            ImVec4 color;
            const char* prefix;
            switch (w.level)
            {
                case WarningMessage::kError:
                    color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
                    prefix = "Error: ";
                    break;
                case WarningMessage::kWarning:
                    color = ImVec4(1.0f, 0.8f, 0.0f, 1.0f);
                    prefix = "Warning: ";
                    break;
                default:
                    color = ImVec4(0.4f, 0.7f, 1.0f, 1.0f);
                    prefix = "Hint: ";
                    break;
            }
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
        default:                           pill_color_id = Colors::kBorderGray;  break;
    }
    StatusPill(status_label.c_str(), settings.GetColor(pill_color_id));

    if (m_run_start_time > 0.0)
    {
        double end     = (m_run_end_time > 0.0) ? m_run_end_time : ImGui::GetTime();
        double elapsed = end - m_run_start_time;
        ImGui::SameLine(0.0f, 12.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Elapsed  %.1fs", elapsed);
    }
    if (!status_detail.empty())
    {
        ImGui::SameLine(0.0f, 12.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", status_detail.c_str());
    }

    std::string summary = BuildRunSummary();
    if (!summary.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
        ImGui::TextWrapped("%s", summary.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::Spacing();

    // The console fills everything above the button row.
    float button_h = ImGui::GetFrameHeightWithSpacing() + 16.0f;
    ImGui::BeginChild("RunConsoleArea", ImVec2(0, -button_h), false);
    if (RenderOutputConsole(m_output_text, m_error_message,
                            status_label, status_level, status_detail,
                            m_auto_scroll_output))
    {
        m_output_text.clear();
        m_output_preamble.clear();
        m_output_epilogue.clear();
        m_process_output_raw.clear();
        m_process_output_stripped.clear();
        m_error_message.clear();
    }
    ImGui::EndChild();

    RenderRunButtonRow();
}

std::string ProfilerLauncherDialog::BuildRunSummary() const
{
    std::ostringstream ss;

    if (m_backend_index >= 0 && m_backend_index < static_cast<int>(m_backends.size()))
    {
        ss << m_backends[m_backend_index]->DisplayName();
        auto tools = m_backends[m_backend_index]->GetTools();
        if (m_tool_index >= 0 && m_tool_index < static_cast<int>(tools.size()))
        {
            ss << " (" << tools[m_tool_index].display_name << ")";
        }
    }

    if (!m_config.target.executable.empty())
    {
        ss << "  ->  " << m_config.target.executable;
        if (!m_config.target.arguments.empty())
        {
            ss << " " << m_config.target.arguments;
        }
    }

#ifdef ROCPROFVIS_ENABLE_REMOTE
    if (IsSshMode())
    {
        std::string host = m_remote_uri->GetRemoteHostString();
        std::string user = m_remote_uri->GetRemoteUserString();
        ss << "   [Remote: " << (user.empty() ? "?" : user.c_str()) << "@"
           << (host.empty() ? "?" : host.c_str()) << "]";
    }
    else
    {
        ss << "   [Local]";
    }
#else
    ss << "   [Local]";
#endif

    return ss.str();
}

void ProfilerLauncherDialog::RenderToolbar()
{
    // Profiler selector
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Profiler:");
    ImGui::SameLine();
    ImGui::PushItemWidth(140);
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

    VerticalSeparator();

    // Tool selector
    IProfilerBackend const* backend = m_backends[m_backend_index].get();
    auto tools = backend->GetTools();
    ImGui::Text("Tool:");
    ImGui::SameLine();
    ImGui::PushItemWidth(120);
    if (ImGui::BeginCombo("##ToolSelector",
                          tools[m_tool_index].display_name.c_str()))
    {
        for (size_t i = 0; i < tools.size(); i++)
        {
            bool selected = (static_cast<int>(i) == m_tool_index);
            if (ImGui::Selectable(tools[i].display_name.c_str(), selected))
            {
                m_tool_index = static_cast<int>(i);
                m_config.tool_id = tools[i].id;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    VerticalSeparator();

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
            backend = m_backends[m_backend_index].get();
            tools = backend->GetTools();
            for (size_t i = 0; i < tools.size(); i++)
            {
                if (tools[i].id == m_config.tool_id)
                {
                    m_tool_index = static_cast<int>(i);
                    break;
                }
            }
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
    auto tabs = backend->GetTabs(m_config.tool_id);
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
    ImGui::BeginChild("cfg_form", ImVec2(left_w, 0.0f), ImGuiChildFlags_None);
#ifdef ROCPROFVIS_ENABLE_REMOTE
    // Where to run first: pick the machine before the paths.
    BeginLaunchCard("card_connection");
    LaunchCardHeader(ICON_CHAIN, "Where to run");
    RenderRemoteSection();
    EndLaunchCard();
#endif

    BeginLaunchCard("card_target");
    LaunchCardHeader(ICON_OPEN, "Target",
                     "The program to profile and where its results go");
    RenderTargetSection(m_config.target, m_config.connection, m_app_window);
    EndLaunchCard();

    BeginLaunchCard("card_general");
    LaunchCardHeader(ICON_CHART_BAR, "Profiling Options");
    if (general_tabs.size() == 1)
    {
        general_tabs[0]->render_fn();
    }
    else if (!general_tabs.empty())
    {
        if (ImGui::BeginTabBar("GeneralTabs"))
        {
            for (auto const* tab : general_tabs)
            {
                if (ImGui::BeginTabItem(tab->display_name.c_str()))
                {
                    tab->render_fn();
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    }
    EndLaunchCard();

    BeginLaunchCard("card_inputs");
    LaunchCardHeader(ICON_LIST, "Arguments & Environment");
    RenderArgsEnvPanel();
    EndLaunchCard();

    if (!advanced_tabs.empty())
    {
        if (ImGui::Button("Advanced Options...", ImVec2(180, 0)))
        {
            m_show_advanced_window = true;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Sampling, ROCm domains, Perfetto, parallelism, logging");
        }
    }
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
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, settings.GetDefaultStyle().ChildRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, settings.GetDefaultStyle().WindowPadding);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgPanel));
    ImGui::PushStyleColor(ImGuiCol_Border, settings.GetColor(Colors::kPanelBorderSubtle));
    ImGui::BeginChild("cfg_preview", ImVec2(0.0f, 0.0f),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
    RenderCommandPreview(m_execution_cache.command_preview);
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, def.WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, def.WindowRounding);

    bool open    = true;
    bool visible = ImGui::Begin("Advanced Profiling Options", &open);
    if (visible)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, def.ItemSpacing);

        ImGui::TextDisabled("Fine-grained settings, applied on top of the selected preset.");
        ImGui::Spacing();

        auto tabs = backend->GetTabs(m_config.tool_id);
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
                    ImGui::Spacing();
                    ImGui::BeginChild("adv_scroll", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
                    tab.render_fn();
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }

        ImGui::PopStyleVar(1);
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
    LaunchSubHeader("COMMAND LINE ARGUMENTS",
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
    // RemoteProfilerSession) to render.
    const float label_w = 90.0f;

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Run on");
    ImGui::SameLine(label_w);

    // A two-button segmented control reads more clearly than a dropdown for a
    // binary choice.
    bool is_local = (m_config.connection == ConnectionType::kLocal);
    if (ImGui::RadioButton("This machine", is_local))
    {
        m_config.connection = ConnectionType::kLocal;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Remote (SSH)", !is_local))
    {
        m_config.connection = ConnectionType::kSsh;
    }

    if (!IsSshMode())
    {
        return;
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Host");
    ImGui::SameLine(label_w);

    std::string host = m_remote_uri->GetRemoteHostString();
    std::string user = m_remote_uri->GetRemoteUserString();
    if (host.empty())
    {
        ImGui::TextDisabled("no connection configured");
    }
    else
    {
        ImGui::Text("%s@%s:%s", user.empty() ? "?" : user.c_str(), host.c_str(),
                    m_remote_uri->GetRemotePortString().c_str());
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(host.empty() ? "Configure..." : "Change..."))
    {
        m_ssh_settings_dialog = std::make_unique<SshSettingsDialog>(
            m_connection_store, m_selected_connection_id,
            [this](const std::string& id)
            {
                m_selected_connection_id = id;
                ApplySelectedConnection();
            });
    }
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

    SshSession* ssh_session = m_orchestrator.GetRemoteSshSession();

    // Auth prompts / host-key requests.
    RenderSshAuthModal(ssh_session);

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

    // TODO: share this dialog with the SshTestDialog, which has a similar download progress popup.
    if (m_remote_show_progress_popup)
    {
        PopUpStyle popup_style;
        popup_style.PushPopupStyles();
        popup_style.PushTitlebarColors();
        popup_style.CenterPopup();
        ImGui::SetNextWindowSize(ImVec2(440, 0));

        if (ImGui::BeginPopupModal("Remote Trace Download", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoTitleBar))
        {
            const auto& fetch = m_remote_last_progress;
            ImGui::Text("Downloading: %s", fetch.name.c_str());

            uint64_t done  = fetch.downloaded;
            uint64_t total = fetch.size;
            if (total > 0)
            {
                float frac = static_cast<float>(done) / static_cast<float>(total);
                if (frac > 1.0f) { frac = 1.0f; }
                std::string label = std::to_string(done / 1024) + " / " +
                                    std::to_string(total / 1024) + " KiB";
                ImGui::ProgressBar(frac, ImVec2(-FLT_MIN, 0), label.c_str());
            }
            else
            {
                ImGui::Text("Starting...");
            }

            // Close as soon as the download phase ends (completed, failed, or the
            // session was torn down). This avoids hanging open when the final
            // "downloaded == size" snapshot never arrives for fast transfers.
            if (!downloading)
            {
                ImGui::CloseCurrentPopup();
                m_remote_show_progress_popup = false;
            }

            ImGui::EndPopup();
        }
        else
        {
            // Popup not actually open (e.g. dismissed); clear our flag so it can
            // be reopened on the next download.
            m_remote_show_progress_popup = false;
        }
        popup_style.PopStyles();
    }
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

    if (!can_launch) ImGui::BeginDisabled();
    if (AccentButton("Launch Profiler", ImVec2(160, 0)))
    {
        OnLaunchClicked();
    }
    if (!can_launch) ImGui::EndDisabled();

    // If a run is in flight or a previous run's output is available, let the
    // user jump straight to the focused run view.
    bool has_run_view = is_running ||
        state == kRPVProfilerStateRunning ||
        state == kRPVProfilerStateCompleted ||
        state == kRPVProfilerStateFailed ||
        state == kRPVProfilerStateCancelled;
    if (has_run_view && !m_output_text.empty())
    {
        ImGui::SameLine();
        if (ImGui::Button(is_running ? "View Run" : "View Last Run", ImVec2(130, 0)))
        {
            m_show_run_view = true;
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Close", ImVec2(120, 0)))
    {
        OnCloseClicked();
        m_show_window   = false;
        m_show_run_view = false;
    }

    // Readiness feedback to the right of the buttons, so it is obvious why
    // Launch is (or isn't) available.
    if (!is_running)
    {
        SettingsManager& settings = SettingsManager::Get();
        ImGui::SameLine(0.0f, 16.0f);
        ImGui::AlignTextToFramePadding();
        if (valid)
        {
            ImVec4 ok = ImGui::ColorConvertU32ToFloat4(settings.GetColor(Colors::kTextSuccess));
            ImGui::TextColored(ok, "Ready to launch");
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.25f, 1.0f), "%s", readiness.c_str());
        }
    }
}

void ProfilerLauncherDialog::RenderRunButtonRow()
{
    ImGui::Separator();

    bool                        is_running = m_orchestrator.IsRunning();
    rocprofvis_profiler_state_t state      = m_orchestrator.GetState();

    if (is_running)
    {
        if (AccentButton("Cancel", ImVec2(140, 0)))
        {
            OnCancelClicked();
        }
    }
    else
    {
        if (AccentButton("Run Again", ImVec2(140, 0)))
        {
            OnLaunchClicked();
        }

        ImGui::SameLine();
        if (ImGui::Button("Back to Configuration", ImVec2(190, 0)))
        {
            m_show_run_view = false;
        }

        // Offer a manual open for a completed local run (remote runs auto-load
        // the downloaded trace through the orchestrator).
        std::string trace_path = m_orchestrator.GetTracePath();
        if (state == kRPVProfilerStateCompleted && !trace_path.empty() &&
            !m_orchestrator.IsRemote())
        {
            ImGui::SameLine();
            if (ImGui::Button("Open Trace", ImVec2(140, 0)) && m_app_window)
            {
                m_app_window->OpenFile(trace_path);
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Close", ImVec2(120, 0)))
    {
        OnCloseClicked();
        m_show_window   = false;
        m_show_run_view = false;
    }
}

void ProfilerLauncherDialog::Update()
{
    if (m_show_window)
    {
        // Only rebuild the (allocation-heavy) execution cache / command preview
        // when inputs may have changed: an explicit dirty request, or while the
        // user is actively editing a widget (ImGui::IsAnyItemActive reflects the
        // previous frame here, so the deactivation frame is still captured). This
        // covers the backend-owned settings tabs without instrumenting each
        // widget individually.
        if (m_execution_cache_dirty || ImGui::IsAnyItemActive())
        {
            RefreshExecutionCache();
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

rocprofvis_profiler_type_t ProfilerLauncherDialog::ResolveProfilerType() const
{
    if (m_config.tool_id == "instrument")
    {
        return kRPVProfilerTypeRocprofSysInstrument;
    }
    // "run" / "sample" / default
    return kRPVProfilerTypeRocprofSysRun;
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

    RefreshExecutionCache();

    // Assemble the run request from the config / execution cache, then hand off
    // to the orchestrator. The backend scraper resolves the produced trace path
    // from the profiler's stdout (local and remote).
    ProfilerLaunchOrchestrator::LaunchRequest request;
    request.profiler_type     = ResolveProfilerType();
    request.profiler_path     = m_execution_cache.profiler_path;
    request.target_executable = m_config.target.executable;
    request.target_args       = m_config.target.arguments;
    request.output_directory  = m_config.target.output_directory;
    request.profiler_args     = m_execution_cache.profiler_args;
    request.env_vars          = m_execution_cache.env_vars;
    request.is_remote         = is_remote;
    request.auto_load_trace   = m_config.target.auto_load_trace;
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
        if (is_remote)
        {
            preamble << "[remote] ";
        }
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

void ProfilerLauncherDialog::OnCloseClicked()
{
    m_orchestrator.Close();
    m_last_seen_state = kRPVProfilerStateIdle;
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
        if (is_remote)
        {
            // Remote completion here means the remote profiler finished; the
            // trace download/open is driven by the orchestrator's session.
            m_output_epilogue += "\nRemote profiler completed.\n";
        }
        else
        {
            m_output_epilogue += "\nProfiler completed successfully.\n";
        }

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
            m_output_epilogue += "\nRemote profiler failed.\n";
            m_error_message = remote_msg.empty() ? std::string("Remote profiler failed.")
                                                 : remote_msg;
            RebuildComposedOutput();
        }
        else
        {
            int32_t exit_code = m_orchestrator.GetExitCode();
            char exit_msg[128];
            std::snprintf(exit_msg, sizeof(exit_msg),
                          "\nProfiler failed (exit code %d).\n", exit_code);
            m_output_epilogue += exit_msg;
            RebuildComposedOutput();
            if (exit_code == 127)
            {
                m_error_message =
                    "Profiler executable not found or could not be started (exit code 127)";
            }
            else
            {
                std::snprintf(exit_msg, sizeof(exit_msg),
                              "Profiler execution failed (exit code %d)", exit_code);
                m_error_message = exit_msg;
            }
        }
    }
    else if (new_state == kRPVProfilerStateCancelled)
    {
        m_output_epilogue += is_remote ? "\nRemote profiler cancelled by user.\n"
                                       : "\nProfiler cancelled by user.\n";
        RebuildComposedOutput();
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
            out_label = "Failed";
            out_level = ConsoleStatusLevel::kError;
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
    cache.profiler_path = GetProfilerPath();
    backend->FlattenToExecution(m_config, cache.curated_env_vars, cache.argv);

    cache.env_vars = cache.curated_env_vars;
    for (auto const& kv : m_config.extra_env)
    {
        cache.env_vars.emplace_back(kv.first, kv.second);
    }

    for (size_t i = 0; i < cache.argv.size(); i++)
    {
        if (i > 0)
        {
            cache.profiler_args += " ";
        }
        cache.profiler_args += cache.argv[i];
    }

    for (auto const& arg : m_config.extra_argv)
    {
        if (!cache.profiler_args.empty())
        {
            cache.profiler_args += " ";
        }
        cache.profiler_args += arg;
    }

    cache.command_preview = BuildCommandPreviewString(
        m_config, cache.profiler_path, cache.env_vars, cache.argv);

    m_execution_cache = std::move(cache);
}

void ProfilerLauncherDialog::SwitchBackend(int index)
{
    if (index < 0 || index >= static_cast<int>(m_backends.size()))
    {
        return;
    }
    m_backend_index = index;
    m_tool_index = 0;
    m_config.profiler_id = m_backends[index]->Id();
    m_config.tool_id = m_backends[index]->GetTools()[0].id;
    m_backends[index]->LoadSettings(jt::Json());
    m_config.backend_payload = m_backends[index]->SaveSettings();
    m_execution_cache_dirty = true;
}

void ProfilerLauncherDialog::LoadFromSettings()
{
    SettingsManager& settings = SettingsManager::Get();
    ProfilerSettings& ps = settings.GetProfilerSettings();

    m_profiler_path_override = ps.profiler_path;
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
}

void ProfilerLauncherDialog::SaveToSettings()
{
    SettingsManager& settings = SettingsManager::Get();
    ProfilerSettings& ps = settings.GetProfilerSettings();

    ps.profiler_path = m_profiler_path_override;
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

std::string ProfilerLauncherDialog::GetProfilerPath() const
{
    if (!m_profiler_path_override.empty())
    {
        return m_profiler_path_override;
    }
    IProfilerBackend const* backend = m_backends[m_backend_index].get();
    return backend->GetDefaultBinary(m_config.tool_id);
}

}  // namespace View
}  // namespace RocProfVis
