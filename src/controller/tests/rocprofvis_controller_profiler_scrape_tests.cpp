// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// The scrape engine and the rule table, exercised directly. Nothing here
// launches a process, so it runs on every platform.

#include "rocprofvis_controller.h"
#include "profiler/rocprofvis_controller_profiler_scrape.h"
#include "profiler/rocprofvis_controller_profiler_scrape_rules.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace ProfilerScrapeRules = RocProfVis::Controller::ProfilerScrapeRules;

using RocProfVis::Controller::kProfilerScrapeLineMaxBytes;
using RocProfVis::Controller::ProfilerScrapeEngine;
using RocProfVis::Controller::ProfilerScrapeRuleSpec;
using RocProfVis::Controller::ProfilerStageSpec;
using RocProfVis::Controller::resolve_stage_placeholders;
using RocProfVis::Controller::ScrapePolicy;
using RocProfVis::Controller::strip_ansi;

namespace
{

ProfilerScrapeRuleSpec make_rule(char const* key, char const* pattern,
                                 ScrapePolicy policy = ScrapePolicy::kFirstMatch,
                                 uint32_t     group  = 1)
{
    ProfilerScrapeRuleSpec rule;
    rule.key     = key;
    rule.pattern = pattern;
    rule.group   = group;
    rule.policy  = policy;
    return rule;
}

// Most tests care about the status, not about proving the key exists, so fail
// loudly here rather than repeating the two-line unpack everywhere.
rocprofvis_profiler_scrape_status_t status_of(ProfilerScrapeEngine const& engine,
                                             char const*                 key)
{
    rocprofvis_profiler_scrape_status_t status = kRPVProfilerScrapePending;
    REQUIRE(engine.GetStatus(key, status) == kRocProfVisResultSuccess);
    return status;
}

} // namespace

TEST_CASE("strip_ansi removes CSI color sequences", "[profiler][scrape]")
{
    std::string text = "\x1b[33m          WARNING\x1b[0m Created file: myrun.db";
    strip_ansi(text);
    CHECK(text == "          WARNING Created file: myrun.db");
}

TEST_CASE("Scrape engine extracts compute and rocprof-sys fixtures", "[profiler][scrape]")
{
    ProfilerStageSpec stage;
    stage.scrape_rules.push_back(
        make_rule("workload_dir", R"(Output directory:\s*(\S.*?)\s*$)",
                  ScrapePolicy::kFirstMatch));
    stage.scrape_rules.push_back(
        make_rule("analysis_db", R"(Created file:\s*(\S.*?)\s*$)",
                  ScrapePolicy::kFirstMatch));
    stage.scrape_rules.push_back(
        make_rule("trace_db", R"((?:Database:|File:)\s*'?([^\s'"]+\.db)\b)",
                  ScrapePolicy::kLastMatch));

    ProfilerScrapeEngine engine;
    REQUIRE(engine.Compile({stage}) == kRocProfVisResultSuccess);
    engine.BeginStage(0);

    SECTION("plain Created file line")
    {
        engine.Feed("Created file: myrun.db\n");
        engine.EndStage("");
        std::string value;
        REQUIRE(engine.GetValue("analysis_db", value) == kRocProfVisResultSuccess);
        CHECK(value == "myrun.db");
    }

    SECTION("ANSI plus 16-column level prefix")
    {
        engine.Feed("\x1b[33m          WARNING\x1b[0m Created file: myrun.db\n");
        engine.EndStage("");
        std::string value;
        REQUIRE(engine.GetValue("analysis_db", value) == kRocProfVisResultSuccess);
        CHECK(value == "myrun.db");
    }

    SECTION("first Output directory wins")
    {
        engine.Feed("Output directory: /first/wl\n");
        engine.Feed("Output directory: /second/wl\n");
        engine.EndStage("");
        std::string value;
        REQUIRE(engine.GetValue("workload_dir", value) == kRocProfVisResultSuccess);
        CHECK(value == "/first/wl");
    }

    SECTION("last Database/File match wins for trace_db")
    {
        engine.Feed("Opening file: /tmp/old.db\n");
        engine.Feed("Database: '/tmp/new.db'\n");
        engine.EndStage("");
        std::string value;
        REQUIRE(engine.GetValue("trace_db", value) == kRocProfVisResultSuccess);
        CHECK(value == "/tmp/new.db");
    }
}

