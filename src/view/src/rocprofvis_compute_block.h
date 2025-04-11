// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "imgui.h"
#include <list>
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

typedef enum block_diagram_level_t
{
    kGPULevel = 0,
    kShaderEngineLevel = 1,
    kComputeUnitLevel = 2,
    kCacheLevel = 3
} block_diagram_level_t;

class ComputeBlockDiagramNavHelper
{
public:
    ComputeBlockDiagramNavHelper();

    block_diagram_level_t Current();
    bool BackAvailable();
    bool ForwardAvailable();

    void GoBack();
    void GoForward();
    void Go(block_diagram_level_t level);

private:
    block_diagram_level_t m_current_level;
    std::list<block_diagram_level_t> m_previous_levels;
    std::list<block_diagram_level_t> m_next_levels;
};

class ComputeBlockDiagram : public RocWidget
{
    friend class ComputeBlockView;

public:
    void Render();
    ComputeBlockDiagram();
    ~ComputeBlockDiagram();

private:
    void RenderGPULevel();
    void RenderShaderEngineLevel();
    void RenderComputeUnitLevel();
    void RenderCacheLevel();
    bool BlockButton(const char* name, const char* tooltip, bool navigation = false, ImVec2 rel_pos = ImVec2(0, 0), ImVec2 rel_size = ImVec2(1, 1));
    void Link(ImVec2 rel_left, ImVec2 rel_right, ImGuiDir direction = ImGuiDir_None);

    ImVec2 m_content_region;
    ImVec2 m_content_region_center;
    ImDrawList* m_draw_list;
    std::unique_ptr<ComputeBlockDiagramNavHelper> m_navigation;
};
class ComputeBlockView : public RocWidget
{
public:
    void Render();
    ComputeBlockView();
    ~ComputeBlockView();

private:
    void RenderMenuBar();

    std::shared_ptr<ComputeBlockDiagram> m_block_diagram;
    std::shared_ptr<HSplitContainer> m_container;
};

}  // namespace View
}  // namespace RocProfVis
