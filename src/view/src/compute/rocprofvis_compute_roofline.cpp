// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_roofline.h"
#include "icons/rocprovfis_icon_defines.h"
#include "implot/implot.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_settings_manager.h"
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
};
constexpr const char* DISPLAY_NAMES_PRESET[] = {
    "FP4",   // PresetModel::Type::FP4
    "FP6",   // PresetModel::Type::FP6
    "FP8",   // PresetModel::Type::FP8
    "FP16",  // PresetModel::Type::FP16
    "FP32",  // PresetModel::Type::FP32
    "FP64",  // PresetModel::Type::FP64
};

Roofline::Roofline(DataProvider& data_provider)
: m_data_provider(data_provider)
, m_settings(SettingsManager::GetInstance())
, m_show_menus(true)
, m_menus_mode(Legend)
, m_scale_intensity(true)
, m_hovered_item_distance(FLT_MAX)
, m_workload_changed(false)
, m_kernel_changed(false)
, m_options_changed(false)
, m_workload(nullptr)
, m_requested_workload_id(0)
, m_kernel(nullptr)
, m_requested_kernel_id(0)
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
        const std::unordered_map<uint32_t, WorkloadInfo>& workloads =
            m_data_provider.ComputeModel().GetWorkloads();
        if(workloads.count(m_requested_workload_id) > 0)
        {
            m_workload = &workloads.at(m_requested_workload_id);
        }
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
            uint64_t kernel_duration_scale = 0.0;
            for(const std::pair<const uint32_t, KernelInfo>& kernel : m_workload->kernels)
            {
                kernel_duration_scale =
                    std::max(kernel_duration_scale, kernel.second.duration_total);
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
                        static_cast<float>(
                            static_cast<double>(kernel.second.duration_total) /
                            static_cast<double>(kernel_duration_scale)) });
                }
            }
            ApplyPreset(PresetModel::FP32);
        }
        m_workload_changed = false;
    }
    if(m_kernel_changed && m_workload)
    {
        if(m_workload->kernels.count(m_requested_kernel_id) > 0)
        {
            m_kernel = &m_workload->kernels.at(m_requested_kernel_id);
        }
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
    if(m_workload)
    {
        ImGui::BeginChild("roofline", ImVec2(ImGui::GetContentRegionAvail().x,
                                             ImGui::GetContentRegionAvail().x * 0.5f));
        const ImVec2       region     = ImGui::GetContentRegionAvail();
        const ImGuiStyle&  style      = ImGui::GetStyle();
        const ImPlotStyle& plot_style = ImPlot::GetStyle();
        ImGui::SetCursorPos(ImVec2(region.x * 0.5f - plot_style.PlotBorderSize -
                                       ImGui::CalcTextSize("Roofline Analysis").x * 0.5f,
                                   plot_style.PlotPadding.y));
        ImGui::TextUnformatted("Roofline Analysis");
        if(m_workload->roofline.ceiling_bandwidth.empty() ||
           m_workload->roofline.ceiling_compute.empty() ||
           (m_kernel && m_kernel->roofline.intensities.empty()))
        {
            ImGui::GetWindowDrawList()->AddRect(
                ImGui::GetCursorScreenPos() +
                    ImVec2(plot_style.PlotBorderSize + plot_style.PlotPadding.x,
                           plot_style.PlotBorderSize + plot_style.PlotPadding.y),
                ImGui::GetCursorScreenPos() + region -
                    ImVec2(plot_style.PlotBorderSize + plot_style.PlotPadding.x,
                           plot_style.PlotBorderSize + plot_style.PlotPadding.y),
                ImGui::GetColorU32(style.Colors[ImGuiCol_TableBorderStrong]));
            ImGui::SetCursorPos((region - ImGui::CalcTextSize("No data available.")) *
                                0.5f);
            ImGui::TextDisabled("No data available.");
        }
        else
        {
            ImPlot::PushStyleColor(ImPlotCol_PlotBg,
                                   ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
            ImPlot::PushStyleColor(ImPlotCol_FrameBg,
                                   m_settings.GetColor(Colors::kTransparent));
            ImPlot::PushColormap("flame");
            if(ImPlot::BeginPlot("plot", ImVec2(-1, -1),
                                 ImPlotFlags_NoTitle | ImPlotFlags_NoFrame |
                                     ImPlotFlags_NoLegend | ImPlotFlags_NoMenus |
                                     ImPlotFlags_Crosshairs))
            {
                ImPlot::SetupAxis(ImAxis_X1, "Arithmetic Intensity (FLOP/Byte)",
                                  ImPlotAxisFlags_NoSideSwitch |
                                      ImPlotAxisFlags_NoHighlight);
                ImPlot::SetupAxis(ImAxis_Y1, "Performance (GFLOP/s)",
                                  ImPlotAxisFlags_NoSideSwitch |
                                      ImPlotAxisFlags_NoHighlight);
                ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Log10);
                ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
                ImPlot::SetupAxisLimits(ImAxis_X1, m_workload->roofline.min.x,
                                        m_workload->roofline.max.x);
                ImPlot::SetupAxisLimits(ImAxis_Y1, m_workload->roofline.min.y / 10,
                                        m_workload->roofline.max.y * 10);
                ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, m_workload->roofline.min.x,
                                                   m_workload->roofline.max.x);
                ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1,
                                                   m_workload->roofline.min.y / 10,
                                                   m_workload->roofline.max.y * 10);
                PlotHoverIdx();
                for(size_t i = 0; i < m_items.size(); i++)
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
                            display =
                                m_items[i].info.intensity &&
                                (m_kernel ? m_items[i].parent_info.kernel == m_kernel
                                          : true);
                        }
                    }
                    display &= m_items[i].visible;
                    if(display)
                    {
                        ImGui::PushID(i);
                        bool hovered =
                            m_hovered_item_idx && m_hovered_item_idx.value() == i;
                        switch(m_items[i].type)
                        {
                            case ItemModel::Type::CeilingCompute:
                            case ItemModel::Type::CeilingBandwidth:
                            {
                                ImPlot::SetNextLineStyle(ImPlot::GetColormapColor(i),
                                                         hovered ? plot_style.LineWeight *
                                                                       3.0f
                                                                 : plot_style.LineWeight);
                                ImPlot::PlotLineG(
                                    "",
                                    [](int idx, void* user_data) -> ImPlotPoint {
                                        const WorkloadInfo::Roofline::Line* line =
                                            static_cast<
                                                const WorkloadInfo::Roofline::Line*>(
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
                                        (m_scale_intensity ? m_items[i].weight : 0.0f) *
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
                            ImGui::PushStyleVar(
                                ImGuiStyleVar_WindowRounding,
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
                                                ImGui::CalcTextSize(
                                                    m_items[i].label.c_str()),
                                            ImGui::GetColorU32(
                                                ImPlot::GetColormapColor(i)));
                                        ImGui::TextUnformatted(m_items[i].label.c_str());
                                        ImGui::Text(
                                            "%.0f GFLOP/s",
                                            std::round(
                                                m_items[i].info.ceiling->throughput));
                                        break;
                                    }
                                    case ItemModel::Type::CeilingBandwidth:
                                    {
                                        ImGui::GetWindowDrawList()->AddRectFilled(
                                            ImGui::GetCursorScreenPos(),
                                            ImGui::GetCursorScreenPos() +
                                                ImGui::CalcTextSize(
                                                    m_items[i].label.c_str()),
                                            ImGui::GetColorU32(
                                                ImPlot::GetColormapColor(i)));
                                        ImGui::TextUnformatted(m_items[i].label.c_str());
                                        ImGui::Text(
                                            "%.0f GB/s",
                                            std::round(
                                                m_items[i].info.ceiling->throughput));
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
                                            ImGui::GetColorU32(
                                                ImPlot::GetColormapColor(i)));
                                        ImGui::TextUnformatted(
                                            DISPLAY_NAMES_KERNEL_INTENSITY
                                                [m_items[i].subtype.intensity]);
                                        ImVec2 reserved_pos = ImGui::GetCursorPos();
                                        ImGui::NewLine();
                                        ImGui::Text(
                                            "Inovcation(s): %u",
                                            m_items[i]
                                                .parent_info.kernel->invocation_count);
                                        ImGui::Text(
                                            "Duration: %llu",
                                            m_items[i]
                                                .parent_info.kernel->duration_total);
                                        ImGui::Text(
                                            "Arithmetic Intensity: %f FLOP/Byte",
                                            m_items[i].info.intensity->position.x);
                                        ImGui::Text(
                                            "Performance: %f GFLOP/s",
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
                ImPlot::EndPlot();
            }
            bool menus_item_hovered = false;
            RenderMenus(region, style, plot_style, menus_item_hovered);
            if(!menus_item_hovered)
            {
                m_hovered_item_idx      = std::nullopt;
                m_hovered_item_distance = FLT_MAX;
            }
            ImPlot::PopColormap();
            ImPlot::PopStyleColor(2);
        }
        ImGui::EndChild();
    }
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
Roofline::RenderMenus(const ImVec2 region, const ImGuiStyle& style,
                      const ImPlotStyle& plot_style, bool& item_hovered)
{
    ImVec2 window_pos = ImVec2(0.75f * region.x - 2 * plot_style.PlotPadding.x,
                               ImGui::GetFontSize() + plot_style.PlotBorderSize +
                                   3 * plot_style.PlotPadding.y);
    ImVec2 button_pos =
        m_show_menus ? window_pos - ImVec2(ImGui::GetFrameHeightWithSpacing(), 0.0f)
                     : ImVec2(region.x - 2 * plot_style.PlotPadding.x -
                                  ImGui::GetFrameHeightWithSpacing(),
                              ImGui::GetFontSize() + plot_style.PlotBorderSize +
                                  3 * plot_style.PlotPadding.y);
    ImGui::SetCursorPos(button_pos);
    if(ImGui::ArrowButton("toggle_menus", m_show_menus ? ImGuiDir_Right : ImGuiDir_Left))
    {
        m_show_menus = !m_show_menus;
    }
    if(m_show_menus)
    {
        ImGui::SetCursorPos(button_pos +
                            ImVec2(0.0f, ImGui::GetFrameHeightWithSpacing()));
        ImGui::PushFont(m_settings.GetFontManager().GetIconFont(FontType::kDefault));
        if(ImGui::Button(m_menus_mode == Legend ? ICON_GEAR : ICON_LIST))
        {
            if(m_menus_mode == Legend)
            {
                m_menus_mode = Options;
            }
            else
            {
                m_menus_mode = Legend;
            }
        }
        ImGui::PopFont();
        ImGui::SetCursorPos(window_pos);
        float menus_width      = region.x * 0.25f;
        float max_menus_height = ImGui::GetContentRegionAvail().y -
                                 3 * ImGui::GetFontSize() - 5 * plot_style.PlotPadding.y -
                                 plot_style.PlotBorderSize;
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(menus_width, ImGui::GetFrameHeightWithSpacing() * 2.0f),
            ImVec2(menus_width, max_menus_height));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, plot_style.LegendInnerPadding);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, style.Colors[ImGuiCol_WindowBg]);
        ImGui::BeginChild("menus_window", ImVec2(menus_width, 0.0f),
                          ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY);
        float menus_content_width =
            ImGui::GetWindowWidth() - 2.0f * style.WindowPadding.x;
        float scroll_bar_width = ImGui::GetScrollMaxY() ? style.ScrollbarSize : 0.0f;
        ImGui::BeginGroup();
        if(m_menus_mode == Options)
        {
            ImGui::SeparatorText("Presets");
            for(PresetModel& preset : m_presets)
            {
                if(!preset.item_indices.empty())
                {
                    if(ImGui::Button(DISPLAY_NAMES_PRESET[preset.type],
                                     ImVec2(-1.0f, 0.0f)))
                    {
                        ApplyPreset(preset.type);
                    }
                }
            }
            ImGui::SeparatorText("Custom");
        }
        ImGui::EndGroup();
        float header_height = ImGui::GetItemRectSize().y + 2 * style.WindowPadding.y;
        float footer_height =
            m_kernel ? 0.0f
                     : 2 * ImGui::GetFrameHeightWithSpacing() + 2 * style.WindowPadding.y;
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
        for(size_t i = 0; i < m_items.size(); i++)
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
                    display =
                        m_items[i].info.intensity &&
                        (m_kernel ? m_items[i].parent_info.kernel == m_kernel : true);
                    break;
                }
            }
            if(display &&
               (m_menus_mode == Legend && m_items[i].visible || m_menus_mode == Options))
            {
                empty = false;
                ImGui::PushID(i);
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
                    ImGui::PushFont(
                        m_settings.GetFontManager().GetIconFont(FontType::kDefault));
                    ImGui::TextUnformatted(m_items[i].visible ? ICON_EYE
                                                              : ICON_EYE_SLASH);
                    ImGui::PopFont();
                    ImGui::SameLine();
                }
                ElidedText(m_items[i].label.c_str(), ImGui::GetContentRegionAvail().x,
                           region.x * 0.5f);
                ImGui::EndDisabled();
                if(row_hovered)
                {
                    m_hovered_item_idx = i;
                    item_hovered       = true;
                    if(row_clicked && m_menus_mode == Options)
                    {
                        m_items[i].visible = !m_items[i].visible;
                        m_options_changed  = true;
                    }
                }
                ImGui::PopID();
            }
        }
        if(empty)
        {
            ImGui::TextDisabled("No data selected.");
        }
        ImGui::EndChild();
        ImGui::EndChild();
        if(m_menus_mode == Options && !m_kernel)
        {
            ImGui::SeparatorText("Options");
            ImGui::Checkbox("##scale_intensity", &m_scale_intensity);
            ImGui::SameLine();
            ElidedText("Scale kernel marker size to duration",
                       ImGui::GetContentRegionAvail().x, region.x * 0.5f, true);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
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
        // This has to be a separate pass (as opposed to being part of render pass)
        // to prevent case where multiple items become hovered due to a higher index
        // item being a better canidate than a lower index item.
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
    // Disable all compute cielings...
    for(size_t i = 0;
        i < static_cast<size_t>(__KRPVControllerRooflineCeilingComputeTypeLast); i++)
    {
        m_items[i].visible = false;
    }
    // Enable preset's compute ceilings...
    for(const size_t& item_idx : m_presets[type].item_indices)
    {
        m_items[item_idx].visible = true;
    }
    // Enable all bandwidth ceilings...
    for(size_t i = static_cast<size_t>(__KRPVControllerRooflineCeilingComputeTypeLast);
        i < static_cast<size_t>(__KRPVControllerRooflineCeilingComputeTypeLast +
                                __KRPVControllerRooflineCeilingBandwidthTypeLast);
        i++)
    {
        m_items[i].visible = true;
    }
    // Enable all kernels...
    for(size_t i = static_cast<size_t>(__KRPVControllerRooflineCeilingComputeTypeLast +
                                       __KRPVControllerRooflineCeilingBandwidthTypeLast);
        i < m_items.size(); i++)
    {
        m_items[i].visible = true;
    }
    m_options_changed = true;
}

}  // namespace View
}  // namespace RocProfVis
