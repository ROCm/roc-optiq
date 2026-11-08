// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_trace_server.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <limits>
#include <system_error>
#include <utility>

#include "httplib.h"
#include "spdlog/sinks/ringbuffer_sink.h"
#include "spdlog/spdlog.h"

#include "model/compute/rocprofvis_compute_data_model.h"
#include "model/rocprofvis_trace_data_model.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_data_provider_json.h"
#include "rocprofvis_utils.h"
#include "rocprofvis_version.h"
#ifdef ROCPROFVIS_WEB_ASSISTANT
#    include "rocprofvis_assistant_relay.h"
#endif

namespace RocProfVis
{
namespace Server
{

using View::DataProviderJson::TraceKind;

// Long enough for a summary or a table query on a large trace; short enough
// that a stuck provider thread reaches the client as an error rather than a
// hung request.
constexpr std::chrono::seconds PROVIDER_REPLY_TIMEOUT(300);
// How long a request may wait for the provider to accept its fetch, for
// instance while another query of the same table type runs.
constexpr std::chrono::seconds FETCH_START_TIMEOUT(60);

constexpr int32_t     HTTP_OK                     = 200;
constexpr int32_t     HTTP_NO_CONTENT             = 204;
constexpr int32_t     HTTP_BAD_REQUEST            = 400;
constexpr int32_t     HTTP_NOT_FOUND              = 404;
constexpr int32_t     HTTP_CONFLICT               = 409;
constexpr int32_t     HTTP_UNSUPPORTED_MEDIA_TYPE = 415;
constexpr int32_t     HTTP_INTERNAL_ERROR         = 500;
constexpr int32_t     HTTP_UNAVAILABLE            = 503;
constexpr const char* JSON_CONTENT_TYPE           = "application/json";
constexpr size_t      HTTP_WORKER_COUNT           = 8;
constexpr size_t      HTTP_QUEUED_REQUEST_LIMIT   = 64;

// Log lines the log viewer can read back.
constexpr size_t LOG_LINES = 1000;

constexpr uint64_t DEFAULT_HORIZONTAL_PIXELS = 1000;
constexpr uint64_t MAX_HORIZONTAL_PIXELS     = 8192;
constexpr uint64_t DEFAULT_TABLE_ROWS        = 100;
constexpr uint64_t MAX_TABLE_ROWS            = 10000;

// What the file browser lists besides folders: the types the desktop app opens.
constexpr const char* TRACE_EXTENSIONS[] = { ".db",   ".rpd",   ".yaml",
                                             ".json", ".proto", ".pftrace" };

using Params = std::map<std::string, std::string>;

// A route capture or query value as a whole uint64; false for anything else.
static bool
parse_uint64(const std::string& text, uint64_t& value_out)
{
    const char*            last   = text.data() + text.size();
    std::from_chars_result result = std::from_chars(text.data(), last, value_out);
    return !text.empty() && result.ec == std::errc() && result.ptr == last;
}

static bool
parse_uint32(const std::string& text, uint32_t& value_out)
{
    uint64_t value = 0;
    bool     parsed =
        parse_uint64(text, value) && value <= std::numeric_limits<uint32_t>::max();
    if(parsed)
    {
        value_out = static_cast<uint32_t>(value);
    }
    return parsed;
}

static bool
parse_double(const std::string& text, double& value_out)
{
    char*  end   = nullptr;
    double value = std::strtod(text.c_str(), &end);
    bool   parsed =
        !text.empty() && end == text.c_str() + text.size() && std::isfinite(value);
    if(parsed)
    {
        value_out = value;
    }
    return parsed;
}

static std::vector<std::string>
split(const std::string& text, char separator)
{
    std::vector<std::string> parts;
    size_t                   begin = 0;
    while(begin < text.size())
    {
        size_t end = text.find(separator, begin);
        end        = end == std::string::npos ? text.size() : end;
        if(end > begin)
        {
            parts.push_back(text.substr(begin, end - begin));
        }
        begin = end + 1;
    }
    return parts;
}

// A comma-separated list of uint64 values; an empty string is an empty list.
static bool
parse_ids(const std::string& text, std::vector<uint64_t>& ids_out)
{
    bool valid = true;
    for(const std::string& part : split(text, ','))
    {
        uint64_t id = 0;
        valid       = valid && parse_uint64(part, id);
        ids_out.push_back(id);
    }
    return valid;
}

// Optional query values keep the caller's default when absent; false means the
// value is there but malformed.
static bool
read_uint64(const Params& params, const char* name, uint64_t& value)
{
    auto it = params.find(name);
    return it == params.end() || parse_uint64(it->second, value);
}

static bool
read_double(const Params& params, const char* name, double& value)
{
    auto it = params.find(name);
    return it == params.end() || parse_double(it->second, value);
}

static std::string
ascii_lower(const std::string& text)
{
    std::string lower = text;
    for(char& character : lower)
    {
        character =
            static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return lower;
}

static bool
is_loopback_address(const std::string& host)
{
    const std::string normalized = ascii_lower(host);
    return normalized == "localhost" || normalized == "::1" ||
           normalized.rfind("127.", 0) == 0;
}

static bool
get_trace_kind(rocprofvis_controller_t* controller, TraceKind& kind_out)
{
    rocprofvis_controller_object_type_t type = kRPVControllerObjectTypeControllerSystem;
    const rocprofvis_result_t           result =
        rocprofvis_controller_get_object_type(controller, &type);
    const bool valid = result == kRocProfVisResultSuccess &&
                       (type == kRPVControllerObjectTypeControllerSystem ||
                        type == kRPVControllerObjectTypeControllerCompute);
    if(valid)
    {
        kind_out = type == kRPVControllerObjectTypeControllerSystem ? TraceKind::kSystem
                                                                    : TraceKind::kCompute;
    }
    return valid;
}

// The kernel a /workloads/<id>/kernels/<id> route names, or nullptr.
static const View::KernelInfo*
find_kernel(View::DataProvider& provider, const std::vector<std::string>& captures)
{
    uint32_t                workload_id = 0;
    uint32_t                kernel_id   = 0;
    const View::KernelInfo* kernel      = nullptr;
    if(parse_uint32(captures[0], workload_id) && parse_uint32(captures[1], kernel_id))
    {
        kernel = provider.ComputeModel().GetKernelInfo(workload_id, kernel_id);
    }
    return kernel;
}

static bool
summary_loaded(const View::TraceDataModel& model)
{
    const uint64_t revision = model.GetSummary().GetRevision();
    return revision != 0 && revision == model.GetTopology().GetRevision();
}

// The table slots a client can query, the controller table behind each, and
// the request id its fetch occupies.
struct TableRoute
{
    View::TableType                    slot;
    rocprofvis_controller_table_type_t type;
    uint64_t                           request_id;
    bool                               search;
};

static const TableRoute*
find_table_route(View::TableType slot)
{
    using View::DataProvider;
    using View::TableType;
    static const TableRoute ROUTES[] = {
        { TableType::kEventTable, kRPVControllerTableTypeEvents,
          DataProvider::EVENT_TABLE_REQUEST_ID, false },
        { TableType::kSampleTable, kRPVControllerTableTypeSamples,
          DataProvider::SAMPLE_TABLE_REQUEST_ID, false },
        { TableType::kEventSearchTable, kRPVControllerTableTypeSearchResults,
          DataProvider::EVENT_SEARCH_REQUEST_ID, true },
        { TableType::kSummaryKernelTable, kRPVControllerTableTypeSummaryKernelInstances,
          DataProvider::SUMMARY_KERNEL_INSTANCE_TABLE_REQUEST_ID, true },
        { TableType::kSummaryMemoryTable, kRPVControllerTableTypeSummaryMemoryInstances,
          DataProvider::SUMMARY_MEMORY_INSTANCE_TABLE_REQUEST_ID, true },
        { TableType::kSummaryThreadTable, kRPVControllerTableTypeSummaryThreadInstances,
          DataProvider::SUMMARY_THREAD_INSTANCE_TABLE_REQUEST_ID, true },
        { TableType::kAnalysisTopInstrumentedEventsTable,
          kRPVControllerTableTypeInstrumentedEvents,
          DataProvider::ANALYSIS_TOP_INSTRUMENTED_EVENTS_TABLE_REQUEST_ID, false },
        { TableType::kAnalysisTopDispatchEventsTable,
          kRPVControllerTableTypeDispatchEvents,
          DataProvider::ANALYSIS_TOP_DISPATCH_EVENTS_TABLE_REQUEST_ID, false },
        { TableType::kAnalysisTopMemoryAllocationEventsTable,
          kRPVControllerTableTypeMemoryAllocationEvents,
          DataProvider::ANALYSIS_TOP_MEMORY_ALLOCATION_EVENTS_TABLE_REQUEST_ID, false },
        { TableType::kAnalysisTopMemoryCopyEventsTable,
          kRPVControllerTableTypeMemoryCopyEvents,
          DataProvider::ANALYSIS_TOP_MEMORY_COPY_EVENTS_TABLE_REQUEST_ID, false },
        { TableType::kAnalysisTopSampledEventsTable, kRPVControllerTableTypeSampledEvents,
          DataProvider::ANALYSIS_TOP_LAUNCH_SAMPLED_TABLE_REQUEST_ID, false },
    };
    for(const TableRoute& route : ROUTES)
    {
        if(route.slot == slot)
        {
            return &route;
        }
    }
    return nullptr;
}

// The operations a search covers, as the desktop search and summary tabs ask.
static std::vector<rocprofvis_dm_event_operation_t>
search_operations(rocprofvis_controller_table_type_t type)
{
    switch(type)
    {
        case kRPVControllerTableTypeSummaryKernelInstances:
            return { kRocProfVisDmOperationDispatch };
        case kRPVControllerTableTypeSummaryMemoryInstances:
            return { kRocProfVisDmOperationMemoryAllocate,
                     kRocProfVisDmOperationMemoryCopy };
        case kRPVControllerTableTypeSummaryThreadInstances:
            return { kRocProfVisDmOperationLaunch, kRocProfVisDmOperationLaunchSample };
        default:
            return {
                kRocProfVisDmOperationLaunch,       kRocProfVisDmOperationDispatch,
                kRocProfVisDmOperationMemoryCopy,   kRocProfVisDmOperationMemoryAllocate,
                kRocProfVisDmOperationLaunchSample, kRocProfVisDmOperationHipEvent
            };
    }
}

// Paths are fixed against the working directory once, so they still name the
// same file if a later load changes the working directory.
static std::string
absolute_path(const std::string& path)
{
    std::error_code       error;
    std::filesystem::path absolute =
        std::filesystem::absolute(std::filesystem::u8path(path), error);
    return error ? path : absolute.lexically_normal().u8string();
}

static bool
is_trace_file(const std::filesystem::path& path)
{
    const std::string extension = ascii_lower(path.extension().u8string());
    return std::find_if(std::begin(TRACE_EXTENSIONS), std::end(TRACE_EXTENSIONS),
                        [&extension](const char* known) { return extension == known; }) !=
           std::end(TRACE_EXTENSIONS);
}

static std::string
environment(const char* name)
{
#ifdef _WIN32
    char*             value = nullptr;
    size_t            size  = 0;
    const std::string text  = _dupenv_s(&value, &size, name) == 0 && value ? value : "";
    std::free(value);
    return text;
#else
    const char* value = std::getenv(name);
    return value ? value : "";
#endif
}

// Folders and trace files in a folder, folders first, each sorted by name.
static bool
list_folder(const std::filesystem::path& folder, jt::Json& json_out)
{
    namespace fs = std::filesystem;
    std::error_code        error;
    fs::directory_iterator entries(folder, fs::directory_options::skip_permission_denied,
                                   error);
    std::vector<std::pair<std::pair<bool, std::string>, jt::Json>> listed;
    for(; !error && entries != fs::directory_iterator(); entries.increment(error))
    {
        std::error_code   entry_error;
        const bool        is_folder = entries->is_directory(entry_error);
        const fs::path&   path      = entries->path();
        const std::string name      = path.filename().u8string();
        if(!name.empty() && name[0] != '.' && (is_folder || is_trace_file(path)))
        {
            jt::Json entry;
            entry["name"]      = name;
            entry["path"]      = path.u8string();
            entry["is_folder"] = is_folder;
            entry["size"] =
                is_folder ? 0.0 : static_cast<double>(entries->file_size(entry_error));
            listed.emplace_back(std::make_pair(!is_folder, ascii_lower(name)),
                                std::move(entry));
        }
    }
    std::sort(listed.begin(), listed.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    jt::Json list;
    list.setArray();
    for(auto& entry : listed)
    {
        list.getArray().push_back(std::move(entry.second));
    }
    jt::Json roots;
    roots.setArray();
#ifdef _WIN32
    for(char drive = 'A'; drive <= 'Z'; drive++)
    {
        const std::string root = std::string(1, drive) + ":\\";
        std::error_code   root_error;
        if(fs::exists(root, root_error))
        {
            roots.getArray().push_back(root);
        }
    }
    const char* home_variable = "USERPROFILE";
#else
    roots.getArray().push_back("/");
    const char* home_variable = "HOME";
#endif
    // The folders a file dialog offers on the side.
    jt::Json places;
    places.setArray();
    const std::string home = environment(home_variable);
    if(!home.empty())
    {
        for(const char* name : { "", "Desktop", "Documents", "Downloads" })
        {
            const fs::path  place = *name ? fs::u8path(home) / name : fs::u8path(home);
            std::error_code place_error;
            if(fs::is_directory(place, place_error))
            {
                jt::Json entry;
                entry["name"] = *name ? name : "Home";
                entry["path"] = place.u8string();
                places.getArray().push_back(std::move(entry));
            }
        }
    }
    json_out["path"]    = folder.u8string();
    json_out["parent"]  = folder.parent_path().u8string();
    json_out["entries"] = std::move(list);
    json_out["roots"]   = std::move(roots);
    json_out["places"]  = std::move(places);
    return !error;
}

// The server's recent log lines, kept beside its usual output from the first call.
static std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt>
recent_log()
{
    static std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> sink = []() {
        auto ring = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(LOG_LINES);
        spdlog::default_logger()->sinks().push_back(ring);
        return ring;
    }();
    return sink;
}

// A POST body, accepted only as a JSON object sent as application/json. That
// content type forces a CORS preflight, so another site's page cannot post
// here without the server allowing its origin.
static bool
read_json_body(const httplib::Request& request, jt::Json& body_out)
{
    if(request.get_header_value("Content-Type").rfind(JSON_CONTENT_TYPE, 0) != 0)
    {
        return false;
    }
    std::pair<jt::Json::Status, jt::Json> body = jt::Json::parse(request.body);
    body_out                                   = std::move(body.second);
    return body.first == jt::Json::success && body_out.isObject();
}

std::string
TraceServer::RouteArgs::Param(const std::string& name, const std::string& fallback) const
{
    auto it = params.find(name);
    return it == params.end() ? fallback : it->second;
}

TraceServer::TraceServer(View::DataProvider& provider, const std::string& allowed_origin,
                         const std::string& web_root)
: m_provider(provider)
, m_allowed_origin(allowed_origin)
, m_web_root(web_root)
, m_http(std::make_unique<httplib::Server>())
, m_listener_failed(false)
, m_start_attempted(false)
, m_stopping(false)
, m_trace_kind(TraceKind::kSystem)
, m_save_succeeded(false)
, m_cleanup_succeeded(false)
{
    m_http->new_task_queue = []() {
        return new httplib::ThreadPool(HTTP_WORKER_COUNT, HTTP_WORKER_COUNT,
                                       HTTP_QUEUED_REQUEST_LIMIT);
    };
    // The library's default (SO_REUSEPORT, or SO_REUSEADDR on Windows) lets a
    // second process bind the same port and take some of its requests.
    m_http->set_socket_options([](socket_t socket) {
#ifdef _WIN32
        httplib::set_socket_opt(socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, 1);
#else
        httplib::set_socket_opt(socket, SOL_SOCKET, SO_REUSEADDR, 1);
#endif
    });
    m_provider.SetTableDataReadyCallback([this](const std::string&, uint64_t request_id,
                                                uint64_t, const std::string& error) {
        if(error.empty())
        {
            m_table_errors.erase(request_id);
        }
        else
        {
            m_table_errors[request_id] = error;
        }
    });
    recent_log();
    m_provider.SetSaveTraceCallback(
        [this](bool succeeded) { m_save_succeeded = succeeded; });
    m_provider.SetCleanupDatabaseCallback(
        [this](bool succeeded) { m_cleanup_succeeded = succeeded; });
    RegisterRoutes();
}

TraceServer::~TraceServer() { Stop(); }

bool
TraceServer::Start(const std::string& host, uint16_t port)
{
    if(!is_loopback_address(host))
    {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if(m_start_attempted || m_stopping)
        {
            return false;
        }
        m_start_attempted = true;
    }

    // Any loopback name reaches a loopback listener, so a browser may use any.
    const std::string bound_host = ascii_lower(host);
    for(const std::string& name :
        { bound_host.find(':') == std::string::npos ? bound_host : "[" + bound_host + "]",
          std::string("localhost"), std::string("127.0.0.1"), std::string("[::1]") })
    {
        m_accepted_hosts.push_back(name);
        m_accepted_hosts.push_back(name + ":" + std::to_string(port));
    }

    bool bound = m_http->bind_to_port(host, port);
    if(bound)
    {
        try
        {
            m_listener = std::thread([this]() {
                bool listened = false;
                try
                {
                    listened = m_http->listen_after_bind();
                } catch(const std::exception& error)
                {
                    spdlog::error("HTTP listener failed: {}", error.what());
                } catch(...)
                {
                    spdlog::error("HTTP listener failed with an unknown error.");
                }
                if(!listened)
                {
                    m_http->decommission();
                    m_listener_failed.store(true);
                    m_task_queued.notify_all();
                }
            });
        } catch(const std::exception& error)
        {
            spdlog::error("Could not start the HTTP listener thread: {}", error.what());
            m_http->stop();
            bound = false;
        }
        if(bound)
        {
            m_http->wait_until_ready();
            if(!m_http->is_running())
            {
                m_listener_failed.store(true);
                if(m_listener.joinable())
                {
                    m_listener.join();
                }
                bound = false;
            }
        }
    }
    return bound;
}

void
TraceServer::Stop()
{
    std::deque<Task> abandoned;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopping = true;
        abandoned.swap(m_tasks);
    }
    m_task_queued.notify_all();
    for(Task& task : abandoned)
    {
        m_active.push_back(std::move(task));
    }
    for(Task& task : m_active)
    {
        if(!task.cancelled->load())
        {
            task.reply->set_value(
                Error(HTTP_UNAVAILABLE, "The server is shutting down."));
        }
    }
    m_active.clear();
    m_http->stop();
    if(m_listener.joinable())
    {
        m_listener.join();
    }
}

bool
TraceServer::Open(const std::string& trace_path, std::string& error_out)
{
    const std::string        path        = absolute_path(trace_path);
    const std::string        config_path = View::get_application_config_path(true);
    rocprofvis_controller_t* controller =
        rocprofvis_controller_alloc(path.c_str(), config_path.c_str());
    TraceKind kind   = TraceKind::kSystem;
    bool      opened = false;
    if(!controller)
    {
        error_out = "Could not open '" + path + "' as a trace.";
    }
    else if(!get_trace_kind(controller, kind))
    {
        error_out = "'" + path + "' is not a supported system or compute trace.";
        rocprofvis_controller_free(controller);
    }
    // The provider owns the controller from here, and frees it on failure.
    else if(!m_provider.FetchTrace(controller, path))
    {
        error_out = "Could not start loading '" + path + "'.";
    }
    else
    {
        m_trace_kind = kind;
        m_track_groups.clear();
        m_table_errors.clear();
        opened = true;
    }
    return opened;
}

void
TraceServer::RunPendingRequests()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for(Task& task : m_tasks)
        {
            m_active.push_back(std::move(task));
        }
        m_tasks.clear();
    }
    for(auto task = m_active.begin(); task != m_active.end();)
    {
        task = task->cancelled->load() || Advance(*task) ? m_active.erase(task)
                                                         : std::next(task);
    }
}

bool
TraceServer::Advance(Task& task)
{
    bool  answered = true;
    Reply reply    = Error(HTTP_INTERNAL_ERROR, "The request could not be completed.");
    try
    {
        if(!task.made)
        {
            task.job  = task.make_job();
            task.made = true;
        }
        task.started = task.started || !task.job.start || task.job.start();
        if(!task.started)
        {
            answered =
                std::chrono::steady_clock::now() - task.queued_at > FETCH_START_TIMEOUT;
            reply = Error(HTTP_UNAVAILABLE, "The provider is busy; try again.");
        }
        else if(task.job.ready && !task.job.ready())
        {
            answered = false;
        }
        else
        {
            reply = task.job.reply();
        }
    } catch(const std::exception& error)
    {
        spdlog::error("Trace server provider task failed: {}", error.what());
    } catch(...)
    {
        spdlog::error("Trace server provider task failed with an unknown error.");
    }
    if(answered)
    {
        task.reply->set_value(std::move(reply));
    }
    return answered;
}

void
TraceServer::WaitForRequests(std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_task_queued.wait_for(lock, timeout, [this]() {
        return m_stopping || m_listener_failed.load() || !m_tasks.empty();
    });
}

bool
TraceServer::ListenerFailed() const
{
    return m_listener_failed.load();
}

TraceServer::Reply
TraceServer::OnProviderThread(std::function<Job()> make_job)
{
    Task task;
    task.make_job  = std::move(make_job);
    task.reply     = std::make_shared<std::promise<Reply>>();
    task.cancelled = std::make_shared<std::atomic<bool>>(false);
    task.queued_at = std::chrono::steady_clock::now();
    std::shared_ptr<std::atomic<bool>> cancelled = task.cancelled;
    std::future<Reply>                 reply     = task.reply->get_future();
    bool                               queued    = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if(!m_stopping)
        {
            m_tasks.push_back(std::move(task));
            queued = true;
        }
    }

