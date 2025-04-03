// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_appwindow.h"
#include "ImGuiFileDialog.h"
#include "imgui.h"
#include "implot.h"
#include "rocprofvis_controller.h"
#include "widgets/rocprofvis_debug_window.h"

using namespace RocProfVis::View;

AppWindow* AppWindow::m_instance = nullptr;

AppWindow*
AppWindow::GetInstance()
{
    if(!m_instance)
    {
        m_instance = new AppWindow();
    }

    return m_instance;
}

AppWindow::AppWindow()
: m_data_changed(false)
, m_is_loading_trace(false)
, m_is_trace_loaded(false)
, m_trace_future(nullptr)
, m_trace_controller(nullptr)
, m_trace_timeline(nullptr)
, m_graph_data_array(nullptr)
, m_graph_futures(nullptr)
, m_show_debug_widow(false)
{}

AppWindow::~AppWindow()
{
    if(m_trace_controller)
    {
        rocprofvis_controller_free(m_trace_controller);
        m_trace_controller = nullptr;
    }
}

bool
AppWindow::Init()
{
    ImPlot::CreateContext();

    LayoutItem statusBarItem(-1, 30.0f);
    statusBarItem.m_item = std::make_shared<RocWidget>();
    LayoutItem mainAreaItem(-1, -30.0f);
    m_home_screen       = std::make_shared<HomeScreen>();
    mainAreaItem.m_item = m_home_screen;

    std::vector<LayoutItem> layoutItems;
    layoutItems.push_back(mainAreaItem);
    layoutItems.push_back(statusBarItem);
    m_main_view = std::make_shared<VFixedContainer>(layoutItems);

    return true;
}

void
AppWindow::HandleOpenFile(std::string& file_path)
{
    // only one controller at time
    if(m_trace_controller)
    {
        rocprofvis_controller_free(m_trace_controller);
        m_trace_controller = nullptr;
    }

    m_trace_controller = rocprofvis_controller_alloc();
    if(m_trace_controller)
    {
        rocprofvis_result_t result = kRocProfVisResultUnknownError;
        m_trace_future             = rocprofvis_controller_future_alloc();
        if(m_trace_future)
        {
            result = rocprofvis_controller_load_async(m_trace_controller,
                                                      file_path.c_str(), m_trace_future);
            assert(result == kRocProfVisResultSuccess);

            if(result != kRocProfVisResultSuccess)
            {
                rocprofvis_controller_future_free(m_trace_future);
                m_trace_future = nullptr;
            }
        }
        if(result != kRocProfVisResultSuccess)
        {
            rocprofvis_controller_free(m_trace_controller);
            m_trace_controller = nullptr;
        }
    }
}

