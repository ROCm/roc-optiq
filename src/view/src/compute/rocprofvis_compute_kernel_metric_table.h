// Copyright (c) 2024 Advanced Micro Devices, Inc. All rights reserved.
// SPDX-License-Identifier: MIT

#pragma once
#include "widgets/rocprofvis_widget.h"
#include "widgets/rocprofvis_query_builder.h"
#include <memory>

namespace RocProfVis
{
namespace View
{

class DataProvider;
class ComputeSelection;

class KernelMetricTable : public RocWidget
{
public:
    KernelMetricTable(DataProvider&                     data_provider,
                      std::shared_ptr<ComputeSelection> compute_selection);
    void Update() override;
    void Render() override;

    void ClearData();
    void FetchData(uint32_t workload_id);
    void HandleNewData();

private:

    void RenderLoadingIndicator() const;

    struct MetricInfo
    {
        AvailableMetrics::Entry entry;
        std::string             value_name;
    };
    std::vector<MetricInfo>  m_metrics_info;
    std::vector<std::string> m_metrics_params;

    std::vector<std::string> m_metrics_column_names;
    std::vector<std::string> m_permanent_column_names;

    DataProvider& m_data_provider;
    QueryBuilder  m_query_builder;

    int  m_sort_column_index;
    int  m_sort_order;
    bool m_sort_specs_initialized;

    int  m_selected_row;

    bool m_fetch_requested;
    uint32_t m_workload_id;

    // used for selecting kernel
    std::shared_ptr<ComputeSelection> m_compute_selection;
    uint32_t                          m_selected_kernel_id_local;
    bool                              m_update_table_selection;
    bool                              m_allow_deselect;
};

}  // namespace View
}  // namespace RocProfVis