    Reply result = Error(HTTP_UNAVAILABLE, "The server is shutting down.");
    if(queued)
    {
        m_task_queued.notify_one();
        if(reply.wait_for(PROVIDER_REPLY_TIMEOUT) == std::future_status::ready)
        {
            result = reply.get();
        }
        else
        {
            cancelled->store(true);
            result =
                Error(HTTP_UNAVAILABLE, "The trace provider did not answer in time.");
        }
    }
    return result;
}

bool
TraceServer::IsExpectedHost(const std::string& host) const
{
    return std::find(m_accepted_hosts.begin(), m_accepted_hosts.end(),
                     ascii_lower(host)) != m_accepted_hosts.end();
}

void
TraceServer::Route(Method method, const std::string& pattern, Handler handler)
{
    httplib::Server::Handler http_handler = [this, method,
                                             handler](const httplib::Request& request,
                                                      httplib::Response&      response) {
        RouteArgs args;
        for(size_t i = 1; i < request.matches.size(); i++)
        {
            args.captures.push_back(request.matches[i].str());
        }
        for(const std::pair<const std::string, std::string>& param : request.params)
        {
            args.params.emplace(param.first, param.second);
        }

        Reply reply = Error(HTTP_UNSUPPORTED_MEDIA_TYPE,
                            "Send a JSON object with Content-Type: application/json.");
        bool  valid = method == Method::kGet || read_json_body(request, args.body);
        if(valid)
        {
            reply = OnProviderThread(
                [this, handler, args]() { return handler(m_provider, args); });
        }
        response.status = reply.status;
        response.set_content(request.has_param("pretty") ? reply.body.toStringPretty()
                                                         : reply.body.toString(),
                             JSON_CONTENT_TYPE);
    };
    if(method == Method::kGet)
    {
        m_http->Get(pattern, http_handler);
    }
    else
    {
        m_http->Post(pattern, http_handler);
    }
}