void
AppWindow::Render()
{
    DebugWindow::GetInstance()->Reset();

    if(m_home_screen && m_data_changed)
    {
        m_home_screen->SetData(m_trace_timeline, m_graph_data_array);
        m_data_changed = false;

        rocprofvis_controller_array_free(m_graph_data_array);
        m_graph_data_array = nullptr;
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
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(0, 0));  // Controls spacing between items
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));  // X, Y padding
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    ImGui::Begin("Main Window", nullptr,
                 ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus);

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

    if(m_is_trace_loaded)
    {
        // Show home screen
        if(m_main_view)
        {
            m_main_view->Render();
        }
    }

    ImGui::End();
    ImGui::PopStyleVar(3);

    // handle Dialog stuff
    if(ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey"))
    {
        m_is_trace_loaded = false;
        if(ImGuiFileDialog::Instance()->IsOk())
        {
            std::string file_path = ImGuiFileDialog::Instance()->GetFilePathName();

            HandleOpenFile(file_path);
            m_is_loading_trace = true;
        }

        ImGuiFileDialog::Instance()->Close();
    }

    // load data
    if(m_trace_future || m_graph_futures)
    {
        if(m_trace_future)
        {
            rocprofvis_result_t result =
                rocprofvis_controller_future_wait(m_trace_future, 0);
            assert(result == kRocProfVisResultSuccess ||
                   result == kRocProfVisResultTimeout);
            if(result == kRocProfVisResultSuccess)
            {
                uint64_t uint64_result = 0;
                result                 = rocprofvis_controller_get_uint64(
                    m_trace_future, kRPVControllerFutureResult, 0, &uint64_result);
                assert(result == kRocProfVisResultSuccess &&
                       uint64_result == kRocProfVisResultSuccess);

                result = rocprofvis_controller_get_object(
                    m_trace_controller, kRPVControllerTimeline, 0, &m_trace_timeline);

                if(result == kRocProfVisResultSuccess && m_trace_timeline)
                {
                    uint64_t num_graphs = 0;
                    result              = rocprofvis_controller_get_uint64(
                        m_trace_timeline, kRPVControllerTimelineNumGraphs, 0,
                        &num_graphs);

                    double min_ts = 0;
                    result        = rocprofvis_controller_get_double(
                        m_trace_timeline, kRPVControllerTimelineMinTimestamp, 0, &min_ts);

                    double max_ts = 0;
                    result        = rocprofvis_controller_get_double(
                        m_trace_timeline, kRPVControllerTimelineMaxTimestamp, 0, &max_ts);

                    m_graph_data_array = rocprofvis_controller_array_alloc(num_graphs);
                    m_graph_futures    = rocprofvis_controller_array_alloc(num_graphs);

                    for(uint32_t i = 0;
                        i < num_graphs && result == kRocProfVisResultSuccess; i++)
                    {
                        rocprofvis_controller_future_t* graph_future =
                            rocprofvis_controller_future_alloc();
                        rocprofvis_controller_array_t* graph_array =
                            rocprofvis_controller_array_alloc(32);
                        rocprofvis_handle_t* graph = nullptr;
                        result                     = rocprofvis_controller_get_object(
                            m_trace_timeline, kRPVControllerTimelineGraphIndexed, i,
                            &graph);
                        if(result == kRocProfVisResultSuccess && graph && graph_future &&
                           graph_array)
                        {
                            rocprofvis_handle_t* track = nullptr;
                            result                     = rocprofvis_controller_get_object(
                                graph, kRPVControllerGraphTrack, 0, &track);
                            if(result == kRocProfVisResultSuccess)
                            {
                                result = rocprofvis_controller_graph_fetch_async(
                                    m_trace_controller, graph, min_ts, max_ts, 1000,
                                    graph_future, graph_array);
                                if(result == kRocProfVisResultSuccess)
                                {
                                    result = rocprofvis_controller_set_object(
                                        m_graph_data_array,
                                        kRPVControllerArrayEntryIndexed, i, graph_array);
                                    assert(result == kRocProfVisResultSuccess);

                                    result = rocprofvis_controller_set_object(
                                        m_graph_futures, kRPVControllerArrayEntryIndexed,
                                        i, graph_future);
                                    assert(result == kRocProfVisResultSuccess);
                                }
                            }
                        }
                    }
                }

                rocprofvis_controller_future_free(m_trace_future);
                m_trace_future = nullptr;
            }
        }
        else if(m_graph_futures)
        {
            uint64_t            num_tracks = 0;
            rocprofvis_result_t result     = rocprofvis_controller_get_uint64(
                m_graph_futures, kRPVControllerArrayNumEntries, 0, &num_tracks);
            assert(result == kRocProfVisResultSuccess);

            for(uint32_t i = 0; i < num_tracks && result == kRocProfVisResultSuccess; i++)
            {
                rocprofvis_handle_t* future = nullptr;
                result                      = rocprofvis_controller_get_object(
                    m_graph_futures, kRPVControllerArrayEntryIndexed, i, &future);
                assert(result == kRocProfVisResultSuccess && future);

                rocprofvis_result_t result = rocprofvis_controller_future_wait(
                    (rocprofvis_controller_future_t*) future, 0);
                assert(result == kRocProfVisResultSuccess ||
                       result == kRocProfVisResultTimeout);

                if(result != kRocProfVisResultSuccess)
                {
                    break;
                }
            }

            if(result != kRocProfVisResultTimeout)
            {
                for(uint32_t i = 0; i < num_tracks && result == kRocProfVisResultSuccess;
                    i++)
                {
                    rocprofvis_handle_t* future = nullptr;
                    result                      = rocprofvis_controller_get_object(
                        m_graph_futures, kRPVControllerArrayEntryIndexed, i, &future);
                    assert(result == kRocProfVisResultSuccess && future);

                    rocprofvis_controller_future_free(
                        (rocprofvis_controller_future_t*) future);

                    if(result != kRocProfVisResultSuccess)
                    {
                        rocprofvis_handle_t* array = nullptr;
                        result                     = rocprofvis_controller_get_object(
                            m_graph_data_array, kRPVControllerArrayEntryIndexed, i,
                            &future);
                        assert(result == kRocProfVisResultSuccess && array);

                        rocprofvis_controller_array_free(
                            (rocprofvis_controller_array_t*) array);
                    }
                }

                rocprofvis_controller_array_free(m_graph_futures);
                m_graph_futures = nullptr;

                if(result != kRocProfVisResultSuccess)
                {
                    rocprofvis_controller_array_free(m_graph_data_array);
                    m_graph_data_array = nullptr;
                }
            }

            if(result == kRocProfVisResultSuccess)
            {
                m_is_loading_trace = false;
                m_is_trace_loaded  = true;
                m_data_changed     = true;
                ImGui::CloseCurrentPopup();
            }
        }

        if(!m_is_trace_loaded)
        {
            if(ImGui::BeginPopupModal("Loading"))
            {
                ImGui::Text("Please wait...");
                ImGui::EndPopup();
            }

            if(m_is_loading_trace)
            {
                ImGui::SetNextWindowSize(ImVec2(300, 200));
                ImGui::OpenPopup("Loading");
            }
        }
    }

    RenderDebugOuput();
}


void AppWindow::RenderDebugOuput() {
    if(m_show_debug_widow) {
        DebugWindow::GetInstance()->Render();
    }

    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsKeyPressed(ImGuiKey_D)) {
        m_show_debug_widow = !m_show_debug_widow;
        
        if(m_show_debug_widow) {
            ImGui::SetWindowFocus("Debug Window");
        }
    }
    
}
