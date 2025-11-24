// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_block.h"
#include "rocprofvis_navigation_manager.h"
#include "rocprofvis_navigation_url.h"
#include "widgets/rocprofvis_compute_widget.h"
#include <array>

namespace RocProfVis
{
namespace View
{

const ImVec4 BLOCK_HIGHLIGHT_COLOR = ImVec4(0.9f, 0.4f, 0.4f, 0.5f);

const std::array BLOCK_DEFINITIONS {
    BlockDefinition{BlockIDGPU, BlockLevelGPU | BlockLevelCache, "GPU", "GPU", "", "", {}, {}},
    BlockDefinition{BlockIDCP, BlockLevelGPU | BlockLevelCache, "CP", "Command Processor", "", COMPUTE_TABLE_COMMAND_PROCESSOR_URL, {}, {}},
    BlockDefinition{BlockIDCPC, BlockLevelGPU | BlockLevelCache, "CPC", "Command Processor Packet Processor", "", COMPUTE_TABLE_COMMAND_PROCESSOR_URL, {}, {}},
    BlockDefinition{BlockIDCPF, BlockLevelGPU | BlockLevelCache, "CPF", "Command Processor Fetcher", "", COMPUTE_TABLE_COMMAND_PROCESSOR_URL, {}, {}},
    BlockDefinition{BlockIDSE, BlockLevelGPU | BlockLevelShaderEngine | BlockLevelCache, "SE", "Shader Engine", "", "", {}, {}},
    BlockDefinition{BlockIDL2, BlockLevelGPU | BlockLevelCache, "L2", "L2 Cache", "", COMPUTE_TABLE_L2_CACHE_URL, {
        PlotDefinition{kRPVControllerComputePlotTypeL2CacheSpeedOfLight, PlotTypeBar},
        PlotDefinition{kRPVControllerComputePlotTypeL2CacheFabricSpeedOfLight, PlotTypeBar},
        PlotDefinition{kRPVControllerComputePlotTypeL2CacheFabricStallsRead, PlotTypeBar},
        PlotDefinition{kRPVControllerComputePlotTypeL2CacheFabricStallsWrite, PlotTypeBar}
    }, {
        LabelDefinition{"Rd", "", kRPVControllerComputeMetricTypeL2CacheRd},
        LabelDefinition{"Wr", "", kRPVControllerComputeMetricTypeL2CacheWr},
        LabelDefinition{"Atomic", "", kRPVControllerComputeMetricTypeL2CacheAtomic},
        LabelDefinition{"Hit", "%", kRPVControllerComputeMetricTypeL2CacheHitRate}
    }},
    BlockDefinition{BlockIDFabric, BlockLevelGPU | BlockLevelCache, "Fabric", "Infinity Fabric", "", "", {}, {
        LabelDefinition{"Rd", "cycles", kRPVControllerComputeMetricTypeFabricRd},
        LabelDefinition{"Wr", "cycles", kRPVControllerComputeMetricTypeFabricWr},
        LabelDefinition{"Atomic", "cycles", kRPVControllerComputeMetricTypeFabricAtomic}
    }},
    BlockDefinition{BlockIDCU, BlockLevelShaderEngine | BlockLevelComputeUnit | BlockLevelCache, "CU", "Compute Unit", "", COMPUTE_TABLE_COMPUTE_PIPELINE_URL, {
        PlotDefinition{kRPVControllerComputePlotTypeInstrMix, PlotTypeBar},
        PlotDefinition{kRPVControllerComputePlotTypeCUOps, PlotTypeBar}
    }, {
        LabelDefinition{"VGPRs", "", kRPVControllerComputeMetricTypeVGPR},
        LabelDefinition{"SGPRs", "", kRPVControllerComputeMetricTypeSGPR},
        LabelDefinition{"LDS Alloc", "", kRPVcontrollerComputeMetricTypeLDSAlloc},
        LabelDefinition{"Scratch Alloc", "", kRPVControllerComputeMetricTypeScratchAlloc},
        LabelDefinition{"Wavefronts", "", kRPVControllerComputeMetricTypeWavefronts},
        LabelDefinition{"Workgroups", "", kRPVControllerComputeMetricTypeWorkgroups}
    }},
    BlockDefinition{BlockIDSPI, BlockLevelShaderEngine, "SPI", "Workgroup Manager", "", COMPUTE_TABLE_WORKGROUP_MANAGER_URL, {}, {}},
    BlockDefinition{BlockIDsL1D, BlockLevelShaderEngine | BlockLevelCache, "sL1D", "Scalar L1 Data Cache", "", COMPUTE_TABLE_SCALAR_CACHE_URL, {
        PlotDefinition{kRPVControllerComputePlotTypeSL1CacheSpeedOfLight, PlotTypeBar}
    }, {
        LabelDefinition{"Hit", "%", kRPVControllerComputeMetricTypeSL1CacheHitRate},
    }},
    BlockDefinition{BlockIDvL1D, BlockLevelComputeUnit | BlockLevelCache, "vL1D", "Vector L1 Data Cache", "", COMPUTE_TABLE_VECTOR_CACHE_URL, {
        PlotDefinition{kRPVControllerComputePlotTypeVL1CacheSpeedOfLight, PlotTypeBar},
        PlotDefinition{kRPVControllerComputePlotTypeVL1CacheL2NCTransactions, PlotTypeBar},
        PlotDefinition{kRPVControllerComputePlotTypeVL1CacheL2UCTransactions, PlotTypeBar},
        PlotDefinition{kRPVControllerComputePlotTypeVL1CacheL2RWTransactions, PlotTypeBar},
        PlotDefinition{kRPVControllerComputePlotTypeVL1CacheL2CCTransactions, PlotTypeBar}
    }, {
        LabelDefinition{"Hit", "%", kRPVControllerComputeMetricTypeVL1CacheHitRate},
        LabelDefinition{"Coales", "%", kRPVControllerComputeMetricTypeVL1CacheCoales},
        LabelDefinition{"Stall", "%", kRPVControllerComputeMetricTypeVL1CacheStall},
    }},
    BlockDefinition{BlockIDL1I, BlockLevelShaderEngine | BlockLevelCache, "L1I", "L1 Instruction Cache", "", COMPUTE_TABLE_INSTRUCTION_CACHE_URL, {
        PlotDefinition{kRPVControllerComputePlotTypeInstrCacheSpeedOfLight, PlotTypeBar}
    }, {
        LabelDefinition{"Hit", "%", kRPVControllerComputeMetricTypeInstrCacheHitRate},
        LabelDefinition{"Lat", "cycles", kRPVControllerComputeMetricTypeInstrCacheLat},
    }},
    BlockDefinition{BlockIDScheduler, BlockLevelComputeUnit, "Scheduler", "Scheduler", "", COMPUTE_TABLE_WAVEFRONT_URL, {}, {}},
    BlockDefinition{BlockIDVALU, BlockLevelComputeUnit, "VALU", "Vector Arithmetic Logic Unit", "", "", {
        PlotDefinition{kRPVControllerComputePlotTypeVALUInstrMix, PlotTypeBar}
    }, {}},
    BlockDefinition{BlockIDSALU, BlockLevelComputeUnit, "SALU", "Scalar Arithmetic Logic Unit", "", "", {}, {}},
    BlockDefinition{BlockIDMFMA, BlockLevelComputeUnit, "MFMA", "Matrix Fused Multiply-Add", "", "", {}, {}},
    BlockDefinition{BlockIDBranch, BlockLevelComputeUnit, "Branch", "Branch", "", "", {}, {}},
    BlockDefinition{BlockIDLDS, BlockLevelComputeUnit, "LDS", "Local Data Share", "", COMPUTE_TABLE_LOCAL_DATA_STORE_URL, {
        PlotDefinition{kRPVControllerComputePlotTypeLDSSpeedOfLight, PlotTypeBar}
    }, {
        LabelDefinition{"Util", "%", kRPVControllerComputeMetricTypeLDSUtil},
        LabelDefinition{"Lat", "cycles", kRPVControllerComputeMetricTypeLDSLat},
    }},
    BlockDefinition{BlockIDHBM, BlockLevelCache, "HBM", "HBM", "", "", {}, {}},
    BlockDefinition{BlockIDPCIe, BlockLevelCache, "PCIe", "PCIe", "", "", {}, {}},
    BlockDefinition{BlockIDDRAM, BlockLevelCache, "CPU DRAM", "CPU DRAM", "", "", {}, {}}
};

const std::array LINK_DEFINITIONS {
    LinkDefinition{LinkIDvL1D_L2_Read, LabelDefinition{"Rd", "", kRPVControllerComputeMetricTypeVL1_L2Rd}},
    LinkDefinition{LinkIDvL1D_L2_Write, LabelDefinition{"Wr", "", kRPVControllerComputeMetricTypeVL1_L2Wr}},
    LinkDefinition{LinkIDvL1D_L2_Atomic, LabelDefinition{"Atomic", "", kRPVControllerComputeMetricTypeVL1_L2Atomic}},
    LinkDefinition{LinkIDsL1D_L2_Read, LabelDefinition{"Rd", "", kRPVControllerComputeMetricTypeSL1_L2Rd}},
    LinkDefinition{LinkIDsL1D_L2_Write, LabelDefinition{"Wr", "", kRPVControllerComputeMetricTypeSL1_L2Wr}},
    LinkDefinition{LinkIDsL1D_L2_Atomic, LabelDefinition{"Atomic", "", kRPVControllerComputeMetricTypeSL1_L2Atomic}},
    LinkDefinition{LinkIDL1I_L2, LabelDefinition{"Req", "", kRPVControllerComputeMetricTypeInst_L2Req}},
    LinkDefinition{LinkIDL2_Fabric_Read, LabelDefinition{"Rd", "", kRPVControllerComputeMetricTypeL2_FabricRd}},
    LinkDefinition{LinkIDL2_Fabric_Write, LabelDefinition{"Wr", "", kRPVControllerComputeMetricTypeL2_FabricWr}},
    LinkDefinition{LinkIDL2_Fabric_Atomic, LabelDefinition{"Atomic", "", kRPVControllerComputeMetricTypeL2_FabricAtomic}},
    LinkDefinition{LinkIDFabric_HBM_Read, LabelDefinition{"Rd", "", kRPVControllerComputeMetricTypeFabric_HBMRd}},
    LinkDefinition{LinkIDFabric_HBM_Write, LabelDefinition{"Wr", "", kRPVControllerComputeMetricTypeFabric_HBMWr}},
    LinkDefinition{LinkIDCU_vL1D_Read, LabelDefinition{"Rd", "", kRPVControllerComputeMetricTypeCU_VL1Rd}},
    LinkDefinition{LinkIDCU_vL1D_Write, LabelDefinition{"Wr", "", kRPVControllerComputeMetricTypeCU_VL1Wr}},
    LinkDefinition{LinkIDCU_vL1D_Atomic, LabelDefinition{"Atomic", "", kRPVControllerComputeMetricTypeCU_VL1Atomic}},
    LinkDefinition{LinkIDCU_sL1D, LabelDefinition{"Rd", "", kRPVControllerComputeMetricTypeCU_SL1Rd}},
    LinkDefinition{LinkIDCU_L1I, LabelDefinition{"Fetch", "", kRPVControllerComputeMetricTypeCU_InstrReq}},
    LinkDefinition{LinkIDCU_LDS, LabelDefinition{"Req", "", kRPVControllerComputeMetricTypeCU_LDSReq}}
};

constexpr int WINDOW_PADDING_DEFAULT = 8;
constexpr int LEVEL_HISTORY_LIMIT = 5;

ComputeBlockDiagramNavHelper::ComputeBlockDiagramNavHelper()
: m_current_location(BlockNavigationLocation{BlockLevelGPU, BlockIDNone})
{

}

BlockNavigationLocation ComputeBlockDiagramNavHelper::Current()
{
    return m_current_location;
}

bool ComputeBlockDiagramNavHelper::BackAvailable()
{
    return !m_previous_levels.empty();
}

bool ComputeBlockDiagramNavHelper::ForwardAvailable()
{
    return !m_next_levels.empty();
}

void ComputeBlockDiagramNavHelper::GoBack()
{
    if (!m_previous_levels.empty())
    {
        m_next_levels.push_back(m_current_location.m_level);
        m_current_location.m_level = m_previous_levels.back();
        m_previous_levels.pop_back();
        m_current_location.m_block = BlockIDNone;
        NotifyNavigationChanged();
    }
}

void ComputeBlockDiagramNavHelper::GoForward()
{
    if (!m_next_levels.empty())
    {
        m_previous_levels.push_back(m_current_location.m_level);
        m_current_location.m_level = m_next_levels.back();        
        m_next_levels.pop_back();
        m_current_location.m_block = BlockIDNone;
        NotifyNavigationChanged();
    }
}

void ComputeBlockDiagramNavHelper::Go(BlockLevel level)
{
    if (m_current_location.m_level != level)
    {
        m_previous_levels.push_back(m_current_location.m_level);
        m_current_location.m_level = level;
        m_next_levels = {};
        if (m_previous_levels.size() > LEVEL_HISTORY_LIMIT)
        {
            m_previous_levels.pop_front();
        }
        m_current_location.m_block = BlockIDNone;
        NotifyNavigationChanged();
    }
}

void ComputeBlockDiagramNavHelper::Select(BlockID block)
{
    if (m_current_location.m_block != block)
    {
        m_current_location.m_block = block;
    }
    else
    {
        m_current_location.m_block = BlockIDNone;
    }
    NotifyNavigationChanged();
}

void ComputeBlockDiagramNavHelper::NotifyNavigationChanged()
{
    std::shared_ptr<RocEvent> nav_event = std::make_shared<RocEvent>(static_cast<int>(RocEvents::kComputeBlockNavigationChanged));
    nav_event->SetAllowPropagate(false);
    EventManager::GetInstance()->AddEvent(nav_event);
}

ComputeBlockView::ComputeBlockView(std::string owner_id, std::shared_ptr<ComputeDataProvider> data_provider)
: m_left_column(nullptr)
, m_right_column(nullptr)
, m_container(nullptr)
, m_draw_list(nullptr)
, m_navigation(nullptr)
, m_block_diagram_region(ImVec2(-1, -1))
, m_block_diagram_center(ImVec2(-1, -1))
, m_navigation_changed(true)
, m_navigation_event_token(-1)
, m_owner_id(owner_id)
{
    m_navigation = std::make_unique<ComputeBlockDiagramNavHelper>();

    for (const BlockDefinition& block : BLOCK_DEFINITIONS)
    {
        for (const LabelDefinition& label_def : block.m_labels)
        {
            m_block_metrics[block.m_id].push_back(std::make_unique<ComputeMetric>(data_provider, label_def.m_metric, label_def.m_name, label_def.m_unit));
        }
        for (const PlotDefinition& plot_def : block.m_plots)
        {
            std::unique_ptr<ComputePlot> plot;
            if (plot_def.m_plot_type == PlotTypePie)
            {
                plot = std::make_unique<ComputePlotPie>(data_provider, plot_def.m_type);
            }
            else if (plot_def.m_plot_type == PlotTypeBar)
            {
                plot = std::make_unique<ComputePlotBar>(data_provider, plot_def.m_type);
            }
            m_plots[block.m_id].push_back(std::move(plot));
        }
    }

    for (const LinkDefinition& link : LINK_DEFINITIONS)
    {
        m_link_metrics[link.m_id] = std::make_unique<ComputeMetric>(data_provider, link.m_label.m_metric, link.m_label.m_name, link.m_label.m_unit);
    }

    m_left_column = std::make_shared<RocCustomWidget>([this]()
    {
        this->RenderBlockDiagram();
    });
    m_right_column = std::make_shared<RocCustomWidget>([this]()
    {
        this->RenderBlockDetails();
    });

    LayoutItem::Ptr left = std::make_shared<LayoutItem>();
    left->m_item = m_left_column;
    LayoutItem::Ptr right = std::make_shared<LayoutItem>();
    right->m_item = m_right_column;
    m_container = std::make_shared<HSplitContainer>(left, right);
    m_container->SetSplit(0.5f);

    auto navigation_changed_handler = [this](std::shared_ptr<RocEvent> event) 
    {
        m_navigation_changed = true;
    };
    m_navigation_event_token = EventManager::GetInstance()->Subscribe(static_cast<int>(RocEvents::kComputeBlockNavigationChanged), navigation_changed_handler);
}

ComputeBlockView::~ComputeBlockView()
{
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kComputeBlockNavigationChanged), m_navigation_event_token);
}

