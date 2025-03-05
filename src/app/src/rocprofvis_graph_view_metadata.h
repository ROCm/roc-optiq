// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "structs.h"
#include <string>

class GraphViewMetadata
{
public:
    GraphViewMetadata(int graph_id, float size, std::string type, meta_map_struct data);

    ~GraphViewMetadata();

    void renderData();

private:
    float           m_size;
    int             m_graph_id;
    std::string     m_type;
    meta_map_struct m_data;
  
};
