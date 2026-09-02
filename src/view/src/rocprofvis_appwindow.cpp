// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_appwindow.h"
#include "imgui.h"
#include "implot.h"
#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
#    include "nfd.h"
#endif
#include "ImGuiFileDialog.h"

#include "rocprofvis_appmonitor.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_events.h"
#include "rocprofvis_project.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_hotkey_manager.h"
#include "rocprofvis_render_scheduler.h"
#include "rocprofvis_settings_panel.h"
#include "rocprofvis_version.h"
#include "rocprofvis_utils.h"
#include "rocprofvis_root_view.h"
#include "rocprofvis_trace_view.h"
#include "rocprofvis_view_module.h"
#include "widgets/rocprofvis_debug_window.h"
#include "widgets/rocprofvis_log_viewer.h"
#include "widgets/rocprofvis_dialog.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_widget.h"
#include "widgets/rocprofvis_notification_manager.h"
// TEMPORARY (profiler launch): remove guard when the profiler feature graduates.
#ifdef ROCPROFVIS_ENABLE_PROFILER
#include "rocprofvis_profiler_launcher_dialog.h"
#endif
#ifdef ROCPROFVIS_ENABLE_REMOTE
#include "remote/rocprofvis_ssh_auth_modal.h"
#include "remote/rocprofvis_ssh_session.h"
#endif
#include "welcome/rocprofvis_welcome_page.h"
#include <algorithm>
#include <filesystem>
#include <sstream>
#include <utility>

namespace RocProfVis
{
namespace View
{

constexpr ImVec2      FILE_DIALOG_SIZE       = ImVec2(480.0f, 360.0f);
constexpr const char* FILE_DIALOG_NAME       = "ChooseFileDlgKey";
constexpr const char* TAB_CONTAINER_SRC_NAME = "MainTabContainer";
constexpr const char* ABOUT_DIALOG_NAME      = "About##_dialog";
constexpr const char* APP_SHUTDOWN_NOTIFICATION_ID = "provider_cleanup_app_shutdown";
constexpr const char* SHUTDOWN_DIALOG_NAME = "Closing Traces##_shutdown";

#ifdef ROCPROFVIS_PERFETTO_ENABLED
const std::vector<std::string> TRACE_EXTENSIONS   = { "db", "rpd", "yaml", "json", "proto", "pftrace"};
#else
const std::vector<std::string> TRACE_EXTENSIONS   = { "db", "rpd", "yaml"};
#endif
const std::vector<std::string> PROJECT_EXTENSIONS = { "rpv" };
#ifdef ROCPROFVIS_PERFETTO_ENABLED
const std::vector<std::string> ALL_EXTENSIONS     = { "db", "rpd", "yaml", "rpv", "json", "proto", "pftrace" };
#else
const std::vector<std::string> ALL_EXTENSIONS     = { "db", "rpd", "yaml", "rpv" };
#endif
const std::vector<std::string> COMPARE_EXTENSIONS = { "db" };

constexpr const char* CLEANUP_MESSAGE = "Waiting for requests to finish cleanup...";
constexpr const char* CLOSING_MESSAGE = "Closing...";

// Upper bound on how long the shutdown exit gate waits for AppMonitor
// operations to drain before exiting anyway. AppMonitor's destructor then runs
// a final bounded, cancelling drain as the backstop so no worker is left
// holding freed resources.
constexpr auto MONITOR_SHUTDOWN_GRACE_PERIOD = std::chrono::seconds(5);

// For testing DataProvider
void
RenderProviderTest(DataProvider& provider);

AppWindow* AppWindow::s_instance = nullptr;

AppWindow*
AppWindow::GetInstance()
{
    if(!s_instance)
    {
        s_instance = new AppWindow();
    }

    return s_instance;
}

void
AppWindow::DestroyInstance()
{
    if(s_instance)
    {
        delete s_instance;
        s_instance = nullptr;
    }
}

AppWindow::AppWindow()
: m_main_view(nullptr)
, m_settings_panel(nullptr)
, m_tab_container(nullptr)
, m_default_padding(0.0f, 0.0f)
, m_default_spacing(0.0f, 0.0f)
, m_open_about_dialog(false)
, m_tabclosed_event_token(EventManager::InvalidSubscriptionToken)
, m_tabselected_event_token(EventManager::InvalidSubscriptionToken)
, m_font_changed_token(EventManager::InvalidSubscriptionToken)
#ifdef ROCPROFVIS_DEVELOPER_MODE
, m_show_debug_window(false)
, m_show_provider_test_widow(false)
, m_show_metrics(false)
#endif
, m_confirmation_dialog(std::make_unique<ConfirmationDialog>(
      SettingsManager::GetInstance().GetUserSettings().dont_ask_before_exit))
, m_message_dialog(std::make_unique<MessageDialog>())
, m_compare_files_dialog(std::make_unique<CompareFilesDialog>(
      [this](CompareFilesDialog::FileSlot slot) { HandleCompareFileBrowse(slot); },
      [this](const std::string& first, const std::string& second) {
          OpenCompare(first, second);
      }))
, m_tool_bar_index(0)
, m_is_fullscreen(false)
, m_file_dialog_preference(kRocProfVisViewFileDialog_Auto)
, m_use_native_file_dialog(false)
, m_init_file_dialog(false)
#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
, m_is_native_file_dialog_open(false)
#endif
, m_disable_app_interaction(false)
, m_shutdown_requested(false)
, m_exit_notification_sent(false)
, m_restore_fullscreen_later(false)
, m_next_provider_cleanup_id(0)
, m_status_show_busy_indicator(false)
{}

AppWindow::~AppWindow()
{
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kTabClosed),
                                             m_tabclosed_event_token);
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kTabSelected),
                                             m_tabselected_event_token);
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kFontSizeChanged),
                                             m_font_changed_token);

    for(auto& job : m_provider_cleanup_jobs)
    {
        if(job.future.valid())
        {
            job.future.get();
        }
    }
    m_provider_cleanup_jobs.clear();
    // Drop tab view references before destroying projects (see BeginAppShutdown).
    if(m_tab_container)
    {
        m_tab_container->Clear();
    }
    m_projects.clear();
    // Destroy owners of monitored sessions (e.g. the profiler dialog and the
    // remote-trace orchestrator) before tearing down the monitor so they
    // unregister cleanly instead of lazily re-creating the singleton during
    // their own destruction.
#ifdef ROCPROFVIS_ENABLE_PROFILER
    m_profiler_launcher_dialog.reset();
#endif
#ifdef ROCPROFVIS_ENABLE_REMOTE
    m_ssh_test_dialog.reset();
#endif
    AppMonitor::DestroyInstance();

    LogViewer::DestroyInstance();
}

bool
AppWindow::Init()
{
    std::string config_path = get_application_config_path(true);

    std::filesystem::path ini_path     = std::filesystem::path(config_path) / "imgui.ini";
    ImGuiIO&              io           = ImGui::GetIO();
    static std::string    ini_path_str = ini_path.string();
    io.IniFilename                     = ini_path_str.c_str();

    ImPlot::CreateContext();

    SettingsManager& settings = SettingsManager::GetInstance();
    bool             result   = settings.Init();
    if(result)
    {
        m_settings_panel = std::make_unique<SettingsPanel>(settings);
    }
    else
    {
        spdlog::warn("Failed to initialize SettingsManager");
    }

    m_welcome_page = std::make_unique<WelcomePage>(
        [this]() { HandleOpenFile(); },
        [this](const std::string& file_path) { OpenFile(file_path); });

    constexpr float initial_status_bar_height = 30.0f;
    LayoutItem status_bar_item(-1, initial_status_bar_height);
    status_bar_item.m_item =
        std::make_shared<RocCustomWidget>([this]() { RenderStatusBar(); });
    LayoutItem main_area_item(-1, -initial_status_bar_height);
    LayoutItem tool_bar_item(-1, 0);
    tool_bar_item.m_child_flags = ImGuiChildFlags_AutoResizeY;

    m_tab_container = std::make_shared<TabContainer>();
    m_tab_container->SetEventSourceName(TAB_CONTAINER_SRC_NAME);
    m_tab_container->EnableSendCloseEvent(true);
    m_tab_container->EnableSendChangeEvent(true);

    main_area_item.m_item = std::make_shared<RocCustomWidget>([this]() {
        if(m_shutdown_requested)
        {
            RenderShutdownState();
        }
        else if(m_tab_container && !m_tab_container->GetTabs().empty())
        {
            m_tab_container->Render();
        }
        else
        {
            m_welcome_page->Render();
        }
    });

    std::vector<LayoutItem> layout_items;
    layout_items.push_back(tool_bar_item);
    m_tool_bar_index = static_cast<int>(layout_items.size() - 1);
    layout_items.push_back(main_area_item);
    layout_items.push_back(status_bar_item);
    m_main_view = std::make_shared<VFixedContainer>(layout_items);

    m_default_padding = ImGui::GetStyle().WindowPadding;
    m_default_spacing = ImGui::GetStyle().ItemSpacing;

    auto new_tab_closed_handler = [this](std::shared_ptr<RocEvent> e) {
        HandleTabClosed(e);
    };
    m_tabclosed_event_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTabClosed), new_tab_closed_handler);

    auto new_tab_selected_handler = [this](std::shared_ptr<RocEvent> e) {
        HandleTabSelectionChanged(e);
    };

    m_tabselected_event_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTabSelected), new_tab_selected_handler);

    auto font_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        (void) e;
        HandleFontChanged();
    };

    m_font_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kFontSizeChanged), font_changed_handler);

    ConfigureFileDialogBackend();
    HandleFontChanged();
    return result;
}