void ComputeBlockView::Render()
{
    RenderMenuBar();
    if(m_container)
    {
        m_container->Render();
        return;
    }
}

void ComputeBlockView::RenderMenuBar()
{
    ImVec2 cursor_position = ImGui::GetCursorScreenPos();
    ImVec2 content_region = ImGui::GetContentRegionAvail();
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(cursor_position.x, cursor_position.y),
        ImVec2(cursor_position.x + content_region.x,
               cursor_position.y + ImGui::GetFrameHeightWithSpacing()),
        ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg]), 
        0.0f);

    ImGui::BeginDisabled(!m_navigation->BackAvailable());
    if(ImGui::ArrowButton("Compute_Block_View_Back", ImGuiDir_Left))
    {
        m_navigation->GoBack();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!m_navigation->ForwardAvailable());
    if(ImGui::ArrowButton("Compute_Block_View_Forward", ImGuiDir_Right))
    {
        m_navigation->GoForward();
    }
    ImGui::EndDisabled();
}

void ComputeBlockView::Update()
{
    for (auto& it : m_block_metrics)
    {
        for (std::unique_ptr<ComputeMetric>& metric : it.second)
        {
            metric->Update();
        }
    }
    for (auto& it : m_link_metrics)
    {
        it.second->Update();
    }
    for (auto it = m_plots.begin(); it != m_plots.end(); it ++)
    {
        for (std::unique_ptr<ComputePlot>& plot : it->second)
        {
            plot->Update();
        }
    }
}

