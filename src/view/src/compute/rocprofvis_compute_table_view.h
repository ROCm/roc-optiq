// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_data_provider.h"
#include "rocprofvis_root_view.h"
#include "widgets/rocprofvis_tab_container.h"

namespace RocProfVis
{
namespace View
{

class ComputeTableView: public RocWidget
{
public:
    ComputeTableView(DataProvider& data_provider);
    ~ComputeTableView(){};

    void Update() override;
    void Render() override;

private:
    void RenderCategory(const AvailableMetrics::Category& cat);

    DataProvider&                         m_data_provider;
    uint32_t                              m_workload_id = 0;
    uint32_t                              m_last_workload_id = 0;
    std::unordered_set<uint32_t>          m_kernel_ids;
    std::shared_ptr<TabContainer>         m_tabs;
};

}  // namespace View
}  // namespace RocProfVis
