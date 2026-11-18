// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_profiler_launcher_dialog.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "rocprofvis_launch_shared_tabs.h"
#include "rocprofvis_rocprof_compute_backend.h"
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
#include "widgets/rocprofvis_split_containers.h"
#include <algorithm>
#include <cstdio>
#include <sstream>
#include <vector>

namespace RocProfVis
{
namespace View
{

namespace
{

constexpr const char*      SUMMARY_SEPARATOR            = "  \xC2\xB7  ";
constexpr int              CONTAINER_TOOLBAR_ITEM_IDX   = 0;
constexpr int              CONTAINER_CONTENT_ITEM_IDX   = 1;
constexpr int              CONTAINER_MESSAGE_ITEM_IDX   = 2;
constexpr int              CONTAINER_STATUSBAR_ITEM_IDX = 3;
constexpr ImVec2           MIN_SIZE_RELATIVE            = ImVec2(24.0f, 10.0f);
constexpr ImVec2           PREFERRED_SIZE_RELATIVE      = ImVec2(36.0f, 25.0f);
constexpr ImGuiWindowFlags WINDOW_FLAGS =
    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

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
, m_show_tool_locations_window(false)
, m_environment_edit()
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
, m_bring_to_front_on_focus(true)
, m_container(nullptr)
, m_configure_container(nullptr)
, m_settings(SettingsManager::GetInstance())
{
    m_backends.push_back(std::make_unique<RocprofSysBackend>());
    m_backends.push_back(std::make_unique<RocprofComputeBackend>());

    SyncToolWithBackend();
    for(std::unique_ptr<IProfilerBackend>& backend : m_backends)
    {
        backend->LoadSettings(jt::Json());
    }
    m_config.backend_payload = m_backends[m_backend_index]->SaveSettings();

#ifdef ROCPROFVIS_ENABLE_REMOTE
    // Before LoadFromSettings() so it can validate the saved profile's
    // connection ref against the store.
    m_connection_store.Load();
#endif

    LoadFromSettings();
    RefreshExecutionCache();

#ifdef ROCPROFVIS_ENABLE_REMOTE
    if(m_connection_store.Get(m_selected_connection_id) == nullptr && !m_connection_store.Empty())
    {
        m_selected_connection_id = m_connection_store.List().front().id;
    }
    ApplySelectedConnection();
#endif
    ApplyLaunchEnvironment();

    LayoutItem::Ptr config = LayoutItem::CreateFromWidget(
        std::make_shared<RocCustomWidget>([this]() { RenderConfigureOptions(); }));
    config->m_child_flags       = ImGuiChildFlags_None;
    config->m_item_spacing      = m_settings.GetDefaultStyle().ItemSpacing;
    LayoutItem::Ptr cmd_preview = LayoutItem::CreateFromWidget(
        std::make_shared<RocCustomWidget>([this]() { RenderConfigureCMD(); }));
    cmd_preview->m_child_flags  = ImGuiChildFlags_None;
    cmd_preview->m_item_spacing = m_settings.GetDefaultStyle().ItemSpacing;
    m_configure_container       = std::make_unique<HSplitContainer>(config, cmd_preview);
    m_configure_container->SetSplit(0.5f);
    m_configure_container->SetSplitterSize(6.0f);
    m_configure_container->SetSplitSpacing(m_settings.GetDefaultStyle().ItemSpacing.x);

    std::vector<LayoutItem> items(4);
    items[CONTAINER_TOOLBAR_ITEM_IDX].m_item =
        std::make_shared<RocCustomWidget>([this]() { RenderToolbar(); });
    items[CONTAINER_CONTENT_ITEM_IDX].m_item =
        std::make_shared<RocCustomWidget>([this]() { RenderContent(); });
    items[CONTAINER_MESSAGE_ITEM_IDX].m_item =
        std::make_shared<RocCustomWidget>([this]() { RenderMessages(); });
    items[CONTAINER_STATUSBAR_ITEM_IDX].m_item =
        std::make_shared<RocCustomWidget>([this]() { RenderStatusBar(); });
    m_container = std::make_unique<VFixedContainer>(items);
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
    ImGui::SetNextWindowSizeConstraints(ImVec2(24.0f, 10.0f) * ImGui::GetFrameHeight(),
                                        ImVec2(FLT_MAX, FLT_MAX));
    ImVec2 window_size = ImGui::GetWindowSize();
    ImVec2 min_size    = MIN_SIZE_RELATIVE * ImGui::GetFrameHeight();
    ImVec2 pref_size   = PREFERRED_SIZE_RELATIVE * ImGui::GetFrameHeight();
    ImGui::SetNextWindowSize(
        ImVec2(window_size.x < min_size.x
                   ? window_size.x
                   : std::clamp(window_size.x * 0.8f, min_size.x, pref_size.x),
               window_size.y < min_size.y
                   ? window_size.y
                   : std::clamp(window_size.y * 0.8f, min_size.y, pref_size.y)),
        ImGuiCond_FirstUseEver);

    PopUpStyle popup_style;
    popup_style.PushTitlebarColors();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,
                        m_settings.GetDefaultStyle().WindowRounding);

