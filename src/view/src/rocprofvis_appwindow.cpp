// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_appwindow.h"
#include "imgui.h"
#include "implot.h"
#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
#    include "nfd.h"
#endif
#include "ImGuiFileDialog.h"

#include "amd_rocm_optiq_logo_png_light.h"
#include "amd_rocm_optiq_logo_png_dark.h"
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
#include "rocprofvis_view_module.h"
#include "widgets/rocprofvis_debug_window.h"
#include "widgets/rocprofvis_dialog.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_widget.h"
#include "widgets/rocprofvis_notification_manager.h"
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
constexpr float EMPTY_STATE_CONTENT_EM      = 32.0f;
constexpr float EMPTY_STATE_BUTTON_EM       = 10.0f;
constexpr float EMPTY_STATE_LOGO_EM         = 12.0f;
constexpr float EMPTY_STATE_RECENT_FILES_EM = 22.0f;

const std::vector<std::string> TRACE_EXTENSIONS   = { "db", "rpd", "yaml" };
const std::vector<std::string> PROJECT_EXTENSIONS = { "rpv" };
const std::vector<std::string> ALL_EXTENSIONS     = { "db", "rpd", "yaml", "rpv" };
constexpr const char* SUPPORTED_FILE_TYPES_HINT   = "Supported types: .db, .rpd, .yaml, .rpv";

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
, m_amd_logo_light(amd_rocm_optiq_logo_png_light,
                   static_cast<int>(sizeof(amd_rocm_optiq_logo_png_light)))
, m_amd_logo_dark(amd_rocm_optiq_logo_png_dark,
                  static_cast<int>(sizeof(amd_rocm_optiq_logo_png_dark)))
, m_default_padding(0.0f, 0.0f)
, m_default_spacing(0.0f, 0.0f)
, m_open_about_dialog(false)
, m_tabclosed_event_token(static_cast<EventManager::SubscriptionToken>(-1))
, m_tabselected_event_token(static_cast<EventManager::SubscriptionToken>(-1))
, m_font_changed_token(static_cast<EventManager::SubscriptionToken>(-1))
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
        (void)e;
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
    EventManager::GetInstance()->DispatchEvents();
    DebugWindow::GetInstance()->ClearTransient();
    m_tab_container->Update();
#ifdef ROCPROFVIS_DEVELOPER_MODE
    m_test_data_provider.Update();
#endif
    UpdateStatusBar();
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
    EmbeddedImage& logo = settings.GetUserSettings().display_settings.use_dark_mode
                              ? m_amd_logo_dark
                              : m_amd_logo_light;
    if(logo.Valid())
    {
        const float avail       = ImGui::GetContentRegionAvail().x;
        const float logo_width  = std::min(avail * 0.42f, font_size * EMPTY_STATE_LOGO_EM);
        const float logo_height = logo_width * static_cast<float>(logo.GetHeight()) /
                                  static_cast<float>(logo.GetWidth());
        ImVec2 logo_pos = ImGui::GetCursorScreenPos();
        logo_pos.x += (avail - logo_width) * 0.5f;
        ImGui::Dummy(ImVec2(avail, logo_height));
        logo.Render(logo_pos, logo_width);
        ImGui::Dummy(ImVec2(0.0f, font_size));
    }

    // --- Title ---

    ImGui::PushFont(NULL, settings.GetFontManager().GetFontSize(FontSize::kLarge));
    CenterNextTextItem("Open a trace or project");
    ImGui::TextUnformatted("Open a trace or project");
    ImGui::PopFont();

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

    // Calculate status bar height based on font size, with some padding.
    ImGuiStyle& style     = ImGui::GetStyle();
    float       line_pad  = style.CellPadding.y * 2.0f;
    float       line_size = ImGui::GetTextLineHeight() + line_pad;

    status_bar_item->m_height = line_size;

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

void
AppWindow::UpdateStatusBar()
{
    // Update status message every N frames
    const int UPDATE_STEP = 4;
    if(ImGui::GetFrameCount() % UPDATE_STEP == 0)
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
        if(pending_requests > 0 || clean_up_jobs > 0)
        {
            if(pending_requests > 0)
            {
                m_status_message = "Working: " + std::to_string(pending_requests) +
                                   " pending request(s)";
            }
            if(clean_up_jobs > 0)
            {
                m_status_message = (pending_requests > 0 ? m_status_message + " | " : "") +
                                    ("Cleaning up: " + std::to_string(clean_up_jobs) +
                                     " pending job(s)");
            }
            m_status_show_busy_indicator = true;
        }
        else
        {
            m_status_message             = "";
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
    ImGui::Dummy(ImVec2(style.WindowPadding.x, ImGui::GetFrameHeight()));
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

