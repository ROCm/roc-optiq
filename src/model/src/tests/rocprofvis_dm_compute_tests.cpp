// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_c_interface.h"
#include "rocprofvis_core.h"
#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <map>
#include <set>
#include <string.h>
#include <vector>

#define MULTI_LINE_LOG_START auto multi_line_log = fmt::memory_buffer()
#define MULTI_LINE_LOG(format, ...)                                                      \
    fmt::format_to(std::back_inserter(multi_line_log), format, __VA_ARGS__)
#define MULTI_LINE_LOG_ARGS "{:.{}}", multi_line_log.data(), multi_line_log.size()

std::string g_input_file = "sample/rocprof_compute_23ed6f36.db";

int
main(int argc, char** argv)
{
    Catch::Session session;

    using namespace Catch::Clara;
    auto cli = session.cli() |
               Opt(g_input_file, "input_file")["--input_file"]("Path to input file");

    // Now pass the new composite back to Catch2 so it uses that
    session.cli(cli);

    // Let Catch2 (using Clara) parse the command line
    int returnCode = session.applyCommandLine(argc, argv);
    if(returnCode != 0)  // Indicates a command line error
        return returnCode;

    std::string log_file = "Testing/Temporary/rocprofvis_dm_compute_tests/" +
                           std::filesystem::path(g_input_file).filename().string() +
                           ".txt";
    rocprofvis_core_enable_log(log_file.c_str(), spdlog::level::trace);

    return session.run();
}

void
CheckMemoryFootprint(rocprofvis_dm_trace_t trace)
{
    uint64_t memory_used = 0;
    if(trace)
    {
        memory_used += rocprofvis_dm_get_property_as_uint64(
            trace, kRPVDMTraceMemoryFootprintUInt64, 0);
        rocprofvis_dm_database_t db =
            rocprofvis_dm_get_property_as_handle(trace, kRPVDMDatabaseHandle, 0);
        if(db)
        {
            memory_used += rocprofvis_db_get_memory_footprint(db);
        }
    }

    spdlog::info("\x1b[0mTotal memory utilization so far = {}", memory_used);
}

#define LIST_SIZE_LIMIT 2
#define HEADER_LEN      100

void
PrintHeader(const char* fmt, ...)
{
    std::string header;
    va_list     argptr;
    char        buffer[256];
    va_start(argptr, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, argptr);
    va_end(argptr);
    size_t text_len = strlen(buffer);
    if(HEADER_LEN > text_len) header.assign((HEADER_LEN - text_len) / 2, '*');

    spdlog::info("\x1b[0m{0}{1}{2}", header, std::string(buffer), header);
}

void
db_progress(rocprofvis_db_filename_t db_name, rocprofvis_db_future_id_t /*id*/,
            rocprofvis_db_progress_percent_t progress, rocprofvis_db_status_t status,
            rocprofvis_db_status_message_t msg, void* /*user_data*/)
{
    const char* str   = " ERROR ";
    const char* color = "\x1b[31m";
    if(status == kRPVDbSuccess)
    {
        color = "\x1b[32m";
        str   = " DONE ";
    }
    else if(status == kRPVDbBusy)
    {
        color = "\x1b[33m";
        str   = " BUSY ";
    }

    spdlog::info("{0}[{1}] {2} - {3} - {4}", color, db_name, progress, str, msg);
}

struct ComputeQueryResult
{
    std::map<rocprofvis_db_compute_column_enum_t, int> columns;
    std::vector<std::vector<std::string>>              rows;
};

