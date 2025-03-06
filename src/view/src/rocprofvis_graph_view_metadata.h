// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_structs.h"
#include <string>

namespace RocProfVis
{
namespace View
{

class GraphViewMetadata
{
public:
    GraphViewMetadata(int graph_id, float size, std::string type,
                      rocprofvis_meta_map_struct_t data);

    ~GraphViewMetadata();

    void renderData();

private:
    rocprofvis_meta_map_struct_t m_data;
    std::string                  m_type;

    float m_size;
    int   m_graph_id;
};

}  // namespace View
}  // namespace RocProfVis