void ComputeBlockView::RenderBlockDiagram()
{
    ImVec2 content_region = ImGui::GetContentRegionAvail();
    ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
    float  min_dim = std::min(content_region.x, content_region.y);
    m_block_diagram_region = ImVec2(min_dim * 0.9f, min_dim * 0.9f);
    m_block_diagram_center = ImVec2(cursor_pos.x + content_region.x / 2, cursor_pos.y + content_region.y / 2);
    m_draw_list = ImGui::GetWindowDrawList();

    ImGui::PushStyleColor(ImGuiCol_Button, BLOCK_HIGHLIGHT_COLOR);
    switch(m_navigation->Current().m_level)
    {
        case BlockLevelGPU: 
            RenderBlockDiagramGPU(); 
            break;
        case BlockLevelShaderEngine:
            RenderBlockDiagramShaderEngine();
            break;
        case BlockLevelComputeUnit:
            RenderBlockDiagramComputeUnit();
            break;
        case BlockLevelCache: 
            RenderBlockDiagramCache();
            break;
    }
    ImGui::PopStyleColor();
}

void ComputeBlockView::RenderBlockDiagramGPU()
{
    BlockButton(BlockIDGPU, ImVec2(0, 0), ImVec2(1, 1));
    BlockButton(BlockIDCP, ImVec2(0, -0.4f),ImVec2(0.9f, 0.1f));
    BlockButton(BlockIDCPC, ImVec2(-0.2175f, -0.4f), ImVec2(0.4075f, 0.05f));
    BlockButton(BlockIDCPF, ImVec2(0.2175f, -0.4f), ImVec2(0.4075f, 0.05f));
    if (BlockButton(BlockIDFabric, ImVec2(0, 0.4f), ImVec2(0.9f, 0.1f), BlockOption_Navigation | BlockOption_LabelMetrics))
    {
        m_navigation->Go(BlockLevelCache);
    }
    if (BlockButton(BlockIDL2, ImVec2(0, 0), ImVec2(0.2f, 0.6f), BlockOption_Navigation | BlockOption_LabelMetrics))
    {
        m_navigation->Go(BlockLevelCache);
    }
    for (int row = 0; row < 2; row++) 
    {
        for (int col = 0; col < 2; col++)
        {
            ImGui::PushID(row * 2 + col);
            if (BlockButton(BlockIDSE, ImVec2(0.3f - col * 0.6f, 0.165f - row * 0.33f), ImVec2(0.3f, 0.27f), BlockOption_Navigation))
            {
                m_navigation->Go(BlockLevelShaderEngine);
            }
            ImGui::PopID();
        }
    }
}

