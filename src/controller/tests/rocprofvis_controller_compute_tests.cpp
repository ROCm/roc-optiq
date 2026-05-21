// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller.h"
#include "rocprofvis_core.h"
#include <algorithm>
#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cfloat>
#include <filesystem>
#include <unordered_map>

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

    std::string log_file = "Testing/Temporary/rocprofvis_controller_compute_tests/" +
                           std::filesystem::path(g_input_file).filename().string() +
                           ".txt";
    rocprofvis_core_enable_log(log_file.c_str(), spdlog::level::trace);

    return session.run();
}

struct RocProfVisControllerFixture
{
    struct AvailableMetrics
    {
        struct Entry
        {
            uint64_t category_id = 0;
            uint64_t table_id    = 0;
            uint64_t id          = 0;
        };
        struct Table
        {
            uint64_t                 id          = 0;
            uint64_t                 entry_count = 0;
            std::vector<std::string> value_names;
        };
        struct Category
        {
            uint64_t                            id = 0;
            std::unordered_map<uint64_t, Table> tables;
        };
        std::vector<Entry>                     list;
        std::unordered_map<uint64_t, Category> tree;
    };
    struct KernelInfo
    {
        uint64_t id = 0;
    };
    struct WorkloadInfo
    {
        uint64_t                id     = 0;
        rocprofvis_handle_t*    handle = nullptr;
        AvailableMetrics        available_metrics;
        std::vector<KernelInfo> kernels;
    };

    mutable rocprofvis_controller_t*  m_controller = nullptr;
    mutable std::vector<WorkloadInfo> m_workloads;
};

