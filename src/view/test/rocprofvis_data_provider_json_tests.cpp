// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "model/compute/rocprofvis_compute_data_model.h"
#include "model/rocprofvis_common_defs.h"
#include "model/rocprofvis_trace_data_model.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_core.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_data_provider_json.h"

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace RocProfVis::View;

std::string g_input_file =
    (std::filesystem::path(ROCPROFVIS_TEST_SOURCE_DIR) / "sample" / "rocpd-transpose.db")
        .string();

int
main(int argc, char** argv)
{
    Catch::Session session;

    using namespace Catch::Clara;
    auto cli = session.cli() |
               Opt(g_input_file, "input_file")["--input_file"]("Path to input file");
    session.cli(cli);

    int returnCode = session.applyCommandLine(argc, argv);
    if(returnCode != 0)
    {
        return returnCode;
    }

    std::string log_file = "Testing/Temporary/rocprofvis_data_provider_json_tests/" +
                           std::filesystem::path(g_input_file).filename().string() +
                           ".txt";
    rocprofvis_core_enable_log(log_file.c_str(), spdlog::level::info);

    return session.run();
}

// Long enough to load and fetch from the sample traces on a slow CI machine.
constexpr std::chrono::seconds WAIT_LIMIT(120);
constexpr uint32_t             TRACK_PIXELS      = 1000;
constexpr uint64_t             TABLE_ROWS        = 100;
constexpr uint64_t             METRICS_CLIENT_ID = 7;

