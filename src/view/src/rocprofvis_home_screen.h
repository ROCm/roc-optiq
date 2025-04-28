#pragma once
#include "imgui.h"
#include "rocprofvis_analysis_view.h"
#include "rocprofvis_controller_types.h"
#include "rocprofvis_data_provider.h"
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
    HomeScreen();
    ~HomeScreen();

    void Update();
    void Render();

    bool OpenFile(const std::string& file_path);

    void CreateView();
    void DestroyView();

private:
    std::shared_ptr<MainView>        m_main_view;
    std::shared_ptr<SideBar>         m_sidebar;
    std::shared_ptr<HSplitContainer> m_container;
    std::shared_ptr<AnalysisView>        m_analysis;

    DataProvider m_data_provider;
    bool         m_view_created;
    bool         m_open_loading_popup;
};

}  // namespace View
}  // namespace RocProfVis
