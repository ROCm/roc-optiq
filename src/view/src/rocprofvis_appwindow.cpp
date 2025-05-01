// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_appwindow.h"
#include "ImGuiFileDialog.h"
#include "imgui.h"
#include "implot.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_events.h"
#include "widgets/rocprofvis_debug_window.h"

using namespace RocProfVis::View;

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
: m_show_debug_widow(false)
, m_show_provider_test_widow(false)
{}

AppWindow::~AppWindow() {}

bool
AppWindow::Init()
{
    ImPlot::CreateContext();

    LayoutItem status_bar_item(-1, 30.0f);
    status_bar_item.m_item = std::make_shared<RocWidget>();
    LayoutItem main_area_item(-1, -30.0f);
    m_home_screen         = std::make_shared<TraceView>();
    main_area_item.m_item = m_home_screen;

    std::vector<LayoutItem> layout_items;
    layout_items.push_back(main_area_item);
    layout_items.push_back(status_bar_item);
    m_main_view = std::make_shared<VFixedContainer>(layout_items);

    m_compute_root = std::make_shared<ComputeRoot>();

    m_default_padding = ImGui::GetStyle().WindowPadding;
    m_default_spacing = ImGui::GetStyle().ItemSpacing;

    return true;
}

void
AppWindow::Update()
{
    EventManager::GetInstance()->DispatchEvents();

    DebugWindow::GetInstance()->ClearTransient();
    m_data_provider.Update();
    if(m_home_screen)
    {
        m_home_screen->Update();
    }

    if(m_compute_root)
    {
        m_compute_root->Update();
    }
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
                std::string supported_extensions = ".db,.rpd,.csv";
#ifdef JSON_SUPPORT
                supported_extensions += ",.json";
#endif
                ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File",
                                                        supported_extensions.c_str(),
                                                        config);
            }

            if(ImGui::MenuItem("Test Provider", "CTRL+T"))
            {
                IGFD::FileDialogConfig config;
                config.path                      = ".";
                std::string supported_extensions = ".db,.rpd";
#ifdef JSON_SUPPORT
                supported_extensions += ",.json";
#endif
                ImGuiFileDialog::Instance()->OpenDialog(
                    "DebugFile", "Choose File", supported_extensions.c_str(), config);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    ImGui::PopStyleVar(2);  // Pop ImGuiStyleVar_ItemSpacing, ImGuiStyleVar_WindowPadding

    // Show main view container
    if(m_compute_root && m_compute_root->MetricsLoaded())
    {
        m_compute_root->Render();
    }
    else if(m_main_view)
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
    ImGui::SetNextWindowSize(ImVec2(480.0f, 360.0f), ImGuiCond_Appearing);
    if(ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey"))
    {
        if(ImGuiFileDialog::Instance()->IsOk())
        {
            std::filesystem::path file_path(
                ImGuiFileDialog::Instance()->GetFilePathName());
            if(m_compute_root && file_path.extension().string() == ".csv")
            {
                m_compute_root->SetMetricsPath(file_path.parent_path());
            }
            else if(m_home_screen)
            {
                m_home_screen->OpenFile(file_path.string());
                spdlog::info("Opening file: {}", file_path.string());
            }
        }

        ImGuiFileDialog::Instance()->Close();
    }

    ImGui::SetNextWindowPos(
        ImVec2(m_default_spacing.x, m_default_spacing.y + ImGui::GetFrameHeight()),
        ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(480.0f, 360.0f), ImGuiCond_Appearing);
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

    RenderDebugOuput();
    if(m_show_provider_test_widow)
    {
        RenderProviderTest(m_data_provider);
    }
}

void
RenderProviderTest(DataProvider& provider)
{
    ImGui::Begin("Data Provider Test Window", nullptr, ImGuiWindowFlags_None);

    static char buffer[10] = "";  // Buffer to hold the user input

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

    // InputText with numeric filtering
    ImGui::InputText("Track index", buffer, IM_ARRAYSIZE(buffer),
                     ImGuiInputTextFlags_CallbackCharFilter, NumericFilter);

    int index = std::atoi(buffer);

    if(ImGui::Button("Fetch"))
    {
        provider.FetchTrack(index, provider.GetStartTime(), provider.GetEndTime(), 1000,
                            0);
    }
    if(ImGui::Button("Delete"))
    {
        provider.FreeTrack(index);
    }
    if(ImGui::Button("Print"))
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
    if(m_show_debug_widow)
    {
        DebugWindow::GetInstance()->Render();
    }

    ImGuiIO& io = ImGui::GetIO();
    if(ImGui::IsKeyPressed(ImGuiKey_D))
    {
        m_show_debug_widow = !m_show_debug_widow;

        if(m_show_debug_widow)
        {
            ImGui::SetWindowFocus("Debug Window");
        }
    }
}
