// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <sqlite3.h>

// Add schema-2.2 sampling tables to a private copy of the compute fixture.
// Renaming one table injects a real query failure without mocking the controller.
struct PcSamplingFailureFixture
{
    std::filesystem::path path;
    std::unique_ptr<rocprofvis_controller_t, decltype(&rocprofvis_controller_free)>
        controller{ nullptr, &rocprofvis_controller_free };
    std::unique_ptr<rocprofvis_controller_arguments_t,
                    decltype(&rocprofvis_controller_arguments_free)>
                         args{ nullptr, &rocprofvis_controller_arguments_free };
    rocprofvis_handle_t* sampling = nullptr;

    ~PcSamplingFailureFixture()
    {
        controller.reset();
        std::error_code error;
        std::filesystem::remove(path, error);
    }

    void Execute(const std::string& sql)
    {
        sqlite3*  raw_db = nullptr;
        const int opened = sqlite3_open(path.string().c_str(), &raw_db);
        std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw_db, &sqlite3_close);
        REQUIRE(opened == SQLITE_OK);
        char*     error  = nullptr;
        const int result = sqlite3_exec(db.get(), sql.c_str(), nullptr, nullptr, &error);
        const std::string message = error ? error : "";
        sqlite3_free(error);
        INFO(message);
        REQUIRE(result == SQLITE_OK);
    }

    uint64_t Get(rocprofvis_property_t property)
    {
        uint64_t value = 0;
        REQUIRE(rocprofvis_controller_get_uint64(sampling, property, 0, &value) ==
                kRocProfVisResultSuccess);
        return value;
    }

    void Initialize(const std::string& broken_table = "")
    {
        const std::string suffix =
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        path = std::filesystem::temp_directory_path() /
               ("roc-optiq-pcsampling-" + suffix + ".db");
        std::error_code error;
        std::filesystem::copy_file(std::filesystem::path(ROCPROFVIS_TEST_SOURCE_DIR) /
                                       "sample" / "rocprof_compute_23ed6f36.db",
                                   path, error);
        REQUIRE_FALSE(error);
        Execute(R"sql(
            UPDATE compute_metadata SET schema_version = '2.2.0';
            ALTER TABLE compute_kernel_roofline_data ADD COLUMN lds_cache_data REAL;
            CREATE TABLE compute_kernel_symbol(kernel_symbol_uuid INTEGER,
                                                kernel_uuid INTEGER, code_object_uuid INTEGER);
            CREATE TABLE compute_instruction_line(instruction_uuid INTEGER,
                                                   kernel_symbol_uuid INTEGER);
            CREATE TABLE compute_pc_sample_state(pc_sample_state_uuid INTEGER,
                instruction_uuid INTEGER, total_count INTEGER, issue_count INTEGER,
                stall_count INTEGER, active_thread_percent REAL,
                wave_occupancy_percent REAL, dispatch_uuid INTEGER);
            CREATE TABLE compute_pc_sample_stall_reason(pc_sample_stall_reason_uuid INTEGER,
                pc_sample_state_uuid INTEGER, pc_sample_stall_reason_lookup_uuid INTEGER,
                count INTEGER);
            CREATE TABLE compute_pc_sample_stall_reason_lookup(
                pc_sample_stall_reason_lookup_uuid INTEGER, text TEXT);
            INSERT INTO compute_instruction_line VALUES(10, 1);
            INSERT INTO compute_pc_sample_state VALUES(1, 10, 100, 30, 70, 0, 0, 0);
            INSERT INTO compute_pc_sample_stall_reason VALUES(1, 1, 1, 70);
            INSERT INTO compute_pc_sample_stall_reason_lookup VALUES(1, 'WAITCNT');
        )sql");

        if(!broken_table.empty())
        {
            Execute("ALTER TABLE " + broken_table + " RENAME TO failed_query;");
        }

        controller.reset(rocprofvis_controller_alloc(path.string().c_str(), nullptr));
        REQUIRE(controller);
        std::unique_ptr<rocprofvis_controller_future_t,
                        decltype(&rocprofvis_controller_future_free)>
            future(rocprofvis_controller_future_alloc(),
                   &rocprofvis_controller_future_free);
        REQUIRE(future);
        REQUIRE(rocprofvis_controller_load_async(controller.get(), future.get()) ==
                kRocProfVisResultSuccess);
        REQUIRE(rocprofvis_controller_future_wait(future.get(), 30.0f) ==
                kRocProfVisResultSuccess);
        uint64_t result = 0;
        REQUIRE(rocprofvis_controller_get_uint64(future.get(), kRPVControllerFutureResult,
                                                 0, &result) == kRocProfVisResultSuccess);
        REQUIRE(result == kRocProfVisResultSuccess);

        rocprofvis_handle_t* workload = nullptr;
        rocprofvis_handle_t* kernel   = nullptr;
        REQUIRE(rocprofvis_controller_get_object(controller.get(),
                                                 kRPVControllerWorkloadIndexed, 0,
                                                 &workload) == kRocProfVisResultSuccess);
        REQUIRE(rocprofvis_controller_get_object(workload,
                                                 kRPVControllerWorkloadKernelIndexed, 0,
                                                 &kernel) == kRocProfVisResultSuccess);
        uint64_t kernel_id = 0;
        REQUIRE(rocprofvis_controller_get_uint64(kernel, kRPVControllerKernelId, 0,
                                                 &kernel_id) == kRocProfVisResultSuccess);
        Execute("INSERT INTO compute_kernel_symbol(kernel_symbol_uuid, kernel_uuid) "
                "VALUES(1, " +
                std::to_string(kernel_id) + ");");
        REQUIRE(rocprofvis_controller_get_object(kernel, kRPVControllerKernelPcSampling,
                                                 0,
                                                 &sampling) == kRocProfVisResultSuccess);

        args.reset(rocprofvis_controller_arguments_alloc());
        REQUIRE(args);
        REQUIRE(rocprofvis_controller_set_uint64(args.get(),
                                                 kRPVControllerPcSamplingArgsKernelId, 0,
                                                 kernel_id) == kRocProfVisResultSuccess);
        REQUIRE(rocprofvis_controller_set_uint64(
                    args.get(), kRPVControllerPcSamplingArgsIncludeInstructionSamples, 0,
                    0) == kRocProfVisResultSuccess);
    }

    rocprofvis_result_t Fetch()
    {
        std::unique_ptr<rocprofvis_controller_future_t,
                        decltype(&rocprofvis_controller_future_free)>
            future(rocprofvis_controller_future_alloc(),
                   &rocprofvis_controller_future_free);
        REQUIRE(future);
        REQUIRE(rocprofvis_controller_pc_sampling_fetch_stalls_async(
                    controller.get(), args.get(), future.get(), sampling) ==
                kRocProfVisResultSuccess);
        REQUIRE(rocprofvis_controller_future_wait(future.get(), 30.0f) ==
                kRocProfVisResultSuccess);
        uint64_t result = 0;
        REQUIRE(rocprofvis_controller_get_uint64(future.get(), kRPVControllerFutureResult,
                                                 0, &result) == kRocProfVisResultSuccess);
        return static_cast<rocprofvis_result_t>(result);
    }
};