void
AppWindow::ConfigureFileDialogBackend()
{
    bool want_native = false;
    switch(m_file_dialog_preference)
    {
        case kRocProfVisViewFileDialog_ImGui:
            want_native = false;
            break;
        case kRocProfVisViewFileDialog_Native:
            want_native = true;
            break;
        case kRocProfVisViewFileDialog_Auto:
        default:
#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
            want_native = !is_remote_display_session();
#else
            want_native = false;
#endif
            break;
    }

#ifndef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
    if(want_native)
    {
        spdlog::warn("--file-dialog=native requested but native dialog was "
                     "not compiled in; using ImGui.");
        want_native = false;
    }
#else
    if(want_native)
    {
        nfdresult_t nfd_result = NFD_Init();
        if(nfd_result != NFD_OKAY)
        {
            const char* err = NFD_GetError();
            spdlog::warn("NFD_Init failed ({}); falling back to in-process "
                         "ImGui file dialog.",
                         err ? err : "unknown");
            NFD_ClearError();
            want_native = false;
        }
        else
        {
            NFD_Quit();
        }
    }
#endif

    m_use_native_file_dialog.store(want_native);
    spdlog::info("File dialog backend: {}",
                 want_native ? "system file dialog" : "in-process ImGuiFileDialog");
}

void
AppWindow::SetFileDialogPreference(rocprofvis_view_file_dialog_preference_t pref)
{
    m_file_dialog_preference = pref;
}

void
AppWindow::SetNotificationCallback(std::function<void(int)> callback)
{
    m_notification_callback = std::move(callback);
}

const std::string&
AppWindow::GetMainTabSourceName() const
{
    return m_tab_container->GetEventSourceName();
}

void
AppWindow::SetTabLabel(const std::string& label, const std::string& id)
{
    m_tab_container->SetTabLabel(label, id);
}

void
AppWindow::ShowCloseConfirm()
{
    if(m_shutdown_requested)
    {
        RequestExitIfProviderCleanupsComplete();
        return;
    }

    if(m_tab_container->GetTabs().size() == 0 ||
       SettingsManager::GetInstance().GetUserSettings().dont_ask_before_exit)
    {
        BeginAppShutdown();
        return;
    }

    // Only show the dialog if there are open tabs
    ShowConfirmationDialog(
        "Confirm Close",
        "Are you sure you want to close the application? Any "
        "unsaved data will be lost.",
        [this]() { BeginAppShutdown(); });
}

void
AppWindow::SetFullscreenState(bool is_fullscreen)
{
    m_is_fullscreen = is_fullscreen;
}

bool
AppWindow::GetFullscreenState() const
{
    return m_is_fullscreen;
}

void
AppWindow::ShowConfirmationDialog(const std::string& title, const std::string& message,
                                  std::function<void()> on_confirm_callback) const
{
    m_confirmation_dialog->Show(title, message, on_confirm_callback);
}

void
AppWindow::ShowMessageDialog(const std::string& title, const std::string& message) const
{
    m_message_dialog->Show(title, message);
}

void
AppWindow::ShowSaveFileDialog(const std::string& title, const std::vector<FileFilter>& file_filters,
                              const std::string&               initial_path,
                              std::function<void(std::string)> callback)
{
#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
    if(m_use_native_file_dialog.load())
    {
        (void)title;
        ShowNativeFileDialog(file_filters, initial_path, callback, true);
        return;
    }
#endif
    ShowImGuiFileDialog(title, file_filters, initial_path, true, callback);
}

void
AppWindow::ShowOpenFileDialog(const std::string& title, const std::vector<FileFilter>& file_filters,
                              const std::string&               initial_path,
                              std::function<void(std::string)> callback)
{
#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
    if(m_use_native_file_dialog.load())
    {
        (void)title;
        ShowNativeFileDialog(file_filters, initial_path, callback, false);
        return;
    }
#endif
    ShowImGuiFileDialog(title, file_filters, initial_path, false, callback);
}

void
AppWindow::ShowOpenFilesDialog(
    const std::string& title, const std::vector<FileFilter>& file_filters,
    const std::string&                                   initial_path,
    std::function<void(const std::vector<std::string>&)> callback)
{
#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
    if(m_use_native_file_dialog.load())
    {
        (void)title;
        ShowNativeFilesDialog(file_filters, initial_path, callback);
        return;
    }
#endif
    m_files_dialog_callback = std::move(callback);
    ShowImGuiFileDialog(title, file_filters, initial_path, false, nullptr, false,
                        /*multi_select*/ true);
}

void
AppWindow::ShowPathPickerDialog(const std::string& title, const std::string& initial_path,
                                std::function<void(std::string)> callback)
{
#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
    if(m_use_native_file_dialog.load())
    {
        (void)title;
        ShowNativeFileDialog({}, initial_path, callback, false, true);
        return;
    }
#endif
    ShowImGuiFileDialog(title, {}, initial_path, false, callback, true);
}

Project*
AppWindow::GetProject(const std::string& id)
{
    Project* project = nullptr;
    if(m_projects.count(id) > 0)
    {
        project = m_projects[id].get();
    }
    return project;
}

Project*
AppWindow::GetCurrentProject()
{
    Project*       project    = nullptr;
    const TabItem* active_tab = m_tab_container->GetActiveTab();
    if(active_tab)
    {
        project = GetProject(active_tab->m_id);
    }
    return project;
}

Project*
AppWindow::FindProjectContainingSource(const std::string& file_path)
{
    auto canonical = [](const std::string& p) -> std::string {
        std::error_code      ec;
        std::filesystem::path c = std::filesystem::weakly_canonical(p, ec);
        return ec ? p : c.string();
    };
    const std::string target = canonical(file_path);
    for(auto& [id, project] : m_projects)
    {
        if(!project)
        {
            continue;
        }
        for(const std::string& source : project->GetSourceFiles())
        {
            if(canonical(source) == target)
            {
                return project.get();
            }
        }
    }
    return nullptr;
}

void
AppWindow::BeginAppShutdown()
{
    if(m_shutdown_requested)
    {
        RequestExitIfProviderCleanupsComplete();
        return;
    }

    m_shutdown_requested      = true;
    m_shutdown_start          = std::chrono::steady_clock::now();
    m_disable_app_interaction = true;

    NotificationManager::GetInstance().ShowPersistent(
        APP_SHUTDOWN_NOTIFICATION_ID,
        "Closing traces... " + std::to_string(m_provider_cleanup_jobs.size()) +
            " cleanup job(s) remaining",
        NotificationLevel::Info);

    for(auto& item : m_projects)
    {
        if(item.second)
        {
            item.second->Close();
            DetachProjectProviderCleanup(*item.second,
                                         ProviderCleanupReason::kAppShutdown);
        }
    }

#ifdef ROCPROFVIS_DEVELOPER_MODE
    StartProviderCleanup(m_test_data_provider.DetachCleanupWork(),
                         "developer data provider",
                         ProviderCleanupReason::kAppShutdown);
#endif

    // Null the toolbar item first: it holds a widget from the active view whose callback
    // captures that view, so it must be released before the views are destroyed below.
    if(m_main_view)
    {
        m_main_view->GetMutableAt(m_tool_bar_index)->m_item = nullptr;
    }
    // Drop the tab's view references before destroying projects so ~Project owns the last
    // view reference and destroys the view (and its ProjectSettings) while the project - and
    // its settings registry - is still alive (keeps ~ProjectSetting's unregister safe).
    if(m_tab_container)
    {
        m_tab_container->Clear();
    }
    m_projects.clear();

    // Release the profiler dialog and remote-trace orchestrator now so their
    // sessions transfer any in-flight work to the AppMonitor (non-blocking).
    // Subsequent Update() frames drain the monitor; the exit gate waits until it
    // is empty.
#ifdef ROCPROFVIS_ENABLE_PROFILER
    m_profiler_launcher_dialog.reset();
#endif
#ifdef ROCPROFVIS_ENABLE_REMOTE
    m_ssh_test_dialog.reset();
#endif

    if(!m_provider_cleanup_jobs.empty())
    {
        NotificationManager::GetInstance().ShowPersistent(
            APP_SHUTDOWN_NOTIFICATION_ID, "Closing traces...",
            NotificationLevel::Info);
    }

    RequestExitIfProviderCleanupsComplete();
}

void
AppWindow::DetachProjectProviderCleanup(Project& project, ProviderCleanupReason reason)
{
    std::shared_ptr<RootView> root_view =
        std::dynamic_pointer_cast<RootView>(project.GetView());
    if(!root_view)
    {
        return;
    }

    std::optional<DataProviderCleanupWork> cleanup_work =
        root_view->DetachProviderCleanup();
    if(cleanup_work)
    {
        StartProviderCleanup(std::move(*cleanup_work), project.GetName(), reason);
    }
}

void
AppWindow::StartProviderCleanup(DataProviderCleanupWork cleanup_work,
                                const std::string&    label,
                                ProviderCleanupReason reason)
{
    if(cleanup_work.requests.empty() && !cleanup_work.controller)
    {
        return;
    }

    const std::string cleanup_label =
        label.empty() ? cleanup_work.trace_file_path : label;
    ProviderCleanupJob job;
    job.label           = cleanup_label;
    job.reason          = reason;
    job.notification_id = "provider_cleanup_" +
                          std::to_string(++m_next_provider_cleanup_id);

    const std::string message =
        "Closing trace: " + cleanup_label + ", canceling " +
        std::to_string(cleanup_work.requests.size()) + " request(s)";
    NotificationManager::GetInstance().ShowPersistent(job.notification_id, message,
                                                      NotificationLevel::Info);

    job.future = std::async(
        std::launch::async,
        [cleanup_work = std::move(cleanup_work)]() mutable {
            return DataProvider::CleanupDetachedResources(std::move(cleanup_work));
        });
    m_provider_cleanup_jobs.push_back(std::move(job));
}

void
AppWindow::UpdateProviderCleanups()
{
    for(auto it = m_provider_cleanup_jobs.begin(); it != m_provider_cleanup_jobs.end();)
    {
        if(it->future.valid() &&
           it->future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            DataProviderCleanupResult result = it->future.get();
            spdlog::info("Provider cleanup completed for {} ({} request(s))",
                         result.trace_file_path.empty() ? it->label
                                                        : result.trace_file_path,
                         result.request_count);
            NotificationManager::GetInstance().Hide(it->notification_id);
            it = m_provider_cleanup_jobs.erase(it);
        }
        else
        {
            ++it;
        }
    }

    if(m_shutdown_requested)
    {
        if(m_provider_cleanup_jobs.empty())
        {
            NotificationManager::GetInstance().Hide(APP_SHUTDOWN_NOTIFICATION_ID);
        }
        RequestExitIfProviderCleanupsComplete();
    }
}