TraceServer::Handler
TraceServer::For(TraceKind kind, Handler handler) const
{
    return [this, kind, handler](View::DataProvider& provider, const RouteArgs& args) {
        Job job = Now(Error(HTTP_CONFLICT,
                            std::string("The open trace is not a ") +
                                View::DataProviderJson::TraceKindName(kind) + " trace."));
        if(provider.GetState() != View::ProviderState::kReady)
        {
            job = Now(Error(HTTP_UNAVAILABLE, "No trace is ready."));
        }
        else if(m_trace_kind == kind)
        {
            job = handler(provider, args);
        }
        return job;
    };
}

TraceServer::Job
TraceServer::Now(Reply reply)
{
    Job job;
    job.reply = [reply]() { return reply; };
    return job;
}

TraceServer::Reply
TraceServer::Ok(jt::Json body)
{
    return Reply{ HTTP_OK, std::move(body) };
}

TraceServer::Reply
TraceServer::Error(int32_t status, const std::string& message)
{
    jt::Json body;
    body["error"] = message;
    return Reply{ status, std::move(body) };
}

void
TraceServer::RegisterRoutes()
{
    m_http->set_pre_routing_handler(
        [this](const httplib::Request& request, httplib::Response& response) {
            if(request.get_header_value_count("Host") == 1 &&
               IsExpectedHost(request.get_header_value("Host")))
            {
                return httplib::Server::HandlerResponse::Unhandled;
            }

            Reply reply     = Error(HTTP_BAD_REQUEST, "Unexpected Host header.");
            response.status = reply.status;
            response.set_content(reply.body.toString(), JSON_CONTENT_TYPE);
            return httplib::Server::HandlerResponse::Handled;
        });
    m_http->set_exception_handler([](const httplib::Request&, httplib::Response& response,
                                     std::exception_ptr error) {
        try
        {
            if(error)
            {
                std::rethrow_exception(error);
            }
        } catch(const std::exception& exception)
        {
            spdlog::error("Trace server request failed: {}", exception.what());
        } catch(...)
        {
            spdlog::error("Trace server request failed with an unknown error.");
        }
        Reply reply = Error(HTTP_INTERNAL_ERROR, "The request could not be completed.");
        response.status = reply.status;
        response.set_content(reply.body.toString(), JSON_CONTENT_TYPE);
    });
    m_http->set_post_routing_handler(
        [this](const httplib::Request& request, httplib::Response& response) {
            if(request.path.rfind("/api/", 0) == 0)
            {
                response.set_header("Cache-Control", "no-store");
            }
            if(!m_allowed_origin.empty())
            {
                response.set_header("Access-Control-Allow-Origin", m_allowed_origin);
                response.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
                response.set_header("Access-Control-Allow-Headers", "Content-Type");
                response.set_header("Vary", "Origin");
            }
        });
    m_http->Options(R"(/api/.*)",
                    [](const httplib::Request&, httplib::Response& response) {
                        response.status = HTTP_NO_CONTENT;
                    });
    // Requests that match no route still get a JSON body.
    m_http->set_error_handler([](const httplib::Request&, httplib::Response& response) {
        if(response.body.empty())
        {
            jt::Json body;
            body["error"] = httplib::status_message(response.status);
            response.set_content(body.toString(), JSON_CONTENT_TYPE);
        }
    });
    if(!m_web_root.empty() && !m_http->set_mount_point("/", absolute_path(m_web_root)))
    {
        spdlog::warn("'{}' is not a folder; the web app is not served.", m_web_root);
    }

    Route(Method::kGet, "/api/status",
          [this](View::DataProvider& provider, const RouteArgs&) {
              jt::Json status   = View::DataProviderJson::Status(provider, m_trace_kind);
              status["version"] = std::to_string(ROCPROFVIS_VERSION_MAJOR) + "." +
                                  std::to_string(ROCPROFVIS_VERSION_MINOR) + "." +
                                  std::to_string(ROCPROFVIS_VERSION_PATCH);
              return Now(Ok(std::move(status)));
          });
    Route(Method::kGet, "/api/snapshot",
          [this](View::DataProvider& provider, const RouteArgs&) {
              return Now(Ok(View::DataProviderJson::Snapshot(provider, m_trace_kind)));
          });
    Route(Method::kGet, "/api/files",
          [](View::DataProvider& provider, const RouteArgs& args) {
              namespace fs = std::filesystem;
              std::error_code error;
              fs::path        folder = fs::u8path(args.Param("path"));
              if(folder.empty())
              {
                  const std::string& trace = provider.GetTraceFilePath();
                  folder = trace.empty() ? fs::current_path(error)
                                         : fs::u8path(trace).parent_path();
              }
              folder = fs::absolute(folder, error).lexically_normal();
              jt::Json listing;
              return Now(!error && list_folder(folder, listing)
                             ? Ok(std::move(listing))
                             : Error(HTTP_NOT_FOUND, "Could not read that folder."));
          });
    Route(Method::kPost, "/api/open",
          [this](View::DataProvider& provider, const RouteArgs& args) {
              jt::Json    body = args.body;
              std::string error;
              if(!body["path"].isString() || body["path"].getString().empty())
              {
                  return Now(
                      Error(HTTP_BAD_REQUEST, "Send {\"path\": \"<trace file>\"}."));
              }
              return Now(Open(body["path"].getString(), error)
                             ? Ok(View::DataProviderJson::Status(provider, m_trace_kind))
                             : Error(HTTP_BAD_REQUEST, error));
          });
    m_http->Get("/api/logs", [](const httplib::Request&, httplib::Response& response) {
        jt::Json lines;
        lines.setArray();
        for(std::string& line : recent_log()->last_formatted())
        {
            lines.getArray().push_back(std::move(line));
        }
        jt::Json json;
        json["lines"] = std::move(lines);
        response.set_content(json.toString(), JSON_CONTENT_TYPE);
    });
#ifdef ROCPROFVIS_WEB_ASSISTANT
    // Ask Optiq never reads the provider, so it answers on the HTTP worker
    // rather than queueing behind trace work.
    m_http->Get("/api/assistant",
                [](const httplib::Request&, httplib::Response& response) {
                    response.set_content(AssistantStatus().toString(), JSON_CONTENT_TYPE);
                });
    m_http->Post("/api/assistant/chat",
                 [](const httplib::Request& request, httplib::Response& response) {
                     jt::Json body;
                     Reply    reply = Error(HTTP_UNSUPPORTED_MEDIA_TYPE,
                                            "Send a JSON object with Content-Type: "
                                            "application/json.");
                     if(read_json_body(request, body))
                     {
                         reply.body = AssistantChat(std::move(body), reply.status);
                     }
                     response.status = reply.status;
                     response.set_content(reply.body.toString(), JSON_CONTENT_TYPE);
                 });
#endif
    RegisterSystemRoutes();
    RegisterComputeRoutes();
}