    bool window_open = true;
    if(ImGui::Begin("Launch Profiler", &window_open,
                    m_bring_to_front_on_focus
                        ? WINDOW_FLAGS
                        : WINDOW_FLAGS | ImGuiWindowFlags_NoBringToFrontOnFocus))
    {
        const ImGuiStyle& style         = m_settings.GetDefaultStyle();
        const size_t      message_lines = m_messages.size() + (m_error_message.empty() ? 0 : 1);
        float             message_height = 0.0f;
        if(!m_show_run_view && message_lines > 0)
        {
            message_height =
                ImGui::GetTextLineHeight() * static_cast<float>(message_lines) +
                static_cast<float>(1 + (m_messages.empty() ? 0 : 1) +
                                   (m_error_message.empty() ? 0 : 1)) *
                    2.0f * style.WindowPadding.y +
                static_cast<float>(message_lines - 1) * style.ItemSpacing.y;
        }
        float body_height =
            ImGui::GetContentRegionAvail().y -
            2.0f * (ImGui::GetFrameHeight() + 2.0f * style.WindowPadding.y);
        m_container->GetMutableAt(CONTAINER_TOOLBAR_ITEM_IDX)->m_height =
            (ImGui::GetContentRegionAvail().y - body_height) / 2.0f;
        m_container->GetMutableAt(CONTAINER_CONTENT_ITEM_IDX)->m_height =
            body_height - message_height;
        m_container->GetMutableAt(CONTAINER_MESSAGE_ITEM_IDX)->m_height  = message_height;
        m_container->GetMutableAt(CONTAINER_MESSAGE_ITEM_IDX)->m_visible = message_height;
        m_container->GetMutableAt(CONTAINER_STATUSBAR_ITEM_IDX)->m_height =
            (ImGui::GetContentRegionAvail().y - body_height) / 2.0f;
        m_configure_container->SetMinLeftWidth(
            ImGui::GetFrameHeightWithSpacing() +
            ImGui::CalcTextSize("Open the trace automatically when profiling finishes")
                .x +
            4.0f * style.WindowPadding.x);
        m_configure_container->SetMinRightWidth(
            2.0f * style.ItemSpacing.x + 2.0f * style.FramePadding.x +
            ImGui::CalcTextSize("Command PreviewCopy").x + 4.0f * style.WindowPadding.x);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
        m_container->Render();
        ImGui::PopStyleVar();
    }
    ImGui::End();
    ImGui::PopStyleVar();

    // The Advanced Options and Tool Locations windows are separate floating
    // windows (only relevant while configuring).
    if (!m_show_run_view)
    {
        RenderAdvancedWindow();
        RenderToolLocationsWindow();
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
    m_bring_to_front_on_focus = !window_open;
}

void
ProfilerLauncherDialog::RenderContent()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        m_settings.GetDefaultStyle().WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        m_settings.GetDefaultStyle().ItemSpacing);
    ImGui::BeginChild("content", ImVec2(0.0f, 0.0f),
                      ImGuiChildFlags_AlwaysUseWindowPadding);
    if(m_show_run_view)
    {
        BeginCodePanel("RunOutput", ImVec2(0.0f, 0.0f));

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
        switch(status_level)
        {
            case ConsoleStatusLevel::kSuccess:
                pill_color_id = Colors::kTextSuccess;
                break;
            case ConsoleStatusLevel::kError: pill_color_id = Colors::kTextError; break;
            case ConsoleStatusLevel::kRunning: pill_color_id = Colors::kAccent; break;
            default: pill_color_id = Colors::kTextDim; break;
        }
        const float spacing      = ImGui::GetStyle().ItemSpacing.x;
        const float actions_w    = CheckboxWidth("Auto-scroll") + ButtonWidth("Copy") +
                                   ButtonWidth("Clear") + spacing * 2.0f;
        const float status_end_x = ImGui::GetCursorPosX() +
                                   ImGui::GetContentRegionAvail().x - actions_w - spacing;
        StatusPill(status_label.c_str(), settings.GetColor(pill_color_id));

        std::string info;
        if(m_run_start_time > 0.0)
        {
            double end     = (m_run_end_time > 0.0) ? m_run_end_time : ImGui::GetTime();
            double elapsed = end - m_run_start_time;
            char   elapsed_text[32];
            std::snprintf(elapsed_text, sizeof(elapsed_text), "Elapsed %.1fs", elapsed);
            info = elapsed_text;
        }
        if(!status_detail.empty())
        {
            info += (info.empty() ? "" : SUMMARY_SEPARATOR) + status_detail;
        }
        if(!info.empty())
        {
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            FittedDimText(info, status_end_x - ImGui::GetCursorPosX());
        }

        SameLineRightAligned(actions_w);
        ImGui::Checkbox("Auto-scroll##Output", &m_auto_scroll_output);
        ImGui::SameLine();
        if(ImGui::Button("Copy##OutputCopy"))
        {
            ImGui::SetClipboardText(m_output_text.c_str());
        }
        ImGui::SameLine();
        if(ImGui::Button("Clear##OutputClear"))
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
    }
    else
    {
        // Sync typed settings to backend_payload so preset save sees current values
        IProfilerBackend* backend = m_backends[m_backend_index].get();
        m_config.backend_payload  = backend->SaveSettings();

#ifdef ROCPROFVIS_ENABLE_REMOTE
    // Keep the launch profile's SSH connection reference in sync with the
    // currently selected connection so saved profiles reference it.
    m_config.ssh_connection_ref = m_selected_connection_id;
#endif

        backend = m_backends[m_backend_index].get();

        // Layout only: the cards inside paint their own surfaces over the window.
        ImGui::BeginChild("MainPane", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoBackground);
        m_configure_container->Render();
        ImGui::EndChild();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}

void
ProfilerLauncherDialog::RenderStatusBar()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        m_settings.GetDefaultStyle().WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        m_settings.GetDefaultStyle().ItemSpacing);
    ImGui::BeginChild("status", ImVec2(0.0f, 0.0f),
                      ImGuiChildFlags_AlwaysUseWindowPadding);
    bool                        is_running = m_orchestrator.IsRunning();
    rocprofvis_profiler_state_t state      = m_orchestrator.GetState();
    bool                        state_ready =
        !is_running &&
        (state == kRPVProfilerStateIdle || state == kRPVProfilerStateCompleted ||
         state == kRPVProfilerStateFailed || state == kRPVProfilerStateCancelled);

