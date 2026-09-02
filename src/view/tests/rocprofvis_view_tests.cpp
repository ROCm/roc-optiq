// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// Headless VIEW-layer integrity tests for in-place add/remove. They drive a real DataProvider
// (no window/GL/ImGui) and assert on the models the UI renders from (TimelineModel / TrackInfo /
// mini-map / TopologyDataModel), catching view-only bugs - wrong transcription, orphan/remnant
// tracks, missing mini-map strips - that controller-level tests cannot see. Needs two distinct,
// schema-compatible traces (--input_file / --input_file_b); self-skips without a second file.

#include "rocprofvis_controller.h"
#include "rocprofvis_data_provider.h"
#include "model/rocprofvis_trace_data_model.h"
#include "model/rocprofvis_timeline_model.h"
#include "model/rocprofvis_topology_model.h"
#include "model/rocprofvis_model_types.h"
#include "rocprofvis_core.h"

#include <algorithm>
#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <set>
#include <string>
#include <thread>
#include <vector>

// Input traces, repeatable (pass --input_file per file). Index 0 is the primary trace (defaults to
// the bundled sample); 1, 2, ... are extra traces for the multi-trace view suites, which skip when
// too few are given. Adding a trace needs no new global or flag - just one more --input_file.
std::vector<std::string> g_input_traces;

// The i-th --input_file (by reference so callers may take .c_str()), or "" when not supplied.
static const std::string&
InputFile(size_t index = 0)
{
    static const std::string kEmpty;
    return index < g_input_traces.size() ? g_input_traces[index] : kEmpty;
}

int
main(int argc, char** argv)
{
    Catch::Session session;

    using namespace Catch::Clara;
    auto cli = session.cli() |
               Opt(g_input_traces, "input_file")["--input_file"](
                   "Path to an input trace; repeat the flag to supply extra traces for the "
                   "multi-trace view suites");
    session.cli(cli);

    int returnCode = session.applyCommandLine(argc, argv);
    if(returnCode != 0)
    {
        return returnCode;
    }

    // Default the primary trace so the single-file suites still run with no arguments.
    if(g_input_traces.empty())
    {
        g_input_traces.push_back("sample/rocpd-transpose.db");
    }
    return session.run();
}

using namespace RocProfVis::View;