void
AppWindow::RequestExitIfProviderCleanupsComplete()
{
    if(!m_shutdown_requested || m_exit_notification_sent ||
       !m_provider_cleanup_jobs.empty())
    {
        return;
    }

    if(AppMonitor::GetInstance()->HasPendingOperations())
    {
        // The monitor drains non-blocking each shutdown frame. Bound the wait so
        // a stuck / never-resolving future cannot pin the app on the shutdown
        // screen forever. AppMonitor's destructor runs a final bounded,
        // cancelling drain (kShutdownDrainTimeoutSeconds) as the backstop.
        if(std::chrono::steady_clock::now() - m_shutdown_start <
           MONITOR_SHUTDOWN_GRACE_PERIOD)
        {
            return;
        }
        spdlog::warn("AppWindow: {} monitored operation(s) still pending after shutdown "
                     "grace period; exiting anyway",
                     AppMonitor::GetInstance()->GetActiveOperationCount());
    }

    m_exit_notification_sent = true;
    m_disable_app_interaction = false;
    if(m_notification_callback)
    {
        m_notification_callback(
            rocprofvis_view_notification_t::kRocProfVisViewNotification_Exit_App);
    }
}

void
AppWindow::Update()
{
    RenderScheduler::GetInstance().BeginFrame();

#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
    UpdateNativeFileDialog();
#endif
    UpdateProviderCleanups();
    if(m_shutdown_requested)
    {
        // Keep draining cancelling/in-flight monitored operations so their
        // resources are freed (non-blocking) before the app exits.
        AppMonitor::GetInstance()->Update();
        return;
    }

    HotkeyManager::GetInstance().ProcessInput();
    SettingsManager::GetInstance().GetFontManager().Update();
    // Poll long-running operations (profiler sessions, SSH) and queue any
    // status-change events before they are dispatched below this frame.
    AppMonitor::GetInstance()->Update();
    EventManager::GetInstance()->DispatchEvents();
    LogViewer::GetInstance()->Poll();
    DebugWindow::GetInstance()->ClearTransient();
    m_tab_container->Update();
#ifdef ROCPROFVIS_ENABLE_PROFILER
    if (m_profiler_launcher_dialog)
    {
        m_profiler_launcher_dialog->Update();
    }
#endif
#ifdef ROCPROFVIS_DEVELOPER_MODE
    m_test_data_provider.Update();
#endif
    UpdateStatusBar();
}

bool
AppWindow::WantsContinuousRender()
{
    // Animations and render-driven work push a frame request from their own
    // Update()/Render() via RenderScheduler, so no per-feature branch is needed
    // here.
    if(RenderScheduler::GetInstance().WantsRender())
    {
        return true;
    }

    if(!m_provider_cleanup_jobs.empty() || m_disable_app_interaction ||
       m_shutdown_requested || EventManager::GetInstance()->HasPendingEvents())
    {
        return true;
    }

#ifdef ROCPROFVIS_DEVELOPER_MODE
    if(m_test_data_provider.GetState() == ProviderState::kLoading ||
       m_test_data_provider.GetPendingRequestCount() > 0)
    {
        return true;
    }
#endif

    // Polled across every tab (not just the active one) so background loads keep
    // progressing. kLoading spans the whole load even when the pending count
    // briefly hits zero between stages, so we never freeze mid-load.
    bool wants_render = false;
    for(const auto& [id, project] : m_projects)
    {
        RootView* root_view = dynamic_cast<RootView*>(project->GetView().get());
        if(root_view)
        {
            DataProvider* data_provider = root_view->GetDataProvider();
            if(data_provider &&
               (data_provider->GetState() == ProviderState::kLoading ||
                data_provider->GetPendingRequestCount() > 0))
            {
                wants_render = true;
                break;
            }
        }
    }
    return wants_render;
}

void
AppWindow::Render()
{
    Update();

#ifdef ROCPROFVIS_ENABLE_INTERNAL_BANNER
    DrawInternalBuildBanner("Evaluation Build");
#endif
#ifdef IMGUI_HAS_VIEWPORT
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
#else
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
#endif

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    ImGui::Begin("Main Window", nullptr,
                 ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14, m_default_spacing.y));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 6));
    if(ImGui::BeginMenuBar())
    {
        Project* project = GetCurrentProject();
        RenderFileMenu(project);
        RenderEditMenu(project);
        RenderViewMenu(project);
        RenderHelpMenu();
#ifdef ROCPROFVIS_DEVELOPER_MODE
        RenderDeveloperMenu();
#endif
        ImGui::EndMenuBar();
    }
    ImGui::PopStyleVar(3);  // ItemSpacing, WindowPadding, FramePadding

    if(m_main_view)
    {
        m_main_view->Render();
    }

    if(m_open_about_dialog)
    {
        ImGui::OpenPopup(ABOUT_DIALOG_NAME);
        m_open_about_dialog = false;  // Reset the flag after opening the dialog
    }
    RenderAboutDialog();  // Popup dialogs need to be rendered as part of the main window
#ifdef ROCPROFVIS_ENABLE_REMOTE
    if(m_ssh_test_dialog)
    {
        m_ssh_test_dialog->Render();
    }
#endif
    m_confirmation_dialog->Render();
    m_message_dialog->Render();
    m_compare_files_dialog->Render();
    m_settings_panel->Render();
#ifdef ROCPROFVIS_ENABLE_PROFILER
    if (m_profiler_launcher_dialog)
    {
        m_profiler_launcher_dialog->Render();
    }
#endif

    ImGui::End();
    // Pop ImGuiStyleVar_ItemSpacing, ImGuiStyleVar_WindowPadding,
    // ImGuiStyleVar_WindowRounding
    ImGui::PopStyleVar(3);

    RenderFileDialog();

    LogViewer::GetInstance()->Render();
#ifdef ROCPROFVIS_DEVELOPER_MODE
    RenderDebugOuput();
#endif

#ifdef ROCPROFVIS_ENABLE_REMOTE
    // Centralized SSH auth prompts: draw the blocking prompt / host-key modal
    // for every live session (including connections owned privately by widgets
    // such as the remote file browser), so no session can wedge its worker
    // waiting on a prompt that no dialog happens to render.
    RenderSshAuthModals();
#endif

    // render notifications last
    NotificationManager::GetInstance().Render();

    RenderDisableScreen();
}

void
AppWindow::RenderShutdownState()
{
    ImGui::OpenPopup(SHUTDOWN_DIALOG_NAME);

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
               viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
        ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_Always);

    PopUpStyle ps;
    ps.PushPopupStyles();
    ps.CenterPopup();
    if(ImGui::BeginPopupModal(SHUTDOWN_DIALOG_NAME, nullptr,
                              ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoCollapse))
    {
        const size_t cleanup_jobs = m_provider_cleanup_jobs.size();
        const size_t monitor_ops  = AppMonitor::GetInstance()->GetActiveOperationCount();

        if(cleanup_jobs == 0 && monitor_ops == 0)
        {
            CenterNextTextItem(CLOSING_MESSAGE);
            ImGui::TextUnformatted(CLOSING_MESSAGE);
        }
        else
        {
            CenterNextTextItem(CLEANUP_MESSAGE);
            ImGui::TextUnformatted(CLEANUP_MESSAGE);
            ImGui::Spacing();
            // Draw indicator dots to show that the app is still responsive
            RenderLoadingIndicator(SettingsManager::GetInstance().GetColor(Colors::kTextMain),
                                   nullptr, kCenterHorizontal);
            ImGui::Spacing();
            if(cleanup_jobs > 0)
            {
                const std::string remaining_message =
                    "Cleanup jobs remaining: " + std::to_string(cleanup_jobs);
                CenterNextTextItem(remaining_message.c_str());
                ImGui::TextUnformatted(remaining_message.c_str());
            }
            if(monitor_ops > 0)
            {
                const std::string ops_message =
                    "Background operations remaining: " + std::to_string(monitor_ops);
                CenterNextTextItem(ops_message.c_str());
                ImGui::TextUnformatted(ops_message.c_str());
            }
        }
        ImGui::EndPopup();
    }
    ps.PopStyles();
}

void
AppWindow::RenderFileDialog()
{
    if(!ImGuiFileDialog::Instance()->IsOpened(FILE_DIALOG_NAME))
    {
        return;  // No file dialog is opened, nothing to render
    }

    // Set Itemspacing to values from original default ImGui style
    // custom values to break the 3rd party file dialog implementation
    // especially the cell padding
    auto defaultStyle = SettingsManager::GetInstance().GetDefaultIMGUIStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, defaultStyle.ItemSpacing);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, defaultStyle.WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, defaultStyle.CellPadding);

    if(m_init_file_dialog)
    {
        // Basically ImGuiCond_Appearing, except overwrite confirmation is a popup
        // ontop of dialog which triggers ImGuiCond_Appearing, thus flag cannot be used.
        ImGui::SetNextWindowPos(
            ImVec2(m_default_spacing.x, m_default_spacing.y + ImGui::GetFrameHeight()));
        ImGui::SetNextWindowSize(FILE_DIALOG_SIZE);
        m_init_file_dialog = false;
    }

    if(ImGuiFileDialog::Instance()->Display(FILE_DIALOG_NAME))
    {
        if(ImGuiFileDialog::Instance()->IsOk())
        {
            if(m_file_dialog_is_multi)
            {
                std::vector<std::string> paths;
                const std::string        current_dir =
                    ImGuiFileDialog::Instance()->GetCurrentPath();
                for(const auto& entry : ImGuiFileDialog::Instance()->GetSelection())
                {
                    // GetSelection() maps fileName -> filePathName (full path). Fall back
                    // to current_dir/fileName if the full path was not provided.
                    std::string full = entry.second.empty()
                                           ? (std::filesystem::path(current_dir) /
                                              entry.first)
                                                 .string()
                                           : entry.second;
                    paths.push_back(std::filesystem::path(full).string());
                }
                if(m_files_dialog_callback)
                {
                    m_files_dialog_callback(paths);
                }
            }
            else
            {
                // Directory mode reports its result via GetCurrentPath();
                // GetFilePathName() is empty in that case.
                const std::string result =
                    m_imgui_file_dialog_folder_mode
                        ? ImGuiFileDialog::Instance()->GetCurrentPath()
                        : ImGuiFileDialog::Instance()->GetFilePathName();
                if(m_file_dialog_callback)
                {
                    m_file_dialog_callback(std::filesystem::path(result).string());
                }
            }
        }
        m_file_dialog_is_multi = false;
        ImGuiFileDialog::Instance()->Close();
    }
    ImGui::PopStyleVar(3);
}

