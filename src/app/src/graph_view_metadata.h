#pragma once

#ifndef GRAPH_VIEW_METADATA_H
#    define GRAPH_VIEW_METADATA_H
#    include <string>
#include "structs.h"
class GraphViewMetadata
{
public:
    float size; 
    std::string type; 
    rocprofvis_metadata_visualization data;
    int                               graphID;
    // Constructor
    GraphViewMetadata(int graphID, float size, std::string type, rocprofvis_metadata_visualization data);

    // Destructor
    ~GraphViewMetadata();

    void renderData(); 

 
};

#endif  // GRAPH_VIEW_METADATA_H