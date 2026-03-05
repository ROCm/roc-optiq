// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_web_server.h"

#include "httplib.h"
#include "spdlog/spdlog.h"

#include "json.h"

#include <sstream>

namespace RocProfVis
{
namespace Web
{

static std::string
GetParam(const httplib::Request& req, const std::string& key,
         const std::string& default_val = "")
{
    if(req.has_param(key))
        return req.get_param_value(key);
    return default_val;
}

static std::vector<uint64_t>
ParseTrackIds(const std::string& param)
{
    std::vector<uint64_t> ids;
    std::istringstream    ss(param);
    std::string           token;
    while(std::getline(ss, token, ','))
    {
        try { ids.push_back(std::stoull(token)); }
        catch(...) {}
    }
    return ids;
}

WebServer::WebServer(const ServerConfig& config)
    : m_config(config)
    , m_server(std::make_unique<httplib::Server>())
{
    RegisterRoutes();
}

WebServer::~WebServer()
{
    Stop();
}

void
WebServer::Run()
{
    spdlog::info("Starting web server at http://{}:{}", m_config.host, m_config.port);
    spdlog::info("Frontend directory: {}", m_config.frontend_dir);
    m_server->listen(m_config.host, m_config.port);
}

void
WebServer::Stop()
{
    if(m_server)
        m_server->stop();
}

void
WebServer::RegisterRoutes()
{
    // Log every request
    m_server->set_logger([](const httplib::Request& req, const httplib::Response& res) {
        spdlog::info("{} {} -> {} ({} bytes)", req.method, req.path, res.status,
                     res.body.size());
    });

    // CORS preflight
    m_server->Options(R"(/api/.*)", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 204;
    });

    // Serve frontend
    m_server->set_mount_point("/", m_config.frontend_dir);

    // POST /api/trace/load  { "file_path": "/path/to/trace.db" }
    m_server->Post("/api/trace/load",
                   [this](const httplib::Request& req, httplib::Response& res) {
                       auto [status, body] = jt::Json::parse(req.body);
                       std::string path;
                       if(status == jt::Json::success && body.isObject() &&
                          body.contains("file_path"))
                       {
                           path = body["file_path"].getString();
                       }
                       if(path.empty())
                       {
                           jt::Json err;
                           err.setObject();
                           err["status"]  = "error";
                           err["message"] = "file_path is required";
                           res.set_content(err.toString(), "application/json");
                           res.status = 400;
                           return;
                       }
                       jt::Json result = m_middleware.LoadTrace(path);
                       res.set_header("Access-Control-Allow-Origin", "*");
                       res.set_content(result.toString(), "application/json");
                   });

    // DELETE /api/trace
    m_server->Delete("/api/trace",
                     [this](const httplib::Request&, httplib::Response& res) {
                         jt::Json result = m_middleware.CloseTrace();
                         res.set_header("Access-Control-Allow-Origin", "*");
                         res.set_content(result.toString(), "application/json");
                     });

    // GET /api/trace/status
    m_server->Get("/api/trace/status",
                  [this](const httplib::Request&, httplib::Response& res) {
                      jt::Json result;
                      result.setObject();
                      result["loaded"] = m_middleware.IsLoaded();
                      result["status"] = "ok";
                      res.set_header("Access-Control-Allow-Origin", "*");
                      res.set_content(result.toString(), "application/json");
                  });

    // GET /api/trace/topology
    m_server->Get("/api/trace/topology",
                  [this](const httplib::Request&, httplib::Response& res) {
                      jt::Json result = m_middleware.GetTopology();
                      res.set_header("Access-Control-Allow-Origin", "*");
                      res.set_content(result.toString(), "application/json");
                  });

    // GET /api/trace/timeline
    m_server->Get("/api/trace/timeline",
                  [this](const httplib::Request&, httplib::Response& res) {
                      jt::Json result = m_middleware.GetTimelineInfo();
                      res.set_header("Access-Control-Allow-Origin", "*");
                      res.set_content(result.toString(), "application/json");
                  });