namespace
{
// A content fingerprint of one track as the VIEW models it. No timestamps: merging distinct
// traces rebases their timelines, so absolute min/max are not comparable across a merge.
struct ViewTrackFingerprint
{
    std::string category;
    std::string main_name;
    std::string sub_name;
    uint64_t    track_type  = 0;
    uint64_t    num_entries = 0;
};

bool
operator<(const ViewTrackFingerprint& a, const ViewTrackFingerprint& b)
{
    if(a.category != b.category) return a.category < b.category;
    if(a.main_name != b.main_name) return a.main_name < b.main_name;
    if(a.sub_name != b.sub_name) return a.sub_name < b.sub_name;
    if(a.track_type != b.track_type) return a.track_type < b.track_type;
    return a.num_entries < b.num_entries;
}

bool
operator==(const ViewTrackFingerprint& a, const ViewTrackFingerprint& b)
{
    return a.category == b.category && a.main_name == b.main_name && a.sub_name == b.sub_name &&
           a.track_type == b.track_type && a.num_entries == b.num_entries;
}

// A snapshot of the view-side models after a load/add/remove settles.
struct ViewSnapshot
{
    uint64_t                         track_count   = 0;
    uint64_t                         total_entries = 0;
    std::vector<ViewTrackFingerprint> tracks;      // sorted
    std::set<uint64_t>               track_ids;    // ids present in the timeline model
    std::set<uint64_t>               minimap_ids;  // ids that have a mini-map strip
    size_t                           nodes            = 0;
    size_t                           devices          = 0;
    size_t                           processes        = 0;
    size_t                           instr_threads    = 0;
    size_t                           sampled_threads  = 0;
    size_t                           queues           = 0;
    size_t                           streams          = 0;
    size_t                           counters         = 0;
};

void
PumpToReady(DataProvider& provider)
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
    while(provider.GetState() == ProviderState::kLoading &&
          std::chrono::steady_clock::now() < deadline)
    {
        provider.Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    REQUIRE(provider.GetState() == ProviderState::kReady);
}

ViewSnapshot
SnapshotView(DataProvider& provider)
{
    ViewSnapshot         snap;
    TraceDataModel&      model    = provider.DataModel();
    TimelineModel&       timeline = model.GetTimeline();
    TopologyDataModel&   topology = model.GetTopology();

    snap.track_count = timeline.GetTrackCount();
    for(const TrackInfo* track : timeline.GetTrackList())
    {
        if(track == nullptr)
        {
            continue;
        }
        ViewTrackFingerprint fp;
        fp.category    = track->category;
        fp.main_name   = track->main_name;
        fp.sub_name    = track->sub_name;
        fp.track_type  = static_cast<uint64_t>(track->track_type);
        fp.num_entries = track->num_entries;
        snap.tracks.push_back(fp);
        snap.total_entries += track->num_entries;
        snap.track_ids.insert(track->id);
    }
    std::sort(snap.tracks.begin(), snap.tracks.end());

    for(const auto& [track_id, strip] : timeline.GetMiniMap())
    {
        snap.minimap_ids.insert(track_id);
    }

    snap.nodes           = topology.NodeCount();
    snap.devices         = topology.DeviceCount();
    snap.processes       = topology.ProcessCount();
    snap.instr_threads   = topology.InstrumentedThreadCount();
    snap.sampled_threads = topology.SampledThreadCount();
    snap.queues          = topology.QueueCount();
    snap.streams         = topology.StreamCount();
    snap.counters        = topology.CounterCount();
    return snap;
}

// Every state must be internally consistent: the track count matches the track list, and the
// mini-map has exactly one strip per track (no missing strips, no orphan/remnant strips). This
// is the core view-transcription invariant.
void
RequireConsistent(const ViewSnapshot& snap)
{
    REQUIRE(snap.track_count == snap.tracks.size());
    REQUIRE(snap.track_count == snap.track_ids.size());
    REQUIRE(snap.minimap_ids == snap.track_ids);
}

// Two view snapshots must be equivalent (track set, counts, topology counts). Used to compare an
// in-place result against a reference built a different way, and a reduced-to-one-file state
// against a clean single open.
void
RequireViewEqual(const ViewSnapshot& actual, const ViewSnapshot& expected)
{
    REQUIRE(actual.track_count == expected.track_count);
    REQUIRE(actual.total_entries == expected.total_entries);
    REQUIRE(actual.tracks == expected.tracks);
    REQUIRE(actual.nodes == expected.nodes);
    REQUIRE(actual.devices == expected.devices);
    REQUIRE(actual.processes == expected.processes);
    REQUIRE(actual.instr_threads == expected.instr_threads);
    REQUIRE(actual.sampled_threads == expected.sampled_threads);
    REQUIRE(actual.queues == expected.queues);
    REQUIRE(actual.streams == expected.streams);
    REQUIRE(actual.counters == expected.counters);
}

// A merged view of two distinct files must be the union of the two single opens (track and
// record counts add; the track set is the multiset union).
void
RequireViewUnion(const ViewSnapshot& actual, const ViewSnapshot& a, const ViewSnapshot& b)
{
    REQUIRE(actual.track_count == a.track_count + b.track_count);
    REQUIRE(actual.total_entries == a.total_entries + b.total_entries);

    std::vector<ViewTrackFingerprint> merged = a.tracks;
    merged.insert(merged.end(), b.tracks.begin(), b.tracks.end());
    std::sort(merged.begin(), merged.end());
    REQUIRE(actual.tracks == merged);
}

// Loads a single trace through a DataProvider and snapshots the view models.
ViewSnapshot
SnapshotSingle(const std::string& path)
{
    DataProvider             provider;
    rocprofvis_controller_t* controller = rocprofvis_controller_alloc(path.c_str(), nullptr);
    REQUIRE(controller != nullptr);
    REQUIRE(provider.FetchTrace(controller, path));
    PumpToReady(provider);
    return SnapshotView(provider);
}

// Loads two traces via the batch combined-open path and snapshots the view models. Returns
// false in `ok` if the combined open is unsupported for the pair.
ViewSnapshot
SnapshotCombined(const std::string& a, const std::string& b, bool& ok)
{
    ok = false;
    std::vector<const char*> ptrs{ a.c_str(), b.c_str() };
    rocprofvis_controller_t* controller =
        rocprofvis_controller_alloc_compare(ptrs.data(), ptrs.size());
    if(controller == nullptr)
    {
        return ViewSnapshot{};
    }
    DataProvider provider;
    if(!provider.FetchTrace(controller, a))
    {
        return ViewSnapshot{};
    }
    PumpToReady(provider);
    ok = true;
    return SnapshotView(provider);
}
}  // namespace

