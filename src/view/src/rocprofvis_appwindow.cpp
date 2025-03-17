#include "rocprofvis_appwindow.h"

#include "ImGuiFileDialog.h"
#include "imgui.h"
#include "implot.h"
#include "json.h"

#include <iostream>

using namespace RocProfVis::View;

AppWindow * AppWindow::m_instance = nullptr;

AppWindow* AppWindow::getInstance() {
    if(!m_instance) {
        m_instance = new AppWindow();
    }

    return m_instance;
}

AppWindow::AppWindow()
: data_changed(false)
, is_loading_trace(false)
{}

AppWindow::~AppWindow() {}

bool
AppWindow::Init()
{
    ImPlot::CreateContext();

    trace_object.m_min_ts          = DBL_MAX;
    trace_object.m_max_ts          = 0.0;
    trace_object.m_is_trace_loaded = false;

    LayoutItem statusBarItem(-1, 30.0f);
    statusBarItem.m_item = std::make_shared<RocWidget>();
    LayoutItem mainAreaItem(-1, -30.0f);
    home_screen       = std::make_shared<HomeScreen>();
    mainAreaItem.m_item = home_screen;

    std::vector<LayoutItem> layoutItems;
    layoutItems.push_back(mainAreaItem);
    layoutItems.push_back(statusBarItem);
    main_view = std::make_shared<VFixedContainer>(layoutItems);

    return true;
}

void
AppWindow::Render()
{
    std::map<std::string, rocprofvis_trace_process_t>& trace_data =
        trace_object.m_trace_data;

    if(home_screen && data_changed)
    {
        home_screen->SetData(trace_data);
        data_changed = false;
    }

#ifdef IMGUI_HAS_VIEWPORT
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetWorkPos());
    ImGui::SetNextWindowSize(viewport->GetWorkSize());
    ImGui::SetNextWindowViewport(viewport->ID);
#else
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
#endif
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));   // Controls spacing between items
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); // X, Y padding
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    
    ImGui::Begin("Main Window", nullptr,
                 ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoResize);

    if(ImGui::BeginMenuBar())
    {
        if(ImGui::BeginMenu("File"))
        {
            if(ImGui::MenuItem("Open", "CTRL+O"))
            {
                IGFD::FileDialogConfig config;
                config.path = ".";
                ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File",
                                                        ".json", config);
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    if(trace_object.m_is_trace_loaded)
    {
        // Show home screen
        if(main_view)
        {
            main_view->Render();
        }
    }

    ImGui::End();
    ImGui::PopStyleVar(3);

    // handle Dialog stuff
    if(ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey"))
    {
        trace_object.m_is_trace_loaded = false;
        std::cout << "This happened" << std::endl;

        if(ImGuiFileDialog::Instance()->IsOk())
        {
            std::string file_path = ImGuiFileDialog::Instance()->GetFilePathName();
            trace_object.m_loading_future =
                rocprofvis_trace_async_load_json_trace(file_path, trace_object);
            is_loading_trace = true;
        }

        ImGuiFileDialog::Instance()->Close();
    }

    if(rocprofvis_trace_is_loading(trace_object.m_loading_future))
    {
        std::chrono::milliseconds timeout = std::chrono::milliseconds::min();
        if(rocprofvis_trace_is_loaded(trace_object.m_loading_future))
        {
            trace_object.m_is_trace_loaded = trace_object.m_loading_future.get();
            is_loading_trace               = false;
            data_changed                   = true;
            ImGui::CloseCurrentPopup();
        }
        else
        {
            if(ImGui::BeginPopupModal(
                   "Loading"))  //, NULL, ImGuiWindowFlags_AlwaysAutoResize)
            {
                ImGui::Text("Please wait...");
                ImGui::EndPopup();
            }

            if(is_loading_trace)
            {
                ImGui::SetNextWindowSize(ImVec2(300, 200));
                ImGui::OpenPopup("Loading");
            }
        }
    }
}
