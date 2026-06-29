// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// Unit tests for the view-side data models in src/view/src/model. These are
// pure in-memory caches/projections (no ImGui, no controller calls) that own
// CRUD + small query helpers, which makes them clean unit-test targets.

#include "rocprofvis_common_defs.h"
#include "rocprofvis_event_model.h"
#include "rocprofvis_model_types.h"
#include "rocprofvis_summary_model.h"
#include "rocprofvis_topology_model.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace RocProfVis::View;
using Catch::Matchers::WithinAbs;

namespace
{
EventInfo
MakeEvent(const std::string& name, double start_ts, double duration)
{
    EventInfo info{};
    info.basic_info.name     = name;
    info.basic_info.start_ts = start_ts;
    info.basic_info.duration = duration;
    return info;
}
}  // namespace

// ---------------------------------------------------------------------------
// EventModel
// ---------------------------------------------------------------------------

TEST_CASE("EventModel add/get/remove lifecycle", "[model][event]")
{
    EventModel model;
    CHECK(model.EventCount() == 0);
    CHECK(model.GetEvent(1) == nullptr);

    CHECK(model.AddEvent(1, MakeEvent("copy", 100.0, 50.0)));
    CHECK(model.EventCount() == 1);

    const EventInfo* ev = model.GetEvent(1);
    REQUIRE(ev != nullptr);
    CHECK(ev->basic_info.name == "copy");
    CHECK_THAT(ev->basic_info.start_ts, WithinAbs(100.0, 1e-9));

    SECTION("duplicate ids are rejected and do not overwrite")
    {
        CHECK_FALSE(model.AddEvent(1, MakeEvent("other", 0.0, 0.0)));
        CHECK(model.EventCount() == 1);
        CHECK(model.GetEvent(1)->basic_info.name == "copy");
    }

    SECTION("remove deletes the entry; removing twice fails")
    {
        CHECK(model.RemoveEvent(1));
        CHECK(model.EventCount() == 0);
        CHECK_FALSE(model.RemoveEvent(1));
    }

    SECTION("clear empties the model")
    {
        model.AddEvent(2, MakeEvent("kernel", 0.0, 0.0));
        model.ClearEvents();
        CHECK(model.EventCount() == 0);
    }
}

TEST_CASE("EventModel non-const accessor allows mutation", "[model][event]")
{
    EventModel model;
    model.AddEvent(7, MakeEvent("k", 0.0, 0.0));
    EventInfo* ev = model.GetEvent(7);
    REQUIRE(ev != nullptr);
    ev->basic_info.duration = 999.0;
    CHECK_THAT(model.GetEvent(7)->basic_info.duration, WithinAbs(999.0, 1e-9));
}

TEST_CASE("EventModel::GetEventTimeRange spans selected events", "[model][event]")
{
    EventModel model;
    model.AddEvent(1, MakeEvent("a", 100.0, 50.0));   // [100, 150]
    model.AddEvent(2, MakeEvent("b", 300.0, 200.0));  // [300, 500]
    model.AddEvent(3, MakeEvent("c", 50.0, 10.0));    // [50, 60]

    SECTION("empty id list returns false")
    {
        double start = 0.0, end = 0.0;
        CHECK_FALSE(model.GetEventTimeRange({}, start, end));
    }

    SECTION("range covers the earliest start and latest end")
    {
        double start = 0.0, end = 0.0;
        CHECK(model.GetEventTimeRange({1, 2, 3}, start, end));
        CHECK_THAT(start, WithinAbs(50.0, 1e-9));
        CHECK_THAT(end, WithinAbs(500.0, 1e-9));
    }

    SECTION("a single event yields its own bounds")
    {
        double start = 0.0, end = 0.0;
        CHECK(model.GetEventTimeRange({2}, start, end));
        CHECK_THAT(start, WithinAbs(300.0, 1e-9));
        CHECK_THAT(end, WithinAbs(500.0, 1e-9));
    }

    SECTION("unknown ids are skipped but a known one still succeeds")
    {
        double start = 0.0, end = 0.0;
        CHECK(model.GetEventTimeRange({999, 1, 12345}, start, end));
        CHECK_THAT(start, WithinAbs(100.0, 1e-9));
        CHECK_THAT(end, WithinAbs(150.0, 1e-9));
    }

    SECTION("only-unknown ids return false")
    {
        double start = 0.0, end = 0.0;
        CHECK_FALSE(model.GetEventTimeRange({999, 1000}, start, end));
    }
}

// ---------------------------------------------------------------------------
// TopologyDataModel
// ---------------------------------------------------------------------------

TEST_CASE("TopologyDataModel node CRUD and counts", "[model][topology]")
{
    TopologyDataModel model;
    CHECK(model.NodeCount() == 0);
    CHECK(model.GetNode(1) == nullptr);

    NodeInfo node{};
    node.id        = 1;
    node.host_name = "host-a";
    model.AddNode(1, std::move(node));

    CHECK(model.NodeCount() == 1);
    REQUIRE(model.GetNode(1) != nullptr);
    CHECK(model.GetNode(1)->host_name == "host-a");
    CHECK(model.GetNodeList().size() == 1);

    model.ClearNodes();
    CHECK(model.NodeCount() == 0);
}