TEST_CASE("Output that arrives after a stage ends does not revise it", "[profiler][scrape]")
{
    ProfilerStageSpec stage;
    stage.scrape_rules.push_back(make_rule("path", R"(Database:\s*(\S+))"));

    ProfilerScrapeEngine engine;
    REQUIRE(engine.Compile({stage}) == kRocProfVisResultSuccess);
    engine.BeginStage(0);
    engine.EndStage("");
    REQUIRE(status_of(engine, "path") == kRPVProfilerScrapeUnmatched);

    // A pipe still holds bytes after the process that wrote them is gone, so a
    // drain can land after the stage was ended. Matching them would revise a
    // verdict the caller may already have read, and after a cancel it would
    // resolve a stage that was reported as abandoned.
    engine.Feed("Database: /tmp/too-late.db\n");
    CHECK(status_of(engine, "path") == kRPVProfilerScrapeUnmatched);

    // Ending twice settles nothing further either.
    engine.EndStage("");
    CHECK(status_of(engine, "path") == kRPVProfilerScrapeUnmatched);
}

TEST_CASE("First-match vs last-match policy is applied within one rule", "[profiler][scrape]")
{
    ProfilerStageSpec first_stage;
    first_stage.scrape_rules.push_back(
        make_rule("path", R"(File:\s*(\S+))", ScrapePolicy::kFirstMatch));
    ProfilerStageSpec last_stage;
    last_stage.scrape_rules.push_back(
        make_rule("path", R"(File:\s*(\S+))", ScrapePolicy::kLastMatch));

    char const* feed = "File: a.db\nFile: b.db\n";

    ProfilerScrapeEngine first;
    REQUIRE(first.Compile({first_stage}) == kRocProfVisResultSuccess);
    first.BeginStage(0);
    first.Feed(feed);
    first.EndStage("");
    std::string first_value;
    REQUIRE(first.GetValue("path", first_value) == kRocProfVisResultSuccess);
    CHECK(first_value == "a.db");

    ProfilerScrapeEngine last;
    REQUIRE(last.Compile({last_stage}) == kRocProfVisResultSuccess);
    last.BeginStage(0);
    last.Feed(feed);
    last.EndStage("");
    std::string last_value;
    REQUIRE(last.GetValue("path", last_value) == kRocProfVisResultSuccess);
    CHECK(last_value == "b.db");
}

TEST_CASE("Lines longer than the scrape cap are skipped", "[profiler][scrape]")
{
    ProfilerStageSpec stage;
    stage.scrape_rules.push_back(make_rule("path", R"(([^\s]+\.db))"));

    std::string line(kProfilerScrapeLineMaxBytes + 1, 'a');
    line += ".db\n";

    ProfilerScrapeEngine engine;
    REQUIRE(engine.Compile({stage}) == kRocProfVisResultSuccess);
    engine.BeginStage(0);
    engine.Feed(line);
    engine.EndStage("");
    CHECK(status_of(engine, "path") == kRPVProfilerScrapeUnmatched);
    std::string value = "sentinel";
    CHECK(engine.GetValue("path", value) == kRocProfVisResultNotAvailable);
    CHECK(value == "sentinel");
}

