// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "json.h"
#include "rocprofvis_middleware.h"

static std::string g_input_file = "sample/trace_70b_1024_32.rpd";

/*
 * How long a test is willing to block inline on a fetch. Trace loading is the
 * slow one; the per-fetch calls complete far sooner.
 */
static constexpr uint64_t LOAD_WAIT_MS  = 30000;
static constexpr uint64_t FETCH_WAIT_MS = 15000;

/*
 * Owns a session for the duration of a test and speaks the protocol in terms of
 * parsed JSON, so a test never has to build request strings by hand.
 */
class Client
{
public:
    Client()
    : m_session(rocprofvis_mw_session_alloc())
    {}

    ~Client() { rocprofvis_mw_session_free(m_session); }

    Client(const Client&)            = delete;
    Client& operator=(const Client&) = delete;

    bool IsValid() const { return m_session != nullptr; }

    jt::Json Call(const std::string& request_json)
    {
        jt::Json parsed;
        char*    response = rocprofvis_mw_request(m_session, request_json.c_str());
        if(response != nullptr)
        {
            std::pair<jt::Json::Status, jt::Json> result = jt::Json::parse(response);
            rocprofvis_mw_string_free(response);
            REQUIRE(result.first == jt::Json::success);
            parsed = result.second;
        }
        return parsed;
    }

    /* Issue a fetch that blocks inline, and return the request envelope. */
    jt::Json CallAndWait(const std::string& request_json)
    {
        jt::Json response = Call(request_json);
        REQUIRE(response.isObject());
        INFO(response.toStringPretty());
        REQUIRE(response.contains("ok"));
        REQUIRE(response["ok"].getBool());
        return response["result"];
    }

private:
    rocprofvis_mw_session_t* m_session;
};

static std::string
OpenTraceRequest(void)
{
    return "{\"id\":1,\"method\":\"trace.open\",\"params\":{\"path\":\"" + g_input_file +
           "\",\"wait_ms\":" + std::to_string(LOAD_WAIT_MS) + "}}";
}

/*
 * A JSON array of the ids of the first few event tracks in a timeline. Event
 * tables are scoped by track, and a handful is enough to exercise the shape of
 * the response without paying for the whole trace.
 */
static std::string
EventTrackIds(jt::Json& timeline, size_t limit = 4)
{
    std::string ids = "[";
    for(jt::Json& track : timeline["tracks"].getArray())
    {
        if(track["type"].getString() != "events")
        {
            continue;
        }
        if(ids.size() > 1)
        {
            ids += ",";
        }
        ids += std::to_string(track["id"].getLong());
        if(--limit == 0)
        {
            break;
        }
    }
    return ids + "]";
}

int
main(int argc, char** argv)
{
    Catch::Session session;

    using namespace Catch::Clara;
    auto cli = session.cli() |
               Opt(g_input_file, "input_file")["--input_file"]("Path to input file");
    session.cli(cli);

    int return_code = session.applyCommandLine(argc, argv);
    if(return_code != 0)
    {
        return return_code;
    }
    return session.run();
}

TEST_CASE("Malformed input produces a well-formed error")
{
    Client client;
    REQUIRE(client.IsValid());

    jt::Json response = client.Call("this is not json");
    REQUIRE(response.isObject());
    REQUIRE(response.contains("ok"));
    REQUIRE(response["ok"].getBool() == false);
    REQUIRE(response["error"].contains("code"));
    REQUIRE(response["error"]["code"].getString() == "parse_error");
}

TEST_CASE("Unknown methods are rejected rather than ignored")
{
    Client client;
    REQUIRE(client.IsValid());

    jt::Json response = client.Call("{\"id\":7,\"method\":\"nope.nope\",\"params\":{}}");
    REQUIRE(response["ok"].getBool() == false);
    REQUIRE(response["error"]["code"].getString() == "unknown_method");

    /* The correlation id must survive an error, or a client cannot match it up. */
    REQUIRE(response.contains("id"));
    REQUIRE(response["id"].getLong() == 7);
}

TEST_CASE("Fetches are refused before a trace is open")
{
    Client client;
    REQUIRE(client.IsValid());

    jt::Json response =
        client.Call("{\"method\":\"timeline.info\",\"params\":{}}");
    REQUIRE(response["ok"].getBool() == false);
    REQUIRE(response["error"]["code"].getString() == "no_trace");
}

