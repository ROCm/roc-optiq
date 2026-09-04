// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_roofline.h"
#include "icons/rocprovfis_icon_defines.h"
#include "implot/implot.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

namespace RocProfVis
{
namespace View
{

constexpr float       IMPLOT_LEGEND_ICON_SHRINK       = 2.0f;  // Implot_internal.h
constexpr float       HOVER_THESHOLD                  = 8.0f;
constexpr float       HOVER_LINE_WEIGHT_BOOST         = 2.0f;
constexpr float       LINE_THICKNESS_DEFAULT          = 1.0f;
constexpr float       LINE_THICKNESS_MIN              = 1.0f;
constexpr float       LINE_THICKNESS_MAX              = 6.0f;
constexpr float       KERNEL_MARKER_WEIGHT_DEFAULT    = 1.0f;
constexpr const char* DISPLAY_NAMES_CEILING_COMPUTE[] = {
    "Peak MFMA FP4",   // kRPVControllerRooflineCeilingComputeMFMAFP4
    "Peak MFMA FP6",   // kRPVControllerRooflineCeilingComputeMFMAFP6
    "Peak MFMA FP8",   // kRPVControllerRooflineCeilingComputeMFMAFP8
    "Peak VALU I8",    // kRPVControllerRooflineCeilingComputeVALUI8
    "Peak MFMA I8",    // kRPVControllerRooflineCeilingComputeMFMAI8
    "Peak VALU FP16",  // kRPVControllerRooflineCeilingComputeVALUFP16
    "Peak MFMA FP16",  // kRPVControllerRooflineCeilingComputeMFMAFP16
    "Peak MFMA BF16",  // kRPVControllerRooflineCeilingComputeMFMABF16
    "Peak VALU FP32",  // kRPVControllerRooflineCeilingComputeVALUFP32
    "Peak MFMA FP32",  // kRPVControllerRooflineCeilingComputeMFMAFP32
    "Peak VALU I32",   // kRPVControllerRooflineCeilingComputeVALUI32
    "Peak VALU FP64",  // kRPVControllerRooflineCeilingComputeVALUFP64
    "Peak MFMA FP64",  // kRPVControllerRooflineCeilingComputeMFMAFP64
    "Peak VALU I64",   // kRPVControllerRooflineCeilingComputeVALUI64
};
constexpr const char* DISPLAY_NAMES_CEILING_BANDWIDTH[] = {
    "Peak HBM",  // kRPVControllerRooflineCeilingTypeBandwidthHBM
    "Peak L2",   // kRPVControllerRooflineCeilingTypeBandwidthL2
    "Peak L1",   // kRPVControllerRooflineCeilingTypeBandwidthL1
    "Peak LDS",  // kRPVControllerRooflineCeilingTypeBandwidthLDS
};
constexpr const char* DISPLAY_NAMES_KERNEL_INTENSITY[] = {
    "HBM Intensity",  // kRPVControllerRooflineKernelIntensityTypeHBM
    "L2 Intensity",   // kRPVControllerRooflineKernelIntensityTypeL2
    "L1 Intensity",   // kRPVControllerRooflineKernelIntensityTypeL1
    "LDS Intensity",  // kRPVControllerRooflineKernelIntensityTypeLDS
};
constexpr const char* MEMORY_LEVEL_NAMES[] = { "HBM", "L2", "L1", "LDS" };
constexpr const char* FILTER_OPTION_ALL    = "All";
// Shown by every dropdown once visibility has been hand-edited via Custom, so
// the toolbar does not claim a selection that no longer matches the plot.
constexpr const char* FILTER_OPTION_CUSTOM = "-";
constexpr const char* DISPLAY_NAMES_PRESET[] = {
    "FP4",   // PresetModel::Type::FP4
    "FP6",   // PresetModel::Type::FP6
    "FP8",   // PresetModel::Type::FP8
    "FP16",  // PresetModel::Type::FP16
    "FP32",  // PresetModel::Type::FP32
    "FP64",  // PresetModel::Type::FP64
};

constexpr const char* CHART_ZOOM_HINT = "Click chart to enable zoom";

Roofline::Roofline(DataProvider& data_provider, KernelMode kernel_mode)
: m_data_provider(data_provider)
, m_settings(SettingsManager::GetInstance())
, m_show_menus(true)
, m_menus_mode(Legend)
, m_menus_placement(InsideTopRight)
, m_scale_intensity(true)
, m_line_thickness(LINE_THICKNESS_DEFAULT)
, m_active_preset(PresetModel::FP32)
, m_memory_peak_filter(std::nullopt)
, m_menus_rendered_height(0.0f)
, m_hovered_item_distance(FLT_MAX)
, m_workload_changed(false)
, m_kernel_changed(false)
, m_kernel_mode(kernel_mode)
, m_options_changed(false)
, m_plot_zoom_enabled(false)
, m_workload(nullptr)
, m_requested_workload_id(0)
, m_kernel(nullptr)
, m_requested_kernel_id(0)
, m_isolated_kernel(nullptr)
, m_isolated_bandwidth(std::nullopt)
, m_custom_visibility(false)
{
    m_widget_name = GenUniqueName("roofline");
    m_items.resize(static_cast<size_t>(__KRPVControllerRooflineCeilingComputeTypeLast +
                                       __KRPVControllerRooflineCeilingBandwidthTypeLast));
    ItemModel::Type    model_type = ItemModel::CeilingCompute;
    ItemModel::SubType model_subtype;
    for(uint32_t i = __KRPVControllerRooflineCeilingComputeTypeFirst;
        i < __KRPVControllerRooflineCeilingComputeTypeLast; i++)
    {
        rocprofvis_controller_roofline_ceiling_compute_type_t type =
            static_cast<rocprofvis_controller_roofline_ceiling_compute_type_t>(i);
        model_subtype.compute = type;
        m_items[type]         = { model_type, model_subtype, nullptr,
                                  nullptr,    false,         DISPLAY_NAMES_CEILING_COMPUTE[type],
                                  1.0f };
    }
    model_type = ItemModel::CeilingBandwidth;
    for(uint32_t i = __KRPVControllerRooflineCeilingBandwidthTypeFirst;
        i < __KRPVControllerRooflineCeilingBandwidthTypeLast; i++)
    {
        rocprofvis_controller_roofline_ceiling_bandwidth_type_t type =
            static_cast<rocprofvis_controller_roofline_ceiling_bandwidth_type_t>(i);
        model_subtype.bandwidth                                        = type;
        m_items[__KRPVControllerRooflineCeilingComputeTypeLast + type] = {
            model_type, model_subtype, nullptr,
            nullptr,    false,         DISPLAY_NAMES_CEILING_BANDWIDTH[type],
            1.0f
        };
    }
    m_presets = { { PresetModel::FP4, {} },  { PresetModel::FP6, {} },
                  { PresetModel::FP8, {} },  { PresetModel::FP16, {} },
                  { PresetModel::FP32, {} }, { PresetModel::FP64, {} } };
}

void
Roofline::Update()
{
    if(m_workload_changed)
    {
        // Kernel pointers are rebuilt below, so drop any stale filters.
        m_isolated_kernel    = nullptr;
        m_isolated_bandwidth = std::nullopt;
        m_memory_peak_filter = std::nullopt;
        m_workload =
            m_data_provider.ComputeModel().GetWorkload(m_requested_workload_id);
        if(m_workload)
        {
            m_items.resize(__KRPVControllerRooflineCeilingComputeTypeLast +
                           __KRPVControllerRooflineCeilingBandwidthTypeLast);
            for(PresetModel& preset : m_presets)
            {
                preset.item_indices.clear();
            }
            // Discover ceilings...
            for(size_t i = 0; i < m_items.size(); i++)
            {
                switch(m_items[i].type)
                {
                    case ItemModel::Type::CeilingCompute:
                    {
                        if(m_workload->roofline.ceiling_compute.count(
                               m_items[i].subtype.compute) > 0)
                        {
                            m_items[i].info.ceiling =
                                &m_workload->roofline.ceiling_compute
                                     .at(m_items[i].subtype.compute)
                                     .begin()
                                     ->second;
                            m_items[i].parent_info.workload = m_workload;
                            // Assign to presets...
                            switch(m_items[i].subtype.compute)
                            {
                                case kRPVControllerRooflineCeilingComputeMFMAFP4:
                                {
                                    m_presets[PresetModel::FP4].item_indices.emplace_back(
                                        i);
                                    break;
                                }
                                case kRPVControllerRooflineCeilingComputeMFMAFP6:
                                {
                                    m_presets[PresetModel::FP6].item_indices.emplace_back(
                                        i);
                                    break;
                                }
                                case kRPVControllerRooflineCeilingComputeMFMAFP8:
                                {
                                    m_presets[PresetModel::FP8].item_indices.emplace_back(
                                        i);
                                    break;
                                }
                                case kRPVControllerRooflineCeilingComputeVALUFP16:
                                case kRPVControllerRooflineCeilingComputeMFMAFP16:
                                case kRPVControllerRooflineCeilingComputeMFMABF16:
                                {
                                    m_presets[PresetModel::FP16]
                                        .item_indices.emplace_back(i);
                                    break;
                                }
                                case kRPVControllerRooflineCeilingComputeVALUFP32:
                                case kRPVControllerRooflineCeilingComputeMFMAFP32:
                                {
                                    m_presets[PresetModel::FP32]
                                        .item_indices.emplace_back(i);
                                    break;
                                }
                                case kRPVControllerRooflineCeilingComputeVALUFP64:
                                case kRPVControllerRooflineCeilingComputeMFMAFP64:
                                {
                                    m_presets[PresetModel::FP64]
                                        .item_indices.emplace_back(i);
                                    break;
                                }
                            }
                        }
                        else
                        {
                            m_items[i].info.ceiling         = nullptr;
                            m_items[i].parent_info.workload = nullptr;
                        }
                        break;
                    }
                    case ItemModel::Type::CeilingBandwidth:
                    {
                        if(m_workload->roofline.ceiling_bandwidth.count(
                               m_items[i].subtype.bandwidth) > 0)
                        {
                            m_items[i].info.ceiling =
                                &m_workload->roofline.ceiling_bandwidth
                                     .at(m_items[i].subtype.bandwidth)
                                     .begin()
                                     ->second;
                            m_items[i].parent_info.workload = m_workload;
                        }
                        else
                        {
                            m_items[i].info.ceiling         = nullptr;
                            m_items[i].parent_info.workload = nullptr;
                        }
                        break;
                    }
                }
            }
            // Discover kernels...
            uint64_t kernel_duration_scale = 0;
            for(const std::pair<const uint32_t, KernelInfo>& kernel : m_workload->kernels)
            {
                kernel_duration_scale =
                    std::max(kernel_duration_scale,
                             kernel.second.dispatch_metrics[KernelInfo::DurationTotal]);
            }
            ItemModel::Type       model_type = ItemModel::Intensity;
            ItemModel::SubType    model_subtype;
            ItemModel::Info       model_info;
            ItemModel::ParentInfo model_parent_info;
            for(const std::pair<const uint32_t, KernelInfo>& kernel : m_workload->kernels)
            {
                model_parent_info.kernel = &kernel.second;
                for(const std::pair<
                        const rocprofvis_controller_roofline_kernel_intensity_type_t,
                        KernelInfo::Roofline::Intensity>& intensity :
                    kernel.second.roofline.intensities)
                {
                    model_subtype.intensity = intensity.second.type;
                    model_info.intensity    = &intensity.second;
                    m_items.emplace_back(ItemModel{
                        model_type, model_subtype, model_info, model_parent_info, false,
                        std::string(
                            DISPLAY_NAMES_KERNEL_INTENSITY[intensity.second.type]) +
                            ": " + kernel.second.name,
                        kernel_duration_scale > 0
                            ? static_cast<float>(
                                  static_cast<double>(
                                      kernel.second
                                          .dispatch_metrics[KernelInfo::DurationTotal]) /
                                  static_cast<double>(kernel_duration_scale))
                            : KERNEL_MARKER_WEIGHT_DEFAULT });
                }
            }
            // Build filter dropdown options from what the workload actually has;
            // empty memory levels should not be offered.
            m_available_intensities.clear();
            m_available_bandwidths.clear();
            bool intensity_present[IM_ARRAYSIZE(MEMORY_LEVEL_NAMES)] = {};
            for(const std::pair<const uint32_t, KernelInfo>& kernel : m_workload->kernels)
            {
                for(const std::pair<
                        const rocprofvis_controller_roofline_kernel_intensity_type_t,
                        KernelInfo::Roofline::Intensity>& intensity :
                    kernel.second.roofline.intensities)
                {
                    intensity_present[intensity.second.type] = true;
                }
            }
            for(uint32_t i = 0; i < IM_ARRAYSIZE(MEMORY_LEVEL_NAMES); i++)
            {
                if(intensity_present[i])
                {
                    m_available_intensities.emplace_back(
                        static_cast<
                            rocprofvis_controller_roofline_kernel_intensity_type_t>(i));
                }
            }
            for(uint32_t i = __KRPVControllerRooflineCeilingBandwidthTypeFirst;
                i < __KRPVControllerRooflineCeilingBandwidthTypeLast; i++)
            {
                rocprofvis_controller_roofline_ceiling_bandwidth_type_t bandwidth =
                    static_cast<rocprofvis_controller_roofline_ceiling_bandwidth_type_t>(
                        i);
                if(m_workload->roofline.ceiling_bandwidth.count(bandwidth) > 0)
                {
                    m_available_bandwidths.emplace_back(bandwidth);
                }
            }
            // Prefer FP32, then descend in precision, using FP64 as a last resort.
            static constexpr PresetModel::Type PRESET_FALLBACK_ORDER[] = {
                PresetModel::FP32, PresetModel::FP16, PresetModel::FP8,
                PresetModel::FP6,  PresetModel::FP4,  PresetModel::FP64,
            };
            PresetModel::Type selected_preset = PresetModel::FP32;
            for(PresetModel::Type candidate : PRESET_FALLBACK_ORDER)
            {
                if(!m_presets[candidate].item_indices.empty())
                {
                    selected_preset = candidate;
                    break;
                }
            }
            ApplyPreset(selected_preset);
        }
        m_workload_changed = false;
    }
    if(m_kernel_changed)
    {
        m_kernel = nullptr;
        if(m_workload && m_workload->kernels.count(m_requested_kernel_id) > 0)
        {
            m_kernel = &m_workload->kernels.at(m_requested_kernel_id);
        }
        RecomputeVisibility();
        m_kernel_changed = false;
    }
    if(m_options_changed)
    {
        // Determine combination of ceiling variations that are continuous...
        ItemModel* ceiling_ridge_compute   = nullptr;
        ItemModel* ceiling_ridge_bandwidth = nullptr;
        for(ItemModel& item : m_items)
        {
            if(item.type == ItemModel::Type::CeilingCompute && item.info.ceiling &&
               item.visible)
            {
                if(!ceiling_ridge_compute ||
                   (ceiling_ridge_compute &&
                    item.info.ceiling->position.p1.y >
                        ceiling_ridge_compute->info.ceiling->position.p1.y))
                {
                    ceiling_ridge_compute = &item;
                }
            }
            else if(item.type == ItemModel::Type::CeilingBandwidth && item.info.ceiling &&
                    item.visible)
            {
                if(!ceiling_ridge_bandwidth ||
                   (ceiling_ridge_bandwidth &&
                    item.info.ceiling->position.p2.x <
                        ceiling_ridge_bandwidth->info.ceiling->position.p2.x))
                {
                    ceiling_ridge_bandwidth = &item;
                }
            }
        }
        if(ceiling_ridge_bandwidth || ceiling_ridge_compute)
        {
            for(ItemModel& item : m_items)
            {
                if(ceiling_ridge_bandwidth &&
                   item.type == ItemModel::Type::CeilingCompute && item.info.ceiling)
                {
                    item.info.ceiling =
                        &m_workload->roofline.ceiling_compute.at(item.subtype.compute)
                             .at(ceiling_ridge_bandwidth->subtype.bandwidth);
                }
                else if(ceiling_ridge_compute &&
                        item.type == ItemModel::Type::CeilingBandwidth &&
                        item.info.ceiling)
                {
                    item.info.ceiling =
                        &m_workload->roofline.ceiling_bandwidth.at(item.subtype.bandwidth)
                             .at(ceiling_ridge_compute->subtype.compute);
                }
            }
        }
        m_options_changed = false;
    }
}

void
Roofline::Render()
{
    // Fill the parent. AutoResizeY would collapse here since the plot uses (0,0).
    ImGui::PushStyleColor(ImGuiCol_ChildBg, m_settings.GetColor(Colors::kBgPanel));
    ImGui::PushStyleColor(ImGuiCol_Border, m_settings.GetColor(Colors::kBorderColor));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,
                        m_settings.GetDefaultStyle().ChildRounding);
    ImGui::BeginChild("roofline_card", ImVec2(0, 0),
                      ImGuiChildFlags_Borders |
                          ImGuiChildFlags_AlwaysUseWindowPadding);
    SectionTitle("Roofline Analysis");
    bool has_roofline =
        m_workload && !m_workload->roofline.ceiling_bandwidth.empty() &&
        !m_workload->roofline.ceiling_compute.empty() &&
        !(m_kernel_mode == SingleKernel &&
          (!m_kernel || m_kernel->roofline.intensities.empty()));
    if(has_roofline)
    {
        RenderToolbar();
    }
    ImGui::BeginChild("roofline");
    const ImVec2       region     = ImGui::GetContentRegionAvail();
    const ImGuiStyle&  style      = ImGui::GetStyle();
    const ImPlotStyle& plot_style = ImPlot::GetStyle();
    if(!m_workload ||
       (m_workload->roofline.ceiling_bandwidth.empty() ||
        m_workload->roofline.ceiling_compute.empty()) ||
       m_kernel_mode == SingleKernel &&
           (!m_kernel || m_kernel->roofline.intensities.empty()))
    {
        ImGui::GetWindowDrawList()->AddRect(
            ImGui::GetCursorScreenPos() +
                ImVec2(plot_style.PlotBorderSize + plot_style.PlotPadding.x,
                       plot_style.PlotBorderSize + plot_style.PlotPadding.y),
            ImGui::GetCursorScreenPos() + region -
                ImVec2(plot_style.PlotBorderSize + plot_style.PlotPadding.x,
                       plot_style.PlotBorderSize + plot_style.PlotPadding.y +
                           ImGui::GetFrameHeightWithSpacing()),
            ImGui::GetColorU32(style.Colors[ImGuiCol_TableBorderStrong]));
        ImGui::SetCursorPos((region - ImGui::CalcTextSize("No data available.")) * 0.5f);
        ImGui::TextDisabled("No data available.");
    }
    else
    {
        ImPlot::PushStyleColor(ImPlotCol_FrameBg,
                               ThemeColor(m_settings, Colors::kTransparent));
        ImPlot::PushStyleColor(ImPlotCol_PlotBg,
                               ThemeColor(m_settings, Colors::kBgFrame));
        ImPlot::PushStyleColor(ImPlotCol_PlotBorder,
                               ThemeColor(m_settings, Colors::kBorderColor, 0.85f));
        ImPlot::PushStyleColor(ImPlotCol_LegendBg,
                               ThemeColor(m_settings, Colors::kBgPanel, 0.96f));
        ImPlot::PushStyleColor(ImPlotCol_LegendBorder,
                               ThemeColor(m_settings, Colors::kBorderColor, 0.85f));
        ImPlot::PushStyleColor(ImPlotCol_LegendText,
                               ThemeColor(m_settings, Colors::kTextMain));
        ImPlot::PushStyleColor(ImPlotCol_TitleText,
                               ThemeColor(m_settings, Colors::kTextMain));
        ImPlot::PushStyleColor(ImPlotCol_InlayText,
                               ThemeColor(m_settings, Colors::kTextMain));
        ImPlot::PushStyleColor(ImPlotCol_AxisText,
                               ThemeColor(m_settings, Colors::kTextMain));
        ImPlot::PushStyleColor(ImPlotCol_AxisGrid,
                               ThemeColor(m_settings, Colors::kBorderColor, 0.7f));
        ImPlot::PushStyleColor(ImPlotCol_AxisTick,
                               ThemeColor(m_settings, Colors::kTextDim, 0.56f));
        ImPlot::PushStyleColor(ImPlotCol_AxisBg,
                               ThemeColor(m_settings, Colors::kTransparent));
        ImPlot::PushStyleColor(ImPlotCol_AxisBgHovered,
                               ThemeColor(m_settings, Colors::kButtonHovered, 0.72f));
        ImPlot::PushStyleColor(ImPlotCol_AxisBgActive,
                               ThemeColor(m_settings, Colors::kButtonActive, 0.80f));
        ImPlot::PushStyleColor(ImPlotCol_Selection,
                               ThemeColor(m_settings, Colors::kSelectionBorder));
        ImPlot::PushStyleColor(ImPlotCol_Crosshairs,
                               ThemeColor(m_settings, Colors::kSelectionBorder, 0.72f));
        ImPlot::PushColormap(m_settings.GetFlameColormapName());
        ImGui::PushID(m_workload->id);
        bool   menus_outside = (m_menus_placement == Outside) && m_show_menus;
        ImVec2 plot_pos;
        ImVec2 plot_size;

        if(ImPlot::BeginPlot("plot", ImVec2(menus_outside ? 0.75f * region.x : -1, -1),
                             ImPlotFlags_NoTitle | ImPlotFlags_NoFrame |
                                 ImPlotFlags_NoLegend | ImPlotFlags_NoMenus |
                                 ImPlotFlags_Crosshairs))
        {
            ImPlotAxisFlags axis_flags =
                ImPlotAxisFlags_NoSideSwitch | ImPlotAxisFlags_NoHighlight;
            if(!m_plot_zoom_enabled)
            {
                axis_flags |= ImPlotAxisFlags_Lock;
            }
            ImPlot::SetupAxis(ImAxis_X1, "Arithmetic Intensity (FLOP/Byte)", axis_flags);
            ImPlot::SetupAxis(ImAxis_Y1, "Performance (GFLOP/s)", axis_flags);

            ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Log10);
            ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
            ImPlot::SetupAxisLimits(ImAxis_X1, m_workload->roofline.min.x,
                                    m_workload->roofline.max.x);
            ImPlot::SetupAxisLimits(ImAxis_Y1, m_workload->roofline.min.y / 10,
                                    m_workload->roofline.max.y * 10);
            ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, m_workload->roofline.min.x,
                                               m_workload->roofline.max.x);
            ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, m_workload->roofline.min.y / 10,
                                               m_workload->roofline.max.y * 10);
            PlotHoverIdx();
            int item_count = static_cast<int>(m_items.size());
            for(int i = 0; i < item_count; i++)
            {
                bool display = false;
                switch(m_items[i].type)
                {
                    case ItemModel::Type::CeilingCompute:
                    case ItemModel::Type::CeilingBandwidth:
                    {
                        display = m_items[i].info.ceiling;
                        break;
                    }
                    case ItemModel::Type::Intensity:
                    {
                        display = m_items[i].info.intensity;
                    }
                }
                display &= m_items[i].visible;
                if(display)
                {
                    ImGui::PushID(i);
                    bool hovered = m_hovered_item_idx && m_hovered_item_idx.value() == i;
                    switch(m_items[i].type)
                    {
                        case ItemModel::Type::CeilingCompute:
                        case ItemModel::Type::CeilingBandwidth:
                        {
                            ImPlot::SetNextLineStyle(
                                ImPlot::GetColormapColor(i),
                                hovered ? m_line_thickness + HOVER_LINE_WEIGHT_BOOST
                                        : m_line_thickness);
                            ImPlot::PlotLineG(
                                "",
                                [](int idx, void* user_data) -> ImPlotPoint {
                                    const WorkloadInfo::Roofline::Line* line =
                                        static_cast<const WorkloadInfo::Roofline::Line*>(
                                            user_data);
                                    ImPlotPoint point(-1.0, -1.0);
                                    if(line)
                                    {
                                        if(idx == 0)
                                        {
                                            point.x = line->p1.x;
                                            point.y = line->p1.y;
                                        }
                                        else
                                        {
                                            point.x = line->p2.x;
                                            point.y = line->p2.y;
                                        }
                                    }
                                    return point;
                                },
                                (void*) &m_items[i].info.ceiling->position, 2);
                            break;
                        }
                        case ItemModel::Type::Intensity:
                        {
                            ImPlot::SetNextMarkerStyle(
                                IMPLOT_AUTO,
                                plot_style.MarkerSize +
                                    (m_scale_intensity && m_kernel_mode == AllKernels
                                         ? m_items[i].weight
                                         : 0.0f) *
                                        2.0f * plot_style.MarkerSize +
                                    (hovered ? plot_style.MarkerSize : 0.0f),
                                ImPlot::GetColormapColor(i), IMPLOT_AUTO,
                                ImPlot::GetColormapColor(i));
                            ImPlot::PlotScatter(
                                "", &m_items[i].info.intensity->position.x,
                                &m_items[i].info.intensity->position.y, 1);
                            break;
                        }
                    }
                    if(hovered)
                    {
                        ImGui::PushStyleVar(
                            ImGuiStyleVar_WindowPadding,
                            m_settings.GetDefaultIMGUIStyle().WindowPadding);
                        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,
                                            m_settings.GetDefaultStyle().FrameRounding);
                        if(ImGui::BeginItemTooltip())
                        {
                            switch(m_items[i].type)
                            {
                                case ItemModel::Type::CeilingCompute:
                                {
                                    ImGui::GetWindowDrawList()->AddRectFilled(
                                        ImGui::GetCursorScreenPos(),
                                        ImGui::GetCursorScreenPos() +
                                            ImGui::CalcTextSize(m_items[i].label.c_str()),
                                        ImGui::GetColorU32(ImPlot::GetColormapColor(i)));
                                    ImGui::TextUnformatted(m_items[i].label.c_str());
                                    ImGui::Text(
                                        "%.0f GFLOP/s",
                                        std::round(m_items[i].info.ceiling->throughput));
                                    break;
                                }
                                case ItemModel::Type::CeilingBandwidth:
                                {
                                    ImGui::GetWindowDrawList()->AddRectFilled(
                                        ImGui::GetCursorScreenPos(),
                                        ImGui::GetCursorScreenPos() +
                                            ImGui::CalcTextSize(m_items[i].label.c_str()),
                                        ImGui::GetColorU32(ImPlot::GetColormapColor(i)));
                                    ImGui::TextUnformatted(m_items[i].label.c_str());
                                    ImGui::Text(
                                        "%.0f GB/s",
                                        std::round(m_items[i].info.ceiling->throughput));
                                    break;
                                }
                                case ItemModel::Type::Intensity:
                                {
                                    ImGui::BeginGroup();
                                    ImGui::GetWindowDrawList()->AddRectFilled(
                                        ImGui::GetCursorScreenPos(),
                                        ImGui::GetCursorScreenPos() +
                                            ImGui::CalcTextSize(
                                                DISPLAY_NAMES_KERNEL_INTENSITY
                                                    [m_items[i].subtype.intensity]),
                                        ImGui::GetColorU32(ImPlot::GetColormapColor(i)));
                                    ImGui::TextUnformatted(
                                        DISPLAY_NAMES_KERNEL_INTENSITY
                                            [m_items[i].subtype.intensity]);
                                    ImVec2 reserved_pos = ImGui::GetCursorPos();
                                    ImGui::NewLine();
                                    ImGui::Text(
                                        "Invocation(s): %llu",
                                        m_items[i].parent_info.kernel->dispatch_metrics
                                            [KernelInfo::InvocationCount]);
                                    ImGui::Text(
                                        "Duration: %s",
                                        nanosecond_to_formatted_str(
                                            static_cast<double>(
                                                m_items[i]
                                                    .parent_info.kernel->dispatch_metrics
                                                        [KernelInfo::DurationTotal]),
                                            m_settings.GetUserSettings()
                                                .unit_settings.time_format,
                                            true)
                                            .c_str());
                                    ImGui::Text("Arithmetic Intensity: %f FLOP/Byte",
                                                m_items[i].info.intensity->position.x);
                                    ImGui::Text("Performance: %f GFLOP/s",
                                                m_items[i].info.intensity->position.y);
                                    ImGui::EndGroup();
                                    ImGui::SetCursorPos(reserved_pos);
                                    ElidedText(
                                        m_items[i].parent_info.kernel->name.c_str(),
                                        ImGui::GetItemRectSize().x);
                                    break;
                                }
                            }
                            ImGui::EndTooltip();
                        }
                        ImGui::PopStyleVar(2);
                    }
                    ImGui::PopID();
                }
            }
            plot_pos  = ImPlot::GetPlotPos();
            plot_size = ImPlot::GetPlotSize();
            ImPlot::EndPlot();
        }
        ImGui::PopID();
        bool roofline_hovered = plot_size.x > 0.0f && plot_size.y > 0.0f &&
                                ImGui::IsMouseHoveringRect(
                                    plot_pos, plot_pos + plot_size, false);
        if(!m_plot_zoom_enabled && roofline_hovered)
        {
            ImVec2      hint_size = ImGui::CalcTextSize(CHART_ZOOM_HINT);
            ImVec2      hint_pos = plot_pos +
                              ImVec2(plot_size.x - hint_size.x, 0.0f) * 0.5f +
                              ImVec2(0.0f, plot_style.PlotPadding.y);
            ImGui::GetWindowDrawList()->AddText(
                hint_pos, ImGui::GetColorU32(style.Colors[ImGuiCol_TextDisabled]),
                CHART_ZOOM_HINT);
        }
        bool menus_item_hovered = false;
        RenderMenus(region, plot_pos, plot_size, style, plot_style, menus_item_hovered);
        bool dot_hovered =
            !menus_item_hovered && m_kernel_mode == AllKernels && m_hovered_item_idx &&
            m_items[m_hovered_item_idx.value()].type == ItemModel::Type::Intensity;
        bool bandwidth_line_hovered =
            !menus_item_hovered && m_hovered_item_idx &&
            m_items[m_hovered_item_idx.value()].type ==
                ItemModel::Type::CeilingBandwidth;
        // Drag check so panning a zoomed plot is not treated as a click.
        if(roofline_hovered && IsMouseReleasedWithDragCheck(ImGuiMouseButton_Left))
        {
            if(dot_hovered)
            {
                ToggleKernelIsolation(
                    m_items[m_hovered_item_idx.value()].parent_info.kernel);
            }
            else if(bandwidth_line_hovered)
            {
                ToggleBandwidthIsolation(
                    m_items[m_hovered_item_idx.value()].subtype.bandwidth);
            }
        }
        if(!m_plot_zoom_enabled && roofline_hovered && !dot_hovered &&
           !bandwidth_line_hovered && !menus_item_hovered &&
           ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_plot_zoom_enabled = true;
        }
        else if(m_plot_zoom_enabled &&
                (ImGui::IsKeyPressed(ImGuiKey_Escape) ||
                 (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !roofline_hovered)))
        {
            m_plot_zoom_enabled = false;
        }
        if(!menus_item_hovered)
        {
            m_hovered_item_idx      = std::nullopt;
            m_hovered_item_distance = FLT_MAX;
        }
        ImPlot::PopColormap();
        ImPlot::PopStyleColor(16);
    }
    ImGui::EndChild();  // "roofline" inner
    ImGui::EndChild();  // "roofline_card" outer
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