TEST_CASE("A carriage return ends a line", "[profiler][scrape]")
{
    // The profilers themselves log whole newline-terminated lines. This is
    // about the target's output on the same stream: an in-place progress bar,
    // as any ML workload might print.
    ProfilerStageSpec stage;
    stage.scrape_rules.push_back(
        make_rule("epoch", R"(epoch\s+(\d+)/\d+)", ScrapePolicy::kLastMatch));

    ProfilerScrapeEngine engine;
    REQUIRE(engine.Compile({stage}) == kRocProfVisResultSuccess);
    engine.BeginStage(0);

    SECTION("no newline anywhere in the output")
    {
        engine.Feed("epoch 1/3 [==    ]\repoch 2/3 [====  ]\repoch 3/3 [======]\r");
        engine.EndStage("");
        std::string value;
        REQUIRE(engine.GetValue("epoch", value) == kRocProfVisResultSuccess);
        CHECK(value == "3");
    }

    SECTION("a CRLF split across two chunks is one line, not two")
    {
        engine.Feed("epoch 2/3 done\r");
        engine.Feed("\nepoch 3/3 done\n");
        engine.EndStage("");
        std::string value;
        REQUIRE(engine.GetValue("epoch", value) == kRocProfVisResultSuccess);
        CHECK(value == "3");
    }

    SECTION("a value arriving one byte at a time still matches")
    {
        char const* output = "epoch 1/9 x\repoch 7/9 x\r";
        for (char const* p = output; *p != '\0'; ++p)
        {
            engine.Feed(std::string(1, *p));
        }
        engine.EndStage("");
        std::string value;
        REQUIRE(engine.GetValue("epoch", value) == kRocProfVisResultSuccess);
        CHECK(value == "7");
    }
}

TEST_CASE("Output with no terminator does not grow without bound", "[profiler][scrape]")
{
    ProfilerStageSpec stage;
    stage.scrape_rules.push_back(make_rule("path", R"(Database:\s*(\S+))"));
    stage.scrape_rules.push_back(make_rule("junk", R"((x{8,}))"));

    ProfilerScrapeEngine engine;
    REQUIRE(engine.Compile({stage}) == kRocProfVisResultSuccess);
    engine.BeginStage(0);

    // A quarter of a megabyte on one unterminated line. This used to sit in the
    // buffer for the length of the run.
    for (int i = 0; i < 256; ++i)
    {
        engine.Feed(std::string(1024, 'x'));
    }
    // Terminating it discards the line instead of matching the fragment that
    // happened to survive, and the engine recovers for the next real line.
    engine.Feed("\nDatabase: /tmp/after-overflow.db\n");
    engine.EndStage("");

    std::string value;
    REQUIRE(engine.GetValue("path", value) == kRocProfVisResultSuccess);
    CHECK(value == "/tmp/after-overflow.db");
    CHECK(status_of(engine, "junk") == kRPVProfilerScrapeUnmatched);
}

TEST_CASE("A skipped line is reported only when a key is also missing", "[profiler][scrape]")
{
    ProfilerStageSpec stage;
    stage.scrape_rules.push_back(make_rule("path", R"(Database:\s*(\S+))"));

    std::string long_line(kProfilerScrapeLineMaxBytes + 1, 'a');
    long_line += "\n";

    ProfilerScrapeEngine engine;
    std::string          console;
    engine.SetDiagnosticSink(&console);
    REQUIRE(engine.Compile({stage}) == kRocProfVisResultSuccess);
    engine.BeginStage(0);

    SECTION("a long line plus a missing key names both")
    {
        engine.Feed(long_line);
        engine.EndStage("");

        REQUIRE(status_of(engine, "path") == kRPVProfilerScrapeUnmatched);
        CHECK(console.find("'path'") != std::string::npos);
        CHECK(console.find("too long to scan") != std::string::npos);
    }

    SECTION("a long line on its own is ordinary and says nothing")
    {
        engine.Feed(long_line);
        engine.Feed("Database: /tmp/x.db\n");
        engine.EndStage("");

        std::string value;
        REQUIRE(engine.GetValue("path", value) == kRocProfVisResultSuccess);
        CHECK(console.empty());
    }

    SECTION("a missing key with nothing skipped has nothing to blame it on")
    {
        engine.Feed("nothing of interest here\n");
        engine.EndStage("");

        REQUIRE(status_of(engine, "path") == kRPVProfilerScrapeUnmatched);
        CHECK(console.empty());
    }
}

