// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_compute_table.h"

namespace RocProfVis
{
namespace View
{

constexpr char* CATEGORY_LABELS[kTableCategoryCount] = {
    "System Speed of Light", 
    "Command Processor", 
    "Workgroup Manager", 
    "Wavefront", 
    "Instruction Mix", 
    "Compute Pipeline", 
    "Local Data Store", 
    "L1 Instruction Cache",
    "Scaler L1 Data Cache",
    "Address Processing Unit & Data Return Path",
    "Vector L1 Data Cache",
    "L2 Cache",
    "L2 Cache (Per Channel)"
};
constexpr int CSV_COUNT = 45;
constexpr std::array<std::pair<table_view_category_t, char*>, CSV_COUNT> CATEGORY_DEFINITIONS = {{
    {kTableCategorySystemSOL, "2.1_Speed-of-Light.csv"}, 
    {kTableCategoryCP, "5.1_Command_Processor_Fetcher.csv"},
    {kTableCategoryCP, "5.2_Packet_Processor.csv"},
    {kTableCategorySPI, "6.1_Workgroup_Manager_Utilizations.csv"},
    {kTableCategorySPI, "6.2_Workgroup_Manager_-_Resource_Allocation.csv"},
    {kTableCategoryWavefrontStats, "7.1_Wavefront_Launch_Stats.csv"},
    {kTableCategoryWavefrontStats, "7.2_Wavefront_Runtime_Stats.csv"},
    {kTableCategoryInstructionMix, "10.1_Overall_Instruction_Mix.csv"},
    {kTableCategoryInstructionMix, "10.2_VALU_Arithmetic_Instr_Mix.csv"},
    {kTableCategoryInstructionMix, "10.3_VMEM_Instr_Mix.csv"},
    {kTableCategoryInstructionMix, "10.4_MFMA_Arithmetic_Instr_Mix.csv"},
    {kTableCategoryCU, "11.1_Speed-of-Light.csv"},
    {kTableCategoryCU, "11.2_Pipeline_Stats.csv"},
    {kTableCategoryCU, "11.3_Arithmetic_Operations.csv"},
    {kTableCategoryLDS, "12.1_Speed-of-Light.csv"},
    {kTableCategoryLDS, "12.2_LDS_Stats.csv"},
    {kTableCategoryL1I, "13.1_Speed-of-Light.csv"},
    {kTableCategoryL1I, "13.2_Instruction_Cache_Accesses.csv"},
    {kTableCategoryL1I, "13.3_Instruction_Cache_-_L2_Interface.csv"},
    {kTableCategorySL1D, "14.1_Speed-of-Light.csv"},
    {kTableCategorySL1D, "14.2_Scalar_L1D_Cache_Accesses.csv"},
    {kTableCategorySL1D, "14.3_Scalar_L1D_Cache_-_L2_Interface.csv"},
    {kTableCategoryAddressProcessingUnit, "15.1_Address_Processing_Unit.csv"},
    {kTableCategoryAddressProcessingUnit, "15.2_Data-Return_Path.csv"},
    {kTableCategoryVL1D, "16.1_Speed-of-Light.csv"},
    {kTableCategoryVL1D, "16.2_L1D_Cache_Stalls_(%).csv"},
    {kTableCategoryVL1D, "16.3_L1D_Cache_Accesses.csv"},
    {kTableCategoryVL1D, "16.4_L1D_-_L2_Transactions.csv"},
    {kTableCategoryVL1D, "16.5_L1D_Addr_Translation.csv"},
    {kTableCategoryL2, "17.1_Speed-of-Light.csv"},
    {kTableCategoryL2, "17.2_L2_-_Fabric_Transactions.csv"},
    {kTableCategoryL2, "17.3_L2_Cache_Accesses.csv"},
    {kTableCategoryL2, "17.4_L2_-_Fabric_Interface_Stalls.csv"},
    {kTableCategoryL2, "17.5_L2_-_Fabric_Detailed_Transaction_Breakdown.csv"},
    {kTableCategoryL2PerChannel, "18.1_Aggregate_Stats_(All_channels).csv"},
    {kTableCategoryL2PerChannel, "18.2_L2_Cache_Hit_Rate_(pct)"},
    {kTableCategoryL2PerChannel, "18.3_L2_Requests_(per_normUnit).csv"},
    {kTableCategoryL2PerChannel, "18.4_L2_Requests_(per_normUnit).csv"},
    {kTableCategoryL2PerChannel, "18.5_L2-Fabric_Requests_(per_normUnit).csv"},
    {kTableCategoryL2PerChannel, "18.6_L2-Fabric_Read_Latency_(Cycles).csv"},
    {kTableCategoryL2PerChannel, "18.7_L2-Fabric_Write_and_Atomic_Latency_(Cycles).csv"},
    {kTableCategoryL2PerChannel, "18.8_L2-Fabric_Atomic_Latency_(Cycles).csv"},
    {kTableCategoryL2PerChannel, "18.9_L2-Fabric_Read_Stall_(Cycles_per_normUnit).csv"},
    {kTableCategoryL2PerChannel, "18.10_L2-Fabric_Write_and_Atomic_Stall_(Cycles_per_normUnit).csv"},
    {kTableCategoryL2PerChannel, "18.12_L2-Fabric_(128B_read_requests_per_normUnit).csv"},
}};

ComputeTableView::ComputeTableView(std::shared_ptr<ComputeDataProvider> data_provider) {
    for (int i = 0; i < CATEGORY_DEFINITIONS.size(); i ++)
    {
        std::unique_ptr<ComputeMetric> metric = std::make_unique<ComputeMetric>(data_provider, CATEGORY_DEFINITIONS[i].second);
        m_metrics_map[CATEGORY_DEFINITIONS[i].first].push_back(std::move(metric));        
    }
}

ComputeTableView::~ComputeTableView() {}

void ComputeTableView::Update()
{
    for (auto it = m_metrics_map.begin(); it != m_metrics_map.end(); it ++)
    {
        for (std::unique_ptr<ComputeMetric>& metric : it->second)
        {
            metric->Update();
        }
    }
}

void ComputeTableView::RenderMenuBar()
{
    ImVec2 cursor_position = ImGui::GetCursorScreenPos();
    ImVec2 content_region = ImGui::GetContentRegionAvail();
    ImGui::GetWindowDrawList()->AddRectFilled(
    ImVec2(cursor_position.x, cursor_position.y),
    ImVec2(cursor_position.x + content_region.x,
            cursor_position.y + ImGui::GetFrameHeightWithSpacing()),
    ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg]), 
    0.0f);