void
AppWindow::OpenFile(std::string file_path)
{
    // Merged/compare views live in Recents as their synthetic id (no file on disk); rebuild the
    // view instead of trying to open the id as a path.
    if(file_path.rfind("combined://", 0) == 0)
    {
        OpenCombined(SettingsManager::ParseRecentFileList(file_path));
        return;
    }
    if(file_path.rfind("compare://", 0) == 0)
    {
        OpenCompare(SettingsManager::ParseRecentFileList(file_path));
        return;
    }

    // While the Compare dialog is up, dropped/opened files fill its slots rather than
    // opening standalone trace tabs behind the modal.
    if(m_compare_files_dialog->IsOpen())
    {
        m_compare_files_dialog->AddDroppedFile(file_path);
        return;
    }

    spdlog::info("Opening file: {}", file_path);

    std::unique_ptr<Project> project = std::make_unique<Project>();
    switch(project->Open(file_path))
    {
        case Project::OpenResult::Success:
        {
            RegisterAndActivateProject(std::move(project));
            SettingsManager::GetInstance().AddRecentFile(file_path);
            break;
        }
        case Project::OpenResult::Duplicate:
        {
            // trace already open, tell the user which tab and switch to it
            Project* existing = GetProject(file_path);
            ShowMessageDialog("Trace Already Open",
                              "This trace is already open in \"" +
                                  (existing ? existing->GetName() : file_path) +
                                  "\".\n\nSwitched to the existing tab.");
            m_tab_container->SetActiveTab(file_path);
            break;
        }
        default:
        {
            SettingsManager::GetInstance().RemoveRecentFile(file_path);
            break;
        }
    }
}

std::string
AppWindow::JoinFileListId(const char* scheme, const std::vector<std::string>& files)
{
    std::string id = scheme;
    for(size_t i = 0; i < files.size(); i++)
    {
        if(i > 0)
        {
            id += "|";
        }
        id += files[i];
    }
    return id;
}

void
AppWindow::RegisterAndActivateProject(std::unique_ptr<Project> project)
{
    const std::string id = project->GetID();
    m_tab_container->AddTab(TabItem{ project->GetName(), id, project->GetView(), true });
    m_tab_container->SetActiveTab(id);
    m_projects[id] = std::move(project);
}

std::string
AppWindow::MakeCompareId(const std::vector<std::string>& files)
{
    return JoinFileListId("compare://", files);
}

void
AppWindow::OpenCompare(const std::string& first_file, const std::string& second_file)
{
    OpenCompare(std::vector<std::string>{ first_file, second_file });
}

void
AppWindow::OpenCompare(const std::vector<std::string>& files)
{
    if(files.size() < 2)
    {
        return;
    }

    spdlog::info("Opening compare view of {} traces", files.size());

    // Synthetic, deterministic project id so the compare tab has a stable identity
    // without a file on disk (the traces are loaded directly by the controller).
    const std::string compare_id = MakeCompareId(files);
    if(GetProject(compare_id))
    {
        m_tab_container->SetActiveTab(compare_id);
        return;
    }

    std::unique_ptr<Project> project = std::make_unique<Project>();
    if(project->OpenCompare(compare_id, files) == Project::OpenResult::Success)
    {
        RegisterAndActivateProject(std::move(project));
    }
}

std::string
AppWindow::MakeCombinedId(const std::vector<std::string>& files)
{
    return JoinFileListId("combined://", files);
}

void
AppWindow::OpenCombined(const std::vector<std::string>& files)
{
    if(files.empty())
    {
        return;
    }
    if(files.size() == 1)
    {
        OpenFile(files.front());
        return;
    }

    spdlog::info("Opening merged view of {} traces", files.size());

    // Synthetic, deterministic project id so the merged tab has a stable identity without
    // a file on disk (the traces are loaded directly by the controller).
    const std::string combined_id = MakeCombinedId(files);
    if(GetProject(combined_id))
    {
        m_tab_container->SetActiveTab(combined_id);
        return;
    }

    std::unique_ptr<Project> project = std::make_unique<Project>();
    if(project->OpenCombined(combined_id, files) == Project::OpenResult::Success)
    {
        RegisterAndActivateProject(std::move(project));
    }
}

void
AppWindow::RemoveTraceFromView(const std::string& file_to_remove)
{
    Project* project = GetCurrentProject();
    if(project == nullptr || project->GetTraceType() != Project::System ||
       project->IsCompare())
    {
        return;
    }

    const std::vector<std::string> files = project->GetSourceFiles();
    // Need at least 2 files to remove one, and the file must actually be part of this view.
    if(files.size() < 2 ||
       std::find(files.begin(), files.end(), file_to_remove) == files.end())
    {
        return;
    }

    // In-place remove: drop the file from the SAME session/controller, no new tab. The
    // project is retained, so per-track settings persist automatically (they are keyed by
    // track id, which is stable for the surviving tracks) - no reopen, no settings carry-over.
    TraceView* trace_view = dynamic_cast<TraceView*>(project->GetView().get());
    DataProvider* provider = trace_view ? trace_view->GetDataProvider() : nullptr;
    if(provider)
    {
        // Commit the source-list/tab change only when the async remove actually succeeds.
        WireSourceMutationCallback(provider, project->GetID());
    }
    if(!provider || !provider->RemoveTraceSource(file_to_remove))
    {
        ShowMessageDialog(
            "Remove Trace from View",
            "Could not remove the trace. The current view must be idle.");
    }
}

void
AppWindow::AddTraceToCurrentView()
{
    Project* project = GetCurrentProject();
    if(project == nullptr || project->GetTraceType() != Project::System ||
       project->IsCompare())
    {
        ShowMessageDialog(
            "Add Trace to View",
            "Open a system trace first, then add another trace to merge it into the "
            "same view.");
        return;
    }

    // Capture the tab id so the async dialog callback can re-resolve the (still-open)
    // project and inject the picked file into the SAME view via the incremental controller
    // path - no new tab, existing tracks + view state preserved.
    const std::string project_id = project->GetID();

    std::vector<FileFilter> file_filters;
    FileFilter              trace_filter;
    trace_filter.m_name       = "Traces";
    trace_filter.m_extensions = COMPARE_EXTENSIONS;
    file_filters.push_back(trace_filter);

    ShowOpenFileDialog(
        "Add Trace to View", file_filters, "",
        [this, project_id](std::string file_path) -> void {
            if(file_path.empty())
            {
                return;
            }
            Project* current = GetProject(project_id);
            if(current == nullptr)
            {
                return;
            }
            // One view per trace file: refuse a file already shown in this or another view.
            if(Project* owner = FindProjectContainingSource(file_path))
            {
                ShowMessageDialog(
                    "Add Trace to View",
                    owner == current
                        ? "That trace is already in this view."
                        : "That trace is already open in \"" + owner->GetName() +
                              "\". Close it there first, then add it here.");
                return;
            }
            TraceView* trace_view = dynamic_cast<TraceView*>(current->GetView().get());
            DataProvider* provider =
                trace_view ? trace_view->GetDataProvider() : nullptr;
            if(provider)
            {
                // Commit the source-list/tab change only when the async add actually succeeds.
                WireSourceMutationCallback(provider, current->GetID());
            }
            if(!provider || !provider->AddTraceSource(file_path))
            {
                ShowMessageDialog(
                    "Add Trace to View",
                    "Could not add the trace. It must be a compatible rocprof .db of the "
                    "same schema, and the current view must be idle.");
            }
        });
}

void
AppWindow::WireSourceMutationCallback(DataProvider* provider, const std::string& project_id)
{
    provider->SetSourceMutationDoneCallback(
        [this, project_id](const std::string& path, bool is_add, bool success) {
            if(success)
            {
                if(Project* project = GetProject(project_id))
                {
                    if(is_add)
                    {
                        project->AddSourceFile(path);
                    }
                    else
                    {
                        project->RemoveSourceFile(path);
                    }
                }
            }
            else
            {
                ShowMessageDialog(is_add ? "Add Trace to View" : "Remove Trace from View",
                                  is_add ? "Could not add the trace. It must be a compatible "
                                           "rocprof .db of the same schema."
                                         : "Could not remove the trace from the view.");
            }
        });
}

void
AppWindow::RenderDisableScreen()
{
    if(m_shutdown_requested)
    {
        return;
    }

    if(m_disable_app_interaction)
    {
        ImGui::OpenPopup("GhostModal");
    }

    // Use a modal popup to disable interaction with the rest of the UI
    if(ImGui::IsPopupOpen("GhostModal"))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        if(ImGui::BeginPopupModal("GhostModal", NULL,
                                  ImGuiWindowFlags_NoDecoration |
                                      ImGuiWindowFlags_NoBackground))
        {
            ImGui::SetWindowSize(ImVec2(1, 1));  // As small as possible
            if(!m_disable_app_interaction)
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);
    }
}