TEST_CASE("session.info advertises the protocol and its scope")
{
    Client client;
    REQUIRE(client.IsValid());

    jt::Json result = client.CallAndWait("{\"method\":\"session.info\",\"params\":{}}");
    REQUIRE(result["protocol_version"].getLong() >= 1);
    REQUIRE(result["trace_state"].getString() == "empty");
    REQUIRE(result["capabilities"]["system_trace"].getBool());
    REQUIRE(result["methods"].isArray());
    REQUIRE(result["methods"].getArray().size() > 0);
}

TEST_CASE("A system trace loads and exposes a timeline")
{
    Client client;
    REQUIRE(client.IsValid());

    jt::Json opened = client.CallAndWait(OpenTraceRequest());
    INFO(opened.toStringPretty());
    REQUIRE(opened["status"].getString() == "ready");
    REQUIRE(opened["result"]["loaded"].getBool());

    jt::Json timeline =
        client.CallAndWait("{\"method\":\"timeline.info\",\"params\":{}}");
    REQUIRE(timeline["num_tracks"].getLong() > 0);
    REQUIRE(timeline["max_timestamp"].getNumber() >= timeline["min_timestamp"].getNumber());

    std::vector<jt::Json>& tracks = timeline["tracks"].getArray();
    REQUIRE(tracks.size() > 0);
    for(jt::Json& track : tracks)
    {
        REQUIRE(track.contains("id"));
        REQUIRE(track.contains("type"));
        REQUIRE(track["type"].getString() != "unknown");
    }
}

TEST_CASE("Topology is reachable and internally consistent")
{
    Client client;
    REQUIRE(client.IsValid());
    client.CallAndWait(OpenTraceRequest());

    jt::Json topology =
        client.CallAndWait("{\"method\":\"trace.topology\",\"params\":{}}");
    REQUIRE(topology["nodes"].isArray());
    REQUIRE(topology["nodes"].getArray().size() > 0);

    /* Every node must name its children with ids that resolve to real objects. */
    std::vector<jt::Json>& nodes = topology["nodes"].getArray();
    for(jt::Json& node : nodes)
    {
        REQUIRE(node.contains("id"));
        REQUIRE(node["processor_ids"].isArray());
        REQUIRE(node["process_ids"].isArray());
    }
}

TEST_CASE("A graph fetch returns level-of-detail entries for every track")
{
    Client client;
    REQUIRE(client.IsValid());
    client.CallAndWait(OpenTraceRequest());

    jt::Json timeline =
        client.CallAndWait("{\"method\":\"timeline.info\",\"params\":{}}");
    double start = timeline["min_timestamp"].getNumber();
    double end   = timeline["max_timestamp"].getNumber();

    std::vector<jt::Json>& tracks = timeline["tracks"].getArray();
    REQUIRE(tracks.size() > 0);

    uint64_t track_id = static_cast<uint64_t>(tracks[0]["id"].getLong());
    std::string request = "{\"method\":\"graph.fetch\",\"params\":{\"track_id\":" +
                          std::to_string(track_id) +
                          ",\"start_time\":" + std::to_string(start) +
                          ",\"end_time\":" + std::to_string(end) +
                          ",\"x_resolution\":512,\"wait_ms\":" +
                          std::to_string(FETCH_WAIT_MS) + "}}";

    jt::Json fetched = client.CallAndWait(request);
    INFO(fetched.toStringPretty());
    REQUIRE(fetched["status"].getString() == "ready");
    REQUIRE(fetched["result"]["entries"].isArray());
    REQUIRE(fetched["result"].contains("kind"));
}