TEST_CASE("An unknown key is reported separately from an unmatched one",
          "[profiler][scrape]")
{
    ProfilerStageSpec stage;
    stage.scrape_rules.push_back(make_rule("workload_dir", R"(Output directory:\s*(\S+))"));

    ProfilerScrapeEngine engine;
    REQUIRE(engine.Compile({stage}) == kRocProfVisResultSuccess);
    engine.BeginStage(0);
    engine.Feed("nothing of interest\n");
    engine.EndStage("");

    rocprofvis_profiler_scrape_status_t status = kRPVProfilerScrapeResolved;
    CHECK(engine.GetStatus("workload_dir", status) == kRocProfVisResultSuccess);
    CHECK(status == kRPVProfilerScrapeUnmatched);

    // A key no rule declares is the caller's mistake, not the tool's, and the
    // caller's status variable is left alone.
    rocprofvis_profiler_scrape_status_t untouched = kRPVProfilerScrapeResolved;
    CHECK(engine.GetStatus("typo_dir", untouched) == kRocProfVisResultInvalidArgument);
    CHECK(untouched == kRPVProfilerScrapeResolved);
    CHECK_FALSE(engine.HasKey("typo_dir"));

    CHECK(engine.GetStageStatus(0, "typo_dir", untouched) == kRocProfVisResultInvalidArgument);
    // Right key, wrong stage.
    CHECK(engine.GetStageStatus(1, "workload_dir", untouched) ==
          kRocProfVisResultInvalidArgument);

    std::vector<std::string> argv{"-p", "{stage0.typo_dir}"};
    std::string              error;
    CHECK(resolve_stage_placeholders(argv, engine, error) == kRocProfVisResultInvalidArgument);
    CHECK(error.find("no stage produces") != std::string::npos);
}

TEST_CASE("Expected-value table: scrape wins, fallback, unmatched", "[profiler][scrape]")
{
    ProfilerStageSpec stage;
    stage.scrape_rules.push_back(make_rule("workload_dir", R"(Output directory:\s*(\S+))"));
    stage.expected.emplace_back("workload_dir", "/expected/wl");

    SECTION("scraped value wins over expected")
    {
        ProfilerScrapeEngine engine;
        REQUIRE(engine.Compile({stage}) == kRocProfVisResultSuccess);
        engine.BeginStage(0);
        engine.Feed("Output directory: /scraped/wl\n");
        engine.EndStage("");
        std::string value;
        REQUIRE(engine.GetValue("workload_dir", value) == kRocProfVisResultSuccess);
        CHECK(value == "/scraped/wl");
        CHECK(status_of(engine, "workload_dir") == kRPVProfilerScrapeResolved);
    }

    SECTION("missing scrape falls back to expected")
    {
        ProfilerScrapeEngine engine;
        REQUIRE(engine.Compile({stage}) == kRocProfVisResultSuccess);
        engine.BeginStage(0);
        engine.Feed("unrelated line\n");
        engine.EndStage("");
        std::string value;
        REQUIRE(engine.GetValue("workload_dir", value) == kRocProfVisResultSuccess);
        CHECK(value == "/expected/wl");
        CHECK(status_of(engine, "workload_dir") == kRPVProfilerScrapeResolved);
    }

    SECTION("neither scrape nor expected stays unmatched")
    {
        ProfilerStageSpec bare;
        bare.scrape_rules.push_back(make_rule("workload_dir", R"(Output directory:\s*(\S+))"));
        ProfilerScrapeEngine engine;
        REQUIRE(engine.Compile({bare}) == kRocProfVisResultSuccess);
        engine.BeginStage(0);
        engine.Feed("unrelated line\n");
        engine.EndStage("");
        std::string value = "sentinel";
        CHECK(engine.GetValue("workload_dir", value) == kRocProfVisResultNotAvailable);
        CHECK(value == "sentinel");
        CHECK(status_of(engine, "workload_dir") == kRPVProfilerScrapeUnmatched);
    }
}

