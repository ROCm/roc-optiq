// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_compute_table.h"
#include "rocprofvis_navigation_manager.h"
#include "rocprofvis_navigation_url.h"
#include "widgets/rocprofvis_compute_widget.h"
#include <array>

namespace RocProfVis
{
namespace View
{

const std::array TAB_DEFINITIONS {
    table_view_category_info_t{kTableCategorySystemSOL, "System Speed of Light", COMPUTE_TABLE_SPEED_OF_LIGHT_URL, {
        kRPVControllerComputeTableTypeSpeedOfLight
    }},
    table_view_category_info_t{kTableCategoryCP, "Command Processor", COMPUTE_TABLE_COMMAND_PROCESSOR_URL, {
        kRPVControllerComputeTableTypeCPFetcher, 
        kRPVControllerComputeTableTypeCPPacketProcessor
    }},
    table_view_category_info_t{kTableCategorySPI, "Workgroup Manager", COMPUTE_TABLE_WORKGROUP_MANAGER_URL, {
        kRPVControllerComputeTableTypeWorkgroupMngrUtil, 
        kRPVControllerComputeTableTypeWorkgroupMngrRescAlloc
    }},
    table_view_category_info_t{kTableCategoryWavefrontStats, "Wavefront", COMPUTE_TABLE_WAVEFRONT_URL, {
        kRPVControllerComputeTableTypeWavefrontLaunch, 
        kRPVControllerComputeTableTypeWavefrontRuntime
    }},
    table_view_category_info_t{kTableCategoryInstructionMix, "Instruction Mix", COMPUTE_TABLE_INSTRUCTION_MIX_URL, {
        kRPVControllerComputeTableTypeInstrMix, 
        kRPVControllerComputeTableTypeVALUInstrMix, 
        kRPVControllerComputeTableTypeVMEMInstrMix, 
        kRPVControllerComputeTableTypeMFMAInstrMix
    }},
    table_view_category_info_t{kTableCategoryCU, "Compute Pipeline", COMPUTE_TABLE_COMPUTE_PIPELINE_URL, {
        kRPVControllerComputeTableTypeCUSpeedOfLight, 
        kRPVControllerComputeTableTypeCUPipelineStats, 
        kRPVControllerComputeTableTypeCUOps
    }},
    table_view_category_info_t{kTableCategoryLDS, "Local Data Store", COMPUTE_TABLE_LOCAL_DATA_STORE_URL, {
        kRPVControllerComputeTableTypeLDSSpeedOfLight, 
        kRPVControllerComputeTableTypeLDSStats
    }},
    table_view_category_info_t{kTableCategoryL1I, "L1 Instruction Cache", COMPUTE_TABLE_INSTRUCTION_CACHE_URL, {
        kRPVControllerComputeTableTypeInstrCacheSpeedOfLight, 
        kRPVControllerComputeTableTypeInstrCacheAccesses, 
        kRPVControllerComputeTableTypeInstrCacheL2Interface
    }},
    table_view_category_info_t{kTableCategorySL1D, "Scalar L1 Data Cache", COMPUTE_TABLE_SCALAR_CACHE_URL, {
        kRPVControllerComputeTableTypeSL1CacheSpeedOfLight, 
        kRPVControllerComputeTableTypeSL1CacheAccesses, 
        kRPVControllerComputeTableTypeSL1CacheL2Transactions
    }},
    table_view_category_info_t{kTableCategoryAddressProcessingUnit, "Address Processing Unit & Data Return Path", COMPUTE_TABLE_ADDRESS_PROCESSING_UNIT_URL, {
        kRPVControllerComputeTableTypeAddrProcessorStats, 
        kRPVControllerComputeTableTypeAddrProcessorReturnPath
    }},
    table_view_category_info_t{kTableCategoryVL1D, "Vector L1 Data Cache", COMPUTE_TABLE_VECTOR_CACHE_URL, {
        kRPVControllerComputeTableTypeVL1CacheSpeedOfLight, 
        kRPVControllerComputeTableTypeVL1CacheStalls, 
        kRPVControllerComputeTableTypeVL1CacheAccesses, 
        kRPVControllerComputeTableTypeVL1CacheL2Transactions, 
        kRPVControllerComputeTableTypeVL1CacheAddrTranslations
    }},
    table_view_category_info_t{kTableCategoryL2, "L2 Cache", COMPUTE_TABLE_L2_CACHE_URL, {
        kRPVControllerComputeTableTypeL2CacheSpeedOfLight, 
        kRPVControllerComputeTableTypeL2CacheFabricTransactions, 
        kRPVControllerComputeTableTypeL2CacheAccesses, 
        kRPVControllerComputeTableTypeL2CacheFabricStalls, 
        kRPVControllerComputeTableTypeL2CacheFabricTransactionsDetailed
    }},
    table_view_category_info_t{kTableCategoryL2PerChannel, "L2 Cache (Per Channel)", COMPUTE_TABLE_L2_CACHE_PER_CHANNEL_URL, {
        kRPVControllerComputeTableTypeL2CacheStats, 
        kRPVControllerComputeTableTypeL2CacheHitRate, 
        kRPVControllerComputeTableTypeL2CacheReqs1, 
        kRPVControllerComputeTableTypeL2CacheReqs2,
        kRPVControllerComputeTableTypeL2CacheFabricReqs, 
        kRPVControllerComputeTableTypeL2CacheFabricRdLat, 
        kRPVControllerComputeTableTypeL2CacheFabricWrAtomLat, 
        kRPVControllerComputeTableTypeL2CacheFabricAtomLat,
        kRPVControllerComputeTableTypeL2CacheRdStalls, 
        kRPVControllerComputeTableTypeL2CacheWrAtomStalls, 
        kRPVControllerComputeTableTypeL2Cache128Reqs
    }}
};

ComputeTableCategory::ComputeTableCategory(std::shared_ptr<ComputeDataProvider> data_provider, table_view_category_t category)
: m_search_event_token(-1)
{
    for (const rocprofvis_controller_compute_table_types_t& table : TAB_DEFINITIONS[category].m_table_types)
    {
        m_tables.push_back(std::make_unique<ComputeTable>(data_provider, table));
    }

    auto search_event_handler = [this](std::shared_ptr<RocEvent> event) 
    {
        this->OnSearchChanged(event);
    };
    m_search_event_token = EventManager::GetInstance()->Subscribe(static_cast<int>(RocEvents::kComputeTableSearchChanged), search_event_handler);
}

void ComputeTableCategory::OnSearchChanged(std::shared_ptr<RocEvent> event)
{
    std::shared_ptr<ComputeTableSearchEvent> navigation_event = std::dynamic_pointer_cast<ComputeTableSearchEvent>(event);
    for (std::unique_ptr<ComputeTable>& table : m_tables)
    {
        table->Search(navigation_event->GetSearchTerm());
    }
}

void ComputeTableCategory::Render()
{
    ImGui::BeginChild("", ImVec2(-1, -1));
    for (std::unique_ptr<ComputeTable>& table : m_tables)
    {
        table->Render();
    }
    ImGui::EndChild();
}

void ComputeTableCategory::Update()
{
    for (std::unique_ptr<ComputeTable>& table : m_tables)
    {
        table->Update();
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
        m_tab_container->AddTab(TabItem{category.m_name, category.m_id, std::make_shared<ComputeTableCategory>(data_provider, category.m_category), false});
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
    ImGui::InputTextWithHint("##compute_table_search_bar", "Metric Name", m_search_term, IM_ARRAYSIZE(m_search_term), 
                             ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CallbackEdit, 
                             [](ImGuiInputTextCallbackData* data) -> int 
                             {
                                 //ImGuiInputTextCallback on edit.
                                 std::string data_str(data->Buf);
                                 EventManager::GetInstance()->AddEvent(std::make_shared<ComputeTableSearchEvent>(static_cast<int>(RocEvents::kComputeTableSearchChanged), data_str));
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
