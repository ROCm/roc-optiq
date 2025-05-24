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
    std::shared_ptr<TabContainer> m_tab_container;
    std::shared_ptr<ComputeDataProvider> m_compute_data_provider;
};

}  // namespace View
}  // namespace RocProfVis