void
AppWindow::RenderFileMenu(Project* project)
{
    bool is_open_file_dialog_open = ImGuiFileDialog::Instance()->IsOpened(FILE_DIALOG_NAME);
#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
    is_open_file_dialog_open = is_open_file_dialog_open || m_is_native_file_dialog_open.load();
#endif

    if(ImGui::BeginMenu("File"))
    {
        if(ImGui::MenuItem("Open", nullptr, false, !is_open_file_dialog_open))
        {
            HandleOpenFile();
        }
        // Add/Remove Trace apply to single and merged system traces, but NOT to compare
        // projects: reopening a subset would drop the A/B/... compare tagging.
        const bool can_add_trace = project != nullptr &&
                                   project->GetTraceType() == Project::System &&
                                   !project->IsCompare();
        if(ImGui::MenuItem("Add Trace to View...", nullptr, false,
                           can_add_trace && !is_open_file_dialog_open))
        {
            AddTraceToCurrentView();
        }
        const std::vector<std::string> view_sources =
            can_add_trace ? project->GetSourceFiles() : std::vector<std::string>{};
        if(ImGui::BeginMenu("Remove Trace from View",
                            view_sources.size() >= 2 && !is_open_file_dialog_open))
        {
            for(const std::string& source : view_sources)
            {
                std::string label = std::filesystem::path(source).filename().string();
                if(ImGui::MenuItem(label.c_str()))
                {
                    RemoveTraceFromView(source);
                    break;
                }
            }
            ImGui::EndMenu();
        }
#ifdef ROCPROFVIS_DEVELOPER_MODE
        if(ImGui::MenuItem("Compare", nullptr, false, !is_open_file_dialog_open))
        {
            HandleCompareFiles();
        }
#endif
        if(ImGui::MenuItem("Save", nullptr, false,
                           !is_open_file_dialog_open && (project && project->IsProject())))
        {
            project->Save();
        }
        if(ImGui::MenuItem("Save As", nullptr, false,
                           project && project->GetTraceType() == Project::System &&
                               !is_open_file_dialog_open))
        {
            HandleSaveAsFile();
        }
        
#ifdef ROCPROFVIS_ENABLE_PROFILER
        // TEMPORARY (profiler launch): remove guard when the feature graduates.
        if(ImGui::MenuItem("Launch Profiler..."))
        {
            ShowProfilerLauncher();
        }
#endif
        ImGui::Separator();
        {
            TraceView* trace_view = nullptr;
            bool       has_trace  = false;
            bool       cleanup_pending = false;
            if(project && project->GetTraceType() == Project::System)
            {
                trace_view = dynamic_cast<TraceView*>(project->GetView().get());
                has_trace  = (trace_view != nullptr);
                if(has_trace)
                {
                    cleanup_pending = trace_view->IsCleanupPending();
                }
            }

            std::string project_id = has_trace ? project->GetID() : "";
            auto start_cleanup = [this, trace_view, project_id](bool rebuild) {
                trace_view->CleanupDatabase(rebuild, [this, project_id]() {
                    m_tab_container->RemoveTab(project_id);
                });
            };

            bool submenu_enabled = has_trace && !cleanup_pending;
            if(ImGui::BeginMenu("Database", submenu_enabled))
            {
#ifdef ROCPROFVIS_DEVELOPER_MODE
                if(ImGui::MenuItem("Fast Cleanup"))
                {
                    start_cleanup(false);
                }
#endif
                if(ImGui::MenuItem("Full Cleanup"))
                {
                    ShowConfirmationDialog(
                        "Full Database Cleanup",
                        "This will remove all Optiq metadata present in the database, "
                        "including service tables, indexes, and rebuild "
                        "(VACUUM) the database file. This may take a while.\n\n"
                        "Continue?",
                        [start_cleanup]() { start_cleanup(true); });
                }
                ImGui::EndMenu();
            }
        }
        
        ImGui::Separator();
        const std::list<std::string>& recent_files =
            SettingsManager::GetInstance().GetInternalSettings().recent_files;
        if(ImGui::BeginMenu("Recent Files", !recent_files.empty()))
        {
            for(std::string file : recent_files)
            {
                ImGui::PushID(file.c_str());
                const bool clicked =
                    ImGui::MenuItem(SettingsManager::RecentDisplayName(file).c_str(), nullptr);
                ImGui::PopID();
                if(clicked)
                {
                    OpenFile(file);
                    break;
                }
            }
            ImGui::Separator();
            if(ImGui::MenuItem("Clear Recent Files"))
            {
                SettingsManager::GetInstance().ClearRecentFiles();
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if(ImGui::MenuItem("Exit"))
        {
            ShowCloseConfirm();
        }
        ImGui::EndMenu();
    }
}

void
AppWindow::RenderEditMenu(Project* project)
{
    if(ImGui::BeginMenu("Edit"))
    {
        if(project)
        {
            std::shared_ptr<RootView> root_view =
                std::dynamic_pointer_cast<RootView>(project->GetView());
            if(root_view)
            {
                root_view->RenderEditMenuOptions();
            }
        }
        if(ImGui::MenuItem("Preferences"))
        {
            m_settings_panel->Show();
        }
        ImGui::EndMenu();
    }
}

void
AppWindow::RenderViewMenu(Project* project)
{
    (void) project;

    if(ImGui::BeginMenu("View"))
    {
        AppWindowSettings& settings =
            SettingsManager::GetInstance().GetAppWindowSettings();
        if(ImGui::MenuItem("Show Tool Bar", nullptr, &settings.show_toolbar))
        {
            LayoutItem* tool_bar_item = m_main_view->GetMutableAt(m_tool_bar_index);
            if(tool_bar_item)
            {
                tool_bar_item->m_visible = settings.show_toolbar;
            }
        }
#ifndef __APPLE__
        if(ImGui::MenuItem("Fullscreen", "F11", m_is_fullscreen))
        {
            if(m_notification_callback)
            {
                m_notification_callback(
                    rocprofvis_view_notification_t::
                        kRocProfVisViewNotification_Toggle_Fullscreen);
            }
        }
#endif
        ImGui::SeparatorText("System Profiler Panels");
        if(ImGui::MenuItem("Show Advanced Details Panel", nullptr,
                           &settings.show_details_panel))
        {
            for(const auto& tab : m_tab_container->GetTabs())
            {
                auto trace_view_tab =
                    std::dynamic_pointer_cast<RocProfVis::View::TraceView>(tab->m_widget);
                if(trace_view_tab)
                    trace_view_tab->SetAnalysisViewVisibility(
                        settings.show_details_panel);
            }
        }
        if(ImGui::MenuItem("Show System Topology Panel", nullptr, &settings.show_sidebar))
        {
            for(const auto& tab : m_tab_container->GetTabs())
            {
                auto trace_view_tab =
                    std::dynamic_pointer_cast<RocProfVis::View::TraceView>(tab->m_widget);
                if(trace_view_tab)
                    trace_view_tab->SetSidebarViewVisibility(settings.show_sidebar);
            }
        }
        if(ImGui::MenuItem("Show Histogram", nullptr, &settings.show_histogram))
        {
            for(const auto& tab : m_tab_container->GetTabs())
            {
                auto trace_view_tab =
                    std::dynamic_pointer_cast<RocProfVis::View::TraceView>(tab->m_widget);
                if(trace_view_tab)
                    trace_view_tab->SetHistogramVisibility(settings.show_histogram);
            }
        }
        ImGui::MenuItem("Show Summary", nullptr, &settings.show_summary);

        ImGui::Separator();
        ImGui::MenuItem("Show Log Viewer", nullptr,
                        LogViewer::GetInstance()->VisiblePtr());
        ImGui::EndMenu();
    }
}

void
AppWindow::RenderHelpMenu()
{
    if(ImGui::BeginMenu("Help"))
    {
        if(ImGui::MenuItem("About"))
        {
            m_open_about_dialog = true;
        }
        ImGui::EndMenu();
    }
}

void
AppWindow::HandleOpenFile()
{
    std::vector<FileFilter> file_filters;

    FileFilter all_filter;
    all_filter.m_name       = "All Supported";
    all_filter.m_extensions = ALL_EXTENSIONS;

    FileFilter trace_filter;
    trace_filter.m_name       = "Traces";
    trace_filter.m_extensions = TRACE_EXTENSIONS;

    FileFilter project_filter;
    project_filter.m_name       = "Projects";
    project_filter.m_extensions = PROJECT_EXTENSIONS;

    file_filters.push_back(all_filter);
    file_filters.push_back(trace_filter);
    file_filters.push_back(project_filter);

    ShowOpenFilesDialog(
        "Choose File(s)", file_filters, "",
        [this](const std::vector<std::string>& files) -> void {
            if(files.empty())
            {
                return;
            }
            // Projects (.rpv) open individually; multiple selected traces merge into one
            // unified view (like a yaml manifest), while a single trace opens on its own.
            std::vector<std::string> traces;
            for(const std::string& file : files)
            {
                std::string ext = std::filesystem::path(file).extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(),
                               [](unsigned char c) {
                                   return static_cast<char>(std::tolower(c));
                               });
                if(ext == ".rpv")
                {
                    OpenFile(file);
                }
                else
                {
                    traces.push_back(file);
                }
            }
            if(traces.size() == 1)
            {
                OpenFile(traces.front());
            }
            else if(traces.size() >= 2)
            {
                OpenCombined(traces);
            }
        });
}

void
AppWindow::HandleCompareFiles()
{
    m_compare_files_dialog->Show();
}

void
AppWindow::HandleCompareFileBrowse(CompareFilesDialog::FileSlot slot)
{
    std::vector<FileFilter> file_filters;

    FileFilter trace_filter;
    trace_filter.m_name       = "Trace Files";
    trace_filter.m_extensions = COMPARE_EXTENSIONS;
    file_filters.push_back(trace_filter);

    ShowOpenFileDialog(
        "Choose Trace", file_filters, "",
        [this, slot](std::string file_path) -> void {
            m_compare_files_dialog->SetFilePath(slot, file_path);
        });
}

void
AppWindow::HandleSaveAsFile()
{    
    Project* project = GetCurrentProject();
    if(project)
    {
        FileFilter trace_filter;
        trace_filter.m_name = "Projects";
        trace_filter.m_extensions = { "rpv" };

        std::vector<FileFilter> filters;
        filters.push_back(trace_filter);

        ShowSaveFileDialog(
            "Save as Project", filters, "",
            [project](std::string file_path) { project->SaveAs(file_path); });
    }
}

void
AppWindow::HandleTabClosed(std::shared_ptr<RocEvent> e)
{
    auto tab_closed_event = std::dynamic_pointer_cast<TabEvent>(e);
    auto project_it =
        tab_closed_event ? m_projects.find(tab_closed_event->GetTabId())
                         : m_projects.end();
    if(tab_closed_event && project_it != m_projects.end())
    {
        auto activeProject = GetCurrentProject();
        if(!activeProject)
        {
            spdlog::debug("No active project found after tab closed");
            m_main_view->GetMutableAt(m_tool_bar_index)->m_item = nullptr;
        }
        else
        {
            spdlog::debug("Active project found after tab closed: {}",
                          activeProject->GetName());
            std::shared_ptr<RootView> root_view =
                std::dynamic_pointer_cast<RootView>(activeProject->GetView());
            if(root_view)
            {
                m_main_view->GetMutableAt(m_tool_bar_index)->m_item =
                    root_view->GetToolbar();
            }
        }
        // A merged view has no file on disk; record its synthetic id in Recents on close so it can
        // be reopened (single traces are recorded on open, .rpv projects on save).
        Project* closing = project_it->second.get();
        if(closing && !closing->IsProject() && !closing->IsCompare() &&
           closing->GetTraceType() == Project::System)
        {
            const std::vector<std::string> sources = closing->GetSourceFiles();
            if(sources.size() >= 2)
            {
                SettingsManager::GetInstance().AddRecentFile(MakeCombinedId(sources));
            }
        }
        spdlog::debug("Tab closed: {}", tab_closed_event->GetTabId());
        project_it->second->Close();
        DetachProjectProviderCleanup(*project_it->second,
                                     ProviderCleanupReason::kTabClose);
        m_projects.erase(project_it);
    }
}

void
AppWindow::HandleTabSelectionChanged(std::shared_ptr<RocEvent> e)
{
    auto tab_selected_event = std::dynamic_pointer_cast<TabEvent>(e);
    if(tab_selected_event)
    {
        // Only handle the event if the tab source is the main tab source
        if(tab_selected_event->GetSourceId() == GetMainTabSourceName())
        {
            m_main_view->GetMutableAt(m_tool_bar_index)->m_item = nullptr;

            auto id = tab_selected_event->GetTabId();
            spdlog::debug("Tab selected: {}", id);
            auto project = GetProject(id);
            if(!project)
            {
                spdlog::warn("Project not found for tab: {}", id);
                return;
            }
            else
            {
                std::shared_ptr<RootView> root_view =
                    std::dynamic_pointer_cast<RootView>(project->GetView());
                if(root_view)
                {
                    m_main_view->GetMutableAt(m_tool_bar_index)->m_item =
                        root_view->GetToolbar();
                }
            }
        }
    }
}

void
AppWindow::HandleFontChanged()
{
    // Update status bar height based on new font size
    int count = static_cast<int>(m_main_view->ItemCount());

    // status bar (assume as the last item)
    auto status_bar_item = m_main_view->GetMutableAt(count - 1);
    if(!status_bar_item)
    {
        return;
    }

    // Size the slot to one framed text line plus the child border so the
    // status bar content does not overflow into a scrollbar.
    const ImGuiStyle& default_style = SettingsManager::GetInstance().GetDefaultStyle();
    const float       content_height =
        ImGui::GetFontSize() + (default_style.FramePadding.y * 2.0f);
    const float border_height = (status_bar_item->m_window_padding.y * 2.0f) +
                                (ImGui::GetStyle().ChildBorderSize * 2.0f);
    status_bar_item->m_height = content_height + border_height;

    // adjust main view's size to account for new status bar height
    auto main_view_item = m_main_view->GetMutableAt(count - 2);
    if(!main_view_item)
    {
        return;
    }
    main_view_item->m_height = -status_bar_item->m_height;
}

void
AppWindow::RenderAboutDialog()
{
    static constexpr const char* NAME_LABEL = "ROCm (TM) Optiq";
    static constexpr const char* COPYRIGHT_LABEL =
        "Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.";
    static constexpr const char* DOC_LABEL = "ROCm (TM) Optiq Documentation";
    static constexpr const char* DOC_URL =
        "https://rocm.docs.amd.com/projects/roc-optiq/en/latest/";
    static const std::string VERSION_LABEL = []() {
        std::stringstream ss;
        ss << "Version " << ROCPROFVIS_VERSION_MAJOR << "." << ROCPROFVIS_VERSION_MINOR
           << "." << ROCPROFVIS_VERSION_PATCH;
        return ss.str();
    }();

    PopUpStyle popup_style;
    popup_style.PushPopupStyles();
    popup_style.PushTitlebarColors();
    popup_style.CenterPopup();

    ImGui::SetNextWindowSize(
        GetResponsiveWindowSize(ImVec2(580.0f, 0.0f), ImVec2(360.0f, 0.0f)));

    if(ImGui::BeginPopupModal(ABOUT_DIALOG_NAME, nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize |
                                  ImGuiWindowFlags_NoMove))
    {
        ImGui::PushFont(NULL, SettingsManager::GetInstance().GetFontManager().GetFontSize(
                                  FontSize::kLarge));

        ImGui::SetCursorPosX(
            (ImGui::GetWindowSize().x - ImGui::CalcTextSize(NAME_LABEL).x) * 0.5f);
        ImGui::TextUnformatted(NAME_LABEL);
        ImGui::PopFont();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::SetCursorPosX(
            (ImGui::GetWindowSize().x - ImGui::CalcTextSize(VERSION_LABEL.c_str()).x) *
            0.5f);
        ImGui::TextUnformatted(VERSION_LABEL.c_str());

        ImGui::Spacing();

        ImGui::SetCursorPosX(
            (ImGui::GetWindowSize().x - ImGui::CalcTextSize(COPYRIGHT_LABEL).x) * 0.5f);
        ImGui::TextUnformatted(COPYRIGHT_LABEL);

        ImGui::Spacing();

        ImGui::SetCursorPosX(
            (ImGui::GetWindowSize().x - ImGui::CalcTextSize(DOC_LABEL).x) * 0.5f);
        ImGui::TextLink(DOC_LABEL);
        if(ImGui::IsItemClicked())
        {
            open_url(DOC_URL);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float button_width =
            ImGui::CalcTextSize("Close").x + ImGui::GetStyle().FramePadding.x * 2;
        ImGui::SetCursorPosX(ImGui::GetWindowSize().x - button_width -
                             ImGui::GetStyle().ItemSpacing.x);
        if(ImGui::Button("Close"))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    popup_style.PopStyles();

 }

#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
void
AppWindow::UpdateNativeFileDialog()
{
    if(!m_is_native_file_dialog_open)
    {
        return;
    }

    bool completed = false;
    if(m_file_dialog_is_multi)
    {
        if(m_files_dialog_future.valid() &&
           m_files_dialog_future.wait_for(std::chrono::seconds(0)) ==
               std::future_status::ready)
        {
            m_disable_app_interaction         = false;
            std::vector<std::string> paths    = m_files_dialog_future.get();
            auto                     callback = m_files_dialog_callback;
            m_is_native_file_dialog_open      = false;
            m_files_dialog_callback           = nullptr;
            m_file_dialog_is_multi            = false;
            if(!paths.empty() && callback)
            {
                callback(paths);
            }
            completed = true;
        }
    }
    else if(m_file_dialog_future.valid() &&
            m_file_dialog_future.wait_for(std::chrono::seconds(0)) ==
                std::future_status::ready)
    {
        m_disable_app_interaction = false;
        std::string file_path     = m_file_dialog_future.get();
        if(!file_path.empty() && m_file_dialog_callback)
        {
            m_file_dialog_callback(file_path);
        }
        m_is_native_file_dialog_open = false;
        m_file_dialog_callback       = nullptr;
        completed                    = true;
    }

    if(completed && m_restore_fullscreen_later)
    {
        // toggle fullscreen on if it should be restored after dialog closes
        if(!m_is_fullscreen && m_notification_callback)
        {
            m_notification_callback(
                rocprofvis_view_notification_t::kRocProfVisViewNotification_Toggle_Fullscreen);
        }
        m_restore_fullscreen_later = false;
    }
}

// Build the NFD filter list into caller-owned vectors. items[i] points into extensions[i] and
// file_filters[i].m_name, so the caller must keep both (and file_filters) alive for the NFD call.
static void
BuildNfdFilters(const std::vector<FileFilter>&  file_filters,
                std::vector<std::string>&       extensions,
                std::vector<nfdu8filteritem_t>& items)
{
    extensions.reserve(file_filters.size());
    items.reserve(file_filters.size());
    for(size_t i = 0; i < file_filters.size(); ++i)
    {
        std::string extensions_str;
        for(size_t j = 0; j < file_filters[i].m_extensions.size(); ++j)
        {
            extensions_str += file_filters[i].m_extensions[j];
            if(j < file_filters[i].m_extensions.size() - 1)
            {
                extensions_str += ",";
            }
        }
        extensions.push_back(std::move(extensions_str));
    }
    // Build items after extensions is fully grown so extensions[i].c_str() stays valid.
    for(size_t i = 0; i < file_filters.size(); ++i)
    {
        items.push_back({ file_filters[i].m_name.c_str(), extensions[i].c_str() });
    }
}

void
AppWindow::ShowNativeFileDialog(const std::vector<FileFilter>&   file_filters,
                                const std::string&               initial_path,
                                std::function<void(std::string)> callback,
                                bool                             save_dialog,
                                bool                             path_picker)
{
    if(m_is_native_file_dialog_open)
    {
        return;
    }
    m_is_native_file_dialog_open = true;
    m_file_dialog_callback       = callback;
    m_disable_app_interaction    = true;

    if(m_is_fullscreen)
    {
        // toggle fullscreen off before opening native file dialog
        if(m_notification_callback)
        {
            m_restore_fullscreen_later = true;
            m_notification_callback(rocprofvis_view_notification_t::
                                        kRocProfVisViewNotification_Toggle_Fullscreen);
        }
    }

    auto dialog_task = [=]() -> std::string {
        nfdresult_t init_result = NFD_Init();
        if(init_result != NFD_OKAY)
        {
            const char* err = NFD_GetError();
            spdlog::error("NFD_Init failed at dialog open: {}",
                          err ? err : "unknown");
            NFD_ClearError();
            m_use_native_file_dialog.store(false);
            return std::string();
        }
        nfdu8char_t* outPath = nullptr;

        std::vector<std::string>       filter_extensions;
        std::vector<nfdu8filteritem_t> filter_items;
        BuildNfdFilters(file_filters, filter_extensions, filter_items);
        const nfdu8filteritem_t* filters =
            filter_items.empty() ? nullptr : filter_items.data();

        nfdresult_t result;
        if(path_picker)
        {
            nfdpickfolderu8args_t args = {};
            if(!initial_path.empty())
            {
                args.defaultPath = initial_path.c_str();
            }
            result = NFD_PickFolderU8_With(&outPath, &args);
        }
        else if(save_dialog)
        {
            nfdsavedialogu8args_t args = {};
            args.filterList            = filters;
            args.filterCount = static_cast<nfdfiltersize_t>(file_filters.size());
            if(!initial_path.empty())
            {
                args.defaultPath = initial_path.c_str();
            }
            result = NFD_SaveDialogU8_With(&outPath, &args);
        }
        else
        {
            nfdopendialogu8args_t args = {};
            args.filterList  = filters;
            args.filterCount = static_cast<nfdfiltersize_t>(file_filters.size());
            if(!initial_path.empty())
            {
                args.defaultPath = initial_path.c_str();
            }
            result = NFD_OpenDialogU8_With(&outPath, &args);
        }
        std::string file_path;
        if(result == NFD_OKAY)
        {
            file_path = outPath;
            if(outPath)
            {
                // Save dialog only: append default extension when the name has none (e.g. Linux save).
                // Open dialog must not do this — extensionless executables would get ".*/.exe" appended
                // from the filter list (e.g. "transpose" + "."" + ".*" -> "transpose..*").
                std::filesystem::path p(file_path);
                if(save_dialog && !path_picker && !file_filters.empty() && !p.has_extension())
                {
                    file_path += "." + file_filters[0].m_extensions[0];
                }

                NFD_FreePathU8(outPath);
            }
        }
        else
        {
            spdlog::error("Error opening dialog: {}", NFD_GetError());
            if(outPath)
            {
                NFD_FreePathU8(outPath);
            }
            NFD_ClearError();
        }
        NFD_Quit();
        return file_path;
    };

#if defined(__APPLE__)
    // NSOpenPanel / NSSavePanel are AppKit objects and must be driven from the
    // main thread. Run synchronously here and hand the result to the existing
    // future-polling path via a ready promise.
    std::promise<std::string> dialog_promise;
    dialog_promise.set_value(dialog_task());
    m_file_dialog_future = dialog_promise.get_future();
#else
    m_file_dialog_future = std::async(std::launch::async, std::move(dialog_task));
#endif
}

void
AppWindow::ShowNativeFilesDialog(
    const std::vector<FileFilter>& file_filters, const std::string& initial_path,
    std::function<void(const std::vector<std::string>&)> callback)
{
    if(m_is_native_file_dialog_open)
    {
        return;
    }
    m_is_native_file_dialog_open = true;
    m_files_dialog_callback      = std::move(callback);
    m_file_dialog_is_multi       = true;
    m_disable_app_interaction    = true;

    if(m_is_fullscreen)
    {
        // toggle fullscreen off before opening native file dialog
        if(m_notification_callback)
        {
            m_restore_fullscreen_later = true;
            m_notification_callback(rocprofvis_view_notification_t::
                                        kRocProfVisViewNotification_Toggle_Fullscreen);
        }
    }

    auto dialog_task = [=]() -> std::vector<std::string> {
        std::vector<std::string> results;
        nfdresult_t              init_result = NFD_Init();
        if(init_result != NFD_OKAY)
        {
            const char* err = NFD_GetError();
            spdlog::error("NFD_Init failed at dialog open: {}", err ? err : "unknown");
            NFD_ClearError();
            m_use_native_file_dialog.store(false);
            return results;
        }

        std::vector<std::string>       filter_extensions;
        std::vector<nfdu8filteritem_t> filter_items;
        BuildNfdFilters(file_filters, filter_extensions, filter_items);
        const nfdu8filteritem_t* filters =
            filter_items.empty() ? nullptr : filter_items.data();

        const nfdpathset_t*   path_set = nullptr;
        nfdopendialogu8args_t args     = {};
        args.filterList                = filters;
        args.filterCount = static_cast<nfdfiltersize_t>(file_filters.size());
        if(!initial_path.empty())
        {
            args.defaultPath = initial_path.c_str();
        }
        nfdresult_t result = NFD_OpenDialogMultipleU8_With(&path_set, &args);
        if(result == NFD_OKAY && path_set != nullptr)
        {
            nfdpathsetsize_t count = 0;
            NFD_PathSet_GetCount(path_set, &count);
            for(nfdpathsetsize_t i = 0; i < count; ++i)
            {
                nfdu8char_t* out_path = nullptr;
                if(NFD_PathSet_GetPathU8(path_set, i, &out_path) == NFD_OKAY &&
                   out_path != nullptr)
                {
                    results.emplace_back(out_path);
                    NFD_PathSet_FreePathU8(out_path);
                }
            }
            NFD_PathSet_Free(path_set);
        }
        else if(result == NFD_ERROR)
        {
            spdlog::error("Error opening dialog: {}", NFD_GetError());
            NFD_ClearError();
        }
        NFD_Quit();
        return results;
    };

#if defined(__APPLE__)
    std::promise<std::vector<std::string>> dialog_promise;
    dialog_promise.set_value(dialog_task());
    m_files_dialog_future = dialog_promise.get_future();
#else
    m_files_dialog_future = std::async(std::launch::async, std::move(dialog_task));
#endif
}

#endif

void
AppWindow::ShowImGuiFileDialog(const std::string& title, const std::vector<FileFilter>& file_filters,
                          const std::string& initial_path, const bool& confirm_overwrite,
                          std::function<void(std::string)> callback, bool folder_mode,
                          bool multi_select)
{
    m_file_dialog_callback          = callback;
    m_file_dialog_is_multi          = multi_select;
    m_init_file_dialog              = true;
    m_imgui_file_dialog_folder_mode = folder_mode;

    std::stringstream filter_stream;
    for(const auto& filter : file_filters)
    {
        std::stringstream extensions;
        for(size_t i = 0; i < filter.m_extensions.size(); ++i)
        {
            extensions << "." << filter.m_extensions[i];
            if(i < filter.m_extensions.size() - 1)
            {
                extensions << ",";
            }
        }

        filter_stream << filter.m_name << " (" << extensions.str() << "){"
                      << extensions.str() << "}";
        if(&filter != &file_filters.back())
        {
            filter_stream << ",";
        }
    }

    // An empty filter list leaves ImGuiFileDialog with no dLGFilters, which then hides
    // every regular file (directory-only mode). The regex form matches any file name,
    // including extensionless executables (Linux/macOS).
    std::string filter_string = filter_stream.str();
    if(filter_string.empty())
    {
        filter_string = "All files{((.*))}";
    }

    IGFD::FileDialogConfig config;
    config.path  = initial_path;
    // 0 == infinite selection; 1 == single file (the default single-select behavior).
    config.countSelectionMax = multi_select ? 0 : 1;
    config.flags = confirm_overwrite
                       ? ImGuiFileDialogFlags_Default
                       : ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_HideColumnType;
    // A nullptr filter switches ImGuiFileDialog into directory-selection mode.
    const char* filters = folder_mode ? nullptr : filter_string.c_str();
    ImGuiFileDialog::Instance()->OpenDialog(FILE_DIALOG_NAME, title, filters, config);
}

#if defined(ROCPROFVIS_DEVELOPER_MODE) && defined(ROCPROFVIS_ENABLE_REMOTE)

void
AppWindow::HandleTestRemoteSSH()
{
    if(!m_ssh_test_dialog)
    {
        m_ssh_test_dialog = std::make_unique<SshTestDialog>(this);
    }
    m_ssh_test_dialog->Show();
}

#endif // ROCPROFVIS_DEVELOPER_MODE && ROCPROFVIS_ENABLE_REMOTE
void
AppWindow::UpdateStatusBar()
{
    // Update status message every N frames to avoid rebuilding the string each
    // frame while background work is in flight.
    constexpr int STATUS_BAR_UPDATE_FRAME_STEP = 4;
    if(ImGui::GetFrameCount() % STATUS_BAR_UPDATE_FRAME_STEP == 0)
    {
        // Get number of pending requests from data provider
        size_t pending_requests = 0;
        for(const auto& [id, project] : m_projects)
        {
            auto root_view = dynamic_cast<RootView*>(project->GetView().get());
            if(root_view)
            {
                auto data_provider = root_view->GetDataProvider();
                if(data_provider)
                {
                    pending_requests += data_provider->GetPendingRequestCount();
                }
            }
        }
        // also check if there are any cleanup jobs pending
        size_t clean_up_jobs = m_provider_cleanup_jobs.size();
        // background operations tracked by the monitor (SSH, profiler, etc.)
        AppMonitor* monitor     = AppMonitor::GetInstance();
        size_t      monitor_ops = monitor->GetActiveOperationCount();

        // Live remote/SSH sessions (connections), including idle ones between
        // operations. Only meaningful when remote support is built.
        size_t remote_sessions = 0;
#ifdef ROCPROFVIS_ENABLE_REMOTE
        remote_sessions = SshSession::ActiveSessionCount();
#endif

        // In-flight work drives the busy spinner; an idle-but-connected SSH
        // session is surfaced without the spinner so the user knows a connection
        // is open without implying activity.
        bool has_active_work = (pending_requests > 0 || clean_up_jobs > 0 || monitor_ops > 0);

        std::vector<std::string> segments;
        if(pending_requests > 0)
        {
            segments.push_back("Working: " + std::to_string(pending_requests) +
                               " pending request(s)");
        }
        if(clean_up_jobs > 0)
        {
            segments.push_back("Cleaning up: " + std::to_string(clean_up_jobs) +
                               " pending job(s)");
        }
        if(monitor_ops > 0)
        {
            // Break the generic count down by domain (SSH / profiler) so the
            // user can tell what is keeping the app busy. The domain grouping
            // lives here (the caller), not in the generic AppMonitor.
            size_t remote_ops =
                monitor->GetActiveOperationCount(MonitorOperationType::SshConnection) +
                monitor->GetActiveOperationCount(MonitorOperationType::SshAuthentication) +
                monitor->GetActiveOperationCount(MonitorOperationType::FileTransfer) +
                monitor->GetActiveOperationCount(MonitorOperationType::DirectoryListing);
            size_t profiler_ops =
                monitor->GetActiveOperationCount(MonitorOperationType::ProfilerSession);
            std::string detail;
            if(remote_ops > 0)
            {
                detail = std::to_string(remote_ops) + " SSH";
            }
            if(profiler_ops > 0)
            {
                detail += (detail.empty() ? "" : ", ") +
                          std::to_string(profiler_ops) + " profiler";
            }
            segments.push_back("Background: " + std::to_string(monitor_ops) +
                               " operation(s)" +
                               (detail.empty() ? "" : " (" + detail + ")"));
        }
        if(remote_sessions > 0)
        {
            segments.push_back("SSH: " + std::to_string(remote_sessions) + " session(s)");
        }

        if(!segments.empty())
        {
            m_status_message.clear();
            for(size_t i = 0; i < segments.size(); ++i)
            {
                m_status_message += (i > 0 ? " | " : "") + segments[i];
            }
            m_status_show_busy_indicator = has_active_work;
        }
        else
        {
            m_status_message             = "Ready";
            m_status_show_busy_indicator = false;
        }
    }
}

void
AppWindow::RenderStatusBar()
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, style.ItemSpacing);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, style.FramePadding);

    ImGui::AlignTextToFramePadding();
    ImGui::Dummy(ImVec2(0.f, ImGui::GetFrameHeight()));
    ImGui::SameLine();
    if(m_status_show_busy_indicator)
    {
        float radius = ImGui::GetTextLineHeight() * 0.25f;
        RenderLoadingIndicator(settings.GetColor(Colors::kTextDim), nullptr,
                               kCenterVertical, radius);
        ImGui::SameLine(0.f, style.ItemSpacing.x);
    }
    ImGui::TextUnformatted(m_status_message.c_str());
    ImGui::PopStyleVar(2);
}

