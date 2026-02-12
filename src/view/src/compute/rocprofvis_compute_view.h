// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_data_provider.h"
#include "rocprofvis_root_view.h"

namespace RocProfVis
{
namespace View
{

class SettingsManager;
class ComputeTester;

class ComputeView : public RootView
{
public:
    ComputeView();
    ~ComputeView();

    void Update() override;
    void Render() override;

    bool LoadTrace(rocprofvis_controller_t* controller, const std::string& file_path);

    void CreateView();
    void DestroyView();

    std::shared_ptr<RocWidget> GetToolbar() override;
    void                       RenderEditMenuOptions() override;

private:
    void RenderToolbar();

    bool m_view_created;

    DataProvider                     m_data_provider;
    std::shared_ptr<RocCustomWidget> m_tool_bar;
    std::unique_ptr<ComputeTester>   m_tester;
};

class ComputeTester : public RocWidget
{
public:
    ComputeTester(DataProvider& data_provider);
    ~ComputeTester();

    void Update();
    void Render();

private:
    void RenderKernelSelectionTable();

    struct SelectionState
    {
        enum RooflinePreset
        {
            FP32,
            FP64,
            Custom
        };
        bool                         init;
        uint32_t                     workload_id;
        std::unordered_set<uint32_t> kernel_ids;
        std::unordered_map<
            uint32_t,
            std::unordered_map<uint32_t, std::pair<bool, std::unordered_set<uint32_t>>>>
                       metric_ids;
        RooflinePreset roofline_preset;
        std::unordered_map<
            rocprofvis_controller_roofline_ceiling_compute_type_t,
            std::unordered_set<rocprofvis_controller_roofline_ceiling_bandwidth_type_t>>
            ceilings_compute;
        std::unordered_map<
            rocprofvis_controller_roofline_ceiling_bandwidth_type_t,
            std::unordered_set<rocprofvis_controller_roofline_ceiling_compute_type_t>>
            ceilings_bandwidth;
        std::unordered_map<
            uint32_t,
            std::unordered_set<rocprofvis_controller_roofline_kernel_intensity_type_t>>
            intensities;
    };
    struct DisplayStrings
    {
        std::unordered_map<rocprofvis_controller_roofline_ceiling_compute_type_t,
                           const char*>
            ceiling_compute;
        std::unordered_map<rocprofvis_controller_roofline_ceiling_bandwidth_type_t,
                           const char*>
            ceiling_bandwidth;
        std::unordered_map<rocprofvis_controller_roofline_kernel_intensity_type_t,
                           const char*>
            intensity;
    };

    DataProvider&  m_data_provider;
    SelectionState m_selections;
    DisplayStrings m_display_names;
};

}  // namespace View
}  // namespace RocProfVis
