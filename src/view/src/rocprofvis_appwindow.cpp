// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_appwindow.h"
#include "imgui.h"
#include "implot.h"
#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
#    include "nfd.h"
#endif
#include "ImGuiFileDialog.h"

#include "amd_rocm_optiq_logo_png.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_events.h"
#include "rocprofvis_project.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_hotkey_manager.h"
#include "rocprofvis_settings_panel.h"
#include "rocprofvis_version.h"
#include "rocprofvis_utils.h"
#include "rocprofvis_root_view.h"
#include "rocprofvis_trace_view.h"
#include "model/rocprofvis_trace_data_model.h"
#include "model/rocprofvis_summary_model.h"
#include "model/rocprofvis_timeline_model.h"
#include "model/rocprofvis_model_types.h"
#include "rocprofvis_view_module.h"
#include "widgets/rocprofvis_debug_window.h"
#include "widgets/rocprofvis_dialog.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_widget.h"
#include "widgets/rocprofvis_notification_manager.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

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
constexpr float EMPTY_STATE_CONTENT_EM      = 32.0f;
constexpr float EMPTY_STATE_BUTTON_EM       = 10.0f;
constexpr float EMPTY_STATE_LOGO_EM         = 12.0f;
constexpr float EMPTY_STATE_RECENT_FILES_EM = 22.0f;

const std::vector<std::string> TRACE_EXTENSIONS   = { "db", "rpd", "yaml" };
const std::vector<std::string> PROJECT_EXTENSIONS = { "rpv" };
const std::vector<std::string> ALL_EXTENSIONS     = { "db", "rpd", "yaml", "rpv" };
constexpr const char* SUPPORTED_FILE_TYPES_HINT   = "Supported types: .db, .rpd, .yaml, .rpv";

constexpr float STATUS_BAR_HEIGHT = 30.0f;

constexpr const char* CLEANUP_MESSAGE = "Waiting for requests to finish cleanup...";
constexpr const char* CLOSING_MESSAGE = "Closing...";

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
, m_amd_logo(amd_rocm_optiq_logo_png, static_cast<int>(sizeof(amd_rocm_optiq_logo_png)))
, m_default_padding(0.0f, 0.0f)
, m_default_spacing(0.0f, 0.0f)
, m_open_about_dialog(false)
, m_open_api_key_dialog(false)
, m_open_analysis_results(false)
, m_analysis_in_progress(false)
, m_show_analysis_status(false)
, m_tabclosed_event_token(static_cast<EventManager::SubscriptionToken>(-1))
, m_tabselected_event_token(static_cast<EventManager::SubscriptionToken>(-1))
#ifdef ROCPROFVIS_DEVELOPER_MODE
, m_show_debug_window(false)
, m_show_provider_test_widow(false)
, m_show_metrics(false)
#endif
, m_confirmation_dialog(std::make_unique<ConfirmationDialog>(
      SettingsManager::GetInstance().GetUserSettings().dont_ask_before_exit))
, m_message_dialog(std::make_unique<MessageDialog>())
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
{
    const InternalSettings& is = SettingsManager::GetInstance().GetInternalSettings();
    strncpy(m_api_key_buffer, is.ai_api_key.c_str(), sizeof(m_api_key_buffer) - 1);
    m_api_key_buffer[sizeof(m_api_key_buffer) - 1] = '\0';

    if(!is.ai_user_id.empty())
    {
        strncpy(m_user_id_buffer, is.ai_user_id.c_str(), sizeof(m_user_id_buffer) - 1);
    }
    else
    {
        const char* username = std::getenv("USERNAME");
        if(!username) username = std::getenv("USER");
        strncpy(m_user_id_buffer, username ? username : "", sizeof(m_user_id_buffer) - 1);
    }
    m_user_id_buffer[sizeof(m_user_id_buffer) - 1] = '\0';
}

AppWindow::~AppWindow()
{
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kTabClosed),
                                             m_tabclosed_event_token);
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kTabSelected),
                                             m_tabselected_event_token);
    for(auto& job : m_provider_cleanup_jobs)
    {
        if(job.future.valid())
        {
            job.future.get();
        }
    }
    m_provider_cleanup_jobs.clear();
    m_projects.clear();
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

    LayoutItem status_bar_item(-1, STATUS_BAR_HEIGHT);
    status_bar_item.m_item = std::make_shared<RocWidget>();
    LayoutItem main_area_item(-1, -STATUS_BAR_HEIGHT);
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
            RenderEmptyState();
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
        this->HandleTabClosed(e);
    };
    m_tabclosed_event_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTabClosed), new_tab_closed_handler);

    auto new_tab_selected_handler = [this](std::shared_ptr<RocEvent> e) {
        this->HandleTabSelectionChanged(e);
    };

    m_tabselected_event_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTabSelected), new_tab_selected_handler);

    ConfigureFileDialogBackend();

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

