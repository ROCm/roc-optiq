// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_graph_view_metadata.h"
#include "structs.h"
#include <string>

GraphViewMetadata::GraphViewMetadata(int graph_id, float size, std::string type,
                                     meta_map_struct data)
: m_size(size)
, m_type(type)
, m_data(data)
, m_graph_id(graph_id)
{}

GraphViewMetadata::~GraphViewMetadata() {}

void
GraphViewMetadata::renderData()
{
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove;

    if(ImGui::BeginChild((std::to_string(m_graph_id)).c_str()), true, window_flags)
    {
        ImGui::TextColored(ImVec4(0.0f, 0.0f, 0.0f, 3.0f), m_data.chart_name.c_str());
    }

    ImGui::EndChild();
}
