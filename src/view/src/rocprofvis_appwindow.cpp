// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_appwindow.h"
#include "ImGuiFileDialog.h"
#include "imgui.h"
#include "implot.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_events.h"
#include "rocprofvis_project.h"
#include "rocprofvis_settings.h"
#include "rocprofvis_settings_panel.h"
#include "rocprofvis_trace_view.h"
#include "rocprofvis_version.h"
#ifdef COMPUTE_UI_SUPPORT
#    include "rocprofvis_navigation_manager.h"
#endif
#include "widgets/rocprofvis_debug_window.h"
#include "widgets/rocprofvis_dialog.h"
#include "widgets/rocprofvis_notification_manager.h"
#include <filesystem>

namespace RocProfVis
{
namespace View
{

constexpr ImVec2 FILE_DIALOG_SIZE         = ImVec2(480.0f, 360.0f);
constexpr char*  FILE_DIALOG_NAME         = "ChooseFileDlgKey";
constexpr char*  FILE_SAVE_DIALOG_NAME    = "SaveFileDlgKey";
constexpr char*  PROJECT_SAVE_DIALOG_NAME = "SaveProjectDlgKey";
constexpr char*  TAB_CONTAINER_SRC_NAME   = "MainTabContainer";
constexpr char*  ABOUT_DIALOG_NAME        = "About##_dialog";

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
, m_settings_panel(std::make_unique<SettingsPanel>())
, m_tab_container(nullptr)
, m_default_padding(0.0f, 0.0f)
, m_default_spacing(0.0f, 0.0f)
, m_open_about_dialog(false)
, m_tabclosed_event_token(static_cast<EventManager::SubscriptionToken>(-1))
#ifdef ROCPROFVIS_DEVELOPER_MODE
, m_show_debug_window(false)
, m_show_provider_test_widow(false)
, m_show_metrics(false)
#endif
, m_confirmation_dialog(std::make_unique<ConfirmationDialog>())
, m_message_dialog(std::make_unique<MessageDialog>())
{}

AppWindow::~AppWindow()
{
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kTabClosed),
                                             m_tabclosed_event_token);
    m_projects.clear();
#ifdef COMPUTE_UI_SUPPORT
    NavigationManager::DestroyInstance();
#endif
}

bool
AppWindow::Init()
{
    ImPlot::CreateContext();

    // setup fonts
    bool result = Settings::GetInstance().GetFontManager().Init();
    if(!result)
    {
        spdlog::warn("Failed to initialize fonts");
    }
    else
    {
        Settings::GetInstance().LoadSettings("settings_application.json");
    }

    LayoutItem status_bar_item(-1, 30.0f);
    status_bar_item.m_item = std::make_shared<RocWidget>();
    LayoutItem main_area_item(-1, -30.0f);

    m_tab_container = std::make_shared<TabContainer>();
    m_tab_container->SetEventSourceName(TAB_CONTAINER_SRC_NAME);
#ifdef COMPUTE_UI_SUPPORT
    NavigationManager::GetInstance()->RegisterRootContainer(m_tab_container);
#endif
    main_area_item.m_item = m_tab_container;

    std::vector<LayoutItem> layout_items;
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

    return result;
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
    EventManager::GetInstance()->DispatchEvents();
    DebugWindow::GetInstance()->ClearTransient();
    m_tab_container->Update();
#ifdef ROCPROFVIS_DEVELOPER_MODE
    m_test_data_provider.Update();
#endif
}

bool
AppWindow::IsTrimSaveAllowed()
{
    // Check if save is allowed
    bool save_allowed = false;
    auto active_tab   = m_tab_container->GetActiveTab();
    if(active_tab)
    {
        // Check if the active tab is a TraceView
        auto trace_view = std::dynamic_pointer_cast<TraceView>(active_tab->m_widget);
        if(trace_view)
        {
            // Check if the trace view has a selection that can be saved
            save_allowed = trace_view->IsTrimSaveAllowed();
        }
    }
    return save_allowed;
}

