// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_json_middleware.h"

#include <memory>
#include <string>

namespace httplib { class Server; }

namespace RocProfVis
{
namespace Web
{

struct ServerConfig
{
    std::string host            = "0.0.0.0";
    int         port            = 8080;
    std::string frontend_dir    = "./frontend";
};

class WebServer
{
public:
    explicit WebServer(const ServerConfig& config);
    ~WebServer();

    void Run();
    void Stop();

private:
    void RegisterRoutes();

    ServerConfig                    m_config;
    std::unique_ptr<httplib::Server> m_server;
    JsonMiddleware                   m_middleware;
};

}  // namespace Web
}  // namespace RocProfVis