#ifdef ROCPROFVIS_DEVELOPER_MODE
void
AppWindow::RenderDeveloperMenu()
{
    if(ImGui::BeginMenu("Developer Options"))
    {
        // Toggele ImGui's built-in metrics window
        if(ImGui::MenuItem("Show Metrics", nullptr, m_show_metrics))
        {
            m_show_metrics = !m_show_metrics;
        }
        // Toggle debug output window
        if(ImGui::MenuItem("Show Debug Output Window", nullptr, m_show_debug_window))
        {
            m_show_debug_window = !m_show_debug_window;
            if(m_show_debug_window)
            {
                ImGui::SetWindowFocus("Debug Window");
            }
        }
        // Open a file to test the DataProvider
        if(ImGui::MenuItem("Test Provider", nullptr))
        {
            std::vector<FileFilter> file_filters;
            FileFilter trace_filter;
            trace_filter.m_name       = "Traces";
            trace_filter.m_extensions = { "db", "rpd" };
            file_filters.push_back(trace_filter);
            ShowOpenFileDialog("Choose File", file_filters, "",
                               [this](std::string file_path) -> void {
                                   std::string config_path = get_application_config_path(true);
                                    rocprofvis_controller_t*   controller = rocprofvis_controller_alloc(file_path.c_str(), config_path.c_str());
                                   if(controller)
                                   {
                                       this->m_test_data_provider.FetchTrace(controller, file_path);
                                       spdlog::info("Opening file: {}", file_path);
                                       m_show_provider_test_widow = true;
                                   }
                                   else
                                   {
                                       rocprofvis_controller_free(controller);
                                   }
                               });
        }
#ifdef ROCPROFVIS_ENABLE_REMOTE
        if(ImGui::MenuItem("Open Remote...", nullptr, false))
        {
            HandleTestRemoteSSH();
        }
#endif        
        ImGui::EndMenu();
    }
}

