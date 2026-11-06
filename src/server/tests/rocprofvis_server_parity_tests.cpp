// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// Opens the same trace twice: once with a local controller, once through a
// roc-optiq-server on loopback. Every request is issued to both DataProviders
// with the same parameters, and the models they end up with must match. This is
// what "nothing is lost over the wire" means in practice.

#include "protocol/rocprofvis_data_channel.h"
#include "protocol/rocprofvis_protocol.h"
#include "protocol/rocprofvis_protocol_frame.h"
#include "protocol/rocprofvis_protocol_uri.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_server.h"
#include "rocprofvis_server_connection.h"

#include "httplib.h"
#include "json.h"
#include "spdlog/spdlog.h"

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <vector>

std::string g_input_file = "sample/rocpd-transpose.db";

void release_fixture();

int
main(int argc, char** argv)
{
    Catch::Session session;
    using namespace Catch::Clara;
    session.cli(session.cli() |
                Opt(g_input_file, "input_file")["--input_file"]("Path to input file"));
    int result = session.applyCommandLine(argc, argv);
    spdlog::set_level(spdlog::level::warn);
    if(result == 0)
    {
        result = session.run();
    }
    // Before static destruction: the controller's singletons are created after
    // the fixture, so they would otherwise be destroyed first.
    release_fixture();
    return result;
}

