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
    Roofline(DataProvider& data_provider);

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
    enum MenusPosition
    {
        TopRight,
        TopLeft,
        BottomLeft,
        BottomRight
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

    void RenderMenus(const ImVec2 region, const ImGuiStyle& style,
                     const ImPlotStyle& plot_style, bool& item_hovered);
    void PlotHoverIdx();
    void ApplyPreset(PresetModel::Type type);

    // Internal models...
    std::vector<ItemModel>   m_items;
    std::vector<PresetModel> m_presets;

    // User options...
    bool          m_show_menus;
    MenusMode     m_menus_mode;
    MenusPosition m_menus_position;
    bool          m_scale_intensity;

    // Internal state...
    bool                  m_workload_changed;
    const WorkloadInfo*   m_workload;
    uint32_t              m_requested_workload_id;
    bool                  m_kernel_changed;
    const KernelInfo*     m_kernel;
    uint32_t              m_requested_kernel_id;
    bool                  m_options_changed;
    std::optional<size_t> m_hovered_item_idx;
    float                 m_hovered_item_distance;
    float                 m_menus_height;
    float                 m_plot_left_x;

    DataProvider&    m_data_provider;
    SettingsManager& m_settings;
};

}  // namespace View
}  // namespace RocProfVis