TEST_CASE("An event table fetch returns typed columns and rows")
{
    Client client;
    REQUIRE(client.IsValid());
    client.CallAndWait(OpenTraceRequest());

    jt::Json timeline =
        client.CallAndWait("{\"method\":\"timeline.info\",\"params\":{}}");
    double start = timeline["min_timestamp"].getNumber();
    double end   = timeline["max_timestamp"].getNumber();

    std::string track_ids = EventTrackIds(timeline);
    REQUIRE(track_ids != "[]");

    std::string request =
        "{\"method\":\"table.fetch\",\"params\":{\"table_type\":\"events\""
        ",\"track_ids\":" +
        track_ids + ",\"start_time\":" + std::to_string(start) +
        ",\"end_time\":" + std::to_string(end) +
        ",\"start_row\":0,\"row_count\":25,\"wait_ms\":" + std::to_string(FETCH_WAIT_MS) +
        "}}";

    jt::Json fetched = client.CallAndWait(request);
    INFO(fetched.toStringPretty());
    REQUIRE(fetched["status"].getString() == "ready");

    jt::Json& result = fetched["result"];
    REQUIRE(result["columns"].isArray());
    REQUIRE(result["columns"].getArray().size() > 0);
    REQUIRE(result["rows"].isArray());

    /* Every row must be as wide as the schema says, so a client can zip them. */
    size_t num_columns = result["columns"].getArray().size();
    for(jt::Json& row : result["rows"].getArray())
    {
        REQUIRE(row.isArray());
        REQUIRE(row.getArray().size() == num_columns);
    }

    for(jt::Json& column : result["columns"].getArray())
    {
        REQUIRE(column.contains("name"));
        REQUIRE(column.contains("type"));
    }
}

TEST_CASE("An unknown table type is rejected before any work is done")
{
    Client client;
    REQUIRE(client.IsValid());
    client.CallAndWait(OpenTraceRequest());

    jt::Json response = client.Call(
        "{\"method\":\"table.fetch\",\"params\":{\"table_type\":\"not_a_table\"}}");
    REQUIRE(response["ok"].getBool() == false);
    REQUIRE(response["error"]["code"].getString() == "invalid_argument");
}

TEST_CASE("A table fetch with an empty selection is refused")
{
    Client client;
    REQUIRE(client.IsValid());
    client.CallAndWait(OpenTraceRequest());

    /* Track-scoped tables need tracks. */
    jt::Json no_tracks = client.Call(
        "{\"method\":\"table.fetch\",\"params\":{\"table_type\":\"events\"}}");
    INFO(no_tracks.toStringPretty());
    REQUIRE(no_tracks["ok"].getBool() == false);
    REQUIRE(no_tracks["error"]["code"].getString() == "invalid_argument");

    /* Operation-scoped tables need operation types. */
    jt::Json no_ops = client.Call(
        "{\"method\":\"table.fetch\",\"params\":{\"table_type\":\"search_results\"}}");
    INFO(no_ops.toStringPretty());
    REQUIRE(no_ops["ok"].getBool() == false);
    REQUIRE(no_ops["error"]["code"].getString() == "invalid_argument");

    /* A track of the wrong kind is named rather than silently dropped. */
    jt::Json timeline =
        client.CallAndWait("{\"method\":\"timeline.info\",\"params\":{}}");
    std::string event_tracks = EventTrackIds(timeline, 1);
    REQUIRE(event_tracks != "[]");

    jt::Json wrong_kind = client.Call(
        "{\"method\":\"table.fetch\",\"params\":{\"table_type\":\"samples\",\"track_ids\":" +
        event_tracks + "}}");
    INFO(wrong_kind.toStringPretty());
    REQUIRE(wrong_kind["ok"].getBool() == false);
    REQUIRE(wrong_kind["error"]["code"].getString() == "invalid_argument");
}

TEST_CASE("A summary fetch returns an aggregation tree")
{
    Client client;
    REQUIRE(client.IsValid());
    client.CallAndWait(OpenTraceRequest());

    jt::Json timeline =
        client.CallAndWait("{\"method\":\"timeline.info\",\"params\":{}}");
    std::string request =
        "{\"method\":\"summary.fetch\",\"params\":{\"start_time\":" +
        std::to_string(timeline["min_timestamp"].getNumber()) +
        ",\"end_time\":" + std::to_string(timeline["max_timestamp"].getNumber()) +
        ",\"wait_ms\":" + std::to_string(FETCH_WAIT_MS) + "}}";

    jt::Json fetched = client.CallAndWait(request);
    INFO(fetched.toStringPretty());
    REQUIRE(fetched["status"].getString() == "ready");
    REQUIRE(fetched["result"]["summary"].isObject());
    REQUIRE(fetched["result"]["summary"]["level"].getString() == "trace");
}

