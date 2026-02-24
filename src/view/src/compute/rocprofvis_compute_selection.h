// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include <cstdint>
#include <limits>

namespace RocProfVis
{
namespace View
{

class DataProvider;

class ComputeSelection
{
public:
    ComputeSelection(DataProvider& dp);
    ~ComputeSelection() = default;

    void SelectKernel(uint32_t kernel_id);
    void SelectWorkload(uint32_t workload_id);

    uint32_t GetSelectedWorkload() const;
    uint32_t GetSelectedKernel() const;

    static constexpr uint32_t INVALID_SELECTION_ID = std::numeric_limits<uint32_t>::max();

private:
    uint32_t m_selected_workload_id;
    uint32_t m_selected_kernel_id;

    DataProvider& m_data_provider;

    void SendWorkloadSelectionChanged();
    void SendKernelSelectionChanged();
};

}  // namespace View
}  // namespace RocProfVis