TEST_CASE("Relative scraped paths resolve against the stage working directory",
          "[profiler][scrape]")
{
    ProfilerStageSpec stage;
    stage.working_directory = "/tmp/run dir";
    stage.scrape_rules.push_back(make_rule("analysis_db", R"(Created file:\s*(\S+))"));

    ProfilerScrapeEngine engine;
    REQUIRE(engine.Compile({stage}) == kRocProfVisResultSuccess);
    engine.BeginStage(0);
    engine.Feed("Created file: myrun.db\n");
    engine.EndStage(stage.working_directory);

    std::string value;
    REQUIRE(engine.GetValue("analysis_db", value) == kRocProfVisResultSuccess);
    CHECK(std::filesystem::path(value) == std::filesystem::path("/tmp/run dir") / "myrun.db");
}

TEST_CASE("Pending scrape leaves the caller's value untouched", "[profiler][scrape]")
{
    ProfilerStageSpec stage;
    stage.scrape_rules.push_back(make_rule("workload_dir", R"(Output directory:\s*(\S+))"));

    ProfilerScrapeEngine engine;
    REQUIRE(engine.Compile({stage}) == kRocProfVisResultSuccess);
    engine.BeginStage(0);

    std::string value = "sentinel";
    CHECK(engine.GetValue("workload_dir", value) == kRocProfVisResultPending);
    CHECK(value == "sentinel");
    CHECK(status_of(engine, "workload_dir") == kRPVProfilerScrapePending);
}

TEST_CASE("SkipRemainingFrom marks later keys StageSkipped", "[profiler][scrape]")
{
    ProfilerStageSpec stage0;
    stage0.scrape_rules.push_back(make_rule("a", R"(A:\s*(\S+))"));
    ProfilerStageSpec stage1;
    stage1.scrape_rules.push_back(make_rule("b", R"(B:\s*(\S+))"));

    ProfilerScrapeEngine engine;
    REQUIRE(engine.Compile({stage0, stage1}) == kRocProfVisResultSuccess);
    engine.BeginStage(0);
    engine.EndStage("");
    engine.SkipRemainingFrom(1);

    CHECK(status_of(engine, "a") == kRPVProfilerScrapeUnmatched);
    CHECK(status_of(engine, "b") == kRPVProfilerScrapeStageSkipped);
    std::string value = "sentinel";
    CHECK(engine.GetValue("b", value) == kRocProfVisResultNotAvailable);
    CHECK(value == "sentinel");
}

TEST_CASE("Placeholder substitution and unresolved references", "[profiler][scrape]")
{
    ProfilerStageSpec stage0;
    stage0.scrape_rules.push_back(make_rule("workload_dir", R"(Output directory:\s*(\S+))"));
    ProfilerStageSpec stage1;

    ProfilerScrapeEngine engine;
    REQUIRE(engine.Compile({stage0, stage1}) == kRocProfVisResultSuccess);
    engine.BeginStage(0);
    engine.Feed("Output directory: /tmp/wl\n");
    engine.EndStage("");

    std::vector<std::string> argv{"analyze", "-p", "{stage0.workload_dir}", "kept {stageX.x}"};
    std::string              error;
    REQUIRE(resolve_stage_placeholders(argv, engine, error) == kRocProfVisResultSuccess);
    CHECK(argv[2] == "/tmp/wl");
    CHECK(argv[3] == "kept {stageX.x}");

    ProfilerScrapeEngine empty;
    REQUIRE(empty.Compile({stage0, stage1}) == kRocProfVisResultSuccess);
    empty.BeginStage(0);
    empty.EndStage("");
    std::vector<std::string> bad{"-p", "{stage0.workload_dir}"};
    CHECK(resolve_stage_placeholders(bad, empty, error) == kRocProfVisResultInvalidArgument);
    CHECK(error.find("did not report") != std::string::npos);
    CHECK(bad[1] == "{stage0.workload_dir}");
}