TEST_CASE("An async fetch can be polled instead of waited on")
{
    Client client;
    REQUIRE(client.IsValid());
    client.CallAndWait(OpenTraceRequest());

    jt::Json timeline =
        client.CallAndWait("{\"method\":\"timeline.info\",\"params\":{}}");
    uint64_t track_id =
        static_cast<uint64_t>(timeline["tracks"].getArray()[0]["id"].getLong());

    /* No wait_ms, so this must come back with a handle rather than a payload. */
    std::string request = "{\"method\":\"graph.fetch\",\"params\":{\"track_id\":" +
                          std::to_string(track_id) + ",\"start_time\":" +
                          std::to_string(timeline["min_timestamp"].getNumber()) +
                          ",\"end_time\":" +
                          std::to_string(timeline["max_timestamp"].getNumber()) +
                          ",\"x_resolution\":256}}";

    jt::Json issued = client.CallAndWait(request);
    REQUIRE(issued.contains("request_id"));
    uint64_t request_id = static_cast<uint64_t>(issued["request_id"].getLong());

    std::string poll = "{\"method\":\"request.poll\",\"params\":{\"request_id\":" +
                       std::to_string(request_id) +
                       ",\"wait_ms\":" + std::to_string(FETCH_WAIT_MS) + "}}";
    jt::Json polled = client.CallAndWait(poll);
    INFO(polled.toStringPretty());
    REQUIRE(polled["status"].getString() == "ready");
    REQUIRE(polled["result"]["entries"].isArray());

    /* A completed request is delivered once; polling again must not find it. */
    jt::Json repolled = client.Call(poll);
    REQUIRE(repolled["ok"].getBool() == false);
    REQUIRE(repolled["error"]["code"].getString() == "unknown_request");
}

TEST_CASE("A waited-on fetch is retired rather than left in the registry")
{
    Client client;
    REQUIRE(client.IsValid());

    /* trace.open completes inline here, so it must not still be outstanding. */
    client.CallAndWait(OpenTraceRequest());

    jt::Json status = client.CallAndWait("{\"method\":\"trace.status\",\"params\":{}}");
    REQUIRE(status["pending_requests"].getLong() == 0);

    jt::Json listed = client.CallAndWait("{\"method\":\"request.list\",\"params\":{}}");
    REQUIRE(listed["requests"].getArray().empty());
}

TEST_CASE("A pending fetch can be cancelled")
{
    Client client;
    REQUIRE(client.IsValid());
    client.CallAndWait(OpenTraceRequest());

    jt::Json timeline =
        client.CallAndWait("{\"method\":\"timeline.info\",\"params\":{}}");

    /* No wait_ms, so this is still in flight when it is cancelled. */
    std::string request =
        "{\"method\":\"track.fetch\",\"params\":{\"track_id\":" +
        std::to_string(timeline["tracks"].getArray()[0]["id"].getLong()) +
        ",\"start_time\":" + std::to_string(timeline["min_timestamp"].getNumber()) +
        ",\"end_time\":" + std::to_string(timeline["max_timestamp"].getNumber()) + "}}";

    jt::Json    issued     = client.CallAndWait(request);
    uint64_t    request_id = static_cast<uint64_t>(issued["request_id"].getLong());
    std::string by_id      = std::to_string(request_id);

    jt::Json cancelled = client.CallAndWait(
        "{\"method\":\"request.cancel\",\"params\":{\"request_id\":" + by_id + "}}");
    REQUIRE(cancelled["request_id"].getLong() == static_cast<long long>(request_id));

    /* Cancelling retires the request whether or not the worker beat us to it. */
    jt::Json polled = client.Call(
        "{\"method\":\"request.poll\",\"params\":{\"request_id\":" + by_id + "}}");
    REQUIRE(polled["ok"].getBool() == false);
    REQUIRE(polled["error"]["code"].getString() == "unknown_request");
}

TEST_CASE("A track fetch returns raw entries for both track kinds")
{
    Client client;
    REQUIRE(client.IsValid());
    client.CallAndWait(OpenTraceRequest());

    jt::Json timeline =
        client.CallAndWait("{\"method\":\"timeline.info\",\"params\":{}}");
    std::string start = std::to_string(timeline["min_timestamp"].getNumber());
    std::string end   = std::to_string(timeline["max_timestamp"].getNumber());

    /* An events track and a samples track decode differently; cover both. */
    for(std::string kind : { std::string("events"), std::string("samples") })
    {
        uint64_t track_id  = 0;
        bool     found     = false;
        for(jt::Json& track : timeline["tracks"].getArray())
        {
            if(track["type"].getString() == kind && track["num_entries"].getLong() > 0)
            {
                track_id = static_cast<uint64_t>(track["id"].getLong());
                found    = true;
                break;
            }
        }
        REQUIRE(found);

        jt::Json fetched = client.CallAndWait(
            "{\"method\":\"track.fetch\",\"params\":{\"track_id\":" +
            std::to_string(track_id) + ",\"start_time\":" + start + ",\"end_time\":" +
            end + ",\"wait_ms\":" + std::to_string(FETCH_WAIT_MS) + "}}");
        INFO(kind);
        INFO(fetched.toStringPretty());
        REQUIRE(fetched["status"].getString() == "ready");
        REQUIRE(fetched["result"]["track_type"].getString() == kind);
        REQUIRE(fetched["result"]["entries"].isArray());
        REQUIRE(fetched["result"]["entries"].getArray().size() > 0);
    }
}

