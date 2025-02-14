#include "graph_view_metadata.h"
#include <string>
#include "structs.h"
// Constructor
GraphViewMetadata::GraphViewMetadata(int graphID, float size, std::string type,
                                     rocprofvis_metadata_visualization data)
{
    this->size = size;
    this->type = type;
    this->data = data;
    this->graphID = graphID;
}

// Destructor
GraphViewMetadata::~GraphViewMetadata() {}


void GraphViewMetadata::renderData() {


        ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove;

    if(ImGui::BeginChild((std::to_string(graphID)).c_str()), true, window_flags)
    {
 
         ImGui::TextColored(ImVec4(0.0f, 0.0f, 0.0f, 3.0f), data.chart_name.c_str());
 
     }

    ImGui::EndChild();
}