// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "rocprofvis_compute_data_provider2.h"
#include "rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

class ComputeTable : public RocWidget
{
public:
    ComputeTable(std::shared_ptr<ComputeDataProvider2> data_provider, rocprofvis_controller_compute_table_types_t type);
    void Update() override;
    void Render() override;
    void Search(const std::string& term);

private:
    std::shared_ptr<ComputeDataProvider2> m_data_provider;
    rocprofvis_controller_compute_table_types_t m_type;
    ComputeTableModel* m_model;
    std::string m_id;
};

}  // namespace View
}  // namespace RocProfVis
