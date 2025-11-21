// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include <list>
#include "widgets/rocprofvis_widget.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_controller_enums.h"


namespace RocProfVis
{
namespace View
{

class ComputeDataProvider;
class ComputePlot;
class ComputeMetric;

typedef enum LinkID
{
    LinkIDCPC_L2 = 0,
    LinkIDCPF_L2,
    LinkIDvL1D_L2_Read,
    LinkIDvL1D_L2_Write,
    LinkIDvL1D_L2_Atomic,
    LinkIDsL1D_L2_Read,
    LinkIDsL1D_L2_Write,
    LinkIDsL1D_L2_Atomic,
    LinkIDL1I_L2,
    LinkIDL2_Fabric_Read,
    LinkIDL2_Fabric_Write,
    LinkIDL2_Fabric_Atomic,
    LinkIDFabric_PCIe_Read,
    LinkIDFabric_PCIe_Write,
    LinkIDFabric_HBM_Read,
    LinkIDFabric_HBM_Write,
    LinkIDFabric_DRAM_Read,
    LinkIDFabric_DRAM_Write,
    LinkIDCU_vL1D_Read,
    LinkIDCU_vL1D_Write,
    LinkIDCU_vL1D_Atomic,
    LinkIDCU_sL1D,
    LinkIDCU_L1I,
    LinkIDCU_LDS,
    LinkIDCount
} LinkID;

typedef enum BlockLevel
{
    BlockLevelGPU = 1 << 0,
    BlockLevelShaderEngine = 1 << 1,
    BlockLevelComputeUnit = 1 << 2,
    BlockLevelCache = 1 << 3
} BlockLevel;
typedef int BlockLevelFlags;

typedef enum BlockID
{
    BlockIDNone = -1,
    BlockIDGPU = 0,
    BlockIDCP,
    BlockIDCPC,
    BlockIDCPF,
    BlockIDSE,
    BlockIDL2,
    BlockIDFabric,
    BlockIDCU,
    BlockIDSPI,
    BlockIDsL1D,
    BlockIDvL1D,
    BlockIDL1I,
    BlockIDScheduler,
    BlockIDVALU,
    BlockIDSALU,
    BlockIDMFMA,
    BlockIDBranch,
    BlockIDLDS,
    BlockIDHBM,
    BlockIDPCIe,
    BlockIDDRAM,
    BlockIDCount
} BlockID;

typedef enum BlockOption
{
    BlockOption_None = 1 << 0,
    BlockOption_Navigation = 1 << 1,
    BlockOption_LabelMetrics = 1 << 2
} BlockOption;
typedef int BlockOptionFlags;

typedef enum PlotType
{
    PlotTypePie,
    PlotTypeBar,
} PlotType;

typedef struct PlotDefinition
{
    rocprofvis_controller_compute_plot_types_t m_type;
    PlotType m_plot_type;
} PlotDefinition;

typedef struct LabelDefinition
{
    std::string m_name;
    std::string m_unit;
    rocprofvis_controller_compute_metric_types_t m_metric;
} LabelDefinition;

typedef struct BlockDefinition
{
    BlockID m_id;
    BlockLevelFlags m_levels;
    std::string m_short_name;
    std::string m_full_name;
    std::string m_tooltip;
    std::string m_table_url;
    std::vector<PlotDefinition> m_plots;
    std::vector<LabelDefinition> m_labels;
} BlockDefinition;

typedef struct LinkDefinition
{
    LinkID m_id;
    LabelDefinition m_label;
} LinkDefinition;

typedef struct BlockNavigationLocation
{
    BlockLevel m_level;
    BlockID m_block;
} BlockNavigationLocation;

class ComputeBlockDiagramNavHelper
{
public:
    ComputeBlockDiagramNavHelper();

    BlockNavigationLocation Current();
    bool BackAvailable();
    bool ForwardAvailable();

    void GoBack();
    void GoForward();
    void Go(BlockLevel level);
    void Select(BlockID block);

private:
    void NotifyNavigationChanged();

    BlockNavigationLocation m_current_location;
    std::list<BlockLevel> m_previous_levels;
    std::list<BlockLevel> m_next_levels;
};

class ComputeBlockView : public RocWidget
{
public:
    void Render();
    void Update();
    ComputeBlockView(std::string owner_id, std::shared_ptr<ComputeDataProvider> data_provider);
    ~ComputeBlockView();

private:
    void RenderMenuBar();
    void RenderBlockDiagram();
    void RenderBlockDetails();

    void RenderBlockDiagramGPU();
    void RenderBlockDiagramShaderEngine();
    void RenderBlockDiagramComputeUnit();
    void RenderBlockDiagramCache();

    bool BlockButton(BlockID id, ImVec2 rel_pos, ImVec2 rel_size, BlockOptionFlags options = BlockOption_None);
    void Link(LinkID id, ImVec2 rel_left, ImVec2 rel_right, ImGuiDir direction = ImGuiDir_None);

    std::unique_ptr<ComputeBlockDiagramNavHelper> m_navigation;
    EventManager::SubscriptionToken m_navigation_event_token;
    bool m_navigation_changed;

    std::shared_ptr<RocCustomWidget> m_left_column;
    std::shared_ptr<RocCustomWidget> m_right_column;
    std::shared_ptr<HSplitContainer> m_container;
    std::string m_owner_id;

    std::unordered_map<BlockID, std::vector<std::unique_ptr<ComputeMetric>>> m_block_metrics;
    std::unordered_map<LinkID, std::unique_ptr<ComputeMetric>> m_link_metrics;
    std::unordered_map<BlockID, std::vector<std::unique_ptr<ComputePlot>>> m_plots;

    ImDrawList* m_draw_list;
    ImVec2 m_block_diagram_region;
    ImVec2 m_block_diagram_center;
};

}  // namespace View
}  // namespace RocProfVis