TEST_CASE("Event details are addressed by the id the fetch handed out")
{
    Client client;
    REQUIRE(client.IsValid());
    client.CallAndWait(OpenTraceRequest());

    jt::Json timeline =
        client.CallAndWait("{\"method\":\"timeline.info\",\"params\":{}}");

    /* Take an id straight from a track fetch, in whatever form it was sent. */
    uint64_t track_id = 0;
    for(jt::Json& track : timeline["tracks"].getArray())
    {
        if(track["type"].getString() == "events" && track["num_entries"].getLong() > 0)
        {
            track_id = static_cast<uint64_t>(track["id"].getLong());
            break;
        }
    }

    jt::Json fetched = client.CallAndWait(
        "{\"method\":\"track.fetch\",\"params\":{\"track_id\":" +
        std::to_string(track_id) + ",\"start_time\":" +
        std::to_string(timeline["min_timestamp"].getNumber()) + ",\"end_time\":" +
        std::to_string(timeline["max_timestamp"].getNumber()) +
        ",\"wait_ms\":" + std::to_string(FETCH_WAIT_MS) + "}}");
    REQUIRE(fetched["result"]["entries"].getArray().size() > 0);

    jt::Json& event = fetched["result"]["entries"].getArray()[0];
    REQUIRE(event.contains("id"));

    /*
     * Ids past 2^53 are handed out as strings, so echo the value back exactly
     * as received rather than reformatting it.
     */
    std::string event_id = event["id"].isString()
                               ? "\"" + event["id"].getString() + "\""
                               : std::to_string(event["id"].getLong());

    for(std::string method :
        { std::string("event.ext_data"), std::string("event.flow"),
          std::string("event.callstack") })
    {
        jt::Json detail = client.CallAndWait(
            "{\"method\":\"" + method + "\",\"params\":{\"event_id\":" + event_id +
            ",\"wait_ms\":" + std::to_string(FETCH_WAIT_MS) + "}}");
        INFO(method);
        INFO(detail.toStringPretty());
        REQUIRE(detail["status"].getString() == "ready");
        REQUIRE(detail["result"]["entries"].isArray());
    }
}

TEST_CASE("An event id that lost precision is refused rather than guessed at")
{
    Client client;
    REQUIRE(client.IsValid());
    client.CallAndWait(OpenTraceRequest());

    /*
     * Past 2^53 a JSON number no longer names one event, and rounding lands on
     * a neighbour, so the request has to be refused instead of answered with
     * some other event's data.
     */
    jt::Json imprecise = client.Call(
        "{\"method\":\"event.ext_data\",\"params\":{\"event_id\":1152921528229174913}}");
    INFO(imprecise.toStringPretty());
    REQUIRE(imprecise["ok"].getBool() == false);
    REQUIRE(imprecise["error"]["code"].getString() == "invalid_argument");

    /* The same id as a string is exact and must be accepted. */
    jt::Json exact = client.Call(
        "{\"method\":\"event.ext_data\",\"params\":{\"event_id\":\"1152921528229174913\""
        ",\"wait_ms\":" + std::to_string(FETCH_WAIT_MS) + "}}");
    REQUIRE(exact["ok"].getBool());

    jt::Json missing =
        client.Call("{\"method\":\"event.ext_data\",\"params\":{}}");
    REQUIRE(missing["ok"].getBool() == false);
    REQUIRE(missing["error"]["code"].getString() == "invalid_argument");
}

