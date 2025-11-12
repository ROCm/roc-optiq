// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_appwindow.h"
#include "imgui.h"
#include "implot.h"
#ifdef USE_NATIVE_FILE_DIALOG
#    include "nfd.h"
#else
#    include "ImGuiFileDialog.h"
#endif

#include "rocprofvis_controller.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_events.h"
#include "rocprofvis_project.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_settings_panel.h"
#include "rocprofvis_version.h"
#ifdef COMPUTE_UI_SUPPORT
#    include "rocprofvis_navigation_manager.h"
#endif
#include "rocprofvis_root_view.h"
#include "rocprofvis_trace_view.h"
#include "rocprofvis_view_module.h"
#include "widgets/rocprofvis_debug_window.h"
#include "widgets/rocprofvis_dialog.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_notification_manager.h"
#include <filesystem>

namespace RocProfVis
{
namespace View
{

constexpr ImVec2      FILE_DIALOG_SIZE       = ImVec2(480.0f, 360.0f);
constexpr const char* FILE_DIALOG_NAME       = "ChooseFileDlgKey";
constexpr const char* TAB_CONTAINER_SRC_NAME = "MainTabContainer";
constexpr const char* ABOUT_DIALOG_NAME      = "About##_dialog";

constexpr float STATUS_BAR_HEIGHT = 30.0f;

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
, m_tabclosed_event_token(static_cast<EventManager::SubscriptionToken>(-1))
, m_tabselected_event_token(static_cast<EventManager::SubscriptionToken>(-1))
#ifdef ROCPROFVIS_DEVELOPER_MODE
, m_show_debug_window(false)
, m_show_provider_test_widow(false)
, m_show_metrics(false)
#endif
, m_confirmation_dialog(std::make_unique<ConfirmationDialog>())
, m_message_dialog(std::make_unique<MessageDialog>())
, m_tool_bar_index(0)
, m_histogram_visible(true)
, m_sidebar_visible(true)
, m_analysis_bar_visible(true)
#ifndef USE_NATIVE_FILE_DIALOG
, m_init_file_dialog(false)
#else
, m_is_native_file_dialog_open(false)
#endif
{}

AppWindow::~AppWindow()
{
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kTabClosed),
                                             m_tabclosed_event_token);
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kTabSelected),
                                             m_tabselected_event_token);
    m_projects.clear();
#ifdef COMPUTE_UI_SUPPORT
    NavigationManager::DestroyInstance();
#endif
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
#ifdef COMPUTE_UI_SUPPORT
    NavigationManager::GetInstance()->RegisterRootContainer(m_tab_container);
#endif
    main_area_item.m_item = m_tab_container;

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

    return result;
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
    if(m_tab_container->GetTabs().size() == 0)
    {
        if(m_notification_callback)
            m_notification_callback(
                rocprofvis_view_notification_t::kRocProfVisViewNotification_Exit_App);
        return;
    }

    // Only show the dialog if there are open tabs
    ShowConfirmationDialog(
        "Confirm Close",
        "Are you sure you want to close the application? Any unsaved data will be lost.",
        [this]() {
            if(m_notification_callback)
                m_notification_callback(
                    rocprofvis_view_notification_t::kRocProfVisViewNotification_Exit_App);
        });
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
AppWindow::ShowSaveFileDialog(const std::string& title, const std::string& file_filter,
                              const std::string&               initial_path,
                              std::function<void(std::string)> callback)
{
    #ifdef USE_NATIVE_FILE_DIALOG
    (void)title;
    ShowNativeSaveFileDialog(file_filter, initial_path, callback);
    #else
    ShowFileDialog(title, file_filter, initial_path, true, callback);
    #endif
}

void
AppWindow::ShowOpenFileDialog(const std::string& title, const std::string& file_filter,
                              const std::string&               initial_path,
                              std::function<void(std::string)> callback)
{
    #ifdef USE_NATIVE_FILE_DIALOG
    (void)title;
    ShowNativeOpenFileDialog(file_filter, initial_path, callback);
    #else
    ShowFileDialog(title, file_filter, initial_path, false, callback);
    #endif
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
AppWindow::Update()
{
#ifdef USE_NATIVE_FILE_DIALOG
    UpdateNativeFileDialog();
#endif
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
    DrawInternalBuildBanner();
#endif
#ifdef IMGUI_HAS_VIEWPORT
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetWorkPos());
    ImGui::SetNextWindowSize(viewport->GetWorkSize());
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

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, m_default_spacing);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, m_default_padding);
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
    ImGui::PopStyleVar(2);  // Pop ImGuiStyleVar_ItemSpacing, ImGuiStyleVar_WindowPadding

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