void
TraceServer::RegisterSystemRoutes()
{
    const TraceKind kind = TraceKind::kSystem;
    Route(Method::kGet, "/api/timeline",
          For(kind, [](View::DataProvider& provider, const RouteArgs&) {
              return Now(Ok(View::DataProviderJson::Timeline(provider.DataModel())));
          }));
    Route(Method::kGet, "/api/histogram",
          For(kind, [](View::DataProvider& provider, const RouteArgs&) {
              return Now(Ok(
                  View::DataProviderJson::Histogram(provider.DataModel().GetTimeline())));
          }));
    Route(Method::kGet, "/api/minimap",
          For(kind, [](View::DataProvider& provider, const RouteArgs&) {
              return Now(Ok(
                  View::DataProviderJson::Minimap(provider.DataModel().GetTimeline())));
          }));
    Route(Method::kGet, "/api/topology",
          For(kind, [](View::DataProvider& provider, const RouteArgs&) {
              return Now(Ok(
                  View::DataProviderJson::Topology(provider.DataModel().GetTopology())));
          }));
    Route(Method::kGet, R"(/api/tracks/(\d+))",
          For(kind, [](View::DataProvider& provider, const RouteArgs& args) {
              uint64_t track_id = 0;
              jt::Json track;
              if(parse_uint64(args.captures[0], track_id))
              {
                  track = View::DataProviderJson::Track(provider.DataModel(), track_id);
              }
              return Now(track.isNull() ? Error(HTTP_NOT_FOUND, "No track with that id.")
                                        : Ok(std::move(track)));
          }));

    // Queue utilization or counter statistics over [start, end].
    Route(Method::kGet, R"(/api/tracks/(\d+)/statistics)",
          For(kind, [](View::DataProvider& provider, const RouteArgs& args) {
              const View::TimelineModel& timeline = provider.DataModel().GetTimeline();
              uint64_t                   track_id = 0;
              double                     start    = timeline.GetStartTime();
              double                     end      = timeline.GetEndTime();
              const View::TrackInfo*     track = parse_uint64(args.captures[0], track_id)
                                                     ? timeline.GetTrack(track_id)
                                                     : nullptr;
              const bool                 valid =
                  track &&
                  (track->topology.type == View::TrackInfo::TrackType::Queue ||
                   track->topology.type == View::TrackInfo::TrackType::Counter) &&
                  read_double(args.params, "start", start) &&
                  read_double(args.params, "end", end) && start < end;
              if(!valid)
              {
                  return Now(
                      Error(HTTP_BAD_REQUEST,
                            "Statistics need a queue or counter track and start < end."));
              }
              const uint64_t request_id = View::RequestIdBuilder::MakeTrackDataRequestId(
                  static_cast<uint32_t>(track_id), 0, 0,
                  View::RequestType::kFetchAnalysisTrackStatistics);
              Job job;
              job.start = [&provider, track_id, start, end]() {
                  const View::TrackInfo* track =
                      provider.DataModel().GetTimeline().GetTrack(track_id);
                  const View::AnalysisTrackStatistics* statistics =
                      track ? provider.DataModel().GetAnalysis().RegisterTrack(*track)
                            : nullptr;
                  const bool queued =
                      statistics && provider.FetchAnalysisTrackStatistics(
                                        View::AnalysisTrackStatisticsRequestParams(
                                            track_id, start, end));
                  if(queued)
                  {
                      statistics->state = View::AnalysisTrackStatistics::kRequested;
                  }
                  return queued;
              };
              job.ready = [&provider, request_id]() {
                  return !provider.IsRequestPending(request_id);
              };
              job.reply = [&provider, track_id]() {
                  const View::TrackInfo* track =
                      provider.DataModel().GetTimeline().GetTrack(track_id);
                  return track ? Ok(View::DataProviderJson::TrackStatistics(
                                     *provider.DataModel().GetAnalysis().RegisterTrack(
                                         *track)))
                               : Error(HTTP_NOT_FOUND, "No track with that id.");
              };
              return job;
          }));

    // Events or samples for several tracks over [start, end], merged to px pixels.
    Route(
        Method::kGet, "/api/track_data",
        For(kind, [this](View::DataProvider& provider, const RouteArgs& args) {
            const View::TimelineModel& timeline = provider.DataModel().GetTimeline();
            std::vector<uint64_t>      track_ids;
            double                     start  = timeline.GetStartTime();
            double                     end    = timeline.GetEndTime();
            uint64_t                   pixels = DEFAULT_HORIZONTAL_PIXELS;
            bool valid = parse_ids(args.Param("tracks"), track_ids) &&
                         !track_ids.empty() && read_double(args.params, "start", start) &&
                         read_double(args.params, "end", end) &&
                         read_uint64(args.params, "px", pixels) && start < end &&
                         pixels > 0 && pixels <= MAX_HORIZONTAL_PIXELS;
            for(uint64_t track_id : track_ids)
            {
                valid = valid && track_id <= std::numeric_limits<uint32_t>::max() &&
                        timeline.GetTrack(track_id);
            }
            if(!valid)
            {
                return Now(Error(HTTP_BAD_REQUEST,
                                 "track_data takes tracks=<id,...> of known "
                                 "tracks, start < end and 0 < px <= 8192."));
            }
            auto request_ids = std::make_shared<std::vector<uint64_t>>();
            Job  job;
            job.start = [this, &provider, track_ids, start, end, pixels, request_ids]() {
                for(uint64_t track_id : track_ids)
                {
                    // A new group replaces the track's data rather than adding to it.
                    std::pair<bool, uint64_t> fetch = provider.FetchTrack(
                        static_cast<uint32_t>(track_id), start, end,
                        static_cast<uint32_t>(pixels), ++m_track_groups[track_id]);
                    if(fetch.first)
                    {
                        request_ids->push_back(fetch.second);
                    }
                }
                return true;
            };
            job.ready = [&provider, request_ids]() {
                return std::none_of(
                    request_ids->begin(), request_ids->end(),
                    [&provider](uint64_t id) { return provider.IsRequestPending(id); });
            };
            job.reply = [&provider, track_ids]() {
                jt::Json tracks;
                tracks.setArray();
                for(uint64_t track_id : track_ids)
                {
                    jt::Json data = View::DataProviderJson::TrackData(
                        provider.DataModel().GetTimeline(), track_id);
                    if(!data.isNull())
                    {
                        tracks.getArray().push_back(std::move(data));
                    }
                }
                jt::Json json;
                json["tracks"] = std::move(tracks);
                return Ok(std::move(json));
            };
            return job;
        }));

    // An event's details, flows and call stack.
    Route(Method::kGet, R"(/api/events/(\d+))",
          For(kind, [](View::DataProvider& provider, const RouteArgs& args) {
              uint64_t uuid     = 0;
              uint64_t track_id = 0;
              if(!parse_uint64(args.captures[0], uuid) ||
                 !parse_uint64(args.Param("track"), track_id) ||
                 !provider.DataModel().GetTimeline().GetTrack(track_id))
              {
                  return Now(Error(HTTP_BAD_REQUEST,
                                   "Address an event as /api/events/<uuid>?track=<id>."));
              }
              const View::RequestType parts[] = {
                  View::RequestType::kFetchEventExtendedData,
                  View::RequestType::kFetchEventFlowDetails,
                  View::RequestType::kFetchEventCallStack
              };
              Job job;
              job.start = [&provider, uuid, track_id]() {
                  provider.FetchEvent(track_id, uuid);
                  provider.FetchEventFlowDetails(uuid);
                  provider.FetchEventCallStackData(uuid);
                  return true;
              };
              job.ready = [&provider, uuid, parts]() {
                  return std::none_of(
                      std::begin(parts), std::end(parts), [&](View::RequestType part) {
                          return provider.IsRequestPending(
                              View::RequestIdBuilder::MakeEventDataRequestId(uuid, part));
                      });
              };
              job.reply = [&provider, uuid]() {
                  View::EventModel& events = provider.DataModel().GetEvents();
                  jt::Json          event  = View::DataProviderJson::Event(events, uuid);
                  events.RemoveEvent(uuid);
                  return event.isNull()
                             ? Error(HTTP_NOT_FOUND, "No event with that uuid.")
                             : Ok(std::move(event));
              };
              return job;
          }));

    // A page of a table. Track tables take tracks, filter, group and
    // group_cols; searches take newline-separated terms and the match options;
    // either can be scoped with node, agent, process and thread ids.
    Route(Method::kGet, R"(/api/tables/([a-z_]+))",
          For(kind, [this](View::DataProvider& provider, const RouteArgs& args) {
              View::TableType   slot = View::TableType::__kTableTypeCount;
              const TableRoute* table =
                  View::DataProviderJson::TableTypeFromName(args.captures[0], slot)
                      ? find_table_route(slot)
                      : nullptr;
              if(!table)
              {
                  return Now(Error(HTTP_NOT_FOUND, "No table with that name."));
              }

              const View::TimelineModel& timeline = provider.DataModel().GetTimeline();
              std::vector<uint64_t>      track_ids;
              double                     start  = timeline.GetStartTime();
              double                     end    = timeline.GetEndTime();
              uint64_t                   offset = 0;
              uint64_t                   limit  = DEFAULT_TABLE_ROWS;
              uint64_t                   sort   = 0;
              const std::string          order  = args.Param("order", "asc");
              bool valid = parse_ids(args.Param("tracks"), track_ids) &&
                           (table->search || !track_ids.empty()) &&
                           read_double(args.params, "start", start) &&
                           read_double(args.params, "end", end) &&
                           read_uint64(args.params, "offset", offset) &&
                           read_uint64(args.params, "limit", limit) &&
                           read_uint64(args.params, "sort", sort) && limit > 0 &&
                           limit <= MAX_TABLE_ROWS && (order == "asc" || order == "desc");

              rocprofvis_dm_query_criteria_t criteria;
              bool                           scoped = false;
              for(auto scope : { std::make_pair("node", &criteria.node_id),
                                 std::make_pair("agent", &criteria.agent_id),
                                 std::make_pair("process", &criteria.process_id),
                                 std::make_pair("thread", &criteria.thread_id) })
              {
                  uint64_t id = 0;
                  if(args.params.count(scope.first))
                  {
                      valid         = valid && parse_uint64(args.Param(scope.first), id);
                      *scope.second = c_optional_uint64_t(id);
                      scoped        = true;
                  }
              }
              if(!valid)
              {
                  return Now(
                      Error(HTTP_BAD_REQUEST,
                            "Check tracks, start, end, offset, limit (1-10000), sort, "
                            "order (asc or desc) and the scope ids."));
              }

              const rocprofvis_controller_sort_order_t sort_order =
                  order == "desc" ? kRPVControllerSortOrderDescending
                                  : kRPVControllerSortOrderAscending;
              std::shared_ptr<View::TableRequestParams> params;
              if(table->search)
              {
                  params = std::make_shared<View::EventSearchRequestParams>(
                      table->type, search_operations(table->type), start, end, nullptr,
                      args.Param("substrings", "1") == "1",
                      args.Param("category", "0") == "1",
                      args.Param("partial", "0") == "1", split(args.Param("terms"), '\n'),
                      offset, limit, sort, sort_order);
              }
              else
              {
                  params = std::make_shared<View::TrackTableRequestParams>(
                      table->type, track_ids, start, end, nullptr,
                      args.Param("filter").c_str(), args.Param("group").c_str(),
                      args.Param("group_cols").c_str(), offset, limit, sort, sort_order);
              }
              if(scoped)
              {
                  params->m_source_filter = criteria;
              }

              Job job;
              job.start = [&provider, params]() { return provider.FetchTable(*params); };
              job.ready = [&provider, table]() {
                  return !provider.IsTableRequestPending(table->type);
              };
              job.reply = [this, &provider, table]() {
                  jt::Json json =
                      View::DataProviderJson::Table(provider.DataModel(), table->slot);
                  auto error = m_table_errors.find(table->request_id);
                  if(error != m_table_errors.end())
                  {
                      json["error"] = error->second;
                  }
                  return Ok(std::move(json));
              };
              return job;
          }));

    Route(
        Method::kGet, "/api/summary",
        For(kind, [](View::DataProvider& provider, const RouteArgs&) {
            Job job;
            job.start = [&provider]() {
                return summary_loaded(provider.DataModel()) ||
                       provider.IsRequestPending(
                           View::DataProvider::SUMMARY_REQUEST_ID) ||
                       provider.FetchSummary();
            };
            job.ready = [&provider]() {
                return !provider.IsRequestPending(View::DataProvider::SUMMARY_REQUEST_ID);
            };
            job.reply = [&provider]() {
                jt::Json summary = View::DataProviderJson::Summary(provider.DataModel());
                return summary.isNull() ? Error(HTTP_INTERNAL_ERROR,
                                                "The summary could not be loaded.")
                                        : Ok(std::move(summary));
            };
            return job;
        }));

    // Writes [start_ns, end_ns] of the trace to a new database at path.
    Route(Method::kPost, "/api/save_selection",
          For(kind, [this](View::DataProvider& provider, const RouteArgs& args) {
              jt::Json body = args.body;
              if(!body["path"].isString() || body["path"].getString().empty() ||
                 !body["start_ns"].isNumber() || !body["end_ns"].isNumber() ||
                 !(body["start_ns"].getNumber() < body["end_ns"].getNumber()))
              {
                  return Now(Error(HTTP_BAD_REQUEST,
                                   "Send {\"path\", \"start_ns\", \"end_ns\"}."));
              }
              const std::string path  = body["path"].getString();
              const double      start = body["start_ns"].getNumber();
              const double      end   = body["end_ns"].getNumber();
              Job               job;
              job.start = [this, &provider, path, start, end]() {
                  m_save_succeeded = false;
                  return provider.SaveTrimmedTrace(path, start, end);
              };
              job.ready = [&provider]() {
                  return !provider.IsRequestPending(
                      View::DataProvider::SAVE_TRIMMED_TRACE_REQUEST_ID);
              };
              job.reply = [this, path]() {
                  jt::Json json;
                  json["path"] = path;
                  return m_save_succeeded
                             ? Ok(std::move(json))
                             : Error(HTTP_INTERNAL_ERROR, "Could not save " + path + ".");
              };
              return job;
          }));

    // Rebuilds the trace database's indexes, or with rebuild also vacuums it.
    Route(Method::kPost, "/api/cleanup",
          For(kind, [this](View::DataProvider& provider, const RouteArgs& args) {
              jt::Json   body    = args.body;
              const bool rebuild = body["rebuild"].isBool() && body["rebuild"].getBool();
              Job        job;
              job.start = [this, &provider, rebuild]() {
                  m_cleanup_succeeded = false;
                  return provider.CleanupDatabase(rebuild);
              };
              job.ready = [&provider]() {
                  return !provider.IsRequestPending(
                      View::DataProvider::CLEANUP_DATABASE_REQUEST_ID);
              };
              job.reply = [this]() {
                  jt::Json json;
                  json["succeeded"] = m_cleanup_succeeded;
                  return m_cleanup_succeeded
                             ? Ok(std::move(json))
                             : Error(HTTP_INTERNAL_ERROR, "The database cleanup failed.");
              };
              return job;
          }));
}