void
Roofline::SetWorkload(uint32_t id)
{
    m_requested_workload_id = id;
    m_workload_changed      = true;
}

void
Roofline::SetKernel(uint32_t id)
{
    m_requested_kernel_id = id;
    m_kernel_changed      = true;
}

void
Roofline::RenderToolbar()
{
    const ImGuiStyle& style = ImGui::GetStyle();
    int               count = 1;  // Compute peak is always present.
    if(!m_available_bandwidths.empty())
    {
        count++;
    }
    if(m_kernel_mode == AllKernels)
    {
        count++;
    }
    if(!m_available_intensities.empty())
    {
        count++;
    }
    float cell = (ImGui::GetContentRegionAvail().x -
                  style.ItemSpacing.x * static_cast<float>(count - 1)) /
                 static_cast<float>(count);
    cell = std::max(cell, ImGui::GetFontSize() * 4.0f);

    // Draw "<label>" to the left, then size the next combo to fill the cell.
    auto label_cell = [&](const char* label) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine();
        float combo_width = cell - ImGui::GetItemRectSize().x - style.ItemSpacing.x;
        ImGui::SetNextItemWidth(std::max(combo_width, ImGui::GetFontSize() * 2.0f));
    };

    // Compute peak (preset)...
    ImGui::BeginGroup();
    ImGui::PushID("compute_peak");
    label_cell("Compute peak");
    PushComboStyles();
    if(ImGui::BeginCombo("##compute_peak",
                         m_custom_visibility ? FILTER_OPTION_CUSTOM
                                             : DISPLAY_NAMES_PRESET[m_active_preset]))
    {
        for(PresetModel& preset : m_presets)
        {
            if(!preset.item_indices.empty() &&
               ImGui::Selectable(DISPLAY_NAMES_PRESET[preset.type],
                                 preset.type == m_active_preset))
            {
                ApplyPreset(preset.type);
            }
        }
        ImGui::EndCombo();
    }
    PopComboStyles();
    ImGui::PopID();
    ImGui::EndGroup();

    // Bandwidth peak...
    if(!m_available_bandwidths.empty())
    {
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::PushID("bandwidth_peak");
        label_cell("Bandwidth peak");
        PushComboStyles();
        if(ImGui::BeginCombo("##bandwidth_peak",
                             m_custom_visibility ? FILTER_OPTION_CUSTOM
                             : m_isolated_bandwidth
                                 ? MEMORY_LEVEL_NAMES[m_isolated_bandwidth.value()]
                                 : FILTER_OPTION_ALL))
        {
            if(ImGui::Selectable(FILTER_OPTION_ALL, !m_isolated_bandwidth))
            {
                m_isolated_bandwidth = std::nullopt;
                RecomputeVisibility();
            }
            for(rocprofvis_controller_roofline_ceiling_bandwidth_type_t bandwidth :
                m_available_bandwidths)
            {
                if(ImGui::Selectable(MEMORY_LEVEL_NAMES[bandwidth],
                                     m_isolated_bandwidth &&
                                         m_isolated_bandwidth.value() == bandwidth))
                {
                    m_isolated_bandwidth = bandwidth;
                    RecomputeVisibility();
                }
            }
            ImGui::EndCombo();
        }
        PopComboStyles();
        ImGui::PopID();
        ImGui::EndGroup();
    }

    // Kernel (workload roofline only)...
    if(m_kernel_mode == AllKernels)
    {
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::PushID("kernel");
        label_cell("Kernel");
        PushComboStyles();
        if(ImGui::BeginCombo("##kernel",
                             m_custom_visibility ? FILTER_OPTION_CUSTOM
                             : m_isolated_kernel  ? m_isolated_kernel->name.c_str()
                                                  : FILTER_OPTION_ALL))
        {
            if(ImGui::Selectable(FILTER_OPTION_ALL, !m_isolated_kernel))
            {
                m_isolated_kernel = nullptr;
                RecomputeVisibility();
            }
            for(const KernelInfo* kernel : m_workload->ordered_kernels)
            {
                // Kernels without roofline data do not belong in this list.
                if(kernel->roofline.intensities.empty())
                {
                    continue;
                }
                // Long/mangled names: elide so the popup stays the toolbar width.
                ImGui::PushID(kernel);
                ImVec2 pos     = ImGui::GetCursorPos();
                bool   clicked = ImGui::Selectable("", m_isolated_kernel == kernel);
                ImGui::SetCursorPos(pos);
                ElidedText(kernel->name.c_str(), ImGui::GetContentRegionAvail().x, cell);
                if(clicked)
                {
                    m_isolated_kernel = kernel;
                    RecomputeVisibility();
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        PopComboStyles();
        ImGui::PopID();
        ImGui::EndGroup();
    }

    // Kernel bandwidth (intensity memory level)...
    if(!m_available_intensities.empty())
    {
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::PushID("kernel_bandwidth");
        label_cell("Kernel bandwidth");
        PushComboStyles();
        if(ImGui::BeginCombo("##kernel_bandwidth",
                             m_custom_visibility ? FILTER_OPTION_CUSTOM
                             : m_memory_peak_filter
                                 ? MEMORY_LEVEL_NAMES[m_memory_peak_filter.value()]
                                 : FILTER_OPTION_ALL))
        {
            if(ImGui::Selectable(FILTER_OPTION_ALL, !m_memory_peak_filter))
            {
                m_memory_peak_filter = std::nullopt;
                RecomputeVisibility();
            }
            for(rocprofvis_controller_roofline_kernel_intensity_type_t level :
                m_available_intensities)
            {
                if(ImGui::Selectable(MEMORY_LEVEL_NAMES[level],
                                     m_memory_peak_filter &&
                                         m_memory_peak_filter.value() == level))
                {
                    m_memory_peak_filter = level;
                    RecomputeVisibility();
                }
            }
            ImGui::EndCombo();
        }
        PopComboStyles();
        ImGui::PopID();
        ImGui::EndGroup();
    }
}

void
Roofline::RenderMenus(ImVec2 region, ImVec2 plot_pos, ImVec2 plot_size,
                       const ImGuiStyle& style, const ImPlotStyle& plot_style,
                       bool& item_hovered)
{
    plot_pos -= ImGui::GetWindowPos();
    float menus_width      = region.x * 0.25f;
    float button_size       = ImGui::GetFrameHeight();
    float max_menus_height  = plot_size.y - plot_style.PlotPadding.y * 2.0f -
                              button_size;

    bool menus_on_right = m_menus_placement == InsideTopRight ||
                          m_menus_placement == InsideBottomRight ||
                          m_menus_placement == Outside;
    bool menus_on_bottom =
        m_menus_placement == InsideBottomLeft || m_menus_placement == InsideBottomRight;

    ImVec2 window_pos;
    ImVec2 button_pos;

    switch(m_menus_placement)
    {
        case InsideTopLeft:
        {
            window_pos.x = plot_pos.x + plot_style.PlotPadding.x;
            window_pos.y =
                plot_pos.y + plot_style.PlotBorderSize + plot_style.PlotPadding.y;
            button_pos.x = m_show_menus ? window_pos.x + menus_width
                                        : plot_pos.x + plot_style.PlotPadding.x;
            button_pos.y = window_pos.y;
            break;
        }
        case InsideTopRight:
        {
            window_pos.x =
                plot_pos.x + plot_size.x - plot_style.PlotPadding.x - menus_width;
            window_pos.y =
                plot_pos.y + plot_style.PlotBorderSize + plot_style.PlotPadding.y;
            button_pos.x = m_show_menus ? window_pos.x - button_size
                                        : plot_pos.x + plot_size.x -
                                              plot_style.PlotPadding.x - button_size;
            button_pos.y = window_pos.y;
            break;
        }
        case InsideBottomLeft:
        {
            window_pos.x = plot_pos.x + plot_style.PlotPadding.x;
            window_pos.y = plot_pos.y + plot_size.y - plot_style.PlotPadding.y -
                           m_menus_rendered_height;
            button_pos.x = m_show_menus ? window_pos.x + menus_width
                                        : plot_pos.x + plot_style.PlotPadding.x;
            button_pos.y = window_pos.y + m_menus_rendered_height - button_size;
            break;
        }
        case InsideBottomRight:
        {
            window_pos.x =
                plot_pos.x + plot_size.x - plot_style.PlotPadding.x - menus_width;
            window_pos.y = plot_pos.y + plot_size.y - plot_style.PlotPadding.y -
                           m_menus_rendered_height - ImGui::GetFrameHeightWithSpacing();
            button_pos.x = m_show_menus ? window_pos.x - button_size
                                        : plot_pos.x + plot_size.x -
                                              plot_style.PlotPadding.x - button_size;
            button_pos.y = window_pos.y + m_menus_rendered_height - button_size;
            break;
        }
        default:
        {
            window_pos.x = region.x - 2 * plot_style.PlotPadding.x -
                           plot_style.PlotBorderSize - menus_width;
            window_pos.y = plot_style.PlotBorderSize + 2 * plot_style.PlotPadding.y;
            button_pos.x = m_show_menus
                               ? window_pos.x - button_size
                               : region.x - 2 * plot_style.PlotPadding.x - button_size;
            button_pos.y = window_pos.y;
            break;
        }
    }

    ImGuiDir arrow_dir = menus_on_right ? (m_show_menus ? ImGuiDir_Right : ImGuiDir_Left)
                                        : (m_show_menus ? ImGuiDir_Left : ImGuiDir_Right);

    ImGui::SetCursorPos(button_pos);
    if(ImGui::ArrowButton("toggle_menus", arrow_dir))
    {
        m_show_menus = !m_show_menus;
    }
    if(m_show_menus)
    {
        if(m_menus_placement == Outside)
        {
            // Fill empty space...
            ImGui::SetCursorPos(plot_pos + ImVec2(plot_size.x, 0.0f));
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImGui::GetCursorScreenPos(),
                ImGui::GetCursorScreenPos() + ImVec2(menus_width, plot_size.y),
                m_settings.GetColor(Colors::kBgFrame));
        }
        ImGui::SetCursorPos(button_pos +
                            ImVec2(0.0f, menus_on_bottom ? -button_size : button_size));
        // Draw the icon manually so it stays centered: ImGui::Button left-clamps
        // a glyph that is wider than the (main-font-sized) button.
        const char* mode_icon    = m_menus_mode == Legend ? ICON_GEAR : ICON_LIST;
        ImVec2      mode_min      = ImGui::GetCursorScreenPos();
        ImVec2      mode_size     = ImVec2(button_size, button_size);
        bool        mode_clicked  = ImGui::InvisibleButton("menu_mode", mode_size);
        ImU32       mode_bg =
            ImGui::IsItemActive()    ? ImGui::GetColorU32(ImGuiCol_ButtonActive)
            : ImGui::IsItemHovered() ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
                                     : ImGui::GetColorU32(ImGuiCol_Button);
        ImDrawList* mode_draw = ImGui::GetWindowDrawList();
        mode_draw->AddRectFilled(mode_min, mode_min + mode_size, mode_bg,
                                 style.FrameRounding);
        if(style.FrameBorderSize > 0.0f)
        {
            mode_draw->AddRect(mode_min, mode_min + mode_size,
                               ImGui::GetColorU32(ImGuiCol_Border), style.FrameRounding, 0,
                               style.FrameBorderSize);
        }
        ImGui::PushFont(m_settings.GetFontManager().GetFont(FontType::kIcon), 0.0f);
        ImVec2 mode_icon_size = ImGui::CalcTextSize(mode_icon);
        mode_draw->AddText(mode_min + (mode_size - mode_icon_size) * 0.5f,
                           ImGui::GetColorU32(ImGuiCol_Text), mode_icon);
        ImGui::PopFont();
        if(mode_clicked)
        {
            m_menus_mode = m_menus_mode == Legend ? Options : Legend;
        }
        ImGui::SetCursorPos(window_pos);

        ImGui::SetNextWindowSizeConstraints(ImVec2(menus_width, button_size * 2.0f),
                                            ImVec2(menus_width, max_menus_height));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, plot_style.LegendInnerPadding);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, m_settings.GetColor(Colors::kBgPanel));
        ImGui::PushStyleColor(ImGuiCol_Border, m_settings.GetColor(Colors::kBorderColor));
        ImGui::BeginChild("menus_window", ImVec2(menus_width, 0.0f),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
        float menus_content_width =
            ImGui::GetWindowWidth() - 2.0f * style.WindowPadding.x;
        float scroll_bar_width = ImGui::GetScrollMaxY() ? style.ScrollbarSize : 0.0f;
        ImGui::BeginGroup();
        if(m_menus_mode == Options)
        {
            ImGui::SeparatorText("Custom");
        }
        ImGui::EndGroup();
        float header_height = ImGui::GetItemRectSize().y + 2 * style.WindowPadding.y;
        float footer_height =
            (m_menus_mode == Legend ? 0.0f
                                    : (m_kernel_mode == AllKernels ? 6 : 5) *
                                          ImGui::GetFrameHeightWithSpacing()) +
            2 * style.WindowPadding.y;
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(menus_content_width, 0),
            ImVec2(menus_content_width,
                   max_menus_height - header_height - footer_height));
        ImGui::BeginChild("menus_scroll_view", ImVec2(menus_content_width, 0),
                          ImGuiChildFlags_AutoResizeY);
        float icon_width = ImGui::GetFontSize();
        scroll_bar_width = std::max(scroll_bar_width,
                                    ImGui::GetScrollMaxY() ? style.ScrollbarSize : 0.0f);
        ImGui::BeginChild("menus_scroll_view_content",
                          ImVec2(menus_content_width - scroll_bar_width, 0),
                          ImGuiChildFlags_AutoResizeY,
                          ImGuiWindowFlags_NoScrollWithMouse);
        bool empty = true;

        int item_count = static_cast<int>(m_items.size());
        for(int i = 0; i < item_count; i++)
        {
            bool display = false;
            switch(m_items[i].type)
            {
                case ItemModel::Type::CeilingCompute:
                case ItemModel::Type::CeilingBandwidth:
                {
                    display = m_items[i].info.ceiling;
                    break;
                }
                case ItemModel::Type::Intensity:
                {
                    // The menu always lists the full data set; in single-kernel
                    // mode that data set is just the selected kernel.
                    display = m_items[i].info.intensity &&
                              (m_kernel_mode == SingleKernel
                                   ? m_items[i].parent_info.kernel == m_kernel
                                   : true);
                    break;
                }
            }
            if(display &&
               (m_menus_mode == Legend && m_items[i].visible || m_menus_mode == Options))
            {
                empty = false;
                ImGui::PushID(i);
                ImGui::PushStyleColor(ImGuiCol_Header,
                                      m_settings.GetColor(Colors::kSelection));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                                      m_settings.GetColor(Colors::kHighlightChart));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                                      m_settings.GetColor(Colors::kHighlightChart));
                ImVec2 pos         = ImGui::GetCursorPos();
                bool   row_clicked = ImGui::Selectable(
                    "", false,
                    m_hovered_item_idx && m_hovered_item_idx.value() == i
                          ? ImGuiSelectableFlags_Highlight
                          : ImGuiSelectableFlags_None);
                bool row_hovered = ImGui::IsItemHovered();
                ImGui::SetCursorPos(pos);
                ImGui::BeginDisabled(m_menus_mode == Options && !m_items[i].visible);
                if(m_menus_mode == Legend)
                {
                    switch(m_items[i].type)
                    {
                        case ItemModel::Type::CeilingCompute:
                        case ItemModel::Type::CeilingBandwidth:
                        {
                            ImGui::GetWindowDrawList()->AddRectFilled(
                                ImGui::GetCursorScreenPos() +
                                    ImVec2(2 * IMPLOT_LEGEND_ICON_SHRINK,
                                           2 * IMPLOT_LEGEND_ICON_SHRINK),
                                ImGui::GetCursorScreenPos() +
                                    ImVec2(icon_width - 2 * IMPLOT_LEGEND_ICON_SHRINK,
                                           icon_width - 2 * IMPLOT_LEGEND_ICON_SHRINK),
                                ImGui::GetColorU32(
                                    ImGui::GetColorU32(ImPlot::GetColormapColor(i)),
                                    row_hovered ? 0.75f : 1.0f));
                            break;
                        }
                        case ItemModel::Type::Intensity:
                        {
                            ImGui::GetWindowDrawList()->AddCircleFilled(
                                ImGui::GetCursorScreenPos() +
                                    ImVec2(icon_width, icon_width) * 0.5f,
                                icon_width * 0.5f - 2 * IMPLOT_LEGEND_ICON_SHRINK,
                                ImGui::GetColorU32(
                                    ImGui::GetColorU32(ImPlot::GetColormapColor(i)),
                                    row_hovered ? 0.75f : 1.0f),
                                10);
                            break;
                        }
                    }
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + icon_width);
                }
                else
                {
                    ImGui::PushFont(m_settings.GetFontManager().GetFont(FontType::kIcon), 0.0f);
                    ImGui::TextUnformatted(m_items[i].visible ? ICON_EYE
                                                              : ICON_EYE_SLASH);
                    ImGui::PopFont();
                    ImGui::SameLine();
                }
                ElidedText(m_items[i].label.c_str(), ImGui::GetContentRegionAvail().x,
                           plot_size.x * 0.5f);
                ImGui::EndDisabled();
                if(row_hovered)
                {
                    m_hovered_item_idx = i;
                    item_hovered       = true;
                    if(row_clicked)
                    {
                        if(m_menus_mode == Options)
                        {
                            m_items[i].visible  = !m_items[i].visible;
                            m_custom_visibility = true;
                            m_options_changed   = true;
                        }
                        else if(m_kernel_mode == AllKernels &&
                                m_items[i].type == ItemModel::Type::Intensity)
                        {
                            ToggleKernelIsolation(m_items[i].parent_info.kernel);
                        }
                        else if(m_items[i].type == ItemModel::Type::CeilingBandwidth)
                        {
                            ToggleBandwidthIsolation(m_items[i].subtype.bandwidth);
                        }
                    }
                }
                ImGui::PopStyleColor(3);
                ImGui::PopID();
            }
        }
        if(empty)
        {
            ImGui::TextDisabled("No data selected.");
        }
        ImGui::EndChild();
        ImGui::EndChild();
        if(m_menus_mode == Options)
        {
            ImGui::SeparatorText("Options");
            if(m_kernel_mode == AllKernels)
            {
                ImGui::PushID("kernel_scale");
                ImGui::Checkbox("", &m_scale_intensity);
                ImGui::SameLine();
                ElidedText("Scale kernel marker size to duration",
                           ImGui::GetContentRegionAvail().x, plot_size.x * 0.5f,
                           Alignment_Left, true);
                ImGui::PopID();
            }
            ImGui::PushID("line_thickness");
            ElidedText("Line thickness", ImGui::GetContentRegionAvail().x,
                       plot_size.x * 0.5f, Alignment_Left, true);
            ImGui::SetNextItemWidth(-1.0f);
            if(ImGui::SliderFloat("##line_thickness", &m_line_thickness,
                                  LINE_THICKNESS_MIN, LINE_THICKNESS_MAX, "%.1f px"))
            {
                m_line_thickness =
                    std::clamp(m_line_thickness, LINE_THICKNESS_MIN, LINE_THICKNESS_MAX);
            }
            ImGui::PopID();
            ImGui::PushID("menus_placement");
            ElidedText("Menus position", ImGui::GetContentRegionAvail().x,
                       plot_size.x * 0.5f, Alignment_Left, true);
            ImGui::SetNextItemWidth(-1.0f);
            int placement_idx = static_cast<int>(m_menus_placement);
            PushComboStyles();
            if(ImGui::Combo("##placement", &placement_idx,
                            "Inside, Top Left\0"
                            "Inside, Top Right\0"
                            "Inside, Bottom Left\0"
                            "Inside, Bottom Right\0"
                            "Outside\0\0"))
            {
                m_menus_placement = static_cast<MenusPlacement>(placement_idx);
            }
            PopComboStyles();
            ImGui::PopID();
        }
        m_menus_rendered_height = ImGui::GetWindowHeight();
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();
        ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelX);
    }
}

