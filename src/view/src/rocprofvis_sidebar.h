#pragma once
#include "imgui.h"
#include "rocprofvis_main_view.h"
#include "rocprofvis_structs.h"
#include "widgets/rocprofvis_widget.h"

#include <map>
#include <vector>

namespace RocProfVis
{
namespace View
{

class SideBar : public RocWidget
{
public:
    SideBar();
    ~SideBar();
    virtual void Render();

    void SetGraphMap(std::map<int, rocprofvis_graph_map_t>* graph_map);
    void ConstructTree(std::map<int, rocprofvis_graph_map_t>* tree);

private:
    int                                    m_dropdown_select;
    std::map<int, rocprofvis_graph_map_t>* m_graph_map;
};

}  // namespace View
}  // namespace RocProfVis