#pragma once
#include "imgui.h"
#include "rocprofvis_main_view.h"
#include "rocprofvis_sidebar.h"
#include "widgets/rocprofvis_widget.h"

#include <map>
#include <vector>

namespace RocProfVis
{
namespace View
{

class HomeScreen : public RocWidget
{
public:
    void Render();
    HomeScreen();
    ~HomeScreen();
    void SetData(rocprofvis_controller_timeline_t* trace_timeline, rocprofvis_controller_array_t* graph_data_array); 

private:
    std::shared_ptr<RocProfVis::View::MainView> m_main_view;
    std::shared_ptr<SideBar> m_sidebar;
    std::shared_ptr<HSplitContainer> m_container;

    rocprofvis_controller_timeline_t* m_trace_timeline_ptr;
    rocprofvis_controller_array_t* m_graph_data_array_ptr;    
};

}  // namespace View
}  // namespace RocProfVis