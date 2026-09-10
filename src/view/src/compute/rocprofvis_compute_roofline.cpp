// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_roofline.h"
#include "icons/rocprovfis_icon_defines.h"
#include "implot/implot.h"
#include "rocprofvis_compute_selection.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <limits>

namespace RocProfVis
{
namespace View
{

constexpr float       IMPLOT_LEGEND_ICON_SHRINK       = 2.0f;         // Implot_internal.h
constexpr float       IMPLOT_MARKER_BASE              = 0.86602540f;  // implot_items.cpp
constexpr float       IMPLOT_MARKER_SCALE_FACTOR      = 1.15470054f;  // 2 / sqrt(3)
constexpr double      MIN_X                           = 0.01;         // roofline_calc.py
constexpr double      MAX_X                           = 1000.00;      // roofline_calc.py
constexpr float       HOVER_THESHOLD                  = 8.0f;
constexpr float       HOVER_LINE_WEIGHT_BOOST         = 2.0f;
constexpr float       LINE_THICKNESS_DEFAULT          = 1.0f;
constexpr float       LINE_THICKNESS_MIN              = 1.0f;
constexpr float       LINE_THICKNESS_MAX              = 6.0f;
constexpr float       KERNEL_MARKER_WEIGHT_DEFAULT    = 1.0f;
constexpr float       DASHED_LINE_SEGMENT_LENGTH      = 6.0f;
constexpr float       DASHED_LINE_SEGMENT_GAP         = 4.0f;
constexpr float       DASHED_LINE_SEGMENT_CAP         = 512.0f;
constexpr float       LEGEND_HOVER_COLOR_ALPHA        = 0.75f;
constexpr double      SHADE_STEP_EPSILON              = 1e-9;
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
constexpr const std::pair<const char*, ImPlotMarker> DISPLAY_PROPS_KERNEL_INTENSITY[] = {
    { "HBM Intensity",
      ImPlotMarker_Circle },  // kRPVControllerRooflineKernelIntensityTypeHBM
    { "L2 Intensity",
      ImPlotMarker_Square },  // kRPVControllerRooflineKernelIntensityTypeL2
    { "L1 Intensity",
      ImPlotMarker_Diamond },             // kRPVControllerRooflineKernelIntensityTypeL1
    { "LDS Intensity", ImPlotMarker_Up }  // kRPVControllerRooflineKernelIntensityTypeLDS
};
constexpr const char* DISPLAY_NAMES_FILTER_BANDWIDTH[] = {
    "All",  // FilterModel::BandwidthType::All
    "HBM",  // FilterModel::BandwidthType::HBM
    "L2",   // FilterModel::BandwidthType::L2
    "L1",   // FilterModel::BandwidthType::L1
    "LDS"   // FilterModel::BandwidthType::LDS
};
constexpr const char* DISPLAY_NAMES_FILTER_COMPUTE[] = {
    "All",   // FilterModel::ComputeType::All
    "FP4",   // FilterModel::ComputeType::FP4
    "FP6",   // FilterModel::ComputeType::FP6
    "FP8",   // FilterModel::ComputeType::FP8
    "FP16",  // FilterModel::ComputeType::FP16
    "FP32",  // FilterModel::ComputeType::FP32
    "FP64",  // FilterModel::ComputeType::FP64
};
constexpr const char* HINT_FOCUS         = "Click chart to enable zoom/pan";
constexpr const char* HINT_EMPTY_GENERIC = "No data available.";
constexpr const char* HINT_EMPTY_PRIMARY_KERNEL =
    "No data available for baseline kernel.";
constexpr const char* HINT_EMPTY_SECONDARY_KERNEL =
    "No data available for target kernel.";
constexpr const char* HINT_EMPTY_COMPARE_INIT = "Select a kernel for comparison.";
constexpr const char* DELTA                   = "\xCE\x94";

Roofline::Roofline(DataProvider& data_provider, Mode mode)
: m_requested_primary_workload_id(ComputeSelection::INVALID_SELECTION_ID)
, m_requested_secondary_workload_id(ComputeSelection::INVALID_SELECTION_ID)
, m_requested_primary_kernel_id(ComputeSelection::INVALID_SELECTION_ID)
, m_requested_kernel_secondary_id(ComputeSelection::INVALID_SELECTION_ID)
, m_show_menus(true)
, m_menus_mode(Legend)
, m_menus_placement(InsideTopRight)
, m_scale_intensity(true)
, m_line_thickness(LINE_THICKNESS_DEFAULT)
, m_ceiling_labels(true)
, m_alternate_ceiling_source(false)
, m_active_filter_ceiling_compute(std::numeric_limits<size_t>::max())
, m_active_filter_ceiling_bandwidth(0)
, m_active_filter_intensity_kernel(0)
, m_active_filter_intensity_bandwidth(0)
, m_custom_ceiling_compute(false)
, m_custom_ceiling_bandwidth(false)
, m_custom_intensity(false)
, m_mode(mode)
, m_workload_changed(false)
, m_workload_primary(nullptr)
, m_workload_secondary(nullptr)
, m_ceiling_source(nullptr)
, m_kernel_changed(false)
, m_kernel_primary(nullptr)
, m_kernel_secondary(nullptr)
, m_options_changed(false)
, m_plot_nav_enabled(false)
, m_hovered_item_idx(std::nullopt)
, m_hovered_item_distance(FLT_MAX)
, m_bounding_box_ceiling({ { DBL_MAX, DBL_MAX }, { -DBL_MAX, -DBL_MAX } })
, m_bounding_box_intensity({ { DBL_MAX, DBL_MAX }, { -DBL_MAX, -DBL_MAX } })
, m_menus_rendered_height(0.0f)
, m_global_shade_ctx({})
, m_data_provider(data_provider)
, m_settings(SettingsManager::GetInstance())
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
        m_items[type]         = { model_type,
                                  model_subtype,
                                  ItemModel::Info(),
                                  ItemModel::ParentInfo(),
                                  {},
                                  std::bitset<ItemModel::Visible::Count>("10"),
                                  DISPLAY_NAMES_CEILING_COMPUTE[type],
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
            model_type,
            model_subtype,
            ItemModel::Info(),
            ItemModel::ParentInfo(),
            {},
            std::bitset<ItemModel::Visible::Count>("10"),
            DISPLAY_NAMES_CEILING_BANDWIDTH[type],
            1.0f
        };
    }
    m_filters_ceiling_compute = {
        { DISPLAY_NAMES_FILTER_COMPUTE[FilterModel::ComputeType::ComputeTypeAll], {} },
        { DISPLAY_NAMES_FILTER_COMPUTE[FilterModel::ComputeType::FP4], {} },
        { DISPLAY_NAMES_FILTER_COMPUTE[FilterModel::ComputeType::FP6], {} },
        { DISPLAY_NAMES_FILTER_COMPUTE[FilterModel::ComputeType::FP8], {} },
        { DISPLAY_NAMES_FILTER_COMPUTE[FilterModel::ComputeType::FP16], {} },
        { DISPLAY_NAMES_FILTER_COMPUTE[FilterModel::ComputeType::FP32], {} },
        { DISPLAY_NAMES_FILTER_COMPUTE[FilterModel::ComputeType::FP64], {} }
    };
    m_filters_ceiling_bandwidth = {
        { DISPLAY_NAMES_FILTER_BANDWIDTH[FilterModel::BandwidthType::BandwidthTypeAll],
          {} },
        { DISPLAY_NAMES_FILTER_BANDWIDTH[FilterModel::BandwidthType::HBM], {} },
        { DISPLAY_NAMES_FILTER_BANDWIDTH[FilterModel::BandwidthType::L2], {} },
        { DISPLAY_NAMES_FILTER_BANDWIDTH[FilterModel::BandwidthType::L1], {} },
        { DISPLAY_NAMES_FILTER_BANDWIDTH[FilterModel::BandwidthType::LDS], {} }
    };
    m_filters_intensity_kernel    = { { "All", {} } };
    m_filters_intensity_bandwidth = {
        { DISPLAY_NAMES_FILTER_BANDWIDTH[FilterModel::BandwidthType::BandwidthTypeAll],
          {} },
        { DISPLAY_NAMES_FILTER_BANDWIDTH[FilterModel::BandwidthType::HBM], {} },
        { DISPLAY_NAMES_FILTER_BANDWIDTH[FilterModel::BandwidthType::L2], {} },
        { DISPLAY_NAMES_FILTER_BANDWIDTH[FilterModel::BandwidthType::L1], {} },
        { DISPLAY_NAMES_FILTER_BANDWIDTH[FilterModel::BandwidthType::LDS], {} }
    };
}

