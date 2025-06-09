// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_compute_data_provider2.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_event_manager.h"
#include "spdlog/spdlog.h"

namespace RocProfVis
{
namespace View
{

ComputeDataProvider2::ComputeDataProvider2()
: m_controller(nullptr)
, m_controller_future(nullptr)
, m_trace(nullptr)
{

}

ComputeDataProvider2::~ComputeDataProvider2() 
{
    FreeController();
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
                        std::unordered_map<std::string, ComputeTableNumericMetricModel> metrics_map;
                        uint64_t unit_column = -1;
                        uint32_t length = -1;
                        result = rocprofvis_controller_get_string(table_handle, kRPVControllerTableTitle, 0, nullptr, &length);
                        if (result == kRocProfVisResultSuccess)
                        {
                            ROCPROFVIS_ASSERT(length >= 0);
                            char* data = new char[length + 1];
                            data[length] = '\0';
                            result = rocprofvis_controller_get_string(table_handle, kRPVControllerTableTitle, 0, data, &length);
                            if (result == kRocProfVisResultSuccess)
                            {
                                title = data;
                            }
                            delete[] data;
                        }
                        uint64_t columns = 0;
                        result = rocprofvis_controller_get_uint64(table_handle, kRPVControllerTableNumColumns, 0, &columns);
                        if (result == kRocProfVisResultSuccess)
                        {
                            ROCPROFVIS_ASSERT(columns > 0);
                            for (uint64_t c = 0; c < columns; c ++)
                            {
                                uint32_t length = -1;
                                result = rocprofvis_controller_get_string(table_handle, kRPVControllerTableColumnHeaderIndexed, c, nullptr, &length);
                                if (result == kRocProfVisResultSuccess)
                                {
                                    ROCPROFVIS_ASSERT(length >= 0);
                                    char* data = new char[length + 1];
                                    data[length] = '\0';
                                    result = rocprofvis_controller_get_string(table_handle, kRPVControllerTableColumnHeaderIndexed, c, data, &length);
                                    if (result == kRocProfVisResultSuccess)
                                    {
                                        column_names.push_back(data);
                                        if (strcmp(data, "Unit") == 0)
                                        {
                                            unit_column = c;
                                        }
                                    }
                                    delete[] data;
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
                                            rocprofvis_handle_t* row = nullptr;
                                            result = rocprofvis_controller_get_object(table_data, kRPVControllerArrayEntryIndexed, r, &row);
                                            if (result == kRocProfVisResultSuccess)
                                            {
                                                ROCPROFVIS_ASSERT(row);
                                                std::vector<ComputeTableCellModel> cell_row;
                                                std::string unit;
                                                uint32_t length = -1;
                                                if (unit_column >= 0 && kRocProfVisResultSuccess == rocprofvis_controller_get_string(row, kRPVControllerArrayEntryIndexed, unit_column, nullptr, &length))
                                                {
                                                    ROCPROFVIS_ASSERT(length >= 0);
                                                    char* data = new char[length + 1];
                                                    data[length] = '\0';
                                                    result = rocprofvis_controller_get_string(row, kRPVControllerArrayEntryIndexed, unit_column, data, &length);
                                                    if (result == kRocProfVisResultSuccess)
                                                    {
                                                        unit = data;
                                                    }
                                                    delete[] data;
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
                                                        std::string value;
                                                        bool colorize = false;
                                                        ComputeTableNumericMetricModel* metric = nullptr;
                                                        switch (type)
                                                        {
                                                            case kRPVControllerPrimitiveTypeUInt64:
                                                            {
                                                                uint64_t data = 0;
                                                                result = rocprofvis_controller_get_uint64(row, kRPVControllerArrayEntryIndexed, c, &data);
                                                                if (result == kRocProfVisResultSuccess && data != -1)
                                                                {
                                                                    value = std::to_string(data);
                                                                    if (c > 0)
                                                                    {
                                                                        colorize = ((unit == "Pct" || unit == "Pct of peak") && column_names[c] == "Avg") || column_names[c] == "Pct of Peak";
                                                                        metrics_map[cell_row[0].m_value + " " + column_names[c]] = ComputeTableNumericMetricModel{static_cast<double>(data), unit};
                                                                        metric = &metrics_map[cell_row[0].m_value + " " + column_names[c]];
                                                                    }
                                                                }
                                                                break;
                                                            }
                                                            case kRPVControllerPrimitiveTypeDouble:
                                                            {
                                                                double data = 0;
                                                                result = rocprofvis_controller_get_double(row, kRPVControllerArrayEntryIndexed, c, &data);
                                                                if (result == kRocProfVisResultSuccess && data != -1)
                                                                {
                                                                    value = TrimDecimalPlaces(std::to_string(data), 2);
                                                                    if (c > 0)
                                                                    {
                                                                        colorize = ((unit == "Pct" || unit == "Pct of peak") && column_names[c] == "Avg") || column_names[c] == "Pct of Peak";
                                                                        metrics_map[cell_row[0].m_value + " " + column_names[c]] = ComputeTableNumericMetricModel{data, unit};
                                                                        metric = &metrics_map[cell_row[0].m_value + " " + column_names[c]];
                                                                    }
                                                                }
                                                                break;
                                                            }
                                                            case kRPVControllerPrimitiveTypeString:
                                                            {
                                                                uint32_t length = -1;
                                                                result = rocprofvis_controller_get_string(row, kRPVControllerArrayEntryIndexed, c, nullptr, &length);
                                                                if (result == kRocProfVisResultSuccess)
                                                                {
                                                                    ROCPROFVIS_ASSERT(length >= 0);
                                                                    char* data = new char[length + 1];
                                                                    data[length] = '\0';
                                                                    result = rocprofvis_controller_get_string(row, kRPVControllerArrayEntryIndexed, c, data, &length);
                                                                    if (result == kRocProfVisResultSuccess)
                                                                    {
                                                                        value = data;
                                                                    }
                                                                    delete[] data;
                                                                }
                                                                break;
                                                            }
                                                            default:
                                                            {
                                                                ROCPROFVIS_ASSERT(false);
                                                                break;
                                                            }
                                                        }
                                                        cell_row.push_back(ComputeTableCellModel{std::move(value), std::move(colorize), false, std::move(metric)});                                                    
                                                    }
                                                }
                                                cells.push_back(std::move(cell_row));
                                            }
                                        }
                                    }
                                }
                                rocprofvis_controller_array_free(table_data);
                                rocprofvis_controller_future_free(table_future);
                                rocprofvis_controller_arguments_free(sort);
                            }
                        }
                        m_tables[static_cast<rocprofvis_controller_compute_table_types_t>(table_type)] = std::make_unique<ComputeTableModel>(
                            ComputeTableModel{std::move(title), std::move(column_names), std::move(cells), std::move(metrics_map)}
                        );
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

ComputeTableModel* ComputeDataProvider2::GetTableModel(const rocprofvis_controller_compute_table_types_t type)
{
    ComputeTableModel* table = nullptr;
    if (m_tables.count(type) > 0)
    {
        table = m_tables[type].get();
    }
    return table;
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