struct ViewIntegrityFixture
{
    mutable bool         ready              = false;
    mutable bool         have_two_files     = false;
    mutable bool         combined_available = false;
    mutable ViewSnapshot single_a;
    mutable ViewSnapshot single_b;
    mutable ViewSnapshot combined_ref;
};

TEST_CASE_PERSISTENT_FIXTURE(ViewIntegrityFixture, "In-place multi-trace view integrity")
{
    const std::string file_a = InputFile();
    const std::string file_b = InputFile(1);

    SECTION("reference single-open view snapshots of both files")
    {
        if(file_b.empty())
        {
            WARN("Provide --input_file_b with a second compatible trace to run the multi-trace "
                 "view integrity suite; skipping.");
            ready = true;
            return;
        }
        have_two_files = true;

        single_a = SnapshotSingle(file_a);
        REQUIRE(single_a.track_count > 0);
        RequireConsistent(single_a);

        single_b = SnapshotSingle(file_b);
        REQUIRE(single_b.track_count > 0);
        RequireConsistent(single_b);

        bool ok      = false;
        combined_ref = SnapshotCombined(file_a, file_b, ok);
        combined_available = ok;
        if(!combined_available)
        {
            WARN("Batch combined open unavailable for this pair; 2-file scenarios fall back to "
                 "union checks against the single opens.");
        }
        ready = true;
    }

    SECTION("open two files at once equals the union of both")
    {
        REQUIRE(ready);
        if(!have_two_files) return;
        if(!combined_available)
        {
            WARN("Combined open unavailable; skipping.");
            return;
        }
        RequireConsistent(combined_ref);
        RequireViewUnion(combined_ref, single_a, single_b);
    }

    // Also checked identical to the batch combined view when available.
    SECTION("open one then add another equals the union of both")
    {
        REQUIRE(ready);
        if(!have_two_files) return;
        DataProvider             provider;
        rocprofvis_controller_t* controller = rocprofvis_controller_alloc(file_a.c_str(), nullptr);
        REQUIRE(controller != nullptr);
        REQUIRE(provider.FetchTrace(controller, file_a));
        PumpToReady(provider);
        REQUIRE(provider.AddTraceSource(file_b));
        PumpToReady(provider);

        ViewSnapshot added = SnapshotView(provider);
        RequireConsistent(added);
        RequireViewUnion(added, single_a, single_b);
        if(combined_available)
        {
            RequireViewEqual(added, combined_ref);
        }
    }

    SECTION("add then remove the added file restores the original view")
    {
        REQUIRE(ready);
        if(!have_two_files) return;
        DataProvider             provider;
        rocprofvis_controller_t* controller = rocprofvis_controller_alloc(file_a.c_str(), nullptr);
        REQUIRE(controller != nullptr);
        REQUIRE(provider.FetchTrace(controller, file_a));
        PumpToReady(provider);
        REQUIRE(provider.AddTraceSource(file_b));
        PumpToReady(provider);
        REQUIRE(provider.RemoveTraceSource(file_b));
        PumpToReady(provider);

        ViewSnapshot result = SnapshotView(provider);
        RequireConsistent(result);
        RequireViewEqual(result, single_a);
    }

    // Removing the original (owns the shared topology ancestors) exercises the prune path.
    SECTION("add then remove the original file leaves the added file view")
    {
        REQUIRE(ready);
        if(!have_two_files) return;
        DataProvider             provider;
        rocprofvis_controller_t* controller = rocprofvis_controller_alloc(file_a.c_str(), nullptr);
        REQUIRE(controller != nullptr);
        REQUIRE(provider.FetchTrace(controller, file_a));
        PumpToReady(provider);
        REQUIRE(provider.AddTraceSource(file_b));
        PumpToReady(provider);
        REQUIRE(provider.RemoveTraceSource(file_a));
        PumpToReady(provider);

        ViewSnapshot result = SnapshotView(provider);
        RequireConsistent(result);
        RequireViewEqual(result, single_b);
    }

    SECTION("repeated add/remove cycles restore the original view")
    {
        REQUIRE(ready);
        if(!have_two_files) return;
        DataProvider             provider;
        rocprofvis_controller_t* controller = rocprofvis_controller_alloc(file_a.c_str(), nullptr);
        REQUIRE(controller != nullptr);
        REQUIRE(provider.FetchTrace(controller, file_a));
        PumpToReady(provider);
        for(int cycle = 0; cycle < 3; cycle++)
        {
            REQUIRE(provider.AddTraceSource(file_b));
            PumpToReady(provider);
            REQUIRE(provider.RemoveTraceSource(file_b));
            PumpToReady(provider);
        }

        ViewSnapshot result = SnapshotView(provider);
        RequireConsistent(result);
        RequireViewEqual(result, single_a);
    }
}