void
AppWindow::BeginAppShutdown()
{
    if(m_shutdown_requested)
    {
        RequestExitIfProviderCleanupsComplete();
        return;
    }

    m_shutdown_requested      = true;
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

    m_projects.clear();
    if(m_main_view)
    {
        m_main_view->GetMutableAt(m_tool_bar_index)->m_item = nullptr;
    }

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
#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
    UpdateNativeFileDialog();
#endif
    UpdateProviderCleanups();
    if(m_shutdown_requested)
    {
        return;
    }

    HotkeyManager::GetInstance().ProcessInput();

    if(m_analysis_in_progress && m_analysis_future.valid() &&
       m_analysis_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        m_analysis_result_text  = m_analysis_future.get();
        m_analysis_in_progress  = false;
        m_analysis_status_text  = "Analysis complete. Opening results...";
        m_open_analysis_results = true;
    }

    EventManager::GetInstance()->DispatchEvents();
    DebugWindow::GetInstance()->ClearTransient();
    m_tab_container->Update();
#ifdef ROCPROFVIS_DEVELOPER_MODE
    m_test_data_provider.Update();
#endif
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
        RenderAIAnalysisMenu();
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
    RenderAboutDialog();

    if(m_open_api_key_dialog)
    {
        ImGui::OpenPopup("API Key##_dialog");
        m_open_api_key_dialog = false;
    }
    RenderAPIKeyDialog();

    if(m_show_analysis_status)
    {
        ImGui::OpenPopup("AI Analysis Status##_status");
    }
    RenderAnalysisStatusPopup();

    if(m_open_analysis_results)
    {
        m_open_analysis_results = false;
        m_show_analysis_status  = false;
        OpenAnalysisResultsInBrowser();
    }

    m_confirmation_dialog->Render();
    m_message_dialog->Render();
    m_settings_panel->Render();

    ImGui::End();
    // Pop ImGuiStyleVar_ItemSpacing, ImGuiStyleVar_WindowPadding,
    // ImGuiStyleVar_WindowRounding
    ImGui::PopStyleVar(3);

    RenderFileDialog();
#ifdef ROCPROFVIS_DEVELOPER_MODE
    RenderDebugOuput();
#endif

    // render notifications last
    NotificationManager::GetInstance().Render();

    RenderDisableScreen();
}

void
AppWindow::RenderEmptyState()
{
    SettingsManager&            settings     = SettingsManager::GetInstance();
    const InternalSettings&     internal     = settings.GetInternalSettings();
    const std::list<std::string>& recent_files = internal.recent_files;
    const float font_size     = ImGui::GetFontSize();
    const float window_width  = ImGui::GetContentRegionAvail().x;
    const float window_height = ImGui::GetContentRegionAvail().y;
    const float card_width    = std::min(window_width - font_size * 2.0f,
                                         font_size * EMPTY_STATE_CONTENT_EM);
    const float card_padding  = font_size * 1.8f;
    std::string recent_file_to_open;

    // Vertically center the dialog card
    ImGui::SetCursorPosY(window_height * 0.18f);
    ImGui::SetCursorPosX((window_width - card_width) * 0.5f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(card_padding, card_padding));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,
                        settings.GetDefaultStyle().ChildRounding);
    ImGui::BeginChild("welcome_dialog", ImVec2(card_width, 0.0f),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY,
                      ImGuiWindowFlags_NoScrollbar);

    // --- Logo ---
    if(m_amd_logo.Valid())
    {
        const float avail      = ImGui::GetContentRegionAvail().x;
        const float logo_width = std::min(avail * 0.42f, font_size * EMPTY_STATE_LOGO_EM);
        const float logo_height =
            logo_width * static_cast<float>(m_amd_logo.GetHeight()) /
            static_cast<float>(m_amd_logo.GetWidth());
        const float offset = (avail - logo_width) * 0.5f;
        ImVec2      logo_pos = ImGui::GetCursorScreenPos();
        logo_pos.x += offset;
        ImGui::Dummy(ImVec2(avail, logo_height));
        bool is_dark = settings.GetUserSettings().display_settings.use_dark_mode;
        m_amd_logo.Render(logo_pos, logo_width, is_dark);
        ImGui::Dummy(ImVec2(0.0f, font_size));
    }

    // --- Title ---
    ImFont* title_font = settings.GetFontManager().GetFont(FontType::kLarge);
    if(title_font) ImGui::PushFont(title_font);
    CenterNextTextItem("Open a trace or project");
    ImGui::TextUnformatted("Open a trace or project");
    if(title_font) ImGui::PopFont();

    ImGui::Dummy(ImVec2(0.0f, font_size * 0.25f));

    // --- Subtitle ---
    CenterNextTextItem("Drag and drop files here, or open one from disk.");
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextUnformatted("Drag and drop files here, or open one from disk.");
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, font_size * 0.9f));

    // --- Open button ---
    const float button_width = font_size * EMPTY_STATE_BUTTON_EM;
    CenterNextItem(button_width);
    ImGui::PushStyleColor(ImGuiCol_Button, settings.GetColor(Colors::kAccentRed));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          settings.GetColor(Colors::kAccentRedHover));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          settings.GetColor(Colors::kAccentRedActive));
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextOnAccent));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(ImGui::GetStyle().FramePadding.x,
                               ImGui::GetStyle().FramePadding.y + 4.0f));
    if(ImGui::Button("Open File", ImVec2(button_width, 0.0f)))
    {
        HandleOpenFile();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    if(ImGui::IsItemHovered())
    {
        SetTooltipStyled("%s", SUPPORTED_FILE_TYPES_HINT);
    }

    // --- Recent files ---
    if(!recent_files.empty())
    {
        ImGui::Dummy(ImVec2(0.0f, font_size * 0.6f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, font_size * 0.6f));

        CenterNextTextItem("Recent Files");
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextUnformatted("Recent Files");
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.0f, font_size * 0.25f));

        const float rf_width =
            std::min(ImGui::GetContentRegionAvail().x * 0.78f,
                     font_size * EMPTY_STATE_RECENT_FILES_EM);

        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                              settings.GetColor(Colors::kHighlightChart));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                              settings.GetColor(Colors::kSelection));
        int shown = 0;
        for(const std::string& file : recent_files)
        {
            if(shown++ >= static_cast<int>(MAX_RECENT_FILES)) break;

            const std::filesystem::path fpath(file);
            const std::string fname = fpath.filename().empty() ? file : fpath.filename().string();

            ImGui::PushID(file.c_str());
            CenterNextItem(rf_width);

            if(ImGui::Selectable(fname.c_str(), false, 0, ImVec2(rf_width, 0.0f)))
            {
                recent_file_to_open = file;
            }
            if(ImGui::IsItemHovered())
            {
                SetTooltipStyled("%s", file.c_str());
            }

            ImGui::PopID();
        }
        ImGui::PopStyleColor(2);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);

    if(!recent_file_to_open.empty())
    {
        HandleOpenRecentFile(recent_file_to_open);
    }
}

