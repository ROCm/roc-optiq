// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_web_server.h"

#include "spdlog/spdlog.h"

#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

static RocProfVis::Web::WebServer* g_server = nullptr;

static void
SignalHandler(int)
{
    spdlog::info("Shutting down...");
    if(g_server)
        g_server->Stop();
}

static void
PrintUsage(const char* prog)
{
    std::cout << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  --host <addr>     Bind address (default: 0.0.0.0)\n"
              << "  --port <port>     Port number (default: 8080)\n"
              << "  --frontend <dir>  Frontend directory (default: ./frontend)\n"
              << "  --help            Show this help\n";
}

int
main(int argc, char* argv[])
{
    spdlog::set_level(spdlog::level::info);

    RocProfVis::Web::ServerConfig config;

    for(int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if(arg == "--host" && i + 1 < argc)
            config.host = argv[++i];
        else if(arg == "--port" && i + 1 < argc)
            config.port = std::atoi(argv[++i]);
        else if(arg == "--frontend" && i + 1 < argc)
            config.frontend_dir = argv[++i];
        else if(arg == "--help")
        {
            PrintUsage(argv[0]);
            return 0;
        }
    }

    // Try to find frontend directory relative to executable if default not found
    if(!std::filesystem::exists(config.frontend_dir))
    {
        auto exe_dir = std::filesystem::path(argv[0]).parent_path();
        auto alt     = exe_dir / "frontend";
        if(std::filesystem::exists(alt))
            config.frontend_dir = alt.string();
    }

    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    RocProfVis::Web::WebServer server(config);
    g_server = &server;

    spdlog::info("ROCProfiler Visualizer Web Server");
    spdlog::info("Open http://localhost:{} in your browser", config.port);

    server.Run();

    g_server = nullptr;
    return 0;
}
