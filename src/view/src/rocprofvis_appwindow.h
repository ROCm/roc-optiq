#pragma once

#include "widgets/rocprofvis_widget.h"
//#include "rocprofvis_trace.h"
#include "rocprofvis_home_screen.h"
#include "rocprofvis_controller.h"

namespace RocProfVis
{
namespace View
{

class AppWindow : public RocWidget
{
public:
    static AppWindow* getInstance();

    ~AppWindow();

    bool Init();
    virtual void Render();

private:
    AppWindow();

    void handleOpenFile(std::string &file_path);

    static AppWindow* m_instance;

    //rocprofvis_trace_data_t trace_object;
    std::shared_ptr<HomeScreen> home_screen; 
    std::shared_ptr<RocWidget> main_view;
    bool is_loading_trace;
    bool data_changed;
    bool m_is_trace_loaded;

    rocprofvis_controller_future_t* trace_future = nullptr;
    rocprofvis_controller_t* trace_controller = nullptr;
    rocprofvis_controller_timeline_t* trace_timeline = nullptr;
    rocprofvis_controller_array_t* graph_data_array = nullptr;
    rocprofvis_controller_array_t* graph_futures = nullptr;    
};

}  // namespace View
}  // namespace RocProfVis