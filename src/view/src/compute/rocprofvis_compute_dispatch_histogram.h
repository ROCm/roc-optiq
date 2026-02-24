// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include <cstdint>
#include <memory>
#include <vector>

namespace RocProfVis
{
namespace View
{

class DataProvider;
class ComputeSelection;
struct DispatchInfo;

class DispatchHistogramView
{
public:
    DispatchHistogramView(DataProvider& data_provider,
                          std::shared_ptr<ComputeSelection> compute_selection);
    ~DispatchHistogramView();

    void Render();

private:
    void RenderToolbar();
    void RenderBars();

    void RefreshGpuIds();

    DataProvider&                     m_data_provider;
    std::shared_ptr<ComputeSelection> m_compute_selection;

    uint32_t              m_selected_gpu_id;
    std::vector<uint32_t> m_gpu_ids;
    bool                  m_gpu_ids_dirty;
};

}  // namespace View
}  // namespace RocProfVis
