// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_compute_block.h"

namespace RocProfVis
{
namespace View
{

const std::array BLOCK_DEFINITIONS {
    block_diagram_block_info_t{kBlockDiagramBlockGPU, kBlockDiagramLevelGPU | kBlockDiagramLevelCache, "GPU", "GPU", ""},
    block_diagram_block_info_t{kBlockDiagramBlockCP, kBlockDiagramLevelGPU | kBlockDiagramLevelCache, "CP", "Command Processor", ""},
    block_diagram_block_info_t{kBlockDiagramBlockCPC, kBlockDiagramLevelGPU | kBlockDiagramLevelCache, "CPC", "Command Processor Packet Processor", ""},
    block_diagram_block_info_t{kBlockDiagramBlockCPF, kBlockDiagramLevelGPU | kBlockDiagramLevelCache, "CPF", "Command Processor Fetcher", ""},
    block_diagram_block_info_t{kBlockDiagramBlockSE, kBlockDiagramLevelGPU | kBlockDiagramlevelShaderEngine | kBlockDiagramLevelCache, "SE", "Shader Engine", ""},
    block_diagram_block_info_t{kBlockDiagramBlockL2, kBlockDiagramLevelGPU | kBlockDiagramLevelCache, "L2", "L2 Cache", "", { 
        block_diagram_plot_info_t{"17.1_Speed-of-Light.csv", {0, 1}}, 
        block_diagram_plot_info_t{"17.4_L2_-_Fabric_Interface_Stalls.csv", {0, 1}}
    }, {
        block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Rd", "L2 Rd Value"},
        block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Wr", "L2 Wr Value"},
        block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Atomic", "L2 Atomic Value"},
        block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Hit", "L2 Hit Value"}
    }},
    block_diagram_block_info_t{kBlockDiagramBlockFabric, kBlockDiagramLevelGPU | kBlockDiagramLevelCache, "Fabric", "Infinity Fabric", "", {}, {
        block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Rd", "Fabric Rd Lat Value"},
        block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Wr", "Fabric Wr Lat Value"},
        block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Atomic", "Fabric Atomic Lat Value"}
    }},
    block_diagram_block_info_t{kBlockDiagramBlockCU, kBlockDiagramlevelShaderEngine | kBlockDiagramLevelComputeUnit | kBlockDiagramLevelCache, "CU", "Compute Unit", "", {
        block_diagram_plot_info_t{"10.1_Overall_Instruction_Mix.csv", {0}}, 
        block_diagram_plot_info_t{"11.1_Speed-of-Light.csv", {0}}
    }, {}},
    block_diagram_block_info_t{kBlockDiagramBlockSPI, kBlockDiagramlevelShaderEngine, "SPI", "Workgroup Manager", "", {}, {}},
    block_diagram_block_info_t{kBlockDiagramBlocksL1D, kBlockDiagramlevelShaderEngine | kBlockDiagramLevelCache, "sL1D", "Scalar L1 Data Cache", "", {
        block_diagram_plot_info_t{"14.1_Speed-of-Light.csv", {0}}     
    }, {
        block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Hit", "VL1D Hit Value"}
    }},
    block_diagram_block_info_t{kBlockDiagramBlockvL1D, kBlockDiagramLevelComputeUnit | kBlockDiagramLevelCache, "vL1D", "Vector L1 Data Cache", "", {
        block_diagram_plot_info_t{"16.1_Speed-of-Light.csv", {0}},
        block_diagram_plot_info_t{"16.4_L1D_-_L2_Transactions.csv", {0, 1, 2, 3}}
    }, {
        block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Hit", "VL1 Hit Value"},
        block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Coales", "VL1 Coalesce Value"},
        block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Stall", "VL1 Stall Value"}
    }},
    block_diagram_block_info_t{kBlockDiagramBlockL1I, kBlockDiagramlevelShaderEngine | kBlockDiagramLevelCache, "L1I", "L1 Instruction Cache", "", {
        block_diagram_plot_info_t{"13.1_Speed-of-Light.csv", {0}}
    }, {
        block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Hit", "IL1 Hit Value"},
        block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Lat", "IL1 Lat Value"}
    }},
    block_diagram_block_info_t{kBlockDiagramBlockScheduler, kBlockDiagramLevelComputeUnit, "Scheduler", "Scheduler", "", {}, {}},
    block_diagram_block_info_t{kBlockDiagramBlockVALU, kBlockDiagramLevelComputeUnit, "VALU", "Vector Arithmetic Logic Unit", "", {
        block_diagram_plot_info_t{"10.2_VALU_Arithmetic_Instr_Mix.csv", {0}}
    }, {}},
    block_diagram_block_info_t{kBlockDiagramBlockSALU, kBlockDiagramLevelComputeUnit, "SALU", "Scalar Arithmetic Logic Unit", "", {}, {}},
    block_diagram_block_info_t{kBlockDiagramBlockMFMA, kBlockDiagramLevelComputeUnit, "MFMA", "Matrix Fused Multiply-Add", "", {}, {}},
    block_diagram_block_info_t{kBlockDiagramBlockBranch, kBlockDiagramLevelComputeUnit, "Branch", "Branch", "", {}, {}},
    block_diagram_block_info_t{kBlockDiagramBlockLDS, kBlockDiagramLevelComputeUnit, "LDS", "Local Data Share", "", {
        block_diagram_plot_info_t{"12.1_Speed-of-Light.csv", {0}}
    }, {
        block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Util", "LDS Util Value"},
        block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Lat", "LDS Latency Value"}
    }},
    block_diagram_block_info_t{kBlockDiagramBlockHBM, kBlockDiagramLevelCache, "HBM", "HBM", "", {}, {}},
    block_diagram_block_info_t{kBlockDiagramBlockPCIe, kBlockDiagramLevelCache, "PCIe", "PCIe", "", {}, {}},
    block_diagram_block_info_t{kBlockDiagramBlockDRAM, kBlockDiagramLevelCache, "CPU DRAM", "CPU DRAM", "", {}, {}}
};

const std::array LINK_DEFINITIONS {
    block_diagram_link_info_t{kBlockDiagramLinkCPC_L2, block_diagram_metric_info_t{"", "", ""}},
    block_diagram_link_info_t{kBlockDiagramLinkCPF_L2, block_diagram_metric_info_t{"", "", ""}},
    block_diagram_link_info_t{kBlockDiagramLinkvL1D_L2_Read, block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Rd", "VL1_L2 Rd Value"}},
    block_diagram_link_info_t{kBlockDiagramLinkvL1D_L2_Write, block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Wr", "VL1_L2 Wr Value"}},
    block_diagram_link_info_t{kBlockDiagramLinkvL1D_L2_Atomic, block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Atomic", "VL1_L2 Atomic Value"}},
    block_diagram_link_info_t{kBlockDiagramLinksL1D_L2_Read, block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Rd", "VL1D_L2 Rd Value"}},
    block_diagram_link_info_t{kBlockDiagramLinksL1D_L2_Write, block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Wr", "VL1D_L2 Wr Value"}},
    block_diagram_link_info_t{kBlockDiagramLinksL1D_L2_Atomic, block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Atomic", "VL1D_L2 Atomic Value"}},
    block_diagram_link_info_t{kBlockDiagramLinkL1I_L2, block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Rd", "IL1_L2 Rd Value"}},
    block_diagram_link_info_t{kBlockDiagramLinkL2_Fabric_Read, block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Rd", "Fabric_L2 Rd Value"}},
    block_diagram_link_info_t{kBlockDiagramLinkL2_Fabric_Write, block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Wr", "Fabric_L2 Wr Value"}},
    block_diagram_link_info_t{kBlockDiagramLinkL2_Fabric_Atomic, block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Atomic", "Fabric_L2 Atomic Value"}},
    block_diagram_link_info_t{kBlockDiagramLinkFabric_PCIe_Read, block_diagram_metric_info_t{"", "", ""}},
    block_diagram_link_info_t{kBlockDiagramLinkFabric_PCIe_Write, block_diagram_metric_info_t{"", "", ""}},
    block_diagram_link_info_t{kBlockDiagramLinkFabric_HBM_Read, block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Rd", "HBM Rd Value"}},
    block_diagram_link_info_t{kBlockDiagramLinkFabric_HBM_Write, block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Wr", "HBM Wr Value"}},
    block_diagram_link_info_t{kBlockDiagramLinkFabric_DRAM_Read, block_diagram_metric_info_t{"", "", ""}},
    block_diagram_link_info_t{kBlockDiagramLinkFabric_DRAM_Write, block_diagram_metric_info_t{"", "", ""}},
    block_diagram_link_info_t{kBlockDiagramLinkCU_vL1D_Read, block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Rd", "VL1 Rd Value"}},
    block_diagram_link_info_t{kBlockDiagramLinkCU_vL1D_Write, block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Wr", "VL1 Wr Value"}},
    block_diagram_link_info_t{kBlockDiagramLinkCU_vL1D_Atomic, block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Atomic", "VL1 Atomic Value"}},
    block_diagram_link_info_t{kBlockDiagramLinkCU_sL1D, block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Rd", "VL1D Rd Value"}},
    block_diagram_link_info_t{kBlockDiagramLinkCU_L1I, block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Fetch", "IL1 Fetch Value"}},
    block_diagram_link_info_t{kBlockDiagramLinkCU_LDS, block_diagram_metric_info_t{"3.1_Memory_Chart.csv", "Rd", "LDS Req Value"}}
};

constexpr int WINDOW_PADDING_DEFAULT = 8;
constexpr int LEVEL_HISTORY_LIMIT = 5;

ComputeBlockDetails::ComputeBlockDetails(std::shared_ptr<ComputeDataProvider> data_provider)
: m_block_navigation_event_token(-1)
, m_navigation_changed(true)
, m_current_location(block_diagram_navigation_location_t{kBlockDiagramLevelGPU, kBlockDiagramBlockNone})
{
    for (const block_diagram_block_info_t& block : BLOCK_DEFINITIONS)
    {
        for (const block_diagram_plot_info_t& plot : block.m_plot_infos)
        {
            std::unique_ptr<ComputeMetricGroup> metric = std::make_unique<ComputeMetricGroup>(data_provider, plot.m_metric_category, kComputeMetricBar, std::vector<int>{0}, plot.m_plot_indexes);
            m_metrics_map[block.m_id].push_back(std::move(metric)); 
        }
    }

    auto block_navigation_event_handler = [this](std::shared_ptr<RocEvent> event) 
    {
        this->OnNavigationChanged(event);
    };
    m_block_navigation_event_token = EventManager::GetInstance()->Subscribe(static_cast<int>(RocEvents::kComputeBlockNavigationChanged), block_navigation_event_handler);
}

ComputeBlockDetails::~ComputeBlockDetails() 
{
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kComputeBlockNavigationChanged), m_block_navigation_event_token);
}

void ComputeBlockDetails::Render() 
{
    for (const block_diagram_block_info_t& block : BLOCK_DEFINITIONS)
    {
        if (block.m_levels & m_current_location.m_level)
        {
            if (m_navigation_changed)
            {
                ImGui::SetNextItemOpen(m_current_location.m_block == kBlockDiagramBlockNone || m_current_location.m_block == block.m_id);
            }           
            if (ImGui::CollapsingHeader(block.m_full_name.c_str()))
            {
                if (m_metrics_map.count(block.m_id) == 0)
                {
                    ImGui::Text("TBD");
                }
                else
                {
                    for (std::unique_ptr<ComputeMetricGroup>& metric : m_metrics_map[block.m_id])
                    {
                        metric->Render();
                    }
                }
            }
        }
    }
    m_navigation_changed = false;
}

void ComputeBlockDetails::Update()
{
    for (auto it = m_metrics_map.begin(); it != m_metrics_map.end(); it ++)
    {
        for (std::unique_ptr<ComputeMetricGroup>& metric : it->second)
        {
            metric->Update();
        }
    }
}

void ComputeBlockDetails::OnNavigationChanged(std::shared_ptr<RocEvent> event)
{
    std::shared_ptr<ComputeBlockNavitionEvent> navigation_event = std::dynamic_pointer_cast<ComputeBlockNavitionEvent>(event);
    m_current_location.m_level = static_cast<block_diagram_level_t>(navigation_event->GetLevel());
    m_current_location.m_block = static_cast<block_diagram_block_id_t>(navigation_event->GetBlock());
    m_navigation_changed = true;
}

ComputeBlockDiagramNavHelper::ComputeBlockDiagramNavHelper()
: m_current_location(block_diagram_navigation_location_t{kBlockDiagramLevelGPU, kBlockDiagramBlockNone})
{}

block_diagram_navigation_location_t ComputeBlockDiagramNavHelper::Current()
{
    return m_current_location;
}

bool RocProfVis::View::ComputeBlockDiagramNavHelper::BackAvailable()
{
    return !m_previous_levels.empty();
}

bool RocProfVis::View::ComputeBlockDiagramNavHelper::ForwardAvailable()
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
        m_current_location.m_block = kBlockDiagramBlockNone;
        EventManager::GetInstance()->AddEvent(
            std::make_shared<ComputeBlockNavitionEvent>(static_cast<int>(RocEvents::kComputeBlockNavigationChanged), m_current_location.m_level, m_current_location.m_block)
        );
    }
}

void ComputeBlockDiagramNavHelper::GoForward()
{
    if (!m_next_levels.empty())
    {
        m_previous_levels.push_back(m_current_location.m_level);
        m_current_location.m_level = m_next_levels.back();        
        m_next_levels.pop_back();
        m_current_location.m_block = kBlockDiagramBlockNone;
        EventManager::GetInstance()->AddEvent(
            std::make_shared<ComputeBlockNavitionEvent>(static_cast<int>(RocEvents::kComputeBlockNavigationChanged), m_current_location.m_level, m_current_location.m_block)
        );
    }
}

void ComputeBlockDiagramNavHelper::Go(block_diagram_level_t level)
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
        m_current_location.m_block = kBlockDiagramBlockNone;
        EventManager::GetInstance()->AddEvent(
            std::make_shared<ComputeBlockNavitionEvent>(static_cast<int>(RocEvents::kComputeBlockNavigationChanged), m_current_location.m_level, m_current_location.m_block)
        );
    }
}

void ComputeBlockDiagramNavHelper::Select(block_diagram_block_id_t block)
{
    if (m_current_location.m_block != block)
    {
        m_current_location.m_block = block;
    }
    else
    {
        m_current_location.m_block = kBlockDiagramBlockNone;
    }
    EventManager::GetInstance()->AddEvent(
        std::make_shared<ComputeBlockNavitionEvent>(static_cast<int>(RocEvents::kComputeBlockNavigationChanged), m_current_location.m_level, m_current_location.m_block)
    );
}

ComputeBlockDiagram::ComputeBlockDiagram(std::shared_ptr<ComputeDataProvider> data_provider)
: m_content_region(ImVec2(-1, -1))
, m_content_region_center(ImVec2(-1, -1))
, m_draw_list(nullptr)
, m_navigation(nullptr)
{
    m_navigation = std::make_unique<ComputeBlockDiagramNavHelper>();
   
    for (int i = 0; i < BLOCK_DEFINITIONS.size(); i ++)
    {
        for (const block_diagram_metric_info_t& metric : BLOCK_DEFINITIONS[i].m_metric_infos)
        { 
            m_block_metrics[i].push_back(std::move(std::make_unique<ComputeMetric>(data_provider, metric.m_metric_category, metric.m_metric_id, metric.m_label))); 
        }
    }

    for (int i = 0; i < LINK_DEFINITIONS.size(); i ++)
    {
        m_link_metrics[i] = std::make_unique<ComputeMetric>(data_provider, LINK_DEFINITIONS[i].m_metric_info.m_metric_category, LINK_DEFINITIONS[i].m_metric_info.m_metric_id, LINK_DEFINITIONS[i].m_metric_info.m_label);
    }
}

ComputeBlockDiagram::~ComputeBlockDiagram() {}

bool
RocProfVis::View::ComputeBlockDiagram::BlockButton(block_diagram_block_id_t id, ImVec2 rel_pos, ImVec2 rel_size, block_diagram_block_option_flags options)
{
    const ImVec2 abs_size(rel_size * m_content_region);
    const ImVec2 abs_pos(m_content_region_center + m_content_region * rel_pos - abs_size / 2);

    const char* name = BLOCK_DEFINITIONS[id].m_short_name.c_str();
    const char* tooltip = BLOCK_DEFINITIONS[id].m_tooltip.empty() ? BLOCK_DEFINITIONS[id].m_full_name.c_str() : BLOCK_DEFINITIONS[id].m_tooltip.c_str();

    ImGui::SetCursorScreenPos(abs_pos);
    ImGui::SetNextItemAllowOverlap();
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImGui::GetStyleColorVec4(ImGuiCol_Button));
    bool pressed = ImGui::Button(std::string("##").append(name).c_str(), abs_size);
    ImGui::PopStyleColor();
    if (m_navigation->Current().m_block == id || ImGui::IsItemHovered())
    {
        m_draw_list->AddRect(abs_pos, abs_pos + abs_size, IM_COL32(0, 0, 0, 255));
    }
    if (strlen(tooltip) > 0)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(WINDOW_PADDING_DEFAULT, WINDOW_PADDING_DEFAULT));
        ImGui::SetItemTooltip(tooltip);
        ImGui::PopStyleVar();
    }
    ImGui::SetCursorScreenPos(abs_pos + m_content_region * 0.01f);
    ImGui::Text(name);
    if (options & kBlockDiagramBlockOption_LabelMetrics)
    {    
        float label_pos_y = abs_pos.y + (abs_size.y - ImGui::GetTextLineHeightWithSpacing() * m_block_metrics[id].size()) / 2;
        for (int i = 0; i < m_block_metrics[id].size(); i ++)
        {
            const std::string& metric = m_block_metrics[id][i]->FormattedString();
            const ImVec2 metric_size = ImGui::CalcTextSize(metric.c_str());
            ImGui::SetCursorScreenPos(ImVec2(abs_pos.x + (abs_size.x - metric_size.x) / 2, label_pos_y + i * metric_size.y));
            ImGui::Text(metric.c_str());
        }
    }
    if (options & kBlockDiagramBlockOption_Navigation)
    {
        m_draw_list->AddTriangleFilled(
            ImVec2(abs_pos.x + abs_size.x - 0.02f * m_content_region.x,
                   abs_pos.y + 0.01f * m_content_region.y),
            ImVec2(abs_pos.x + abs_size.x - 0.01f * m_content_region.x,
                   abs_pos.y + 0.01f * m_content_region.y),
            ImVec2(abs_pos.x + abs_size.x - 0.01f * m_content_region.x,
                   abs_pos.y + 0.02f * m_content_region.y),
            IM_COL32(0, 0, 0, 255));
    }
    else if (pressed)
    {
        m_navigation->Select(id);
    }
    return pressed;
}

void ComputeBlockDiagram::Link(block_diagram_link_id_t id, ImVec2 rel_left, ImVec2 rel_right, ImGuiDir direction)
{
    const ImVec2 abs_left(m_content_region_center + m_content_region * rel_left);
    const ImVec2 abs_right(m_content_region_center + m_content_region * rel_right);

    m_draw_list->AddLine(abs_left, abs_right, IM_COL32(0, 0, 0, 255));
    if(direction == ImGuiDir_Left || direction == ImGuiDir_None)
    {
        m_draw_list->AddTriangleFilled(
        abs_left,
        ImVec2(abs_left.x + 0.005f * m_content_region.x,
                abs_left.y - 0.005f * m_content_region.y),
        ImVec2(abs_left.x + 0.005f * m_content_region.x,
                abs_left.y + 0.005f * m_content_region.y),
        IM_COL32(0, 0, 0, 255));
    }
    if(direction == ImGuiDir_Right || direction == ImGuiDir_None)
    {
        m_draw_list->AddTriangleFilled(
        abs_right,
        ImVec2(abs_right.x - 0.005f * m_content_region.x,
                abs_right.y - 0.005f * m_content_region.y),
        ImVec2(abs_right.x - 0.005f * m_content_region.x,
                abs_right.y + 0.005f * m_content_region.y),
        IM_COL32(0, 0, 0, 255));
    }

    if (m_link_metrics[id] && !m_link_metrics[id]->FormattedString().empty())
    {
        const ImVec2 label_size = ImGui::CalcTextSize(m_link_metrics[id]->FormattedString().c_str());
        const ImVec2 label_pos = ImVec2(abs_left.x + (abs_right.x - abs_left.x - label_size.x) / 2, abs_left.y - label_size.y);
        m_draw_list->AddRectFilled(ImVec2(label_pos.x - 2, label_pos.y), ImVec2(label_pos.x + label_size.x + 1, label_pos.y + label_size.y), IM_COL32(0, 0, 0, 255));
        ImGui::SetCursorScreenPos(label_pos);
        ImGui::TextColored(ImVec4(1, 1, 1, 1), m_link_metrics[id]->FormattedString().c_str());
    }
}

void ComputeBlockDiagram::RenderGPULevel()
{
    BlockButton(kBlockDiagramBlockGPU, ImVec2(0, 0), ImVec2(1, 1), kBLockDiagramBlockOption_None);
    BlockButton(kBlockDiagramBlockCP, ImVec2(0, -0.4f),ImVec2(0.9f, 0.1f), kBLockDiagramBlockOption_None);
    BlockButton(kBlockDiagramBlockCPC, ImVec2(-0.2175f, -0.4f), ImVec2(0.4075f, 0.05f));
    BlockButton(kBlockDiagramBlockCPF, ImVec2(0.2175f, -0.4f), ImVec2(0.4075f, 0.05f));
    if (BlockButton(kBlockDiagramBlockFabric, ImVec2(0, 0.4f), ImVec2(0.9f, 0.1f), kBlockDiagramBlockOption_Navigation | kBlockDiagramBlockOption_LabelMetrics))
    {
        m_navigation->Go(kBlockDiagramLevelCache);
    }
    if (BlockButton(kBlockDiagramBlockL2, ImVec2(0, 0), ImVec2(0.2f, 0.6f), kBlockDiagramBlockOption_Navigation | kBlockDiagramBlockOption_LabelMetrics))
    {
        m_navigation->Go(kBlockDiagramLevelCache);
    }
    for (int row = 0; row < 2; row++) 
    {
        for (int col = 0; col < 2; col++)
        {
            ImGui::PushID(row * 2 + col);
            if (BlockButton(kBlockDiagramBlockSE, ImVec2(0.3f - col * 0.6f, 0.165f - row * 0.33f), ImVec2(0.3f, 0.27f), 
                            kBlockDiagramBlockOption_Navigation | kBlockDiagramBlockOption_LabelMetrics))
            {
                m_navigation->Go(kBlockDiagramlevelShaderEngine);
            }
            ImGui::PopID();
        }
    }
}

void ComputeBlockDiagram::RenderShaderEngineLevel()
{
    BlockButton(kBlockDiagramBlockSE, ImVec2(0, 0), ImVec2(1, 1), kBLockDiagramBlockOption_None);
    BlockButton(kBlockDiagramBlockSPI, ImVec2(0.3325f, -0.25f), ImVec2(0.225f, 0.4f));
    if (BlockButton(kBlockDiagramBlocksL1D, ImVec2(0.375f, 0.1f), ImVec2(0.15f, 0.2f), kBlockDiagramBlockOption_Navigation | kBlockDiagramBlockOption_LabelMetrics))
    {
        m_navigation->Go(kBlockDiagramLevelCache);
    }
    if (BlockButton(kBlockDiagramBlockL1I, ImVec2(0.375f, 0.35f), ImVec2(0.15f, 0.2f), kBlockDiagramBlockOption_Navigation | kBlockDiagramBlockOption_LabelMetrics))
    {
        m_navigation->Go(kBlockDiagramLevelCache);
    }
    for (int i = 0; i < 5; i ++)
    {
        ImGui::PushID(i);
        if (BlockButton(kBlockDiagramBlockCU, ImVec2(-0.225f + i * 0.04f, -0.1 + i * 0.05f), ImVec2(0.45f, 0.7f), kBlockDiagramBlockOption_Navigation | kBlockDiagramBlockOption_LabelMetrics))
        {
            m_navigation->Go(kBlockDiagramLevelComputeUnit);
        }
        ImGui::PopID();
    }
    
    Link(kBlockDiagramLinkCU_sL1D, ImVec2(0.16f, 0.1f), ImVec2(0.3f, 0.1f), ImGuiDir_Left);
    Link(kBlockDiagramLinkCU_L1I, ImVec2(0.16f, 0.35f), ImVec2(0.3f, 0.35f), ImGuiDir_Left);
}

void ComputeBlockDiagram::RenderComputeUnitLevel()
{
    BlockButton(kBlockDiagramBlockCU, ImVec2(0, 0), ImVec2(1, 1), kBLockDiagramBlockOption_None);
    BlockButton(kBlockDiagramBlockScheduler, ImVec2(-0.175f, -0.4f), ImVec2(0.55f, 0.1f));
    if (BlockButton(kBlockDiagramBlockvL1D, ImVec2(0.375f, -0.2375f), ImVec2(0.15f, 0.425f), kBlockDiagramBlockOption_Navigation | kBlockDiagramBlockOption_LabelMetrics))
    {
        m_navigation->Go(kBlockDiagramLevelCache);
    }
    BlockButton(kBlockDiagramBlockVALU, ImVec2(-0.4f, 0.075f), ImVec2(0.1f, 0.75f));
    BlockButton(kBlockDiagramBlockSALU, ImVec2(-0.25f, 0.075f), ImVec2(0.1f, 0.75f));
    BlockButton(kBlockDiagramBlockMFMA, ImVec2(-0.1f, 0.075f), ImVec2(0.1f, 0.75f));
    BlockButton(kBlockDiagramBlockBranch, ImVec2(0.05f, 0.075f), ImVec2(0.1f, 0.75f));
    BlockButton(kBlockDiagramBlockLDS, ImVec2(0.375f, 0.2375f), ImVec2(0.15f, 0.425f));

    m_draw_list->AddLine(ImVec2(m_content_region_center.x + 0.16f * m_content_region.x, m_content_region_center.y - 0.5f * m_content_region.y), 
                         ImVec2(m_content_region_center.x + 0.16f * m_content_region.x, m_content_region_center.y + 0.5f * m_content_region.y),
                         IM_COL32(0, 0, 0, 64));

    Link(kBlockDiagramLinkCU_vL1D_Read, ImVec2(0.16f, -0.2725f), ImVec2(0.3f, -0.2725f), ImGuiDir_Left);
    Link(kBlockDiagramLinkCU_vL1D_Write, ImVec2(0.16f, -0.2375f), ImVec2(0.3f, -0.2375f), ImGuiDir_Right);
    Link(kBlockDiagramLinkCU_vL1D_Atomic, ImVec2(0.16f, -0.2025f), ImVec2(0.3f, -0.2025f));
    Link(kBlockDiagramLinkCU_LDS, ImVec2(0.16f, 0.2375f), ImVec2(0.3f, 0.2375f), ImGuiDir_Left);
}

void ComputeBlockDiagram::RenderCacheLevel()
{
    BlockButton(kBlockDiagramBlockGPU, ImVec2(-0.1f, 0), ImVec2(0.8f, 1), kBLockDiagramBlockOption_None);
    BlockButton(kBlockDiagramBlockCP, ImVec2(-0.35f, -0.275f), ImVec2(0.2f, 0.35f));
    BlockButton(kBlockDiagramBlockCPC, ImVec2(-0.35f, -0.35f), ImVec2(0.1f, 0.1f));
    BlockButton(kBlockDiagramBlockCPF, ImVec2(-0.35f, -0.20f), ImVec2(0.1f, 0.1f));
    if (BlockButton(kBlockDiagramBlockSE, ImVec2(-0.325f, 0.2f), ImVec2(0.25f, 0.5f), kBlockDiagramBlockOption_Navigation))
    {
        m_navigation->Go(kBlockDiagramlevelShaderEngine);
    }
    if (BlockButton(kBlockDiagramBlockCU, ImVec2(-0.325f, 0.05f), ImVec2(0.2f, 0.15f), kBlockDiagramBlockOption_Navigation))
    {
        m_navigation->Go(kBlockDiagramLevelComputeUnit);
    }
    BlockButton(kBlockDiagramBlockvL1D, ImVec2(-0.325f, 0.05f), ImVec2(0.15f, 0.1f));
    BlockButton(kBlockDiagramBlocksL1D, ImVec2(-0.325f, 0.2f), ImVec2(0.15f, 0.1f));
    BlockButton(kBlockDiagramBlockL1I, ImVec2(-0.325f, 0.35f), ImVec2(0.15f, 0.1f));
    BlockButton(kBlockDiagramBlockL2, ImVec2(-0.0375f, 0), ImVec2(0.125f, 0.9f));
    BlockButton(kBlockDiagramBlockFabric, ImVec2(0.19f, 0), ImVec2(0.125f, 0.55f));
    BlockButton(kBlockDiagramBlockHBM, ImVec2(0.425f, -0.2f), ImVec2(0.15f, 0.15f));
    BlockButton(kBlockDiagramBlockPCIe, ImVec2(0.425f, 0), ImVec2(0.15f, 0.15f));
    BlockButton(kBlockDiagramBlockDRAM, ImVec2(0.425f, 0.2f), ImVec2(0.15f, 0.15f));

    Link(kBlockDiagramLinkCPC_L2, ImVec2(-0.3f, -0.35f), ImVec2(-0.1f, -0.35f), ImGuiDir_None);
    Link(kBlockDiagramLinkCPF_L2, ImVec2(-0.3f, -0.2f), ImVec2(-0.1f, -0.2f), ImGuiDir_None);
    Link(kBlockDiagramLinkvL1D_L2_Read, ImVec2(-0.25f, 0.015f), ImVec2(-0.1f, 0.015f), ImGuiDir_Left);
    Link(kBlockDiagramLinkvL1D_L2_Write, ImVec2(-0.25f, 0.05f), ImVec2(-0.1f, 0.05f), ImGuiDir_Right);
    Link(kBlockDiagramLinkvL1D_L2_Atomic, ImVec2(-0.25f, 0.085f), ImVec2(-0.1f, 0.085f), ImGuiDir_None);
    Link(kBlockDiagramLinksL1D_L2_Read, ImVec2(-0.25f, 0.165f), ImVec2(-0.1f, 0.165f), ImGuiDir_Left);
    Link(kBlockDiagramLinksL1D_L2_Write, ImVec2(-0.25f, 0.2f), ImVec2(-0.1f, 0.2f), ImGuiDir_Right);
    Link(kBlockDiagramLinksL1D_L2_Atomic, ImVec2(-0.25f, 0.235f), ImVec2(-0.1f, 0.235f), ImGuiDir_None);
    Link(kBlockDiagramLinkL1I_L2, ImVec2(-0.25f, 0.35f), ImVec2(-0.1f, 0.35f), ImGuiDir_Left);
    Link(kBlockDiagramLinkL2_Fabric_Read, ImVec2(0.025, -0.035f), ImVec2(0.125f, -0.035f), ImGuiDir_Left);
    Link(kBlockDiagramLinkL2_Fabric_Write, ImVec2(0.025, 0), ImVec2(0.125f, 0), ImGuiDir_Right);
    Link(kBlockDiagramLinkL2_Fabric_Atomic, ImVec2(0.025, 0.035f), ImVec2(0.125f, 0.035f), ImGuiDir_None);
    Link(kBlockDiagramLinkFabric_DRAM_Read, ImVec2(0.25f, 0.175f), ImVec2(0.35f, 0.175f), ImGuiDir_Left);
    Link(kBlockDiagramLinkFabric_DRAM_Write, ImVec2(0.25f, 0.225f), ImVec2(0.35f, 0.225f), ImGuiDir_Right);
    Link(kBlockDiagramLinkFabric_PCIe_Read, ImVec2(0.25f, -0.0175f), ImVec2(0.35f, -0.0175f), ImGuiDir_Left);
    Link(kBlockDiagramLinkFabric_PCIe_Write, ImVec2(0.25f, 0.0175f), ImVec2(0.35f, 0.0175f), ImGuiDir_Right);
    Link(kBlockDiagramLinkFabric_HBM_Read, ImVec2(0.25f, -0.2175f), ImVec2(0.35f, -0.2175f), ImGuiDir_Left);
    Link(kBlockDiagramLinkFabric_HBM_Write, ImVec2(0.25f, -0.1825f), ImVec2(0.35f, -0.1825f), ImGuiDir_Right);
}

void ComputeBlockDiagram::Render()
{ 
    ImVec2 content_region = ImGui::GetContentRegionAvail();
    ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
    float  min_dim = std::min(content_region.x, content_region.y);
    m_content_region = ImVec2(min_dim * 0.9f, min_dim * 0.9f);
    m_content_region_center = ImVec2(cursor_pos.x + content_region.x / 2, cursor_pos.y + content_region.y / 2);
    m_draw_list = ImGui::GetWindowDrawList();

    switch(m_navigation->Current().m_level)
    {
        case kBlockDiagramLevelGPU: 
            RenderGPULevel(); 
            break;
        case kBlockDiagramlevelShaderEngine:
            RenderShaderEngineLevel();
            break;
        case kBlockDiagramLevelComputeUnit:
            RenderComputeUnitLevel();
            break;
        case kBlockDiagramLevelCache: 
            RenderCacheLevel();
            break;
    }
}

void ComputeBlockDiagram::Update()
{
    for (std::vector<std::unique_ptr<ComputeMetric>>& block : m_block_metrics)
    {
        for (std::unique_ptr<ComputeMetric>& metric : block)
        {
            metric->Update();
        }
    }
    for (std::unique_ptr<ComputeMetric>& metric : m_link_metrics)
    {
        metric->Update();
    }
}

ComputeBlockDiagramNavHelper* ComputeBlockDiagram::Navigation()
{
    ComputeBlockDiagramNavHelper* navigation = nullptr;
    if (m_navigation)
    {
        navigation = m_navigation.get();
    }
    return navigation;
}

ComputeBlockView::ComputeBlockView(std::shared_ptr<ComputeDataProvider> data_provider)
: m_block_diagram(nullptr)
, m_details_panel(nullptr)
, m_container(nullptr)
{
    m_block_diagram = std::make_shared<ComputeBlockDiagram>(data_provider);
    m_details_panel = std::make_shared<ComputeBlockDetails>(data_provider);

    LayoutItem left;
    left.m_item = m_block_diagram;
    LayoutItem right;
    right.m_item = m_details_panel;
    m_container = std::make_shared<HSplitContainer>(left, right);
    m_container->SetSplit(0.5f);
}

ComputeBlockView::~ComputeBlockView() {}

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

    ImGui::BeginDisabled(!m_block_diagram->Navigation()->BackAvailable());
    if(ImGui::ArrowButton("Compute_Block_View_Back", ImGuiDir_Left))
    {
        m_block_diagram->Navigation()->GoBack();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!m_block_diagram->Navigation()->ForwardAvailable());
    if(ImGui::ArrowButton("Compute_Block_View_Forward", ImGuiDir_Right))
    {
        m_block_diagram->Navigation()->GoForward();
    }
    ImGui::EndDisabled();
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

void ComputeBlockView::Update()
{
    if (m_block_diagram)
    {
        m_block_diagram->Update();
    }

    if (m_details_panel)
    {
        m_details_panel->Update();
    }
}

}  // namespace View
}  // namespace RocProfVis
