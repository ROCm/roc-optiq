// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_compute_table.h"
#include "rocprofvis_navigation_manager.h"
#include "rocprofvis_navigation_url.h"

namespace RocProfVis
{
namespace View
{

const std::array TAB_DEFINITIONS = {
    table_view_category_info_t{kTableCategorySystemSOL, "System Speed of Light", {
        "2.1_Speed-of-Light.csv"
    }},
    table_view_category_info_t{kTableCategoryCP, "Command Processor", {
        "5.1_Command_Processor_Fetcher.csv", "5.2_Packet_Processor.csv"
    }},
    table_view_category_info_t{kTableCategorySPI, "Workgroup Manager", {
        "6.1_Workgroup_Manager_Utilizations.csv", "6.2_Workgroup_Manager_-_Resource_Allocation.csv"
    }},
    table_view_category_info_t{kTableCategoryWavefrontStats, "Wavefront", {
        "7.1_Wavefront_Launch_Stats.csv", "7.2_Wavefront_Runtime_Stats.csv"
    }},
    table_view_category_info_t{kTableCategoryInstructionMix, "Instruction Mix", {
        "10.1_Overall_Instruction_Mix.csv", "10.2_VALU_Arithmetic_Instr_Mix.csv", "10.3_VMEM_Instr_Mix.csv", "10.4_MFMA_Arithmetic_Instr_Mix.csv"
    }},
    table_view_category_info_t{kTableCategoryCU, "Compute Pipeline", {
        "11.1_Speed-of-Light.csv", "11.2_Pipeline_Stats.csv", "11.3_Arithmetic_Operations.csv"
    }},
    table_view_category_info_t{kTableCategoryLDS, "Local Data Store", {
        "12.1_Speed-of-Light.csv", "12.2_LDS_Stats.csv"
    }},
    table_view_category_info_t{kTableCategoryL1I, "L1 Instruction Cache", {
        "13.1_Speed-of-Light.csv", "13.2_Instruction_Cache_Accesses.csv", "13.3_Instruction_Cache_-_L2_Interface.csv"
    }},
    table_view_category_info_t{kTableCategorySL1D, "Scalar L1 Data Cache", {
        "14.1_Speed-of-Light.csv", "14.2_Scalar_L1D_Cache_Accesses.csv", "14.3_Scalar_L1D_Cache_-_L2_Interface.csv"
    }},
    table_view_category_info_t{kTableCategoryAddressProcessingUnit, "Address Processing Unit & Data Return Path", {
        "15.1_Address_Processing_Unit.csv", "15.2_Data-Return_Path.csv"
    }},
    table_view_category_info_t{kTableCategoryVL1D, "Vector L1 Data Cache", {
        "16.1_Speed-of-Light.csv", "16.2_L1D_Cache_Stalls_(%).csv", "16.3_L1D_Cache_Accesses.csv", "16.4_L1D_-_L2_Transactions.csv", "16.5_L1D_Addr_Translation.csv",
    }},
    table_view_category_info_t{kTableCategoryL2, "L2 Cache", {
        "17.1_Speed-of-Light.csv", "17.2_L2_-_Fabric_Transactions.csv", "17.3_L2_Cache_Accesses.csv", "17.4_L2_-_Fabric_Interface_Stalls.csv", "17.5_L2_-_Fabric_Detailed_Transaction_Breakdown.csv"
    }},
    table_view_category_info_t{kTableCategoryL2PerChannel, "L2 Cache (Per Channel)", {
        "18.1_Aggregate_Stats_(All_channels).csv", "18.2_L2_Cache_Hit_Rate_(pct)", "18.3_L2_Requests_(per_normUnit).csv", "18.4_L2_Requests_(per_normUnit).csv", 
        "18.5_L2-Fabric_Requests_(per_normUnit).csv", "18.6_L2-Fabric_Read_Latency_(Cycles).csv", "18.7_L2-Fabric_Write_and_Atomic_Latency_(Cycles).csv", "18.8_L2-Fabric_Atomic_Latency_(Cycles).csv", 
        "18.9_L2-Fabric_Read_Stall_(Cycles_per_normUnit).csv", "18.10_L2-Fabric_Write_and_Atomic_Stall_(Cycles_per_normUnit).csv", "18.12_L2-Fabric_(128B_read_requests_per_normUnit).csv"
    }},
};

ComputeTableCategory::ComputeTableCategory(std::shared_ptr<ComputeDataProvider> data_provider, table_view_category_t category)
: m_search_event_token(-1)
{
    for (const std::string& content : TAB_DEFINITIONS[category].m_content_ids)
    {
        m_metrics.push_back(std::make_unique<ComputeMetricGroup>(data_provider, content));
    }

    auto search_event_handler = [this](std::shared_ptr<RocEvent> event) 
    {
        this->OnSearchChanged(event);
    };
    m_search_event_token = EventManager::GetInstance()->Subscribe(static_cast<int>(RocEvents::kComputeTableSearchChanged), search_event_handler);
}

ComputeTableCategory::~ComputeTableCategory() {}

void ComputeTableCategory::OnSearchChanged(std::shared_ptr<RocEvent> event)
{
    std::shared_ptr<ComputeTableSearchEvent> navigation_event = std::dynamic_pointer_cast<ComputeTableSearchEvent>(event);
    for (std::unique_ptr<ComputeMetricGroup>& metric : m_metrics)
    {
        metric->Search(navigation_event->GetSearchTerm());
    }
}

void ComputeTableCategory::Render()
{
    ImGui::BeginChild("", ImVec2(-1, -1));
    for (std::unique_ptr<ComputeMetricGroup>& metric : m_metrics)
    {
        metric->Render();
    }
    ImGui::EndChild();
}

void ComputeTableCategory::Update()
{
    for (std::unique_ptr<ComputeMetricGroup>& metric : m_metrics)
    {
        metric->Update();
    }
}

ComputeTableView::ComputeTableView(std::string owner_id, std::shared_ptr<ComputeDataProvider> data_provider) 
: m_tab_container(nullptr)
, m_search_term("")
, m_search_edited(false)
, m_owner_id(owner_id)
{
    m_tab_container = std::make_shared<TabContainer>();
    for (const table_view_category_info_t& category : TAB_DEFINITIONS)
    {
        m_tab_container->AddTab(TabItem{category.m_name, "compute_table_tab_" + category.m_name, std::make_shared<ComputeTableCategory>(data_provider, category.m_category), false});
    }

    NavigationManager::GetInstance()->RegisterContainer(m_tab_container, COMPUTE_TABLE_VIEW_URL, m_owner_id);
}

ComputeTableView::~ComputeTableView() {
    NavigationManager::GetInstance()->UnregisterContainer(m_tab_container, COMPUTE_TABLE_VIEW_URL, m_owner_id);
}

void ComputeTableView::Update()
{
    if (m_tab_container)
    {
        m_tab_container->Update();
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
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Search");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(350 - ImGui::CalcTextSize("Search").x);
    ImGui::InputTextWithHint("##compute_table_search_bar", "Metric Name", m_search_term, ARRAYSIZE(m_search_term), 
                             ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CallbackEdit, 
                             [](ImGuiInputTextCallbackData* data) -> int 
                             {
                                 //ImGuiInputTextCallback on edit.
                                 EventManager::GetInstance()->AddEvent(std::make_shared<ComputeTableSearchEvent>(static_cast<int>(RocEvents::kComputeTableSearchChanged), std::string(data->Buf)));
                                 return 0;
                             });
}

void ComputeTableView::Render()
{
    RenderMenuBar();

    ImGui::BeginChild("compute_table_tab_bar", ImVec2(-1, -1), ImGuiChildFlags_Borders);
    m_tab_container->Render();
    ImGui::EndChild();
}

}  // namespace View
}  // namespace RocProfVis
