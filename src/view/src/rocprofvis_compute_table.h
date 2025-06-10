// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "rocprofvis_controller_enums.h"
#include "rocprofvis_event_manager.h"
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

class ComputeDataProvider2;
class ComputeTable;

typedef enum table_view_category_t
{
    kTableCategorySystemSOL = 0,
    kTableCategoryCP = 1,
    kTableCategorySPI = 2,
    kTableCategoryWavefrontStats = 3,
    kTableCategoryInstructionMix = 4,
    kTableCategoryCU = 5,
    kTableCategoryLDS = 6,
    kTableCategoryL1I = 7,
    kTableCategorySL1D = 8,
    kTableCategoryAddressProcessingUnit = 9,
    kTableCategoryVL1D = 10,
    kTableCategoryL2 = 11,
    kTableCategoryL2PerChannel = 12,
    kTableCategoryCount
} table_view_category_t;

typedef struct table_view_category_info_t
{
    table_view_category_t m_category;
    std::string m_name;
    std::string m_id;
    std::vector<rocprofvis_controller_compute_table_types_t> m_table_types;
} table_view_category_info_t;

class ComputeTableCategory : public RocWidget
{
public:
    void Render() override;
    void Update() override;
    ComputeTableCategory(std::shared_ptr<ComputeDataProvider2> data_provider, table_view_category_t category);

private:
    void OnSearchChanged(std::shared_ptr<RocEvent> event);

    std::vector<std::unique_ptr<ComputeTable>> m_tables;
    EventManager::SubscriptionToken m_search_event_token;
};

class ComputeTableView : public RocWidget
{
public:
    void Render() override;
    void Update() override;
    ComputeTableView(std::string owner_id, std::shared_ptr<ComputeDataProvider2> data_provider);
    ~ComputeTableView();

private:
    void RenderMenuBar();

    bool m_search_edited;
    char m_search_term[32];
    std::shared_ptr<TabContainer> m_tab_container;
    std::string m_owner_id;
};

}  // namespace View
}  // namespace RocProfVis