void
Roofline::Update()
{
    if(m_workload_changed)
    {
        m_workload_primary =
            m_data_provider.ComputeModel().GetWorkload(m_requested_primary_workload_id);
        m_workload_secondary =
            m_data_provider.ComputeModel().GetWorkload(m_requested_secondary_workload_id);
        m_ceiling_source =
            m_alternate_ceiling_source ? &m_workload_secondary : &m_workload_primary;
        if(*m_ceiling_source)
        {
            UpdateCeilings(*m_ceiling_source);
            if(m_mode == AllKernels && !m_kernel_changed)
            {
                m_kernel_changed = true;
            }
            ApplyFilters();
        }
        m_workload_changed = false;
    }
    if(m_kernel_changed)
    {
        m_kernel_primary   = nullptr;
        m_kernel_secondary = nullptr;
        if(m_workload_primary)
        {
            if(m_mode == AllKernels)
            {
                UpdateIntensities(m_workload_primary, m_workload_primary->ordered_kernels,
                                  false);
            }
            else if((m_mode == SingleKernel || m_mode == Compare) &&
                    m_workload_primary->kernels.count(m_requested_primary_kernel_id))
            {
                m_kernel_primary =
                    &m_workload_primary->kernels.at(m_requested_primary_kernel_id);
                std::vector<const KernelInfo*> kernels = { m_kernel_primary };
                UpdateIntensities(m_workload_primary, kernels, false);
                if(m_mode == Compare && m_workload_secondary &&
                   m_workload_secondary->kernels.count(m_requested_kernel_secondary_id))
                {
                    m_kernel_secondary = &m_workload_secondary->kernels.at(
                        m_requested_kernel_secondary_id);
                    kernels = { m_kernel_secondary };
                    UpdateIntensities(m_workload_secondary, kernels, true);
                    UpdateDeltas();
                }
            }
            ApplyFilters();
        }
        m_kernel_changed = false;
    }
    if(m_options_changed && m_ceiling_source && *m_ceiling_source)
    {
        // Determine combination of ceiling variations that are continuous...
        ItemModel* ceiling_ridge_compute   = nullptr;
        ItemModel* ceiling_ridge_bandwidth = nullptr;
        m_global_shade_ctx.ceiling_compute_visible_sorted.clear();
        m_global_shade_ctx.ceiling_bandwidth_visible_sorted.clear();
        std::array<bool, __kRPVControllerRooflineKernelIntensityTypeLast> delta_visible;
        std::fill(delta_visible.begin(), delta_visible.end(), true);
        for(ItemModel& item : m_items)
        {
            if(item.type == ItemModel::Type::CeilingCompute && item.info.ceiling &&
               item.visible[ItemModel::Visible::Plot])
            {
                if(!ceiling_ridge_compute ||
                   (ceiling_ridge_compute &&
                    item.info.ceiling->position.p1.y >
                        ceiling_ridge_compute->info.ceiling->position.p1.y))
                {
                    ceiling_ridge_compute = &item;
                }
                m_global_shade_ctx.ceiling_compute_visible_sorted.push_back(&item);
            }
            else if(item.type == ItemModel::Type::CeilingBandwidth && item.info.ceiling &&
                    item.visible[ItemModel::Visible::Plot])
            {
                if(!ceiling_ridge_bandwidth ||
                   (ceiling_ridge_bandwidth &&
                    item.info.ceiling->position.p2.x <
                        ceiling_ridge_bandwidth->info.ceiling->position.p2.x))
                {
                    ceiling_ridge_bandwidth = &item;
                }
                m_global_shade_ctx.ceiling_bandwidth_visible_sorted.push_back(&item);
            }
            else if(m_mode == Compare)
            {
                if(item.type == ItemModel::Type::Intensity && item.info.intensity)
                {
                    delta_visible[item.subtype.intensity] &=
                        item.visible[ItemModel::Visible::Plot];
                }
                else if(item.type == ItemModel::Type::IntensityDelta)
                {
                    item.visible[ItemModel::Visible::Plot] =
                        delta_visible[item.subtype.intensity];
                }
            }
        }
        std::stable_sort(m_global_shade_ctx.ceiling_compute_visible_sorted.begin(),
                         m_global_shade_ctx.ceiling_compute_visible_sorted.end(),
                         [](const ItemModel* a, const ItemModel* b) -> bool {
                             return a->info.ceiling->throughput <
                                    b->info.ceiling->throughput;
                         });
        std::stable_sort(m_global_shade_ctx.ceiling_bandwidth_visible_sorted.begin(),
                         m_global_shade_ctx.ceiling_bandwidth_visible_sorted.end(),
                         [](const ItemModel* a, const ItemModel* b) -> bool {
                             return a->info.ceiling->throughput <
                                    b->info.ceiling->throughput;
                         });
        if(ceiling_ridge_bandwidth || ceiling_ridge_compute)
        {
            for(ItemModel& item : m_items)
            {
                if(ceiling_ridge_bandwidth &&
                   item.type == ItemModel::Type::CeilingCompute && item.info.ceiling &&
                   (*m_ceiling_source)
                           ->roofline.ceiling_compute.count(item.subtype.compute) > 0 &&
                   (*m_ceiling_source)
                           ->roofline.ceiling_compute.at(item.subtype.compute)
                           .count(ceiling_ridge_bandwidth->subtype.bandwidth) > 0)
                {
                    item.info.ceiling =
                        &(*m_ceiling_source)
                             ->roofline.ceiling_compute.at(item.subtype.compute)
                             .at(ceiling_ridge_bandwidth->subtype.bandwidth);
                }
                else if(ceiling_ridge_compute &&
                        item.type == ItemModel::Type::CeilingBandwidth &&
                        item.info.ceiling &&
                        (*m_ceiling_source)
                                ->roofline.ceiling_bandwidth.count(
                                    item.subtype.bandwidth) > 0 &&
                        (*m_ceiling_source)
                                ->roofline.ceiling_bandwidth.at(item.subtype.bandwidth)
                                .count(ceiling_ridge_compute->subtype.compute) > 0)
                {
                    item.info.ceiling =
                        &(*m_ceiling_source)
                             ->roofline.ceiling_bandwidth.at(item.subtype.bandwidth)
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
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
    SectionTitle("Roofline Analysis");
    bool ceiling_empty = !(m_ceiling_source && *m_ceiling_source) ||
                         (*m_ceiling_source)->roofline.ceiling_bandwidth.empty() ||
                         (*m_ceiling_source)->roofline.ceiling_compute.empty();
    bool primary_kernel_empty =
        m_mode == AllKernels
            ? m_items.size() <= __KRPVControllerRooflineCeilingComputeTypeLast +
                                    __KRPVControllerRooflineCeilingBandwidthTypeLast
            : !m_kernel_primary || m_kernel_primary->roofline.intensities.empty();
    bool secondary_kernel_empty =
        m_mode == Compare
            ? !m_kernel_secondary || m_kernel_secondary->roofline.intensities.empty()
            : false;
    const ImGuiStyle& style = ImGui::GetStyle();
    if(!(ceiling_empty || primary_kernel_empty || secondary_kernel_empty))
    {
        float filter_width =
            (ImGui::GetContentRegionAvail().x -
             style.ItemSpacing.x * (m_mode == AllKernels ? 3.0f : 2.0f)) /
            (m_mode == AllKernels ? 4.0f : 3.0f);
        FilterCombo("Compute Peak", filter_width, style, m_filters_ceiling_compute,
                    m_custom_ceiling_compute, m_active_filter_ceiling_compute);
        ImGui::SameLine();
        FilterCombo("Bandwidth Peak", filter_width, style, m_filters_ceiling_bandwidth,
                    m_custom_ceiling_bandwidth, m_active_filter_ceiling_bandwidth);
        if(m_mode == AllKernels)
        {
            ImGui::SameLine();
            FilterCombo("Kernel", filter_width, style, m_filters_intensity_kernel,
                        m_custom_intensity, m_active_filter_intensity_kernel);
        }
        ImGui::SameLine();
        FilterCombo("Kernel Bandwidth", filter_width, style,
                    m_filters_intensity_bandwidth, m_custom_intensity,
                    m_active_filter_intensity_bandwidth);
    }
    ImGui::BeginChild("roofline");
    const ImVec2       region     = ImGui::GetContentRegionAvail();
    const ImPlotStyle& plot_style = ImPlot::GetStyle();
    if(ceiling_empty || primary_kernel_empty || secondary_kernel_empty)
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
        const char* hint = HINT_EMPTY_GENERIC;
        if(m_mode == Compare)
        {
            if(m_requested_secondary_workload_id ==
                   ComputeSelection::INVALID_SELECTION_ID ||
               m_requested_kernel_secondary_id == ComputeSelection::INVALID_SELECTION_ID)
            {
                hint = HINT_EMPTY_COMPARE_INIT;
            }
            else if(primary_kernel_empty)
            {
                hint = HINT_EMPTY_PRIMARY_KERNEL;
            }
            else if(secondary_kernel_empty)
            {
                hint = HINT_EMPTY_SECONDARY_KERNEL;
            }
        }
        ImGui::SetCursorPos((region - ImGui::CalcTextSize(hint)) * 0.5f);
        ImGui::TextDisabled("%s", hint);
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
        ImGui::PushID((*m_ceiling_source)->id);
        bool   menus_outside = (m_menus_placement == Outside) && m_show_menus;
        bool   plot_hovered  = false;
        ImVec2 plot_pos;
        ImVec2 plot_size;

        if(ImPlot::BeginPlot("plot", ImVec2(menus_outside ? 0.75f * region.x : -1, -1),
                             ImPlotFlags_NoTitle | ImPlotFlags_NoFrame |
                                 ImPlotFlags_NoLegend | ImPlotFlags_NoMenus |
                                 ImPlotFlags_Crosshairs))
        {
            ImPlotAxisFlags axis_flags =
                ImPlotAxisFlags_NoSideSwitch | ImPlotAxisFlags_NoHighlight;
            if(!m_plot_nav_enabled)
            {
                axis_flags |= ImPlotAxisFlags_Lock;
            }
            ImPlot::SetupAxis(ImAxis_X1, "Arithmetic Intensity (FLOP/Byte)", axis_flags);
            ImPlot::SetupAxis(ImAxis_Y1, "Performance (GFLOP/s)", axis_flags);

            ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Log10);
            ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
            ImPlotPoint axis_min(std::min(m_bounding_box_ceiling.first.x,
                                          m_bounding_box_intensity.first.x) /
                                     10.0,
                                 std::min(m_bounding_box_ceiling.first.y,
                                          m_bounding_box_intensity.first.y) /
                                     10.0);
            ImPlotPoint axis_max(std::max(m_bounding_box_ceiling.second.x,
                                          m_bounding_box_intensity.second.x) *
                                     10.0,
                                 std::max(m_bounding_box_ceiling.second.y,
                                          m_bounding_box_intensity.second.y) *
                                     10.0);
            ImPlot::SetupAxisLimits(ImAxis_X1, axis_min.x, axis_max.x);
            ImPlot::SetupAxisLimits(ImAxis_Y1, axis_min.y, axis_max.y);
            ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, axis_min.x, axis_max.x);
            ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, axis_min.y, axis_max.y);
            ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 0.0f);
            ImPlot::PlotScatter("tl_hint", &axis_min.x, &axis_max.y, 1);
            ImPlot::PlotScatter("br_hint", &axis_max.x, &axis_min.y, 1);
            ImPlot::PopStyleVar();
            plot_hovered = ImPlot::IsPlotHovered();
            plot_pos     = ImPlot::GetPlotPos();
            plot_size    = ImPlot::GetPlotSize();
            PlotHoverIdx();
            for(size_t i = 0; i < m_items.size(); i++)
            {
                if(ItemValid(m_items[i]) && m_items[i].visible[ItemModel::Visible::Plot])
                {
                    ImGui::PushID(static_cast<int>(i));
                    bool hovered = plot_hovered && m_hovered_item_idx &&
                                   m_hovered_item_idx.value() == i;
                    switch(m_items[i].type)
                    {
                        case ItemModel::Type::CeilingCompute:
                        {
                            ShadeInfo shade_info = { i, m_items, m_global_shade_ctx };
                            // Mirror of the bandwidth bands. The floor is the highest
                            // diagonal still below this ceiling, so it saw-tooths down
                            // as each diagonal climbs past the ceiling, and flattens
                            // onto the next lower compute ceiling once none are left.
                            // Breakpoints are needed at both crossing families, or the
                            // fill chords across the max() corner and leaves a wedge.
                            const WorkloadInfo::Roofline::Ceiling* shade_ceiling =
                                m_items[i].info.ceiling;
                            double shade_floor_compute = 0.0;
                            for(const ItemModel* lower :
                                m_global_shade_ctx.ceiling_compute_visible_sorted)
                            {
                                if(lower->info.ceiling->throughput <
                                   shade_ceiling->throughput)
                                {
                                    shade_floor_compute = lower->info.ceiling->throughput;
                                }
                            }
                            std::array<double, ShadeInfo::MaxBreakpoints> breakpoints;
                            size_t breakpoint_count = 0;
                            double shade_left_x     = DBL_MAX;
                            breakpoints[breakpoint_count++] =
                                shade_ceiling->position.p2.x;
                            for(const ItemModel* diagonal :
                                m_global_shade_ctx.ceiling_bandwidth_visible_sorted)
                            {
                                if(breakpoint_count + 2 <= ShadeInfo::MaxBreakpoints)
                                {
                                    breakpoints[breakpoint_count++] =
                                        shade_ceiling->throughput /
                                        diagonal->info.ceiling->throughput;
                                    shade_left_x = std::min(
                                        shade_left_x, breakpoints[breakpoint_count - 1]);
                                    if(shade_floor_compute > 0.0)
                                    {
                                        breakpoints[breakpoint_count++] =
                                            shade_floor_compute /
                                            diagonal->info.ceiling->throughput;
                                    }
                                }
                            }
                            std::sort(breakpoints.begin(),
                                      breakpoints.begin() + breakpoint_count);
                            shade_info.sample_count = 0;
                            for(size_t b = 0; b < breakpoint_count; b++)
                            {
                                if(breakpoints[b] >= shade_left_x &&
                                   breakpoints[b] <= shade_ceiling->position.p2.x &&
                                   shade_info.sample_count + 2 <= ShadeInfo::MaxSamples)
                                {
                                    // Inclusive probe first: the floor steps down as a
                                    // diagonal climbs past the ceiling, so the pair must
                                    // close the run before reopening lower.
                                    for(int side = 1; side >= -1; side -= 2)
                                    {
                                        double probe_y =
                                            shade_ceiling->throughput *
                                            (1.0 + side * SHADE_STEP_EPSILON);
                                        double floor_y = shade_floor_compute;
                                        for(const ItemModel* diagonal :
                                            m_global_shade_ctx
                                                .ceiling_bandwidth_visible_sorted)
                                        {
                                            if(diagonal->info.ceiling->throughput *
                                                   breakpoints[b] <=
                                               probe_y)
                                            {
                                                floor_y = std::max(
                                                    floor_y,
                                                    diagonal->info.ceiling->throughput *
                                                        breakpoints[b]);
                                            }
                                        }
                                        shade_info.sample_x[shade_info.sample_count] =
                                            breakpoints[b];
                                        shade_info
                                            .sample_floor_y[shade_info.sample_count] =
                                            std::min(floor_y, shade_ceiling->throughput);
                                        shade_info.sample_count++;
                                    }
                                }
                            }
                            if(shade_info.sample_count > 0)
                            {
                                ImPlot::SetNextFillStyle(
                                    ImGui::ColorConvertU32ToFloat4(
                                        ItemColor(m_items[i], hovered, false)),
                                    0.25f);
                                ImPlot::PlotShadedG(
                                    "shade",
                                    [](int idx, void* user_data) -> ImPlotPoint {
                                        const ShadeInfo* info =
                                            static_cast<const ShadeInfo*>(user_data);
                                        ImPlotPoint point(-1.0, -1.0);
                                        if(info)
                                        {
                                            point.x = info->sample_x[idx];
                                            point.y = info->items[info->item_idx]
                                                          .info.ceiling->throughput;
                                        }
                                        return point;
                                    },
                                    (void*) &shade_info,
                                    [](int idx, void* user_data) -> ImPlotPoint {
                                        const ShadeInfo* info =
                                            static_cast<const ShadeInfo*>(user_data);
                                        ImPlotPoint point(-1.0, -1.0);
                                        if(info)
                                        {
                                            point.x = info->sample_x[idx];
                                            point.y = info->sample_floor_y[idx];
                                        }
                                        return point;
                                    },
                                    (void*) &shade_info,
                                    static_cast<int>(shade_info.sample_count),
                                    ImPlotItemFlags_NoFit);
                            }
                            ImPlot::SetNextLineStyle(
                                ImGui::ColorConvertU32ToFloat4(
                                    ItemColor(m_items[i], hovered, false)),
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
                                (void*) &m_items[i].info.ceiling->position, 2,
                                ImPlotItemFlags_NoFit);
                            if(m_ceiling_labels)
                            {
                                if(m_items[i].type == ItemModel::Type::CeilingCompute &&
                                   m_items[i].info.ceiling->position.p1.x <
                                       ImPlot::GetPlotLimits().X.Max)
                                {
                                    ImPlot::Annotation(
                                        ImPlot::GetPlotLimits().X.Max,
                                        m_items[i].info.ceiling->position.p1.y,
                                        ImPlot::GetLastItemColor(), ImVec2(-1.0f, 0.0f),
                                        false, m_items[i].label.c_str());
                                }
                                else if(m_items[i].type ==
                                            ItemModel::Type::CeilingBandwidth &&
                                        m_items[i].info.ceiling->position.p2.x >
                                            ImPlot::GetPlotLimits().X.Min)
                                {
                                    ImPlot::Annotation(
                                        ImPlot::GetPlotLimits().X.Min,
                                        m_items[i].info.ceiling->throughput *
                                            ImPlot::GetPlotLimits().X.Min,
                                        ImPlot::GetLastItemColor(), ImVec2(1.0f, 0.0f),
                                        false, m_items[i].label.c_str());
                                }
                            }
                            break;
                        }
                        case ItemModel::Type::CeilingBandwidth:
                        {
                            ShadeInfo shade_info = { i, m_items, m_global_shade_ctx };
                            // The floor is the highest line still below this diagonal:
                            // max(lower diagonal, highest compute ceiling under it).
                            // Breakpoints are needed where a compute ceiling overtakes
                            // this diagonal AND where it crosses the one below, or the
                            // fill chords across the max() corner and leaves a wedge.
                            const WorkloadInfo::Roofline::Ceiling* shade_ceiling =
                                m_items[i].info.ceiling;
                            double shade_floor_throughput = 0.0;
                            for(const ItemModel* lower :
                                m_global_shade_ctx.ceiling_bandwidth_visible_sorted)
                            {
                                if(lower->info.ceiling->throughput <
                                   shade_ceiling->throughput)
                                {
                                    shade_floor_throughput =
                                        lower->info.ceiling->throughput;
                                }
                            }
                            std::array<double, ShadeInfo::MaxBreakpoints> breakpoints;
                            size_t breakpoint_count = 0;
                            breakpoints[breakpoint_count++] =
                                shade_ceiling->position.p1.x;
                            breakpoints[breakpoint_count++] =
                                shade_ceiling->position.p2.x;
                            for(const ItemModel* compute :
                                m_global_shade_ctx.ceiling_compute_visible_sorted)
                            {
                                if(breakpoint_count + 2 <= ShadeInfo::MaxBreakpoints)
                                {
                                    breakpoints[breakpoint_count++] =
                                        compute->info.ceiling->throughput /
                                        shade_ceiling->throughput;
                                    if(shade_floor_throughput > 0.0)
                                    {
                                        breakpoints[breakpoint_count++] =
                                            compute->info.ceiling->throughput /
                                            shade_floor_throughput;
                                    }
                                }
                            }
                            std::sort(breakpoints.begin(),
                                      breakpoints.begin() + breakpoint_count);
                            shade_info.sample_count = 0;
                            for(size_t b = 0; b < breakpoint_count; b++)
                            {
                                if(breakpoints[b] >= shade_ceiling->position.p1.x &&
                                   breakpoints[b] <= shade_ceiling->position.p2.x &&
                                   shade_info.sample_count + 2 <= ShadeInfo::MaxSamples)
                                {
                                    double ceiling_y =
                                        shade_ceiling->throughput * breakpoints[b];
                                    // Evaluate either side of the breakpoint so a step
                                    // lands on two samples sharing an x, and renders
                                    // vertically rather than as a ramp.
                                    for(int side = -1; side <= 1; side += 2)
                                    {
                                        double probe_y =
                                            ceiling_y * (1.0 + side * SHADE_STEP_EPSILON);
                                        double floor_y =
                                            shade_floor_throughput * breakpoints[b];
                                        for(const ItemModel* compute :
                                            m_global_shade_ctx
                                                .ceiling_compute_visible_sorted)
                                        {
                                            if(compute->info.ceiling->throughput <=
                                               probe_y)
                                            {
                                                floor_y = std::max(
                                                    floor_y,
                                                    compute->info.ceiling->throughput);
                                            }
                                        }
                                        shade_info.sample_x[shade_info.sample_count] =
                                            breakpoints[b];
                                        shade_info
                                            .sample_floor_y[shade_info.sample_count] =
                                            std::min(floor_y, ceiling_y);
                                        shade_info.sample_count++;
                                    }
                                }
                            }
                            ImPlot::SetNextFillStyle(
                                ImGui::ColorConvertU32ToFloat4(
                                    ItemColor(m_items[i], hovered, false)),
                                0.25f);
                            ImPlot::PlotShadedG(
                                "shade",
                                [](int idx, void* user_data) -> ImPlotPoint {
                                    const ShadeInfo* info =
                                        static_cast<const ShadeInfo*>(user_data);
                                    ImPlotPoint point(-1.0, -1.0);
                                    if(info)
                                    {
                                        point.x = info->sample_x[idx];
                                        point.y = info->items[info->item_idx]
                                                      .info.ceiling->throughput *
                                                  point.x;
                                    }
                                    return point;
                                },
                                (void*) &shade_info,
                                [](int idx, void* user_data) -> ImPlotPoint {
                                    const ShadeInfo* info =
                                        static_cast<const ShadeInfo*>(user_data);
                                    ImPlotPoint point(-1.0, -1.0);
                                    if(info)
                                    {
                                        point.x = info->sample_x[idx];
                                        point.y = info->sample_floor_y[idx];
                                    }
                                    return point;
                                },
                                (void*) &shade_info,
                                static_cast<int>(shade_info.sample_count),
                                ImPlotItemFlags_NoFit);
                            ImPlot::SetNextLineStyle(
                                ImGui::ColorConvertU32ToFloat4(
                                    ItemColor(m_items[i], hovered, false)),
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
                                (void*) &m_items[i].info.ceiling->position, 2,
                                ImPlotItemFlags_NoFit);
                            if(m_ceiling_labels)
                            {
                                if(m_items[i].type == ItemModel::Type::CeilingCompute &&
                                   m_items[i].info.ceiling->position.p1.x <
                                       ImPlot::GetPlotLimits().X.Max)
                                {
                                    ImPlot::Annotation(
                                        ImPlot::GetPlotLimits().X.Max,
                                        m_items[i].info.ceiling->position.p1.y,
                                        ImPlot::GetLastItemColor(), ImVec2(-1.0f, 0.0f),
                                        false, m_items[i].label.c_str());
                                }
                                else if(m_items[i].type ==
                                            ItemModel::Type::CeilingBandwidth &&
                                        m_items[i].info.ceiling->position.p2.x >
                                            ImPlot::GetPlotLimits().X.Min)
                                {
                                    ImPlot::Annotation(
                                        ImPlot::GetPlotLimits().X.Min,
                                        m_items[i].info.ceiling->throughput *
                                            ImPlot::GetPlotLimits().X.Min,
                                        ImPlot::GetLastItemColor(), ImVec2(1.0f, 0.0f),
                                        false, m_items[i].label.c_str());
                                }
                            }
                            break;
                        }
                        case ItemModel::Type::Intensity:
                        {
                            ImVec4 marker_color = ImGui::ColorConvertU32ToFloat4(
                                ItemColor(m_items[i], hovered, false));
                            ImPlot::SetNextMarkerStyle(
                                DISPLAY_PROPS_KERNEL_INTENSITY[m_items[i]
                                                                   .subtype.intensity]
                                    .second,
                                plot_style.MarkerSize +
                                    (m_scale_intensity && m_mode == AllKernels
                                         ? m_items[i].weight
                                         : 0.0f) *
                                        2.0f * plot_style.MarkerSize +
                                    (hovered ? plot_style.MarkerSize : 0.0f),
                                marker_color, IMPLOT_AUTO, marker_color);
                            ImPlot::PlotScatter("",
                                                &m_items[i].info.intensity->position.x,
                                                &m_items[i].info.intensity->position.y, 1,
                                                ImPlotItemFlags_NoFit);
                            break;
                        }
                        case ItemModel::Type::IntensityDelta:
                        {
                            ImVec2 baseline_pos = ImPlot::PlotToPixels(
                                ImPlotPoint(m_items[i].info.delta.baseline->position.x,
                                            m_items[i].info.delta.baseline->position.y));
                            ImVec2 target_pos = ImPlot::PlotToPixels(
                                ImPlotPoint(m_items[i].info.delta.target->position.x,
                                            m_items[i].info.delta.target->position.y));
                            ImVec2 direction = target_pos - baseline_pos;
                            float  length    = std::sqrt(direction.x * direction.x +
                                                         direction.y * direction.y);
                            // Draw arrow...
                            float radius = (plot_style.MarkerSize +
                                            (m_line_thickness - LINE_THICKNESS_DEFAULT) +
                                            (hovered ? HOVER_LINE_WEIGHT_BOOST : 0.0f)) *
                                           IMPLOT_MARKER_SCALE_FACTOR;
                            if(length > 0.0f)
                            {
                                direction      = direction / length;
                                ImU32 color    = ItemColor(m_items[i], hovered, false);
                                float dash_end = length;
                                ImPlot::PushPlotClipRect();
                                if(length > 3.0f * radius)
                                {
                                    ImVec2 normal = ImVec2(-direction.y, direction.x);
                                    ImVec2 center =
                                        target_pos -
                                        direction * (plot_style.MarkerSize + radius);
                                    ImPlot::GetPlotDrawList()->AddTriangleFilled(
                                        center + direction * radius,
                                        center + (normal * IMPLOT_MARKER_BASE -
                                                  direction * 0.5f) *
                                                     radius,
                                        center - (normal * IMPLOT_MARKER_BASE +
                                                  direction * 0.5f) *
                                                     radius,
                                        color);
                                    // Stop short of the arrow so dashes do not show
                                    // through it.
                                    dash_end =
                                        length - plot_style.MarkerSize - 1.5f * radius;
                                }
                                // Determine clip region...
                                // (Since dashed line is manually
                                // draw, we need to manually clip inside plot bounds)
                                ImVec2 plot_min = ImPlot::GetPlotPos();
                                ImVec2 plot_max = plot_min + ImPlot::GetPlotSize();
                                float x_low = (plot_min.x - baseline_pos.x) / direction.x;
                                float x_high =
                                    (plot_max.x - baseline_pos.x) / direction.x;
                                float y_low = (plot_min.y - baseline_pos.y) / direction.y;
                                float y_high =
                                    (plot_max.y - baseline_pos.y) / direction.y;
                                float dash_enter =
                                    std::max(0.0f, std::max(std::min(x_low, x_high),
                                                            std::min(y_low, y_high)));
                                float dash_exit =
                                    std::min(dash_end, std::min(std::max(x_low, x_high),
                                                                std::max(y_low, y_high)));
                                float dash_begin =
                                    std::floor(dash_enter / (DASHED_LINE_SEGMENT_LENGTH +
                                                             DASHED_LINE_SEGMENT_GAP)) *
                                    (DASHED_LINE_SEGMENT_LENGTH +
                                     DASHED_LINE_SEGMENT_GAP);
                                float dash_limit = dash_exit - dash_begin;
                                // Draw dashed line...
                                ImVec2 dash_origin =
                                    baseline_pos + direction * dash_begin;
                                int dash_count =
                                    dash_limit > 0.0f
                                        ? static_cast<int>(std::min(
                                              dash_limit / (DASHED_LINE_SEGMENT_LENGTH +
                                                            DASHED_LINE_SEGMENT_GAP) +
                                                  1.0f,
                                              DASHED_LINE_SEGMENT_CAP))
                                        : 0;
                                for(int dash = 0; dash < dash_count; dash++)
                                {
                                    ImPlot::GetPlotDrawList()->AddLine(
                                        dash_origin +
                                            direction *
                                                (dash * (DASHED_LINE_SEGMENT_LENGTH +
                                                         DASHED_LINE_SEGMENT_GAP)),
                                        dash_origin +
                                            direction *
                                                std::min(
                                                    dash * (DASHED_LINE_SEGMENT_LENGTH +
                                                            DASHED_LINE_SEGMENT_GAP) +
                                                        DASHED_LINE_SEGMENT_LENGTH,
                                                    dash_limit),
                                        color,
                                        hovered
                                            ? m_line_thickness + HOVER_LINE_WEIGHT_BOOST
                                            : m_line_thickness);
                                }
                                ImPlot::PopPlotClipRect();
                            }
                            break;
                        }
                    }
                    if(hovered)
                    {
                        if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                        {
                            ApplyItemFilter(m_items[i]);
                        }
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
                                        ItemColor(m_items[i], hovered, false));
                                    ImGui::TextUnformatted(m_items[i].label.c_str());
                                    std::isfinite(m_items[i].info.ceiling->throughput)
                                        ? ImGui::Text(
                                              "%.0f GFLOP/s",
                                              std::round(
                                                  m_items[i].info.ceiling->throughput))
                                        : ImGui::TextUnformatted("N/A");
                                    break;
                                }
                                case ItemModel::Type::CeilingBandwidth:
                                {
                                    ImGui::GetWindowDrawList()->AddRectFilled(
                                        ImGui::GetCursorScreenPos(),
                                        ImGui::GetCursorScreenPos() +
                                            ImGui::CalcTextSize(m_items[i].label.c_str()),
                                        ItemColor(m_items[i], hovered, false));
                                    ImGui::TextUnformatted(m_items[i].label.c_str());
                                    std::isfinite(m_items[i].info.ceiling->throughput)
                                        ? ImGui::Text(
                                              "%.0f GB/s",
                                              std::round(
                                                  m_items[i].info.ceiling->throughput))
                                        : ImGui::TextUnformatted("N/A");
                                    break;
                                }
                                case ItemModel::Type::Intensity:
                                {
                                    ImGui::BeginGroup();
                                    ImGui::GetWindowDrawList()->AddRectFilled(
                                        ImGui::GetCursorScreenPos(),
                                        ImGui::GetCursorScreenPos() +
                                            ImGui::CalcTextSize(
                                                DISPLAY_PROPS_KERNEL_INTENSITY
                                                    [m_items[i].subtype.intensity]
                                                        .first),
                                        ItemColor(m_items[i], hovered, false));
                                    ImGui::TextUnformatted(
                                        DISPLAY_PROPS_KERNEL_INTENSITY
                                            [m_items[i].subtype.intensity]
                                                .first);
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
                                    std::isfinite(m_items[i].info.intensity->position.x)
                                        ? ImGui::Text(
                                              "Arithmetic Intensity: %f FLOP/Byte",
                                              m_items[i].info.intensity->position.x)
                                        : ImGui::TextUnformatted(
                                              "Arithmetic Intensity: N/A");
                                    std::isfinite(m_items[i].info.intensity->position.y)
                                        ? ImGui::Text(
                                              "Performance: %f GFLOP/s",
                                              m_items[i].info.intensity->position.y)
                                        : ImGui::TextUnformatted("Performance: N/A");
                                    ImGui::EndGroup();
                                    ImGui::SetCursorPos(reserved_pos);
                                    ElidedText(
                                        m_items[i].parent_info.kernel->name.c_str(),
                                        ImGui::GetItemRectSize().x);
                                    break;
                                }
                                case ItemModel::Type::IntensityDelta:
                                {
                                    const uint64_t& target_invocations =
                                        m_items[i]
                                            .parent_info.delta.target->dispatch_metrics
                                                [KernelInfo::InvocationCount];
                                    const uint64_t& baseline_invocations =
                                        m_items[i]
                                            .parent_info.delta.baseline->dispatch_metrics
                                                [KernelInfo::InvocationCount];
                                    const uint64_t& target_duration =
                                        m_items[i]
                                            .parent_info.delta.target
                                            ->dispatch_metrics[KernelInfo::DurationTotal];
                                    const uint64_t& baseline_duration =
                                        m_items[i]
                                            .parent_info.delta.baseline
                                            ->dispatch_metrics[KernelInfo::DurationTotal];
                                    const double& target_ai =
                                        m_items[i].info.delta.target->position.x;
                                    const double& baseline_ai =
                                        m_items[i].info.delta.baseline->position.x;
                                    const double& target_perf =
                                        m_items[i].info.delta.target->position.y;
                                    const double& baseline_perf =
                                        m_items[i].info.delta.baseline->position.y;
                                    ImGui::GetWindowDrawList()->AddRectFilled(
                                        ImGui::GetCursorScreenPos(),
                                        ImGui::GetCursorScreenPos() +
                                            ImGui::CalcTextSize(m_items[i].label.c_str()),
                                        ItemColor(m_items[i], hovered, false));
                                    ImGui::TextUnformatted(m_items[i].label.c_str());
                                    ImGui::Text(
                                        "%s Invocation(s): %+lld (%+.1f%%)", DELTA,
                                        static_cast<long long>(target_invocations) -
                                            static_cast<long long>(baseline_invocations),
                                        baseline_invocations != 0
                                            ? 100.0 *
                                                  (static_cast<double>(
                                                       target_invocations) -
                                                   static_cast<double>(
                                                       baseline_invocations)) /
                                                  static_cast<double>(
                                                      baseline_invocations)
                                            : 0.0);
                                    ImGui::Text(
                                        "%s Duration: %s%s (%+.1f%%)", DELTA,
                                        target_duration == baseline_duration  ? ""
                                        : target_duration > baseline_duration ? "+"
                                                                              : "-",
                                        nanosecond_to_formatted_str(
                                            static_cast<double>(
                                                target_duration > baseline_duration
                                                    ? target_duration - baseline_duration
                                                    : baseline_duration -
                                                          target_duration),
                                            m_settings.GetUserSettings()
                                                .unit_settings.time_format,
                                            true)
                                            .c_str(),
                                        baseline_duration != 0
                                            ? 100.0 *
                                                  (static_cast<double>(target_duration) -
                                                   static_cast<double>(
                                                       baseline_duration)) /
                                                  static_cast<double>(baseline_duration)
                                            : 0.0);
                                    std::isfinite(baseline_ai) && std::isfinite(target_ai)
                                        ? ImGui::Text(
                                              "%s Arithmetic Intensity: %+f FLOP/Byte "
                                              "(%+.1f%%)",
                                              DELTA, target_ai - baseline_ai,
                                              baseline_ai != 0.0
                                                  ? 100.0 * (target_ai - baseline_ai) /
                                                        baseline_ai
                                                  : 0.0)
                                        : ImGui::Text("%s Arithmetic Intensity: N/A",
                                                      DELTA);
                                    std::isfinite(baseline_perf) &&
                                            std::isfinite(target_perf)
                                        ? ImGui::Text(
                                              "%s Performance: %+f GFLOP/s (%+.1f%%)",
                                              DELTA, target_perf - baseline_perf,
                                              baseline_perf != 0.0
                                                  ? 100.0 *
                                                        (target_perf - baseline_perf) /
                                                        baseline_perf
                                                  : 0.0)
                                        : ImGui::Text("%s Performance: N/A", DELTA);
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
            ImPlot::EndPlot();
        }
        ImGui::PopID();
        if(!m_plot_nav_enabled && plot_hovered)
        {
            ImVec2 hint_size = ImGui::CalcTextSize(HINT_FOCUS);
            ImVec2 hint_pos  = plot_pos + ImVec2(plot_size.x - hint_size.x, 0.0f) * 0.5f +
                               ImVec2(0.0f, plot_style.PlotPadding.y);
            ImGui::GetWindowDrawList()->AddText(
                hint_pos, ImGui::GetColorU32(style.Colors[ImGuiCol_TextDisabled]),
                HINT_FOCUS);
        }
        bool menus_item_hovered = false;
        RenderMenus(region, plot_pos, plot_size, style, plot_style, menus_item_hovered,
                    plot_hovered);
        if(!m_plot_nav_enabled && plot_hovered &&
           ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_plot_nav_enabled = true;
        }
        else if(m_plot_nav_enabled &&
                (ImGui::IsKeyPressed(ImGuiKey_Escape) ||
                 (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !plot_hovered)))
        {
            m_plot_nav_enabled = false;
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
    m_requested_primary_workload_id = id;
    m_workload_changed              = true;
}

void
Roofline::SetKernel(uint32_t id)
{
    m_requested_primary_kernel_id = id;
    m_kernel_changed              = true;
}

void
Roofline::SetCompareTarget(uint32_t workload_id, uint32_t kernel_id)
{
    m_requested_secondary_workload_id = workload_id;
    m_requested_kernel_secondary_id   = kernel_id;
    m_workload_changed                = true;
    m_kernel_changed                  = true;
}

void
Roofline::UpdateCeilings(const WorkloadInfo* workload)
{
    for(FilterModel& filter : m_filters_ceiling_compute)
    {
        filter.item_idx.clear();
    }
    for(FilterModel& filter : m_filters_ceiling_bandwidth)
    {
        filter.item_idx.clear();
    }
    m_bounding_box_ceiling = { { DBL_MAX, DBL_MAX }, { -DBL_MAX, -DBL_MAX } };
    ROCPROFVIS_ASSERT(__KRPVControllerRooflineCeilingComputeTypeLast +
                          __KRPVControllerRooflineCeilingBandwidthTypeLast <=
                      m_items.size());
    for(size_t i = 0; i < __KRPVControllerRooflineCeilingComputeTypeLast +
                              __KRPVControllerRooflineCeilingBandwidthTypeLast;
        i++)
    {
        switch(m_items[i].type)
        {
            case ItemModel::Type::CeilingCompute:
            {
                const WorkloadInfo::Roofline::Ceiling* ceiling = nullptr;
                if(workload->roofline.ceiling_compute.count(m_items[i].subtype.compute) >
                       0 &&
                   !workload->roofline.ceiling_compute.at(m_items[i].subtype.compute)
                        .empty())
                {
                    ceiling =
                        &workload->roofline.ceiling_compute.at(m_items[i].subtype.compute)
                             .begin()
                             ->second;
                }
                if(ceiling && std::isfinite(ceiling->position.p1.x) &&
                   std::isfinite(ceiling->position.p1.y) &&
                   std::isfinite(ceiling->position.p2.x) &&
                   std::isfinite(ceiling->position.p2.y) &&
                   std::isfinite(ceiling->throughput))
                {
                    m_items[i].info.ceiling         = ceiling;
                    m_items[i].parent_info.workload = workload;
                    FilterModel::ComputeType filter_type =
                        FilterComputeType(m_items[i].subtype.compute);
                    if(filter_type != FilterModel::ComputeType::ComputeTypeUnknown)
                    {
                        m_items[i].filter_idx[FilterModel::Type::CeilingCompute] =
                            static_cast<size_t>(filter_type);
                        m_filters_ceiling_compute[filter_type].item_idx.insert(i);
                        m_filters_ceiling_compute
                            [FilterModel::ComputeType::ComputeTypeAll]
                                .item_idx.insert(i);
                    }
                    m_bounding_box_ceiling.second.y =
                        std::max(m_bounding_box_ceiling.second.y,
                                 m_items[i].info.ceiling->position.p1.y);
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
                const WorkloadInfo::Roofline::Ceiling* ceiling = nullptr;
                if(workload->roofline.ceiling_bandwidth.count(
                       m_items[i].subtype.bandwidth) > 0 &&
                   !workload->roofline.ceiling_bandwidth.at(m_items[i].subtype.bandwidth)
                        .empty())
                {
                    ceiling = &workload->roofline.ceiling_bandwidth
                                   .at(m_items[i].subtype.bandwidth)
                                   .begin()
                                   ->second;
                }
                if(ceiling && std::isfinite(ceiling->position.p1.x) &&
                   std::isfinite(ceiling->position.p1.y) &&
                   std::isfinite(ceiling->position.p2.x) &&
                   std::isfinite(ceiling->position.p2.y) &&
                   std::isfinite(ceiling->throughput))
                {
                    m_items[i].info.ceiling         = ceiling;
                    m_items[i].parent_info.workload = workload;
                    FilterModel::BandwidthType filter_type =
                        FilterBandwidthType(m_items[i].subtype.bandwidth);
                    if(filter_type != FilterModel::BandwidthType::BandwidthTypeUnknown)
                    {
                        m_items[i].filter_idx[FilterModel::Type::CeilingBandwidth] =
                            static_cast<size_t>(filter_type);
                        m_filters_ceiling_bandwidth[filter_type].item_idx.insert(i);
                        m_filters_ceiling_bandwidth
                            [FilterModel::BandwidthType::BandwidthTypeAll]
                                .item_idx.insert(i);
                    }
                    m_bounding_box_ceiling.first.y =
                        std::min(m_bounding_box_ceiling.first.y,
                                 m_items[i].info.ceiling->throughput * MIN_X);
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
}

void
Roofline::UpdateIntensities(const WorkloadInfo*                   workload,
                            const std::vector<const KernelInfo*>& kernels, bool append)
{
    if(!append)
    {
        m_items.resize(__KRPVControllerRooflineCeilingComputeTypeLast +
                       __KRPVControllerRooflineCeilingBandwidthTypeLast);
        m_filters_intensity_kernel.resize(1);
        m_filters_intensity_kernel[0].item_idx.clear();
        for(FilterModel& filter : m_filters_intensity_bandwidth)
        {
            filter.item_idx.clear();
        }
        m_bounding_box_intensity         = { { MIN_X * 10.0, DBL_MAX },
                                             { MAX_X / 10.0, -DBL_MAX } };
        m_active_filter_intensity_kernel = 0;
    }
    uint64_t kernel_duration_scale = 0;
    for(const KernelInfo* kernel : workload->ordered_kernels)
    {
        kernel_duration_scale = std::max(
            kernel_duration_scale, kernel->dispatch_metrics[KernelInfo::DurationTotal]);
    }
    ItemModel::SubType    model_subtype;
    ItemModel::Info       model_info;
    ItemModel::ParentInfo model_parent_info;
    for(const KernelInfo* kernel : kernels)
    {
        model_parent_info.kernel = kernel;
        std::unordered_set<size_t> kernel_filter_items;
        for(const std::pair<const rocprofvis_controller_roofline_kernel_intensity_type_t,
                            KernelInfo::Roofline::Intensity>& intensity :
            kernel->roofline.intensities)
        {
            model_subtype.intensity = intensity.second.type;
            model_info.intensity    = &intensity.second;
            FilterModel::BandwidthType filter_type =
                FilterBandwidthType(model_subtype.intensity);
            if(filter_type != FilterModel::BandwidthType::BandwidthTypeUnknown &&
               std::isfinite(intensity.second.position.x) &&
               std::isfinite(intensity.second.position.y))
            {
                m_items.emplace_back(ItemModel{
                    ItemModel::Intensity,
                    model_subtype,
                    model_info,
                    model_parent_info,
                    { { FilterModel::Type::IntensityKernel,
                        m_filters_intensity_kernel.size() },
                      { FilterModel::Type::IntensityBandwidth,
                        static_cast<size_t>(filter_type) } },
                    std::bitset<ItemModel::Visible::Count>("10"),
                    std::string(
                        DISPLAY_PROPS_KERNEL_INTENSITY[intensity.second.type].first) +
                        ": " + kernel->name,
                    kernel_duration_scale > 0
                        ? static_cast<float>(
                              static_cast<double>(
                                  kernel->dispatch_metrics[KernelInfo::DurationTotal]) /
                              static_cast<double>(kernel_duration_scale))
                        : KERNEL_MARKER_WEIGHT_DEFAULT });
                m_bounding_box_intensity = { { std::min(m_bounding_box_intensity.first.x,
                                                        intensity.second.position.x),
                                               std::min(m_bounding_box_intensity.first.y,
                                                        intensity.second.position.y) },
                                             { std::max(m_bounding_box_intensity.second.x,
                                                        intensity.second.position.x),
                                               std::max(m_bounding_box_intensity.second.y,
                                                        intensity.second.position.y) } };
                m_filters_intensity_bandwidth[filter_type].item_idx.insert(
                    m_items.size() - 1);
                m_filters_intensity_bandwidth
                    [FilterModel::BandwidthType::BandwidthTypeAll]
                        .item_idx.insert(m_items.size() - 1);
                kernel_filter_items.insert(m_items.size() - 1);
                m_filters_intensity_kernel[0].item_idx.insert(m_items.size() - 1);
            }
        }
        if(kernel_filter_items.size())
        {
            m_filters_intensity_kernel.emplace_back(
                FilterModel{ kernel->name.c_str(), std::move(kernel_filter_items) });
        }
    }
}

void
Roofline::UpdateDeltas()
{
    ItemModel::SubType    sub_type;
    ItemModel::Info       model_info;
    ItemModel::ParentInfo parent_info;
    for(const std::pair<const rocprofvis_controller_roofline_kernel_intensity_type_t,
                        KernelInfo::Roofline::Intensity>& intensity :
        m_kernel_primary->roofline.intensities)
    {
        if(m_kernel_secondary->roofline.intensities.count(intensity.first) &&
           !(m_kernel_secondary->roofline.intensities.at(intensity.first).position ==
             intensity.second.position))
        {
            sub_type.intensity = intensity.first;
            model_info.delta   = { &intensity.second,
                                   &m_kernel_secondary->roofline.intensities.at(
                                       intensity.first) };
            parent_info.delta  = { m_kernel_primary, m_kernel_secondary };
            FilterModel::BandwidthType filter_type =
                FilterBandwidthType(sub_type.intensity);
            if(filter_type != FilterModel::BandwidthType::BandwidthTypeUnknown &&
               std::isfinite(model_info.delta.baseline->position.x) &&
               std::isfinite(model_info.delta.baseline->position.y) &&
               std::isfinite(model_info.delta.target->position.x) &&
               std::isfinite(model_info.delta.target->position.y))
            {
                m_items.emplace_back(ItemModel{
                    ItemModel::IntensityDelta,
                    std::move(sub_type),
                    std::move(model_info),
                    std::move(parent_info),
                    { { FilterModel::Type::IntensityBandwidth,
                        static_cast<size_t>(filter_type) } },
                    std::bitset<ItemModel::Visible::Count>("01"),
                    DELTA + std::string(
                                DISPLAY_PROPS_KERNEL_INTENSITY[intensity.first].first),
                    1.0f });
            }
        }
    }
}

void
Roofline::RenderMenus(ImVec2 region, ImVec2 plot_pos, ImVec2 plot_size,
                      const ImGuiStyle& style, const ImPlotStyle& plot_style,
                      bool& item_hovered, bool& plot_hovered)
{
    plot_pos -= ImGui::GetWindowPos();
    float menus_width      = region.x * 0.25f;
    float button_size      = ImGui::GetFrameHeight();
    float max_menus_height = plot_size.y - plot_style.PlotPadding.y * 2.0f - button_size;

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
    plot_hovered |= ImGui::IsItemHovered();
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
        ImVec2      mode_min     = ImGui::GetCursorScreenPos();
        ImVec2      mode_size    = ImVec2(button_size, button_size);
        bool        mode_clicked = ImGui::InvisibleButton("menu_mode", mode_size);
        plot_hovered |= ImGui::IsItemHovered();
        ImU32 mode_bg = ImGui::IsItemActive() ? ImGui::GetColorU32(ImGuiCol_ButtonActive)
                        : ImGui::IsItemHovered()
                            ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
                            : ImGui::GetColorU32(ImGuiCol_Button);
        ImDrawList* mode_draw = ImGui::GetWindowDrawList();
        mode_draw->AddRectFilled(mode_min, mode_min + mode_size, mode_bg,
                                 style.FrameRounding);
        if(style.FrameBorderSize > 0.0f)
        {
            mode_draw->AddRect(mode_min, mode_min + mode_size,
                               ImGui::GetColorU32(ImGuiCol_Border), style.FrameRounding,
                               0, style.FrameBorderSize);
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
                                    : (m_mode == AllKernels ? 7
                                       : m_mode == Compare  ? 8
                                                            : 6) *
                                          ImGui::GetFrameHeightWithSpacing()) +
            2 * style.WindowPadding.y;
        float sv_height = max_menus_height - header_height - footer_height;
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(menus_content_width, 0),
            ImVec2(menus_content_width,
                   sv_height > ImGui::GetTextLineHeight() ? sv_height : -1.0f));
        ImGui::BeginChild("menus_scroll_view", ImVec2(menus_content_width, 0),
                          ImGuiChildFlags_AutoResizeY);
        plot_hovered |= ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
        scroll_bar_width = std::max(scroll_bar_width,
                                    ImGui::GetScrollMaxY() ? style.ScrollbarSize : 0.0f);
        ImGui::BeginChild("menus_scroll_view_content",
                          ImVec2(menus_content_width - scroll_bar_width, 0),
                          ImGuiChildFlags_AutoResizeY,
                          ImGuiWindowFlags_NoScrollWithMouse);
        bool empty = true;

        for(size_t i = 0; i < m_items.size(); i++)
        {
            if(ItemValid(m_items[i]) &&
               ((m_menus_mode == Legend && m_items[i].visible[ItemModel::Visible::Plot] &&
                 m_items[i].visible[ItemModel::Visible::Menus]) ||
                (m_menus_mode == Options &&
                 m_items[i].visible[ItemModel::Visible::Menus])))
            {
                empty = false;
                ImGui::PushID(static_cast<int>(i));
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
                ImGui::BeginDisabled(m_menus_mode == Options &&
                                     !m_items[i].visible.test(ItemModel::Visible::Plot));
                if(m_menus_mode == Legend)
                {
                    float  icon_width  = ImGui::GetTextLineHeight();
                    ImVec2 icon_center = ImGui::GetCursorScreenPos() +
                                         ImVec2(icon_width, icon_width) * 0.5f;
                    float icon_radius = icon_width * 0.5f - 2 * IMPLOT_LEGEND_ICON_SHRINK;
                    switch(m_items[i].type)
                    {
                        case ItemModel::Type::CeilingCompute:
                        case ItemModel::Type::CeilingBandwidth:
                        {
                            ImGui::GetWindowDrawList()->AddLine(
                                icon_center - ImVec2(icon_radius, 0.0f),
                                icon_center + ImVec2(icon_radius, 0.0f),
                                ItemColor(m_items[i], row_hovered, true),
                                icon_radius * 0.5f);
                            break;
                        }
                        case ItemModel::Type::Intensity:
                        {
                            ImU32 color = ItemColor(m_items[i], row_hovered, true);
                            switch(DISPLAY_PROPS_KERNEL_INTENSITY[m_items[i]
                                                                      .subtype.intensity]
                                       .second)
                            {
                                case ImPlotMarker_Circle:
                                {
                                    ImGui::GetWindowDrawList()->AddCircleFilled(
                                        icon_center, icon_radius, color, 10);
                                    break;
                                }
                                case ImPlotMarker_Square:
                                {
                                    ImGui::GetWindowDrawList()->AddRectFilled(
                                        icon_center - ImVec2(icon_radius, icon_radius) /
                                                          IMPLOT_MARKER_SCALE_FACTOR,
                                        icon_center + ImVec2(icon_radius, icon_radius) /
                                                          IMPLOT_MARKER_SCALE_FACTOR,
                                        color);
                                    break;
                                }
                                case ImPlotMarker_Diamond:
                                {
                                    ImGui::GetWindowDrawList()->AddQuadFilled(
                                        icon_center + ImVec2(0.0f, -icon_radius),
                                        icon_center + ImVec2(icon_radius, 0.0f),
                                        icon_center + ImVec2(0.0f, icon_radius),
                                        icon_center + ImVec2(-icon_radius, 0.0f), color);
                                    break;
                                }
                                case ImPlotMarker_Up:
                                {
                                    ImGui::GetWindowDrawList()->AddTriangleFilled(
                                        icon_center + ImVec2(0.0f, -icon_radius),
                                        icon_center + ImVec2(icon_radius, icon_radius),
                                        icon_center + ImVec2(-icon_radius, icon_radius),
                                        color);
                                    break;
                                }
                            }
                            break;
                        }
                        case ItemModel::Type::IntensityDelta:
                        {
                            ImU32 color = ItemColor(m_items[i], row_hovered, true);
                            ImGui::GetWindowDrawList()->AddLine(
                                icon_center - ImVec2(icon_radius, 0.0f),
                                icon_center -
                                    ImVec2(DASHED_LINE_SEGMENT_GAP * 0.5f, 0.0f),
                                color, icon_radius * 0.5f);
                            ImGui::GetWindowDrawList()->AddLine(
                                icon_center +
                                    ImVec2(DASHED_LINE_SEGMENT_GAP * 0.5f, 0.0f),
                                icon_center + ImVec2(icon_radius, 0.0f), color,
                                icon_radius * 0.5f);
                            break;
                        }
                    }
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + icon_width);
                }
                else
                {
                    ImGui::PushFont(m_settings.GetFontManager().GetFont(FontType::kIcon),
                                    0.0f);
                    ImGui::TextUnformatted(m_items[i].visible[ItemModel::Visible::Plot]
                                               ? ICON_EYE
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
                            m_items[i].visible.flip(ItemModel::Visible::Plot);
                            switch(m_items[i].type)
                            {
                                case ItemModel::Type::CeilingCompute:
                                {
                                    m_custom_ceiling_compute = true;
                                    break;
                                }
                                case ItemModel::Type::CeilingBandwidth:
                                {
                                    m_custom_ceiling_bandwidth = true;
                                    break;
                                }
                                case ItemModel::Type::Intensity:
                                {
                                    m_custom_intensity = true;
                                    break;
                                }
                            }
                            m_options_changed = true;
                        }
                        else
                        {
                            ApplyItemFilter(m_items[i]);
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
            if(m_mode == AllKernels)
            {
                ImGui::PushID("kernel_scale");
                ImGui::Checkbox("", &m_scale_intensity);
                ImGui::SameLine();
                ElidedText("Scale kernel marker size to duration",
                           ImGui::GetContentRegionAvail().x, plot_size.x * 0.5f,
                           Alignment_Left, true);
                ImGui::PopID();
            }
            ImGui::PushID("ceiling_label");
            ImGui::Checkbox("", &m_ceiling_labels);
            ImGui::SameLine();
            ElidedText("Show peak labels", ImGui::GetContentRegionAvail().x,
                       plot_size.x * 0.5f, Alignment_Left, true);
            ImGui::PopID();
            if(m_mode == Compare && m_workload_primary != m_workload_secondary)
            {
                ElidedText("Peak Source", ImGui::GetContentRegionAvail().x,
                           plot_size.x * 0.5f, Alignment_Left, true);
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImGui::GetCursorScreenPos(),
                    ImGui::GetCursorScreenPos() +
                        ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight()),
                    m_settings.GetColor(Colors::kButton),
                    m_settings.GetDefaultStyle().FrameRounding);
                if(ColoredButton("Baseline",
                                 m_settings.GetColor(!m_alternate_ceiling_source
                                                         ? Colors::kAccent
                                                         : Colors::kButton),
                                 m_settings.GetColor(!m_alternate_ceiling_source
                                                         ? Colors::kAccent
                                                         : Colors::kButtonHovered),
                                 m_settings.GetColor(!m_alternate_ceiling_source
                                                         ? Colors::kAccent
                                                         : Colors::kButtonActive),
                                 m_settings.GetColor(!m_alternate_ceiling_source
                                                         ? Colors::kTextOnAccent
                                                         : Colors::kTextMain),
                                 "Source hardware peaks from baseline workload.",
                                 ImVec2(ImGui::GetContentRegionAvail().x * 0.5f,
                                        ImGui::GetFrameHeight())) &&
                   m_alternate_ceiling_source)
                {
                    m_alternate_ceiling_source = false;
                    m_workload_changed         = true;
                    m_options_changed          = true;
                }
                ImGui::SameLine(0.0f, 0.0f);
                if(ColoredButton("Target",
                                 m_settings.GetColor(m_alternate_ceiling_source
                                                         ? Colors::kAccent
                                                         : Colors::kButton),
                                 m_settings.GetColor(m_alternate_ceiling_source
                                                         ? Colors::kAccent
                                                         : Colors::kButtonHovered),
                                 m_settings.GetColor(m_alternate_ceiling_source
                                                         ? Colors::kAccent
                                                         : Colors::kButtonActive),
                                 m_settings.GetColor(m_alternate_ceiling_source
                                                         ? Colors::kTextOnAccent
                                                         : Colors::kTextMain),
                                 "Source hardware peaks from target workload.",
                                 ImVec2(ImGui::GetContentRegionAvail().x,
                                        ImGui::GetFrameHeight())) &&
                   !m_alternate_ceiling_source)
                {
                    m_alternate_ceiling_source = true;
                    m_workload_changed         = true;
                    m_options_changed          = true;
                }
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
Roofline::FilterCombo(const char* label, const float width, const ImGuiStyle& style,
                      const std::vector<FilterModel>& filters, bool& custom_override,
                      size_t& active_idx)
{
    ImGui::PushID(label);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(width - ImGui::GetItemRectSize().x - style.ItemSpacing.x);
    PushComboStyles();
    if(ImGui::BeginCombo("", custom_override ? "-" : filters[active_idx].name))
    {
        for(size_t i = 0; i < filters.size(); i++)
        {
            if(!filters[i].item_idx.empty())
            {
                ImGui::PushID(static_cast<int>(i));
                if(ImGui::Selectable("", i == active_idx))
                {
                    active_idx      = i;
                    custom_override = false;
                    ApplyFilters();
                }
                ImGui::SameLine(ImGui::GetCursorPosX());
                ElidedText(filters[i].name, ImGui::GetContentRegionAvail().x,
                           ImGui::GetContentRegionAvail().x);
                ImGui::PopID();
            }
        }
        ImGui::EndCombo();
    }
    PopComboStyles();
    ImGui::PopID();
}

void
Roofline::PlotHoverIdx()
{
    if(ImPlot::IsPlotHovered())
    {
        ImVec2 mouse_pos = ImGui::GetMousePos();
        // Pick the closest visible item after plotting all candidates.
        for(size_t i = 0; i < m_items.size(); i++)
        {
            float distance = FLT_MAX;
            if(m_items[i].visible[ItemModel::Visible::Plot])
            {
                switch(m_items[i].type)
                {
                    case ItemModel::Type::CeilingCompute:
                    case ItemModel::Type::CeilingBandwidth:
                    {
                        if(m_items[i].info.ceiling)
                        {
                            distance = PointDistanceFromLine(
                                mouse_pos,
                                ImPlot::PlotToPixels(
                                    ImPlotPoint(m_items[i].info.ceiling->position.p1.x,
                                                m_items[i].info.ceiling->position.p1.y)),
                                ImPlot::PlotToPixels(
                                    ImPlotPoint(m_items[i].info.ceiling->position.p2.x,
                                                m_items[i].info.ceiling->position.p2.y)));
                        }
                        break;
                    }
                    case ItemModel::Type::Intensity:
                    {
                        if(m_items[i].info.intensity)
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
                    case ItemModel::Type::IntensityDelta:
                    {
                        if(m_items[i].info.delta.baseline && m_items[i].info.delta.target)
                        {
                            distance = PointDistanceFromLine(
                                mouse_pos,
                                ImPlot::PlotToPixels(ImPlotPoint(
                                    m_items[i].info.delta.baseline->position.x,
                                    m_items[i].info.delta.baseline->position.y)),
                                ImPlot::PlotToPixels(ImPlotPoint(
                                    m_items[i].info.delta.target->position.x,
                                    m_items[i].info.delta.target->position.y)));
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

float
Roofline::PointDistanceFromLine(ImVec2 point, ImVec2 line_p1, ImVec2 line_p2) const
{
    ImVec2 line_direction = ImVec2(line_p2.x - line_p1.x, line_p2.y - line_p1.y);
    ImVec2 point_to_line  = ImVec2(point.x - line_p1.x, point.y - line_p1.y);
    float  projection     = std::clamp(
        (point_to_line.x * line_direction.x + point_to_line.y * line_direction.y) /
            (line_direction.x * line_direction.x + line_direction.y * line_direction.y),
        0.0f, 1.0f);
    ImVec2 closest_point = ImVec2(line_p1.x + projection * line_direction.x,
                                  line_p1.y + projection * line_direction.y);
    float  dx            = point.x - closest_point.x;
    float  dy            = point.y - closest_point.y;
    return std::sqrt(dx * dx + dy * dy);
}

bool
Roofline::ItemValid(const ItemModel& item) const
{
    bool valid = false;
    switch(item.type)
    {
        case ItemModel::Type::CeilingCompute:
        case ItemModel::Type::CeilingBandwidth:
        {
            valid = item.info.ceiling;
            break;
        }
        case ItemModel::Type::Intensity:
        {
            valid = item.info.intensity && item.parent_info.kernel;
            break;
        }
        case ItemModel::Type::IntensityDelta:
        {
            valid = item.info.delta.baseline && item.info.delta.target;
            break;
        }
    }
    return valid;
}

ImU32
Roofline::ItemColor(const ItemModel& item, bool hovered, bool legend) const
{
    ImU32 color = 0;
    switch(item.type)
    {
        case ItemModel::Type::CeilingCompute:
        {
            color = ImGui::GetColorU32(
                ImPlot::GetColormapColor(static_cast<int>(item.subtype.compute)));
            break;
        }
        case ItemModel::Type::CeilingBandwidth:
        {
            color = ImGui::GetColorU32(ImPlot::GetColormapColor(
                static_cast<int>(__KRPVControllerRooflineCeilingComputeTypeLast +
                                 item.subtype.bandwidth)));
            break;
        }
        case ItemModel::Type::Intensity:
        {
            color = m_mode == Compare
                        ? m_settings.GetColor(item.parent_info.kernel == m_kernel_primary
                                                  ? Colors::kComparisonBase
                                                  : Colors::kComparisonTarget)
                        : ImGui::GetColorU32(ImPlot::GetColormapColor(
                              static_cast<int>(item.parent_info.kernel->id)));
            break;
        }
        case ItemModel::Type::IntensityDelta:
        {
            if(item.info.delta.target->position.y == item.info.delta.baseline->position.y)
            {
                color = m_settings.GetColor(Colors::kTextDim);
            }
            else if(item.info.delta.target->position.y >
                    item.info.delta.baseline->position.y)
            {
                color = m_settings.GetColor(Colors::kComparisonGreater);
            }
            else
            {
                color = m_settings.GetColor(Colors::kComparisonLesser);
            }
            break;
        }
    }
    return ImGui::GetColorU32(color, legend && hovered ? LEGEND_HOVER_COLOR_ALPHA : 1.0f);
}

void
Roofline::ApplyItemFilter(const ItemModel& item)
{
    switch(item.type)
    {
        case ItemModel::Type::CeilingCompute:
        {
            if(item.filter_idx.count(FilterModel::Type::CeilingCompute) > 0)
            {
                m_active_filter_ceiling_compute =
                    m_active_filter_ceiling_compute ==
                            item.filter_idx.at(FilterModel::Type::CeilingCompute)
                        ? static_cast<size_t>(FilterModel::ComputeType::ComputeTypeAll)
                        : item.filter_idx.at(FilterModel::Type::CeilingCompute);
                m_custom_ceiling_compute = false;
                ApplyFilters();
            }
            break;
        }
        case ItemModel::Type::CeilingBandwidth:
        {
            if(item.filter_idx.count(FilterModel::Type::CeilingBandwidth) > 0)
            {
                m_active_filter_ceiling_bandwidth =
                    m_active_filter_ceiling_bandwidth ==
                            item.filter_idx.at(FilterModel::Type::CeilingBandwidth)
                        ? static_cast<size_t>(
                              FilterModel::BandwidthType::BandwidthTypeAll)
                        : item.filter_idx.at(FilterModel::Type::CeilingBandwidth);
                m_custom_ceiling_bandwidth = false;
                ApplyFilters();
            }
            break;
        }
        case ItemModel::Type::Intensity:
        case ItemModel::Type::IntensityDelta:
        {
            if(m_mode == AllKernels &&
               item.filter_idx.count(FilterModel::Type::IntensityKernel) > 0)
            {
                m_active_filter_intensity_kernel =
                    m_active_filter_intensity_kernel ==
                            item.filter_idx.at(FilterModel::Type::IntensityKernel)
                        ? static_cast<size_t>(0)
                        : item.filter_idx.at(FilterModel::Type::IntensityKernel);
                m_custom_intensity = false;
                ApplyFilters();
            }
            else if(m_mode != AllKernels &&
                    item.filter_idx.count(FilterModel::Type::IntensityBandwidth) > 0)
            {
                m_active_filter_intensity_bandwidth =
                    m_active_filter_intensity_bandwidth ==
                            item.filter_idx.at(FilterModel::Type::IntensityBandwidth)
                        ? static_cast<size_t>(
                              FilterModel::BandwidthType::BandwidthTypeAll)
                        : item.filter_idx.at(FilterModel::Type::IntensityBandwidth);
                m_custom_intensity = false;
                ApplyFilters();
            }
            break;
        }
    }
}

void
Roofline::ApplyFilters()
{
    if(m_active_filter_ceiling_compute >= m_filters_ceiling_compute.size() ||
       m_filters_ceiling_compute[m_active_filter_ceiling_compute].item_idx.empty())
    {
        m_active_filter_ceiling_compute = static_cast<size_t>(
            m_filters_ceiling_compute[FilterModel::ComputeType::FP32].item_idx.size()
                ? FilterModel::ComputeType::FP32
            : m_filters_ceiling_compute[FilterModel::ComputeType::FP16].item_idx.size()
                ? FilterModel::ComputeType::FP16
            : m_filters_ceiling_compute[FilterModel::ComputeType::FP8].item_idx.size()
                ? FilterModel::ComputeType::FP8
            : m_filters_ceiling_compute[FilterModel::ComputeType::FP4].item_idx.size()
                ? FilterModel::ComputeType::FP4
            : m_filters_ceiling_compute[FilterModel::ComputeType::FP64].item_idx.size()
                ? FilterModel::ComputeType::FP64
                : 0);
    }
    if(m_active_filter_ceiling_bandwidth >= m_filters_ceiling_bandwidth.size() ||
       m_filters_ceiling_bandwidth[m_active_filter_ceiling_bandwidth].item_idx.empty())
    {
        m_active_filter_ceiling_bandwidth = 0;
    }
    if(m_active_filter_intensity_kernel >= m_filters_intensity_kernel.size() ||
       m_filters_intensity_kernel[m_active_filter_intensity_kernel].item_idx.empty())
    {
        m_active_filter_intensity_kernel = 0;
    }
    if(m_active_filter_intensity_bandwidth >= m_filters_intensity_bandwidth.size() ||
       m_filters_intensity_bandwidth[m_active_filter_intensity_bandwidth]
           .item_idx.empty())
    {
        m_active_filter_intensity_bandwidth = 0;
    }
    for(size_t i = 0; i < m_items.size(); i++)
    {
        switch(m_items[i].type)
        {
            case ItemModel::Type::CeilingCompute:
            {
                m_items[i].visible[ItemModel::Visible::Plot] =
                    m_filters_ceiling_compute[m_active_filter_ceiling_compute]
                        .item_idx.count(i);
                break;
            }
            case ItemModel::Type::CeilingBandwidth:
            {
                m_items[i].visible[ItemModel::Visible::Plot] =
                    m_filters_ceiling_bandwidth[m_active_filter_ceiling_bandwidth]
                        .item_idx.count(i);
                break;
            }
            case ItemModel::Type::Intensity:
            {
                m_items[i].visible[ItemModel::Visible::Plot] =
                    m_filters_intensity_kernel[m_active_filter_intensity_kernel]
                        .item_idx.count(i) &&
                    m_filters_intensity_bandwidth[m_active_filter_intensity_bandwidth]
                        .item_idx.count(i);
                break;
            }
            default:
            {
                break;
            }
        }
    }
    m_options_changed = true;
}

Roofline::FilterModel::ComputeType
Roofline::FilterComputeType(
    rocprofvis_controller_roofline_ceiling_compute_type_t type) const
{
    Roofline::FilterModel::ComputeType filter_type;
    switch(type)
    {
        case kRPVControllerRooflineCeilingComputeMFMAFP4:
        {
            filter_type = FilterModel::ComputeType::FP4;
            break;
        }
        case kRPVControllerRooflineCeilingComputeMFMAFP6:
        {
            filter_type = FilterModel::ComputeType::FP6;
            break;
        }
        case kRPVControllerRooflineCeilingComputeMFMAFP8:
        {
            filter_type = FilterModel::ComputeType::FP8;
            break;
        }
        case kRPVControllerRooflineCeilingComputeVALUFP16:
        case kRPVControllerRooflineCeilingComputeMFMAFP16:
        case kRPVControllerRooflineCeilingComputeMFMABF16:
        {
            filter_type = FilterModel::ComputeType::FP16;
            break;
        }
        case kRPVControllerRooflineCeilingComputeVALUFP32:
        case kRPVControllerRooflineCeilingComputeMFMAFP32:
        {
            filter_type = FilterModel::ComputeType::FP32;
            break;
        }
        case kRPVControllerRooflineCeilingComputeVALUFP64:
        case kRPVControllerRooflineCeilingComputeMFMAFP64:
        {
            filter_type = FilterModel::ComputeType::FP64;
            break;
        }
        default:
        {
            filter_type = Roofline::FilterModel::ComputeType::ComputeTypeUnknown;
            break;
        }
    }
    return filter_type;
}

Roofline::FilterModel::BandwidthType
Roofline::FilterBandwidthType(
    rocprofvis_controller_roofline_ceiling_bandwidth_type_t type) const
{
    Roofline::FilterModel::BandwidthType filter_type;
    switch(type)
    {
        case kRPVControllerRooflineCeilingTypeBandwidthHBM:
        {
            filter_type = FilterModel::BandwidthType::HBM;
            break;
        }
        case kRPVControllerRooflineCeilingTypeBandwidthL2:
        {
            filter_type = FilterModel::BandwidthType::L2;
            break;
        }
        case kRPVControllerRooflineCeilingTypeBandwidthL1:
        {
            filter_type = FilterModel::BandwidthType::L1;
            break;
        }
        case kRPVControllerRooflineCeilingTypeBandwidthLDS:
        {
            filter_type = FilterModel::BandwidthType::LDS;
            break;
        }
        default:
        {
            filter_type = Roofline::FilterModel::BandwidthType::BandwidthTypeUnknown;
            break;
        }
    }
    return filter_type;
}

Roofline::FilterModel::BandwidthType
Roofline::FilterBandwidthType(
    rocprofvis_controller_roofline_kernel_intensity_type_t type) const
{
    Roofline::FilterModel::BandwidthType filter_type;
    switch(type)
    {
        case kRPVControllerRooflineKernelIntensityTypeHBM:
        {
            filter_type = FilterModel::BandwidthType::HBM;
            break;
        }
        case kRPVControllerRooflineKernelIntensityTypeL2:
        {
            filter_type = FilterModel::BandwidthType::L2;
            break;
        }
        case kRPVControllerRooflineKernelIntensityTypeL1:
        {
            filter_type = FilterModel::BandwidthType::L1;
            break;
        }
        case kRPVControllerRooflineKernelIntensityTypeLDS:
        {
            filter_type = FilterModel::BandwidthType::LDS;
            break;
        }
        default:
        {
            filter_type = Roofline::FilterModel::BandwidthType::BandwidthTypeUnknown;
            break;
        }
    }
    return filter_type;
}

}  // namespace View
}  // namespace RocProfVis