rocprofvis_dm_result_t
ExecuteComputeQuery(
    rocprofvis_dm_database_t db, rocprofvis_db_compute_use_case_enum_t use_case,
    const std::vector<std::pair<rocprofvis_db_compute_param_enum_t, std::string>>& args,
    rocprofvis_dm_table_id_t& out_table_id)
{
    std::vector<rocprofvis_db_compute_param_t> query_args(args.size());
    for(size_t i = 0; i < args.size(); i++)
    {
        query_args[i] = { args[i].first, args[i].second.c_str() };
    }

    char*                  query  = nullptr;
    rocprofvis_dm_result_t result = rocprofvis_db_build_compute_query(
        db, use_case, (uint32_t) query_args.size(), query_args.data(), &query);
    if(result != kRocProfVisDmResultSuccess)
    {
        free(query);
        return result;
    }

    rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(db_progress);
    if(object2wait == nullptr)
    {
        free(query);
        return kRocProfVisDmResultInvalidParameter;
    }

    out_table_id = 0;
    result = rocprofvis_db_execute_compute_query_async(db, use_case, query, object2wait,
                                                       &out_table_id);
    if(result == kRocProfVisDmResultSuccess)
    {
        result = rocprofvis_db_future_wait(object2wait, UINT64_MAX);
    }

    rocprofvis_db_future_free(object2wait);
    free(query);
    return result;
}

ComputeQueryResult
ParseComputeQueryResult(rocprofvis_dm_trace_t dm, rocprofvis_dm_table_id_t table_id)
{
    ComputeQueryResult result;

    uint64_t num_tables =
        rocprofvis_dm_get_property_as_uint64(dm, kRPVDMNumberOfTablesUInt64, 0);
    REQUIRE(num_tables > 0);

    rocprofvis_dm_table_t table_handle =
        rocprofvis_dm_get_property_as_handle(dm, kRPVDMTableHandleByID, table_id);
    REQUIRE(table_handle != nullptr);

    uint64_t num_columns = rocprofvis_dm_get_property_as_uint64(
        table_handle, kRPVDMNumberOfTableColumnsUInt64, 0);
    uint64_t num_rows = rocprofvis_dm_get_property_as_uint64(
        table_handle, kRPVDMNumberOfTableRowsUInt64, 0);

    for(uint64_t i = 0; i < num_columns; i++)
    {
        rocprofvis_db_compute_column_enum_t col_enum =
            rocprofvis_db_compute_column_enum_t(rocprofvis_dm_get_property_as_uint64(
                table_handle, kRPVDMExtTableColumnEnumUInt64Indexed, i));
        result.columns[col_enum] = (int) i;
    }

    result.rows.resize(num_rows);
    for(uint64_t i = 0; i < num_rows; i++)
    {
        rocprofvis_dm_table_row_t row_handle = rocprofvis_dm_get_property_as_handle(
            table_handle, kRPVDMExtTableRowHandleIndexed, i);
        REQUIRE(row_handle != nullptr);

        uint64_t num_cells = rocprofvis_dm_get_property_as_uint64(
            row_handle, kRPVDMNumberOfTableRowCellsUInt64, 0);
        REQUIRE(num_cells == num_columns);

        result.rows[i].resize(num_columns);
        for(uint64_t j = 0; j < num_columns; j++)
        {
            const char* cell_data = rocprofvis_dm_get_property_as_charptr(
                row_handle, kRPVDMExtTableRowCellValueCharPtrIndexed, j);
            REQUIRE(cell_data != nullptr);
            result.rows[i][j] = cell_data;
        }
    }

    rocprofvis_dm_delete_table_at(dm, table_id);

    return result;
}

struct RocProfVisDMFixture
{
    struct WorkloadInfo
    {
        std::string                                     id;
        std::string                                     name;
        std::vector<std::string>                        kernel_uuids;
        std::set<std::string>                           metric_prefixes;
        std::vector<std::string>                        full_metric_ids;
        std::map<std::string, std::vector<std::string>> prefix_value_names;
    };

    mutable rocprofvis_dm_trace_t     m_trace = nullptr;
    mutable rocprofvis_dm_database_t  m_db    = nullptr;
    mutable std::vector<WorkloadInfo> m_workloads;
};

