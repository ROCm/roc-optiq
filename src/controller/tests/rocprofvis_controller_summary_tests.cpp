// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_analysis.h"
#include "rocprofvis_controller_test_database.h"
#include "system/rocprofvis_controller_track.h"

#include <cfloat>
#include <cstring>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <sqlite3.h>

struct SummaryFailureFixture
{
    // Persistent fixtures run a const test body. These stay mutable so one
    // Initialize() can be reused by every section of a test case.
    mutable RocProfVis::Controller::Test::TemporaryDatabase database;
    mutable std::string                                     broken_table;
    mutable std::unique_ptr<rocprofvis_controller_t,
                            decltype(&rocprofvis_controller_free)>
        controller{ nullptr, &rocprofvis_controller_free };
    mutable std::unique_ptr<rocprofvis_controller_arguments_t,
                            decltype(&rocprofvis_controller_arguments_free)>
        args{ nullptr, &rocprofvis_controller_arguments_free };
    mutable std::unique_ptr<rocprofvis_controller_summary_metrics_t,
                            decltype(&rocprofvis_controller_summary_metric_free)>
                   metrics{ nullptr, &rocprofvis_controller_summary_metric_free };
    mutable double start          = 0;
    mutable double end            = 0;
    mutable double timeline_start = 0;
    mutable double timeline_end   = 0;

    ~SummaryFailureFixture()
    {
        metrics.reset();
        args.reset();
        controller.reset();
    }

    void Execute(const std::string& sql) const { database.Execute(sql); }