namespace
{

using namespace RocProfVis;

constexpr std::chrono::seconds REQUEST_TIMEOUT(300);
constexpr uint32_t            TRACK_PIXELS     = 1920;
constexpr size_t              MAX_TRACKS       = 48;
constexpr uint64_t            TABLE_ROWS       = 200;
constexpr uint64_t            METRICS_CLIENT   = 42;
constexpr const char*         PARITY_TOKEN     = "parity-test-token";

bool
same_double(double a, double b)
{
    return a == b || (std::isnan(a) && std::isnan(b));
}

bool
same_doubles(const std::vector<double>& a, const std::vector<double>& b)
{
    bool result = a.size() == b.size();
    for(size_t i = 0; result && i < a.size(); i++)
    {
        result = same_double(a[i], b[i]);
    }
    return result;
}

std::string
read_file(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

// Updates every provider until done() holds. False on timeout.
bool
pump_providers(const std::vector<View::DataProvider*>& providers,
               const std::function<bool()>&            done)
{
    auto deadline = std::chrono::steady_clock::now() + REQUEST_TIMEOUT;
    while(!done())
    {
        for(View::DataProvider* provider : providers)
        {
            provider->Update();
        }
        if(std::chrono::steady_clock::now() > deadline)
        {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

// Polls until check() holds or timeout passes.
bool
wait_until(const std::function<bool()>& check, std::chrono::seconds timeout)
{
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while(!check())
    {
        if(std::chrono::steady_clock::now() > deadline)
        {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return true;
}

void
require_same_tracks(View::DataProvider& a, View::DataProvider& b)
{
    const std::vector<const View::TrackInfo*>& tracks_a =
        a.DataModel().GetTimeline().GetTrackList();
    const std::vector<const View::TrackInfo*>& tracks_b =
        b.DataModel().GetTimeline().GetTrackList();
    REQUIRE(!tracks_a.empty());
    REQUIRE(tracks_a.size() == tracks_b.size());
    for(size_t i = 0; i < tracks_a.size(); i++)
    {
        REQUIRE(tracks_a[i]->id == tracks_b[i]->id);
        REQUIRE(tracks_a[i]->track_type == tracks_b[i]->track_type);
        REQUIRE(tracks_a[i]->main_name == tracks_b[i]->main_name);
        REQUIRE(tracks_a[i]->sub_name == tracks_b[i]->sub_name);
        REQUIRE(tracks_a[i]->compare_source.id == tracks_b[i]->compare_source.id);
    }
    REQUIRE(same_double(a.DataModel().GetTimeline().GetStartTime(),
                        b.DataModel().GetTimeline().GetStartTime()));
    REQUIRE(same_double(a.DataModel().GetTimeline().GetEndTime(),
                        b.DataModel().GetTimeline().GetEndTime()));
}

/*
 * The local and remote providers, each on a private copy of the input so the
 * two controllers never share a SQLite file, and the server they talk through.
 */
class ParityFixture
{
public:
    ParityFixture()
    {
        std::error_code error;
        m_directory = std::filesystem::temp_directory_path() /
                      ("roc-optiq-parity-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(m_directory / "local_config", error);
        std::filesystem::create_directories(m_directory / "server_config", error);
        std::filesystem::create_directories(m_directory / "local", error);
        std::filesystem::create_directories(m_directory / "server", error);
        // Same file name on both sides: some formats name the node after the file.
        std::string file_name = std::filesystem::path(g_input_file).filename().string();
        m_local_trace         = (m_directory / "local" / file_name).string();
        m_server_trace        = (m_directory / "server" / file_name).string();
        std::filesystem::copy_file(g_input_file, m_local_trace,
                                   std::filesystem::copy_options::overwrite_existing);
        std::filesystem::copy_file(g_input_file, m_server_trace,
                                   std::filesystem::copy_options::overwrite_existing);

        Server::server_options_t options;
        options.host             = "127.0.0.1";
        options.port             = 0;
        options.config_path      = (m_directory / "server_config").string();
        options.token            = PARITY_TOKEN;
        options.upload_directory = (m_directory / "uploads").string();
        options.allow_profiler   = true;
        options.allow_scripts    = true;
        m_server                 = std::make_unique<Server::Server>(options);
        m_bound             = m_server->Bind();
        if(m_bound)
        {
            m_server_thread = std::thread([this]() { m_server->Listen(); });
        }
    }

    ~ParityFixture()
    {
        remote.CloseController();
        local.CloseController();
        View::ServerConnection::DisconnectAll();
        if(m_server)
        {
            m_server->Stop();
        }
        if(m_server_thread.joinable())
        {
            m_server_thread.join();
        }
        std::error_code error;
        std::filesystem::remove_all(m_directory, error);
    }

    bool Load()
    {
        if(m_loaded)
        {
            return m_load_result;
        }
        m_loaded = true;
        if(!m_bound)
        {
            return false;
        }

        rocprofvis_controller_t* controller = rocprofvis_controller_alloc(
            m_local_trace.c_str(), (m_directory / "local_config").string().c_str());
        if(!controller)
        {
            return false;
        }
        rocprofvis_controller_object_type_t type = kRPVControllerObjectTypeControllerSystem;
        rocprofvis_controller_get_object_type(controller, &type);
        is_compute = type == kRPVControllerObjectTypeControllerCompute;
        if(!local.FetchTrace(controller, m_local_trace))
        {
            return false;
        }

        std::string                             error;
        std::shared_ptr<View::ServerConnection> connection = Connect(error);
        if(!connection)
        {
            spdlog::error("Connect failed: {}", error);
            return false;
        }
        bool                                remote_is_compute = false;
        std::shared_ptr<View::IDataChannel> channel =
            connection->OpenTrace({ m_server_trace }, remote_is_compute, error);
        if(!channel || remote_is_compute != is_compute)
        {
            spdlog::error("Open failed: {}", error);
            return false;
        }
        if(!remote.FetchRemoteTrace(channel,
                                    View::make_server_uri("127.0.0.1", m_server->GetPort(),
                                                          false, m_server_trace),
                                    is_compute))
        {
            return false;
        }

        m_load_result = Pump([this]() {
                            return local.GetState() != View::ProviderState::kLoading &&
                                   remote.GetState() != View::ProviderState::kLoading;
                        }) &&
                        local.GetState() == View::ProviderState::kReady &&
                        remote.GetState() == View::ProviderState::kReady;
        return m_load_result;
    }

    // Updates both providers until done() holds. False on timeout.
    bool Pump(const std::function<bool()>& done)
    {
        auto deadline = std::chrono::steady_clock::now() + REQUEST_TIMEOUT;
        while(!done())
        {
            local.Update();
            remote.Update();
            if(std::chrono::steady_clock::now() > deadline)
            {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return true;
    }

    bool PumpRequest(uint64_t request_id)
    {
        return Pump([this, request_id]() {
            return !local.IsRequestPending(request_id) && !remote.IsRequestPending(request_id);
        });
    }

    std::string TemporaryPath(const std::string& name) const
    {
        return (m_directory / name).string();
    }

    std::shared_ptr<View::ServerConnection> Connect(std::string& error)
    {
        View::ServerConnectionOptions options;
        options.token = PARITY_TOKEN;
        return View::ServerConnection::Get("127.0.0.1", m_server->GetPort(), options, error);
    }

    int GetPort() const { return m_server ? m_server->GetPort() : 0; }

    const std::string& GetLocalTrace() const { return m_local_trace; }
    const std::string& GetServerTrace() const { return m_server_trace; }
    std::filesystem::path GetUploadDirectory() const { return m_directory / "uploads"; }

    View::DataProvider local;
    View::DataProvider remote;
    bool               is_compute = false;

private:
    std::filesystem::path           m_directory;
    std::string                     m_local_trace;
    std::string                     m_server_trace;
    std::unique_ptr<Server::Server> m_server;
    std::thread                     m_server_thread;
    bool                            m_bound       = false;
    bool                            m_loaded      = false;
    bool                            m_load_result = false;
};

std::unique_ptr<ParityFixture>&
fixture_storage()
{
    static std::unique_ptr<ParityFixture> s_fixture;
    return s_fixture;
}

ParityFixture&
fixture()
{
    std::unique_ptr<ParityFixture>& storage = fixture_storage();
    if(!storage)
    {
        storage = std::make_unique<ParityFixture>();
    }
    return *storage;
}

std::vector<const View::TrackInfo*>
tracks_of_type(View::DataProvider& provider, rocprofvis_controller_track_type_t type)
{
    std::vector<const View::TrackInfo*> tracks;
    for(const View::TrackInfo* track : provider.DataModel().GetTimeline().GetTrackList())
    {
        if(track && track->track_type == type && tracks.size() < MAX_TRACKS)
        {
            tracks.push_back(track);
        }
    }
    return tracks;
}

void
require_same_table(const View::TableInfo& a, const View::TableInfo& b)
{
    REQUIRE(a.table_header == b.table_header);
    REQUIRE(a.table_column_types == b.table_column_types);
    REQUIRE(a.total_row_count == b.total_row_count);
    REQUIRE(a.table_data == b.table_data);
}

void
require_same_events(const View::EventInfo& a, const View::EventInfo& b)
{
    REQUIRE(a.track_id == b.track_id);
    REQUIRE(a.basic_info.id.uuid == b.basic_info.id.uuid);
    REQUIRE(a.basic_info.name == b.basic_info.name);
    REQUIRE(same_double(a.basic_info.start_ts, b.basic_info.start_ts));
    REQUIRE(same_double(a.basic_info.duration, b.basic_info.duration));
    REQUIRE(a.basic_info.level == b.basic_info.level);
    REQUIRE(a.basic_info.stream_level == b.basic_info.stream_level);
    REQUIRE(a.ext_info.size() == b.ext_info.size());
    for(size_t i = 0; i < a.ext_info.size(); i++)
    {
        REQUIRE(a.ext_info[i].category == b.ext_info[i].category);
        REQUIRE(a.ext_info[i].name == b.ext_info[i].name);
        REQUIRE(a.ext_info[i].value == b.ext_info[i].value);
        REQUIRE(a.ext_info[i].category_enum == b.ext_info[i].category_enum);
    }
    REQUIRE(a.args.size() == b.args.size());
    for(size_t i = 0; i < a.args.size(); i++)
    {
        REQUIRE(a.args[i].position == b.args[i].position);
        REQUIRE(a.args[i].name == b.args[i].name);
        REQUIRE(a.args[i].value == b.args[i].value);
        REQUIRE(a.args[i].data_type == b.args[i].data_type);
    }
    REQUIRE(a.flow_info.size() == b.flow_info.size());
    for(size_t i = 0; i < a.flow_info.size(); i++)
    {
        REQUIRE(a.flow_info[i].id.uuid == b.flow_info[i].id.uuid);
        REQUIRE(a.flow_info[i].start_timestamp == b.flow_info[i].start_timestamp);
        REQUIRE(a.flow_info[i].end_timestamp == b.flow_info[i].end_timestamp);
        REQUIRE(a.flow_info[i].track_id == b.flow_info[i].track_id);
        REQUIRE(a.flow_info[i].level == b.flow_info[i].level);
        REQUIRE(a.flow_info[i].direction == b.flow_info[i].direction);
        REQUIRE(a.flow_info[i].name == b.flow_info[i].name);
    }
    REQUIRE(a.call_stack_info.size() == b.call_stack_info.size());
    for(size_t i = 0; i < a.call_stack_info.size(); i++)
    {
        REQUIRE(a.call_stack_info[i].file == b.call_stack_info[i].file);
        REQUIRE(a.call_stack_info[i].pc == b.call_stack_info[i].pc);
        REQUIRE(a.call_stack_info[i].name == b.call_stack_info[i].name);
        REQUIRE(a.call_stack_info[i].address == b.call_stack_info[i].address);
        REQUIRE(a.call_stack_info[i].id.uuid == b.call_stack_info[i].id.uuid);
    }
}

}  // namespace

void
release_fixture()
{
    fixture_storage().reset();
}

TEST_CASE("Split frames join back together", "[system][compute]")
{
    std::string frame(5 * View::MAX_TLS_FRAME_SIZE + 123, '\0');
    for(size_t i = 0; i < frame.size(); i++)
    {
        frame[i] = static_cast<char>(i * 31);
    }
    frame[0] = View::FRAME_TAG_COMPRESSED;

    std::vector<std::string> pieces = View::split_frame(frame, View::MAX_TLS_FRAME_SIZE);
    REQUIRE(pieces.size() == 6);
    View::FrameAssembler assembler;
    for(size_t i = 0; i < pieces.size(); i++)
    {
        REQUIRE(pieces[i].size() <= View::MAX_TLS_FRAME_SIZE);
        std::string piece = pieces[i];
        REQUIRE(assembler.Add(piece) == (i + 1 == pieces.size()));
        if(i + 1 == pieces.size())
        {
            REQUIRE(piece == frame);
        }
    }

    // A frame that fits passes through untouched, as one piece.
    std::string small = "{\"type\":\"progress\"}";
    REQUIRE(View::split_frame(small, View::MAX_TLS_FRAME_SIZE).size() == 1);
    std::string passed = small;
    REQUIRE(assembler.Add(passed));
    REQUIRE(passed == small);
}

TEST_CASE("Trace loads through the server with the same metadata", "[system][compute]")
{
    ParityFixture& f = fixture();
    REQUIRE(f.Load());

    if(f.is_compute)
    {
        jt::Json local_model  = View::encode_compute_trace(f.local.ComputeModel(), {});
        jt::Json remote_model = View::encode_compute_trace(f.remote.ComputeModel(), {});
        REQUIRE(local_model.toString() == remote_model.toString());
        for(const View::WorkloadInfo* workload : f.local.ComputeModel().GetWorkloadList())
        {
            const View::WorkloadInfo* remote_workload =
                f.remote.ComputeModel().GetWorkload(workload->id);
            REQUIRE(remote_workload);
            REQUIRE(workload->memory_chart_layout.blocks.size() ==
                    remote_workload->memory_chart_layout.blocks.size());
            REQUIRE(workload->memory_chart_layout.arrows.size() ==
                    remote_workload->memory_chart_layout.arrows.size());
            REQUIRE(workload->available_metrics.ordered_categories.size() ==
                    remote_workload->available_metrics.ordered_categories.size());
            REQUIRE(workload->ordered_kernels.size() == remote_workload->ordered_kernels.size());
        }
        return;
    }

    const View::TimelineModel& local_timeline  = f.local.DataModel().GetTimeline();
    const View::TimelineModel& remote_timeline = f.remote.DataModel().GetTimeline();
    REQUIRE(local_timeline.GetTrackCount() == remote_timeline.GetTrackCount());
    REQUIRE(same_double(local_timeline.GetStartTime(), remote_timeline.GetStartTime()));
    REQUIRE(same_double(local_timeline.GetEndTime(), remote_timeline.GetEndTime()));
    REQUIRE(same_doubles(local_timeline.GetHistogram(), remote_timeline.GetHistogram()));
    REQUIRE(local_timeline.GetMiniMap().size() == remote_timeline.GetMiniMap().size());
    for(const auto& entry : local_timeline.GetMiniMap())
    {
        auto remote_entry = remote_timeline.GetMiniMap().find(entry.first);
        REQUIRE(remote_entry != remote_timeline.GetMiniMap().end());
        REQUIRE(same_doubles(entry.second, remote_entry->second));
    }

    std::vector<const View::TrackInfo*> local_tracks  = local_timeline.GetTrackList();
    std::vector<const View::TrackInfo*> remote_tracks = remote_timeline.GetTrackList();
    REQUIRE(local_tracks.size() == remote_tracks.size());
    for(size_t i = 0; i < local_tracks.size(); i++)
    {
        REQUIRE(local_tracks[i]);
        REQUIRE(remote_tracks[i]);
        const View::TrackInfo& a = *local_tracks[i];
        const View::TrackInfo& b = *remote_tracks[i];
        REQUIRE(a.index == b.index);
        REQUIRE(a.id == b.id);
        REQUIRE(a.track_type == b.track_type);
        REQUIRE(same_double(a.min_ts, b.min_ts));
        REQUIRE(same_double(a.max_ts, b.max_ts));
        REQUIRE(a.num_entries == b.num_entries);
        REQUIRE(a.instance_id == b.instance_id);
        REQUIRE(a.file_id == b.file_id);
        REQUIRE(a.order_rank == b.order_rank);
        REQUIRE(a.agent_or_pid == b.agent_or_pid);
        REQUIRE(a.queue_id_or_tid == b.queue_id_or_tid);
        REQUIRE(same_double(a.min_value, b.min_value));
        REQUIRE(same_double(a.max_value, b.max_value));
        REQUIRE(a.category == b.category);
        REQUIRE(a.main_name == b.main_name);
        REQUIRE(a.sub_name == b.sub_name);
        REQUIRE(a.operation_types == b.operation_types);
        REQUIRE(a.topology.node_id == b.topology.node_id);
        REQUIRE(a.topology.process_id == b.topology.process_id);
        REQUIRE(a.topology.device_id == b.topology.device_id);
        REQUIRE(a.topology.type == b.topology.type);
        REQUIRE(a.topology.id.value == b.topology.id.value);
        REQUIRE(f.local.DataModel().BuildTrackName(a.id) ==
                f.remote.DataModel().BuildTrackName(b.id));
    }

    const View::TopologyTree& local_topology  = f.local.DataModel().GetTopology();
    const View::TopologyTree& remote_topology = f.remote.DataModel().GetTopology();
    REQUIRE(local_topology.ToString() == remote_topology.ToString());
    REQUIRE(local_topology.GetTrackOrder() == remote_topology.GetTrackOrder());
    REQUIRE(local_topology.NodeCount() == remote_topology.NodeCount());
    REQUIRE(local_topology.ProcessCount() == remote_topology.ProcessCount());
    REQUIRE(local_topology.GetGPUCount() == remote_topology.GetGPUCount());
    REQUIRE(local_topology.GetCPUCount() == remote_topology.GetCPUCount());
}

TEST_CASE("Track data matches at timeline zoom", "[system]")
{
    ParityFixture& f = fixture();
    REQUIRE(f.Load());
    if(f.is_compute)
    {
        SUCCEED("Compute trace");
        return;
    }

    std::vector<uint64_t> request_ids;
    std::vector<uint64_t> track_ids;
    for(const View::TrackInfo* track : f.local.DataModel().GetTimeline().GetTrackList())
    {
        if(!track || track_ids.size() >= MAX_TRACKS)
        {
            continue;
        }
        View::TrackRequestParams params(static_cast<uint32_t>(track->id),
                                        f.local.DataModel().GetTimeline().GetStartTime(),
                                        f.local.DataModel().GetTimeline().GetEndTime(),
                                        TRACK_PIXELS, 1, 0, 1);
        std::pair<bool, uint64_t> local_request  = f.local.FetchTrack(params);
        std::pair<bool, uint64_t> remote_request = f.remote.FetchTrack(params);
        REQUIRE(local_request.first == remote_request.first);
        REQUIRE(local_request.second == remote_request.second);
        if(local_request.first)
        {
            request_ids.push_back(local_request.second);
            track_ids.push_back(track->id);
        }
    }
    REQUIRE(!track_ids.empty());
    REQUIRE(f.Pump([&]() {
        for(uint64_t request_id : request_ids)
        {
            if(f.local.IsRequestPending(request_id) || f.remote.IsRequestPending(request_id))
            {
                return false;
            }
        }
        return true;
    }));

    size_t total_items = 0;
    for(uint64_t track_id : track_ids)
    {
        const View::RawTrackData* a = f.local.DataModel().GetTimeline().GetTrackData(track_id);
        const View::RawTrackData* b = f.remote.DataModel().GetTimeline().GetTrackData(track_id);
        REQUIRE((a == nullptr) == (b == nullptr));
        if(!a)
        {
            continue;
        }
        REQUIRE(a->GetType() == b->GetType());
        REQUIRE(a->AllDataReady() == b->AllDataReady());
        if(a->GetType() == kRPVControllerTrackTypeEvents)
        {
            const std::vector<View::TraceEvent>& ea =
                static_cast<const View::RawTrackEventData*>(a)->GetData();
            const std::vector<View::TraceEvent>& eb =
                static_cast<const View::RawTrackEventData*>(b)->GetData();
            REQUIRE(ea.size() == eb.size());
            for(size_t i = 0; i < ea.size(); i++)
            {
                REQUIRE(ea[i].m_id.uuid == eb[i].m_id.uuid);
                REQUIRE(ea[i].m_name == eb[i].m_name);
                REQUIRE(same_double(ea[i].m_start_ts, eb[i].m_start_ts));
                REQUIRE(same_double(ea[i].m_duration, eb[i].m_duration));
                REQUIRE(ea[i].m_level == eb[i].m_level);
                REQUIRE(ea[i].m_child_count == eb[i].m_child_count);
                REQUIRE(ea[i].m_top_combined_name == eb[i].m_top_combined_name);
            }
            total_items += ea.size();
        }
        else
        {
            const std::vector<View::TraceCounter>& sa =
                static_cast<const View::RawTrackSampleData*>(a)->GetData();
            const std::vector<View::TraceCounter>& sb =
                static_cast<const View::RawTrackSampleData*>(b)->GetData();
            REQUIRE(sa.size() == sb.size());
            for(size_t i = 0; i < sa.size(); i++)
            {
                REQUIRE(same_double(sa[i].m_start_ts, sb[i].m_start_ts));
                REQUIRE(same_double(sa[i].m_end_ts, sb[i].m_end_ts));
                REQUIRE(same_double(sa[i].m_value, sb[i].m_value));
            }
            total_items += sa.size();
        }
    }
    REQUIRE(total_items > 0);
}

TEST_CASE("Event, sample and top-events tables match", "[system]")
{
    ParityFixture& f = fixture();
    REQUIRE(f.Load());
    if(f.is_compute)
    {
        SUCCEED("Compute trace");
        return;
    }

    const double start = f.local.DataModel().GetTimeline().GetStartTime();
    const double end   = f.local.DataModel().GetTimeline().GetEndTime();
    std::vector<uint64_t> event_tracks;
    for(const View::TrackInfo* track : tracks_of_type(f.local, kRPVControllerTrackTypeEvents))
    {
        event_tracks.push_back(track->id);
    }
    std::vector<uint64_t> sample_tracks;
    for(const View::TrackInfo* track : tracks_of_type(f.local, kRPVControllerTrackTypeSamples))
    {
        sample_tracks.push_back(track->id);
    }

    if(!event_tracks.empty())
    {
        REQUIRE(f.local.FetchMultiTrackEventTable(event_tracks, start, end, "", "", "", 0,
                                                  TABLE_ROWS, 1, kRPVControllerSortOrderDescending));
        REQUIRE(f.remote.FetchMultiTrackEventTable(event_tracks, start, end, "", "", "", 0,
                                                   TABLE_ROWS, 1, kRPVControllerSortOrderDescending));
        REQUIRE(f.PumpRequest(View::DataProvider::EVENT_TABLE_REQUEST_ID));
        require_same_table(f.local.DataModel().GetTables().GetTable(View::TableType::kEventTable),
                           f.remote.DataModel().GetTables().GetTable(View::TableType::kEventTable));
        REQUIRE(!f.local.DataModel().GetTables().GetTable(View::TableType::kEventTable)
                     .table_data.empty());
    }
    if(!sample_tracks.empty())
    {
        REQUIRE(f.local.FetchMultiTrackSampleTable(sample_tracks, start, end, "", 0, TABLE_ROWS));
        REQUIRE(f.remote.FetchMultiTrackSampleTable(sample_tracks, start, end, "", 0, TABLE_ROWS));
        REQUIRE(f.PumpRequest(View::DataProvider::SAMPLE_TABLE_REQUEST_ID));
        require_same_table(
            f.local.DataModel().GetTables().GetTable(View::TableType::kSampleTable),
            f.remote.DataModel().GetTables().GetTable(View::TableType::kSampleTable));
    }

    const rocprofvis_controller_table_type_t top_events[] = {
        kRPVControllerTableTypeInstrumentedEvents, kRPVControllerTableTypeDispatchEvents,
        kRPVControllerTableTypeMemoryAllocationEvents, kRPVControllerTableTypeMemoryCopyEvents,
        kRPVControllerTableTypeSampledEvents
    };
    for(rocprofvis_controller_table_type_t table_type : top_events)
    {
        View::TrackTableRequestParams params(table_type, event_tracks, start, end, nullptr, "",
                                             "", "", 0, TABLE_ROWS);
        bool local_ok  = f.local.FetchTable(params);
        bool remote_ok = f.remote.FetchTable(params);
        REQUIRE(local_ok == remote_ok);
        if(!local_ok)
        {
            continue;
        }
        REQUIRE(f.Pump([&]() {
            return !f.local.IsTableRequestPending(table_type) &&
                   !f.remote.IsTableRequestPending(table_type);
        }));
        std::pair<View::TablesModel*, View::TableType> local_slot =
            f.local.GetTableSlot(table_type, &params);
        std::pair<View::TablesModel*, View::TableType> remote_slot =
            f.remote.GetTableSlot(table_type, &params);
        require_same_table(local_slot.first->GetTable(local_slot.second),
                           remote_slot.first->GetTable(remote_slot.second));
    }
}

TEST_CASE("Event search matches", "[system]")
{
    ParityFixture& f = fixture();
    REQUIRE(f.Load());
    if(f.is_compute)
    {
        SUCCEED("Compute trace");
        return;
    }

    View::EventSearchRequestParams params(
        kRPVControllerTableTypeSearchResults, {}, f.local.DataModel().GetTimeline().GetStartTime(),
        f.local.DataModel().GetTimeline().GetEndTime(), nullptr, true, false, true, { "a" }, 0,
        TABLE_ROWS);
    REQUIRE(f.local.FetchTable(params));
    REQUIRE(f.remote.FetchTable(params));
    REQUIRE(f.PumpRequest(View::DataProvider::EVENT_SEARCH_REQUEST_ID));
    require_same_table(
        f.local.DataModel().GetTables().GetTable(View::TableType::kEventSearchTable),
        f.remote.DataModel().GetTables().GetTable(View::TableType::kEventSearchTable));
}

TEST_CASE("Summary matches", "[system]")
{
    ParityFixture& f = fixture();
    REQUIRE(f.Load());
    if(f.is_compute)
    {
        SUCCEED("Compute trace");
        return;
    }

    REQUIRE(f.local.FetchSummary());
    REQUIRE(f.remote.FetchSummary());
    REQUIRE(f.PumpRequest(View::DataProvider::SUMMARY_REQUEST_ID));
    const View::SummaryModel& a = f.local.DataModel().GetSummary();
    const View::SummaryModel& b = f.remote.DataModel().GetSummary();
    REQUIRE(View::encode_summary(a).toString() == View::encode_summary(b).toString());
    REQUIRE(a.GetEventValues().size() == b.GetEventValues().size());
    REQUIRE(a.GetCounterValues().size() == b.GetCounterValues().size());
    REQUIRE(a.GetTimeunitValues().size() == b.GetTimeunitValues().size());
    REQUIRE(a.GetSummaryData().sub_metrics.size() == b.GetSummaryData().sub_metrics.size());

    View::EventSearchRequestParams instances(
        kRPVControllerTableTypeSummaryKernelInstances, {}, f.local.DataModel().GetTimeline().GetStartTime(),
        f.local.DataModel().GetTimeline().GetEndTime(), nullptr, false, false, false, {}, 0,
        TABLE_ROWS);
    REQUIRE(f.local.FetchTable(instances));
    REQUIRE(f.remote.FetchTable(instances));
    REQUIRE(f.PumpRequest(View::DataProvider::SUMMARY_KERNEL_INSTANCE_TABLE_REQUEST_ID));
    require_same_table(
        f.local.DataModel().GetSummary().GetTables().GetTable(View::TableType::kSummaryKernelTable),
        f.remote.DataModel().GetSummary().GetTables().GetTable(View::TableType::kSummaryKernelTable));
}

TEST_CASE("Event details, flows and call stacks match", "[system]")
{
    ParityFixture& f = fixture();
    REQUIRE(f.Load());
    if(f.is_compute)
    {
        SUCCEED("Compute trace");
        return;
    }

    size_t checked = 0;
    for(const View::TrackInfo* track : tracks_of_type(f.local, kRPVControllerTrackTypeEvents))
    {
        const View::RawTrackEventData* data = dynamic_cast<const View::RawTrackEventData*>(
            f.local.DataModel().GetTimeline().GetTrackData(track->id));
        if(!data || data->GetData().empty() || checked >= 6)
        {
            continue;
        }
        const uint64_t event_id = data->GetData().front().m_id.uuid;
        REQUIRE(f.local.FetchEvent(track->id, event_id));
        REQUIRE(f.remote.FetchEvent(track->id, event_id));
        REQUIRE(f.PumpRequest(View::DataProvider::EVENT_EXTENDED_DATA_REQUEST_ID));
        REQUIRE(f.local.FetchEventFlowDetails(event_id));
        REQUIRE(f.remote.FetchEventFlowDetails(event_id));
        REQUIRE(f.PumpRequest(View::DataProvider::EVENT_FLOW_DATA_REQUEST_ID));
        REQUIRE(f.local.FetchEventCallStackData(event_id));
        REQUIRE(f.remote.FetchEventCallStackData(event_id));
        REQUIRE(f.PumpRequest(View::DataProvider::EVENT_CALL_STACK_DATA_REQUEST_ID));

        const View::EventInfo* a = f.local.DataModel().GetEvents().GetEvent(event_id);
        const View::EventInfo* b = f.remote.DataModel().GetEvents().GetEvent(event_id);
        REQUIRE(a);
        REQUIRE(b);
        require_same_events(*a, *b);
        checked++;
    }
    REQUIRE(checked > 0);
}

TEST_CASE("Track statistics match", "[system]")
{
    ParityFixture& f = fixture();
    REQUIRE(f.Load());
    if(f.is_compute)
    {
        SUCCEED("Compute trace");
        return;
    }

    size_t checked = 0;
    for(const View::TrackInfo* track : f.local.DataModel().GetTimeline().GetTrackList())
    {
        if(!track || (track->topology.type != View::TrackInfo::TrackType::Queue &&
                      track->topology.type != View::TrackInfo::TrackType::Counter))
        {
            continue;
        }
        const View::AnalysisTrackStatistics* a =
            f.local.DataModel().GetAnalysis().RegisterTrack(*track);
        const View::AnalysisTrackStatistics* b = f.remote.DataModel().GetAnalysis().RegisterTrack(
            *f.remote.DataModel().GetTimeline().GetTrack(track->id));
        REQUIRE(a);
        REQUIRE(b);
        // As the track details pane does before it asks.
        a->state = View::AnalysisTrackStatistics::State::kRequested;
        b->state = View::AnalysisTrackStatistics::State::kRequested;
        View::AnalysisTrackStatisticsRequestParams params(track->id, track->min_ts,
                                                          track->max_ts);
        REQUIRE(f.local.FetchAnalysisTrackStatistics(params));
        REQUIRE(f.remote.FetchAnalysisTrackStatistics(params));
        REQUIRE(f.PumpRequest(View::RequestIdBuilder::MakeTrackDataRequestId(
            static_cast<uint32_t>(track->id), 0, 0,
            View::RequestType::kFetchAnalysisTrackStatistics)));
        REQUIRE(a->state == View::AnalysisTrackStatistics::State::kReady);
        REQUIRE(b->state == View::AnalysisTrackStatistics::State::kReady);
        REQUIRE(a->stats.size() == b->stats.size());
        for(size_t i = 0; i < a->stats.size(); i++)
        {
            REQUIRE(same_double(a->stats[i].value, b->stats[i].value));
            REQUIRE(a->stats[i].full == b->stats[i].full);
        }
        if(++checked >= 8)
        {
            break;
        }
    }
    REQUIRE(checked > 0);
}

TEST_CASE("Exported tables and trimmed traces arrive intact", "[system]")
{
    ParityFixture& f = fixture();
    REQUIRE(f.Load());
    if(f.is_compute)
    {
        SUCCEED("Compute trace");
        return;
    }

    std::vector<uint64_t> event_tracks;
    for(const View::TrackInfo* track : tracks_of_type(f.local, kRPVControllerTrackTypeEvents))
    {
        event_tracks.push_back(track->id);
    }
    REQUIRE(!event_tracks.empty());
    const double start = f.local.DataModel().GetTimeline().GetStartTime();
    const double end   = f.local.DataModel().GetTimeline().GetEndTime();

    std::string local_csv  = f.TemporaryPath("local.csv");
    std::string remote_csv = f.TemporaryPath("remote.csv");
    View::TrackTableRequestParams local_export(kRPVControllerTableTypeEvents, event_tracks,
                                               start, end, nullptr, "", "", "", 0, TABLE_ROWS,
                                               0, kRPVControllerSortOrderAscending, local_csv);
    View::TrackTableRequestParams remote_export(kRPVControllerTableTypeEvents, event_tracks,
                                                start, end, nullptr, "", "", "", 0, TABLE_ROWS,
                                                0, kRPVControllerSortOrderAscending, remote_csv);
    REQUIRE(f.local.FetchTable(local_export));
    REQUIRE(f.remote.FetchTable(remote_export));
    REQUIRE(f.PumpRequest(View::DataProvider::TABLE_EXPORT_REQUEST_ID));
    std::string local_text = read_file(local_csv);
    REQUIRE(!local_text.empty());
    REQUIRE(local_text == read_file(remote_csv));

    bool local_saved  = false;
    bool remote_saved = false;
    f.local.SetSaveTraceCallback([&local_saved](bool success) { local_saved = success; });
    f.remote.SetSaveTraceCallback([&remote_saved](bool success) { remote_saved = success; });
    std::string extension  = std::filesystem::path(g_input_file).extension().string();
    std::string local_trim  = f.TemporaryPath("local_trim" + extension);
    std::string remote_trim = f.TemporaryPath("remote_trim" + extension);
    const double trim_end   = start + (end - start) / 4;
    REQUIRE(f.local.SaveTrimmedTrace(local_trim, start, trim_end));
    REQUIRE(f.remote.SaveTrimmedTrace(remote_trim, start, trim_end));
    REQUIRE(f.PumpRequest(View::DataProvider::SAVE_TRIMMED_TRACE_REQUEST_ID));
    f.local.SetSaveTraceCallback(nullptr);
    f.remote.SetSaveTraceCallback(nullptr);
    REQUIRE(local_saved);
    REQUIRE(remote_saved);
    std::error_code error;
    REQUIRE(std::filesystem::file_size(remote_trim, error) > 0);
    REQUIRE(std::filesystem::file_size(local_trim, error) ==
            std::filesystem::file_size(remote_trim, error));
}

TEST_CASE("Compute metrics, kernel table and PC sampling match", "[compute]")
{
    ParityFixture& f = fixture();
    REQUIRE(f.Load());
    if(!f.is_compute)
    {
        SUCCEED("System trace");
        return;
    }

    for(const View::WorkloadInfo* workload : f.local.ComputeModel().GetWorkloadList())
    {
        std::vector<uint32_t> kernel_ids;
        for(const View::KernelInfo* kernel : workload->ordered_kernels)
        {
            kernel_ids.push_back(kernel->id);
        }
        std::vector<View::MetricsRequestParams::MetricID> metric_ids;
        std::vector<std::string>                           selectors;
        for(const View::AvailableMetrics::Category* category :
            workload->available_metrics.ordered_categories)
        {
            metric_ids.push_back({ category->id, std::nullopt, std::nullopt });
            for(const View::AvailableMetrics::Table* table : category->ordered_tables)
            {
                if(selectors.size() < 4 && !table->ordered_entries.empty() &&
                   !table->value_names.empty())
                {
                    const View::AvailableMetrics::Entry* entry = table->ordered_entries.front();
                    selectors.push_back(std::to_string(entry->category_id) + "." +
                                        std::to_string(entry->table_id) + "." +
                                        std::to_string(entry->id) + ":" +
                                        table->value_names.front());
                }
            }
        }

        View::MetricsRequestParams metrics(workload->id, kernel_ids, metric_ids,
                                           METRICS_CLIENT);
        REQUIRE(f.local.FetchMetrics(metrics));
        REQUIRE(f.remote.FetchMetrics(metrics));
        REQUIRE(f.PumpRequest(View::RequestIdBuilder::MakeClientRequestId(
            View::RequestType::kFetchMetrics, METRICS_CLIENT)));
        for(uint32_t kernel_id : kernel_ids)
        {
            const std::vector<std::shared_ptr<View::MetricValue>>* a =
                f.local.ComputeModel().GetKernelMetricsData(METRICS_CLIENT, kernel_id);
            const std::vector<std::shared_ptr<View::MetricValue>>* b =
                f.remote.ComputeModel().GetKernelMetricsData(METRICS_CLIENT, kernel_id);
            REQUIRE((a == nullptr) == (b == nullptr));
            if(a)
            {
                REQUIRE(a->size() == b->size());
                for(size_t i = 0; i < a->size(); i++)
                {
                    REQUIRE((*a)[i]->values.size() == (*b)[i]->values.size());
                    for(const auto& value : (*a)[i]->values)
                    {
                        auto other = (*b)[i]->values.find(value.first);
                        REQUIRE(other != (*b)[i]->values.end());
                        REQUIRE(same_double(value.second, other->second));
                    }
                }
            }
        }

        if(!selectors.empty())
        {
            View::ComputeTableRequestParams pivot(workload->id, selectors);
            REQUIRE(f.local.FetchMetricPivotTable(pivot));
            REQUIRE(f.remote.FetchMetricPivotTable(pivot));
            REQUIRE(f.PumpRequest(View::DataProvider::METRIC_PIVOT_TABLE_REQUEST_ID));
            const View::ComputeTableInfo& a =
                f.local.ComputeModel().GetKernelSelectionTable().GetTableInfo();
            const View::ComputeTableInfo& b =
                f.remote.ComputeModel().GetKernelSelectionTable().GetTableInfo();
            REQUIRE(a.table_header == b.table_header);
            REQUIRE(a.table_data == b.table_data);
        }

        for(const View::KernelInfo* kernel : workload->ordered_kernels)
        {
            if(!kernel->has_isa_lines)
            {
                continue;
            }
            const View::PcSamplingLayer layers[] = { View::PcSamplingLayer::kIsa,
                                                     View::PcSamplingLayer::kSource,
                                                     View::PcSamplingLayer::kStalls };
            for(View::PcSamplingLayer layer : layers)
            {
                View::PcSamplingRequestParams params(layer, workload->id, kernel->id, 0, 1, 1);
                REQUIRE(f.local.FetchPcSampling(params));
                REQUIRE(f.remote.FetchPcSampling(params));
                REQUIRE(f.PumpRequest(View::RequestIdBuilder::MakeClientRequestId(
                    View::RequestType::kFetchPcSampling, static_cast<uint64_t>(layer))));
                const View::KernelInfo* a =
                    f.local.ComputeModel().GetKernelInfo(workload->id, kernel->id);
                const View::KernelInfo* b =
                    f.remote.ComputeModel().GetKernelInfo(workload->id, kernel->id);
                REQUIRE(View::encode_pc_sampling(a->pc_sampling_data, layer).toString() ==
                        View::encode_pc_sampling(b->pc_sampling_data, layer).toString());
            }
            break;
        }
    }
}

TEST_CASE("Connections without the right token are refused", "[system][compute]")
{
    ParityFixture& f = fixture();
    REQUIRE(f.Load());

    // localhost rather than 127.0.0.1: a live connection is shared by address,
    // and these must be new ones.
    View::ServerConnectionOptions options;
    options.token = "not-the-token";
    std::string error;
    REQUIRE(View::ServerConnection::Get("localhost", f.GetPort(), options, error) == nullptr);
    REQUIRE(error.find("token") != std::string::npos);

    // Nothing to fall back on: no remembered token, no environment, no token file.
    options.token.clear();
    error.clear();
    REQUIRE(View::ServerConnection::Get("localhost", f.GetPort(), options, error) == nullptr);
    REQUIRE(!error.empty());
}

TEST_CASE("Requests in flight survive a dropped connection", "[system]")
{
    ParityFixture& f = fixture();
    REQUIRE(f.Load());
    if(f.is_compute)
    {
        SUCCEED("Compute trace");
        return;
    }
    std::string                             error;
    std::shared_ptr<View::ServerConnection> connection = f.Connect(error);
    REQUIRE(connection);

    for(int drop = 0; drop < 3; drop++)
    {
        std::vector<uint64_t> request_ids;
        std::vector<uint64_t> track_ids;
        for(const View::TrackInfo* track :
            tracks_of_type(f.local, kRPVControllerTrackTypeEvents))
        {
            f.local.DataModel().GetTimeline().FreeTrackData(track->id, true);
            f.remote.DataModel().GetTimeline().FreeTrackData(track->id, true);
            View::TrackRequestParams params(
                static_cast<uint32_t>(track->id),
                f.local.DataModel().GetTimeline().GetStartTime(),
                f.local.DataModel().GetTimeline().GetEndTime(), TRACK_PIXELS / (drop + 1),
                1, 0, 1);
            std::pair<bool, uint64_t> local_request  = f.local.FetchTrack(params);
            std::pair<bool, uint64_t> remote_request = f.remote.FetchTrack(params);
            REQUIRE(local_request.first == remote_request.first);
            if(local_request.first)
            {
                request_ids.push_back(local_request.second);
                track_ids.push_back(track->id);
            }
        }
        // Drops while the responses stream: some never left the server, some
        // were lost on the way. Both must be replayed after the resume.
        connection->DropConnection();
        REQUIRE(f.Pump([&]() {
            for(uint64_t request_id : request_ids)
            {
                if(f.local.IsRequestPending(request_id) || f.remote.IsRequestPending(request_id))
                {
                    return false;
                }
            }
            return true;
        }));
        REQUIRE(connection->GetState() == View::ServerConnectionState::kConnected);
        for(uint64_t track_id : track_ids)
        {
            const View::RawTrackData* a = f.local.DataModel().GetTimeline().GetTrackData(track_id);
            const View::RawTrackData* b = f.remote.DataModel().GetTimeline().GetTrackData(track_id);
            REQUIRE((a == nullptr) == (b == nullptr));
        }
    }

    // And the trace still answers once resumed.
    const double start = f.local.DataModel().GetTimeline().GetStartTime();
    const double end   = f.local.DataModel().GetTimeline().GetEndTime();
    std::vector<uint64_t> event_tracks;
    for(const View::TrackInfo* track : tracks_of_type(f.local, kRPVControllerTrackTypeEvents))
    {
        event_tracks.push_back(track->id);
    }
    if(!event_tracks.empty())
    {
        REQUIRE(f.local.FetchMultiTrackEventTable(event_tracks, start, end, "", "", "", 0,
                                                  TABLE_ROWS));
        REQUIRE(f.remote.FetchMultiTrackEventTable(event_tracks, start, end, "", "", "", 0,
                                                   TABLE_ROWS));
        REQUIRE(f.PumpRequest(View::DataProvider::EVENT_TABLE_REQUEST_ID));
        require_same_table(f.local.DataModel().GetTables().GetTable(View::TableType::kEventTable),
                           f.remote.DataModel().GetTables().GetTable(View::TableType::kEventTable));
    }
}

TEST_CASE("Uploaded traces arrive intact and open", "[system][compute]")
{
    ParityFixture& f = fixture();
    REQUIRE(f.Load());
    std::string                             error;
    std::shared_ptr<View::ServerConnection> connection = f.Connect(error);
    REQUIRE(connection);
    REQUIRE(connection->GetFeatures().upload);

    const uint64_t           upload_id = connection->StartUpload(g_input_file);
    View::ServerUploadStatus status;
    REQUIRE(wait_until(
        [&]() {
            return connection->GetUploadStatus(upload_id, status) &&
                   status.state != View::ServerUploadStatus::State::kRunning;
        },
        std::chrono::seconds(120)));
    INFO(status.error);
    REQUIRE(status.state == View::ServerUploadStatus::State::kDone);
    REQUIRE(status.sent == status.total);
    REQUIRE(std::filesystem::path(status.server_path).filename() ==
            std::filesystem::path(g_input_file).filename());
    REQUIRE(status.server_path.rfind(f.GetUploadDirectory().string(), 0) == 0);
    REQUIRE(read_file(status.server_path) == read_file(g_input_file));

    bool                                is_compute = false;
    std::shared_ptr<View::IDataChannel> channel =
        connection->OpenTrace({ status.server_path }, is_compute, error);
    INFO(error);
    REQUIRE(channel);
    REQUIRE(is_compute == f.is_compute);
    channel->Close();
}

TEST_CASE("Traces compared on the server match a local comparison", "[system]")
{
#ifndef ROCPROFVIS_ENABLE_TRACE_COMPARE
    SUCCEED("Built without trace compare");
#else
    ParityFixture& f = fixture();
    REQUIRE(f.Load());
    if(f.is_compute || std::filesystem::path(g_input_file).extension() != ".db")
    {
        SUCCEED("Only rocpd databases can be compared");
        return;
    }
    std::string                             error;
    std::shared_ptr<View::ServerConnection> connection = f.Connect(error);
    REQUIRE(connection);
    REQUIRE(connection->GetFeatures().compare);

    // Fresh copies, with the same file names on both sides.
    const std::string        file_name = std::filesystem::path(g_input_file).filename().string();
    std::vector<std::string> local_paths;
    std::vector<std::string> server_paths;
    for(const char* side : { "a", "b" })
    {
        for(const char* owner : { "local", "server" })
        {
            std::filesystem::path directory =
                f.TemporaryPath(std::string("compare_") + owner + "_" + side);
            std::filesystem::create_directories(directory);
            std::filesystem::copy_file(g_input_file, directory / file_name,
                                       std::filesystem::copy_options::overwrite_existing);
            (std::string(owner) == "local" ? local_paths : server_paths)
                .push_back((directory / file_name).string());
        }
    }

    std::vector<const char*> local_ptrs = { local_paths[0].c_str(), local_paths[1].c_str() };
    rocprofvis_controller_t* controller =
        rocprofvis_controller_alloc_compare(local_ptrs.data(), local_ptrs.size());
    REQUIRE(controller);
    View::DataProvider local_compare;
    REQUIRE(local_compare.FetchTrace(controller, "compare://local"));
    local_compare.DataModel().SetCompareSources(View::make_compare_sources(local_paths));

    bool                                is_compute = true;
    std::shared_ptr<View::IDataChannel> channel =
        connection->OpenTrace(server_paths, is_compute, error);
    INFO(error);
    REQUIRE(channel);
    REQUIRE(!is_compute);
    View::DataProvider remote_compare;
    REQUIRE(remote_compare.FetchRemoteTrace(channel, "compare://remote", false));
    remote_compare.DataModel().SetCompareSources(View::make_compare_sources(server_paths));

    REQUIRE(pump_providers({ &local_compare, &remote_compare }, [&]() {
        return local_compare.GetState() != View::ProviderState::kLoading &&
               remote_compare.GetState() != View::ProviderState::kLoading;
    }));
    REQUIRE(local_compare.GetState() == View::ProviderState::kReady);
    REQUIRE(remote_compare.GetState() == View::ProviderState::kReady);
    require_same_tracks(local_compare, remote_compare);
    // Both sources' tracks are there.
    REQUIRE(local_compare.DataModel().GetTimeline().GetTrackList().size() >
            f.local.DataModel().GetTimeline().GetTrackList().size());

    remote_compare.CloseController();
    local_compare.CloseController();
#endif
}

TEST_CASE("Profiler runs on the server stream output and their exit state", "[system]")
{
#ifndef ROCPROFVIS_ENABLE_PROFILER
    SUCCEED("Built without the profiler launcher");
#else
    ParityFixture& f = fixture();
    REQUIRE(f.Load());
    if(f.is_compute)
    {
        SUCCEED("Compute trace");
        return;
    }
    std::string                             error;
    std::shared_ptr<View::ServerConnection> connection = f.Connect(error);
    REQUIRE(connection);
    REQUIRE(connection->GetFeatures().profiler);

    auto run = [&connection](uint64_t tool, const std::vector<std::string>& argv) {
        jt::Json launch;
        launch["tool"] = static_cast<long long>(tool);
        jt::Json arguments;
        arguments.setArray();
        for(const std::string& argument : argv)
        {
            arguments.getArray().emplace_back(argument);
        }
        launch["argv"]                  = std::move(arguments);
        const uint64_t               id = connection->StartProfilerRun(launch);
        View::ServerProfilerRunStatus status;
        REQUIRE(wait_until(
            [&]() {
                return connection->GetProfilerRunStatus(id, status) &&
                       status.state != kRPVProfilerStateRunning &&
                       status.state != kRPVProfilerStateIdle;
            },
            std::chrono::seconds(120)));
        connection->ForgetProfilerRun(id);
        return status;
    };

    // An unknown tool is refused before anything runs.
    View::ServerProfilerRunStatus refused = run(9999, {});
    REQUIRE(refused.state == kRPVProfilerStateFailed);
    REQUIRE(!refused.error.empty());

    if(std::system("rocprofv3 --version > /dev/null 2>&1") == 0)
    {
        View::ServerProfilerRunStatus version = run(kRPVProfilerToolRocprofV3, { "--version" });
        INFO(version.output);
        REQUIRE(version.state == kRPVProfilerStateCompleted);
        REQUIRE(version.exit_code == 0);
        REQUIRE(version.output.find("version") != std::string::npos);
    }

    // Opt-in, as it needs a GPU: profiles a HIP program on the server, then opens
    // the trace it wrote through the same server.
    const char* gpu_app = std::getenv("ROCPROFVIS_SERVER_TEST_GPU_APP");
    if(gpu_app && *gpu_app)
    {
        const std::string output_directory = f.TemporaryPath("rocprofv3");
        View::ServerProfilerRunStatus profiled =
            run(kRPVProfilerToolRocprofV3, { "--sys-trace", "--output-format", "rocpd", "-d",
                                             output_directory, "--", gpu_app });
        INFO(profiled.output);
        REQUIRE(profiled.state == kRPVProfilerStateCompleted);
        const std::string marker = "Opened result file: ";
        const size_t      found  = profiled.output.rfind(marker);
        REQUIRE(found != std::string::npos);
        const size_t      begin = found + marker.size();
        const std::string trace =
            profiled.output.substr(begin, profiled.output.find_first_of(" \n", begin) - begin);
        REQUIRE(std::filesystem::exists(trace));

        bool                                is_compute = true;
        std::shared_ptr<View::IDataChannel> channel =
            connection->OpenTrace({ trace }, is_compute, error);
        INFO(error);
        REQUIRE(channel);
        REQUIRE(!is_compute);
        View::DataProvider profiled_trace;
        REQUIRE(profiled_trace.FetchRemoteTrace(channel, "optiq://profiled", false));
        REQUIRE(pump_providers({ &profiled_trace }, [&]() {
            return profiled_trace.GetState() != View::ProviderState::kLoading;
        }));
        REQUIRE(profiled_trace.GetState() == View::ProviderState::kReady);
        REQUIRE(!profiled_trace.DataModel().GetTimeline().GetTrackList().empty());
        profiled_trace.CloseController();
    }
#endif
}

TEST_CASE("Python scripts run on the server with the same result", "[system]")
{
#ifndef ROCPROFVIS_ENABLE_SCRIPTING
    SUCCEED("Built without scripting");
#else
    ParityFixture& f = fixture();
    REQUIRE(f.Load());
    if(f.is_compute)
    {
        SUCCEED("Compute trace");
        return;
    }
    const char* source =
        "count = 0\n"
        "for t in optiq.selection.tracks:\n"
        "    if t.type == optiq.TRACK_TYPE_EVENTS and t.num_entries > 0:\n"
        "        count += len(t.events(start=optiq.selection.start, end=optiq.selection.end))\n"
        "optiq.result.text(str(count))\n";
    std::vector<uint64_t> event_tracks;
    for(const View::TrackInfo* track : tracks_of_type(f.local, kRPVControllerTrackTypeEvents))
    {
        event_tracks.push_back(track->id);
    }
    const double start = f.local.DataModel().GetTimeline().GetStartTime();
    const double end   = f.local.DataModel().GetTimeline().GetEndTime();

    int completions = 0;
    f.remote.SetScriptCompletedCallback(
        [&completions](bool, const std::string&, const std::string&) { completions++; });
    REQUIRE(f.local.ExecuteScript(source, event_tracks, start, end));
    REQUIRE(f.remote.ExecuteScript(source, event_tracks, start, end));
    REQUIRE(f.PumpRequest(View::DataProvider::EXECUTE_SCRIPT_REQUEST_ID));
    f.remote.SetScriptCompletedCallback(nullptr);
    std::string local_text, local_error, remote_text, remote_error;
    const bool  local_ok  = f.local.GetLastScriptResult(local_text, local_error);
    const bool  remote_ok = f.remote.GetLastScriptResult(remote_text, remote_error);
    INFO(local_error << " / " << remote_error);
    REQUIRE(local_ok);
    REQUIRE(remote_ok);
    REQUIRE(completions == 1);
    REQUIRE(!local_text.empty());
    REQUIRE(local_text == remote_text);

    // A server not started with --allow-scripts refuses, and says why.
    Server::server_options_t options;
    options.host        = "127.0.0.1";
    options.port        = 0;
    options.config_path = f.TemporaryPath("no_scripts_config");
    options.token       = PARITY_TOKEN;
    Server::Server server(options);
    REQUIRE(server.Bind());
    struct Listener
    {
        Server::Server& server;
        std::thread     thread;
        ~Listener()
        {
            server.Stop();
            thread.join();
        }
    } listener{ server, std::thread([&server]() { server.Listen(); }) };
    View::ServerConnectionOptions client_options;
    client_options.token = PARITY_TOKEN;
    std::string                             error;
    std::shared_ptr<View::ServerConnection> connection =
        View::ServerConnection::Get("127.0.0.1", server.GetPort(), client_options, error);
    REQUIRE(connection);
    REQUIRE(!connection->GetFeatures().scripts);
    bool                                is_compute = true;
    std::shared_ptr<View::IDataChannel> channel =
        connection->OpenTrace({ f.GetServerTrace() }, is_compute, error);
    REQUIRE(channel);
    View::DataProvider refused;
    REQUIRE(refused.FetchRemoteTrace(channel, "optiq://no-scripts", false));
    REQUIRE(pump_providers({ &refused }, [&]() {
        return refused.GetState() != View::ProviderState::kLoading;
    }));
    REQUIRE(refused.ExecuteScript(source, event_tracks, start, end));
    REQUIRE(pump_providers({ &refused }, [&]() {
        return !refused.IsRequestPending(View::DataProvider::EXECUTE_SCRIPT_REQUEST_ID);
    }));
    std::string refused_text, refused_error;
    REQUIRE(!refused.GetLastScriptResult(refused_text, refused_error));
    REQUIRE(refused_error.find("--allow-scripts") != std::string::npos);
    refused.CloseController();
    connection->Disconnect();
#endif
}

TEST_CASE("TLS connections check the server certificate", "[system]")
{
#ifndef CPPHTTPLIB_SSL_ENABLED
    SUCCEED("Built without TLS");
#else
    ParityFixture& f = fixture();
    REQUIRE(f.Load());
    if(f.is_compute)
    {
        SUCCEED("Compute trace");
        return;
    }
    const std::string certificate = f.TemporaryPath("tls_cert.pem");
    const std::string key         = f.TemporaryPath("tls_key.pem");
    const std::string command =
        "openssl req -x509 -newkey rsa:2048 -nodes -days 1 -subj /CN=localhost "
        "-addext subjectAltName=DNS:localhost,IP:127.0.0.1 -keyout '" +
        key + "' -out '" + certificate + "' > /dev/null 2>&1";
    if(std::system(command.c_str()) != 0)
    {
        SUCCEED("No openssl to make a certificate with");
        return;
    }

    Server::server_options_t options;
    options.host            = "127.0.0.1";
    options.port            = 0;
    options.config_path     = f.TemporaryPath("tls_config");
    options.token           = PARITY_TOKEN;
    options.tls_certificate = certificate;
    options.tls_key         = key;
    Server::Server server(options);
    REQUIRE(server.IsValid());
    REQUIRE(server.Bind());
    // Stopped and joined however the test ends, as a failed REQUIRE throws.
    struct Listener
    {
        Server::Server& server;
        std::thread     thread;
        ~Listener()
        {
            server.Stop();
            thread.join();
        }
    } listener{ server, std::thread([&server]() { server.Listen(); }) };

    View::ServerConnectionOptions client_options;
    client_options.tls     = true;
    client_options.token   = PARITY_TOKEN;
    client_options.ca_file = certificate;
    std::string                             error;
    std::shared_ptr<View::ServerConnection> connection =
        View::ServerConnection::Get("127.0.0.1", server.GetPort(), client_options, error);
    INFO(error);
    CHECK(connection);
    if(connection)
    {
        REQUIRE(connection->IsTls());
        bool                                is_compute = true;
        std::shared_ptr<View::IDataChannel> channel =
            connection->OpenTrace({ f.GetServerTrace() }, is_compute, error);
        REQUIRE(channel);
        View::DataProvider remote_tls;
        REQUIRE(remote_tls.FetchRemoteTrace(channel, "optiqs://tls", false));
        REQUIRE(pump_providers({ &remote_tls }, [&]() {
            return remote_tls.GetState() != View::ProviderState::kLoading;
        }));
        REQUIRE(remote_tls.GetState() == View::ProviderState::kReady);
        require_same_tracks(f.local, remote_tls);

        // Track data, the largest responses, in many TLS records each.
        std::vector<uint64_t> request_ids;
        std::vector<uint64_t> track_ids;
        for(const View::TrackInfo* track :
            tracks_of_type(remote_tls, kRPVControllerTrackTypeEvents))
        {
            View::TrackRequestParams params(
                static_cast<uint32_t>(track->id),
                remote_tls.DataModel().GetTimeline().GetStartTime(),
                remote_tls.DataModel().GetTimeline().GetEndTime(), TRACK_PIXELS, 1, 0, 1);
            std::pair<bool, uint64_t> request = remote_tls.FetchTrack(params);
            if(request.first)
            {
                request_ids.push_back(request.second);
                track_ids.push_back(track->id);
            }
        }
        REQUIRE(!request_ids.empty());
        REQUIRE(pump_providers({ &remote_tls }, [&]() {
            for(uint64_t request_id : request_ids)
            {
                if(remote_tls.IsRequestPending(request_id))
                {
                    return false;
                }
            }
            return true;
        }));
        for(uint64_t track_id : track_ids)
        {
            REQUIRE(remote_tls.DataModel().GetTimeline().GetTrackData(track_id) != nullptr);
        }
        remote_tls.CloseController();
        connection->Disconnect();
    }

    // Without its certificate to trust, a self-signed server is refused...
    client_options.ca_file.clear();
    error.clear();
    CHECK(View::ServerConnection::Get("localhost", server.GetPort(), client_options, error) ==
          nullptr);
    // ...as is plain WebSocket to a TLS port.
    client_options.tls = false;
    error.clear();
    CHECK(View::ServerConnection::Get("localhost", server.GetPort(), client_options, error) ==
          nullptr);
#endif
}