void ComputeBlockView::RenderBlockDiagramShaderEngine()
{
    BlockButton(BlockIDSE, ImVec2(0, 0), ImVec2(1, 1));
    BlockButton(BlockIDSPI, ImVec2(0.3325f, -0.25f), ImVec2(0.225f, 0.4f));
    if (BlockButton(BlockIDsL1D, ImVec2(0.375f, 0.1f), ImVec2(0.15f, 0.2f), BlockOption_Navigation | BlockOption_LabelMetrics))
    {
        m_navigation->Go(BlockLevelCache);
    }
    if (BlockButton(BlockIDL1I, ImVec2(0.375f, 0.35f), ImVec2(0.15f, 0.2f), BlockOption_Navigation | BlockOption_LabelMetrics))
    {
        m_navigation->Go(BlockLevelCache);
    }
    for (int i = 0; i < 5; i ++)
    {
        ImGui::PushID(i);
        if (BlockButton(BlockIDCU, ImVec2(-0.225f + i * 0.04f, -0.1 + i * 0.05f), ImVec2(0.45f, 0.7f), (i == 4 ? BlockOption_Navigation | BlockOption_LabelMetrics : BlockOption_Navigation)))
        {
            m_navigation->Go(BlockLevelComputeUnit);
        }
        ImGui::PopID();
    }
    
    Link(LinkIDCU_sL1D, ImVec2(0.16f, 0.1f), ImVec2(0.3f, 0.1f), ImGuiDir_Left);
    Link(LinkIDCU_L1I, ImVec2(0.16f, 0.35f), ImVec2(0.3f, 0.35f), ImGuiDir_Left);
}