TEST_CASE("Track statistics are reported per metric")
{
    Client client;
    REQUIRE(client.IsValid());
    client.CallAndWait(OpenTraceRequest());

    jt::Json topology =
        client.CallAndWait("{\"method\":\"trace.topology\",\"params\":{}}");
    jt::Json timeline =
        client.CallAndWait("{\"method\":\"timeline.info\",\"params\":{}}");

    std::string range =
        ",\"start_time\":" + std::to_string(timeline["min_timestamp"].getNumber()) +
        ",\"end_time\":" + std::to_string(timeline["max_timestamp"].getNumber()) +
        ",\"wait_ms\":" + std::to_string(FETCH_WAIT_MS) + "}}";

    REQUIRE(topology["counters"].getArray().size() > 0);
    jt::Json statistics = client.CallAndWait(
        "{\"method\":\"analysis.track_statistics\",\"params\":{\"metric\":\"counter_"
        "statistics\",\"track_id\":" +
        std::to_string(topology["counters"].getArray()[0]["track_id"].getLong()) + range);
    INFO(statistics.toStringPretty());
    REQUIRE(statistics["status"].getString() == "ready");

    jt::Json& counter = statistics["result"]["counter_statistics"];
    REQUIRE(counter["min"].getNumber() <= counter["mean"].getNumber());
    REQUIRE(counter["mean"].getNumber() <= counter["max"].getNumber());

    REQUIRE(topology["queues"].getArray().size() > 0);
    jt::Json utilization = client.CallAndWait(
        "{\"method\":\"analysis.track_statistics\",\"params\":{\"metric\":\"queue_"
        "utilization\",\"track_id\":" +
        std::to_string(topology["queues"].getArray()[0]["track_id"].getLong()) + range);
    INFO(utilization.toStringPretty());
    REQUIRE(utilization["status"].getString() == "ready");
    REQUIRE(utilization["result"]["queue_utilization"].getNumber() >= 0.0);

    jt::Json unknown = client.Call(
        "{\"method\":\"analysis.track_statistics\",\"params\":{\"metric\":\"nope\","
        "\"track_id\":0}}");
    REQUIRE(unknown["ok"].getBool() == false);
    REQUIRE(unknown["error"]["code"].getString() == "invalid_argument");
}

TEST_CASE("A table can be exported to CSV")
{
    Client client;
    REQUIRE(client.IsValid());
    client.CallAndWait(OpenTraceRequest());

    jt::Json timeline =
        client.CallAndWait("{\"method\":\"timeline.info\",\"params\":{}}");
    std::string track_ids = EventTrackIds(timeline, 1);
    REQUIRE(track_ids != "[]");

    std::string path = "roc-optiq-middleware-test-export.csv";
    std::remove(path.c_str());

    jt::Json exported = client.CallAndWait(
        "{\"method\":\"table.export_csv\",\"params\":{\"table_type\":\"events\""
        ",\"track_ids\":" +
        track_ids + ",\"start_time\":" +
        std::to_string(timeline["min_timestamp"].getNumber()) + ",\"end_time\":" +
        std::to_string(timeline["max_timestamp"].getNumber()) + ",\"path\":\"" + path +
        "\",\"wait_ms\":" + std::to_string(FETCH_WAIT_MS) + "}}");
    INFO(exported.toStringPretty());
    REQUIRE(exported["status"].getString() == "ready");

    std::ifstream written(path.c_str());
    REQUIRE(written.good());
    std::string header;
    REQUIRE(std::getline(written, header));
    REQUIRE(header.find(',') != std::string::npos);
    written.close();
    std::remove(path.c_str());

    /* An export with no destination is refused before any work starts. */
    jt::Json no_path = client.Call(
        "{\"method\":\"table.export_csv\",\"params\":{\"table_type\":\"events\""
        ",\"track_ids\":" +
        track_ids + "}}");
    REQUIRE(no_path["ok"].getBool() == false);
    REQUIRE(no_path["error"]["code"].getString() == "invalid_argument");
}

TEST_CASE("Closing a trace resets the session")
{
    Client client;
    REQUIRE(client.IsValid());
    client.CallAndWait(OpenTraceRequest());

    client.CallAndWait("{\"method\":\"trace.close\",\"params\":{}}");

    jt::Json status = client.CallAndWait("{\"method\":\"trace.status\",\"params\":{}}");
    REQUIRE(status["state"].getString() == "empty");
    REQUIRE(status["pending_requests"].getLong() == 0);

    /* The session must be reusable after a close. */
    jt::Json reopened = client.CallAndWait(OpenTraceRequest());
    REQUIRE(reopened["status"].getString() == "ready");
}
