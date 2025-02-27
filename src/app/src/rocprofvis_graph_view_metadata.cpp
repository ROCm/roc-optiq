#include "rocprofvis_graph_view_metadata.h"
#include "structs.h"
#include <string>
// Constructor
GraphViewMetadata::GraphViewMetadata(int graph_id, float size, std::string type,
                                     meta_map_struct data)
{
    this->size     = size;
    this->type     = type;
    this->data     = data;
    this->graph_id = graph_id;
}

// Destructor
GraphViewMetadata::~GraphViewMetadata() {}

void
GraphViewMetadata::renderData()
{
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove;

    if(ImGui::BeginChild((std::to_string(graph_id)).c_str()), true, window_flags)
    {
        ImGui::TextColored(ImVec4(0.0f, 0.0f, 0.0f, 3.0f), data.chart_name.c_str());
    }

    ImGui::EndChild();
}