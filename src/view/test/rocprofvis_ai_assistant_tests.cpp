// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// Ask Optiq logic that needs no trace, UI, or endpoint: how the link store
// survives its file, and the run diff behind compare_traces.

#include "agenticprofiling/rocprofvis_ai_trace_compare.h"
#include "agenticprofiling/rocprofvis_ai_trace_links.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace RocProfVis::View;

namespace
{

// A fresh path in a scratch directory, with nothing left at it or beside it
// from an earlier run.
std::filesystem::path
ScratchFile(const char* name)
{
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "rocprofvis_ai_assistant_tests";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const std::filesystem::path path = dir / name;
    std::filesystem::path       temp = path;
    temp += ".tmp";
    std::filesystem::remove(path, ec);
    std::filesystem::remove(temp, ec);
    return path;
}

void
WriteText(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
}

std::string
ReadText(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

TraceLinkMember
Member(const std::string& path, const std::string& label)
{
    TraceLinkMember member;
    member.path  = path;
    member.label = label;
    return member;
}

RunCategoryFigures
Category(const std::vector<RunEventFigures>& events, bool truncated)
{
    RunCategoryFigures figures;
    figures.events    = events;
    figures.truncated = truncated;
    for(const RunEventFigures& event : events)
    {
        figures.total_ns += event.total_ns;
        figures.calls += event.calls;
    }
    return figures;
}

bool
Contains(const std::string& text, const std::string& needle)
{
    return text.find(needle) != std::string::npos;
}

}  // namespace

TEST_CASE("Trace links survive a reload", "[assistant][links]")
{
    const std::filesystem::path path = ScratchFile("reload.json");
    {
        TraceLinkStore store(path.string());
        REQUIRE(store.Link(Member("C:/runs/a.db", "a.db"),
                           Member("C:/runs/b.db", "b.db")) == TraceLinkResult::kLinked);
        REQUIRE(store.Link(Member("C:/runs/a.db", "a.db"),
                           Member("C:/runs/c.db", "c.db")) == TraceLinkResult::kLinked);
    }

    TraceLinkStore reloaded(path.string());
    REQUIRE(reloaded.Groups().size() == 2);
    const std::vector<TraceLink> from_a = reloaded.LinksOf("C:/runs/a.db");
    REQUIRE(from_a.size() == 2);
    CHECK(from_a[0].self.role == TRACE_LINK_ROLE_BASELINE);
    CHECK(from_a[0].other.path == "C:/runs/b.db");
    CHECK(from_a[0].other.role == TRACE_LINK_ROLE_TARGET);
    CHECK(from_a[0].other.label == "b.db");
    CHECK(from_a[1].other.path == "C:/runs/c.db");

    const std::vector<TraceLink> from_b = reloaded.LinksOf("C:/runs/b.db");
    REQUIRE(from_b.size() == 1);
    CHECK(from_b[0].self.role == TRACE_LINK_ROLE_TARGET);
    CHECK(from_b[0].other.role == TRACE_LINK_ROLE_BASELINE);
    CHECK(reloaded.LinksOf("C:/runs/elsewhere.db").empty());
    CHECK(reloaded.LinksOf("").empty());
}

TEST_CASE("Linking refuses one trace twice and repeats in either order",
          "[assistant][links]")
{
    TraceLinkStore store(ScratchFile("refuse.json").string());
    CHECK(store.Link(Member("C:/a.db", "a"), Member("C:/a.db", "a")) ==
          TraceLinkResult::kInvalid);
    CHECK(store.Link(Member("", "a"), Member("C:/b.db", "b")) ==
          TraceLinkResult::kInvalid);
    REQUIRE(store.Link(Member("C:/a.db", "a"), Member("C:/b.db", "b")) ==
            TraceLinkResult::kLinked);
    CHECK(store.Link(Member("C:/b.db", "b"), Member("C:/a.db", "a")) ==
          TraceLinkResult::kAlreadyLinked);
    CHECK(store.Groups().size() == 1);
}

TEST_CASE("Unlinked ids are never handed out again", "[assistant][links]")
{
    const std::filesystem::path path = ScratchFile("ids.json");
    std::string                 first_id;
    {
        TraceLinkStore store(path.string());
        REQUIRE(store.Link(Member("C:/a.db", "a"), Member("C:/b.db", "b")) ==
                TraceLinkResult::kLinked);
        REQUIRE(store.Link(Member("C:/a.db", "a"), Member("C:/c.db", "c")) ==
                TraceLinkResult::kLinked);
        const std::string last_id = store.Groups().back().id;
        first_id                  = store.Groups().front().id;
        CHECK(first_id != last_id);

        REQUIRE(store.Unlink(last_id) == TraceUnlinkResult::kUnlinked);
        CHECK(store.Unlink(last_id) == TraceUnlinkResult::kNotFound);
        REQUIRE(store.Link(Member("C:/b.db", "b"), Member("C:/c.db", "c")) ==
                TraceLinkResult::kLinked);
        CHECK(store.Groups().back().id != last_id);
    }

    // The counter is persisted, so a reload does not start over either.
    TraceLinkStore reloaded(path.string());
    REQUIRE(reloaded.Groups().size() == 2);
    CHECK(reloaded.Groups().front().id == first_id);
    REQUIRE(reloaded.Link(Member("C:/a.db", "a"), Member("C:/d.db", "d")) ==
            TraceLinkResult::kLinked);
    for(size_t i = 0; i + 1 < reloaded.Groups().size(); ++i)
    {
        CHECK(reloaded.Groups()[i].id != reloaded.Groups().back().id);
    }
}

TEST_CASE("A malformed file loads empty and is replaced on the next link",
          "[assistant][links]")
{
    const std::filesystem::path path = ScratchFile("malformed.json");
    WriteText(path, "{ not json");
    TraceLinkStore store(path.string());
    CHECK(store.Groups().empty());
    REQUIRE(store.Link(Member("C:/a.db", "a"), Member("C:/b.db", "b")) ==
            TraceLinkResult::kLinked);

    TraceLinkStore reloaded(path.string());
    CHECK(reloaded.Groups().size() == 1);
}

TEST_CASE("A file from a newer build is read but never overwritten", "[assistant][links]")
{
    const std::filesystem::path path  = ScratchFile("newer.json");
    const std::string           newer = R"({"version": 99, "future": true, "groups": [)"
                                        R"({"id": "link-7", "members": [)"
                                        R"({"path": "C:/a.db", "role": "baseline"},)"
                                        R"({"path": "C:/b.db", "role": "target"}]}]})";
    WriteText(path, newer);

    TraceLinkStore store(path.string());
    REQUIRE(store.Groups().size() == 1);
    CHECK(store.Groups().front().id == "link-7");
    CHECK(store.Link(Member("C:/a.db", "a"), Member("C:/c.db", "c")) ==
          TraceLinkResult::kLinkedNotSaved);
    CHECK(store.LinksOf("C:/a.db").size() == 2);
    CHECK(store.Unlink("link-7") == TraceUnlinkResult::kUnlinkedNotSaved);
    CHECK(ReadText(path) == newer);
}

TEST_CASE("Malformed and duplicate groups are skipped", "[assistant][links]")
{
    const std::filesystem::path path = ScratchFile("partial.json");
    WriteText(path, R"({"version": 1, "groups": [)"
                    R"({"id": "link-1", "members": [{"path": "C:/a.db"}]},)"
                    R"({"members": [{"path": "C:/a.db", "role": "baseline"},)"
                    R"({"path": "C:/b.db", "role": "target"}]},)"
                    R"("not a group",)"
                    R"({"id": "link-3", "kind": "future_kind", "members": [)"
                    R"({"path": "C:/a.db", "role": "baseline"},)"
                    R"({"path": "C:/b.db", "role": "target"}]},)"
                    R"({"id": "link-4", "members": [)"
                    R"({"path": "C:/a.db", "role": "baseline"},)"
                    R"({"path": "C:/a.db", "role": "target"}]},)"
                    R"({"id": "link-5", "members": [)"
                    R"({"path": "C:/a.db", "role": "baseline"},)"
                    R"({"path": "C:/b.db", "role": "target"},)"
                    R"({"path": "C:/c.db", "role": "target"}]},)"
                    R"({"id": "link-6", "members": [)"
                    R"({"path": "C:/a.db", "role": "baseline"},)"
                    R"({"path": "C:/b.db", "role": "target"}]},)"
                    R"({"id": "link-6", "members": [)"
                    R"({"path": "C:/c.db", "role": "baseline"},)"
                    R"({"path": "C:/d.db", "role": "target"}]},)"
                    R"({"id": "link-7", "members": [)"
                    R"({"path": "C:/b.db", "role": "baseline"},)"
                    R"({"path": "C:/a.db", "role": "target"}]}]})");

    TraceLinkStore store(path.string());
    REQUIRE(store.Groups().size() == 1);
    CHECK(store.Groups().front().id == "link-6");
    CHECK(store.Groups().front().members.size() == 2);

    // link-6 is taken, so the next id moves past it.
    REQUIRE(store.Link(Member("C:/b.db", "b"), Member("C:/d.db", "d")) ==
            TraceLinkResult::kLinked);
    CHECK(store.Groups().back().id == "link-8");
}

TEST_CASE("A leftover temporary file does not disturb loading or saving",
          "[assistant][links]")
{
    const std::filesystem::path path = ScratchFile("leftover.json");
    {
        TraceLinkStore store(path.string());
        REQUIRE(store.Link(Member("C:/a.db", "a"), Member("C:/b.db", "b")) ==
                TraceLinkResult::kLinked);
    }
    std::filesystem::path temp = path;
    temp += ".tmp";
    WriteText(temp, "{ half written");

    TraceLinkStore reloaded(path.string());
    REQUIRE(reloaded.Groups().size() == 1);
    REQUIRE(reloaded.Link(Member("C:/a.db", "a"), Member("C:/c.db", "c")) ==
            TraceLinkResult::kLinked);
    CHECK(TraceLinkStore(path.string()).Groups().size() == 2);
}

TEST_CASE("Fingerprints tell captures apart and survive the file", "[assistant][links]")
{
    TraceFingerprint unset;
    TraceFingerprint capture{ 1000.0, 9000.0, 12 };
    CHECK(unset.Matches(capture));
    CHECK(capture.Matches(unset));
    CHECK(capture.Matches(capture));

    TraceFingerprint longer = capture;
    longer.end_ns           = 9500.0;
    CHECK_FALSE(capture.Matches(longer));
    TraceFingerprint more_tracks = capture;
    more_tracks.track_count      = 13;
    CHECK_FALSE(capture.Matches(more_tracks));

    const std::filesystem::path path     = ScratchFile("fingerprint.json");
    TraceLinkMember             baseline = Member("C:/a.db", "a");
    baseline.fingerprint = TraceFingerprint{ 1765432109876.0, 1765440000000.0, 87 };
    {
        TraceLinkStore store(path.string());
        REQUIRE(store.Link(baseline, Member("C:/b.db", "b")) == TraceLinkResult::kLinked);
    }
    const std::vector<TraceLink> links = TraceLinkStore(path.string()).LinksOf("C:/b.db");
    REQUIRE(links.size() == 1);
    CHECK(links[0].other.fingerprint.Matches(baseline.fingerprint));
    CHECK(links[0].other.fingerprint.track_count == 87);
}

TEST_CASE("Equal runs show no change", "[assistant][compare]")
{
    const RunCategoryFigures run =
        Category({ { "k1", 1000, 10 }, { "k2", 500, 5 } }, false);
    const std::vector<RunDiffRow> rows = DiffRunCategory(run, run);
    REQUIRE(rows.size() == 2);
    for(const RunDiffRow& row : rows)
    {
        CHECK(row.state == RunDiffState::kBoth);
        CHECK(row.baseline_ns == row.target_ns);
    }
    CHECK(FormatRunChange(1000, 1000) == "1000 -> 1000 (0, 0.0%)");
    CHECK_FALSE(Contains(FormatRunCategory("kernels", run, run, 10), "per_call_ns"));
}

TEST_CASE("Changes are signed and ranked by size", "[assistant][compare]")
{
    const RunCategoryFigures baseline =
        Category({ { "slow", 1000, 10 }, { "fast", 100, 10 } }, false);
    const RunCategoryFigures target =
        Category({ { "fast", 150, 10 }, { "slow", 400, 10 } }, false);
    const std::vector<RunDiffRow> rows = DiffRunCategory(baseline, target);
    REQUIRE(rows.size() == 2);
    CHECK(rows[0].name == "slow");
    CHECK(rows[1].name == "fast");
    CHECK(FormatRunChange(1000, 400) == "1000 -> 400 (-600, -60.0%)");
    CHECK(FormatRunChange(100, 150) == "100 -> 150 (+50, +50.0%)");
}

TEST_CASE("A zero baseline gets no percentage", "[assistant][compare]")
{
    CHECK(FormatRunChange(0, 250) == "0 -> 250 (+250)");
    CHECK(FormatRunChange(0, 0) == "0 -> 0 (0)");
}

TEST_CASE("Names in one run are new or gone when both lists are complete",
          "[assistant][compare]")
{
    const RunCategoryFigures baseline =
        Category({ { "old", 300, 3 }, { "same", 100, 1 } }, false);
    const RunCategoryFigures target =
        Category({ { "same", 100, 1 }, { "replacement", 200, 3 } }, false);
    const std::vector<RunDiffRow> rows = DiffRunCategory(baseline, target);
    REQUIRE(rows.size() == 3);
    CHECK(rows[0].name == "old");
    CHECK(rows[0].state == RunDiffState::kGone);
    CHECK(rows[1].name == "replacement");
    CHECK(rows[1].state == RunDiffState::kNew);
    CHECK(rows[2].name == "same");
    CHECK(rows[2].state == RunDiffState::kBoth);

    const std::string text = FormatRunCategory("kernels", baseline, target, 10);
    CHECK(Contains(text, "gone from target (baseline total_ns 300, calls 3)"));
    CHECK(Contains(text, "new in target (total_ns 200, calls 3)"));
}

TEST_CASE("A name missing beside an Others row is unknown, not gone",
          "[assistant][compare]")
{
    const RunCategoryFigures baseline =
        Category({ { "big", 500, 5 }, { "small", 900, 1 } }, false);
    const RunCategoryFigures      target = Category({ { "big", 400, 5 } }, true);
    const std::vector<RunDiffRow> rows   = DiffRunCategory(baseline, target);
    REQUIRE(rows.size() == 2);
    // Known changes rank ahead of unknown ones, even a smaller known change.
    CHECK(rows[0].name == "big");
    CHECK(rows[1].name == "small");
    CHECK(rows[1].state == RunDiffState::kHiddenInTarget);
    CHECK(Contains(FormatRunCategory("kernels", baseline, target, 10),
                   "the target keeps this name inside Others"));

    const std::vector<RunDiffRow> reversed = DiffRunCategory(target, baseline);
    REQUIRE(reversed.size() == 2);
    CHECK(reversed[1].state == RunDiffState::kHiddenInBaseline);
}

TEST_CASE("Different call counts show the time per call", "[assistant][compare]")
{
    const RunCategoryFigures baseline = Category({ { "k", 1000, 10 } }, false);
    const RunCategoryFigures target   = Category({ { "k", 1000, 20 } }, false);
    CHECK(Contains(FormatRunCategory("kernels", baseline, target, 10),
                   "per_call_ns 100 -> 50 (-50, -50.0%)"));
}

TEST_CASE("Per-call changes keep fractional nanoseconds", "[assistant][compare]")
{
    const RunCategoryFigures baseline = Category({ { "k", 15, 10 } }, false);
    const RunCategoryFigures target   = Category({ { "k", 19, 20 } }, false);
    CHECK(Contains(FormatRunCategory("kernels", baseline, target, 10),
                   "per_call_ns 1.5 -> 0.95 (-0.55, -36.7%)"));
}

TEST_CASE("The row limit is honored and the rest are counted", "[assistant][compare]")
{
    std::vector<RunEventFigures> events;
    for(uint64_t i = 1; i <= 30; ++i)
    {
        events.push_back({ "kernel_" + std::to_string(i), i * 100, i });
    }
    const RunCategoryFigures baseline = Category(events, false);
    for(RunEventFigures& event : events)
    {
        event.total_ns /= 2;
    }
    const RunCategoryFigures target = Category(events, false);
    const std::string        text   = FormatRunCategory("kernels", baseline, target, 5);
    CHECK(Contains(text, "  5. "));
    CHECK_FALSE(Contains(text, "  6. "));
    CHECK(Contains(text, "... 25 more names not shown"));
}

TEST_CASE("A family neither run has says so", "[assistant][compare]")
{
    RunCategoryFigures empty;
    CHECK(FormatRunCategory("memory_ops", empty, empty, 10) ==
          "memory_ops: none in either run\n");
}