void
AppWindow::Render()
{
    Update();

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
        if(ImGui::BeginMenu("File"))
        {
            if(ImGui::MenuItem("Open", nullptr))
            {
                IGFD::FileDialogConfig config;
                config.path                      = ".";
                std::string supported_extensions = ".rpv,.db,.rpd,";
#ifdef JSON_SUPPORT
                supported_extensions += ",.json";
#endif
#ifdef COMPUTE_UI_SUPPORT
                supported_extensions += ",.csv";
#endif
                ImGuiFileDialog::Instance()->OpenDialog(FILE_DIALOG_NAME, "Choose File",
                                                        supported_extensions.c_str(),
                                                        config);
            }
            Project* project = GetCurrentProject();
            if(ImGui::MenuItem("Save", nullptr, false, project && project->IsProject()))
            {
                project->Save();
            }
            if(ImGui::MenuItem("Save As", nullptr, false, project))
            {
                ImGuiFileDialog::Instance()->OpenDialog(PROJECT_SAVE_DIALOG_NAME,
                                                        "Save as Project", ".rpv");
            }
            if(ImGui::MenuItem("Save Selection", nullptr, false, IsTrimSaveAllowed()))
            {
                // Save the currently selected tab's content
                auto active_tab = m_tab_container->GetActiveTab();
                if(active_tab)
                {
                    // Open the save dialog
                    ImGuiFileDialog::Instance()->OpenDialog(FILE_SAVE_DIALOG_NAME,
                                                            "Save Selection", ".db,.rpd");
                }
            }
            ImGui::EndMenu();
        }

        RenderSettingsMenu();
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

    if(m_settings_panel->IsOpen())
    {
        m_settings_panel->Render();
    }

    ImGui::End();
    // Pop ImGuiStyleVar_ItemSpacing, ImGuiStyleVar_WindowPadding,
    // ImGuiStyleVar_WindowRounding
    ImGui::PopStyleVar(3);

    RenderFileDialogs();
#ifdef ROCPROFVIS_DEVELOPER_MODE
    RenderDebugOuput();
#endif

    // render notifications last
    NotificationManager::GetInstance().Render();
}