TEST_CASE_PERSISTENT_FIXTURE(RocProfVisDMFixture, "Compute Trace Data-Model Tests")
{
    // Allocates a new DM trace handle.
    // Fixture Writes: m_trace
    SECTION("Create Trace")
    {
        PrintHeader("Create trace");
        m_trace = rocprofvis_dm_create_trace();
        REQUIRE(nullptr != m_trace);
    }

    // Opens the database file and binds it to the trace handle.
    // Fixture Reads:  m_trace
    // Fixture Writes: m_db
    SECTION("Data-Model Initialisation")
    {
        CheckMemoryFootprint(m_trace);
        PrintHeader("Open database %s", g_input_file.c_str());
        m_db = rocprofvis_db_open_database(g_input_file.c_str(), kAutodetect);
        REQUIRE(nullptr != m_db);
        rocprofvis_dm_result_t bind_result =
            rocprofvis_dm_bind_trace_to_database(m_trace, m_db);
        REQUIRE(kRocProfVisDmResultSuccess == bind_result);
    }

    // Loads database metadata (schema version, indexes) asynchronously and waits.
    // Fixture Reads: m_db
    SECTION("Issue Read Metadata")
    {
        PrintHeader("Issue Read Metadata");
        rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(db_progress);
        REQUIRE(object2wait);
        spdlog::info("Issue metadata read");
        rocprofvis_dm_result_t meta_result =
            rocprofvis_db_read_metadata_async(m_db, object2wait);
        REQUIRE(kRocProfVisDmResultSuccess == meta_result);
        spdlog::info("Wait for metadata");
        rocprofvis_dm_result_t wait_result =
            rocprofvis_db_future_wait(object2wait, UINT64_MAX);
        REQUIRE(kRocProfVisDmResultSuccess == wait_result);
        rocprofvis_db_future_free(object2wait);
    }

    // Queries all available workloads and stores their id/name in the fixture.
    // Fixture Reads:  m_db, m_trace
    // Fixture Writes: m_workloads[].id, m_workloads[].name
    SECTION("Fetch List Of Workloads")
    {
        PrintHeader("Fetch list of workloads");
        rocprofvis_dm_table_id_t table_id = 0;
        rocprofvis_dm_result_t   dm_result =
            ExecuteComputeQuery(m_db, kRPVComputeFetchListOfWorkloads, {}, table_id);
        REQUIRE(kRocProfVisDmResultSuccess == dm_result);
        ComputeQueryResult result = ParseComputeQueryResult(m_trace, table_id);
        REQUIRE(result.rows.size() > 0);
        REQUIRE(result.columns.count(kRPVComputeColumnWorkloadId) > 0);
        REQUIRE(result.columns.count(kRPVComputeColumnWorkloadName) > 0);

        for(size_t i = 0; i < result.rows.size(); i++)
        {
            WorkloadInfo info;
            info.id   = result.rows[i][result.columns.at(kRPVComputeColumnWorkloadId)];
            info.name = result.rows[i][result.columns.at(kRPVComputeColumnWorkloadName)];
            REQUIRE(!info.id.empty());
            spdlog::info("Workload {}: id={}, name={}", i, info.id, info.name);
            m_workloads.push_back(std::move(info));
        }

        REQUIRE(!m_workloads.empty());
    }

    // Fetches the metric catalog for each workload. Builds unique metric prefixes
    // (tableId.subTableId) and reconstructs full metric IDs (tableId.subTableId.entryId).
    // Fixture Reads:  m_db, m_trace, m_workloads[].id
    // Fixture Writes: m_workloads[].metric_prefixes, m_workloads[].full_metric_ids
    SECTION("Fetch Workload Metrics Definition")
    {
        PrintHeader("Fetch workload metrics definition");
        rocprofvis_dm_table_id_t table_id = 0;
        rocprofvis_dm_result_t   dm_result;

        for(size_t w = 0; w < m_workloads.size(); w++)
        {
            WorkloadInfo& workload = m_workloads[w];
            spdlog::info("Metrics definition for workload {}", workload.id);

            dm_result = ExecuteComputeQuery(
                m_db, kRPVComputeFetchWorkloadMetricsDefinition,
                { { kRPVComputeParamWorkloadId, workload.id } }, table_id);
            REQUIRE(kRocProfVisDmResultSuccess == dm_result);
            ComputeQueryResult metrics = ParseComputeQueryResult(m_trace, table_id);

            REQUIRE(metrics.columns.count(kRPVComputeColumnMetricName) > 0);
            REQUIRE(metrics.columns.count(kRPVComputeColumnTableId) > 0);
            REQUIRE(metrics.columns.count(kRPVComputeColumnSubTableId) > 0);

            int table_id_col     = metrics.columns.at(kRPVComputeColumnTableId);
            int sub_table_id_col = metrics.columns.at(kRPVComputeColumnSubTableId);

            std::map<std::string, int> prefix_entry_count;
            for(size_t i = 0; i < metrics.rows.size(); i++)
            {
                const std::string& name =
                    metrics.rows[i][metrics.columns.at(kRPVComputeColumnMetricName)];
                const std::string& tid  = metrics.rows[i][table_id_col];
                const std::string& stid = metrics.rows[i][sub_table_id_col];
                REQUIRE(!name.empty());
                spdlog::info("  Metric {}: name={}, tableId={}, subTableId={}", i, name,
                             tid, stid);

                if(!tid.empty() && !stid.empty())
                {
                    std::string prefix = tid + "." + stid;
                    workload.metric_prefixes.insert(prefix);
                    int entry_id = prefix_entry_count[prefix]++;
                    workload.full_metric_ids.push_back(prefix + "." +
                                                       std::to_string(entry_id));
                }
            }
        }
    }

    // Queries distinct metric value names for each workload's metric prefixes.
    // Fixture Reads:  m_db, m_trace, m_workloads[].id, m_workloads[].metric_prefixes
    // Fixture Writes: m_workloads[].prefix_value_names
    SECTION("Fetch Workload Metric Value Names")
    {
        PrintHeader("Fetch workload metric value names");
        rocprofvis_dm_table_id_t table_id = 0;
        rocprofvis_dm_result_t   dm_result;

        for(size_t w = 0; w < m_workloads.size(); w++)
        {
            WorkloadInfo& workload = m_workloads[w];

            for(const std::string& metric_prefix : workload.metric_prefixes)
            {
                spdlog::info("Value names for workload={} metric={}", workload.id,
                             metric_prefix);

                dm_result =
                    ExecuteComputeQuery(m_db, kRPVComputeFetchWorkloadMetricValueNames,
                                        { { kRPVComputeParamWorkloadId, workload.id },
                                          { kRPVComputeParamMetricId, metric_prefix } },
                                        table_id);
                REQUIRE(kRocProfVisDmResultSuccess == dm_result);
                ComputeQueryResult value_names =
                    ParseComputeQueryResult(m_trace, table_id);

                REQUIRE(value_names.columns.count(kRPVComputeColumnMetricValueName) > 0);
                for(size_t i = 0; i < value_names.rows.size(); i++)
                {
                    const std::string& name = value_names.rows[i][value_names.columns.at(
                        kRPVComputeColumnMetricValueName)];
                    spdlog::info("  Value name {}: {}", i, name);
                    workload.prefix_value_names[metric_prefix].push_back(name);
                }
            }
        }
    }

    // Fetches the top kernels for each workload, storing their UUIDs in the fixture.
    // Fixture Reads:  m_db, m_trace, m_workloads[].id
    // Fixture Writes: m_workloads[].kernel_uuids
    SECTION("Fetch Workload Top Kernels")
    {
        PrintHeader("Fetch workload top kernels");
        rocprofvis_dm_table_id_t table_id = 0;
        rocprofvis_dm_result_t   dm_result;

        for(size_t w = 0; w < m_workloads.size(); w++)
        {
            WorkloadInfo& workload = m_workloads[w];
            spdlog::info("Top kernels for workload {}", workload.id);

            dm_result = ExecuteComputeQuery(
                m_db, kRPVComputeFetchWorkloadTopKernels,
                { { kRPVComputeParamWorkloadId, workload.id } }, table_id);
            REQUIRE(kRocProfVisDmResultSuccess == dm_result);
            ComputeQueryResult kernels = ParseComputeQueryResult(m_trace, table_id);

            REQUIRE(kernels.columns.count(kRPVComputeColumnKernelUUID) > 0);
            REQUIRE(kernels.columns.count(kRPVComputeColumnKernelName) > 0);

            for(size_t i = 0; i < kernels.rows.size(); i++)
            {
                const std::string& uuid =
                    kernels.rows[i][kernels.columns.at(kRPVComputeColumnKernelUUID)];
                const std::string& name =
                    kernels.rows[i][kernels.columns.at(kRPVComputeColumnKernelName)];
                REQUIRE(!uuid.empty());
                spdlog::info("  Kernel {}: uuid={}, name={}", i, uuid, name);
                workload.kernel_uuids.push_back(uuid);
            }
        }
    }

    // Fetches roofline intensity data (flops, cache) for each kernel in each workload.
    // Fixture Reads: m_db, m_trace, m_workloads[].kernel_uuids
    SECTION("Fetch Kernel Roofline Intensities")
    {
        PrintHeader("Fetch kernel roofline intensities");
        rocprofvis_dm_table_id_t table_id = 0;
        rocprofvis_dm_result_t   dm_result;

        for(size_t w = 0; w < m_workloads.size(); w++)
        {
            const WorkloadInfo& workload = m_workloads[w];

            for(size_t k = 0; k < workload.kernel_uuids.size(); k++)
            {
                const std::string& kernel_id = workload.kernel_uuids[k];
                spdlog::info("Roofline intensities for kernel {}", kernel_id);

                dm_result = ExecuteComputeQuery(
                    m_db, kRPVComputeFetchKernelRooflineIntensities,
                    { { kRPVComputeParamKernelId, kernel_id } }, table_id);
                REQUIRE(kRocProfVisDmResultSuccess == dm_result);
                ComputeQueryResult roofline = ParseComputeQueryResult(m_trace, table_id);

                REQUIRE(roofline.columns.count(kRPVComputeColumnKernelUUID) > 0);
                REQUIRE(roofline.columns.count(kRPVComputeColumnRooflineTotalFlops) > 0);

                for(size_t i = 0; i < roofline.rows.size(); i++)
                {
                    spdlog::info("  Row {}: flops={}", i,
                                 roofline.rows[i][roofline.columns.at(
                                     kRPVComputeColumnRooflineTotalFlops)]);
                }
            }
        }
    }

    // Fetches roofline ceiling benchmarks (bandwidth/compute throughputs) per workload.
    // Fixture Reads: m_db, m_trace, m_workloads[].id
    SECTION("Fetch Workload Roofline Ceiling")
    {
        PrintHeader("Fetch workload roofline ceiling");
        rocprofvis_dm_table_id_t table_id = 0;
        rocprofvis_dm_result_t   dm_result;

        for(size_t w = 0; w < m_workloads.size(); w++)
        {
            const WorkloadInfo& workload = m_workloads[w];
            spdlog::info("Roofline ceiling for workload {}", workload.id);

            dm_result = ExecuteComputeQuery(
                m_db, kRPVComputeFetchWorkloadRooflineCeiling,
                { { kRPVComputeParamWorkloadId, workload.id } }, table_id);
            REQUIRE(kRocProfVisDmResultSuccess == dm_result);
            ComputeQueryResult ceiling = ParseComputeQueryResult(m_trace, table_id);

            spdlog::info("  Ceiling query returned {} rows with {} columns",
                         ceiling.rows.size(), ceiling.columns.size());

            for(const std::pair<const rocprofvis_db_compute_column_enum_t, int>&
                    col_entry : ceiling.columns)
            {
                spdlog::info("  Column enum={}, index={}", (int) col_entry.first,
                             col_entry.second);
            }
        }
    }

    // Fetches per-kernel aggregated metric values for all kernels and metric prefixes.
    // Fixture Reads: m_db, m_trace, m_workloads[].id, m_workloads[].kernel_uuids,
    //        m_workloads[].metric_prefixes
    SECTION("Fetch Kernel Metric Values")
    {
        PrintHeader("Fetch kernel metric values");
        rocprofvis_dm_table_id_t table_id = 0;
        rocprofvis_dm_result_t   dm_result;

        for(size_t w = 0; w < m_workloads.size(); w++)
        {
            const WorkloadInfo& workload = m_workloads[w];

            if(!workload.kernel_uuids.empty() && !workload.metric_prefixes.empty())
            {
                std::vector<std::pair<rocprofvis_db_compute_param_enum_t, std::string>>
                    args;
                for(const std::string& uuid : workload.kernel_uuids)
                {
                    args.push_back({ kRPVComputeParamKernelId, uuid });
                }
                for(const std::string& prefix : workload.metric_prefixes)
                {
                    args.push_back({ kRPVComputeParamMetricId, prefix });
                }

                spdlog::info("Metric values for workload {}", workload.id);
                dm_result = ExecuteComputeQuery(m_db, kRPVComputeFetchMetricValues, args,
                                                table_id);
                REQUIRE(kRocProfVisDmResultSuccess == dm_result);
                ComputeQueryResult values = ParseComputeQueryResult(m_trace, table_id);

                REQUIRE(values.columns.count(kRPVComputeColumnMetricId) > 0);
                REQUIRE(values.columns.count(kRPVComputeColumnMetricName) > 0);
                REQUIRE(values.columns.count(kRPVComputeColumnMetricValueName) > 0);
                REQUIRE(values.columns.count(kRPVComputeColumnMetricValue) > 0);

                spdlog::info("  Metric values: {} rows", values.rows.size());
                for(size_t i = 0; i < values.rows.size(); i++)
                {
                    spdlog::trace(
                        "  Row {}: metric_id={}, name={}, value_name={}, value={}", i,
                        values.rows[i][values.columns.at(kRPVComputeColumnMetricId)],
                        values.rows[i][values.columns.at(kRPVComputeColumnMetricName)],
                        values
                            .rows[i][values.columns.at(kRPVComputeColumnMetricValueName)],
                        values.rows[i][values.columns.at(kRPVComputeColumnMetricValue)]);
                }
            }
        }
    }

    // Fetches per-workload aggregated metric values for all metric prefixes.
    // Falls back gracefully if the DB version does not support this query.
    // Fixture Reads: m_db, m_trace, m_workloads[].id, m_workloads[].metric_prefixes
    SECTION("Fetch Workload Metric Values")
    {
        PrintHeader("Fetch workload metric values");
        rocprofvis_dm_table_id_t table_id = 0;
        rocprofvis_dm_result_t   dm_result;

        for(size_t w = 0; w < m_workloads.size(); w++)
        {
            const WorkloadInfo& workload = m_workloads[w];

            if(!workload.metric_prefixes.empty())
            {
                std::vector<std::pair<rocprofvis_db_compute_param_enum_t, std::string>>
                    args;
                args.push_back({ kRPVComputeParamWorkloadId, workload.id });
                for(const std::string& prefix : workload.metric_prefixes)
                {
                    args.push_back({ kRPVComputeParamMetricId, prefix });
                }

                spdlog::info("Metric values by workload {}", workload.id);
                dm_result = ExecuteComputeQuery(
                    m_db, kRPVComputeFetchMetricValuesByWorkload, args, table_id);

                if(kRocProfVisDmResultSuccess == dm_result)
                {
                    ComputeQueryResult values =
                        ParseComputeQueryResult(m_trace, table_id);

                    REQUIRE(values.columns.count(kRPVComputeColumnMetricId) > 0);
                    REQUIRE(values.columns.count(kRPVComputeColumnMetricName) > 0);
                    REQUIRE(values.columns.count(kRPVComputeColumnMetricValueName) > 0);
                    REQUIRE(values.columns.count(kRPVComputeColumnMetricValue) > 0);

                    spdlog::info("  Metric values by workload: {} rows",
                                 values.rows.size());
                    for(size_t i = 0; i < values.rows.size(); i++)
                    {
                        spdlog::trace(
                            "  Row {}: metric_id={}, name={}, value_name={}, value={}", i,
                            values.rows[i][values.columns.at(kRPVComputeColumnMetricId)],
                            values
                                .rows[i][values.columns.at(kRPVComputeColumnMetricName)],
                            values.rows[i][values.columns.at(
                                kRPVComputeColumnMetricValueName)],
                            values.rows[i]
                                       [values.columns.at(kRPVComputeColumnMetricValue)]);
                    }
                }
                else
                {
                    spdlog::info("FetchMetricValuesByWorkload unsupported");
                }
            }
        }
    }

    // Builds metric selectors from fixture data and executes a kernel metrics matrix
    // (pivot table) query. Validates the result has the 4 static columns plus one
    // dynamic column per selector, and that each row has a valid kernel name and
    // duration.
    // Fixture Reads: m_db, m_trace, m_workloads[].id, m_workloads[].kernel_uuids,
    //        m_workloads[].full_metric_ids, m_workloads[].prefix_value_names
    SECTION("Fetch Kernel Metrics Matrix")
    {
        PrintHeader("Fetch kernel metrics matrix (pivot table)");
        rocprofvis_dm_table_id_t table_id = 0;
        rocprofvis_dm_result_t   dm_result;

        for(size_t w = 0; w < m_workloads.size(); w++)
        {
            const WorkloadInfo& workload = m_workloads[w];
            if(workload.full_metric_ids.empty() || workload.prefix_value_names.empty() ||
               workload.kernel_uuids.empty())
                continue;

            spdlog::info("Building metric selectors for workload {}/{} (id={})", w + 1,
                         m_workloads.size(), workload.id);

            std::vector<std::pair<rocprofvis_db_compute_param_enum_t, std::string>> args;
            args.push_back({ kRPVComputeParamWorkloadId, workload.id });

            size_t num_selectors = 0;
            for(const std::string& mid : workload.full_metric_ids)
            {
                size_t      last_dot = mid.rfind('.');
                std::string prefix   = mid.substr(0, last_dot);

                auto vnames_it = workload.prefix_value_names.find(prefix);
                if(vnames_it != workload.prefix_value_names.end())
                {
                    for(const std::string& vname : vnames_it->second)
                    {
                        args.push_back(
                            { kRPVComputeParamMetricSelector, mid + ":" + vname });
                        num_selectors++;
                    }
                }
            }

            args.push_back({ kRPVComputeParamSortColumnIndex, "0" });
            args.push_back({ kRPVComputeParamSortColumnOrder, "ASC" });

            spdlog::info("  Built {} metric selectors", num_selectors);
            REQUIRE(num_selectors > 0);

            spdlog::info("Kernel metrics matrix for workload {}", workload.id);
            dm_result = ExecuteComputeQuery(m_db, kRPVComputeFetchKernelMetricsMatrix,
                                            args, table_id);
            REQUIRE(kRocProfVisDmResultSuccess == dm_result);
            ComputeQueryResult matrix = ParseComputeQueryResult(m_trace, table_id);

            REQUIRE(matrix.columns.count(kRPVComputeColumnDynamicKernelUUID) > 0);
            REQUIRE(matrix.columns.count(kRPVComputeColumnKernelName) > 0);
            REQUIRE(matrix.columns.count(kRPVComputeColumnKernelsCount) > 0);
            REQUIRE(matrix.columns.count(kRPVComputeColumnKernelDurationsSum) > 0);

            REQUIRE(matrix.rows.size() > 0);
            size_t total_columns = matrix.rows[0].size();
            spdlog::info("  Matrix: {} rows x {} columns (expected {} dynamic)",
                         matrix.rows.size(), total_columns, num_selectors);
            REQUIRE(total_columns == 4 + num_selectors);

            for(size_t i = 0; i < matrix.rows.size(); i++)
            {
                const std::string& kernel_name =
                    matrix.rows[i][matrix.columns.at(kRPVComputeColumnKernelName)];
                REQUIRE(!kernel_name.empty());

                const std::string& duration =
                    matrix
                        .rows[i][matrix.columns.at(kRPVComputeColumnKernelDurationsSum)];
                REQUIRE(!duration.empty());

                spdlog::info("  Row {}: kernel={} duration_sum={}", i, kernel_name,
                             duration);
            }
        }
    }

    // Reports memory footprint and frees the trace handle.
    // Fixture Reads: m_trace
    SECTION("Delete trace")
    {
        CheckMemoryFootprint(m_trace);
        PrintHeader("Delete trace");
        rocprofvis_dm_delete_trace(m_trace);
    }
}
