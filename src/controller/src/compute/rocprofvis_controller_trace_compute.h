// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_controller_trace.h"
#include "rocprofvis_c_interface.h"
#include <functional>
#include <optional>
#include <unordered_map>

namespace RocProfVis
{
namespace Controller
{

class Arguments;
class Array;
class Future;
class ComputeTable;
class Plot;
class ComputePlot;
class Workload;
class MetricsContainer;
class MetricID;

class ComputeTrace : public Trace
{
public:
    ComputeTrace(const std::string& filename);

    virtual ~ComputeTrace();

    virtual rocprofvis_result_t Init() final;

    virtual rocprofvis_result_t Load(Future& future) final;

    rocprofvis_controller_object_type_t GetType(void) final;

    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) final;

    rocprofvis_result_t AsyncFetch(Arguments& args, Future& future, MetricsContainer& output);

private:
    class MetricID
    {
    public:
        MetricID(uint32_t cateory_id);
        ~MetricID() = default;

        void SetTableID(uint32_t table_id);
        void SetEntryID(uint32_t entry_id);

        std::string ToString() const;

    private:
        uint32_t m_category_id;
        std::optional<uint32_t> m_table_id;
        std::optional<uint32_t> m_entry_id;
    };

    typedef std::vector<std::pair<rocprofvis_db_compute_param_enum_t, std::string>> QueryArgumentStore;
    typedef struct {
        std::unordered_map<rocprofvis_db_compute_column_enum_t, std::optional<int>> columns;
        std::vector<std::vector<const char*>> rows;
    } QueryDataStore;
    typedef std::function<void(const QueryDataStore&)> QueryCallback;

    rocprofvis_result_t LoadRocpd();
    rocprofvis_dm_result_t ExecuteQuery(rocprofvis_dm_database_t db, rocprofvis_dm_trace_t dm, rocprofvis_db_future_t db_future, Future* controller_future, rocprofvis_db_compute_use_case_enum_t use_case, QueryArgumentStore& argument_store, QueryDataStore& data_store, QueryCallback callback);

    std::vector<Workload*> m_workloads;
    QueryArgumentStore m_query_arguments;
    QueryDataStore m_query_output;

#pragma region Deprecated
public:   
    rocprofvis_result_t AsyncFetch(Plot& plot, Arguments& args, Future& future, Array& array);

private:
    /*DebugComputeTable function is for debugging purposes only. Feel free to refactor it or remove it */
    rocprofvis_result_t DebugComputeTable(rocprofvis_dm_table_id_t table_id, std::string query, std::string description);
    rocprofvis_result_t LoadCSV();
    rocprofvis_result_t GetMetric(const rocprofvis_controller_compute_metric_types_t metric_type, Data** value);
    std::unordered_map<rocprofvis_controller_compute_table_types_t, ComputeTable*> m_tables;
    std::unordered_map<rocprofvis_controller_compute_plot_types_t, ComputePlot*> m_plots;
#pragma endregion
};

}
}