TEST_CASE_PERSISTENT_FIXTURE(RocProfVisControllerFixture,
                             "Compute Trace Controller Tests")
{
    // Allocates a controller handle for the input trace file.
    // Fixture Writes: m_controller
    SECTION("Create Controller")
    {
        spdlog::info("Allocating Controller");
        m_controller = rocprofvis_controller_alloc(g_input_file.c_str());
        REQUIRE(nullptr != m_controller);
    }

    // Loads the trace asynchronously and waits for completion.
    // Fixture Reads: m_controller
    SECTION("Controller Initialisation")
    {
        spdlog::info("Allocating Future");
        rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
        REQUIRE(future != nullptr);

        spdlog::info("Load trace: {0}", g_input_file);
        rocprofvis_result_t result =
            rocprofvis_controller_load_async(m_controller, future);
        REQUIRE(result == kRocProfVisResultSuccess);

        spdlog::info("Wait for load");
        result = rocprofvis_controller_future_wait(future, FLT_MAX);
        REQUIRE(result == kRocProfVisResultSuccess);

        uint64_t future_result = 0;
        result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult, 0,
                                                  &future_result);
        spdlog::info("Get future result: {0}", future_result);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(future_result == kRocProfVisResultSuccess);

        spdlog::info("Free Future");
        rocprofvis_controller_future_free(future);
    }

    // Discovers all workloads and reads their id, name, system info, and configuration.
    // Fixture Reads:  m_controller
    // Fixture Writes: m_workloads[].id, m_workloads[].handle
    SECTION("Controller Load Workload Data")
    {
        uint64_t            num_workloads = 0;
        rocprofvis_result_t result        = rocprofvis_controller_get_uint64(
            m_controller, kRPVControllerNumWorkloads, 0, &num_workloads);
        spdlog::info("Get num workloads: {0}", num_workloads);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(num_workloads > 0);

        for(uint64_t i = 0; i < num_workloads; i++)
        {
            WorkloadInfo workload;

            rocprofvis_handle_t* workload_handle = nullptr;
            result                               = rocprofvis_controller_get_object(
                m_controller, kRPVControllerWorkloadIndexed, i, &workload_handle);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(workload_handle);
            workload.handle = workload_handle;

            uint64_t workload_id = 0;
            result               = rocprofvis_controller_get_uint64(
                workload_handle, kRPVControllerWorkloadId, 0, &workload_id);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(workload_id > 0);
            workload.id = workload_id;
            spdlog::info("Processing workload {0}/{1} (id={2})", i + 1, num_workloads,
                         workload_id);

            uint32_t name_len = 0;
            result            = rocprofvis_controller_get_string(
                workload_handle, kRPVControllerWorkloadName, 0, nullptr, &name_len);
            REQUIRE(result == kRocProfVisResultSuccess);

            std::string workload_name;
            workload_name.resize(name_len);
            result = rocprofvis_controller_get_string(
                workload_handle, kRPVControllerWorkloadName, 0,
                const_cast<char*>(workload_name.c_str()), &name_len);
            spdlog::info("Workload {0} name: {1}", i, workload_name);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(!workload_name.empty());

            uint64_t num_sys_info = 0;
            result                = rocprofvis_controller_get_uint64(
                workload_handle, kRPVControllerWorkloadSystemInfoNumEntries, 0,
                &num_sys_info);
            spdlog::info("Workload {0} system info entries: {1}", i, num_sys_info);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(num_sys_info > 0);

            for(uint64_t j = 0; j < num_sys_info; j++)
            {
                spdlog::info("Reading system info entry {0}/{1}", j + 1, num_sys_info);
                uint32_t len = 0;
                result       = rocprofvis_controller_get_string(
                    workload_handle, kRPVControllerWorkloadSystemInfoEntryNameIndexed, j,
                    nullptr, &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                std::string entry_name;
                entry_name.resize(len);
                result = rocprofvis_controller_get_string(
                    workload_handle, kRPVControllerWorkloadSystemInfoEntryNameIndexed, j,
                    const_cast<char*>(entry_name.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                len    = 0;
                result = rocprofvis_controller_get_string(
                    workload_handle, kRPVControllerWorkloadSystemInfoEntryValueIndexed, j,
                    nullptr, &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                std::string entry_value;
                entry_value.resize(len);
                result = rocprofvis_controller_get_string(
                    workload_handle, kRPVControllerWorkloadSystemInfoEntryValueIndexed, j,
                    const_cast<char*>(entry_value.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                spdlog::trace("  SysInfo {0}: {1} = {2}", j, entry_name, entry_value);
            }

            uint64_t num_config = 0;
            result              = rocprofvis_controller_get_uint64(
                workload_handle, kRPVControllerWorkloadConfigurationNumEntries, 0,
                &num_config);
            spdlog::info("Workload {0} config entries: {1}", i, num_config);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(num_config > 0);

            for(uint64_t j = 0; j < num_config; j++)
            {
                spdlog::info("Reading config entry {0}/{1}", j + 1, num_config);
                uint32_t len = 0;
                result       = rocprofvis_controller_get_string(
                    workload_handle, kRPVControllerWorkloadConfigurationEntryNameIndexed,
                    j, nullptr, &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                std::string cfg_name;
                cfg_name.resize(len);
                result = rocprofvis_controller_get_string(
                    workload_handle, kRPVControllerWorkloadConfigurationEntryNameIndexed,
                    j, const_cast<char*>(cfg_name.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                len    = 0;
                result = rocprofvis_controller_get_string(
                    workload_handle, kRPVControllerWorkloadConfigurationEntryValueIndexed,
                    j, nullptr, &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                std::string cfg_value;
                cfg_value.resize(len);
                result = rocprofvis_controller_get_string(
                    workload_handle, kRPVControllerWorkloadConfigurationEntryValueIndexed,
                    j, const_cast<char*>(cfg_value.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                spdlog::trace("  Config {0}: {1} = {2}", j, cfg_name, cfg_value);
            }

            m_workloads.push_back(std::move(workload));
        }

        REQUIRE(!m_workloads.empty());
    }

    // Loads the available metrics catalog for each workload: category IDs, table IDs,
    // metric names, descriptions, units, and category/table names. Builds the
    // available_metrics.list (flat entry list) and available_metrics.tree (nested map).
    // Fixture Reads:  m_workloads[].handle
    // Fixture Writes: m_workloads[].available_metrics.list,
    // m_workloads[].available_metrics.tree
    SECTION("Controller Load Available Metrics Catalog")
    {
        for(size_t workload_idx = 0; workload_idx < m_workloads.size(); workload_idx++)
        {
            auto& workload = m_workloads[workload_idx];
            spdlog::info("Loading available metrics for workload {0}/{1} (id={2})",
                         workload_idx + 1, m_workloads.size(), workload.id);
            uint64_t            num_metrics = 0;
            rocprofvis_result_t result      = rocprofvis_controller_get_uint64(
                workload.handle, kRPVControllerWorkloadNumAvailableMetrics, 0,
                &num_metrics);
            spdlog::info("Workload id {0} available metrics: {1}", workload.id,
                         num_metrics);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(num_metrics > 0);

            for(uint64_t j = 0; j < num_metrics; j++)
            {
                spdlog::info("Processing available metric {0}/{1}", j + 1, num_metrics);
                uint64_t category_id = 0;
                result               = rocprofvis_controller_get_uint64(
                    workload.handle,
                    kRPVControllerWorkloadAvailableMetricCategoryIdIndexed, j,
                    &category_id);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(category_id > 0);

                uint64_t table_id = 0;
                result            = rocprofvis_controller_get_uint64(
                    workload.handle, kRPVControllerWorkloadAvailableMetricTableIdIndexed,
                    j, &table_id);
                REQUIRE(result == kRocProfVisResultSuccess);

                uint32_t len = 0;
                result       = rocprofvis_controller_get_string(
                    workload.handle, kRPVControllerWorkloadAvailableMetricNameIndexed, j,
                    nullptr, &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                std::string metric_name;
                metric_name.resize(len);
                result = rocprofvis_controller_get_string(
                    workload.handle, kRPVControllerWorkloadAvailableMetricNameIndexed, j,
                    const_cast<char*>(metric_name.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                len    = 0;
                result = rocprofvis_controller_get_string(
                    workload.handle,
                    kRPVControllerWorkloadAvailableMetricDescriptionIndexed, j, nullptr,
                    &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                std::string description;
                description.resize(len);
                result = rocprofvis_controller_get_string(
                    workload.handle,
                    kRPVControllerWorkloadAvailableMetricDescriptionIndexed, j,
                    const_cast<char*>(description.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                len    = 0;
                result = rocprofvis_controller_get_string(
                    workload.handle, kRPVControllerWorkloadAvailableMetricUnitIndexed, j,
                    nullptr, &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                std::string unit;
                unit.resize(len);
                result = rocprofvis_controller_get_string(
                    workload.handle, kRPVControllerWorkloadAvailableMetricUnitIndexed, j,
                    const_cast<char*>(unit.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                len    = 0;
                result = rocprofvis_controller_get_string(
                    workload.handle,
                    kRPVControllerWorkloadAvailableMetricCategoryNameIndexed, j, nullptr,
                    &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                std::string category_name;
                category_name.resize(len);
                result = rocprofvis_controller_get_string(
                    workload.handle,
                    kRPVControllerWorkloadAvailableMetricCategoryNameIndexed, j,
                    const_cast<char*>(category_name.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                len    = 0;
                result = rocprofvis_controller_get_string(
                    workload.handle,
                    kRPVControllerWorkloadAvailableMetricTableNameIndexed, j, nullptr,
                    &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                std::string table_name;
                table_name.resize(len);
                result = rocprofvis_controller_get_string(
                    workload.handle,
                    kRPVControllerWorkloadAvailableMetricTableNameIndexed, j,
                    const_cast<char*>(table_name.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                auto& category    = workload.available_metrics.tree[category_id];
                category.id       = category_id;
                auto& table       = category.tables[table_id];
                table.id          = table_id;
                uint64_t entry_id = static_cast<uint64_t>(table.entry_count++);

                workload.available_metrics.list.push_back(
                    AvailableMetrics::Entry{ category_id, table_id, entry_id });

                spdlog::info("  Metric {0}: cat={1}({2}) tbl={3}({4}) name={5}", entry_id,
                             category_id, category_name, table_id, table_name,
                             metric_name);
            }

            REQUIRE(!workload.available_metrics.list.empty());
        }
    }

    // Loads the distinct metric value names per (category, table) pair for each workload.
    // Populates the value_names vectors inside the available_metrics tree.
    // Fixture Reads:  m_workloads[].handle
    // Fixture Writes: m_workloads[].available_metrics.tree[].tables[].value_names
    SECTION("Controller Load Metric Value Names")
    {
        for(size_t workload_idx = 0; workload_idx < m_workloads.size(); workload_idx++)
        {
            auto& workload = m_workloads[workload_idx];
            spdlog::info("Loading metric value names for workload {0}/{1} (id={2})",
                         workload_idx + 1, m_workloads.size(), workload.id);
            uint64_t            num_value_names = 0;
            rocprofvis_result_t result          = rocprofvis_controller_get_uint64(
                workload.handle, kRPVControllerWorkloadNumMetricValueNames, 0,
                &num_value_names);
            spdlog::info("Workload id {0} metric value names: {1}", workload.id,
                         num_value_names);
            REQUIRE(result == kRocProfVisResultSuccess);

            for(uint64_t k = 0; k < num_value_names; k++)
            {
                spdlog::info("Processing metric value name {0}/{1}", k + 1,
                             num_value_names);
                uint64_t cat_id = 0;
                result          = rocprofvis_controller_get_uint64(
                    workload.handle,
                    kRPVControllerWorkloadMetricValueNameCategoryIdIndexed, k, &cat_id);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(cat_id > 0);

                uint64_t tbl_id = 0;
                result          = rocprofvis_controller_get_uint64(
                    workload.handle, kRPVControllerWorkloadMetricValueNameTableIdIndexed,
                    k, &tbl_id);
                REQUIRE(result == kRocProfVisResultSuccess);

                uint32_t len = 0;
                result       = rocprofvis_controller_get_string(
                    workload.handle, kRPVControllerWorkloadMetricValueNameStringIndexed,
                    k, nullptr, &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                std::string name;
                name.resize(len);
                result = rocprofvis_controller_get_string(
                    workload.handle, kRPVControllerWorkloadMetricValueNameStringIndexed,
                    k, const_cast<char*>(name.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(!name.empty());

                workload.available_metrics.tree[cat_id]
                    .tables[tbl_id]
                    .value_names.push_back(name);

                spdlog::info("  ValueName {0}: cat={1} tbl={2} name={3}", k, cat_id,
                             tbl_id, name);
            }
        }
    }

    // Loads kernel data for each workload: kernel ID, name, invocation count,
    // and duration statistics (total, min, max, mean, median).
    // Fixture Reads:  m_workloads[].handle
    // Fixture Writes: m_workloads[].kernels[].id
    SECTION("Controller Load Kernel Data")
    {
        for(size_t workload_idx = 0; workload_idx < m_workloads.size(); workload_idx++)
        {
            auto& workload = m_workloads[workload_idx];
            spdlog::info("Loading kernels for workload {0}/{1} (id={2})",
                         workload_idx + 1, m_workloads.size(), workload.id);
            uint64_t            num_kernels = 0;
            rocprofvis_result_t result      = rocprofvis_controller_get_uint64(
                workload.handle, kRPVControllerWorkloadNumKernels, 0, &num_kernels);
            spdlog::info("Workload id {0} num kernels: {1}", workload.id, num_kernels);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(num_kernels > 0);

            for(uint64_t j = 0; j < num_kernels; j++)
            {
                rocprofvis_handle_t* kernel_handle = nullptr;
                result                             = rocprofvis_controller_get_object(
                    workload.handle, kRPVControllerWorkloadKernelIndexed, j,
                    &kernel_handle);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(kernel_handle);

                uint64_t kernel_id = 0;
                result             = rocprofvis_controller_get_uint64(
                    kernel_handle, kRPVControllerKernelId, 0, &kernel_id);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(kernel_id > 0);
                spdlog::info("Processing kernel {0}/{1} (id={2})", j + 1, num_kernels,
                             kernel_id);

                uint32_t len = 0;
                result       = rocprofvis_controller_get_string(
                    kernel_handle, kRPVControllerKernelName, 0, nullptr, &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                std::string kernel_name;
                kernel_name.resize(len);
                result = rocprofvis_controller_get_string(
                    kernel_handle, kRPVControllerKernelName, 0,
                    const_cast<char*>(kernel_name.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(!kernel_name.empty());

                uint64_t invocation_count = 0;
                result                    = rocprofvis_controller_get_uint64(
                    kernel_handle, kRPVControllerKernelInvocationCount, 0,
                    &invocation_count);
                spdlog::info("  Kernel {0}: id={1} name={2} invocations={3}", j,
                             kernel_id, kernel_name, invocation_count);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(invocation_count > 0);

                uint64_t duration_total = 0;
                result                  = rocprofvis_controller_get_uint64(
                    kernel_handle, kRPVControllerKernelDurationTotal, 0, &duration_total);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(duration_total > 0);

                uint64_t duration_min = 0;
                result                = rocprofvis_controller_get_uint64(
                    kernel_handle, kRPVControllerKernelDurationMin, 0, &duration_min);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(duration_min > 0);

                uint64_t duration_max = 0;
                result                = rocprofvis_controller_get_uint64(
                    kernel_handle, kRPVControllerKernelDurationMax, 0, &duration_max);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(duration_max > 0);

                uint64_t duration_mean = 0;
                result                 = rocprofvis_controller_get_uint64(
                    kernel_handle, kRPVControllerKernelDurationMean, 0, &duration_mean);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(duration_mean > 0);

                uint64_t duration_median = 0;
                result                   = rocprofvis_controller_get_uint64(
                    kernel_handle, kRPVControllerKernelDurationMedian, 0,
                    &duration_median);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(duration_median > 0);

                workload.kernels.push_back(KernelInfo{ kernel_id });
            }

            REQUIRE(!workload.kernels.empty());
        }
    }

    // Loads roofline data for each workload: ridge points, compute ceilings,
    // bandwidth ceilings, and per-kernel intensity points.
    // Fixture Reads: m_workloads[].handle, m_workloads[].id
    SECTION("Controller Load Roofline Data")
    {
        for(size_t workload_idx = 0; workload_idx < m_workloads.size(); workload_idx++)
        {
            auto& workload = m_workloads[workload_idx];
            spdlog::info("Loading roofline for workload {0}/{1} (id={2})",
                         workload_idx + 1, m_workloads.size(), workload.id);
            rocprofvis_handle_t* roofline_handle = nullptr;
            rocprofvis_result_t  result          = rocprofvis_controller_get_object(
                workload.handle, kRPVControllerWorkloadRoofline, 0, &roofline_handle);
            spdlog::info("Workload id {0} roofline: {1}", workload.id,
                         (void*) roofline_handle);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(roofline_handle);

            uint64_t num_ridge = 0;
            result             = rocprofvis_controller_get_uint64(
                roofline_handle, kRPVControllerRooflineNumCeilingsRidge, 0, &num_ridge);
            spdlog::info("  Ridge points: {0}", num_ridge);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(num_ridge > 0);

            for(uint64_t j = 0; j < num_ridge; j++)
            {
                spdlog::info("Processing ridge point {0}/{1}", j + 1, num_ridge);
                uint64_t compute_type = 0;
                result                = rocprofvis_controller_get_uint64(
                    roofline_handle, kRPVControllerRooflineCeilingRidgeComputeTypeIndexed,
                    j, &compute_type);
                REQUIRE(result == kRocProfVisResultSuccess);

                uint64_t bandwidth_type = 0;
                result                  = rocprofvis_controller_get_uint64(
                    roofline_handle,
                    kRPVControllerRooflineCeilingRidgeBandwidthTypeIndexed, j,
                    &bandwidth_type);
                REQUIRE(result == kRocProfVisResultSuccess);

                double ridge_x = 0;
                result         = rocprofvis_controller_get_double(
                    roofline_handle, kRPVControllerRooflineCeilingRidgeXIndexed, j,
                    &ridge_x);
                REQUIRE(result == kRocProfVisResultSuccess);

                double ridge_y = 0;
                result         = rocprofvis_controller_get_double(
                    roofline_handle, kRPVControllerRooflineCeilingRidgeYIndexed, j,
                    &ridge_y);
                REQUIRE(result == kRocProfVisResultSuccess);

                spdlog::info("  Ridge {0}: compute={1} bw={2} x={3} y={4}", j,
                             compute_type, bandwidth_type, ridge_x, ridge_y);
            }

            uint64_t num_compute = 0;
            result               = rocprofvis_controller_get_uint64(
                roofline_handle, kRPVControllerRooflineNumCeilingsCompute, 0,
                &num_compute);
            spdlog::info("  Compute ceilings: {0}", num_compute);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(num_compute > 0);

            for(uint64_t j = 0; j < num_compute; j++)
            {
                spdlog::info("Processing compute ceiling {0}/{1}", j + 1, num_compute);
                uint64_t compute_type = 0;
                result                = rocprofvis_controller_get_uint64(
                    roofline_handle, kRPVControllerRooflineCeilingComputeTypeIndexed, j,
                    &compute_type);
                REQUIRE(result == kRocProfVisResultSuccess);

                double x = 0;
                result   = rocprofvis_controller_get_double(
                    roofline_handle, kRPVControllerRooflineCeilingComputeXIndexed, j, &x);
                REQUIRE(result == kRocProfVisResultSuccess);

                double y = 0;
                result   = rocprofvis_controller_get_double(
                    roofline_handle, kRPVControllerRooflineCeilingComputeYIndexed, j, &y);
                REQUIRE(result == kRocProfVisResultSuccess);

                double throughput = 0;
                result            = rocprofvis_controller_get_double(
                    roofline_handle,
                    kRPVControllerRooflineCeilingComputeThroughputIndexed, j,
                    &throughput);
                REQUIRE(result == kRocProfVisResultSuccess);

                spdlog::info("  ComputeCeiling {0}: type={1} x={2} y={3} tp={4}", j,
                             compute_type, x, y, throughput);
            }

            uint64_t num_bandwidth = 0;
            result                 = rocprofvis_controller_get_uint64(
                roofline_handle, kRPVControllerRooflineNumCeilingsBandwidth, 0,
                &num_bandwidth);
            spdlog::info("  Bandwidth ceilings: {0}", num_bandwidth);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(num_bandwidth > 0);

            for(uint64_t j = 0; j < num_bandwidth; j++)
            {
                spdlog::info("Processing bandwidth ceiling {0}/{1}", j + 1,
                             num_bandwidth);
                uint64_t bandwidth_type = 0;
                result                  = rocprofvis_controller_get_uint64(
                    roofline_handle, kRPVControllerRooflineCeilingBandwidthTypeIndexed, j,
                    &bandwidth_type);
                REQUIRE(result == kRocProfVisResultSuccess);

                double x = 0;
                result   = rocprofvis_controller_get_double(
                    roofline_handle, kRPVControllerRooflineCeilingBandwidthXIndexed, j,
                    &x);
                REQUIRE(result == kRocProfVisResultSuccess);

                double y = 0;
                result   = rocprofvis_controller_get_double(
                    roofline_handle, kRPVControllerRooflineCeilingBandwidthYIndexed, j,
                    &y);
                REQUIRE(result == kRocProfVisResultSuccess);

                double throughput = 0;
                result            = rocprofvis_controller_get_double(
                    roofline_handle,
                    kRPVControllerRooflineCeilingBandwidthThroughputIndexed, j,
                    &throughput);
                REQUIRE(result == kRocProfVisResultSuccess);

                spdlog::info("  BandwidthCeiling {0}: type={1} x={2} y={3} tp={4}", j,
                             bandwidth_type, x, y, throughput);
            }

            uint64_t num_roofline_kernels = 0;
            result = rocprofvis_controller_get_uint64(roofline_handle,
                                                      kRPVControllerRooflineNumKernels, 0,
                                                      &num_roofline_kernels);
            spdlog::info("  Roofline kernels: {0}", num_roofline_kernels);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(num_roofline_kernels > 0);

            for(uint64_t j = 0; j < num_roofline_kernels; j++)
            {
                uint64_t intensity_type = 0;
                result                  = rocprofvis_controller_get_uint64(
                    roofline_handle, kRPVControllerRooflineKernelIntensityTypeIndexed, j,
                    &intensity_type);
                REQUIRE(result == kRocProfVisResultSuccess);

                double intensity_x = 0;
                result             = rocprofvis_controller_get_double(
                    roofline_handle, kRPVControllerRooflineKernelIntensityXIndexed, j,
                    &intensity_x);
                REQUIRE(result == kRocProfVisResultSuccess);

                double intensity_y = 0;
                result             = rocprofvis_controller_get_double(
                    roofline_handle, kRPVControllerRooflineKernelIntensityYIndexed, j,
                    &intensity_y);
                REQUIRE(result == kRocProfVisResultSuccess);

                uint64_t roofline_kernel_id = 0;
                result                      = rocprofvis_controller_get_uint64(
                    roofline_handle, kRPVControllerRooflineKernelIdIndexed, j,
                    &roofline_kernel_id);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(roofline_kernel_id > 0);
                spdlog::info("Processing roofline kernel {0}/{1} (id={2})", j + 1,
                             num_roofline_kernels, roofline_kernel_id);

                spdlog::trace("  KernelIntensity {0}: type={1} x={2} y={3} kid={4}", j,
                              intensity_type, intensity_x, intensity_y,
                              roofline_kernel_id);
            }
        }
    }

    // Fetches per-kernel metric values asynchronously for all kernels and metrics in
    // each workload. Validates that each returned metric ID and value name match an
    // entry in the available_metrics catalog.
    // Fixture Reads: m_controller, m_workloads[].kernels[].id,
    //        m_workloads[].available_metrics.list, m_workloads[].available_metrics.tree
    SECTION("Controller Fetch Kernel Metrics")
    {
        for(size_t workload_idx = 0; workload_idx < m_workloads.size(); workload_idx++)
        {
            auto& workload = m_workloads[workload_idx];
            spdlog::info("Fetching metrics for workload {0}/{1} (id={2})",
                         workload_idx + 1, m_workloads.size(), workload.id);
            REQUIRE(!workload.kernels.empty());
            REQUIRE(!workload.available_metrics.list.empty());

            spdlog::info(
                "Fetching metrics for workload {0} with {1} kernels, {2} metrics",
                workload.id, workload.kernels.size(),
                workload.available_metrics.list.size());

            rocprofvis_controller_arguments_t* args =
                rocprofvis_controller_arguments_alloc();
            REQUIRE(args != nullptr);

            rocprofvis_result_t result = rocprofvis_controller_set_uint64(
                args, kRPVControllerMetricArgsNumKernels, 0, workload.kernels.size());
            REQUIRE(result == kRocProfVisResultSuccess);

            for(size_t i = 0; i < workload.kernels.size(); i++)
            {
                result = rocprofvis_controller_set_uint64(
                    args, kRPVControllerMetricArgsKernelIdIndexed, i,
                    workload.kernels[i].id);
                REQUIRE(result == kRocProfVisResultSuccess);
            }

            result = rocprofvis_controller_set_uint64(
                args, kRPVControllerMetricArgsNumMetrics, 0,
                workload.available_metrics.list.size());
            REQUIRE(result == kRocProfVisResultSuccess);

            for(size_t i = 0; i < workload.available_metrics.list.size(); i++)
            {
                const auto& entry = workload.available_metrics.list[i];
                result            = rocprofvis_controller_set_uint64(
                    args, kRPVControllerMetricArgsMetricCategoryIdIndexed, i,
                    entry.category_id);
                REQUIRE(result == kRocProfVisResultSuccess);

                result = rocprofvis_controller_set_uint64(
                    args, kRPVControllerMetricArgsMetricTableIdIndexed, i,
                    entry.table_id);
                REQUIRE(result == kRocProfVisResultSuccess);

                result = rocprofvis_controller_set_uint64(
                    args, kRPVControllerMetricArgsMetricEntryIdIndexed, i, entry.id);
                REQUIRE(result == kRocProfVisResultSuccess);
            }

            rocprofvis_controller_metrics_container_t* output =
                rocprofvis_controller_metrics_container_alloc();
            REQUIRE(output != nullptr);

            rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
            REQUIRE(future != nullptr);

            result = rocprofvis_controller_metric_fetch_async(m_controller, args, future,
                                                              output);
            REQUIRE(result == kRocProfVisResultSuccess);

            spdlog::info("Wait for metrics fetch");
            result = rocprofvis_controller_future_wait(future, FLT_MAX);
            REQUIRE(result == kRocProfVisResultSuccess);

            uint64_t future_result = 0;
            result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult,
                                                      0, &future_result);
            spdlog::info("Metrics fetch result: {0}", future_result);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(future_result == kRocProfVisResultSuccess);

            uint64_t num_metrics = 0;
            result               = rocprofvis_controller_get_uint64(
                output, kRPVControllerMetricsContainerNumMetrics, 0, &num_metrics);
            spdlog::info("Fetched {0} metric results", num_metrics);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(num_metrics > 0);

            for(uint64_t i = 0; i < num_metrics; i++)
            {
                uint64_t kernel_id = 0;
                result             = rocprofvis_controller_get_uint64(
                    output, kRPVControllerMetricsContainerKernelIdIndexed, i, &kernel_id);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(kernel_id > 0);
                spdlog::info("Validating fetched metric {0}/{1} (kernel_id={2})", i + 1,
                             num_metrics, kernel_id);

                uint32_t len = 0;
                result       = rocprofvis_controller_get_string(
                    output, kRPVControllerMetricsContainerMetricIdIndexed, i, nullptr,
                    &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                std::string metric_id;
                metric_id.resize(len);
                result = rocprofvis_controller_get_string(
                    output, kRPVControllerMetricsContainerMetricIdIndexed, i,
                    const_cast<char*>(metric_id.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(!metric_id.empty());

                auto     cat_end = metric_id.find('.');
                auto     tbl_end = metric_id.find('.', cat_end + 1);
                uint64_t parsed_cat_id =
                    static_cast<uint64_t>(std::stoll(metric_id.substr(0, cat_end)));
                uint64_t parsed_tbl_id = static_cast<uint64_t>(std::stoll(
                    metric_id.substr(cat_end + 1, tbl_end - cat_end - 1)));
                uint64_t parsed_entry_id =
                    static_cast<uint64_t>(std::stoll(metric_id.substr(tbl_end + 1)));

                auto cat_it = workload.available_metrics.tree.find(parsed_cat_id);
                REQUIRE(cat_it != workload.available_metrics.tree.end());

                auto tbl_it = cat_it->second.tables.find(parsed_tbl_id);
                REQUIRE(tbl_it != cat_it->second.tables.end());

                bool entry_found = false;
                for(size_t entry_idx = 0;
                    entry_idx < workload.available_metrics.list.size(); entry_idx++)
                {
                    const auto& e = workload.available_metrics.list[entry_idx];
                    spdlog::trace("Checking metric entry {0}/{1}", entry_idx + 1,
                                  workload.available_metrics.list.size());
                    if(e.category_id == parsed_cat_id && e.table_id == parsed_tbl_id &&
                       e.id == parsed_entry_id)
                    {
                        entry_found = true;
                        break;
                    }
                }
                REQUIRE(entry_found);

                len    = 0;
                result = rocprofvis_controller_get_string(
                    output, kRPVControllerMetricsContainerMetricValueNameIndexed, i,
                    nullptr, &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                std::string value_name;
                value_name.resize(len);
                result = rocprofvis_controller_get_string(
                    output, kRPVControllerMetricsContainerMetricValueNameIndexed, i,
                    const_cast<char*>(value_name.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(!value_name.empty());

                const auto& valid_names = tbl_it->second.value_names;
                REQUIRE(std::find(valid_names.begin(), valid_names.end(), value_name) !=
                        valid_names.end());

                double value = 0;
                result       = rocprofvis_controller_get_double(
                    output, kRPVControllerMetricsContainerMetricValueValueIndexed, i,
                    &value);
                REQUIRE(result == kRocProfVisResultSuccess);

                spdlog::info("  Metric {0}: kid={1} mid={2} vname={3} val={4}", i,
                             kernel_id, metric_id, value_name, value);
            }

            spdlog::info("Free metrics resources");
            rocprofvis_controller_metrics_container_free(output);
            rocprofvis_controller_arguments_free(args);
            rocprofvis_controller_future_free(future);
        }
    }

    // Fetches per-workload aggregated metric values asynchronously (no kernel IDs set).
    // Validates that each returned workload ID matches the queried workload and that
    // each metric ID and value name match entries in the available_metrics catalog.
    // Fixture Reads: m_controller, m_workloads[].id,
    //        m_workloads[].available_metrics.list, m_workloads[].available_metrics.tree
    SECTION("Controller Fetch Workload Metrics")
    {
        for(size_t workload_idx = 0; workload_idx < m_workloads.size(); workload_idx++)
        {
            auto& workload = m_workloads[workload_idx];
            spdlog::info("Fetching workload metrics for workload {0}/{1} (id={2})",
                         workload_idx + 1, m_workloads.size(), workload.id);
            REQUIRE(!workload.available_metrics.list.empty());

            rocprofvis_controller_arguments_t* args =
                rocprofvis_controller_arguments_alloc();
            REQUIRE(args != nullptr);

            rocprofvis_result_t result = rocprofvis_controller_set_uint64(
                args, kRPVControllerMetricArgsWorkloadId, 0, workload.id);
            REQUIRE(result == kRocProfVisResultSuccess);

            result = rocprofvis_controller_set_uint64(
                args, kRPVControllerMetricArgsNumMetrics, 0,
                workload.available_metrics.list.size());
            REQUIRE(result == kRocProfVisResultSuccess);

            for(size_t i = 0; i < workload.available_metrics.list.size(); i++)
            {
                const auto& entry = workload.available_metrics.list[i];
                result            = rocprofvis_controller_set_uint64(
                    args, kRPVControllerMetricArgsMetricCategoryIdIndexed, i,
                    entry.category_id);
                REQUIRE(result == kRocProfVisResultSuccess);

                result = rocprofvis_controller_set_uint64(
                    args, kRPVControllerMetricArgsMetricTableIdIndexed, i,
                    entry.table_id);
                REQUIRE(result == kRocProfVisResultSuccess);

                result = rocprofvis_controller_set_uint64(
                    args, kRPVControllerMetricArgsMetricEntryIdIndexed, i, entry.id);
                REQUIRE(result == kRocProfVisResultSuccess);
            }

            rocprofvis_controller_metrics_container_t* output =
                rocprofvis_controller_metrics_container_alloc();
            REQUIRE(output != nullptr);

            rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
            REQUIRE(future != nullptr);

            result = rocprofvis_controller_metric_fetch_async(m_controller, args, future,
                                                              output);
            REQUIRE(result == kRocProfVisResultSuccess);

            spdlog::info("Wait for workload metrics fetch");
            result = rocprofvis_controller_future_wait(future, FLT_MAX);
            REQUIRE(result == kRocProfVisResultSuccess);

            uint64_t future_result = 0;
            result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult,
                                                      0, &future_result);
            spdlog::info("Workload metrics fetch result: {0}", future_result);

            if(future_result == kRocProfVisResultSuccess)
            {
                uint64_t num_metrics = 0;
                result               = rocprofvis_controller_get_uint64(
                    output, kRPVControllerMetricsContainerNumMetrics, 0, &num_metrics);
                spdlog::info("Fetched {0} workload metric results", num_metrics);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(num_metrics > 0);

                for(uint64_t i = 0; i < num_metrics; i++)
                {
                    uint64_t workload_id_out = 0;
                    result                   = rocprofvis_controller_get_uint64(
                        output, kRPVControllerMetricsContainerWorkloadIdIndexed, i,
                        &workload_id_out);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    REQUIRE(workload_id_out == workload.id);
                    spdlog::info("Validating fetched workload metric {0}/{1} (wid={2})",
                                 i + 1, num_metrics, workload_id_out);

                    uint32_t len = 0;
                    result       = rocprofvis_controller_get_string(
                        output, kRPVControllerMetricsContainerMetricIdIndexed, i, nullptr,
                        &len);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    std::string metric_id;
                    metric_id.resize(len);
                    result = rocprofvis_controller_get_string(
                        output, kRPVControllerMetricsContainerMetricIdIndexed, i,
                        const_cast<char*>(metric_id.c_str()), &len);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    REQUIRE(!metric_id.empty());

                    auto     cat_end = metric_id.find('.');
                    auto     tbl_end = metric_id.find('.', cat_end + 1);
                    uint64_t parsed_cat_id =
                        static_cast<uint64_t>(std::stoll(metric_id.substr(0, cat_end)));
                    uint64_t parsed_tbl_id = static_cast<uint64_t>(std::stoll(
                        metric_id.substr(cat_end + 1, tbl_end - cat_end - 1)));
                    uint64_t parsed_entry_id =
                        static_cast<uint64_t>(std::stoll(metric_id.substr(tbl_end + 1)));

                    auto cat_it = workload.available_metrics.tree.find(parsed_cat_id);
                    REQUIRE(cat_it != workload.available_metrics.tree.end());

                    auto tbl_it = cat_it->second.tables.find(parsed_tbl_id);
                    REQUIRE(tbl_it != cat_it->second.tables.end());

                    bool entry_found = false;
                    for(size_t entry_idx = 0;
                        entry_idx < workload.available_metrics.list.size(); entry_idx++)
                    {
                        const auto& e = workload.available_metrics.list[entry_idx];
                        spdlog::trace("Checking metric entry {0}/{1}", entry_idx + 1,
                                      workload.available_metrics.list.size());
                        if(e.category_id == parsed_cat_id &&
                           e.table_id == parsed_tbl_id && e.id == parsed_entry_id)
                        {
                            entry_found = true;
                            break;
                        }
                    }
                    REQUIRE(entry_found);

                    len    = 0;
                    result = rocprofvis_controller_get_string(
                        output, kRPVControllerMetricsContainerMetricValueNameIndexed, i,
                        nullptr, &len);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    std::string value_name;
                    value_name.resize(len);
                    result = rocprofvis_controller_get_string(
                        output, kRPVControllerMetricsContainerMetricValueNameIndexed, i,
                        const_cast<char*>(value_name.c_str()), &len);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    REQUIRE(!value_name.empty());

                    const auto& valid_names = tbl_it->second.value_names;
                    REQUIRE(std::find(valid_names.begin(), valid_names.end(),
                                      value_name) != valid_names.end());

                    double value = 0;
                    result       = rocprofvis_controller_get_double(
                        output, kRPVControllerMetricsContainerMetricValueValueIndexed, i,
                        &value);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    spdlog::info("  Metric {0}: wid={1} mid={2} vname={3} val={4}", i,
                                 workload_id_out, metric_id, value_name, value);
                }
            }
            else
            {
                spdlog::info("Fetching workload metrics unsupported");
            }

            spdlog::info("Free workload metrics resources");
            rocprofvis_controller_metrics_container_free(output);
            rocprofvis_controller_arguments_free(args);
            rocprofvis_controller_future_free(future);
        }
    }

    // Builds metric selectors (metric_id:value_name) from the available_metrics catalog,
    // fetches a kernel metrics matrix (pivot table) via the table fetch API, and
    // validates column headers, column types, and cell values for every row.
    // Fixture Reads: m_controller, m_workloads[].id,
    //        m_workloads[].available_metrics.list, m_workloads[].available_metrics.tree
    SECTION("Controller Fetch Metric Pivot Table")
    {
        for(size_t workload_idx = 0; workload_idx < m_workloads.size(); workload_idx++)
        {
            auto& workload = m_workloads[workload_idx];
            spdlog::info("Fetching pivot table for workload {0}/{1} (id={2})",
                         workload_idx + 1, m_workloads.size(), workload.id);

            rocprofvis_handle_t* table_handle = nullptr;
            rocprofvis_result_t  result       = rocprofvis_controller_get_object(
                m_controller, kRPVControllerKernelMetricTable, 0, &table_handle);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(table_handle);

            rocprofvis_controller_arguments_t* args =
                rocprofvis_controller_arguments_alloc();
            REQUIRE(args != nullptr);

            result = rocprofvis_controller_set_uint64((rocprofvis_handle_t*) args,
                                                      kRPVControllerCPTArgsWorkloadId, 0,
                                                      workload.id);
            REQUIRE(result == kRocProfVisResultSuccess);

            std::vector<std::string> selectors;
            for(size_t metric_idx = 0;
                metric_idx < workload.available_metrics.list.size(); metric_idx++)
            {
                const auto& entry = workload.available_metrics.list[metric_idx];
                auto cat_it = workload.available_metrics.tree.find(entry.category_id);
                if(cat_it != workload.available_metrics.tree.end())
                {
                    auto tbl_it = cat_it->second.tables.find(entry.table_id);
                    if(tbl_it != cat_it->second.tables.end())
                    {
                        for(size_t vname_idx = 0;
                            vname_idx < tbl_it->second.value_names.size(); vname_idx++)
                        {
                            const auto& vname = tbl_it->second.value_names[vname_idx];
                            selectors.push_back(std::to_string(entry.category_id) + "." +
                                                std::to_string(entry.table_id) + "." +
                                                std::to_string(entry.id) + ":" + vname);
                        }
                    }
                }
            }

            spdlog::info("Built {0} metric selectors for pivot table", selectors.size());

            result = rocprofvis_controller_set_uint64(
                (rocprofvis_handle_t*) args, kRPVControllerCPTArgsNumMetricSelectors, 0,
                selectors.size());
            REQUIRE(result == kRocProfVisResultSuccess);

            for(size_t i = 0; i < selectors.size(); i++)
            {
                result = rocprofvis_controller_set_string(
                    (rocprofvis_handle_t*) args,
                    kRPVControllerCPTArgsMetricSelectorIndexed, i, selectors[i].c_str());
                REQUIRE(result == kRocProfVisResultSuccess);
            }

            result = rocprofvis_controller_set_uint64(
                (rocprofvis_handle_t*) args, kRPVControllerCPTArgsSortColumnIndex, 0, 0);
            REQUIRE(result == kRocProfVisResultSuccess);

            result = rocprofvis_controller_set_uint64((rocprofvis_handle_t*) args,
                                                      kRPVControllerCPTArgsSortOrder, 0,
                                                      kRPVControllerSortOrderAscending);
            REQUIRE(result == kRocProfVisResultSuccess);

            result = rocprofvis_controller_set_uint64(
                (rocprofvis_handle_t*) args, kRPVControllerCPTArgsNumColumnFilters, 0, 0);
            REQUIRE(result == kRocProfVisResultSuccess);

            rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
            REQUIRE(future != nullptr);

            rocprofvis_controller_array_t* array = rocprofvis_controller_array_alloc(0);
            REQUIRE(array != nullptr);

            result = rocprofvis_controller_table_fetch_async(m_controller, table_handle,
                                                             args, future, array);
            REQUIRE(result == kRocProfVisResultSuccess);

            spdlog::info("Wait for pivot table fetch");
            result = rocprofvis_controller_future_wait(future, FLT_MAX);
            REQUIRE(result == kRocProfVisResultSuccess);

            uint64_t future_result = 0;
            result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult,
                                                      0, &future_result);
            spdlog::info("Pivot table fetch result: {0}", future_result);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(future_result == kRocProfVisResultSuccess);

            uint64_t num_rows = 0;
            result = rocprofvis_controller_get_uint64((rocprofvis_handle_t*) array,
                                                      kRPVControllerArrayNumEntries, 0,
                                                      &num_rows);
            spdlog::info("Pivot table rows: {0}", num_rows);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(num_rows > 0);

            uint64_t num_columns = 0;
            result = rocprofvis_controller_get_uint64((rocprofvis_handle_t*) table_handle,
                                                      kRPVControllerTableNumColumns, 0,
                                                      &num_columns);
            spdlog::info("Pivot table columns: {0}", num_columns);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(num_columns > 0);

            std::vector<uint64_t> column_types;
            for(uint64_t col = 0; col < num_columns; col++)
            {
                spdlog::info("Reading pivot column {0}/{1}", col + 1, num_columns);
                uint32_t len = 0;
                result       = rocprofvis_controller_get_string(
                    (rocprofvis_handle_t*) table_handle,
                    kRPVControllerTableColumnHeaderIndexed, col, nullptr, &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                std::string col_name;
                col_name.resize(len);
                result = rocprofvis_controller_get_string(
                    (rocprofvis_handle_t*) table_handle,
                    kRPVControllerTableColumnHeaderIndexed, col,
                    const_cast<char*>(col_name.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                uint64_t col_type = 0;
                result            = rocprofvis_controller_get_uint64(
                    (rocprofvis_handle_t*) table_handle,
                    kRPVControllerTableColumnTypeIndexed, col, &col_type);
                REQUIRE(result == kRocProfVisResultSuccess);
                column_types.push_back(col_type);

                spdlog::trace("  Column {0}: name={1} type={2}", col, col_name, col_type);
            }

            for(uint64_t row_idx = 0; row_idx < num_rows; row_idx++)
            {
                spdlog::info("Validating pivot row {0}/{1}", row_idx + 1, num_rows);
                rocprofvis_handle_t* row_handle = nullptr;
                result = rocprofvis_controller_get_object((rocprofvis_handle_t*) array,
                                                          kRPVControllerArrayEntryIndexed,
                                                          row_idx, &row_handle);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(row_handle);

                for(uint64_t col = 0; col < num_columns; col++)
                {
                    spdlog::trace("Validating pivot cell column {0}/{1}", col + 1,
                                  num_columns);
                    switch(column_types[col])
                    {
                        case kRPVControllerPrimitiveTypeUInt64:
                        {
                            uint64_t value = 0;
                            result         = rocprofvis_controller_get_uint64(
                                row_handle, kRPVControllerArrayEntryIndexed, col, &value);
                            REQUIRE(result == kRocProfVisResultSuccess);
                            break;
                        }
                        case kRPVControllerPrimitiveTypeDouble:
                        {
                            double value = 0;
                            result       = rocprofvis_controller_get_double(
                                row_handle, kRPVControllerArrayEntryIndexed, col, &value);
                            REQUIRE(result == kRocProfVisResultSuccess);
                            break;
                        }
                        case kRPVControllerPrimitiveTypeString:
                        {
                            uint32_t len = 0;
                            result       = rocprofvis_controller_get_string(
                                row_handle, kRPVControllerArrayEntryIndexed, col, nullptr,
                                &len);
                            REQUIRE(result == kRocProfVisResultSuccess);

                            std::string value;
                            value.resize(len);
                            result = rocprofvis_controller_get_string(
                                row_handle, kRPVControllerArrayEntryIndexed, col,
                                const_cast<char*>(value.c_str()), &len);
                            REQUIRE(result == kRocProfVisResultSuccess);
                            break;
                        }
                        case kRPVControllerPrimitiveTypeObject:
                        default:
                        {
                            break;
                        }
                    }
                }
            }

            spdlog::info("Free pivot table resources");
            rocprofvis_controller_array_free(array);
            rocprofvis_controller_arguments_free(args);
            rocprofvis_controller_future_free(future);
        }
    }

    // Frees the controller handle and all associated resources.
    // Fixture Reads: m_controller
    SECTION("Delete controller")
    {
        spdlog::info("Free Controller");
        rocprofvis_controller_free(m_controller);
    }
}