void
Roofline::PlotHoverIdx()
{
    if(ImPlot::IsPlotHovered())
    {
        ImVec2 mouse_pos = ImGui::GetMousePos();
        float  distance  = FLT_MAX;
        // Pick the closest visible item after plotting all candidates.
        for(size_t i = 0; i < m_items.size(); i++)
        {
            if(m_items[i].visible)
            {
                switch(m_items[i].type)
                {
                    case ItemModel::Type::CeilingCompute:
                    case ItemModel::Type::CeilingBandwidth:
                    {
                        if(m_items[i].info.ceiling)
                        {
                            ImVec2 p1_pos = ImPlot::PlotToPixels(
                                ImPlotPoint(m_items[i].info.ceiling->position.p1.x,
                                            m_items[i].info.ceiling->position.p1.y));
                            ImVec2 p2_pos = ImPlot::PlotToPixels(
                                ImPlotPoint(m_items[i].info.ceiling->position.p2.x,
                                            m_items[i].info.ceiling->position.p2.y));
                            ImVec2 line_direction =
                                ImVec2(p2_pos.x - p1_pos.x, p2_pos.y - p1_pos.y);
                            ImVec2 point_to_mouse =
                                ImVec2(mouse_pos.x - p1_pos.x, mouse_pos.y - p1_pos.y);
                            float projection =
                                std::clamp((point_to_mouse.x * line_direction.x +
                                            point_to_mouse.y * line_direction.y) /
                                               (line_direction.x * line_direction.x +
                                                line_direction.y * line_direction.y),
                                           0.0f, 1.0f);
                            ImVec2 closest_point =
                                ImVec2(p1_pos.x + projection * line_direction.x,
                                       p1_pos.y + projection * line_direction.y);
                            float dx = mouse_pos.x - closest_point.x;
                            float dy = mouse_pos.y - closest_point.y;
                            distance = std::sqrt(dx * dx + dy * dy);
                        }
                        break;
                    }
                    case ItemModel::Type::Intensity:
                    {
                        if(m_items[i].info.ceiling)
                        {
                            ImVec2 closest_point = ImPlot::PlotToPixels(
                                ImPlotPoint(m_items[i].info.intensity->position.x,
                                            m_items[i].info.intensity->position.y));
                            float dx = mouse_pos.x - closest_point.x;
                            float dy = mouse_pos.y - closest_point.y;
                            distance = std::sqrt(dx * dx + dy * dy);
                        }
                        break;
                    }
                }
                if(distance < HOVER_THESHOLD && distance < m_hovered_item_distance)
                {
                    m_hovered_item_idx      = i;
                    m_hovered_item_distance = distance;
                }
            }
        }
    }
}

