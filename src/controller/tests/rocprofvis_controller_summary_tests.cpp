// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_analysis.h"
#include "system/rocprofvis_controller_track.h"

#include <cfloat>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <sqlite3.h>

struct SummaryFailureFixture
{
    std::filesystem::path path;
    std::string           broken_table;
    std::unique_ptr<rocprofvis_controller_t, decltype(&rocprofvis_controller_free)>
        controller{ nullptr, &rocprofvis_controller_free };
    std::unique_ptr<rocprofvis_controller_arguments_t,
                    decltype(&rocprofvis_controller_arguments_free)>
        args{ nullptr, &rocprofvis_controller_arguments_free };
    std::unique_ptr<rocprofvis_controller_summary_metrics_t,
                    decltype(&rocprofvis_controller_summary_metric_free)>
           metrics{ nullptr, &rocprofvis_controller_summary_metric_free };
    double start = 0;
    double end   = 0;

    ~SummaryFailureFixture()
    {
        metrics.reset();
        args.reset();
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

    void DropTable(const std::string& prefix)
    {
        sqlite3*  raw_db = nullptr;
        const int opened = sqlite3_open(path.string().c_str(), &raw_db);
        std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw_db, &sqlite3_close);
        REQUIRE(opened == SQLITE_OK);
        std::string       table;
        const std::string query = "SELECT name FROM sqlite_master WHERE type = 'table' "
                                  "AND name LIKE '" +
                                  prefix + "_%';";
        const int result = sqlite3_exec(
            db.get(), query.c_str(),
            [](void* output, int, char** values, char**) -> int {
                *static_cast<std::string*>(output) = values[0];
                return 0;
            },
            &table, nullptr);
        REQUIRE(result == SQLITE_OK);
        REQUIRE_FALSE(table.empty());
        db.reset();
        Execute("DROP TABLE \"" + table + "\";");
        broken_table = table;
    }

    uint64_t Get(rocprofvis_handle_t* handle, rocprofvis_property_t property)
    {
        uint64_t value = 0;
        REQUIRE(rocprofvis_controller_get_uint64(handle, property, 0, &value) ==
                kRocProfVisResultSuccess);
        return value;
    }

    void Initialize()
    {
        const std::string suffix =
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        path = std::filesystem::temp_directory_path() /
               ("roc-optiq-summary-" + suffix + ".db");
        std::error_code error;
        std::filesystem::copy_file(std::filesystem::path(ROCPROFVIS_TEST_SOURCE_DIR) /
                                       "sample" / "rocpd-transpose.db",
                                   path, error);
        REQUIRE_FALSE(error);
        controller.reset(rocprofvis_controller_alloc(path.string().c_str(), nullptr));
        REQUIRE(controller);
        std::unique_ptr<rocprofvis_controller_future_t,
                        decltype(&rocprofvis_controller_future_free)>
            future(rocprofvis_controller_future_alloc(),
                   &rocprofvis_controller_future_free);
        REQUIRE(future);
        REQUIRE(rocprofvis_controller_load_async(controller.get(), future.get()) ==
                kRocProfVisResultSuccess);
        REQUIRE(rocprofvis_controller_future_wait(future.get(), FLT_MAX) ==
                kRocProfVisResultSuccess);
        REQUIRE(Get(future.get(), kRPVControllerFutureResult) ==
                kRocProfVisResultSuccess);

        rocprofvis_handle_t* timeline = nullptr;
        REQUIRE(rocprofvis_controller_get_object(controller.get(),
                                                 kRPVControllerSystemTimeline, 0,
                                                 &timeline) == kRocProfVisResultSuccess);
        REQUIRE(rocprofvis_controller_get_double(timeline,
                                                 kRPVControllerTimelineMinTimestamp, 0,
                                                 &start) == kRocProfVisResultSuccess);
        REQUIRE(rocprofvis_controller_get_double(timeline,
                                                 kRPVControllerTimelineMaxTimestamp, 0,
                                                 &end) == kRocProfVisResultSuccess);
        args.reset(rocprofvis_controller_arguments_alloc());
        metrics.reset(rocprofvis_controller_summary_metrics_alloc());
        REQUIRE(args);
        REQUIRE(metrics);
        // A failed async fetch must leave this caller-owned output unchanged.
        REQUIRE(rocprofvis_controller_set_uint64(
                    metrics.get(), kRPVAnalysisSummaryMetricPropertyAggregationLevel, 0,
                    kRPVAnalysisSummaryAggregationLevelNode) == kRocProfVisResultSuccess);
    }