// Updates the provider, the way a server's loop would, until done() holds or
// WAIT_LIMIT passes.
static bool
UpdateUntil(DataProvider& provider, const std::function<bool()>& done)
{
    std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + WAIT_LIMIT;
    while(!done() && std::chrono::steady_clock::now() < deadline)
    {
        provider.Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return done();
}

static bool
NoRequestsPending(DataProvider& provider)
{
    return UpdateUntil(provider,
                       [&provider]() { return provider.GetPendingRequestCount() == 0; });
}

static bool
Load(DataProvider& provider)
{
    rocprofvis_controller_t* controller =
        rocprofvis_controller_alloc(g_input_file.c_str(), nullptr);
    bool loaded = controller && provider.FetchTrace(controller, g_input_file);
    if(loaded)
    {
        loaded = UpdateUntil(provider,
                             [&provider]() {
                                 return provider.GetState() != ProviderState::kLoading;
                             }) &&
                 provider.GetState() == ProviderState::kReady;
    }
    return loaded;
}

// What every 64-bit id in the JSON must be: a string of decimal digits.
static bool
IsIdString(jt::Json& value)
{
    return value.isString() && !value.getString().empty() &&
           value.getString().find_first_not_of("0123456789") == std::string::npos;
}

TEST_CASE("A system trace serializes with ids that survive JavaScript", "[system]")
{
    DataProvider provider;
    REQUIRE(Load(provider));

    jt::Json snapshot = DataProviderJson::Snapshot(provider);
    REQUIRE(snapshot["status"]["state"].isString());
    CHECK(snapshot["status"]["state"].getString() == "ready");

    std::pair<jt::Json::Status, jt::Json> parsed = jt::Json::parse(snapshot.toString());
    CHECK(parsed.first == jt::Json::success);

    jt::Json& tracks = snapshot["system"]["timeline"]["tracks"];
    REQUIRE(tracks.isArray());
    REQUIRE_FALSE(tracks.getArray().empty());
    for(jt::Json& track : tracks.getArray())
    {
        REQUIRE(IsIdString(track["id"]));
        CHECK(IsIdString(track["topology"]["node_id"]));
    }

    jt::Json& first_id = tracks.getArray().front()["id"];
    jt::Json  track =
        DataProviderJson::Track(provider.DataModel(), std::stoull(first_id.getString()));
    REQUIRE(track["id"].isString());
    CHECK(track["id"].getString() == first_id.getString());
    CHECK(DataProviderJson::Track(provider.DataModel(), INVALID_UINT64_INDEX).isNull());

    jt::Json& nodes = snapshot["system"]["topology"]["nodes"];
    REQUIRE(nodes.isArray());
    CHECK_FALSE(nodes.getArray().empty());
}

TEST_CASE("The histogram is normalized with no timeline drawing it", "[system]")
{
    DataProvider provider;
    REQUIRE(Load(provider));

    jt::Json  histogram = DataProviderJson::Histogram(provider.DataModel().GetTimeline());
    jt::Json& bins      = histogram["bins"];
    REQUIRE(bins.isArray());
    REQUIRE_FALSE(bins.getArray().empty());
    double largest = 0.0;
    for(jt::Json& bin : bins.getArray())
    {
        REQUIRE(bin.isNumber());
        REQUIRE(std::isfinite(bin.getNumber()));
        CHECK(bin.getNumber() >= 0.0);
        CHECK(bin.getNumber() <= 1.0);
        largest = std::max(largest, bin.getNumber());
    }
    CHECK(largest > 0.0);
}

TEST_CASE("Track data, an event, a table and the summary load with no UI", "[system]")
{
    DataProvider provider;
    REQUIRE(Load(provider));
    TraceDataModel& model = provider.DataModel();

    const TrackInfo* events_track = nullptr;
    for(const TrackInfo* track : model.GetTimeline().GetTrackList())
    {
        if(!events_track && track && track->track_type == kRPVControllerTrackTypeEvents &&
           track->num_entries > 0)
        {
            events_track = track;
        }
    }
    REQUIRE(events_track != nullptr);
    const uint64_t track_id = events_track->id;

    CHECK(DataProviderJson::TrackData(model.GetTimeline(), track_id).isNull());
    REQUIRE(provider
                .FetchTrack(static_cast<uint32_t>(track_id), events_track->min_ts,
                            events_track->max_ts, TRACK_PIXELS, 0)
                .first);
    REQUIRE(UpdateUntil(provider, [&model, track_id]() {
        const RawTrackData* data = model.GetTimeline().GetTrackData(track_id);
        return data && data->AllDataReady();
    }));
    jt::Json data = DataProviderJson::TrackData(model.GetTimeline(), track_id);
    REQUIRE(data["events"].isArray());
    std::vector<jt::Json>& events = data["events"].getArray();
    REQUIRE_FALSE(events.empty());

    // A combined level-of-detail event stands for several, so pick a single one.
    jt::Json* single = nullptr;
    for(jt::Json& event : events)
    {
        REQUIRE(IsIdString(event["uuid"]));
        if(!single && event["child_count"].getLong() == 1)
        {
            single = &event;
        }
    }
    REQUIRE(single != nullptr);
    const uint64_t uuid = std::stoull((*single)["uuid"].getString());
    CHECK(DataProviderJson::Event(model.GetEvents(), uuid).isNull());
    REQUIRE(provider.FetchEvent(track_id, uuid));
    REQUIRE(NoRequestsPending(provider));
    jt::Json event = DataProviderJson::Event(model.GetEvents(), uuid);
    REQUIRE(event["uuid"].isString());
    CHECK(event["uuid"].getString() == std::to_string(uuid));
    CHECK(event["args"].isArray());

    CHECK(DataProviderJson::Table(model, TableType::kEventTable).isNull());
    REQUIRE(provider.FetchMultiTrackEventTable({ track_id }, events_track->min_ts,
                                               events_track->max_ts, "", "", "", 0,
                                               TABLE_ROWS));
    REQUIRE(NoRequestsPending(provider));
    jt::Json table = DataProviderJson::Table(model, TableType::kEventTable);
    REQUIRE(table["columns"].isArray());
    REQUIRE(table["rows"].isArray());
    REQUIRE_FALSE(table["rows"].getArray().empty());
    CHECK(table["rows"].getArray().front().getArray().size() ==
          table["columns"].getArray().size());
    REQUIRE(table["request"]["track_ids"].isArray());
    CHECK(table["request"]["track_ids"].getArray().front().getString() ==
          std::to_string(track_id));

    CHECK(DataProviderJson::Summary(model).isNull());
    REQUIRE(provider.FetchSummary());
    REQUIRE(NoRequestsPending(provider));
    jt::Json summary = DataProviderJson::Summary(model);
    REQUIRE(summary["root"]["level"].isString());
    CHECK(summary["root"]["level"].getString() == "trace");
}

TEST_CASE("Every table slot name maps back to its slot", "[system]")
{
    for(size_t i = 0; i < static_cast<size_t>(TableType::__kTableTypeCount); i++)
    {
        const TableType type        = static_cast<TableType>(i);
        TableType       parsed_type = TableType::__kTableTypeCount;
        REQUIRE(DataProviderJson::TableTypeFromName(DataProviderJson::TableTypeName(type),
                                                    parsed_type));
        CHECK(parsed_type == type);
    }
    TableType unknown = TableType::__kTableTypeCount;
    CHECK_FALSE(DataProviderJson::TableTypeFromName("not_a_table", unknown));
}

TEST_CASE("A compute trace serializes its workloads, kernels and metrics", "[compute]")
{
    DataProvider provider;
    REQUIRE(Load(provider));
    const ComputeDataModel& model = provider.ComputeModel();

    jt::Json                              compute = DataProviderJson::Compute(model);
    std::pair<jt::Json::Status, jt::Json> parsed  = jt::Json::parse(compute.toString());
    CHECK(parsed.first == jt::Json::success);

    REQUIRE(compute["profiler"]["version"].isString());
    CHECK(compute["profiler"]["version"].getString() ==
          model.GetAnalysisInfo().profiler_version);

    REQUIRE(compute["workloads"].isArray());
    std::vector<jt::Json>& workloads = compute["workloads"].getArray();
    REQUIRE(workloads.size() == model.GetWorkloadList().size());
    REQUIRE_FALSE(workloads.empty());
    REQUIRE(workloads.front()["kernels"].isArray());
    std::vector<jt::Json>& kernels = workloads.front()["kernels"].getArray();
    REQUIRE_FALSE(kernels.empty());
    // 32-bit ids stay numbers; only 64-bit ones become strings.
    CHECK(kernels.front()["id"].isLong());
    CHECK(kernels.front()["dispatch"]["invocation_count"].isLong());

    const WorkloadInfo* workload = model.GetWorkloadList().front();
    REQUIRE_FALSE(workload->ordered_kernels.empty());
    REQUIRE_FALSE(workload->available_metrics.ordered_categories.empty());
    const KernelInfo*              kernel = workload->ordered_kernels.front();
    MetricsRequestParams::MetricID category{
        workload->available_metrics.ordered_categories.front()->id, std::nullopt,
        std::nullopt
    };
    CHECK(DataProviderJson::KernelMetrics(model, METRICS_CLIENT_ID, kernel->id).isNull());
    REQUIRE(provider.FetchMetrics(MetricsRequestParams(workload->id, { kernel->id },
                                                       { category }, METRICS_CLIENT_ID)));
    REQUIRE(NoRequestsPending(provider));

    jt::Json metrics =
        DataProviderJson::KernelMetrics(model, METRICS_CLIENT_ID, kernel->id);
    REQUIRE(metrics["client_id"].isString());
    CHECK(metrics["client_id"].getString() == std::to_string(METRICS_CLIENT_ID));
    REQUIRE(metrics["metrics"].isArray());
    REQUIRE_FALSE(metrics["metrics"].getArray().empty());
    jt::Json& metric = metrics["metrics"].getArray().front();
    REQUIRE(metric["metric_id"].isString());
    CHECK(metric["metric_id"].getString().rfind(
              std::to_string(category.category_id) + ".", 0) == 0);
}