void ComputeBlockView::RenderBlockDiagramComputeUnit()
{
    BlockButton(BlockIDCU, ImVec2(0, 0), ImVec2(1, 1));
    BlockButton(BlockIDScheduler, ImVec2(-0.175f, -0.4f), ImVec2(0.55f, 0.1f));
    if (BlockButton(BlockIDvL1D, ImVec2(0.375f, -0.2375f), ImVec2(0.15f, 0.425f), BlockOption_Navigation | BlockOption_LabelMetrics))
    {
        m_navigation->Go(BlockLevelCache);
    }
    BlockButton(BlockIDVALU, ImVec2(-0.4f, 0.075f), ImVec2(0.1f, 0.75f));
    BlockButton(BlockIDSALU, ImVec2(-0.25f, 0.075f), ImVec2(0.1f, 0.75f));
    BlockButton(BlockIDMFMA, ImVec2(-0.1f, 0.075f), ImVec2(0.1f, 0.75f));
    BlockButton(BlockIDBranch, ImVec2(0.05f, 0.075f), ImVec2(0.1f, 0.75f));
    BlockButton(BlockIDLDS, ImVec2(0.375f, 0.2375f), ImVec2(0.15f, 0.425f), BlockOption_LabelMetrics);

    ImVec4 divider_color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    divider_color.w = 0.5f;
    m_draw_list->AddLine(ImVec2(m_block_diagram_center.x + 0.16f * m_block_diagram_region.x, m_block_diagram_center.y - 0.5f * m_block_diagram_region.y), 
                         ImVec2(m_block_diagram_center.x + 0.16f * m_block_diagram_region.x, m_block_diagram_center.y + 0.5f * m_block_diagram_region.y),
                         ImGui::GetColorU32(divider_color));

    Link(LinkIDCU_vL1D_Read, ImVec2(0.16f, -0.2725f), ImVec2(0.3f, -0.2725f), ImGuiDir_Left);
    Link(LinkIDCU_vL1D_Write, ImVec2(0.16f, -0.2375f), ImVec2(0.3f, -0.2375f), ImGuiDir_Right);
    Link(LinkIDCU_vL1D_Atomic, ImVec2(0.16f, -0.2025f), ImVec2(0.3f, -0.2025f));
    Link(LinkIDCU_LDS, ImVec2(0.16f, 0.2375f), ImVec2(0.3f, 0.2375f), ImGuiDir_Left);
}