    rocprofvis_result_t Fetch()
    {
        REQUIRE(rocprofvis_controller_set_double(args.get(),
                                                 kRPVAnalysisSummaryArgsStartTimestamp, 0,
                                                 start) == kRocProfVisResultSuccess);
        REQUIRE(rocprofvis_controller_set_double(args.get(),
                                                 kRPVAnalysisSummaryArgsEndTimestamp, 0,
                                                 end) == kRocProfVisResultSuccess);
        std::unique_ptr<rocprofvis_controller_future_t,
                        decltype(&rocprofvis_controller_future_free)>
            future(rocprofvis_controller_future_alloc(),
                   &rocprofvis_controller_future_free);
        REQUIRE(future);
        REQUIRE(rocprofvis_analysis_fetch_summary(controller.get(), args.get(),
                                                  future.get(), metrics.get()) ==
                kRocProfVisResultSuccess);
        REQUIRE(rocprofvis_controller_future_wait(future.get(), FLT_MAX) ==
                kRocProfVisResultSuccess);
        return static_cast<rocprofvis_result_t>(
            Get(future.get(), kRPVControllerFutureResult));
    }

    void CheckOutputUnchanged()
    {
        CHECK(Get(metrics.get(), kRPVAnalysisSummaryMetricPropertyAggregationLevel) ==
              kRPVAnalysisSummaryAggregationLevelNode);
        CHECK(Get(metrics.get(), kRPVAnalysisSummaryMetricPropertyNumSubMetrics) == 0);
    }
};

TEST_CASE_METHOD(SummaryFailureFixture, "Summary reports top-event query failures",
                 "[summary][failure]")
{
    Initialize();
    // Metadata has already loaded. Removing a backing table now fails the actual SQL
    // query without preventing trace initialization or changing its topology.
    SECTION("Memory query fails") { DropTable("rocpd_memory_copy"); }
    SECTION("Thread launch query fails") { DropTable("rocpd_region"); }
    INFO("Dropped table: " << broken_table);
    CHECK(Fetch() == kRocProfVisResultUnknownError);
    CheckOutputUnchanged();
}

TEST_CASE_METHOD(SummaryFailureFixture, "Summary reports counter-statistics failures",
                 "[summary][failure]")
{
    Initialize();
    const uint64_t num_tracks = Get(controller.get(), kRPVControllerSystemNumTracks);
    bool           injected   = false;
    for(uint64_t i = 0; i < num_tracks; i++)
    {
        rocprofvis_handle_t* handle = nullptr;
        REQUIRE(rocprofvis_controller_get_object(controller.get(),
                                                 kRPVControllerSystemTrackIndexed, i,
                                                 &handle) == kRocProfVisResultSuccess);
        RocProfVis::Controller::Track* track =
            reinterpret_cast<RocProfVis::Controller::Track*>(handle);
        if(track->GetCounter() && track->GetNumberOfEntries() > 0)
        {
            // Break the track's counter association so statistics fetching
            // returns InvalidArgument, before it starts a database read.
            track->SetCounter(nullptr);
            injected = true;
            break;
        }
    }
    REQUIRE(injected);
    CHECK(Fetch() == kRocProfVisResultInvalidArgument);
    CheckOutputUnchanged();
}

TEST_CASE_METHOD(SummaryFailureFixture, "Summary accepts populated and empty time ranges",
                 "[summary]")
{
    Initialize();
    bool empty = false;
    SECTION("Populated range") {}
    SECTION("Empty range")
    {
        start = end + 1;
        end   = start + 1;
        empty = true;
    }
    CHECK(Fetch() == kRocProfVisResultSuccess);
    CHECK(Get(metrics.get(), kRPVAnalysisSummaryMetricPropertyAggregationLevel) ==
          kRPVAnalysisSummaryAggregationLevelTrace);
    const uint64_t memory_events =
        Get(metrics.get(), kRPVAnalysisSummaryTraceNodePropertyNumTopMemoryEvents);
    if(empty)
    {
        CHECK(memory_events == 0);
    }
    else
    {
        CHECK(memory_events > 0);
    }
}
