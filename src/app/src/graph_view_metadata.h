#pragma once

#ifndef GRAPH_VIEW_METADATA_H
#    define GRAPH_VIEW_METADATA_H
#    include "structs.h"
#    include <string>
class GraphViewMetadata
{
public:
    float                             size;
    std::string                       type;
    meta_map_struct data;
    int                               graph_id;
    // Constructor
    GraphViewMetadata(int graph_id, float size, std::string type,
                      meta_map_struct data);

    // Destructor
    ~GraphViewMetadata();

    void renderData();
};

#endif  // GRAPH_VIEW_METADATA_H