void ComputeBlockView::RenderBlockDiagramCache()
{
    BlockButton(BlockIDGPU, ImVec2(-0.06f, 0), ImVec2(0.9f, 1));
    BlockButton(BlockIDCP, ImVec2(-0.35f, -0.275f), ImVec2(0.2f, 0.35f));
    BlockButton(BlockIDCPC, ImVec2(-0.35f, -0.35f), ImVec2(0.1f, 0.1f));
    BlockButton(BlockIDCPF, ImVec2(-0.35f, -0.20f), ImVec2(0.1f, 0.1f));
    if (BlockButton(BlockIDSE, ImVec2(-0.325f, 0.2f), ImVec2(0.25f, 0.5f), BlockOption_Navigation))
    {
        m_navigation->Go(BlockLevelShaderEngine);
    }
    if (BlockButton(BlockIDCU, ImVec2(-0.325f, 0.05f), ImVec2(0.2f, 0.15f), BlockOption_Navigation))
    {
        m_navigation->Go(BlockLevelComputeUnit);
    }
    BlockButton(BlockIDvL1D, ImVec2(-0.325f, 0.05f), ImVec2(0.15f, 0.1f), BlockOption_LabelMetrics);
    BlockButton(BlockIDsL1D, ImVec2(-0.325f, 0.2f), ImVec2(0.15f, 0.1f), BlockOption_LabelMetrics);
    BlockButton(BlockIDL1I, ImVec2(-0.325f, 0.35f), ImVec2(0.15f, 0.1f), BlockOption_LabelMetrics);
    BlockButton(BlockIDL2, ImVec2(-0.025f, 0), ImVec2(0.15f, 0.9f), BlockOption_LabelMetrics);
    BlockButton(BlockIDFabric, ImVec2(0.25f, 0), ImVec2(0.15f, 0.55f), BlockOption_LabelMetrics);
    BlockButton(BlockIDHBM, ImVec2(0.475f, -0.2f), ImVec2(0.05f, 0.15f));
    BlockButton(BlockIDPCIe, ImVec2(0.475f, 0), ImVec2(0.05f, 0.15f));
    BlockButton(BlockIDDRAM, ImVec2(0.475f, 0.2f), ImVec2(0.05f, 0.15f));

    Link(LinkIDCPC_L2, ImVec2(-0.3f, -0.35f), ImVec2(-0.1f, -0.35f), ImGuiDir_None);
    Link(LinkIDCPF_L2, ImVec2(-0.3f, -0.2f), ImVec2(-0.1f, -0.2f), ImGuiDir_None);
    Link(LinkIDvL1D_L2_Read, ImVec2(-0.25f, 0.015f), ImVec2(-0.1f, 0.015f), ImGuiDir_Left);
    Link(LinkIDvL1D_L2_Write, ImVec2(-0.25f, 0.05f), ImVec2(-0.1f, 0.05f), ImGuiDir_Right);
    Link(LinkIDvL1D_L2_Atomic, ImVec2(-0.25f, 0.085f), ImVec2(-0.1f, 0.085f), ImGuiDir_None);
    Link(LinkIDsL1D_L2_Read, ImVec2(-0.25f, 0.165f), ImVec2(-0.1f, 0.165f), ImGuiDir_Left);
    Link(LinkIDsL1D_L2_Write, ImVec2(-0.25f, 0.2f), ImVec2(-0.1f, 0.2f), ImGuiDir_Right);
    Link(LinkIDsL1D_L2_Atomic, ImVec2(-0.25f, 0.235f), ImVec2(-0.1f, 0.235f), ImGuiDir_None);
    Link(LinkIDL1I_L2, ImVec2(-0.25f, 0.35f), ImVec2(-0.1f, 0.35f), ImGuiDir_Left);
    Link(LinkIDL2_Fabric_Read, ImVec2(0.05f, -0.035f), ImVec2(0.175f, -0.035f), ImGuiDir_Left);
    Link(LinkIDL2_Fabric_Write, ImVec2(0.05f, 0), ImVec2(0.175f, 0), ImGuiDir_Right);
    Link(LinkIDL2_Fabric_Atomic, ImVec2(0.05f, 0.035f), ImVec2(0.175f, 0.035f), ImGuiDir_None);
    Link(LinkIDFabric_DRAM_Read, ImVec2(0.325f, 0.1825f), ImVec2(0.45f, 0.1825f), ImGuiDir_Left);
    Link(LinkIDFabric_DRAM_Write, ImVec2(0.325f, 0.2175f), ImVec2(0.45f, 0.2175f), ImGuiDir_Right);
    Link(LinkIDFabric_PCIe_Read, ImVec2(0.325f, -0.0175f), ImVec2(0.45f, -0.0175f), ImGuiDir_Left);
    Link(LinkIDFabric_PCIe_Write, ImVec2(0.325f, 0.0175f), ImVec2(0.45f, 0.0175f), ImGuiDir_Right);
    Link(LinkIDFabric_HBM_Read, ImVec2(0.325f, -0.2175f), ImVec2(0.45f, -0.2175f), ImGuiDir_Left);
    Link(LinkIDFabric_HBM_Write, ImVec2(0.325f, -0.1825f), ImVec2(0.45f, -0.1825f), ImGuiDir_Right);
}