TEST_CASE("Every shipped rule compiles and declares a reachable group", "[profiler][rules]")
{
    // The rule table is the only source of patterns, so a typo in it is a
    // launch failure for a real user. Compile the whole table here instead.
    for (uint32_t tool = 1; tool < __kRPVProfilerToolLast; ++tool)
    {
        for (uint32_t op = 0; op < __kRPVProfilerOperationLast; ++op)
        {
            ProfilerStageSpec stage;
            stage.tool      = static_cast<rocprofvis_profiler_tool_t>(tool);
            stage.operation = static_cast<rocprofvis_profiler_operation_t>(op);

            std::string artifact_key;
            ProfilerScrapeRules::Apply(stage, artifact_key);
            if (stage.scrape_rules.empty())
            {
                continue;
            }

            ProfilerScrapeEngine engine;
            INFO("tool " << tool << " operation " << op);
            // Compile validates both the pattern and that the requested group
            // exists in it.
            CHECK(engine.Compile({stage}) == kRocProfVisResultSuccess);
            CHECK_FALSE(artifact_key.empty());
            CHECK(engine.HasKey(artifact_key));
        }
    }
}

TEST_CASE("Rules are selected by tool, operation, and version", "[profiler][rules]")
{
    SECTION("compute capture and analyze get different keys")
    {
        ProfilerStageSpec capture;
        capture.tool      = kRPVProfilerToolRocprofCompute;
        capture.operation = kRPVProfilerOperationCapture;
        std::string capture_artifact;
        ProfilerScrapeRules::Apply(capture, capture_artifact);
        CHECK(capture_artifact == ProfilerScrapeRules::kKeyWorkloadDir);

        ProfilerStageSpec analyze;
        analyze.tool      = kRPVProfilerToolRocprofCompute;
        analyze.operation = kRPVProfilerOperationAnalyze;
        std::string analyze_artifact;
        ProfilerScrapeRules::Apply(analyze, analyze_artifact);
        CHECK(analyze_artifact == ProfilerScrapeRules::kKeyAnalysisDb);
    }

    SECTION("a tool with no rules is not an error")
    {
        ProfilerStageSpec probe;
        probe.tool = kRPVProfilerToolRocprofSysAvail;
        std::string artifact;
        ProfilerScrapeRules::Apply(probe, artifact);
        CHECK(probe.scrape_rules.empty());
        CHECK(artifact.empty());
    }

    SECTION("an unknown version falls back to the base rules")
    {
        ProfilerStageSpec base;
        base.tool = kRPVProfilerToolRocprofSysRun;
        std::string base_artifact;
        ProfilerScrapeRules::Apply(base, base_artifact);

        ProfilerStageSpec odd;
        odd.tool         = kRPVProfilerToolRocprofSysRun;
        odd.tool_version = "not-a-version";
        std::string odd_artifact;
        ProfilerScrapeRules::Apply(odd, odd_artifact);

        CHECK(odd.scrape_rules.size() == base.scrape_rules.size());
        CHECK(odd_artifact == base_artifact);
    }

    SECTION("Apply overwrites any rules already on the stage")
    {
        ProfilerStageSpec stage;
        stage.tool = kRPVProfilerToolRocprofSysRun;
        stage.scrape_rules.push_back(make_rule("smuggled", R"((.*))"));
        std::string artifact;
        ProfilerScrapeRules::Apply(stage, artifact);
        for (ProfilerScrapeRuleSpec const& rule : stage.scrape_rules)
        {
            CHECK(rule.key != "smuggled");
        }
    }
}