void
TraceServer::RegisterComputeRoutes()
{
    const TraceKind kind = TraceKind::kCompute;
    Route(Method::kGet, "/api/compute",
          For(kind, [](View::DataProvider& provider, const RouteArgs&) {
              return Now(Ok(View::DataProviderJson::Compute(provider.ComputeModel())));
          }));
    Route(Method::kGet, R"(/api/compute/workloads/(\d+))",
          For(kind, [](View::DataProvider& provider, const RouteArgs& args) {
              uint32_t                  workload_id = 0;
              const View::WorkloadInfo* workload =
                  parse_uint32(args.captures[0], workload_id)
                      ? provider.ComputeModel().GetWorkload(workload_id)
                      : nullptr;
              return Now(workload ? Ok(View::DataProviderJson::Workload(*workload))
                                  : Error(HTTP_NOT_FOUND, "No workload with that id."));
          }));
    Route(Method::kGet, R"(/api/compute/workloads/(\d+)/kernels/(\d+))",
          For(kind, [](View::DataProvider& provider, const RouteArgs& args) {
              const View::KernelInfo* kernel = find_kernel(provider, args.captures);
              return Now(kernel ? Ok(View::DataProviderJson::Kernel(*kernel))
                                : Error(HTTP_NOT_FOUND,
                                        "No kernel with that id in that workload."));
          }));

    // Metric values for kernels, or for the workload when kernels is empty.
    // metrics lists category, category.table or category.table.entry ids;
    // client keeps one caller's values apart from another's.
    Route(Method::kGet, R"(/api/compute/workloads/(\d+)/metrics)",
          For(kind, [](View::DataProvider& provider, const RouteArgs& args) {
              uint32_t                                          workload_id = 0;
              uint64_t                                          client_id   = 0;
              std::vector<uint64_t>                             kernel_ids;
              std::vector<View::MetricsRequestParams::MetricID> metric_ids;
              bool valid = parse_uint32(args.captures[0], workload_id) &&
                           provider.ComputeModel().GetWorkload(workload_id) &&
                           parse_ids(args.Param("kernels"), kernel_ids) &&
                           read_uint64(args.params, "client", client_id);
              for(const std::string& metric : split(args.Param("metrics"), ','))
              {
                  std::vector<std::string> parts = split(metric, '.');
                  std::vector<uint32_t>    ids(parts.size(), 0);
                  for(size_t i = 0; i < parts.size(); i++)
                  {
                      valid = valid && parse_uint32(parts[i], ids[i]);
                  }
                  valid = valid && !ids.empty() && ids.size() <= 3;
                  if(valid)
                  {
                      metric_ids.push_back(View::MetricsRequestParams::MetricID{
                          ids[0],
                          ids.size() > 1 ? std::optional<uint32_t>(ids[1]) : std::nullopt,
                          ids.size() > 2 ? std::optional<uint32_t>(ids[2])
                                         : std::nullopt });
                  }
              }
              std::vector<uint32_t> kernels;
              for(uint64_t kernel_id : kernel_ids)
              {
                  valid = valid && kernel_id <= std::numeric_limits<uint32_t>::max() &&
                          provider.ComputeModel().GetKernelInfo(
                              workload_id, static_cast<uint32_t>(kernel_id));
                  kernels.push_back(static_cast<uint32_t>(kernel_id));
              }
              if(!valid || metric_ids.empty())
              {
                  return Now(Error(HTTP_BAD_REQUEST,
                                   "metrics takes metrics=<c[.t[.e]],...>, "
                                   "kernels=<id,...> of the workload and client."));
              }
              Job job;
              job.start = [&provider, workload_id, kernels, metric_ids, client_id]() {
                  View::ComputeDataModel& model = provider.ComputeModel();
                  if(kernels.empty())
                  {
                      model.ClearWorkloadMetricValues(client_id, workload_id);
                  }
                  for(uint32_t kernel_id : kernels)
                  {
                      model.ClearKernelMetricValues(client_id, kernel_id);
                  }
                  return provider.FetchMetrics(View::MetricsRequestParams(
                      workload_id, kernels, metric_ids, client_id));
              };
              job.ready = [&provider, client_id]() {
                  return !provider.IsRequestPending(
                      View::RequestIdBuilder::MakeClientRequestId(
                          View::RequestType::kFetchMetrics, client_id));
              };
              job.reply = [&provider, workload_id, kernels, client_id]() {
                  const View::ComputeDataModel& model    = provider.ComputeModel();
                  const View::WorkloadInfo*     workload = model.GetWorkload(workload_id);
                  jt::Json                      list;
                  list.setArray();
                  for(uint32_t kernel_id : kernels)
                  {
                      list.getArray().push_back(View::DataProviderJson::KernelMetrics(
                          model, client_id, kernel_id));
                  }
                  jt::Json json;
                  json["workload"] = kernels.empty() && workload
                                         ? View::DataProviderJson::WorkloadMetrics(
                                               model, client_id, *workload)
                                         : jt::Json();
                  json["kernels"]  = std::move(list);
                  return Ok(std::move(json));
              };
              return job;
          }));

    // The kernel table with a column per selector ("c.t.e:value_name"),
    // sorted by column sort and filtered by filter_<column>=<expression>.
    Route(Method::kGet, R"(/api/compute/workloads/(\d+)/kernel_table)",
          For(kind, [this](View::DataProvider& provider, const RouteArgs& args) {
              uint32_t          workload_id = 0;
              uint64_t          sort        = 1;
              const std::string order       = args.Param("order", "desc");
              std::unordered_map<uint64_t, std::string> filters;
              bool valid = parse_uint32(args.captures[0], workload_id) &&
                           provider.ComputeModel().GetWorkload(workload_id) &&
                           read_uint64(args.params, "sort", sort) &&
                           (order == "asc" || order == "desc");
              for(const std::pair<const std::string, std::string>& param : args.params)
              {
                  uint64_t column = 0;
                  if(param.first.rfind("filter_", 0) == 0)
                  {
                      valid = valid && parse_uint64(param.first.substr(7), column);
                      filters[column] = param.second;
                  }
              }
              if(!valid)
              {
                  return Now(Error(HTTP_BAD_REQUEST,
                                   "Check the workload, sort, order and filters."));
              }
              View::ComputeTableRequestParams params(
                  workload_id, split(args.Param("metrics"), ','), sort,
                  order == "desc" ? kRPVControllerSortOrderDescending
                                  : kRPVControllerSortOrderAscending,
                  filters);
              Job job;
              job.start = [&provider, params]() {
                  return provider.FetchMetricPivotTable(params);
              };
              job.ready = [&provider]() {
                  return !provider.IsRequestPending(
                      View::DataProvider::METRIC_PIVOT_TABLE_REQUEST_ID);
              };
              job.reply = [this, &provider]() {
                  jt::Json json = View::DataProviderJson::KernelSelectionTable(
                      provider.ComputeModel().GetKernelSelectionTable());
                  auto error = m_table_errors.find(
                      View::DataProvider::METRIC_PIVOT_TABLE_REQUEST_ID);
                  if(error != m_table_errors.end())
                  {
                      json["error"] = error->second;
                  }
                  return Ok(std::move(json));
              };
              return job;
          }));

    // One layer of a kernel's PC sampling: isa, source (of source_file, or
    // the first file) or stalls. The reply holds every layer loaded so far.
    Route(Method::kGet, R"(/api/compute/workloads/(\d+)/kernels/(\d+)/pc_sampling)",
          For(kind, [](View::DataProvider& provider, const RouteArgs& args) {
              uint32_t                    workload_id = 0;
              uint32_t                    kernel_id   = 0;
              uint64_t                    source_file = 0;
              const std::string           layer_name  = args.Param("layer", "isa");
              const View::PcSamplingLayer layer =
                  layer_name == "source"   ? View::PcSamplingLayer::kSource
                  : layer_name == "stalls" ? View::PcSamplingLayer::kStalls
                                           : View::PcSamplingLayer::kIsa;
              const bool valid = find_kernel(provider, args.captures) &&
                                 parse_uint32(args.captures[0], workload_id) &&
                                 parse_uint32(args.captures[1], kernel_id) &&
                                 read_uint64(args.params, "source_file", source_file) &&
                                 (layer_name == "isa" || layer_name == "source" ||
                                  layer_name == "stalls");
              if(!valid)
              {
                  return Now(Error(HTTP_BAD_REQUEST,
                                   "Name a kernel of the workload and a layer "
                                   "(isa, source or stalls)."));
              }
              const uint64_t request_id = View::RequestIdBuilder::MakeClientRequestId(
                  View::RequestType::kFetchPcSampling, static_cast<uint64_t>(layer));
              Job job;
              job.start = [&provider, layer, workload_id, kernel_id, source_file]() {
                  return provider.FetchPcSampling(View::PcSamplingRequestParams(
                      layer, workload_id, kernel_id, source_file, 0, 0));
              };
              job.ready = [&provider, request_id]() {
                  return !provider.IsRequestPending(request_id);
              };
              job.reply = [&provider, workload_id, kernel_id]() {
                  const View::KernelInfo* kernel =
                      provider.ComputeModel().GetKernelInfo(workload_id, kernel_id);
                  return kernel ? Ok(View::DataProviderJson::PcSampling(
                                      kernel->pc_sampling_data))
                                : Error(HTTP_NOT_FOUND,
                                        "No kernel with that id in that workload.");
              };
              return job;
          }));
}

}  // namespace Server
}  // namespace RocProfVis
