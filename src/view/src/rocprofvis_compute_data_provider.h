// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "rocprofvis_controller_types.h"
#include "rocprofvis_controller_enums.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>


namespace RocProfVis
{
namespace View
{

typedef struct ComputeTableCellModel
{
    std::string m_str_value;
    rocprofvis_controller_primitive_type_t m_type;
    union {
        uint64_t m_uint64;
        double m_double;
    } m_num_value;
    bool m_colorize;
    bool m_highlight;
} ComputeTableCellModel;

typedef struct ComputeTableModel
{
    std::string m_title;
    std::vector<std::string> m_column_names;
    std::vector<std::vector<ComputeTableCellModel>> m_cells;
} ComputeTableModel;

typedef struct ComputePlotAxisModel
{
    std::string m_name;
    std::vector<const char*> m_tick_labels;
    double m_max;
    double m_min;
    double m_min_non_zero;
} ComputePlotAxisModel;

typedef struct ComputePlotSeriesModel
{
    std::string m_name;
    std::vector<double> m_x_values;
    std::vector<double> m_y_values;
} ComputePlotSeriesModel;

typedef struct ComputePlotModel
{
    std::string m_title;
    ComputePlotAxisModel m_x_axis;
    ComputePlotAxisModel m_y_axis;
    std::vector<ComputePlotSeriesModel> m_series;
} ComputePlotModel;

typedef struct ComputeMetricModel
{
    rocprofvis_controller_primitive_type_t m_type;
    union {
        uint64_t m_uint64;
        double m_double;
    } m_value;
} ComputeMetricModel;

class ComputeDataProvider
{
public:
    ComputeDataProvider();
    ~ComputeDataProvider();

    void InitController();
    void FreeController();
    rocprofvis_result_t LoadTrace(const std::string& path);

    ComputeTableModel* GetTableModel(const rocprofvis_controller_compute_table_types_t type) const;
    ComputePlotModel* GetPlotModel(const rocprofvis_controller_compute_plot_types_t type) const;
    ComputeMetricModel* GetMetricModel(const rocprofvis_controller_compute_metric_types_t type) const;

private:
    rocprofvis_result_t GetStringPropertyFromHandle(rocprofvis_handle_t* handle, const rocprofvis_property_t property, const uint64_t index, std::string& output);
    std::string TrimDecimalPlaces(std::string& double_str, const int decimal_places);

    rocprofvis_controller_t* m_controller;
    rocprofvis_controller_future_t* m_controller_future;
    rocprofvis_controller_compute_trace_t* m_trace;

    std::unordered_map<rocprofvis_controller_compute_table_types_t, std::unique_ptr<ComputeTableModel>> m_tables;
    std::unordered_map<rocprofvis_controller_compute_plot_types_t, std::unique_ptr<ComputePlotModel>> m_plots;
    std::unordered_map<rocprofvis_controller_compute_metric_types_t, std::unique_ptr<ComputeMetricModel>> m_metrics;
};

}  // namespace View
}  // namespace RocProfVis