TEST_CASE("ParseVersion accepts the shapes tools actually print", "[profiler][rules]")
{
    uint32_t major = 99;
    uint32_t minor = 99;
    uint32_t patch = 99;

    REQUIRE(ProfilerScrapeRules::ParseVersion("3.1.0", major, minor, patch));
    CHECK(major == 3);
    CHECK(minor == 1);
    CHECK(patch == 0);

    REQUIRE(ProfilerScrapeRules::ParseVersion("4.0.0-rc1", major, minor, patch));
    CHECK(major == 4);
    CHECK(minor == 0);
    CHECK(patch == 0);

    REQUIRE(ProfilerScrapeRules::ParseVersion("2.5", major, minor, patch));
    CHECK(major == 2);
    CHECK(minor == 5);
    CHECK(patch == 0);

    CHECK_FALSE(ProfilerScrapeRules::ParseVersion("", major, minor, patch));
    CHECK_FALSE(ProfilerScrapeRules::ParseVersion("v3.1.0", major, minor, patch));
    CHECK_FALSE(ProfilerScrapeRules::ParseVersion("99999999999", major, minor, patch));
}

TEST_CASE("The shipped rocprof-sys rules read a real database line", "[profiler][rules]")
{
    ProfilerStageSpec stage;
    stage.tool = kRPVProfilerToolRocprofSysRun;
    std::string artifact_key;
    ProfilerScrapeRules::Apply(stage, artifact_key);

    ProfilerScrapeEngine engine;
    REQUIRE(engine.Compile({stage}) == kRocProfVisResultSuccess);
    engine.BeginStage(0);
    engine.Feed("[rocprof-sys] Opening /tmp/decoy.db\n");
    engine.Feed("[rocprof-sys] Database: '/tmp/real.db'\n");
    engine.EndStage("");

    std::string value;
    REQUIRE(engine.GetValue(artifact_key, value) == kRocProfVisResultSuccess);
    // The labelled rule is listed first, so it wins over the bare sweep even
    // though the sweep also matched a later line.
    CHECK(value == "/tmp/real.db");
}

TEST_CASE("The shipped rocprof-sys rules prefer Database: over a later File:",
          "[profiler][rules]")
{
    ProfilerStageSpec stage;
    stage.tool = kRPVProfilerToolRocprofSysRun;
    std::string artifact_key;
    ProfilerScrapeRules::Apply(stage, artifact_key);

    ProfilerScrapeEngine engine;
    REQUIRE(engine.Compile({stage}) == kRocProfVisResultSuccess);
    engine.BeginStage(0);
    engine.Feed("[database][info] Database: /run/rocpd-1234-0.db\n");
    engine.Feed("|  RocPD database  |  File: /run/summary-copy.db  |\n");
    engine.EndStage("");

    std::string value;
    REQUIRE(engine.GetValue(artifact_key, value) == kRocProfVisResultSuccess);
    // Same preference order the View's ParseTraceOutputPath documents. The two
    // read the same log, so a tool that prints both labels must not lead them
    // to different files.
    CHECK(value == "/run/rocpd-1234-0.db");
}

TEST_CASE("The shipped compute rules read progress and the output directory",
          "[profiler][rules]")
{
    ProfilerStageSpec stage;
    stage.tool      = kRPVProfilerToolRocprofCompute;
    stage.operation = kRPVProfilerOperationCapture;
    std::string artifact_key;
    ProfilerScrapeRules::Apply(stage, artifact_key);

    ProfilerScrapeEngine engine;
    REQUIRE(engine.Compile({stage}) == kRocProfVisResultSuccess);
    engine.BeginStage(0);
    engine.Feed("Output directory: /tmp/wl\n");
    engine.Feed("\x1b[36m[Run 1/3]\x1b[0m pass\n");
    engine.Feed("\x1b[36m[Run 3/3]\x1b[0m pass\n");
    engine.EndStage("");

    std::string value;
    REQUIRE(engine.GetValue(ProfilerScrapeRules::kKeyWorkloadDir, value) ==
            kRocProfVisResultSuccess);
    CHECK(value == "/tmp/wl");
    REQUIRE(engine.GetValue(ProfilerScrapeRules::kKeyRunIndex, value) ==
            kRocProfVisResultSuccess);
    CHECK(value == "3");
    REQUIRE(engine.GetValue(ProfilerScrapeRules::kKeyRunTotal, value) ==
            kRocProfVisResultSuccess);
    CHECK(value == "3");
}