TEST_CASE("TopologyDataModel::GetDeviceTypeLabel maps processor types", "[model][topology]")
{
    TopologyDataModel model;

    DeviceInfo gpu{};
    gpu.type       = kRPVControllerProcessorTypeGPU;
    gpu.type_index = 2;

    DeviceInfo cpu{};
    cpu.type       = kRPVControllerProcessorTypeCPU;
    cpu.type_index = 0;

    std::string label;
    CHECK(model.GetDeviceTypeLabel(gpu, label));
    CHECK(label == "GPU2");
    CHECK(model.GetDeviceTypeLabel(cpu, label));
    CHECK(label == "CPU0");
}

TEST_CASE("TopologyDataModel resolves a device through a queue", "[model][topology]")
{
    TopologyDataModel model;

    DeviceInfo device{};
    device.id.value   = 42;
    device.type       = kRPVControllerProcessorTypeGPU;
    device.type_index = 1;
    device.product_name = "MI300X";
    model.AddDevice(42, std::move(device));

    QueueInfo queue{};
    queue.id        = 7;
    queue.device_id = 42;
    model.AddQueue(7, std::move(queue));

    SECTION("queue -> device id")
    {
        CHECK(model.GetDeviceIdByInfoId(7, TrackInfo::TrackType::Queue) == 42u);
    }

    SECTION("queue -> device info")
    {
        const DeviceInfo* dev = model.GetDeviceByInfoId(7, TrackInfo::TrackType::Queue);
        REQUIRE(dev != nullptr);
        CHECK(dev->product_name == "MI300X");
    }

    SECTION("queue -> labels")
    {
        CHECK(model.GetDeviceTypeLabelByInfoId(7, TrackInfo::TrackType::Queue) == "GPU1");
        CHECK(model.GetDeviceProductLabelByInfoId(7, TrackInfo::TrackType::Queue) ==
              "MI300X");
    }

    SECTION("unknown queue id falls back to sentinels / defaults")
    {
        CHECK(model.GetDeviceIdByInfoId(999, TrackInfo::TrackType::Queue) ==
              INVALID_UINT64_INDEX);
        CHECK(model.GetDeviceByInfoId(999, TrackInfo::TrackType::Queue) == nullptr);
        CHECK(model.GetDeviceTypeLabelByInfoId(999, TrackInfo::TrackType::Queue,
                                               "n/a") == "n/a");
        CHECK(model.GetDeviceProductLabelByInfoId(999, TrackInfo::TrackType::Queue,
                                                  "n/a") == "n/a");
    }

    SECTION("track types without a device mapping return the sentinel")
    {
        CHECK(model.GetDeviceIdByInfoId(7, TrackInfo::TrackType::InstrumentedThread) ==
              INVALID_UINT64_INDEX);
    }
}

TEST_CASE("TopologyDataModel resolves a device through a counter", "[model][topology]")
{
    TopologyDataModel model;

    DeviceInfo device{};
    device.id.value   = 5;
    device.type       = kRPVControllerProcessorTypeCPU;
    device.type_index = 3;
    model.AddDevice(5, std::move(device));

    CounterInfo counter{};
    counter.id        = 11;
    counter.device_id = 5;
    model.AddCounter(11, std::move(counter));

    CHECK(model.GetDeviceIdByInfoId(11, TrackInfo::TrackType::Counter) == 5u);
    CHECK(model.GetDeviceTypeLabelByInfoId(11, TrackInfo::TrackType::Counter) == "CPU3");
}

TEST_CASE("TopologyDataModel::Clear wipes every category", "[model][topology]")
{
    TopologyDataModel model;
    model.AddNode(1, NodeInfo{});
    model.AddDevice(1, DeviceInfo{});
    model.AddProcess(1, ProcessInfo{});
    model.AddQueue(1, QueueInfo{});
    model.AddStream(1, StreamInfo{});
    model.AddCounter(1, CounterInfo{});
    model.AddInstrumentedThread(1, ThreadInfo{});
    model.AddSampledThread(1, ThreadInfo{});

    model.Clear();

    CHECK(model.NodeCount() == 0);
    CHECK(model.DeviceCount() == 0);
    CHECK(model.ProcessCount() == 0);
    CHECK(model.QueueCount() == 0);
    CHECK(model.StreamCount() == 0);
    CHECK(model.CounterCount() == 0);
    CHECK(model.InstrumentedThreadCount() == 0);
    CHECK(model.SampledThreadCount() == 0);
}

// ---------------------------------------------------------------------------
// SummaryModel
// ---------------------------------------------------------------------------

TEST_CASE("SummaryModel stores and clears aggregate metrics", "[model][summary]")
{
    SummaryModel model;

    SummaryInfo::AggregateMetrics metrics{};
    metrics.type = kRPVControllerSummaryAggregationLevelNode;
    metrics.name = std::string("node-0");
    metrics.gpu.kernel_exec_time_total = 1234.0;
    model.SetSummaryData(std::move(metrics));

    const SummaryInfo::AggregateMetrics& stored = model.GetSummaryData();
    REQUIRE(stored.name.has_value());
    CHECK(stored.name.value() == "node-0");
    CHECK_THAT(stored.gpu.kernel_exec_time_total, WithinAbs(1234.0, 1e-9));

    SECTION("non-const accessor allows in-place mutation")
    {
        model.GetSummaryData().gpu.kernel_exec_time_total = 42.0;
        CHECK_THAT(model.GetSummaryData().gpu.kernel_exec_time_total,
                   WithinAbs(42.0, 1e-9));
    }

    SECTION("clear resets to a default-constructed aggregate")
    {
        model.Clear();
        CHECK_FALSE(model.GetSummaryData().name.has_value());
        CHECK(model.GetSummaryData().gpu.top_kernels.empty());
    }
}
