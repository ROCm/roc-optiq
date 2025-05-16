// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "rocprofvis_compute_data_provider.h"
#include "rocprofvis_compute_metric.h"
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

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
    std::vector<std::string> m_content_ids;
} table_view_category_info_t;

class ComputeTableCategory : public RocWidget
{
public:
    void Render();
    void Update();
    ComputeTableCategory(std::shared_ptr<ComputeDataProvider> data_provider, table_view_category_t category);
    ~ComputeTableCategory();

private:
    table_view_category_t m_category;
    std::vector<std::unique_ptr<ComputeMetricGroup>> m_metrics;
};

class ComputeTableView : public RocWidget
{
public:
    void Render();
    void Update();
    ComputeTableView(std::shared_ptr<ComputeDataProvider> data_provider);
    ~ComputeTableView();

private:
    void RenderMenuBar();

    std::shared_ptr<TabContainer> m_tab_container;
};

}  // namespace View
}  // namespace RocProfVis
