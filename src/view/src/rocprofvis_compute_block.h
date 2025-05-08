// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "imgui.h"
#include <list>
#include "widgets/rocprofvis_widget.h"
#include "rocprofvis_compute_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_compute_metric.h"


namespace RocProfVis
{
namespace View
{

typedef enum block_diagram_level_t
{
    kBlockDiagramLevelGPU = 1 << 0,
    kBlockDiagramlevelShaderEngine = 1 << 1,
    kBlockDiagramLevelComputeUnit = 1 << 2,
    kBlockDiagramLevelCache = 1 << 3
} block_diagram_level_t;

typedef int block_diagram_level_flags_t;

typedef enum block_diagram_block_option_t
{
    kBLockDiagramBlockOption_None = 1 << 0,
    kBlockDiagramBlockOption_Navigation = 1 << 1,
    kBlockDiagramBlockOption_LabelMetrics = 1 << 2
} block_diagram_block_option_t;

typedef int block_diagram_block_option_flags;

typedef enum block_diagram_link_id_t
{
    kBlockDiagramLinkCPC_L2 = 0,
    kBlockDiagramLinkCPF_L2,
    kBlockDiagramLinkvL1D_L2_Read,
    kBlockDiagramLinkvL1D_L2_Write,
    kBlockDiagramLinkvL1D_L2_Atomic,
    kBlockDiagramLinksL1D_L2_Read,
    kBlockDiagramLinksL1D_L2_Write,
    kBlockDiagramLinksL1D_L2_Atomic,
    kBlockDiagramLinkL1I_L2,
    kBlockDiagramLinkL2_Fabric_Read,
    kBlockDiagramLinkL2_Fabric_Write,
    kBlockDiagramLinkL2_Fabric_Atomic,
    kBlockDiagramLinkFabric_PCIe_Read,
    kBlockDiagramLinkFabric_PCIe_Write,
    kBlockDiagramLinkFabric_HBM_Read,
    kBlockDiagramLinkFabric_HBM_Write,
    kBlockDiagramLinkFabric_DRAM_Read,
    kBlockDiagramLinkFabric_DRAM_Write,
    kBlockDiagramLinkCU_vL1D_Read,
    kBlockDiagramLinkCU_vL1D_Write,
    kBlockDiagramLinkCU_vL1D_Atomic,
    kBlockDiagramLinkCU_sL1D,
    kBlockDiagramLinkCU_L1I,
    kBlockDiagramLinkCU_LDS,
    kBlockDiagramLinkCount
} block_diagram_link_id_t;

typedef enum block_diagram_block_id_t
{
    kBlockDiagramBlockNone = -1,
    kBlockDiagramBlockGPU = 0,
    kBlockDiagramBlockCP,
    kBlockDiagramBlockCPC,
    kBlockDiagramBlockCPF,
    kBlockDiagramBlockSE,
    kBlockDiagramBlockL2,
    kBlockDiagramBlockFabric,
    kBlockDiagramBlockCU,
    kBlockDiagramBlockSPI,
    kBlockDiagramBlocksL1D,
    kBlockDiagramBlockvL1D,
    kBlockDiagramBlockL1I,
    kBlockDiagramBlockScheduler,
    kBlockDiagramBlockVALU,
    kBlockDiagramBlockSALU,
    kBlockDiagramBlockMFMA,
    kBlockDiagramBlockBranch,
    kBlockDiagramBlockLDS,
    kBlockDiagramBlockHBM,
    kBlockDiagramBlockPCIe,
    kBlockDiagramBlockDRAM,
    kBlockDiagramBlockCount
} block_diagram_block_id_t;

typedef struct block_diagram_navigation_location_t
{
    block_diagram_level_t m_level;
    block_diagram_block_id_t m_block;
} block_diagram_navigation_location_t;

typedef struct block_diagram_metric_info_t
{
    std::string m_metric_category;
    std::string m_label;
    std::string m_metric_id;
} block_diagram_metric_info_t;

typedef struct block_diagram_link_info_t
{
    block_diagram_link_id_t m_id;
    block_diagram_metric_info_t m_metric_info;
} block_diagram_link_info_t;

typedef struct block_diagram_plot_info_t
{
    std::string m_metric_category;
    std::vector<int> m_plot_indexes;
} block_diagram_plot_info_t;

typedef struct block_diagram_block_info_t
{
    block_diagram_block_id_t m_id;
    block_diagram_level_flags_t m_levels;
    std::string m_short_name;
    std::string m_full_name;
    std::string m_tooltip;
    std::vector<block_diagram_plot_info_t> m_plot_infos;
    std::vector<block_diagram_metric_info_t> m_metric_infos;
} block_diagram_block_info_t;

class ComputeBlockDetails : public RocWidget
{
public:
    void Render();
    void Update();
    ComputeBlockDetails(std::shared_ptr<ComputeDataProvider> data_provider);
    ~ComputeBlockDetails();

private:
    void OnNavigationChanged(std::shared_ptr<RocEvent> event);

    std::unordered_map<block_diagram_block_id_t, std::vector<std::unique_ptr<ComputeMetricGroup>>> m_metrics_map;
    EventManager::EventHandler m_block_navigation_event_handler;
    block_diagram_navigation_location_t m_current_location;
    bool m_navigation_changed;
};

class ComputeBlockDiagramNavHelper
{
public:
    ComputeBlockDiagramNavHelper();

    block_diagram_navigation_location_t Current();
    bool BackAvailable();
    bool ForwardAvailable();

    void GoBack();
    void GoForward();
    void Go(block_diagram_level_t level);
    void Select(block_diagram_block_id_t block);

private:
    block_diagram_navigation_location_t m_current_location;
    std::list<block_diagram_level_t> m_previous_levels;
    std::list<block_diagram_level_t> m_next_levels;
};

class ComputeBlockDiagram : public RocWidget
{
public:
    void Render();
    void Update();
    ComputeBlockDiagram(std::shared_ptr<ComputeDataProvider> data_provider);
    ~ComputeBlockDiagram();

    ComputeBlockDiagramNavHelper* Navigation();

private:
    void RenderGPULevel();
    void RenderShaderEngineLevel();
    void RenderComputeUnitLevel();
    void RenderCacheLevel();
    bool BlockButton(block_diagram_block_id_t id, ImVec2 rel_pos, ImVec2 rel_size, block_diagram_block_option_flags options = kBlockDiagramBlockOption_LabelMetrics);
    void Link(block_diagram_link_id_t id, ImVec2 rel_left, ImVec2 rel_right, ImGuiDir direction = ImGuiDir_None);

    ImVec2 m_content_region;
    ImVec2 m_content_region_center;
    ImDrawList* m_draw_list;
    std::unique_ptr<ComputeBlockDiagramNavHelper> m_navigation;

    std::array<std::unique_ptr<ComputeMetric>, kBlockDiagramLinkCount> m_link_metrics;
    std::array<std::vector<std::unique_ptr<ComputeMetric>>, kBlockDiagramBlockCount> m_block_metrics;
};

class ComputeBlockView : public RocWidget
{
public:
    void Render();
    void Update();
    ComputeBlockView(std::shared_ptr<ComputeDataProvider> data_provider);
    ~ComputeBlockView();

private:
    void RenderMenuBar();

    std::shared_ptr<ComputeBlockDiagram> m_block_diagram;
    std::shared_ptr<ComputeBlockDetails> m_details_panel;
    std::shared_ptr<HSplitContainer> m_container;
};

}  // namespace View
}  // namespace RocProfVis
