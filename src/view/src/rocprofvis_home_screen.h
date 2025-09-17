#pragma once
#include "imgui.h"
#include "rocprofvis_structs.h"
#include <map>
#include "rocprofvis_main_view.h"
#include "rocprofvis_sidebar.h"
#include "widgets/rocprofvis_widget.h"

#include <vector>

namespace RocProfVis
{
namespace View
{

class HomeScreen : public RocWidget
{
public:
    void Render();//std::map<std::string, rocprofvis_trace_process_t>& trace_data);
    HomeScreen();
    ~HomeScreen();
    void SetData(std::map<std::string, rocprofvis_trace_process_t>& trace_data); 

private:
    std::shared_ptr<RocProfVis::View::MainView> m_main_view;
    std::shared_ptr<SideBar> m_sidebar;
    std::shared_ptr<HSplitContainer> m_container;
    std::map<std::string, rocprofvis_trace_process_t>* m_trace_data_ptr;
};

}  // namespace View
}  // namespace RocProfVis