void
RenderProviderTest(DataProvider& provider)
{
    ImGui::Begin("Data Provider Test Window", nullptr, ImGuiWindowFlags_None);

    static char    track_index_buffer[64]     = "0";
    static char    end_track_index_buffer[64] = "1";  // for setting table track range
    static uint8_t group_id_counter           = 0;

    // Callback function to filter non-numeric characters
    auto NumericFilter = [](ImGuiInputTextCallbackData* data) -> int {
        if(data->EventChar < '0' || data->EventChar > '9')
        {
            // Allow backspace
            if(data->EventChar != '\b')
            {
                return 1;  // Block non-numeric characters
            }
        }
        return 0;  // Allow numeric characters
    };

    ImGui::InputText("Track index", track_index_buffer, IM_ARRAYSIZE(track_index_buffer),
                     ImGuiInputTextFlags_CallbackCharFilter, NumericFilter);

    int index = std::atoi(track_index_buffer);

    ImGui::Separator();
    ImGui::Text("Table Parameters");
    ImGui::InputText("End Track index", end_track_index_buffer,
                     IM_ARRAYSIZE(end_track_index_buffer),
                     ImGuiInputTextFlags_CallbackCharFilter, NumericFilter);

    static char row_start_buffer[64] = "-1";
    ImGui::InputText("Start Row", row_start_buffer, IM_ARRAYSIZE(row_start_buffer),
                     ImGuiInputTextFlags_CallbackCharFilter, NumericFilter);
    uint64_t start_row = std::atoi(row_start_buffer);

    static char row_count_buffer[64] = "-1";
    ImGui::InputText("Row Count", row_count_buffer, IM_ARRAYSIZE(row_count_buffer),
                     ImGuiInputTextFlags_CallbackCharFilter, NumericFilter);
    uint64_t row_count = std::atoi(row_count_buffer);

    TimelineModel& timeline = provider.DataModel().GetTimeline();
    if(ImGui::Button("Fetch Single Track Event Table"))
    {
        provider.FetchSingleTrackEventTable(index, timeline.GetStartTime(),
                                            timeline.GetEndTime(), "", "", "", start_row,
                                            row_count);
    }
    if(ImGui::Button("Fetch Multi Track Event Table"))
    {
        int                   end_index = std::atoi(end_track_index_buffer);
        std::vector<uint64_t> vect;
        for(int i = index; i < end_index; ++i)
        {
            vect.push_back(i);
        }
        provider.FetchMultiTrackEventTable(vect, timeline.GetStartTime(),
                                           timeline.GetEndTime(), "", "", "", start_row,
                                           row_count);
    }
    if(ImGui::Button("Print Event Table"))
    {
        provider.DataModel().GetTables().DumpTable(TableType::kEventTable);
    }

    if(ImGui::Button("Fetch Single Track Sample Table"))
    {
        provider.FetchSingleTrackSampleTable(index, timeline.GetStartTime(),
                                             timeline.GetEndTime(), "", start_row,
                                             row_count);
    }
    if(ImGui::Button("Fetch Multi Track Sample Table"))
    {
        int                   end_index = std::atoi(end_track_index_buffer);
        std::vector<uint64_t> vect;
        for(int i = index; i < end_index; ++i)
        {
            vect.push_back(i);
        }
        provider.FetchMultiTrackSampleTable(vect, timeline.GetStartTime(),
                                            timeline.GetEndTime(), "", start_row,
                                            row_count);
    }
    if(ImGui::Button("Print Sample Table"))
    {
        provider.DataModel().GetTables().DumpTable(TableType::kSampleTable);
    }

    ImGui::Separator();

    if(ImGui::Button("Fetch Track"))
    {
        provider.FetchTrack(index, timeline.GetStartTime(), timeline.GetEndTime(), 1000,
                            group_id_counter++);
    }

    if(ImGui::Button("Fetch Whole Track"))
    {
        provider.FetchWholeTrack(index, timeline.GetStartTime(), timeline.GetEndTime(),
                                 1000, group_id_counter++);
    }
    if(ImGui::Button("Delete Track"))
    {
        timeline.FreeTrackData(index);
    }
    if(ImGui::Button("Print Track"))
    {
        timeline.DumpTrack(index);
    }
    if(ImGui::Button("Print Track List"))
    {
        timeline.DumpMetaData();
    }

    ImGui::End();
}

void
AppWindow::RenderDebugOuput()
{
    if(m_show_metrics)
    {
        ImGui::ShowMetricsWindow(&m_show_metrics);
    }

    if(m_show_debug_window)
    {
        DebugWindow::GetInstance()->Render();
    }

    if(m_show_provider_test_widow)
    {
        RenderProviderTest(m_test_data_provider);
    }
}
#endif  // ROCPROFVIS_DEVELOPER_MODE

#ifdef ROCPROFVIS_ENABLE_PROFILER
// TEMPORARY (profiler launch): remove guard when the feature graduates.
void
AppWindow::ShowProfilerLauncher()
{
    // Create dialog if it doesn't exist (lazy initialization)
    // Dialog owns its own DataProvider - not tied to any specific trace
    if (!m_profiler_launcher_dialog)
    {
        m_profiler_launcher_dialog = std::make_unique<ProfilerLauncherDialog>(this);
    }

    m_profiler_launcher_dialog->Show();
}
#endif  // ROCPROFVIS_ENABLE_PROFILER

}  // namespace View
}  // namespace RocProfVis

