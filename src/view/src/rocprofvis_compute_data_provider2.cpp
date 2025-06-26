// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_compute_data_provider2.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_event_manager.h"
#include "spdlog/spdlog.h"
#include <cfloat>

namespace RocProfVis
{
namespace View
{

constexpr int PLOT_LABEL_MAX_LENGTH = 40;

ComputeDataProvider2::ComputeDataProvider2()
: m_controller(nullptr)
, m_controller_future(nullptr)
, m_trace(nullptr)
{

}

ComputeDataProvider2::~ComputeDataProvider2() 
{
    FreeController();
    for (auto& it : m_plots)
    {
        for (const char* label : it.second->m_x_axis.m_tick_labels)
        {
            delete[] label;
        }
        for (const char* label : it.second->m_y_axis.m_tick_labels)
        {
            delete[] label;
        }
    }
}

void ComputeDataProvider2::InitController()
{
    if (!m_controller)
    {
        m_controller = rocprofvis_controller_alloc();
        ROCPROFVIS_ASSERT(m_controller);
    }
    if (!m_controller_future)
    {
        m_controller_future = rocprofvis_controller_future_alloc();
        ROCPROFVIS_ASSERT(m_controller_future);
    }
}

void ComputeDataProvider2::FreeController()
{
    if (!m_controller_future)
    {
        rocprofvis_controller_future_free(m_controller_future);
        m_controller_future = nullptr;
    }
    if (!m_controller)
    {
        rocprofvis_controller_free(m_controller);
        m_controller = nullptr;
    }
}

rocprofvis_result_t ComputeDataProvider2::LoadTrace(const std::string& path)
{
    InitController();
    m_tables.clear();
    rocprofvis_result_t result = rocprofvis_controller_load_async(m_controller, path.c_str(), m_controller_future);
    if (result == kRocProfVisResultSuccess)
    {
        result = rocprofvis_controller_future_wait(m_controller_future, FLT_MAX);
        if (result == kRocProfVisResultSuccess)
        {
            result = rocprofvis_controller_get_object(m_controller, kRPVControllerComputeTrace, 0, &m_trace);
            if (result == kRocProfVisResultSuccess)
            {
                ROCPROFVIS_ASSERT(m_trace);
                rocprofvis_handle_t* table_handle = nullptr;
                for (uint64_t table_type = kRPVControllerComputeTableTypeKernelList; table_type < kRPVControllerComputeTableTypeCount; table_type ++)
                {
                    result = rocprofvis_controller_get_object(m_trace, table_type, 0, &table_handle);
                    if (result == kRocProfVisResultSuccess)
                    {
                        ROCPROFVIS_ASSERT(table_handle);
                        std::string title;
                        std::vector<std::string> column_names;
                        std::vector<std::vector<ComputeTableCellModel>> cells;
                        uint64_t unit_column = -1;
                        GetStringPropertyFromHandle(table_handle, kRPVControllerTableTitle, 0, title);
                        uint64_t columns = 0;
                        result = rocprofvis_controller_get_uint64(table_handle, kRPVControllerTableNumColumns, 0, &columns);
                        if (result == kRocProfVisResultSuccess)
                        {
                            ROCPROFVIS_ASSERT(columns > 0);
                            column_names.resize(columns);
                            for (uint64_t c = 0; c < columns; c ++)
                            {
                                result = GetStringPropertyFromHandle(table_handle, kRPVControllerTableColumnHeaderIndexed, c, column_names[c]);
                                if (result == kRocProfVisResultSuccess && column_names[c] == "Unit")
                                {
                                    unit_column = c;
                                }
                            }
                            uint64_t rows = 0;
                            result = rocprofvis_controller_get_uint64(table_handle, kRPVControllerTableNumRows, 0, &rows);
                            if (result == kRocProfVisResultSuccess)
                            {
                                ROCPROFVIS_ASSERT(rows > 0);
                                rocprofvis_controller_array_t* table_data = rocprofvis_controller_array_alloc(0);
                                ROCPROFVIS_ASSERT(table_data);
                                rocprofvis_controller_future_t* table_future = rocprofvis_controller_future_alloc();
                                ROCPROFVIS_ASSERT(table_future);
                                rocprofvis_controller_arguments_t* sort = rocprofvis_controller_arguments_alloc();
                                ROCPROFVIS_ASSERT(sort);                                
                                result = rocprofvis_controller_table_fetch_async(m_trace, table_handle, sort, table_future, table_data);
                                if (result == kRocProfVisResultSuccess)
                                {
                                    result = rocprofvis_controller_future_wait(table_future, FLT_MAX);
                                    if (result == kRocProfVisResultSuccess)
                                    {
                                        for (uint64_t r = 0; r < rows; r ++)
                                        {
                                            rocprofvis_handle_t* row_handle = nullptr;
                                            result = rocprofvis_controller_get_object(table_data, kRPVControllerArrayEntryIndexed, r, &row_handle);
                                            if (result == kRocProfVisResultSuccess)
                                            {
                                                ROCPROFVIS_ASSERT(row_handle);
                                                std::vector<ComputeTableCellModel> cell_row;
                                                std::string unit;
                                                uint32_t length = -1;
                                                if (unit_column >= 0)
                                                {
                                                    GetStringPropertyFromHandle(row_handle, kRPVControllerArrayEntryIndexed, unit_column, unit);
                                                }
                                                for (uint64_t c = 0; c < columns; c ++)
                                                {
                                                    if (column_names[c] == "Pct of Peak")
                                                    {
                                                        unit = "Pct of peak";
                                                    }
                                                    uint64_t type = -1;
                                                    result = rocprofvis_controller_get_uint64(table_handle, kRPVControllerTableColumnTypeIndexed, c, &type);
                                                    if (result == kRocProfVisResultSuccess)
                                                    {
                                                        ComputeTableCellModel model;
                                                        model.m_type = static_cast<rocprofvis_controller_primitive_type_t>(type);
                                                        model.m_colorize = false;
                                                        model.m_highlight = false;
                                                        switch (type)
                                                        {
                                                            case kRPVControllerPrimitiveTypeUInt64:
                                                            {
                                                                uint64_t data = 0;
                                                                result = rocprofvis_controller_get_uint64(row_handle, kRPVControllerArrayEntryIndexed, c, &data);
                                                                if (result == kRocProfVisResultSuccess && data != -1)
                                                                {
                                                                    model.m_str_value = std::to_string(data);
                                                                    if (c > 0)
                                                                    {
                                                                        model.m_colorize = ((unit == "Pct" || unit == "Pct of peak") && column_names[c] == "Avg") || column_names[c] == "Pct of Peak";
                                                                        model.m_num_value.m_uint64 = data;
                                                                    }
                                                                }
                                                                break;
                                                            }
                                                            case kRPVControllerPrimitiveTypeDouble:
                                                            {
                                                                double data = 0;
                                                                result = rocprofvis_controller_get_double(row_handle, kRPVControllerArrayEntryIndexed, c, &data);
                                                                if (result == kRocProfVisResultSuccess && data != -1)
                                                                {
                                                                    std::string data_str = std::to_string(data);
                                                                    model.m_str_value = TrimDecimalPlaces(data_str, 2);
                                                                    if (c > 0)
                                                                    {
                                                                        model.m_colorize = ((unit == "Pct" || unit == "Pct of peak") && column_names[c] == "Avg") || column_names[c] == "Pct of Peak";
                                                                        model.m_num_value.m_double = data;
                                                                    }
                                                                }
                                                                break;
                                                            }
                                                            case kRPVControllerPrimitiveTypeString:
                                                            {
                                                                result = GetStringPropertyFromHandle(row_handle, kRPVControllerArrayEntryIndexed, c, model.m_str_value);
                                                                break;
                                                            }
                                                            default:
                                                            {
                                                                ROCPROFVIS_ASSERT(false);
                                                                break;
                                                            }
                                                        }
                                                        if (result == kRocProfVisResultSuccess)
                                                        {
                                                            cell_row.push_back(std::move(model)); 
                                                        }                                                   
                                                    }
                                                }
                                                if (result == kRocProfVisResultSuccess)
                                                {
                                                    cells.push_back(std::move(cell_row));
                                                }                                                
                                            }
                                        }
                                    }
                                }
                                rocprofvis_controller_array_free(table_data);
                                rocprofvis_controller_future_free(table_future);
                                rocprofvis_controller_arguments_free(sort);
                            }
                        }
                        if (result == kRocProfVisResultSuccess)
                        {
                            m_tables[static_cast<rocprofvis_controller_compute_table_types_t>(table_type)] = std::make_unique<ComputeTableModel>(
                                ComputeTableModel{std::move(title), std::move(column_names), std::move(cells)}
                            );
                        }
                    }
                }
                rocprofvis_handle_t* plot_handle = nullptr;
                for (uint64_t plot_type = kRPVControllerComputePlotTypeKernelDurationPercentage; plot_type < kRPVControllerComputePlotTypeCount; plot_type ++)
                {
                    result = rocprofvis_controller_get_object(m_trace, plot_type, 0, &plot_handle);
                    if (result == kRocProfVisResultSuccess)
                    {
                        ROCPROFVIS_ASSERT(plot_handle);
                        std::string title;
                        ComputePlotAxisModel x_axis;
                        ComputePlotAxisModel y_axis;
                        std::vector<ComputePlotSeriesModel> series_models;
                        GetStringPropertyFromHandle(plot_handle, kRPVControllerPlotTitle, 0, title);
                        GetStringPropertyFromHandle(plot_handle, kRPVControllerPlotXAxisTitle, 0, x_axis.m_name);
                        GetStringPropertyFromHandle(plot_handle, kRPVControllerPlotYAxisTitle, 0, y_axis.m_name);
                        uint64_t num_axis_labels = 0;
                        result = rocprofvis_controller_get_uint64(plot_handle, kRPVControllerPlotNumXAxisLabels, 0, &num_axis_labels);
                        if (result == kRocProfVisResultSuccess && num_axis_labels > 0)
                        {
                            x_axis.m_tick_labels.resize(num_axis_labels);
                            for (int i = 0; i < num_axis_labels; i ++)
                            {
                                std::string data;
                                result = GetStringPropertyFromHandle(plot_handle, kRPVControllerPlotXAxisLabelsIndexed, i, data);
                                if (result == kRocProfVisResultSuccess)
                                {
                                    if (data.length() > PLOT_LABEL_MAX_LENGTH)
                                    {
                                        data = data.substr(0, PLOT_LABEL_MAX_LENGTH) + "...";
                                    }
                                    char* label = new char[data.length() + 1];
                                    ROCPROFVIS_ASSERT(strcpy(label, data.c_str()));
                                    x_axis.m_tick_labels[i] = label;
                                }
                            }
                        }
                        result = rocprofvis_controller_get_uint64(plot_handle, kRPVControllerPlotNumYAxisLabels, 0, &num_axis_labels);
                        if (result == kRocProfVisResultSuccess && num_axis_labels > 0)
                        {
                            y_axis.m_tick_labels.resize(num_axis_labels);
                            for (int i = 0; i < num_axis_labels; i ++)
                            {
                                std::string data;
                                result = GetStringPropertyFromHandle(plot_handle, kRPVControllerPlotYAxisLabelsIndexed, i, data);
                                if (result == kRocProfVisResultSuccess)
                                {
                                    char* label = new char[data.length() + 1];
                                    if (data.length() > PLOT_LABEL_MAX_LENGTH)
                                    {
                                        data = data.substr(0, PLOT_LABEL_MAX_LENGTH) + "...";
                                    }
                                    ROCPROFVIS_ASSERT(strcpy(label, data.c_str()));
                                    y_axis.m_tick_labels[i] = label;
                                }
                            }
                        }
                        uint64_t num_series = 0;
                        result = rocprofvis_controller_get_uint64(plot_handle, kRPVControllerPlotNumSeries, 0, &num_series);
                        if (result == kRocProfVisResultSuccess)
                        {
                            ROCPROFVIS_ASSERT(num_series > 0);
                            rocprofvis_controller_array_t* plot_data = rocprofvis_controller_array_alloc(0);
                            ROCPROFVIS_ASSERT(plot_data);
                            rocprofvis_controller_future_t* plot_future = rocprofvis_controller_future_alloc();
                            ROCPROFVIS_ASSERT(plot_future);
                            rocprofvis_controller_arguments_t* args = rocprofvis_controller_arguments_alloc();
                            ROCPROFVIS_ASSERT(args);
                            result = rocprofvis_controller_plot_fetch_async(m_controller, plot_handle, args, plot_future, plot_data);
                            if (result == kRocProfVisResultSuccess)
                            {
                                result = rocprofvis_controller_future_wait(plot_future, FLT_MAX);
                                if (result == kRocProfVisResultSuccess)
                                {
                                    for (uint64_t i = 0; i < num_series; i ++)
                                    {
                                        rocprofvis_handle_t* series_handle = nullptr;
                                        result = rocprofvis_controller_get_object(plot_data, kRPVControllerArrayEntryIndexed, i, &series_handle);
                                        if (result == kRocProfVisResultSuccess)
                                        {
                                            ROCPROFVIS_ASSERT(series_handle);
                                            std::string name;
                                            std::vector<double> x_values;
                                            std::vector<double> y_values;
                                            uint64_t num_values = 0;
                                            result = rocprofvis_controller_get_uint64(series_handle, kRPVControllerPlotSeriesNumValues, 0, &num_values);
                                            if (result == kRocProfVisResultSuccess)
                                            {
                                                ROCPROFVIS_ASSERT(num_values > 0);
                                                x_values.resize(num_values);
                                                y_values.resize(num_values);
                                                x_axis.m_max = 100;
                                                x_axis.m_min = 0;
                                                y_axis.m_max = 0;
                                                y_axis.m_min = 0;
                                                for (uint64_t j = 0; j < num_values; j ++)
                                                {
                                                    result = rocprofvis_controller_get_double(series_handle, kRPVControllerPlotSeriesXValuesIndexed, j, &x_values[j]);
                                                    if (result == kRocProfVisResultSuccess)
                                                    {
                                                        result = rocprofvis_controller_get_double(series_handle, kRPVControllerPlotSeriesYValuesIndexed, j, &y_values[j]);
                                                        if (result == kRocProfVisResultSuccess)
                                                        {
                                                            result = GetStringPropertyFromHandle(series_handle, kRPVControllerPlotSeriesName, 0, name);
                                                            x_axis.m_max = std::max(x_axis.m_max, x_values[j]);
                                                            x_axis.m_min = std::min(x_axis.m_min, x_values[j]);
                                                            y_axis.m_max = std::max(y_axis.m_max, y_values[j]);
                                                            y_axis.m_min = std::min(y_axis.m_min, y_values[j]);
                                                        }
                                                    }
                                                }
                                            }
                                            if (result == kRocProfVisResultSuccess)
                                            {
                                                series_models.emplace_back(ComputePlotSeriesModel{std::move(name), std::move(x_values), std::move(y_values)});
                                            }
                                        }
                                    }
                                    if (result == kRocProfVisResultSuccess)
                                    {
                                        m_plots[static_cast<rocprofvis_controller_compute_plot_types_t>(plot_type)] = std::make_unique<ComputePlotModel>(
                                            ComputePlotModel{std::move(title), std::move(x_axis), std::move(y_axis), std::move(series_models)}
                                        );
                                    }
                                }
                            }
                            rocprofvis_controller_array_free(plot_data);
                            rocprofvis_controller_future_free(plot_future);
                            rocprofvis_controller_arguments_free(args);
                        }
                    }
                }
                for (uint64_t metric_type = kRPVControllerComputeMetricTypeL2CacheRd; metric_type < kRPVControllerComputeMetricTypeCount; metric_type ++)
                {
                    uint64_t data;
                    result = rocprofvis_controller_get_uint64(m_trace, metric_type, 0, &data);
                    if (result == kRocProfVisResultSuccess)
                    {
                        std::unique_ptr<ComputeMetricModel> model = std::make_unique<ComputeMetricModel>();
                        model->m_type = kRPVControllerPrimitiveTypeUInt64;
                        model->m_value.m_uint64 = data;
                        m_metrics[static_cast<rocprofvis_controller_compute_metric_types_t>(metric_type)] = std::move(model);
                    }
                }
            }
        }
    }
    std::shared_ptr<RocEvent> dirty_event = std::make_shared<RocEvent>(static_cast<int>(RocEvents::kComputeDataDirty));
    dirty_event->SetAllowPropagate(false);
    EventManager::GetInstance()->AddEvent(dirty_event);
    return result;
}

ComputeTableModel* ComputeDataProvider2::GetTableModel(const rocprofvis_controller_compute_table_types_t type) const
{
    ComputeTableModel* table = nullptr;
    if (m_tables.count(type) > 0)
    {
        table = m_tables.at(type).get();
    }
    return table;
}

ComputePlotModel* ComputeDataProvider2::GetPlotModel(const rocprofvis_controller_compute_plot_types_t type) const
{
    ComputePlotModel* plot = nullptr;
    if (m_plots.count(type) > 0)
    {
        plot = m_plots.at(type).get();
    }
    return plot;
}

ComputeMetricModel* ComputeDataProvider2::GetMetricModel(const rocprofvis_controller_compute_metric_types_t type) const
{
    ComputeMetricModel* metric = nullptr;
    if (m_metrics.count(type) > 0)
    {
        return m_metrics.at(type).get();
    }
    return metric;
}

rocprofvis_result_t ComputeDataProvider2::GetStringPropertyFromHandle(rocprofvis_handle_t* handle, const rocprofvis_property_t property, const uint64_t index, std::string& output)
{
    uint32_t length = -1;
    rocprofvis_result_t result = rocprofvis_controller_get_string(handle, property, index, nullptr, &length);
    if (result == kRocProfVisResultSuccess)
    {
        ROCPROFVIS_ASSERT(length >= 0);
        char* data = new char[length + 1];
        data[length] = '\0';
        result = rocprofvis_controller_get_string(handle, property, index, data, &length);
        if (result == kRocProfVisResultSuccess)
        {
            output = data;
        }
        delete[] data;
    }
    return result;
}

std::string ComputeDataProvider2::TrimDecimalPlaces(std::string& double_str, const int decimal_places)
{
    size_t pos = double_str.find('.');
    if (pos != std::string::npos && decimal_places + 1 < double_str.size())
    {
        return double_str.erase(pos + decimal_places + 1, std::string::npos);
    }
    return double_str;
}

}  // namespace View
}  // namespace RocProfVis
