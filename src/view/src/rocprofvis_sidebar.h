#pragma once
#include "imgui.h"
#include "rocprofvis_main_view.h"
#include "rocprofvis_structs.h"
#include <map>

#include <vector>
class SideBar
{
public:
    void Render();
    SideBar();
    ~SideBar();
    void ConstructTree(std::map<int, rocprofvis_graph_map_t>* tree); 

private:
  int m_dropdown_select; 
};