    ImGui::SetCursorScreenPos(ImVec2(cursor_position.x + ImGui::GetContentRegionAvail().x - 350, cursor_position.y));
    ImGui::Text("Search");
    char searchTerm[32] = "";
    ImGui::SameLine();
    ImGui::SetNextItemWidth(350 - ImGui::CalcTextSize("Search").x);
    ImGui::InputText("##compute_table_search_bar", searchTerm, 32);
}

void ComputeTableView::RenderMetricsTab(table_view_category_t category)
{
    if (ImGui::BeginTabItem(CATEGORY_LABELS[category]))
    {
        ImGui::BeginChild("compute_table_category", ImVec2(-1, -1), ImGuiChildFlags_Borders);
        for (std::unique_ptr<ComputeMetric>& metric : m_metrics_map[category])
        {
            metric->Render();
        }
        ImGui::EndChild();
        ImGui::EndTabItem();      
    }
}

void ComputeTableView::Render()
{
    RenderMenuBar();
    if (ImGui::BeginTabBar("compute_table_tab_bar", ImGuiTabBarFlags_None))
    {
        for (int i = 0; i < static_cast<int>(kTableCategoryCount); i ++)
        {
            RenderMetricsTab(static_cast<table_view_category_t>(i));
        }
        ImGui::EndTabBar();
    }
}

}  // namespace View
}  // namespace RocProfVis