// Reproduces "merged view of two traces, then add a third in place" through the real DataProvider
// (headless). Matches a user-reported crash. Needs three --input_file traces.
TEST_CASE("In-place add a third trace to a combined view (view models)")
{
    if(InputFile().empty() || InputFile(1).empty() || InputFile(2).empty())
    {
        WARN("Needs three --input_file traces; skipping.");
        return;
    }
    std::vector<const char*> ptrs{ InputFile().c_str(), InputFile(1).c_str() };
    rocprofvis_controller_t* controller =
        rocprofvis_controller_alloc_compare(ptrs.data(), ptrs.size());
    REQUIRE(controller != nullptr);
    DataProvider provider;
    REQUIRE(provider.FetchTrace(controller, InputFile()));
    PumpToReady(provider);

    ViewSnapshot before = SnapshotView(provider);
    RequireConsistent(before);

    const bool added = provider.AddTraceSource(InputFile(2));
    PumpToReady(provider);

    ViewSnapshot after = SnapshotView(provider);
    RequireConsistent(after);
    if(added)
    {
        REQUIRE(after.track_count >= before.track_count);
    }

    // Drive the render data path headlessly: fetch every track over the full range and pump to
    // completion - the segment fetch/merge/process work SnapshotView doesn't reach.
    TimelineModel&        tlm = provider.DataModel().GetTimeline();
    const double          s   = tlm.GetStartTime();
    const double          e   = tlm.GetEndTime();
    std::vector<uint64_t> ids;
    for(const TrackInfo* t : tlm.GetTrackList())
    {
        if(t)
        {
            ids.push_back(t->id);
        }
    }
    for(uint64_t id : ids)
    {
        std::pair<bool, uint64_t> r =
            provider.FetchTrack(static_cast<uint32_t>(id), s, e, 2000, 0);
        if(!r.first)
        {
            continue;
        }
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
        while(provider.IsRequestPending(r.second) &&
              std::chrono::steady_clock::now() < deadline)
        {
            provider.Update();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
    SUCCEED("fetched all tracks after 3-way add without crashing");
}