void ComputeBlockView::RenderBlockDetails() 
{
    for (int i = 0; i < BLOCK_DEFINITIONS.size(); i ++)
    {
        const BlockDefinition& block = BLOCK_DEFINITIONS[i];
        if (block.m_levels & m_navigation->Current().m_level)
        {            
            if (m_navigation_changed)
            {
                ImGui::SetNextItemOpen(m_navigation->Current().m_block == BlockIDNone || m_navigation->Current().m_block == block.m_id);
            }
            bool empty = (m_plots.count(block.m_id) == 0);
            ImVec2 table_shortcut_size(ImGui::CalcTextSize("View Table", NULL, true).x + 3 * ImGui::GetStyle().FramePadding.x, ImGui::GetFrameHeightWithSpacing());
            ImGui::SetNextItemAllowOverlap();
            if (ImGui::CollapsingHeader(block.m_full_name.c_str()))
            {
                if (empty)
                {
                    ImGui::Text("TBD");
                }
                else
                {   
                    ImGui::SameLine();
                    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x - table_shortcut_size.x , table_shortcut_size.y));
                    ImGui::SameLine();
                    ImGui::PushID(i);
                    if (ImGui::SmallButton("View Table"))
                    {
                        NavigationManager::GetInstance()->Go(m_owner_id + "/" + COMPUTE_TABLE_VIEW_URL + "/" + block.m_table_url);                 
                    }
                    for (std::unique_ptr<ComputePlot>& plot : m_plots[block.m_id])
                    {
                        plot->Render();
                    }
                    ImGui::PopID();
                }
            }
            else if (!empty)
            {           
                ImGui::SameLine();
                ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x - table_shortcut_size.x , table_shortcut_size.y));
                ImGui::SameLine();
                ImGui::PushID(i);
                if (ImGui::SmallButton("View Table"))
                {
                    NavigationManager::GetInstance()->Go(m_owner_id + "/" + COMPUTE_TABLE_VIEW_URL + "/" + block.m_table_url);
                }
                ImGui::PopID();
            }
        }
    }
    m_navigation_changed = false;
}