void
AppWindow::RenderShutdownState()
{
    ImGui::OpenPopup(SHUTDOWN_DIALOG_NAME);

    const float dpi = SettingsManager::GetInstance().GetDPI();
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
               viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
        ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(360.0f * dpi, 0.0f), ImGuiCond_Always);

    PopUpStyle ps;
    ps.PushPopupStyles();
    ps.CenterPopup();
    if(ImGui::BeginPopupModal(SHUTDOWN_DIALOG_NAME, nullptr,
                              ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoCollapse))
    {
        if(m_provider_cleanup_jobs.empty())
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
            const std::string remaining_message =
                "Cleanup jobs remaining: " +
                std::to_string(m_provider_cleanup_jobs.size());
            CenterNextTextItem(remaining_message.c_str());
            ImGui::TextUnformatted(remaining_message.c_str());
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
            m_file_dialog_callback(
                std::filesystem::path(ImGuiFileDialog::Instance()->GetFilePathName())
                    .string());
        }
        ImGuiFileDialog::Instance()->Close();
    }
    ImGui::PopStyleVar(3);
}

void
AppWindow::OpenFile(std::string file_path)
{
    spdlog::info("Opening file: {}", file_path);

    std::unique_ptr<Project> project = std::make_unique<Project>();
    switch(project->Open(file_path))
    {
        case Project::OpenResult::Success:
        {
            TabItem tab =
                TabItem{ project->GetName(), project->GetID(), project->GetView(), true };
            m_tab_container->AddTab(std::move(tab));
            m_projects[project->GetID()] = std::move(project);
            SettingsManager::GetInstance().AddRecentFile(file_path);
            break;
        }
        case Project::OpenResult::Duplicate:
        {
            // File is already opened, just switch to that tab
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

void
AppWindow::HandleOpenRecentFile(const std::string& file_path)
{
    if(!std::filesystem::exists(file_path))
    {
        SettingsManager::GetInstance().RemoveRecentFile(file_path);
        ShowMessageDialog("Recent File Not Found",
                          "This recent file could not be found and was removed from the list:\n\n" +
                              file_path);
        return;
    }
    OpenFile(file_path);
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
            for(const std::string& file : recent_files)
            {
                if(ImGui::MenuItem(file.c_str(), nullptr))
                {
                    HandleOpenRecentFile(file);
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
        if(ImGui::MenuItem("Fullscreen", "F11", m_is_fullscreen))
        {
            if(m_notification_callback)
            {
                m_notification_callback(
                    rocprofvis_view_notification_t::
                        kRocProfVisViewNotification_Toggle_Fullscreen);
            }
        }
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
AppWindow::RenderAIAnalysisMenu()
{
    if(ImGui::BeginMenu("AI Analysis"))
    {
        if(ImGui::MenuItem("API Key"))
        {
            m_open_api_key_dialog = true;
        }

        bool has_key     = strlen(m_api_key_buffer) > 0;
        bool has_project = GetCurrentProject() != nullptr;
        if(ImGui::MenuItem("Start Analysis", nullptr, false,
                           has_key && has_project && !m_analysis_in_progress))
        {
            StartAIAnalysis();
        }
        if(m_analysis_in_progress)
        {
            ImGui::MenuItem("Analysis in progress...", nullptr, false, false);
        }
        ImGui::EndMenu();
    }
}

void
AppWindow::RenderAPIKeyDialog()
{
    PopUpStyle popup_style;
    popup_style.PushPopupStyles();
    popup_style.PushTitlebarColors();
    popup_style.CenterPopup();

    ImGui::SetNextWindowSize(ImVec2(480, 0));

    if(ImGui::BeginPopupModal("API Key##_dialog", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize |
                                  ImGuiWindowFlags_NoMove))
    {
        ImGui::TextUnformatted("User ID:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##user_id_input", m_user_id_buffer,
                         IM_ARRAYSIZE(m_user_id_buffer));

        ImGui::Spacing();

        ImGui::TextUnformatted("API Key:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##api_key_input", m_api_key_buffer,
                         IM_ARRAYSIZE(m_api_key_buffer),
                         ImGuiInputTextFlags_Password);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float save_width =
            ImGui::CalcTextSize("Save").x + ImGui::GetStyle().FramePadding.x * 2;
        float cancel_width =
            ImGui::CalcTextSize("Cancel").x + ImGui::GetStyle().FramePadding.x * 2;
        float total_width = save_width + cancel_width + ImGui::GetStyle().ItemSpacing.x;

        ImGui::SetCursorPosX(
            ImGui::GetWindowSize().x - total_width - ImGui::GetStyle().ItemSpacing.x);

        if(ImGui::Button("Save"))
        {
            InternalSettings& is = SettingsManager::GetInstance().GetInternalSettings();
            is.ai_api_key = std::string(m_api_key_buffer);
            is.ai_user_id = std::string(m_user_id_buffer);
            SettingsManager::GetInstance().ApplyUserSettings(
                SettingsManager::GetInstance().GetUserSettings(), true);
            spdlog::info("API key and user ID saved to settings");
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    popup_style.PopStyles();
}

void
AppWindow::RenderAnalysisStatusPopup()
{
    PopUpStyle popup_style;
    popup_style.PushPopupStyles();
    popup_style.PushTitlebarColors();
    popup_style.CenterPopup();

    ImGui::SetNextWindowSize(ImVec2(400, 0));

    if(ImGui::BeginPopupModal("AI Analysis Status##_status", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize |
                                  ImGuiWindowFlags_NoMove))
    {
        ImGui::Spacing();

        if(m_analysis_in_progress)
        {
            float spinner_radius = 8.0f;
            ImU32 spinner_color  = IM_COL32(224, 62, 62, 255);
            float t = static_cast<float>(ImGui::GetTime());

            ImVec2 pos = ImGui::GetCursorScreenPos();
            float cx = pos.x + spinner_radius + 4.0f;
            float cy = pos.y + spinner_radius + 2.0f;

            int segments = 12;
            for(int i = 0; i < segments; i++)
            {
                float angle = (static_cast<float>(i) / segments) * 2.0f * 3.14159f + t * 4.0f;
                float alpha = (static_cast<float>(i) / segments);
                float x = cx + cosf(angle) * spinner_radius;
                float y = cy + sinf(angle) * spinner_radius;
                ImGui::GetWindowDrawList()->AddCircleFilled(
                    ImVec2(x, y), 2.5f,
                    IM_COL32(
                        (spinner_color >> 0)  & 0xFF,
                        (spinner_color >> 8)  & 0xFF,
                        (spinner_color >> 16) & 0xFF,
                        static_cast<int>(alpha * 255)));
            }

            ImGui::Dummy(ImVec2(spinner_radius * 2 + 8, spinner_radius * 2 + 4));
            ImGui::SameLine();
            ImGui::TextUnformatted(m_analysis_status_text.c_str());
        }
        else
        {
            ImGui::TextColored(ImVec4(0.35f, 0.78f, 0.47f, 1.0f), "Done!");
            ImGui::SameLine();
            ImGui::TextUnformatted(m_analysis_status_text.c_str());
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float btn_width =
            ImGui::CalcTextSize("Close").x + ImGui::GetStyle().FramePadding.x * 2;
        ImGui::SetCursorPosX(
            ImGui::GetWindowSize().x - btn_width - ImGui::GetStyle().ItemSpacing.x);
        if(ImGui::Button("Close"))
        {
            m_show_analysis_status = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    popup_style.PopStyles();
}

void
AppWindow::OpenAnalysisResultsInBrowser()
{
    std::string config_path = get_application_config_path(true);
    std::filesystem::path html_path =
        std::filesystem::path(config_path) / "ai_analysis_results.html";

    auto escape_html = [](const std::string& s) -> std::string {
        std::string out;
        out.reserve(s.size());
        for(char c : s)
        {
            switch(c)
            {
                case '&':  out += "&amp;"; break;
                case '<':  out += "&lt;"; break;
                case '>':  out += "&gt;"; break;
                case '"':  out += "&quot;"; break;
                default:   out += c; break;
            }
        }
        return out;
    };

    auto markdown_to_html = [&escape_html](const std::string& md) -> std::string {
        std::istringstream stream(md);
        std::string line;
        std::string html;
        bool in_table = false;
        bool first_table_row = true;
        bool in_list = false;
        bool in_olist = false;

        auto close_lists = [&]() {
            if(in_list)  { html += "</ul>\n"; in_list = false; }
            if(in_olist) { html += "</ol>\n"; in_olist = false; }
        };

        auto trim = [](const std::string& s) -> std::string {
            size_t a = s.find_first_not_of(" \t");
            size_t b = s.find_last_not_of(" \t");
            return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
        };

        auto is_separator_row = [](const std::string& l) -> bool {
            for(char c : l)
                if(c != '|' && c != '-' && c != ':' && c != ' ') return false;
            return l.find('-') != std::string::npos;
        };

        auto inline_fmt = [&escape_html](const std::string& l) -> std::string {
            std::string out;
            size_t i = 0, len = l.size();
            while(i < len)
            {
                if(i + 1 < len && l[i] == '*' && l[i + 1] == '*')
                {
                    size_t e = l.find("**", i + 2);
                    if(e != std::string::npos)
                    {
                        out += "<strong>" + escape_html(l.substr(i + 2, e - i - 2)) + "</strong>";
                        i = e + 2;
                        continue;
                    }
                }
                if(l[i] == '`')
                {
                    size_t e = l.find('`', i + 1);
                    if(e != std::string::npos)
                    {
                        out += "<code>" + escape_html(l.substr(i + 1, e - i - 1)) + "</code>";
                        i = e + 1;
                        continue;
                    }
                }
                std::string ch(1, l[i]);
                out += escape_html(ch);
                i++;
            }
            return out;
        };

        while(std::getline(stream, line))
        {
            std::string t = trim(line);

            if(t.empty())
            {
                if(in_table) { html += "</tbody></table>\n"; in_table = false; }
                close_lists();
                html += "<br>\n";
                continue;
            }

            if(t.find('|') != std::string::npos)
            {
                int pipes = 0;
                for(char c : t) if(c == '|') pipes++;
                if(pipes >= 2)
                {
                    if(is_separator_row(t)) continue;

                    std::vector<std::string> cells;
                    size_t s = (t[0] == '|') ? 1 : 0;
                    size_t e = t.size();
                    if(e > 0 && t[e-1] == '|') e--;
                    std::istringstream cs(t.substr(s, e - s));
                    std::string cell;
                    while(std::getline(cs, cell, '|'))
                        cells.push_back(trim(cell));

                    if(!in_table)
                    {
                        close_lists();
                        html += "<table><thead><tr>";
                        for(const auto& c : cells)
                            html += "<th>" + inline_fmt(c) + "</th>";
                        html += "</tr></thead><tbody>\n";
                        in_table = true;
                        first_table_row = true;
                        continue;
                    }

                    html += "<tr>";
                    for(const auto& c : cells)
                        html += "<td>" + inline_fmt(c) + "</td>";
                    html += "</tr>\n";
                    continue;
                }
            }

            if(in_table) { html += "</tbody></table>\n"; in_table = false; }

            if(t.rfind("### ", 0) == 0)
            {
                close_lists();
                html += "<h3>" + inline_fmt(t.substr(4)) + "</h3>\n";
            }
            else if(t.rfind("## ", 0) == 0)
            {
                close_lists();
                html += "<h2>" + inline_fmt(t.substr(3)) + "</h2>\n";
            }
            else if(t.rfind("# ", 0) == 0)
            {
                close_lists();
                html += "<h1>" + inline_fmt(t.substr(2)) + "</h1>\n";
            }
            else if(t == "---" || t == "***" || t == "___")
            {
                close_lists();
                html += "<hr>\n";
            }
            else if(t.rfind("> ", 0) == 0)
            {
                close_lists();
                html += "<blockquote>" + inline_fmt(t.substr(2)) + "</blockquote>\n";
            }
            else if(t.rfind("- ", 0) == 0 || t.rfind("* ", 0) == 0)
            {
                if(in_olist) { html += "</ol>\n"; in_olist = false; }
                if(!in_list) { html += "<ul>\n"; in_list = true; }
                html += "<li>" + inline_fmt(t.substr(2)) + "</li>\n";
            }
            else if(t.size() > 2 && t[0] >= '0' && t[0] <= '9' &&
                    t.find(". ") != std::string::npos)
            {
                if(in_list) { html += "</ul>\n"; in_list = false; }
                if(!in_olist) { html += "<ol>\n"; in_olist = true; }
                size_t dot = t.find(". ");
                html += "<li>" + inline_fmt(t.substr(dot + 2)) + "</li>\n";
            }
            else
            {
                close_lists();
                html += "<p>" + inline_fmt(t) + "</p>\n";
            }
        }

        if(in_table) html += "</tbody></table>\n";
        close_lists();
        return html;
    };

    std::string body_html = markdown_to_html(m_analysis_result_text);

    std::ofstream out(html_path);
    if(!out.is_open())
    {
        ShowMessageDialog("AI Analysis", "Failed to save HTML report.");
        return;
    }

    out << R"(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ROCm Optiq - AI Analysis Results</title>
<style>
  :root { color-scheme: dark; }
  body {
    font-family: 'Segoe UI', -apple-system, BlinkMacSystemFont, sans-serif;
    background: #1a1b1e; color: #d4d4d8; margin: 0; padding: 0;
    line-height: 1.7;
  }
  .container { max-width: 900px; margin: 0 auto; padding: 40px 32px; }
  .header {
    text-align: center; padding-bottom: 24px;
    border-bottom: 2px solid #e03e3e; margin-bottom: 32px;
  }
  .header h1 { color: #e03e3e; margin: 0 0 4px 0; font-size: 1.8em; }
  .header .subtitle { color: #888; font-size: 0.9em; }
  h1 { color: #e03e3e; font-size: 1.5em; border-bottom: 1px solid #333; padding-bottom: 8px; margin-top: 32px; }
  h2 { color: #60a5fa; font-size: 1.3em; margin-top: 28px; }
  h3 { color: #93c5fd; font-size: 1.1em; margin-top: 20px; }
  p { margin: 8px 0; }
  strong { color: #f0f0f0; }
  code {
    background: #2d2d30; color: #c8dcb4; padding: 2px 6px;
    border-radius: 4px; font-family: 'Cascadia Code', 'Consolas', monospace; font-size: 0.9em;
  }
  blockquote {
    border-left: 3px solid #e03e3e; margin: 12px 0; padding: 8px 16px;
    background: #222; color: #aaa; border-radius: 0 6px 6px 0;
  }
  table {
    width: 100%; border-collapse: collapse; margin: 16px 0;
    background: #222; border-radius: 8px; overflow: hidden;
  }
  th {
    background: #2a4a7a; color: #93c5fd; text-align: left;
    padding: 10px 14px; font-weight: 600; font-size: 0.9em;
    text-transform: uppercase; letter-spacing: 0.5px;
  }
  td { padding: 8px 14px; border-bottom: 1px solid #333; }
  tr:hover td { background: #2a2a2e; }
  ul, ol { padding-left: 24px; margin: 8px 0; }
  li { margin: 4px 0; }
  li::marker { color: #e03e3e; }
  hr { border: none; border-top: 1px solid #444; margin: 24px 0; }
  .timestamp { color: #666; font-size: 0.8em; text-align: center; margin-top: 40px; }
</style>
</head>
<body>
<div class="container">
  <div class="header">
    <h1>ROCm Optiq &mdash; AI Analysis</h1>
    <div class="subtitle">Automated profiling analysis powered by LLM</div>
  </div>
)" << body_html << R"(
  <div class="timestamp">Generated by ROCm Optiq on )" << __DATE__ << R"(</div>
</div>
</body>
</html>)";

    out.close();

#ifdef _WIN32
    ShellExecuteW(nullptr, L"open", html_path.wstring().c_str(),
                  nullptr, nullptr, SW_SHOWNORMAL);
#endif
    spdlog::info("AI Analysis report saved to: {}", html_path.string());
}

std::string
AppWindow::BuildAnalysisPrompt()
{
    std::stringstream ss;
    ss << "You are a performance analysis expert for GPU computing workloads. "
       << "Analyze the following ROCm profiling trace data and provide a detailed summary of:\n"
       << "1. GPU Usage - utilization, top kernels by execution time\n"
       << "2. Memory Usage - GPU memory utilization patterns\n"
       << "3. CPU Usage - host-side activity if available\n"
       << "4. Performance recommendations\n\n";

    Project* project = GetCurrentProject();
    if(!project) return ss.str();

    auto trace_view = std::dynamic_pointer_cast<TraceView>(project->GetView());
    if(!trace_view) return ss.str();

    const DataProvider& provider = trace_view->GetDataProvider();
    const TraceDataModel& model = provider.DataModel();

    ss << "=== Trace File ===\n"
       << "Path: " << model.GetTraceFilePath() << "\n\n";

    const SummaryModel& summary = model.GetSummary();
    const SummaryInfo::AggregateMetrics& root = summary.GetSummaryData();

    std::function<void(const SummaryInfo::AggregateMetrics&, int)> dump_metrics;
    dump_metrics = [&](const SummaryInfo::AggregateMetrics& m, int depth) {
        std::string indent(depth * 2, ' ');
        if(m.name.has_value())
            ss << indent << "Name: " << m.name.value() << "\n";
        if(m.id.has_value())
            ss << indent << "ID: " << m.id.value() << "\n";

        if(m.gpu.gfx_utilization.has_value())
            ss << indent << "GPU GFX Utilization: "
               << m.gpu.gfx_utilization.value() * 100.0f << "%\n";
        if(m.gpu.mem_utilization.has_value())
            ss << indent << "GPU Memory Utilization: "
               << m.gpu.mem_utilization.value() * 100.0f << "%\n";

        if(m.gpu.kernel_exec_time_total > 0)
            ss << indent << "Total Kernel Exec Time: "
               << m.gpu.kernel_exec_time_total << " ns\n";

        if(!m.gpu.top_kernels.empty())
        {
            ss << indent << "Top Kernels:\n";
            for(const auto& k : m.gpu.top_kernels)
            {
                ss << indent << "  - " << k.name
                   << " | invocations=" << k.invocations
                   << " | total=" << k.exec_time_sum << "ns"
                   << " | min=" << k.exec_time_min << "ns"
                   << " | max=" << k.exec_time_max << "ns"
                   << " | pct=" << k.exec_time_pct * 100.0f << "%\n";
            }
        }

        for(const auto& sub : m.sub_metrics)
            dump_metrics(sub, depth + 1);
    };

    ss << "=== Summary Metrics ===\n";
    dump_metrics(root, 0);

    const TimelineModel& timeline = model.GetTimeline();
    ss << "\n=== Timeline ===\n"
       << "Start Time: " << timeline.GetStartTime() << " ns\n"
       << "End Time: " << timeline.GetEndTime() << " ns\n"
       << "Duration: " << (timeline.GetEndTime() - timeline.GetStartTime()) << " ns\n"
       << "Track Count: " << timeline.GetTrackCount() << "\n";

    ss << "\nProvide your analysis in a clear, structured format.";
    return ss.str();
}

#ifdef _WIN32
std::string
AppWindow::CallLLMApi(const std::string& api_key, const std::string& prompt)
{
    std::string user_id(m_user_id_buffer);

    std::string escaped_prompt;
    for(char c : prompt)
    {
        switch(c)
        {
            case '"':  escaped_prompt += "\\\""; break;
            case '\\': escaped_prompt += "\\\\"; break;
            case '\n': escaped_prompt += "\\n"; break;
            case '\r': escaped_prompt += "\\r"; break;
            case '\t': escaped_prompt += "\\t"; break;
            default:   escaped_prompt += c; break;
        }
    }

    std::string body =
        R"({"model":"GPT-oss-20B","max_completion_tokens":2048,"temperature":0.7,"messages":[)"
        R"({"role":"system","content":"You are a performance analysis expert for GPU computing workloads."},)"
        R"({"role":"user","content":")" + escaped_prompt + R"("}]})";

    HINTERNET session = WinHttpOpen(L"ROCm-Optiq/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if(!session) return "Error: Failed to open HTTP session.";

    HINTERNET connect = WinHttpConnect(session, L"llm-api.amd.com",
                                        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if(!connect)
    {
        WinHttpCloseHandle(session);
        return "Error: Failed to connect to llm-api.amd.com.";
    }

    HINTERNET request = WinHttpOpenRequest(connect, L"POST",
                                            L"/OnPrem/chat/completions",
                                            nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            WINHTTP_FLAG_SECURE);
    if(!request)
    {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return "Error: Failed to create HTTP request.";
    }

    std::wstring headers = L"Content-Type: application/json\r\n";
    headers += L"Ocp-Apim-Subscription-Key: " +
               std::wstring(api_key.begin(), api_key.end()) + L"\r\n";
    headers += L"user: " +
               std::wstring(user_id.begin(), user_id.end()) + L"\r\n";

    WinHttpAddRequestHeaders(request, headers.c_str(), -1L,
                              WINHTTP_ADDREQ_FLAG_ADD);

    BOOL sent = WinHttpSendRequest(request,
                                    WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                    (LPVOID)body.c_str(),
                                    static_cast<DWORD>(body.size()),
                                    static_cast<DWORD>(body.size()), 0);
    if(!sent)
    {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return "Error: Failed to send request. Check your network connection.";
    }

    BOOL received = WinHttpReceiveResponse(request, nullptr);
    if(!received)
    {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return "Error: No response from server.";
    }

    DWORD status_code = 0;
    DWORD size        = sizeof(status_code);
    WinHttpQueryHeaders(request,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &size,
                        WINHTTP_NO_HEADER_INDEX);

    std::string response_body;
    DWORD bytes_available = 0;
    do
    {
        WinHttpQueryDataAvailable(request, &bytes_available);
        if(bytes_available > 0)
        {
            std::vector<char> buffer(bytes_available + 1, 0);
            DWORD bytes_read = 0;
            WinHttpReadData(request, buffer.data(), bytes_available, &bytes_read);
            response_body.append(buffer.data(), bytes_read);
        }
    } while(bytes_available > 0);

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);

    if(status_code != 200)
    {
        return "Error: API returned status " + std::to_string(status_code) +
               "\n\n" + response_body;
    }

    auto parse_result = jt::Json::parse(response_body);
    if(parse_result.first == jt::Json::success)
    {
        jt::Json& json = parse_result.second;
        if(json["choices"].isArray() && json["choices"][0].isObject())
        {
            jt::Json& message = json["choices"][0]["message"];
            if(message["content"].isString())
            {
                return message["content"].getString();
            }
        }
    }

    return "Error: Failed to parse API response.\n\n" + response_body;
}
#else
std::string
AppWindow::CallLLMApi(const std::string& /*api_key*/, const std::string& /*prompt*/)
{
    return "Error: AI Analysis is only supported on Windows.";
}
#endif

void
AppWindow::StartAIAnalysis()
{
    std::string api_key(m_api_key_buffer);
    if(api_key.empty())
    {
        ShowMessageDialog("AI Analysis", "Please set your API key first via AI Analysis > API Key.");
        return;
    }

    Project* project = GetCurrentProject();
    if(!project)
    {
        ShowMessageDialog("AI Analysis", "Please open a trace file before starting analysis.");
        return;
    }

    m_analysis_in_progress  = true;
    m_show_analysis_status  = true;
    m_analysis_status_text  = "Extracting profiling data from trace...";
    m_analysis_result_text.clear();

    std::string prompt = BuildAnalysisPrompt();
    m_analysis_status_text = "Sending data to LLM gateway...";

    m_analysis_future = std::async(std::launch::async,
        [this, api_key, prompt]() -> std::string {
            return CallLLMApi(api_key, prompt);
        });
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

    ShowOpenFileDialog(
        "Choose File", file_filters, "",
        [this](std::string file_path) -> void { this->OpenFile(file_path); });
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
        ImFont* large_font =
            SettingsManager::GetInstance().GetFontManager().GetFont(FontType::kLarge);
        if(large_font) ImGui::PushFont(large_font);

        ImGui::SetCursorPosX(
            (ImGui::GetWindowSize().x - ImGui::CalcTextSize(NAME_LABEL).x) * 0.5f);
        ImGui::TextUnformatted(NAME_LABEL);
        if(large_font) ImGui::PopFont();

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
    if(m_is_native_file_dialog_open)
    {
        if(m_file_dialog_future.valid() &&
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

            if(m_restore_fullscreen_later)
            {
                // toggle fullscreen on if it should be restored after dialog closes
                if(!m_is_fullscreen && m_notification_callback)
                {
                    m_notification_callback(
                        rocprofvis_view_notification_t::
                            kRocProfVisViewNotification_Toggle_Fullscreen);
                }
                m_restore_fullscreen_later = false;
            }
        }
    }
}

void
AppWindow::ShowNativeFileDialog(const std::vector<FileFilter>&   file_filters,
                                const std::string&               initial_path,
                                std::function<void(std::string)> callback,
                                bool                             save_dialog)
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

        nfdu8filteritem_t*       filters = new nfdu8filteritem_t[file_filters.size()];
        std::vector<std::string> extension_stings;
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
            extension_stings.push_back(std::move(extensions_str));
        }
        for(size_t i = 0; i < file_filters.size(); ++i)
        {
            filters[i] = { file_filters[i].m_name.c_str(), extension_stings[i].c_str() };
        }

        nfdresult_t result;
        if(save_dialog)
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
            args.filterList            = filters;
            args.filterCount = static_cast<nfdfiltersize_t>(file_filters.size());
            if(!initial_path.empty())
            {
                args.defaultPath = initial_path.c_str();
            }
            result = NFD_OpenDialogU8_With(&outPath, &args);
        }
        delete[] filters;
        std::string file_path;
        if(result == NFD_OKAY)
        {
            file_path = outPath;
            if(outPath)
            {
                std::filesystem::path p(file_path);
                if(!p.has_extension())
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

#endif

void
AppWindow::ShowImGuiFileDialog(const std::string& title, const std::vector<FileFilter>& file_filters,
                          const std::string& initial_path, const bool& confirm_overwrite,
                          std::function<void(std::string)> callback)
{
    m_file_dialog_callback = callback;
    m_init_file_dialog     = true;

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

    IGFD::FileDialogConfig config;
    config.path  = initial_path;
    config.flags = confirm_overwrite
                       ? ImGuiFileDialogFlags_Default
                       : ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_HideColumnType;
    ImGuiFileDialog::Instance()->OpenDialog(FILE_DIALOG_NAME, title,
                                            filter_stream.str().c_str(), config);
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
                                   rocprofvis_controller_t* controller = rocprofvis_controller_alloc(file_path.c_str());
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

}  // namespace View
}  // namespace RocProfVis