    // GET /api/trace/tracks
    m_server->Get("/api/trace/tracks",
                  [this](const httplib::Request&, httplib::Response& res) {
                      jt::Json result = m_middleware.GetTracks();
                      res.set_header("Access-Control-Allow-Origin", "*");
                      res.set_content(result.toString(), "application/json");
                  });

    // GET /api/trace/tracks/:id/graph?start=&end=&resolution=
    m_server->Get(R"(/api/trace/tracks/(\d+)/graph)",
                  [this](const httplib::Request& req, httplib::Response& res) {
                      uint64_t track_id   = std::stoull(req.matches[1]);
                      double   start      = std::stod(GetParam(req, "start", "0"));
                      double   end        = std::stod(GetParam(req, "end", "0"));
                      uint32_t resolution = static_cast<uint32_t>(
                          std::stoul(GetParam(req, "resolution", "1920")));

                      jt::Json result =
                          m_middleware.GetTrackGraphData(track_id, start, end, resolution);
                      res.set_header("Access-Control-Allow-Origin", "*");
                      res.set_content(result.toString(), "application/json");
                  });

    // GET /api/trace/tables/events
    m_server->Get("/api/trace/tables/events",
                  [this](const httplib::Request& req, httplib::Response& res) {
                      auto     track_ids = ParseTrackIds(GetParam(req, "track_ids"));
                      double   start     = std::stod(GetParam(req, "start", "0"));
                      double   end       = std::stod(GetParam(req, "end", "0"));
                      uint64_t offset    = std::stoull(GetParam(req, "offset", "0"));
                      uint64_t count     = std::stoull(GetParam(req, "count", "100"));
                      uint64_t sort_col  = std::stoull(GetParam(req, "sort_col", "0"));
                      bool     sort_asc  = GetParam(req, "sort_asc", "true") == "true";
                      std::string filter     = GetParam(req, "filter");
                      std::string group      = GetParam(req, "group");
                      std::string group_cols = GetParam(req, "group_cols");

                      jt::Json result = m_middleware.GetEventTable(
                          track_ids, start, end, offset, count, sort_col,
                          sort_asc, filter, group, group_cols);
                      res.set_header("Access-Control-Allow-Origin", "*");
                      res.set_content(result.toString(), "application/json");
                  });

    // GET /api/trace/tables/samples
    m_server->Get("/api/trace/tables/samples",
                  [this](const httplib::Request& req, httplib::Response& res) {
                      auto     track_ids = ParseTrackIds(GetParam(req, "track_ids"));
                      double   start     = std::stod(GetParam(req, "start", "0"));
                      double   end       = std::stod(GetParam(req, "end", "0"));
                      uint64_t offset    = std::stoull(GetParam(req, "offset", "0"));
                      uint64_t count     = std::stoull(GetParam(req, "count", "100"));
                      uint64_t sort_col  = std::stoull(GetParam(req, "sort_col", "0"));
                      bool     sort_asc  = GetParam(req, "sort_asc", "true") == "true";
                      std::string filter = GetParam(req, "filter");

                      jt::Json result = m_middleware.GetSampleTable(
                          track_ids, start, end, offset, count, sort_col,
                          sort_asc, filter);
                      res.set_header("Access-Control-Allow-Origin", "*");
                      res.set_content(result.toString(), "application/json");
                  });

    // GET /api/trace/events/:id
    m_server->Get(R"(/api/trace/events/(\d+))",
                  [this](const httplib::Request& req, httplib::Response& res) {
                      uint64_t event_id = std::stoull(req.matches[1]);
                      jt::Json result   = m_middleware.GetEventDetails(event_id);
                      res.set_header("Access-Control-Allow-Origin", "*");
                      res.set_content(result.toString(), "application/json");
                  });

    // GET /api/trace/summary?start=&end=
    m_server->Get("/api/trace/summary",
                  [this](const httplib::Request& req, httplib::Response& res) {
                      double start = std::stod(GetParam(req, "start", "-1"));
                      double end   = std::stod(GetParam(req, "end", "-1"));

                      jt::Json result = m_middleware.GetSummary(start, end);
                      res.set_header("Access-Control-Allow-Origin", "*");
                      res.set_content(result.toString(), "application/json");
                  });

    spdlog::info("Routes registered");
}

}  // namespace Web
}  // namespace RocProfVis
