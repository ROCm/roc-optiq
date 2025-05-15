// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "rocprofvis_compute_block.h"
#include "rocprofvis_compute_data_provider.h"
#include "rocprofvis_compute_roofline.h"
#include "rocprofvis_compute_summary.h"
#include "rocprofvis_compute_table.h"
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

class ComputeRoot : public RocWidget
{
public:
    void Render() override;
    void Update() override;
    void SetProfilePath(std::filesystem::path path);
    bool ProfileLoaded();
    ComputeRoot();
    ~ComputeRoot();

private:
    std::shared_ptr<ComputeSummaryView> m_compute_summary_view;
    std::shared_ptr<ComputeRooflineView> m_compute_roofline_view;
    std::shared_ptr<ComputeBlockView> m_compute_block_view;
    std::shared_ptr<ComputeTableView> m_compute_table_view;
    std::shared_ptr<ComputeDataProvider> m_compute_data_provider;
};

}  // namespace View
}  // namespace RocProfVis