bool ComputeBlockView::BlockButton(BlockID id, ImVec2 rel_pos, ImVec2 rel_size, BlockOptionFlags options)
{
    const ImVec2 abs_size(rel_size * m_block_diagram_region);
    const ImVec2 abs_pos(m_block_diagram_center + m_block_diagram_region * rel_pos - abs_size / 2);

    const char* name = BLOCK_DEFINITIONS[id].m_short_name.c_str();
    const char* tooltip = BLOCK_DEFINITIONS[id].m_tooltip.empty() ? BLOCK_DEFINITIONS[id].m_full_name.c_str() : BLOCK_DEFINITIONS[id].m_tooltip.c_str();

    ImGui::SetCursorScreenPos(abs_pos);
    ImGui::SetNextItemAllowOverlap();
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImGui::GetStyleColorVec4(ImGuiCol_Button));
    bool pressed = ImGui::Button(std::string("##").append(name).c_str(), abs_size);
    ImGui::PopStyleColor();
    if (m_navigation->Current().m_block == id || ImGui::IsItemHovered())
    {
        m_draw_list->AddRect(abs_pos, abs_pos + abs_size, ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_Text)));
    }
    if (strlen(tooltip) > 0)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(WINDOW_PADDING_DEFAULT, WINDOW_PADDING_DEFAULT));
        ImGui::SetItemTooltip(tooltip);
        ImGui::PopStyleVar();
    }
    ImGui::SetCursorScreenPos(abs_pos + m_block_diagram_region * 0.01f);
    ImGui::PushTextWrapPos(ImGui::GetCursorScreenPos().x + abs_size.x);
    ImGui::Text(name);
    ImGui::PopTextWrapPos();
    if (options & BlockOption_LabelMetrics)
    {    
        float label_pos_y = abs_pos.y + (abs_size.y - ImGui::GetTextLineHeightWithSpacing() * m_block_metrics[id].size()) / 2;
        for (int i = 0; i < m_block_metrics[id].size(); i ++)
        {
            const std::string& metric = m_block_metrics[id][i]->GetFormattedString();
            const ImVec2 metric_size = ImGui::CalcTextSize(metric.c_str());
            ImGui::SetCursorScreenPos(ImVec2(abs_pos.x + (abs_size.x - metric_size.x) / 2, label_pos_y + i * metric_size.y));
            ImGui::Text(metric.c_str());
        }
    }
    if (options & BlockOption_Navigation)
    {
        m_draw_list->AddTriangleFilled(
            ImVec2(abs_pos.x + abs_size.x - 0.02f * m_block_diagram_region.x,
                   abs_pos.y + 0.01f * m_block_diagram_region.y),
            ImVec2(abs_pos.x + abs_size.x - 0.01f * m_block_diagram_region.x,
                   abs_pos.y + 0.01f * m_block_diagram_region.y),
            ImVec2(abs_pos.x + abs_size.x - 0.01f * m_block_diagram_region.x,
                   abs_pos.y + 0.02f * m_block_diagram_region.y),
            ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_Text)));
    }
    else if (pressed)
    {
        m_navigation->Select(id);
    }
    return pressed;
}

void ComputeBlockView::Link(LinkID id, ImVec2 rel_left, ImVec2 rel_right, ImGuiDir direction)
{
    const ImVec2 abs_left(m_block_diagram_center + m_block_diagram_region * rel_left);
    const ImVec2 abs_right(m_block_diagram_center + m_block_diagram_region * rel_right);

    m_draw_list->AddLine(abs_left, abs_right, ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_Text)));
    if(direction == ImGuiDir_Left || direction == ImGuiDir_None)
    {
        m_draw_list->AddTriangleFilled(
        abs_left,
        ImVec2(abs_left.x + 0.005f * m_block_diagram_region.x,
                abs_left.y - 0.005f * m_block_diagram_region.y),
        ImVec2(abs_left.x + 0.005f * m_block_diagram_region.x,
                abs_left.y + 0.005f * m_block_diagram_region.y),
        ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_Text)));
    }
    if(direction == ImGuiDir_Right || direction == ImGuiDir_None)
    {
        m_draw_list->AddTriangleFilled(
        abs_right,
        ImVec2(abs_right.x - 0.005f * m_block_diagram_region.x,
                abs_right.y - 0.005f * m_block_diagram_region.y),
        ImVec2(abs_right.x - 0.005f * m_block_diagram_region.x,
                abs_right.y + 0.005f * m_block_diagram_region.y),
        ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_Text)));
    }
    if (m_link_metrics.count(id) > 0 && !m_link_metrics[id]->GetFormattedString().empty())
    {
        const ImVec2 label_size = ImGui::CalcTextSize(m_link_metrics[id]->GetFormattedString().c_str());
        const ImVec2 label_pos = ImVec2(abs_left.x + (abs_right.x - abs_left.x - label_size.x) / 2, abs_left.y - label_size.y);
        m_draw_list->AddRectFilled(ImVec2(label_pos.x - 2, label_pos.y), ImVec2(label_pos.x + label_size.x + 1, label_pos.y + label_size.y), ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_Text)));
        ImGui::SetCursorScreenPos(label_pos);
        ImVec4 text_color = ImVec4(1, 1, 1, 1) - ImGui::GetStyleColorVec4(ImGuiCol_Text);
        text_color.w = 1;
        ImGui::TextColored(text_color, m_link_metrics[id]->GetFormattedString().c_str());
    }
}

}  // namespace View
}  // namespace RocProfVis