    // Live readiness: backend validation plus (for remote) a configured host.
    IProfilerBackend* backend = m_backends[m_backend_index].get();
    bool              valid   = backend->Validate(m_config).empty();
#ifdef ROCPROFVIS_ENABLE_REMOTE
    valid &= !IsSshMode() || (m_remote_uri->GetRemoteHostString().size() &&
                              m_remote_uri->GetRemoteUserString().size());
    // readiness = "Configure the SSH connection (host/user) to launch";
#endif
    bool can_launch = state_ready && valid;
    // If a run is in flight or a previous run's output is available, let the
    // user jump straight to the focused run view.
    bool              has_run_view  = is_running || state == kRPVProfilerStateRunning ||
                                      state == kRPVProfilerStateCompleted ||
                                      state == kRPVProfilerStateFailed ||
                                      state == kRPVProfilerStateCancelled;
    const bool        show_view_run = has_run_view && !m_output_text.empty();
    const std::string trace_path    = m_orchestrator.GetTracePath();
    // A completed local run can be opened by hand; remote runs load the
    // downloaded trace through the orchestrator.
    const bool can_open_trace = !is_running && !trace_path.empty() &&
                                m_orchestrator.GetState() == kRPVProfilerStateCompleted &&
                                !m_orchestrator.IsRemote();
    if(!m_show_run_view)
    {
        StatusPill(is_running   ? "Profiling"
                   : can_launch ? "Ready to launch"
                                : "Configuration error",
                   m_settings.GetColor(is_running   ? Colors::kAccent
                                       : can_launch ? Colors::kTextSuccess
                                                    : Colors::kTextError));
    }

    ImGui::SameLine(
        ImGui::GetContentRegionAvail().x -
        ImGui::CalcTextSize("Back to ConfigurationOpen TraceLaunch Profiler").x -
        6.0f * m_settings.GetDefaultStyle().FramePadding.x -
        2.0f * m_settings.GetDefaultStyle().ItemSpacing.x);
    if(m_show_run_view)
    {
        if(!can_open_trace)
        {
            ImGui::Dummy(ImVec2(ImGui::CalcTextSize("Open Trace").x +
                                    2.0f * m_settings.GetDefaultStyle().FramePadding.x,
                                ImGui::GetFrameHeight()));
            ImGui::SameLine();
        }
        if(ImGui::Button("Back to Configuration"))
        {
            m_show_run_view = false;
        }
        ImGui::SameLine();
        if(can_open_trace)
        {
            if(ImGui::Button("Open Trace"))
            {
                m_app_window->OpenFile(trace_path);
            }
            ImGui::SameLine();
        }
        if(AccentButton(is_running ? "Cancel" : "Run Again",
                        ImVec2(ImGui::CalcTextSize("Launch Profiler").x +
                                   2.0f * m_settings.GetDefaultStyle().FramePadding.x,
                               0.0f)))
        {
            if(is_running)
            {
                OnCancelClicked();
            }
            else
            {
                OnLaunchClicked();
            }
        }
    }
    else
    {
        ImGui::Dummy(ImVec2(ImGui::CalcTextSize("Open Trace").x +
                                2.0f * m_settings.GetDefaultStyle().FramePadding.x,
                            ImGui::GetFrameHeight()));
        ImGui::SameLine();
        ImGui::BeginDisabled(!show_view_run);
        if(ImGui::Button("View Last Run",
                         ImVec2(ImGui::CalcTextSize("Back to Configuration").x +
                                    2.0f * m_settings.GetDefaultStyle().FramePadding.x,
                                ImGui::GetFrameHeight())))
        {
            m_show_run_view = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!can_launch);
        if(AccentButton("Launch Profiler"))
        {
            OnLaunchClicked();
        }
        ImGui::EndDisabled();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}

std::string
ProfilerLauncherDialog::BuildRunSummary() const
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

        TargetSpec const& target = m_backends[m_backend_index]->GetTarget(m_config.operation);
        if(!target.path.empty())
        {
            ss << SUMMARY_SEPARATOR << target.path;
            if(!target.arguments.empty())
            {
                ss << " " << target.arguments;
            }
        }
    }

