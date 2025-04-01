#include "rocprofvis_appwindow.h"

#include "ImGuiFileDialog.h"
#include "imgui.h"
#include "implot.h"

#include "rocprofvis_controller.h"
#include <fstream>
#include <future>
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
, m_is_trace_loaded(false)
{}

AppWindow::~AppWindow() {}

bool
AppWindow::Init()
{
    ImPlot::CreateContext();

    // trace_object.m_min_ts          = DBL_MAX;
    // trace_object.m_max_ts          = 0.0;
    // trace_object.m_is_trace_loaded = false;

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

void AppWindow::handleOpenFile(std::string &file_path) {
    trace_controller = rocprofvis_controller_alloc();
    if (trace_controller)
    {
        rocprofvis_result_t result = kRocProfVisResultUnknownError;
        trace_future = rocprofvis_controller_future_alloc();
        if(trace_future)
        {
            result = rocprofvis_controller_load_async(trace_controller, file_path.c_str(), trace_future);
            assert(result == kRocProfVisResultSuccess);

            if(result != kRocProfVisResultSuccess)
            {
                rocprofvis_controller_future_free(trace_future);
                trace_future = nullptr;
            }
        }
        if(result != kRocProfVisResultSuccess)
        {
            rocprofvis_controller_free(trace_controller);
            trace_controller = nullptr;
        }
    }
}

void
AppWindow::Render()
{
    // std::map<std::string, rocprofvis_trace_process_t>& trace_data =
    //     trace_object.m_trace_data;

    if(home_screen && data_changed)
    {
        //home_screen->SetData(trace_data);
        home_screen->SetData(trace_timeline, graph_data_array);
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

    if(m_is_trace_loaded)
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
        m_is_trace_loaded = false;
        std::cout << "This happened" << std::endl;

        if(ImGuiFileDialog::Instance()->IsOk())
        {
            std::string file_path = ImGuiFileDialog::Instance()->GetFilePathName();
            
            handleOpenFile(file_path);
            is_loading_trace = true;
        }

        ImGuiFileDialog::Instance()->Close();
    }

    //load data
    if(trace_future || graph_futures)
    {
        if(trace_future)
        {
            rocprofvis_result_t result =
                rocprofvis_controller_future_wait(trace_future, 0);
            assert(result == kRocProfVisResultSuccess ||
                   result == kRocProfVisResultTimeout);
            if(result == kRocProfVisResultSuccess)
            {
                uint64_t uint64_result = 0;
                result                 = rocprofvis_controller_get_uint64(
                    trace_future, kRPVControllerFutureResult, 0, &uint64_result);
                assert(result == kRocProfVisResultSuccess &&
                       uint64_result == kRocProfVisResultSuccess);

                result = rocprofvis_controller_get_object(
                    trace_controller, kRPVControllerTimeline, 0, &trace_timeline);

                if(result == kRocProfVisResultSuccess && trace_timeline)
                {
                    uint64_t num_graphs = 0;
                    result              = rocprofvis_controller_get_uint64(
                        trace_timeline, kRPVControllerTimelineNumGraphs, 0, &num_graphs);

                    double min_ts = 0;
                    result        = rocprofvis_controller_get_double(
                        trace_timeline, kRPVControllerTimelineMinTimestamp, 0, &min_ts);

                    double max_ts = 0;
                    result        = rocprofvis_controller_get_double(
                        trace_timeline, kRPVControllerTimelineMaxTimestamp, 0, &max_ts);

                    graph_data_array = rocprofvis_controller_array_alloc(num_graphs);
                    graph_futures    = rocprofvis_controller_array_alloc(num_graphs);

                    for(uint32_t i = 0;
                        i < num_graphs && result == kRocProfVisResultSuccess; i++)
                    {
                        rocprofvis_controller_future_t* graph_future =
                            rocprofvis_controller_future_alloc();
                        rocprofvis_controller_array_t* graph_array =
                            rocprofvis_controller_array_alloc(32);
                        rocprofvis_handle_t* graph = nullptr;
                        result                     = rocprofvis_controller_get_object(
                            trace_timeline, kRPVControllerTimelineGraphIndexed, i,
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
                                    trace_controller, graph, min_ts, max_ts, 1000,
                                    graph_future, graph_array);
                                if(result == kRocProfVisResultSuccess)
                                {
                                    result = rocprofvis_controller_set_object(
                                        graph_data_array, kRPVControllerArrayEntryIndexed,
                                        i, graph_array);
                                    assert(result == kRocProfVisResultSuccess);

                                    result = rocprofvis_controller_set_object(
                                        graph_futures, kRPVControllerArrayEntryIndexed, i,
                                        graph_future);
                                    assert(result == kRocProfVisResultSuccess);
                                }
                            }
                        }
                    }
                }

                rocprofvis_controller_future_free(trace_future);
                trace_future = nullptr;
            }
        }
        else if(graph_futures)
        {
            uint64_t            num_tracks = 0;
            rocprofvis_result_t result     = rocprofvis_controller_get_uint64(
                graph_futures, kRPVControllerArrayNumEntries, 0, &num_tracks);
            assert(result == kRocProfVisResultSuccess);

            for(uint32_t i = 0; i < num_tracks && result == kRocProfVisResultSuccess; i++)
            {
                rocprofvis_handle_t* future = nullptr;
                result                      = rocprofvis_controller_get_object(
                    graph_futures, kRPVControllerArrayEntryIndexed, i, &future);
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
                        graph_futures, kRPVControllerArrayEntryIndexed, i, &future);
                    assert(result == kRocProfVisResultSuccess && future);

                    rocprofvis_controller_future_free(
                        (rocprofvis_controller_future_t*) future);

                    if(result != kRocProfVisResultSuccess)
                    {
                        rocprofvis_handle_t* array = nullptr;
                        result                     = rocprofvis_controller_get_object(
                            graph_data_array, kRPVControllerArrayEntryIndexed, i,
                            &future);
                        assert(result == kRocProfVisResultSuccess && array);

                        rocprofvis_controller_array_free(
                            (rocprofvis_controller_array_t*) array);
                    }
                }

                rocprofvis_controller_array_free(graph_futures);
                graph_futures = nullptr;

                if(result != kRocProfVisResultSuccess)
                {
                    rocprofvis_controller_array_free(graph_data_array);
                    graph_data_array = nullptr;
                }
            }

            if(result == kRocProfVisResultSuccess)
            {
                is_loading_trace = false;
                m_is_trace_loaded = true;
                data_changed = true;
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

            if(is_loading_trace)
            {
                ImGui::SetNextWindowSize(ImVec2(300, 200));
                ImGui::OpenPopup("Loading");
                //is_open = true;
            }
        }        
    }

}
