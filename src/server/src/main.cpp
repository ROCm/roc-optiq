// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include <cctype>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <string>
#include <system_error>

#include "rocprofvis_data_provider.h"
#include "rocprofvis_trace_server.h"
#include "rocprofvis_version.h"

#include "spdlog/spdlog.h"

namespace RocProfVis
{
namespace Server
{

constexpr const char* DEFAULT_HOST = "127.0.0.1";
constexpr uint16_t    DEFAULT_PORT = 8080;
// How long the provider thread sleeps between updates when no request is waiting.
constexpr std::chrono::milliseconds UPDATE_INTERVAL(10);

constexpr int32_t EXIT_CODE_OK    = 0;
constexpr int32_t EXIT_CODE_ERROR = 1;
constexpr int32_t EXIT_CODE_USAGE = 2;

struct ServerOptions
{
    std::string trace_path;
    std::string host = DEFAULT_HOST;
    uint16_t    port = DEFAULT_PORT;
    std::string allowed_origin;
    std::string web_root;
};

enum class ParseOutcome
{
    kRun,
    kExit,
    kUsageError
};

static volatile std::sig_atomic_t g_stop_requested = 0;

static void
request_stop(int)
{
    g_stop_requested = 1;
}

static void
print_usage()
{
    std::cout << "Usage: roc-optiq-server [options] [trace]\n"
                 "Serves a ROCm Optiq trace as JSON over HTTP, under /api/. With no\n"
                 "trace, clients open one through the API.\n"
                 "\n"
                 "  --host <address>         Loopback address (default 127.0.0.1)\n"
                 "  --port <number>          Port to listen on (default 8080)\n"
                 "  --web <folder>           Serve the built web app from folder at /\n"
                 "  --allow-origin <origin>  Let pages served from origin read the API\n"
                 "  --version                Print the version and exit\n"
                 "  --help                   Print this help and exit\n"
                 "\n"
                 "The server has no authentication or TLS. Bind only to a trusted "
                 "interface;\n"
                 "--allow-origin is a browser CORS setting, not access control.\n";
    std::cout.flush();
}

static void
print_version()
{
    std::cout << "roc-optiq-server version: " << ROCPROFVIS_VERSION_MAJOR << "."
              << ROCPROFVIS_VERSION_MINOR << "." << ROCPROFVIS_VERSION_PATCH << "."
              << ROCPROFVIS_VERSION_BUILD << " commit: " << ROCPROFVIS_GIT_COMMIT;
    if(std::string(ROCPROFVIS_GIT_COMMIT) == "unknown")
    {
        std::cout << "\n" << ROCPROFVIS_GIT_COMMIT_UNKNOWN_NOTE;
    }
    std::cout << std::endl;
}

static bool
parse_port(const std::string& text, uint16_t& port_out)
{
    uint32_t               port   = 0;
    const char*            last   = text.data() + text.size();
    std::from_chars_result result = std::from_chars(text.data(), last, port);
    bool                   valid =
        result.ec == std::errc() && result.ptr == last && port > 0 && port <= UINT16_MAX;
    if(valid)
    {
        port_out = static_cast<uint16_t>(port);
    }
    return valid;
}

static bool
is_loopback_host(const std::string& host)
{
    std::string normalized = host;
    for(char& character : normalized)
    {
        character =
            static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return normalized == "localhost" || normalized == "::1" ||
           normalized.rfind("127.", 0) == 0;
}

static ParseOutcome
parse_args(int argc, char** argv, ServerOptions& options)
{
    ParseOutcome outcome = ParseOutcome::kRun;
    for(int i = 1; i < argc && outcome == ParseOutcome::kRun; i++)
    {
        const std::string arg       = argv[i];
        const bool        has_value = i + 1 < argc;
        if(arg == "--help" || arg == "-h")
        {
            print_usage();
            outcome = ParseOutcome::kExit;
        }
        else if(arg == "--version" || arg == "-v")
        {
            print_version();
            outcome = ParseOutcome::kExit;
        }
        else if(arg == "--host" && has_value)
        {
            i++;
            options.host = argv[i];
        }
        else if(arg == "--port" && has_value)
        {
            i++;
            if(!parse_port(argv[i], options.port))
            {
                spdlog::error("'{}' is not a port number.", argv[i]);
                outcome = ParseOutcome::kUsageError;
            }
        }
        else if(arg == "--allow-origin" && has_value)
        {
            i++;
            options.allowed_origin = argv[i];
        }
        else if(arg == "--web" && has_value)
        {
            i++;
            options.web_root = argv[i];
        }
        else if(!arg.empty() && arg[0] != '-' && options.trace_path.empty())
        {
            options.trace_path = arg;
        }
        else
        {
            spdlog::error("Unexpected argument '{}'.", arg);
            outcome = ParseOutcome::kUsageError;
        }
    }
    if(outcome == ParseOutcome::kUsageError)
    {
        print_usage();
    }
    return outcome;
}

// Updates the provider and answers requests on this thread until asked to stop.
// A trace that fails to load stays reported in /api/status until a client opens
// another.
static int32_t
serve(View::DataProvider& provider, TraceServer& server)
{
    std::signal(SIGINT, request_stop);
    std::signal(SIGTERM, request_stop);

    View::ProviderState state = provider.GetState();
    while(!g_stop_requested && !server.ListenerFailed())
    {
        provider.Update();
        server.RunPendingRequests();
        View::ProviderState updated = provider.GetState();
        if(updated != state && updated == View::ProviderState::kReady)
        {
            spdlog::info("Loaded {}", provider.GetTraceFilePath());
        }
        else if(updated != state && updated == View::ProviderState::kError)
        {
            spdlog::error("Could not load '{}'.", provider.GetTraceFilePath());
        }
        state = updated;
        server.WaitForRequests(UPDATE_INTERVAL);
    }
    const bool listener_failed = server.ListenerFailed();
    server.Stop();
    if(listener_failed)
    {
        spdlog::error("The HTTP listener stopped unexpectedly.");
    }
    return listener_failed ? EXIT_CODE_ERROR : EXIT_CODE_OK;
}

static int32_t
run(const ServerOptions& options)
{
    if(!is_loopback_host(options.host))
    {
        spdlog::error(
            "The unauthenticated server only accepts loopback listen addresses.");
        return EXIT_CODE_ERROR;
    }

    View::DataProvider provider;
    TraceServer        server(provider, options.allowed_origin, options.web_root);
    std::string        error;
    if(!options.trace_path.empty() && !server.Open(options.trace_path, error))
    {
        spdlog::error("{}", error);
        return EXIT_CODE_ERROR;
    }
    if(!server.Start(options.host, options.port))
    {
        spdlog::error("Could not listen on {}:{}.", options.host, options.port);
        return EXIT_CODE_ERROR;
    }
    spdlog::info("Serving {} at http://{}:{}/",
                 options.trace_path.empty() ? "no trace yet" : options.trace_path,
                 options.host, options.port);
    return serve(provider, server);
}

}  // namespace Server
}  // namespace RocProfVis

int
main(int argc, char** argv)
{
    RocProfVis::Server::ServerOptions options;
    RocProfVis::Server::ParseOutcome  outcome =
        RocProfVis::Server::parse_args(argc, argv, options);
    int32_t exit_code = RocProfVis::Server::EXIT_CODE_OK;
    if(outcome == RocProfVis::Server::ParseOutcome::kUsageError)
    {
        exit_code = RocProfVis::Server::EXIT_CODE_USAGE;
    }
    else if(outcome == RocProfVis::Server::ParseOutcome::kRun)
    {
        exit_code = RocProfVis::Server::run(options);
    }
    return exit_code;
}
