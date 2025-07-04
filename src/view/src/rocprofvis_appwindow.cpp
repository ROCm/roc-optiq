// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_appwindow.h"
#include "ImGuiFileDialog.h"
#include "imgui.h"
#include "implot.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_events.h"
#include "widgets/rocprofvis_debug_window.h"
#ifdef COMPUTE_UI_SUPPORT
#include "rocprofvis_navigation_manager.h"
#endif
#include <filesystem>

using namespace RocProfVis::View;

constexpr ImVec2 FILE_DIALOG_SIZE = ImVec2(480.0f, 360.0f);

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
: m_show_debug_window(false)
, m_show_provider_test_widow(false)
, m_show_metrics(false)
, m_main_view(nullptr)
, m_tabclosed_event_token(static_cast<EventManager::SubscriptionToken>(-1))
{}

AppWindow::~AppWindow()
{
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kTabClosed),
                                             m_tabclosed_event_token);

    m_open_views.clear();
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

    LayoutItem status_bar_item(-1, 30.0f);
    status_bar_item.m_item = std::make_shared<RocWidget>();
    LayoutItem main_area_item(-1, -30.0f);

    m_tab_container = std::make_shared<TabContainer>();
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

void
AppWindow::Update()
{
    EventManager::GetInstance()->DispatchEvents();

    DebugWindow::GetInstance()->ClearTransient();
    m_data_provider.Update();
    m_tab_container->Update();
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
            if(ImGui::MenuItem("Open", "CTRL+O"))
            {
                IGFD::FileDialogConfig config;
                config.path                      = ".";
                std::string supported_extensions = ".db,.rpd";
#ifdef JSON_SUPPORT
                supported_extensions += ",.json";
#endif
#ifdef COMPUTE_UI_SUPPORT
                supported_extensions += ",.csv";
#endif
                ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File",
                                                        supported_extensions.c_str(),
                                                        config);
            }
            
            ImGui::EndMenu();
        }

        RenderSettingsMenu();
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

    ImGui::End();
    // Pop ImGuiStyleVar_ItemSpacing, ImGuiStyleVar_WindowPadding,
    // ImGuiStyleVar_WindowRounding
    ImGui::PopStyleVar(3);

    // handle Dialog stuff
    ImGui::SetNextWindowPos(
        ImVec2(m_default_spacing.x, m_default_spacing.y + ImGui::GetFrameHeight()),
        ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(FILE_DIALOG_SIZE, ImGuiCond_Appearing);
    if(ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey"))
    {
        if(ImGuiFileDialog::Instance()->IsOk())
        {
            std::filesystem::path file_path(
                ImGuiFileDialog::Instance()->GetFilePathName());

            std::string file_path_str = file_path.string();

            // Check if the file is already opened using our m_open_views map
            auto it = m_open_views.find(file_path_str);
            if(it != m_open_views.end())
            {
                // File is already opened, just switch to that tab
                m_tab_container->SetActiveTab(it->second.m_id);
            }
            else
            {
                TabItem tab_item;
                tab_item.m_label     = file_path.filename().string();
                tab_item.m_id        = file_path_str;
                tab_item.m_can_close = true;
#ifdef COMPUTE_UI_SUPPORT
                // Determine the type of view to create based on the file extension
                if(file_path.extension().string() == ".csv")
                {
                    auto compute_view = std::make_shared<ComputeRoot>();
                    compute_view->OpenTrace(file_path.parent_path().string());
                    tab_item.m_widget = compute_view;
                    spdlog::info("Opening file: {}", file_path.string());
                    m_tab_container->AddTab(tab_item);
                    m_open_views[file_path_str] = tab_item;
                    NavigationManager::GetInstance()->RefreshNavigationTree();
                }
                else
#endif
                {
                    auto trace_view = std::make_shared<TraceView>();
                    trace_view->OpenFile(file_path.string());
                    tab_item.m_widget = trace_view;
                    spdlog::info("Opening file: {}", file_path.string());
                    m_tab_container->AddTab(tab_item);
                    m_open_views[file_path_str] = tab_item;
                }
            }
        }

        ImGuiFileDialog::Instance()->Close();
    }

    ImGui::SetNextWindowPos(
        ImVec2(m_default_spacing.x, m_default_spacing.y + ImGui::GetFrameHeight()),
        ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(FILE_DIALOG_SIZE, ImGuiCond_Appearing);
    if(ImGuiFileDialog::Instance()->Display("DebugFile"))
    {
        if(ImGuiFileDialog::Instance()->IsOk())
        {
            std::string file_path = ImGuiFileDialog::Instance()->GetFilePathName();

            m_data_provider.FetchTrace(file_path);
            spdlog::info("Opening file: {}", file_path);

            m_show_provider_test_widow = true;
        }

        ImGuiFileDialog::Instance()->Close();
    }

#ifdef ROCPROFVIS_DEVELOPER_MODE
    RenderDebugOuput();
    // handle debug window
    if(m_show_provider_test_widow)
    {
        RenderProviderTest(m_data_provider);
    }
#endif
}

void
AppWindow::RenderSettingsMenu()
{
    if(ImGui::BeginMenu("Settings"))
    {
        if(ImGui::MenuItem("Light Theme", nullptr, !Settings::GetInstance().IsDarkMode()))
        {
            Settings::GetInstance().LightMode();
        }
        if(ImGui::MenuItem("Dark Theme", nullptr, Settings::GetInstance().IsDarkMode()))
        {
            Settings::GetInstance().DarkMode();
        }

        ImGui::EndMenu();
    }
}

void
AppWindow::HandleTabClosed(std::shared_ptr<RocEvent> e)
{
    auto tab_closed_event = std::dynamic_pointer_cast<TabClosedEvent>(e);
    if(tab_closed_event)
    {
        auto it = m_open_views.find(tab_closed_event->GetTabId());
        if(it != m_open_views.end())
        {
            m_open_views.erase(it);
        }
    }
#ifdef COMPUTE_UI_SUPPORT
    NavigationManager::GetInstance()->RefreshNavigationTree();
#endif
}

#ifdef ROCPROFVIS_DEVELOPER_MODE
void
AppWindow::RenderDeveloperMenu()
{
    if(ImGui::BeginMenu("Developer Options"))
    {
        if(ImGui::MenuItem("Horizontal Render", nullptr,
                           Settings::GetInstance().IsHorizontalRender()))
        {
            Settings::GetInstance().HorizontalRender();
        }
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
#ifdef JSON_SUPPORT
            supported_extensions += ",.json";
#endif
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

    static char track_index_buffer[64]     = "0";
    static char end_track_index_buffer[64] = "1";  // for setting table track range

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
                                            provider.GetEndTime(), start_row, row_count);
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
                                           provider.GetEndTime(), start_row, row_count);
    }
    if(ImGui::Button("Print Event Table"))
    {
        provider.DumpTable(TableType::kEventTable);
    }

    if(ImGui::Button("Fetch Single Track Sample Table"))
    {
        provider.FetchSingleTrackSampleTable(index, provider.GetStartTime(),
                                             provider.GetEndTime(), start_row, row_count);
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
                                            provider.GetEndTime(), start_row, row_count);
    }
    if(ImGui::Button("Print Sample Table"))
    {
        provider.DumpTable(TableType::kSampleTable);
    }

    ImGui::Separator();

    if(ImGui::Button("Fetch Track"))
    {
        provider.FetchTrack(index, provider.GetStartTime(), provider.GetEndTime(), 1000);
    }

    if(ImGui::Button("Fetch Whole Track"))
    {
        provider.FetchWholeTrack(index, provider.GetStartTime(), provider.GetEndTime(),
                                 1000);
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

    if(ImGui::IsKeyPressed(ImGuiKey_D))
    {
        m_show_debug_window = !m_show_debug_window;

        if(m_show_debug_window)
        {
            ImGui::SetWindowFocus("Debug Window");
        }
    }
}
#endif  // ROCPROFVIS_DEVELOPER_MODE
