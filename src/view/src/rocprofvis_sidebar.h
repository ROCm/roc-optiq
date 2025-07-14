// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "imgui.h"
#include "rocprofvis_timeline_view.h"
// #include "rocprofvis_structs.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_settings.h"
#include "rocprofvis_view_structs.h"
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
    SideBar(DataProvider& dp);
    ~SideBar();
    virtual void Render();

    void SetGraphs(std::vector<rocprofvis_graph_t>* graphs);
    void ConstructTree();

private:
    int                              m_dropdown_select;
    std::vector<rocprofvis_graph_t>* m_graphs;

    Settings&     m_settings;
    DataProvider& m_data_provider;
};

}  // namespace View
}  // namespace RocProfVis