#ifndef USE_NATIVE_FILE_DIALOG    
     RenderFileDialog();
#endif
#ifdef ROCPROFVIS_DEVELOPER_MODE
    RenderDebugOuput();
#endif

    // render notifications last
    NotificationManager::GetInstance().Render();
}

#ifndef USE_NATIVE_FILE_DIALOG
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
#endif

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

            // Set initial visibility to save the same settings in different tabs
            auto trace_view_tab =
                std::dynamic_pointer_cast<RocProfVis::View::TraceView>(tab.m_widget);
            if(trace_view_tab)
            {
                trace_view_tab->SetAnalysisViewVisibility(m_analysis_bar_visible);
                trace_view_tab->SetSidebarViewVisibility(m_sidebar_visible);
            }

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
AppWindow::RenderFileMenu(Project* project)
{
    bool is_open_file_dialog_open = false;
    #ifdef USE_NATIVE_FILE_DIALOG
    is_open_file_dialog_open = m_is_native_file_dialog_open;
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
        if(ImGui::MenuItem("Save As", nullptr, false, project || !is_open_file_dialog_open))
        {
            HandleSaveAsFile();
        }
        ImGui::Separator();
        const std::list<std::string> recent_files =
            SettingsManager::GetInstance().GetInternalSettings().recent_files;
        if(ImGui::BeginMenu("Recent Files", !recent_files.empty()))
        {
            for(const std::string& file : recent_files)
            {
                if(ImGui::MenuItem(file.c_str(), nullptr))
                {
                    OpenFile(file);
                }
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
    Project::TraceType trace_type =
        project == nullptr ? Project::Undefined : project->GetTraceType();

    if(ImGui::BeginMenu("Edit"))
    {
        // Trace project specific menu options
        if(trace_type == Project::System)
        {
            std::shared_ptr<RootView> root_view =
                std::dynamic_pointer_cast<RootView>(project->GetView());

            if(root_view)
            {
                root_view->RenderEditMenuOptions();
            }
        }
        if(ImGui::MenuItem("Save Trace Selection", nullptr, false,
                           project && project->IsTrimSaveAllowed()))
        {
            ShowSaveFileDialog("Save Trace Selection", ".db,.rpd", "",
                           [project](std::string file_path) -> void {
                               project->TrimSave(file_path);
                           });
        }
        ImGui::Separator();
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
        LayoutItem* tool_bar_item = m_main_view->GetMutableAt(m_tool_bar_index);
        if(tool_bar_item)
        {
            if(ImGui::MenuItem("Show Tool Bar", nullptr, tool_bar_item->m_visible))
            {
                tool_bar_item->m_visible = !tool_bar_item->m_visible;
            }
            if(ImGui::MenuItem("Show Analysis Bar", nullptr, m_analysis_bar_visible))
            {
                m_analysis_bar_visible = !m_analysis_bar_visible;
                for(const auto& tab : m_tab_container->GetTabs())
                {
                    auto trace_view_tab =
                        std::dynamic_pointer_cast<RocProfVis::View::TraceView>(
                            tab->m_widget);
                    if(trace_view_tab)
                        trace_view_tab->SetAnalysisViewVisibility(m_analysis_bar_visible);
                }
            }
            if(ImGui::MenuItem("Show Side Bar", nullptr, m_sidebar_visible))
            {
                m_sidebar_visible = !m_sidebar_visible;
                for(const auto& tab : m_tab_container->GetTabs())
                {
                    auto trace_view_tab =
                        std::dynamic_pointer_cast<RocProfVis::View::TraceView>(
                            tab->m_widget);
                    if(trace_view_tab)
                        trace_view_tab->SetSidebarViewVisibility(m_sidebar_visible);
                }
            }
            if(ImGui::MenuItem("Show Histogram", nullptr, m_histogram_visible))
            {
                m_histogram_visible = !m_histogram_visible;
                for(const auto& tab : m_tab_container->GetTabs())
                {
                    auto trace_view_tab =
                        std::dynamic_pointer_cast<RocProfVis::View::TraceView>(
                            tab->m_widget);
                    if(trace_view_tab)
                        trace_view_tab->SetHistogramVisibility(m_histogram_visible);
                }
            }
        }
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
#ifdef USE_NATIVE_FILE_DIALOG
    std::string default_path = "";
    std::string filters = "db,rpd,rpv";
#    ifdef JSON_TRACE_SUPPORT
    filters += ",json";
#    endif
#    ifdef COMPUTE_UI_SUPPORT
    filters += ",csv";
#    endif
#else
    std::string default_path = ".";
    std::string trace_types = ".db,.rpd";
#    ifdef JSON_TRACE_SUPPORT
    trace_types += ",.json";
#    endif
#    ifdef COMPUTE_UI_SUPPORT
    trace_types += ",.csv";
    std::string filters = "All (.rpv," + trace_types + "){.rpv," + trace_types +
                          "},Projects (.rpv){.rpv},Traces (" + trace_types + "){" +
                          trace_types + "}";
#    endif
#endif
    ShowOpenFileDialog(
        "Choose File", filters, default_path,
        [this](std::string file_path) -> void { this->OpenFile(file_path); });
}

void
AppWindow::HandleSaveAsFile()
{    
    Project* project = GetCurrentProject();
    if(project)
    {
        #ifdef USE_NATIVE_FILE_DIALOG
        std::string filters = "rpv";
        #else
        std::string filters = "Projects (.rpv){.rpv}";
        #endif

        ShowSaveFileDialog(
            "Save as Project", filters, "",
            [project](std::string file_path) { project->SaveAs(file_path); });
    }
}

void
AppWindow::HandleTabClosed(std::shared_ptr<RocEvent> e)
{
    auto tab_closed_event = std::dynamic_pointer_cast<TabEvent>(e);
    if(tab_closed_event && m_projects.count(tab_closed_event->GetTabId()) > 0)
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
        m_projects[tab_closed_event->GetTabId()]->Close();
        m_projects.erase(tab_closed_event->GetTabId());
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
   
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, m_default_spacing);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, m_default_padding);

        if(ImGui::BeginPopupModal(ABOUT_DIALOG_NAME, nullptr,
                                  ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImFont* large_font =
                SettingsManager::GetInstance().GetFontManager().GetFont(FontType::kLarge);
            if(large_font) ImGui::PushFont(large_font);

            ImGui::SetCursorPosX(
                (ImGui::GetWindowSize().x - ImGui::CalcTextSize("ROCm (TM) Optiq").x) * 0.5f);
            ImGui::Text("ROCm (TM) Optiq");
            if(large_font) ImGui::PopFont();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::SetCursorPosX(
                (ImGui::GetWindowSize().x - ImGui::CalcTextSize("Version 00.00.00").x) *
                0.5f);
            ImGui::Text("Version %d.%d.%d", ROCPROFVIS_VERSION_MAJOR,
                        ROCPROFVIS_VERSION_MINOR, ROCPROFVIS_VERSION_PATCH);

            ImGui::Spacing();

            ImGui::SetCursorPosX(
                (ImGui::GetWindowSize().x -
                 ImGui::CalcTextSize("Copyright (C) 2025 Advanced Micro Devices, Inc. "
                                     "All rights reserved.")
                     .x) *
                0.5f);
            ImGui::Text(
                "Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.");

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
        ImGui::PopStyleVar(2);
    
}

#ifdef USE_NATIVE_FILE_DIALOG
void
AppWindow::UpdateNativeFileDialog()
{
    if(m_is_native_file_dialog_open)
    {
        if(m_open_file_dialog_future.valid() &&
           m_open_file_dialog_future.wait_for(std::chrono::seconds(0)) ==
               std::future_status::ready)
        {
            std::string file_path = m_open_file_dialog_future.get();
            if(!file_path.empty() && m_open_file_dialog_callback)
            {
                m_open_file_dialog_callback(file_path);
            }
            m_is_native_file_dialog_open = false;
            m_open_file_dialog_callback  = nullptr;
        }

        if(m_save_file_dialog_future.valid() &&
           m_save_file_dialog_future.wait_for(std::chrono::seconds(0)) ==
               std::future_status::ready)
        {
            std::string file_path = m_save_file_dialog_future.get();
            if(!file_path.empty() && m_save_file_dialog_callback)
            {
                m_save_file_dialog_callback(file_path);
            }
            m_is_native_file_dialog_open = false;
            m_save_file_dialog_callback  = nullptr;
        }
    }
}    

void
AppWindow::ShowNativeSaveFileDialog(const std::string&               file_filter,
                                    const std::string&               initial_path,
                                    std::function<void(std::string)> callback)
{
    if(m_is_native_file_dialog_open)
    {
        return;
    }
    m_is_native_file_dialog_open = true;
    m_save_file_dialog_callback  = callback;

    m_save_file_dialog_future = std::async(std::launch::async, [=]() -> std::string {
        NFD_Init();
        nfdu8char_t*          outPath    = nullptr;
        nfdu8filteritem_t     filterItem = { "File", file_filter.c_str() };
        nfdsavedialogu8args_t args       = {};
        args.filterList                  = &filterItem;
        args.filterCount                 = 1;
        args.defaultPath                 = initial_path.c_str();
        nfdresult_t result               = NFD_SaveDialogU8_With(&outPath, &args);
        std::string file_path;
        if(result == NFD_OKAY)
        {
            file_path = outPath;
            if(outPath)
            {
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
    });
}

void
AppWindow::ShowNativeOpenFileDialog(const std::string&               file_filter,
                                    const std::string&               initial_path,
                                    std::function<void(std::string)> callback)
{
    if(m_is_native_file_dialog_open)
    {
        return;
    }
    m_is_native_file_dialog_open = true;
    m_open_file_dialog_callback  = callback;

    m_open_file_dialog_future = std::async(std::launch::async, [=]() -> std::string {
        NFD_Init();
        nfdu8char_t*          outPath    = nullptr;
        nfdu8filteritem_t     filterItem = { "Supported Files", file_filter.c_str() };
        nfdopendialogu8args_t args       = {};
        args.filterList                  = &filterItem;
        args.filterCount                 = 1;
        args.defaultPath                 = initial_path.c_str();
        nfdresult_t result               = NFD_OpenDialogU8_With(&outPath, &args);
        std::string file_path;
        if(result == NFD_OKAY)
        {
            file_path = outPath;
            if(outPath)
            {
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
    });
}
#endif

#ifndef USE_NATIVE_FILE_DIALOG
void
AppWindow::ShowFileDialog(const std::string& title, const std::string& file_filter,
                          const std::string& initial_path, const bool& confirm_overwrite,
                          std::function<void(std::string)> callback)
{
    m_file_dialog_callback = callback;
    m_init_file_dialog     = true;
    IGFD::FileDialogConfig config;
    config.path  = initial_path;
    config.flags = confirm_overwrite
                       ? ImGuiFileDialogFlags_Default
                       : ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_HideColumnType;
    ImGuiFileDialog::Instance()->OpenDialog(FILE_DIALOG_NAME, title, file_filter.c_str(),
                                            config);
}
#endif

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
#    ifdef USE_NATIVE_FILE_DIALOG
            std::string default_path         = "";
            std::string supported_extensions = "db,rpd";
#        ifdef JSON_TRACE_SUPPORT
            supported_extensions += ",json";
#        endif
#    else
            std::string supported_extensions = ".db,.rpd";
#        ifdef JSON_TRACE_SUPPORT
            supported_extensions += ",.json";
#        endif
            std::string default_path = ".";
#    endif
            ShowOpenFileDialog("Choose File", supported_extensions, default_path,
                               [this](std::string file_path) -> void {
                                   this->m_test_data_provider.FetchTrace(file_path);
                                   spdlog::info("Opening file: {}", file_path);
                                   m_show_provider_test_widow = true;
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

    if(ImGui::Button("Fetch Single Track Event Table"))
    {
        provider.FetchSingleTrackEventTable(index, provider.GetStartTime(),
                                            provider.GetEndTime(), "", "", "", start_row,
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
        provider.FetchMultiTrackEventTable(vect, provider.GetStartTime(),
                                           provider.GetEndTime(), "", "", "", start_row,
                                           row_count);
    }
    if(ImGui::Button("Print Event Table"))
    {
        provider.DumpTable(TableType::kEventTable);
    }

    if(ImGui::Button("Fetch Single Track Sample Table"))
    {
        provider.FetchSingleTrackSampleTable(index, provider.GetStartTime(),
                                             provider.GetEndTime(), "", start_row,
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
        provider.FetchMultiTrackSampleTable(vect, provider.GetStartTime(),
                                            provider.GetEndTime(), "", start_row,
                                            row_count);
    }
    if(ImGui::Button("Print Sample Table"))
    {
        provider.DumpTable(TableType::kSampleTable);
    }

    ImGui::Separator();

    if(ImGui::Button("Fetch Track"))
    {
        provider.FetchTrack(index, provider.GetStartTime(), provider.GetEndTime(), 1000,
                            group_id_counter++);
    }

    if(ImGui::Button("Fetch Whole Track"))
    {
        provider.FetchWholeTrack(index, provider.GetStartTime(), provider.GetEndTime(),
                                 1000, group_id_counter++);
    }
    if(ImGui::Button("Delete Track"))
    {
        provider.FreeTrack(index);
    }
    if(ImGui::Button("Print Track"))
    {
        provider.DumpTrack(index);
    }
    if(ImGui::Button("Print Track List"))
    {
        provider.DumpMetaData();
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
