// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "compute/rocprofvis_compute_model_types.h"
#include "rocprofvis_controller_enums.h"
#include "widgets/rocprofvis_widget.h"
#include <optional>
#include <vector>

struct ImPlotStyle;

namespace RocProfVis
{
namespace View
{

class DataProvider;
class SettingsManager;

class Roofline : public RocWidget
{
public:
    enum KernelMode
    {
        SingleKernel,
        AllKernels,
    };

    Roofline(DataProvider& data_provider, KernelMode kernel_mode);

    void Update() override;
    void Render() override;

    void SetWorkload(uint32_t id);
    void SetKernel(uint32_t id);

private:
    enum MenusMode
    {
        Legend,
        Options
    };
    enum MenusPlacement
    {
        InsideTopLeft,
        InsideTopRight,
        InsideBottomLeft,
        InsideBottomRight,
        Outside
    };
    struct ItemModel
    {
        enum Type
        {
            CeilingCompute,
            CeilingBandwidth,
            Intensity,
        };
        union SubType
        {
            rocprofvis_controller_roofline_ceiling_compute_type_t   compute;
            rocprofvis_controller_roofline_ceiling_bandwidth_type_t bandwidth;
            rocprofvis_controller_roofline_kernel_intensity_type_t  intensity;
        };
        union Info
        {
            const WorkloadInfo::Roofline::Ceiling* ceiling;
            const KernelInfo::Roofline::Intensity* intensity;
        };
        union ParentInfo
        {
            const WorkloadInfo* workload;
            const KernelInfo*   kernel;
        };
        Type        type;
        SubType     subtype;
        Info        info;
        ParentInfo  parent_info;
        bool        visible;
        std::string label;
        float       weight;
    };
    struct PresetModel
    {
        enum Type
        {
            FP4,
            FP6,
            FP8,
            FP16,
            FP32,
            FP64,
        };
        Type                type;
        std::vector<size_t> item_indices;
    };

    void RenderMenus(ImVec2 region, ImVec2 plot_pos, ImVec2 plot_size,
                     const ImGuiStyle& style, const ImPlotStyle& plot_style,
                     bool& item_hovered);
    void PlotHoverIdx();
    void ApplyPreset(PresetModel::Type type);
    // Kernel intensity dots exist per memory level; this decides whether a
    // given item survives the active memory-peak filter (non-intensity items
    // always pass).
    bool IntensityMatchesMemoryFilter(const ItemModel& item) const;
    // Isolate a single kernel's dots in the workload roofline. Clicking the
    // same kernel again clears isolation and restores the whole workload.
    void ToggleKernelIsolation(const KernelInfo* kernel);
    // Whether an item survives the active kernel isolation (non-intensity
    // items, and everything outside AllKernels mode, always pass).
    bool KernelPassesIsolation(const ItemModel& item) const;
    void ToggleBandwidthIsolation(
        rocprofvis_controller_roofline_ceiling_bandwidth_type_t bandwidth);
    bool BandwidthPassesIsolation(const ItemModel& item) const;

    // Internal models...
    std::vector<ItemModel>   m_items;
    std::vector<PresetModel> m_presets;

    // User options...
    bool           m_show_menus;
    MenusMode      m_menus_mode;
    MenusPlacement m_menus_placement;
    bool           m_scale_intensity;
    float          m_line_thickness;
    // Which memory level's kernel intensity dots to show. nullopt = all levels.
    std::optional<rocprofvis_controller_roofline_kernel_intensity_type_t>
        m_memory_peak_filter;

    // Internal state...
    bool                  m_workload_changed;
    const WorkloadInfo*   m_workload;
    uint32_t              m_requested_workload_id;
    KernelMode            m_kernel_mode;
    bool                  m_kernel_changed;
    const KernelInfo*     m_kernel;
    uint32_t              m_requested_kernel_id;
    const KernelInfo*     m_isolated_kernel;
    std::optional<rocprofvis_controller_roofline_ceiling_bandwidth_type_t>
        m_isolated_bandwidth;
    bool                  m_options_changed;
    bool                  m_plot_zoom_enabled;
    std::optional<size_t> m_hovered_item_idx;
    float                 m_hovered_item_distance;
    float                 m_menus_rendered_height;

    DataProvider&    m_data_provider;
    SettingsManager& m_settings;
};

}  // namespace View
}  // namespace RocProfVis
