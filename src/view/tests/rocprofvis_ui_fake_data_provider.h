// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_data_provider.h"
#include "rocprofvis_model_types.h"

#include <cstdint>
#include <utility>

namespace RocProfVis
{
namespace View
{
namespace Test
{

class FakeDataProvider
{
public:
    static constexpr uint64_t kTrackId = 7;

    DataProvider& Get()
    {
        return m_provider;
    }

    uint64_t SeedEventDetails()
    {
        TraceEventId event_id;
        event_id.uuid                    = 0;
        event_id.bitfield.event_id       = 42;
        event_id.bitfield.event_node     = 0;
        event_id.bitfield.event_op       = 1;

        EventInfo event;
        event.track_id                   = kTrackId;
        event.basic_info.id              = event_id;
        event.basic_info.name            = "hipMemcpy";
        event.basic_info.start_ts        = 1000.0;
        event.basic_info.duration        = 250.0;
        event.basic_info.level           = 0;
        event.basic_info.stream_level    = 0;
        event.args.push_back({ 0, "dst", "0x1000", "void*" });
        event.args.push_back({ 1, "sizeBytes", "4096", "size_t" });
        event.ext_info.push_back({ "Runtime", "queue", "gfx942 queue 0", 0 });
        event.call_stack_info.push_back(
            { "kernel.cpp", "0x40", "launchMemcpy", "0x7ff0" });

        m_provider.DataModel().SetTraceFilePath("ui-test://trace");
        m_provider.DataModel().GetTimeline().SetTimeRange(0.0, 2000.0);
        m_provider.DataModel().GetEvents().AddEvent(event_id.uuid, std::move(event));

        return event_id.uuid;
    }

private:
    DataProvider m_provider;
};

}  // namespace Test
}  // namespace View
}  // namespace RocProfVis
