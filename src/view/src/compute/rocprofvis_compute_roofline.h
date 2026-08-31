// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "compute/rocprofvis_compute_model_types.h"
#include "rocprofvis_controller_enums.h"
#include "widgets/rocprofvis_widget.h"
#include <bitset>
#include <optional>
#include <unordered_set>
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
    enum Mode
    {
        SingleKernel,
        AllKernels,
        Compare,
    };

    Roofline(DataProvider& data_provider, Mode mode);

    void Update() override;
    void Render() override;

    void SetWorkload(uint32_t id);
    void SetKernel(uint32_t id);
    void SetCompareTarget(uint32_t workload_id, uint32_t kernel_id);

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
    struct FilterModel
    {
        enum Type
        {
            CeilingCompute,
            CeilingBandwidth,
            IntensityKernel,
            IntensityBandwidth,
        };
        enum ComputeType
        {
            ComputeTypeAll,
            FP4,
            FP6,
            FP8,
            FP16,
            FP32,
            FP64,
            ComputeTypeUnknown,
        };
        enum BandwidthType
        {
            BandwidthTypeAll,
            HBM,
            L2,
            L1,
            LDS,
            BandwidthTypeUnknown,
        };
        const char*                name;
        std::unordered_set<size_t> item_idx;
    };
    struct ItemModel
    {
        enum Type
        {
            CeilingCompute,
            CeilingBandwidth,
            Intensity,
            IntensityDelta,
        };
        union SubType
        {
            rocprofvis_controller_roofline_ceiling_compute_type_t   compute;
            rocprofvis_controller_roofline_ceiling_bandwidth_type_t bandwidth;
            rocprofvis_controller_roofline_kernel_intensity_type_t  intensity;
        };
        union Info
        {
            struct Delta
            {
                const KernelInfo::Roofline::Intensity* baseline;
                const KernelInfo::Roofline::Intensity* target;
            };
            const WorkloadInfo::Roofline::Ceiling* ceiling;
            const KernelInfo::Roofline::Intensity* intensity;
            Delta                                  delta;
        };
        union ParentInfo
        {
            struct Delta
            {
                const KernelInfo* baseline;
                const KernelInfo* target;
            };
            const WorkloadInfo* workload;
            const KernelInfo*   kernel;
            Delta               delta;
        };
        enum Visible
        {
            Plot,
            Menus,
            Count,
        };
        Type                                          type;
        SubType                                       subtype;
        Info                                          info;
        ParentInfo                                    parent_info;
        std::unordered_map<FilterModel::Type, size_t> filter_idx;
        std::bitset<Visible::Count>                   visible;
        std::string                                   label;
        float                                         weight;
    };

    // Update components...
    void UpdateCeilings(const WorkloadInfo* workload);
    void UpdateIntensities(const WorkloadInfo*                   workload,
                           const std::vector<const KernelInfo*>& kernels, bool append);
    void UpdateDeltas();

    // Render components...
    void RenderMenus(ImVec2 region, ImVec2 plot_pos, ImVec2 plot_size,
                     const ImGuiStyle& style, const ImPlotStyle& plot_style,
                     bool& item_hovered, bool& plot_hovered);
    void FilterCombo(const char* label, const float width, const ImGuiStyle& style,
                     const std::vector<FilterModel>& filters, bool& custom_override,
                     size_t& active_idx);

    // Utilities...
    void  PlotHoverIdx();
    float PointDistanceFromLine(ImVec2 point, ImVec2 line_p1, ImVec2 line_p2) const;
    bool  ItemValid(const ItemModel& item) const;
    ImU32 ItemColor(const ItemModel& item, bool hovered, bool legend) const;
    void  ApplyItemFilter(const ItemModel& item);
    void  ApplyFilters();
    FilterModel::ComputeType FilterComputeType(
        rocprofvis_controller_roofline_ceiling_compute_type_t type) const;
    FilterModel::BandwidthType FilterBandwidthType(
        rocprofvis_controller_roofline_ceiling_bandwidth_type_t type) const;
    FilterModel::BandwidthType FilterBandwidthType(
        rocprofvis_controller_roofline_kernel_intensity_type_t type) const;

    // Internal models...
    std::vector<ItemModel>   m_items;
    std::vector<FilterModel> m_filters_ceiling_compute;
    std::vector<FilterModel> m_filters_ceiling_bandwidth;
    std::vector<FilterModel> m_filters_intensity_kernel;
    std::vector<FilterModel> m_filters_intensity_bandwidth;

    // User options...
    uint32_t       m_requested_primary_workload_id;
    uint32_t       m_requested_secondary_workload_id;
    uint32_t       m_requested_primary_kernel_id;
    uint32_t       m_requested_kernel_secondary_id;
    bool           m_show_menus;
    MenusMode      m_menus_mode;
    MenusPlacement m_menus_placement;
    bool           m_scale_intensity;
    float          m_line_thickness;
    bool           m_ceiling_labels;
    bool           m_alternate_ceiling_source;
    size_t         m_active_filter_ceiling_compute;
    size_t         m_active_filter_ceiling_bandwidth;
    size_t         m_active_filter_intensity_kernel;
    size_t         m_active_filter_intensity_bandwidth;
    bool           m_custom_ceiling_compute;
    bool           m_custom_ceiling_bandwidth;
    bool           m_custom_intensity;

    // Internal state...
    Mode                    m_mode;
    bool                    m_workload_changed;
    const WorkloadInfo*     m_workload_primary;
    const WorkloadInfo*     m_workload_secondary;
    const WorkloadInfo**    m_ceiling_source;
    bool                    m_kernel_changed;
    const KernelInfo*       m_kernel_primary;
    const KernelInfo*       m_kernel_secondary;
    bool                    m_options_changed;
    bool                    m_plot_nav_enabled;
    std::optional<size_t>   m_hovered_item_idx;
    float                   m_hovered_item_distance;
    std::pair<Point, Point> m_bounding_box_ceiling;
    std::pair<Point, Point> m_bounding_box_intensity;
    float                   m_menus_rendered_height;

    DataProvider&    m_data_provider;
    SettingsManager& m_settings;
};

}  // namespace View
}  // namespace RocProfVis