void
AppWindow::RenderFileDialogs()
{
    if(!ImGuiFileDialog::Instance()->IsOpened(FILE_DIALOG_NAME) &&
       !ImGuiFileDialog::Instance()->IsOpened(FILE_SAVE_DIALOG_NAME) &&
       !ImGuiFileDialog::Instance()->IsOpened(PROJECT_SAVE_DIALOG_NAME))
    {
        return;  // No file dialog is opened, nothing to render
    }

    // Set Itemspacing to values from original default ImGui style
    // custom values to break the 3rd party file dialog implementation
    // especially the cell padding
    auto defaultStyle = Settings::GetInstance().GetDefaultStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, defaultStyle.ItemSpacing);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, defaultStyle.WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, defaultStyle.CellPadding);

    ImGui::SetNextWindowPos(
        ImVec2(m_default_spacing.x, m_default_spacing.y + ImGui::GetFrameHeight()),
        ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(FILE_DIALOG_SIZE, ImGuiCond_Appearing);
    if(ImGuiFileDialog::Instance()->Display(FILE_DIALOG_NAME))
    {
        if(ImGuiFileDialog::Instance()->IsOk())
        {
            std::filesystem::path file_path(
                ImGuiFileDialog::Instance()->GetFilePathName());

            std::string file_path_str = file_path.string();

            spdlog::info("Opening file: {}", file_path_str);

            std::unique_ptr<Project> project = std::make_unique<Project>();
            switch(project->Open(file_path_str))
            {
                case Project::OpenResult::Success:
                {
                    m_tab_container->AddTab(TabItem{ project->GetName(), project->GetID(),
                                                     project->GetView(), true });
                    m_projects[project->GetID()] = std::move(project);
                    break;
                }
                case Project::OpenResult::Duplicate:
                {
                    // File is already opened, just switch to that tab
                    m_tab_container->SetActiveTab(file_path_str);
                    break;
                }
                default:
                {
                    break;
                }
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }
    // Handle file save dialog
    if(ImGuiFileDialog::Instance()->Display(FILE_SAVE_DIALOG_NAME))
    {
        if(ImGuiFileDialog::Instance()->IsOk())
        {
            std::filesystem::path file_path(
                ImGuiFileDialog::Instance()->GetFilePathName());

            std::string file_path_str = file_path.string();
            HandleSaveSelection(file_path_str);
        }
        ImGuiFileDialog::Instance()->Close();
    }
    Project* project = GetCurrentProject();
    if(ImGuiFileDialog::Instance()->Display(PROJECT_SAVE_DIALOG_NAME) && project)
    {
        if(ImGuiFileDialog::Instance()->IsOk())
        {
            std::filesystem::path file_path(
                ImGuiFileDialog::Instance()->GetFilePathName());

            std::string file_path_str = file_path.string();
            project->SaveAs(file_path_str);
        }
        ImGuiFileDialog::Instance()->Close();
    }

    ImGui::PopStyleVar(3);
}

void
AppWindow::RenderSettingsMenu()
{
    if(ImGui::BeginMenu("Settings"))
    {
        if(ImGui::MenuItem("Display Settings"))
        {
            m_settings_panel->SetOpen(true);
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
AppWindow::HandleTabClosed(std::shared_ptr<RocEvent> e)
{
    auto tab_closed_event = std::dynamic_pointer_cast<TabEvent>(e);
    if(tab_closed_event && m_projects.count(tab_closed_event->GetTabId()) > 0)
    {
        m_projects[tab_closed_event->GetTabId()]->Close();
        m_projects.erase(tab_closed_event->GetTabId());
    }
}

void
AppWindow::HandleSaveSelection(const std::string& file_path_str)
{
    // Check if file already exists
    std::error_code ec;
    if(std::filesystem::exists(file_path_str, ec))
    {
        // Show confirmation dialog
        m_confirmation_dialog->Show(
            "Confirm Save", "File already exists. Do you want to overwrite it?",
            [this, file_path_str]() { SaveSelection(file_path_str); });
        return;
    }
    else
    {
        SaveSelection(file_path_str);
    }
}

void
AppWindow::SaveSelection(const std::string& file_path_str)
{
    // Get the active tab
    auto active_tab = m_tab_container->GetActiveTab();
    if(active_tab)
    {
        // Check if active tab is a trace view
        auto trace_view = std::dynamic_pointer_cast<TraceView>(active_tab->m_widget);
        if(trace_view)
        {
            trace_view->SaveSelection(file_path_str);
        }
        else
        {
            spdlog::warn("Active tab is not a Trace View, cannot save selection.");
        }
    }
    else
    {
        spdlog::warn("No active tab to save selection from.");
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
        ImGui::Text("RocProfiler Visualizer");
        ImGui::Text("Version %d.%d.%d", ROCPROFVIS_VERSION_MAJOR,
                    ROCPROFVIS_VERSION_MINOR, ROCPROFVIS_VERSION_PATCH);
        ImGui::Text(
            "Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.");

        if(ImGui::Button("Close"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);  // Pop ImGuiStyleVar_ItemSpacing, ImGuiStyleVar_WindowPadding
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
            IGFD::FileDialogConfig config;
            config.path                      = ".";
            std::string supported_extensions = ".db,.rpd";
#    ifdef JSON_SUPPORT
            supported_extensions += ",.json";
#    endif
            ImGuiFileDialog::Instance()->OpenDialog("DebugFile", "Choose File",
                                                    supported_extensions.c_str(), config);
        }

        ImGui::EndMenu();
    }
}

void
RenderProviderTest(DataProvider& provider)
{
    ImGui::Begin("Data Provider Test Window", nullptr, ImGuiWindowFlags_None);

    static char     track_index_buffer[64]     = "0";
    static char     end_track_index_buffer[64] = "1";  // for setting table track range
    static uint64_t group_id_counter           = 0;

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
    ImGui::SetNextWindowPos(
        ImVec2(m_default_spacing.x, m_default_spacing.y + ImGui::GetFrameHeight()),
        ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(FILE_DIALOG_SIZE, ImGuiCond_Appearing);
    if(ImGuiFileDialog::Instance()->Display("DebugFile"))
    {
        if(ImGuiFileDialog::Instance()->IsOk())
        {
            std::string file_path = ImGuiFileDialog::Instance()->GetFilePathName();

            m_test_data_provider.FetchTrace(file_path);
            spdlog::info("Opening file: {}", file_path);

            m_show_provider_test_widow = true;
        }

        ImGuiFileDialog::Instance()->Close();
    }

    if(m_show_metrics)
    {
        ImGui::ShowMetricsWindow(&m_show_metrics);
    }

    if(m_show_debug_window)
    {
        DebugWindow::GetInstance()->Render();
    }

    if(ImGui::IsKeyPressed(ImGuiKey_D))
    {
        m_show_debug_window = !m_show_debug_window;

        if(m_show_debug_window)
        {
            ImGui::SetWindowFocus("Debug Window");
        }
    }

    if(m_show_provider_test_widow)
    {
        RenderProviderTest(m_test_data_provider);
    }
}
#endif  // ROCPROFVIS_DEVELOPER_MODE

}  // namespace View
}  // namespace RocProfVis
