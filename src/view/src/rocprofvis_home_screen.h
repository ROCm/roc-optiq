#pragma once
#include "imgui.h"
#include "rocprofvis_controller_types.h"
#include "rocprofvis_main_view.h"
#include "rocprofvis_sidebar.h"
#include "widgets/rocprofvis_widget.h"
#include "rocprofvis_data_provider.h"

#include <map>
#include <vector>

namespace RocProfVis
{
namespace View
{

class HomeScreen : public RocWidget
{
public:
    HomeScreen();
    ~HomeScreen();

    void Update();
    void Render();

    bool OpenFile(const std::string &file_path);

    void CreateView();
    void DestroyView();

    //void SetData(rocprofvis_controller_timeline_t* trace_timeline, rocprofvis_controller_array_t* graph_data_array); 

private:
    std::shared_ptr<MainView> m_main_view;
    std::shared_ptr<SideBar> m_sidebar;
    std::shared_ptr<HSplitContainer> m_container;

    DataProvider m_data_provider;
    bool m_view_created;
};

}  // namespace View
}  // namespace RocProfVis