#ifdef ROCPROFVIS_ENABLE_REMOTE
    if (IsSshMode())
    {
        ss << SUMMARY_SEPARATOR << m_remote_uri->GetRemoteUserString() << "@"
           << m_remote_uri->GetRemoteHostString();
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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        m_settings.GetDefaultStyle().WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        m_settings.GetDefaultStyle().ItemSpacing);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(
                                                m_settings.GetColor(Colors::kBgPanel)));
    ImGui::BeginChild("toolbar", ImVec2(0.0f, 0.0f),
                      ImGuiChildFlags_AlwaysUseWindowPadding);
    if(m_show_run_view)
    {
        // The configure view's frame: one line where its toolbar sits, a card
        // filling the middle, and the same footer, so switching views moves nothing.
        ImGui::AlignTextToFramePadding();
        FittedDimText(m_run_summary, ImGui::GetContentRegionAvail().x);
    }
    else
    {
        // Profiler selector
        PanelFieldLabel("Profiler");
        ImGui::SameLine();
        // Size this based on "compute"...
        ROCPROFVIS_ASSERT(m_backends.size() >= 2);
        ImGui::PushItemWidth(ImGui::CalcTextSize(m_backends[1]->DisplayName()).x +
                             ImGui::GetFrameHeight() +
                             ImGui::GetStyle().FramePadding.x * 2.0f);
        if(ImGui::BeginCombo("##ProfilerBackend",
                             m_backends[m_backend_index]->DisplayName()))
        {
            for(size_t i = 0; i < m_backends.size(); i++)
            {
                bool selected = (static_cast<int>(i) == m_backend_index);
                if(ImGui::Selectable(m_backends[i]->DisplayName(), selected) && !selected)
                {
                    SwitchBackend(static_cast<int>(i));
                }
            }
            // Placeholders for the profilers this launcher is designed to grow into.
            // Disabled until their backends land; listed so the intended scope
            // (rocprof-sys, rocprof-compute, rocprofv3) is visible.
            ImGui::BeginDisabled();
            ImGui::Selectable("rocprofv3 (coming soon)", false);
            ImGui::EndDisabled();
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();

        // Tool selector
        IProfilerBackend const*  backend = m_backends[m_backend_index].get();
        PanelFieldLabel("Tool");
        ImGui::SameLine();
        // Size this based on "instrument"...
        ROCPROFVIS_ASSERT(m_backends[0]->GetTools().size() >= 3);
        ImGui::PushItemWidth(
            ImGui::CalcTextSize(m_backends[0]->GetTools()[2].display_name.c_str()).x +
            ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.x * 2.0f);
        if(ImGui::BeginCombo("##ToolSelector", CurrentToolDisplayName().c_str()))
        {
            for(auto const& option : backend->GetTools())
            {
                if(ImGui::Selectable(option.display_name.c_str(),
                                     option.tool == m_config.tool &&
                                         option.operation == m_config.operation))
                {
                    m_config.tool           = option.tool;
                    m_config.operation      = option.operation;
                    m_execution_cache_dirty = true;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();

        VerticalSeparator();

        // Saved launch profiles (Optiq JSON presets)
        bool        config_reset = false;
        std::string load_name =
            RenderSavedProfileBar(m_preset_manager, m_config.profiler_id,
                                  m_current_preset_name, m_config, backend, m_app_window,
                                  config_reset);
        if(config_reset)
        {
            m_execution_cache_dirty = true;
        }

        if(!load_name.empty())
        {
            LaunchConfig loaded;
            if(m_preset_manager.LoadPreset(load_name, m_config.profiler_id, loaded))
            {
                m_config                = loaded;
                m_execution_cache_dirty = true;
                m_backends[m_backend_index]->LoadSettings(m_config.backend_payload);
                SyncToolWithBackend();
                backend = m_backends[m_backend_index].get();
#ifdef ROCPROFVIS_ENABLE_REMOTE
                // Resolve the profile's referenced SSH connection (if any) so the
                // remote section reflects the saved connection.
                if(!m_config.ssh_connection_ref.empty() &&
                   m_connection_store.Get(m_config.ssh_connection_ref) != nullptr)
                {
                    m_selected_connection_id = m_config.ssh_connection_ref;
                    ApplySelectedConnection();
                }
#endif
                // A profile names where to run, not that destination's tools.
                ApplyLaunchEnvironment();
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void
ProfilerLauncherDialog::RenderConfigureOptions()
{
    // Where to run first: pick the machine before the paths. Rendered in builds
    // without remote support too, since the tool locations apply locally.
    BeginLaunchCard("card_connection");
    LaunchCardHeader(ICON_CHAIN, "Where to run");
    ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize("Environment").x -
                    2.0f * m_settings.GetDefaultStyle().FramePadding.x);
    ImGui::BeginDisabled(IsSshMode() && !HasRemoteConnection());
    if(ImGui::Button("Environment"))
    {
        m_environment_edit.tool_directory     = m_config.tool_directory;
        m_environment_edit.python_environment = m_config.python_environment;
        m_show_tool_locations_window          = true;
    }
    ImGui::EndDisabled();
#ifdef ROCPROFVIS_ENABLE_REMOTE
    RenderRemoteSection();
#endif
    RenderToolResolutionNotice();
    EndLaunchCard();

    BeginLaunchCard("card_target");
    LaunchCardHeader(ICON_OPEN, "Target",
                     "The program to profile and where its results go");
    m_execution_cache_dirty |=
        m_backends[m_backend_index]->RenderTargetSection(m_config, MakeBrowseFn());
    EndLaunchCard();

    BeginLaunchCard("card_general");
    LaunchCardHeader(ICON_CHART_BAR, "Profiling Options");
    // Only when the backend has something to show there.
    if(!m_backends[m_backend_index]
            ->GetAdvancedOptions(m_config.tool, m_config.operation)
            .empty())
    {
        ImGui::SameLine(ImGui::GetContentRegionMax().x -
                        ImGui::CalcTextSize("Advanced").x -
                        2.0f * m_settings.GetDefaultStyle().FramePadding.x);
        if(ImGui::Button("Advanced##ProfilingOptions"))
        {
            m_show_advanced_window = true;
        }
    }
    m_execution_cache_dirty |= m_backends[m_backend_index]
                                   ->GetOptions(m_config.tool, m_config.operation)
                                   .render_fn();
    EndLaunchCard();

    BeginLaunchCard("card_inputs");
    LaunchCardHeader(ICON_LIST, "Arguments & Environment");
    RenderArgsEnvPanel();
    EndLaunchCard();
}

void
ProfilerLauncherDialog::RenderConfigureCMD()
{
    BeginCodePanel("cfg_preview", ImVec2(0.0f, 0.0f));
    RenderCommandPreview(m_execution_cache.command_preview);
    EndCodePanel();
}

void
ProfilerLauncherDialog::RenderMessages()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        m_settings.GetDefaultStyle().WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        m_settings.GetDefaultStyle().ItemSpacing);
    ImGui::BeginChild("messages", ImVec2(0.0f, 0.0f),
                      ImGuiChildFlags_AlwaysUseWindowPadding);
    if(m_messages.size())
    {
        BeginLaunchCard("error_messages");
        for(auto const& w : m_messages)
        {
            Colors      color_id;
            const char* prefix;
            switch(w.level)
            {
                case WarningMessage::kError:
                    color_id = Colors::kTextError;
                    prefix   = "Configuration error: ";
                    break;
                case WarningMessage::kWarning:
                    color_id = Colors::kTextWarning;
                    prefix   = "Configuration warning: ";
                    break;
                default:
                    color_id = Colors::kAccent;
                    prefix   = "Configuration hint: ";
                    break;
            }
            ImGui::PushStyleColor(
                ImGuiCol_Text,
                ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(color_id)));
            ImGui::TextWrapped("%s%s", prefix, w.text.c_str());
            ImGui::PopStyleColor();
        }
        EndLaunchCard();
    }
    if(m_error_message.size())
    {
        BeginLaunchCard("last_run");
        ImGui::PushStyleColor(
            ImGuiCol_Text,
            ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(Colors::kTextError)));
        ImGui::TextWrapped("Last run: %s", m_error_message.c_str());
        ImGui::PopStyleColor();
        EndLaunchCard();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}

void
ProfilerLauncherDialog::RenderToolResolutionNotice()
{
    if (!m_execution_cache.resolve_error.empty() || !m_config.tool_directory.empty() || !m_config.python_environment.empty())
    {
        // Said here rather than only on Launch, because "the profiler is not
        // installed" is not something the user should discover by pressing a
        // button. The command preview still shows the bare tool name, so
        // this line is what explains why it has no path.
        constexpr float NOTICE_ROUNDING     = 6.0f;
        constexpr float NOTICE_FILL_ALPHA   = 0.10f;
        constexpr float NOTICE_BORDER_ALPHA = 0.45f;
        const ImU32     warning             = m_settings.GetColor(Colors::kTextWarning);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ApplyAlpha(warning, NOTICE_FILL_ALPHA));
        ImGui::PushStyleColor(ImGuiCol_Border, ApplyAlpha(warning, NOTICE_BORDER_ALPHA));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, NOTICE_ROUNDING);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                            m_settings.GetDefaultStyle().WindowPadding);
        ImGui::BeginChild("tool_notice", ImVec2(0.0f, 0.0f),
                          ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders |
                              ImGuiChildFlags_AlwaysUseWindowPadding,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        if(m_execution_cache.resolve_error.size())
        {
            ImGui::TextWrapped("%s", m_execution_cache.resolve_error.c_str());
        }
        else
        {
            ImGui::TextUnformatted("Using custom environment.");
            if(m_config.tool_directory.size())
            {
                ImGui::TextWrapped("Tools: %s", m_config.tool_directory.c_str());
            }
            if(m_config.python_environment.size())
            {
                ImGui::TextWrapped("Python: %s", m_config.python_environment.c_str());
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
        return;
    }
}

void
ProfilerLauncherDialog::RenderToolLocationsWindow()
{
    if(m_show_tool_locations_window)
    {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        // The path fields stretch to the window, so the width is set (in units
        // of the font size) and only the height fits the contents.
        ImGui::SetNextWindowSize(
            ImVec2(700.0f, 0.0f),
            ImGuiCond_Appearing);

        PopUpStyle popup_style;
        popup_style.PushPopupStyles();
        popup_style.PushTitlebarColors();
        ImGui::PushStyleColor(ImGuiCol_WindowBg, m_settings.GetColor(Colors::kBgPanel));

        bool open = true;
        if(ImGui::Begin("Environment", &open,
                        WINDOW_FLAGS | ImGuiWindowFlags_AlwaysAutoResize))
        {
            BeginPanelCard("sub", PanelCardTone::kPanel,
                           m_settings.GetDefaultStyle().WindowPadding, true, &m_settings);
            ImGui::BeginDisabled();
            ImGui::TextWrapped(
                "Where the profiler tools and their Python environment live.%s",
                IsSshMode() ? " Paths are on the remote host." : "");
            ImGui::EndDisabled();
            ImGui::Separator();

            // Same browse handler as the Target paths. The fields edit
            // m_environment_edit; nothing reaches the command until OK.
            TargetBrowseFn on_browse = MakeBrowseFn();
            RenderToolLocationSection(m_environment_edit.tool_directory, m_config.connection,
                                      IsSshMode() ? std::string() : m_execution_cache.argv0,
                                      on_browse);
            RenderPythonEnvironmentSection(m_environment_edit.python_environment,
                                           m_config.connection, on_browse);
            EndPanelCard();

            const float button_width = ButtonWidth("Cancel");
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x -
                                 2.0f * button_width -
                                 m_settings.GetDefaultStyle().ItemSpacing.x);
            if(ImGui::Button("Cancel##Environment", ImVec2(button_width, 0.0f)))
            {
                open = false;
            }
            ImGui::SameLine();
            if(AccentButton("Save##Environment", ImVec2(button_width, 0.0f), &m_settings))
            {
                m_config.tool_directory     = m_environment_edit.tool_directory;
                m_config.python_environment = m_environment_edit.python_environment;
                m_execution_cache_dirty     = true;
                const std::string id        = ActiveLaunchEnvironmentId();
                if(!id.empty())
                {
                    ProfilesDocument::Get().SetLaunchEnvironment(id, m_environment_edit);
                }
                open = false;
            }
        }
        ImGui::End();
        ImGui::PopStyleColor();

        // Closing by Cancel or the close box discards the edits.
        if(!open)
        {
            m_show_tool_locations_window = false;
        }
    }
}

void
ProfilerLauncherDialog::RenderAdvancedWindow()
{
    if (!m_show_advanced_window)
    {
        return;
    }

    IProfilerBackend const* backend = m_backends[m_backend_index].get();

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(700, 560), ImGuiCond_FirstUseEver);

    PopUpStyle popup_style;
    popup_style.PushPopupStyles();
    popup_style.PushTitlebarColors();

    bool open    = true;
    if(ImGui::Begin("Advanced Profiling Options", &open, WINDOW_FLAGS))
    {
        ImGui::BeginDisabled();
        ImGui::TextWrapped(
            "Fine-grained settings, applied on top of the selected preset.");
        ImGui::EndDisabled();
        ImGui::Separator();

        if (ImGui::BeginTabBar("AdvancedWindowTabs"))
        {
            for(TabDescriptor const& tab :
                backend->GetAdvancedOptions(m_config.tool, m_config.operation))
            {
                if (ImGui::BeginTabItem(tab.display_name.c_str()))
                {
                    // Wrap the tab body in a padded card, matching the main dialog.
                    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                                          m_settings.GetColor(Colors::kBgPanel));
                    ImGui::PushStyleColor(
                        ImGuiCol_Border, m_settings.GetColor(Colors::kPanelBorderSubtle));
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, PANEL_CARD_ROUNDING);
                    PushSlimScrollbarStyle();
                    ImGui::BeginChild("adv_scroll", ImVec2(0.0f, 0.0f),
                                      ImGuiChildFlags_Borders |
                                          ImGuiChildFlags_AlwaysUseWindowPadding);
                    PopSlimScrollbarStyle();
                    m_execution_cache_dirty |= tab.render_fn();
                    ImGui::EndChild();
                    ImGui::PopStyleVar(1);
                    ImGui::PopStyleColor(2);
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();

    if (!open)
    {
        m_show_advanced_window = false;
    }
}

void ProfilerLauncherDialog::RenderArgsEnvPanel()
{
    const ImU32 accent = m_settings.GetColor(Colors::kAccent);
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

    // Each profiler keeps its own extra arguments and environment variables.
    ExtraArguments& extra =
        m_backends[m_backend_index]->GetExtraArguments(m_config.operation);

    // ===== Command line arguments (lead - they read as a one-liner) =====
    LaunchSubHeader("COMMAND LINE ARGUMENTS",
                    "Passed to the profiler, one entry at a time (a flag, or a flag + value).");

    bool        add_arg = false;
    const float add_w   = LaunchActionButtonWidth("Add");
    ImGui::SetNextItemWidth(-(add_w + style.ItemSpacing.x));
    if (InputTextStringWithHint("##ArgInput", "e.g.  --sampling-freq 500",
                                m_arg_input, ImGuiInputTextFlags_EnterReturnsTrue))
    {
        add_arg = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Add##Arg", ImVec2(add_w, 0.0f)))
    {
        add_arg = true;
    }
    if (add_arg)
    {
        std::string value = trim(m_arg_input);
        if (!value.empty())
        {
            extra.argv.push_back(value);
            m_arg_input.clear();
            m_execution_cache_dirty = true;
        }
    }

    if (!extra.argv.empty())
    {
        ImGui::Spacing();
        int edit_idx   = -1;
        int remove_idx = -1;
        for (size_t i = 0; i < extra.argv.size(); i++)
        {
            ImGui::PushID(static_cast<int>(i));
            PillAction act = EditablePill(extra.argv[i].c_str(), accent);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Click to edit  -  x to remove");
            }
            ImGui::PopID();

            if (act == PillAction::kEdit)   { edit_idx = static_cast<int>(i); }
            if (act == PillAction::kRemove) { remove_idx = static_cast<int>(i); }

            if (i + 1 < extra.argv.size() &&
                keep_on_row(ImGui::CalcTextSize(extra.argv[i + 1].c_str()).x))
            {
                ImGui::SameLine();
            }
        }
        if (edit_idx >= 0)
        {
            // Pick it up into the edit box; modify + Add to re-add.
            m_arg_input = extra.argv[static_cast<size_t>(edit_idx)];
            extra.argv.erase(extra.argv.begin() + edit_idx);
            m_execution_cache_dirty = true;
        }
        else if (remove_idx >= 0)
        {
            extra.argv.erase(extra.argv.begin() + remove_idx);
            m_execution_cache_dirty = true;
        }
    }

    ImGui::Spacing();

    // ===== Environment variables (name = value, grow vertically) =====
    LaunchSubHeader("ENVIRONMENT VARIABLES",
                    "Extra environment variables set for the profiler process.");

    bool        add_env   = false;
    const float add_env_w = LaunchActionButtonWidth("Add");
    ImGui::SetNextItemWidth(200.0f);
    InputTextStringWithHint("##EnvName", "NAME", m_env_name_input);
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("=");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-(add_env_w + style.ItemSpacing.x));
    if (InputTextStringWithHint("##EnvValue", "value", m_env_value_input,
                                ImGuiInputTextFlags_EnterReturnsTrue))
    {
        add_env = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Add##Env", ImVec2(add_env_w, 0.0f)))
    {
        add_env = true;
    }
    if (add_env)
    {
        std::string name = trim(m_env_name_input);
        if (!name.empty())
        {
            extra.env[name] = m_env_value_input;
            m_env_name_input.clear();
            m_env_value_input.clear();
            m_execution_cache_dirty = true;
        }
    }

    if (!extra.env.empty())
    {
        ImGui::Spacing();
        std::vector<std::pair<std::string, std::string>> envs(
            extra.env.begin(), extra.env.end());
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
            auto it = extra.env.find(edit_key);
            if (it != extra.env.end())
            {
                m_env_name_input        = it->first;
                m_env_value_input       = it->second;
                extra.env.erase(it);
                m_execution_cache_dirty = true;
            }
        }
        else if (!remove_key.empty())
        {
            extra.env.erase(remove_key);
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
    // backend's RenderTargetSection) so it sits alongside the SSH connection
    // options, which need dialog-owned state (RemoteUri / SshSettingsDialog /
    // RemoteProfilerSession) to render. Laid out as design-language label/value
    // table rows so it matches the remote trace dialog.
    if (!ImGui::BeginTable("##profiler_run_on", 2, ImGuiTableFlags_SizingStretchProp))
    {
        return;
    }
    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed,
                            ImGui::CalcTextSize("Connection").x);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    PanelFieldLabel("Run on", true, &m_settings);

    // A two-button segmented control reads more clearly than a dropdown for a
    // binary choice.
    ImGui::TableSetColumnIndex(1);
    bool is_local = (m_config.connection == ConnectionType::kLocal);

    if (ImGui::RadioButton("This machine", is_local))
    {
        m_config.connection     = ConnectionType::kLocal;
        m_execution_cache_dirty = true;
        ApplyLaunchEnvironment();
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Remote (SSH)", !is_local))
    {
        m_config.connection     = ConnectionType::kSsh;
        m_execution_cache_dirty = true;
        ApplyLaunchEnvironment();
    }

    if (IsSshMode())
    {
        // Connection chip: accent when configured, dim "Configure" when not -
        // clicking it opens the shared SSH settings dialog (matches the remote
        // trace dialog's header connection button).
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        PanelFieldLabel("Connection", true, &m_settings);

        ImGui::TableSetColumnIndex(1);
        const std::string host = m_remote_uri->GetRemoteHostString();
        // Prefer the connection's user-chosen name; fall back to user@host:port.
        const std::string host_chip =
            host.empty() ? std::string("Configure")
                         : m_remote_uri->GetConnection().DisplayLabel();

        ImGui::PushStyleColor(ImGuiCol_Button, m_settings.GetColor(Colors::kButton));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              m_settings.GetColor(Colors::kButtonHovered));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              m_settings.GetColor(Colors::kButtonActive));
        ImGui::PushStyleColor(ImGuiCol_Text, host.empty()
                                                 ? m_settings.GetColor(Colors::kTextMain)
                                                 : m_settings.GetColor(Colors::kAccent));
        if (ImGui::Button((host_chip + "##profiler_connection").c_str(),
                         ImVec2(-FLT_MIN, 0.0f)))
        {
            m_ssh_settings_dialog = std::make_unique<SshSettingsDialog>(
                m_connection_store, m_selected_connection_id,
                [this](const std::string& id)
                {
                    m_selected_connection_id = id;
                    ApplySelectedConnection();
                    ApplyLaunchEnvironment();
                    m_execution_cache_dirty = true;
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

void
ProfilerLauncherDialog::Update()
{
    if (m_show_window)
    {
        // Rebuild the (allocation-heavy) execution cache / command preview only
        // when a control reported an actual change - every widget, including the
        // backend-owned settings tabs, ORs its ImGui return value into the dirty
        // flag. 
        if (m_execution_cache_dirty)
        {
            RefreshExecutionCache();

            m_messages.clear();
            std::string validate = m_backends[m_backend_index]->Validate(m_config);
            if(validate.size())
            {
                m_messages.push_back({ WarningMessage::kError, validate });
            }
#ifdef ROCPROFVIS_ENABLE_REMOTE
            if(IsSshMode() && (m_remote_uri->GetRemoteHostString().empty() ||
                               m_remote_uri->GetRemoteUserString().empty()))
            {
                m_messages.push_back(
                    { WarningMessage::kError,
                      "Configure the SSH connection (host/user) to launch" });
            }
#endif
            std::vector<WarningMessage> warnings =
                m_backends[m_backend_index]->GetWarnings(m_config);
            m_messages.insert(m_messages.end(), warnings.begin(), warnings.end());
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
        m_config.operation = kRPVProfilerOperationDefault;
        return;
    }

    std::vector<ToolOption> tools = m_backends[m_backend_index]->GetTools();
    if (tools.empty())
    {
        m_config.tool = kRPVProfilerToolNone;
        m_config.operation = kRPVProfilerOperationDefault;
        return;
    }

    for (auto const& option : tools)
    {
        if(option.tool == m_config.tool && option.operation == m_config.operation)
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
    m_config.profiler_id = m_backends[m_backend_index]->Id();
    m_config.tool = tools[0].tool;
    m_config.operation   = tools[0].operation;
}

std::string ProfilerLauncherDialog::CurrentToolDisplayName() const
{
    if (m_backends.empty())
    {
        return std::string();
    }
    for (auto const& option : m_backends[m_backend_index]->GetTools())
    {
        if(option.tool == m_config.tool && option.operation == m_config.operation)
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
    m_run_summary.clear();
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
    // to the orchestrator. The controller scrapes the produced trace path from
    // the profiler's stdout; the operation on the spec selects those rules.
    TargetSpec const& target = backend->GetTarget(m_config.operation);

    ProfilerLaunchOrchestrator::LaunchRequest request;
    BuildLaunchSpec(m_execution_cache, request.spec);
    request.is_remote       = is_remote;
    // A run that writes no trace is not asked to open (or, remotely, download)
    // one, so it is not reported missing either.
    request.auto_load_trace = target.auto_load_trace && backend->ProducesTrace(m_config);
#ifdef ROCPROFVIS_ENABLE_REMOTE
    request.remote_uri = is_remote ? m_remote_uri : nullptr;
#endif

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
        m_run_summary     = BuildRunSummary();
        m_last_seen_state = kRPVProfilerStateRunning;
        RebuildComposedOutput();

        // Swap to the focused run view so the live output owns the window.
        m_show_run_view        = true;
        m_show_advanced_window = false;
        m_show_tool_locations_window = false;
        m_run_start_time       = ImGui::GetTime();
        m_run_end_time         = 0.0;

        SaveToSettings();
        AddRecentPath(backend->GetRecentTargets(m_config.tool, m_config.operation),
                      target.path);
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
    m_show_tool_locations_window = false;
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
                m_error_message = "Profiler executable not found or could not be started "
                                  "(exit code 127)";
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
        m_tool_path.ssh == ssh && m_tool_path.directory == m_config.tool_directory &&
        m_tool_path.python_environment == m_config.python_environment)
    {
        return;
    }

    m_tool_path.tool               = m_config.tool;
    m_tool_path.directory          = m_config.tool_directory;
    m_tool_path.python_environment = m_config.python_environment;
    m_tool_path.ssh                = ssh;
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
            ResolveToolPath(m_config.tool, tool_directory, m_config.python_environment,
                            m_tool_path.error);
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
    for(std::pair<std::string const, std::string> const& kv :
        backend->GetExtraArguments(m_config.operation).env)
    {
        cache.env_vars.emplace_back(kv.first, kv.second);
    }

    // The controller composes the preview from the same config a launch builds,
    // so the environment it shows is the one the run gets.
    ProfilerLaunchSpec spec;
    BuildLaunchSpec(cache, spec);
    rocprofvis_profiler_config_t* config = AllocProfilerConfig(spec);
    if(config != nullptr)
    {
        uint32_t length = 0;
        if(rocprofvis_profiler_config_get_command_preview(config, IsSshMode(),
                                                          cache.argv0.c_str(), nullptr,
                                                          &length) == kRocProfVisResultSuccess)
        {
            std::vector<char> buffer(length + 1, '\0');
            if(rocprofvis_profiler_config_get_command_preview(
                   config, IsSshMode(), cache.argv0.c_str(), buffer.data(), &length) ==
               kRocProfVisResultSuccess)
            {
                cache.command_preview.assign(buffer.data(), length);
            }
        }
        rocprofvis_profiler_config_free(config);
    }

    m_execution_cache = std::move(cache);
}

void
ProfilerLauncherDialog::BuildLaunchSpec(ExecutionCache const& cache,
                                        ProfilerLaunchSpec&   spec) const
{
    TargetSpec const& target =
        m_backends[m_backend_index]->GetTarget(m_config.operation);
    spec.tool               = cache.tool;
    spec.operation          = m_config.operation;
    spec.tool_directory     = m_config.tool_directory;
    spec.python_environment = m_config.python_environment;
    spec.output_directory   = target.output_directory;
    spec.working_directory  = target.working_directory;
    spec.profiler_argv      = cache.argv;
    spec.env_vars           = cache.env_vars;
}

void ProfilerLauncherDialog::SwitchBackend(int index)
{
    if (index < 0 || index >= static_cast<int>(m_backends.size()))
    {
        return;
    }
    // A preset name belongs to the profiler it was selected for: park the
    // outgoing one and show the incoming profiler's own, which matches the
    // settings that backend still holds.
    std::unordered_map<std::string, std::string>& last_preset_names =
        m_settings.GetProfilerSettings().last_preset_names;
    last_preset_names[m_config.profiler_id] = m_current_preset_name;

    m_backend_index = index;
    m_config.profiler_id = m_backends[index]->Id();
    m_current_preset_name = last_preset_names[m_config.profiler_id];
    // A preset deleted since it was selected is not offered.
    if (!m_current_preset_name.empty())
    {
        bool exists = false;
        for (PresetInfo const& preset : m_preset_manager.ListPresets(m_config.profiler_id))
        {
            exists |= (preset.name == m_current_preset_name);
        }
        if (!exists)
        {
            m_current_preset_name.clear();
        }
    }
    // Not carried across: a tool belongs to one profiler, so the new backend's
    // default is the only sensible selection.
    m_config.tool = kRPVProfilerToolNone;
    SyncToolWithBackend();
    // Each backend keeps its own settings, target included, so switching back
    // finds it as it was left. Unsaved changes last for the session only.
    m_config.backend_payload = m_backends[index]->SaveSettings();
    m_execution_cache_dirty = true;

    m_show_tool_locations_window = false;
    m_show_advanced_window       = false;
}

void ProfilerLauncherDialog::LoadFromSettings()
{
    ProfilerSettings& ps = m_settings.GetProfilerSettings();

    // Set before any switch below, so SwitchBackend parks the starting
    // profiler's remembered name rather than overwriting it.
    m_current_preset_name = ps.last_preset_names[m_config.profiler_id];
#ifdef ROCPROFVIS_ENABLE_REMOTE
    m_selected_connection_id = ps.last_ssh_connection_id;
#endif

    if (!ps.last_profiler_id.empty())
    {
        for (size_t i = 0; i < m_backends.size(); i++)
        {
            if (m_backends[i]->Id() == ps.last_profiler_id)
            {
                SwitchBackend(i);
                break;
            }
        }
    }

    // Rehydrate the remembered profile's contents, not just its name (mirrors
    // the combo-select load path in RenderConfigureToolbar()).
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

    // Every backend starts with the settings of the preset it remembers, so
    // switching to one later shows a name that matches what it holds. Only the
    // payload applies here: the rest of m_config belongs to the active profile.
    for (size_t i = 0; i < m_backends.size(); i++)
    {
        std::string& name = ps.last_preset_names[m_backends[i]->Id()];
        if (static_cast<int>(i) != m_backend_index && !name.empty())
        {
            LaunchConfig loaded;
            if (m_preset_manager.LoadPreset(name, m_backends[i]->Id(), loaded))
            {
                m_backends[i]->LoadSettings(loaded.backend_payload);
            }
            else
            {
                name.clear();
            }
        }
    }
}

std::string
ProfilerLauncherDialog::ActiveLaunchEnvironmentId() const
{
    std::string id = LOCAL_LAUNCH_ENVIRONMENT_ID;
#ifdef ROCPROFVIS_ENABLE_REMOTE
    if(IsSshMode())
    {
        id = m_selected_connection_id;
    }
#endif
    return id;
}

void
ProfilerLauncherDialog::ApplyLaunchEnvironment()
{
    LaunchEnvironment environment;
    const std::string id = ActiveLaunchEnvironmentId();
    if(!id.empty())
    {
        environment = ProfilesDocument::Get().GetLaunchEnvironment(id);
    }
    m_config.tool_directory      = environment.tool_directory;
    m_config.python_environment  = environment.python_environment;
    m_execution_cache_dirty      = true;
    m_show_tool_locations_window = false;
}

void ProfilerLauncherDialog::SaveToSettings()
{
    ProfilerSettings& ps = m_settings.GetProfilerSettings();
    ps.last_preset_names[m_config.profiler_id] = m_current_preset_name;
    ps.last_profiler_id = m_config.profiler_id;
#ifdef ROCPROFVIS_ENABLE_REMOTE
    ps.last_ssh_connection_id = m_selected_connection_id;
#endif
    m_settings.SaveProfilerSettings();
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

TargetBrowseFn
ProfilerLauncherDialog::MakeBrowseFn()
{
    // Every pick lands frames after the click, when the row that opened the
    // picker has long returned, so the pick callback is what marks the command
    // preview stale. `field` points into m_config, which outlives the pickers.
    TargetBrowseFn on_browse;
    if(!IsSshMode())
    {
        if(m_app_window != nullptr)
        {
            on_browse = [this](std::string& field, BrowseKind kind, const char* title) {
                std::function<void(std::string)> on_picked = [this,
                                                              &field](std::string path) {
                    field                   = path;
                    m_execution_cache_dirty = true;
                };
                if(kind == File)
                {
                    m_app_window->ShowOpenFileDialog(title, {}, "", on_picked);
                }
                else
                {
                    m_app_window->ShowPathPickerDialog(title, "", on_picked);
                }
            };
        }
    }
#ifdef ROCPROFVIS_ENABLE_REMOTE
    // The connection is chosen under "Where to run", never in the browser, so
    // Browse stays disabled until one is set up.
    else if(HasRemoteConnection())
    {
        on_browse = [this](std::string& field, BrowseKind kind, const char* title) {
            ApplySelectedConnection();
            EnsureRemoteFileBrowser();
            m_remote_file_browser->SetTitle(title);
            m_remote_file_browser->Open(field,
                                        kind == File
                                            ? RemoteFileBrowser::PickMode::kFile
                                            : RemoteFileBrowser::PickMode::kDirectory,
                                        [this, &field](const std::string& path) {
                                            field                   = path;
                                            m_execution_cache_dirty = true;
                                        });
        };
    }
#endif
    return on_browse;
}

void
ProfilerLauncherDialog::AddRecentPath(std::vector<std::string>& recents,
                                      std::string const&        path)
{
    if(!path.empty())
    {
        // Move to the front, de-duplicated, capped at 10.
        recents.erase(std::remove(recents.begin(), recents.end(), path), recents.end());
        recents.insert(recents.begin(), path);
        if(recents.size() > 10)
        {
            recents.resize(10);
        }

        m_settings.SaveProfilerSettings();
    }
}

}  // namespace View
}  // namespace RocProfVis