TEST_CASE_METHOD(PcSamplingFailureFixture,
                 "PC sampling preserves completed query groups across failure and retry",
                 "[pc-sampling]")
{
    std::string broken_table;
    bool        counts_loaded = true;
    SECTION("Sample states fail")
    {
        broken_table  = "compute_pc_sample_state";
        counts_loaded = false;
    }
    SECTION("Stall reasons fail") { broken_table = "compute_pc_sample_stall_reason"; }
    SECTION("Stall lookups fail")
    {
        broken_table = "compute_pc_sample_stall_reason_lookup";
    }
    Initialize(broken_table);
    CHECK(Get(kRPVControllerPCSamplingSampleStatesLoaded) == 0);
    CHECK(Get(kRPVControllerPCSamplingStallDetailsLoaded) == 0);
    CHECK(Fetch() != kRocProfVisResultSuccess);
    CHECK(Get(kRPVControllerPCSamplingSampleStatesLoaded) == (counts_loaded ? 1 : 0));
    CHECK(Get(kRPVControllerPCSamplingStallDetailsLoaded) == 0);
    if(counts_loaded)
    {
        CHECK(Get(kRPVControllerPCSamplingPcSampleStateTotalCount) == 100);
        // Retrying a later group must reuse the completed sample-state query.
        Execute("UPDATE compute_pc_sample_state SET total_count = 999;");
    }
    Execute("ALTER TABLE failed_query RENAME TO " + broken_table + ";");
    CHECK(Fetch() == kRocProfVisResultSuccess);
    CHECK(Get(kRPVControllerPCSamplingSampleStatesLoaded) == 1);
    CHECK(Get(kRPVControllerPCSamplingStallDetailsLoaded) == 1);
    CHECK(Get(kRPVControllerPCSamplingPcSampleStateTotalCount) == 100);
}

TEST_CASE_METHOD(PcSamplingFailureFixture,
                 "PC sampling distinguishes an empty successful query from failure",
                 "[pc-sampling]")
{
    Initialize();
    Execute("DELETE FROM compute_pc_sample_state;");
    CHECK(Fetch() == kRocProfVisResultSuccess);
    CHECK(Get(kRPVControllerPCSamplingSampleStatesLoaded) == 1);
    CHECK(Get(kRPVControllerPCSamplingStallDetailsLoaded) == 1);
    CHECK(Get(kRPVControllerPCSamplingNumPcSampleStates) == 0);
}