void
Roofline::ApplyPreset(PresetModel::Type type)
{
    m_active_preset = type;
    RecomputeVisibility();
}

void
Roofline::RecomputeVisibility()
{
    for(ItemModel& item : m_items)
    {
        switch(item.type)
        {
            case ItemModel::Type::CeilingCompute:
            {
                // Enabled below from the active preset.
                item.visible = false;
                break;
            }
            case ItemModel::Type::CeilingBandwidth:
            {
                item.visible = !m_isolated_bandwidth ||
                               item.subtype.bandwidth == m_isolated_bandwidth.value();
                break;
            }
            case ItemModel::Type::Intensity:
            {
                bool level_ok = !m_memory_peak_filter ||
                                item.subtype.intensity == m_memory_peak_filter.value();
                bool kernel_ok =
                    m_kernel_mode == SingleKernel
                        ? item.parent_info.kernel == m_kernel
                        : (!m_isolated_kernel ||
                           item.parent_info.kernel == m_isolated_kernel);
                item.visible = level_ok && kernel_ok;
                break;
            }
        }
    }
    for(const size_t& item_idx : m_presets[m_active_preset].item_indices)
    {
        m_items[item_idx].visible = true;
    }
    // Visibility now matches the dropdown selections again.
    m_custom_visibility = false;
    // Visibility can change which ceilings are shown, so recompute the ridges.
    m_options_changed = true;
}

void
Roofline::ToggleKernelIsolation(const KernelInfo* kernel)
{
    m_isolated_kernel = (kernel && m_isolated_kernel != kernel) ? kernel : nullptr;
    RecomputeVisibility();
}

void
Roofline::ToggleBandwidthIsolation(
    rocprofvis_controller_roofline_ceiling_bandwidth_type_t bandwidth)
{
    if(m_isolated_bandwidth && m_isolated_bandwidth.value() == bandwidth)
    {
        m_isolated_bandwidth = std::nullopt;
    }
    else
    {
        m_isolated_bandwidth = bandwidth;
    }
    RecomputeVisibility();
}

}  // namespace View
}  // namespace RocProfVis