    void DropTable(const std::string& prefix) const
    {
        sqlite3*  raw_db = nullptr;
        const int opened = sqlite3_open(database.Path().string().c_str(), &raw_db);
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

    uint64_t Get(rocprofvis_handle_t* handle, rocprofvis_property_t property) const
    {
        uint64_t value = 0;
        REQUIRE(rocprofvis_controller_get_uint64(handle, property, 0, &value) ==
                kRocProfVisResultSuccess);
        return value;
    }

    std::string GetString(rocprofvis_handle_t* handle, rocprofvis_property_t property,
                          uint64_t index) const
    {
        uint32_t length = 0;
        if(rocprofvis_controller_get_string(handle, property, index, nullptr, &length) !=
           kRocProfVisResultSuccess)
        {
            return std::string();
        }
        std::string value(length + 1, '\0');
        length += 1;
        if(rocprofvis_controller_get_string(handle, property, index, value.data(),
                                            &length) != kRocProfVisResultSuccess)
        {
            return std::string();
        }
        value.resize(std::strlen(value.c_str()));
        return value;
    }

    std::string DescribeMemoryEvents(uint64_t count) const
    {
        std::ostringstream text;
        text.precision(17);
        text << "window=[" << start << ", " << end << "]";
        for(uint64_t i = 0; i < count; i++)
        {
            uint64_t invocations = 0;
            uint64_t duration    = 0;
            rocprofvis_controller_get_uint64(
                metrics.get(),
                kRPVAnalysisSummaryTraceNodePropertyTopMemoryEventInvocationsIndexed, i,
                &invocations);
            rocprofvis_controller_get_uint64(
                metrics.get(),
                kRPVAnalysisSummaryTraceNodePropertyTopMemoryEventDurationSumIndexed, i,
                &duration);
            text << " | "
                 << GetString(
                        metrics.get(),
                        kRPVAnalysisSummaryTraceNodePropertyTopMemoryEventNameIndexed, i)
                 << " invocations=" << invocations << " duration_sum=" << duration;
        }
        return text.str();
    }

    void Initialize() const
    {
        database.Copy(std::filesystem::path(ROCPROFVIS_TEST_SOURCE_DIR) / "sample" /
                          "rocpd-transpose.db",
                      "roc-optiq-summary-");
        controller.reset(
            rocprofvis_controller_alloc(database.Path().string().c_str(), nullptr));
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
        timeline_start = start;
        timeline_end   = end;
        args.reset(rocprofvis_controller_arguments_alloc());
        metrics.reset(rocprofvis_controller_summary_metrics_alloc());
        REQUIRE(args);
        REQUIRE(metrics);
        ArmUnchangedOutput();
    }

    // A failed fetch must leave this caller-owned output unchanged. Earlier
    // sections share this object and a successful fetch replaces it, so each
    // failure section starts from the same node sentinel Initialize() used.
    void ArmUnchangedOutput() const
    {
        metrics.reset(rocprofvis_controller_summary_metrics_alloc());
        REQUIRE(metrics);
        REQUIRE(rocprofvis_controller_set_uint64(
                    metrics.get(), kRPVAnalysisSummaryMetricPropertyAggregationLevel, 0,
                    kRPVAnalysisSummaryAggregationLevelNode) == kRocProfVisResultSuccess);
    }

    rocprofvis_result_t Fetch() const
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

    void CheckOutputUnchanged() const
    {
        CHECK(Get(metrics.get(), kRPVAnalysisSummaryMetricPropertyAggregationLevel) ==
              kRPVAnalysisSummaryAggregationLevelNode);
        CHECK(Get(metrics.get(), kRPVAnalysisSummaryMetricPropertyNumSubMetrics) == 0);
    }
};

TEST_CASE_PERSISTENT_FIXTURE(SummaryFailureFixture, "Summary reports",
                             "[summary][failure]")
{
    if(controller == nullptr) Initialize();

    // Read-only checks first. These need a clean database, so they must run
    // before any section that drops a table or clears a counter.
    SECTION("Empty range")
    {
        start = timeline_end + 1;
        end   = start + 1;
        CHECK(Fetch() == kRocProfVisResultSuccess);
        CHECK(Get(metrics.get(), kRPVAnalysisSummaryMetricPropertyAggregationLevel) ==
              kRPVAnalysisSummaryAggregationLevelTrace);
        const uint64_t memory_events =
            Get(metrics.get(), kRPVAnalysisSummaryTraceNodePropertyNumTopMemoryEvents);
        INFO(DescribeMemoryEvents(memory_events));
        CHECK(memory_events == 0);
    }
    SECTION("Populated range")
    {
        start = timeline_start;
        end   = timeline_end;
        CHECK(Fetch() == kRocProfVisResultSuccess);
        CHECK(Get(metrics.get(), kRPVAnalysisSummaryMetricPropertyAggregationLevel) ==
              kRPVAnalysisSummaryAggregationLevelTrace);
        const uint64_t memory_events =
            Get(metrics.get(), kRPVAnalysisSummaryTraceNodePropertyNumTopMemoryEvents);
        INFO(DescribeMemoryEvents(memory_events));
        CHECK(memory_events > 0);
    }

    // Counter-statistics needs the backing tables present to reach the counter
    // code path, so it runs before the drop-table sections. The cleared
    // counter is restored afterward so later sections are not contaminated.
    SECTION("Counter-statistics failures")
    {
        const uint64_t num_tracks = Get(controller.get(), kRPVControllerSystemNumTracks);
        bool           injected   = false;
        RocProfVis::Controller::Counter* saved_counter = nullptr;
        for(uint64_t i = 0; i < num_tracks; i++)
        {
            rocprofvis_handle_t* handle = nullptr;
            REQUIRE(rocprofvis_controller_get_object(
                        controller.get(), kRPVControllerSystemTrackIndexed, i, &handle) ==
                    kRocProfVisResultSuccess);
            RocProfVis::Controller::Track* track =
                reinterpret_cast<RocProfVis::Controller::Track*>(handle);
            if(track->GetCounter() && track->GetNumberOfEntries() > 0)
            {
                // Break the track's counter association so statistics fetching
                // returns InvalidArgument, before it starts a database read.
                saved_counter = track->GetCounter();
                track->SetCounter(nullptr);
                injected = true;
                break;
            }
        }
        REQUIRE(injected);
        ArmUnchangedOutput();
        CHECK(Fetch() == kRocProfVisResultInvalidArgument);
        CheckOutputUnchanged();
        // Restore so the drop-table sections fail on SQL, not on the counter.
        for(uint64_t i = 0; i < num_tracks; i++)
        {
            rocprofvis_handle_t* handle = nullptr;
            rocprofvis_controller_get_object(
                controller.get(), kRPVControllerSystemTrackIndexed, i, &handle);
            RocProfVis::Controller::Track* track =
                reinterpret_cast<RocProfVis::Controller::Track*>(handle);
            if(track->GetCounter() == nullptr && track->GetNumberOfEntries() > 0)
            {
                track->SetCounter(saved_counter);
                break;
            }
        }
    }

    // Irreversible: these drop backing tables from the open database. They
    // must run last because the damage persists on the persistent fixture.
    SECTION("Memory query fails")
    {
        DropTable("rocpd_memory_copy");
        INFO("Dropped table: " << broken_table);
        ArmUnchangedOutput();
        CHECK(Fetch() == kRocProfVisResultUnknownError);
        CheckOutputUnchanged();
    }
    SECTION("Thread launch query fails")
    {
        DropTable("rocpd_region");
        INFO("Dropped table: " << broken_table);
        ArmUnchangedOutput();
        CHECK(Fetch() == kRocProfVisResultUnknownError);
        CheckOutputUnchanged();
    }
}
