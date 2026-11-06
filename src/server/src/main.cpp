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

#include "rocprofvis_controller.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_trace_server.h"
#include "rocprofvis_utils.h"
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
    std::cout << "Usage: roc-optiq-server [options] <trace>\n"
                 "Serves a ROCm Optiq trace as JSON over HTTP, under /api/.\n"
                 "\n"
                 "  --host <address>         Loopback address (default 127.0.0.1)\n"
                 "  --port <number>          Port to listen on (default 8080)\n"
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

static bool
get_trace_kind(rocprofvis_controller_t*           controller,
               View::DataProviderJson::TraceKind& kind_out)
{
    rocprofvis_controller_object_type_t type = kRPVControllerObjectTypeControllerSystem;
    const rocprofvis_result_t           result =
        rocprofvis_controller_get_object_type(controller, &type);
    const bool valid = result == kRocProfVisResultSuccess &&
                       (type == kRPVControllerObjectTypeControllerSystem ||
                        type == kRPVControllerObjectTypeControllerCompute);
    if(valid)
    {
        kind_out = type == kRPVControllerObjectTypeControllerSystem
                       ? View::DataProviderJson::TraceKind::kSystem
                       : View::DataProviderJson::TraceKind::kCompute;
    }
    return valid;
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
    if(outcome == ParseOutcome::kRun && options.trace_path.empty())
    {
        spdlog::error("No trace file given.");
        outcome = ParseOutcome::kUsageError;
    }
    if(outcome == ParseOutcome::kUsageError)
    {
        print_usage();
    }
    return outcome;
}

// Updates the provider and answers requests on this thread until asked to stop.
static int32_t
serve(View::DataProvider& provider, TraceServer& server, const ServerOptions& options)
{
    std::signal(SIGINT, request_stop);
    std::signal(SIGTERM, request_stop);

    View::ProviderState state = provider.GetState();
    while(!g_stop_requested && state != View::ProviderState::kError &&
          !server.ListenerFailed())
    {
        provider.Update();
        server.RunPendingRequests();
        View::ProviderState updated = provider.GetState();
        if(updated != state && updated == View::ProviderState::kReady)
        {
            spdlog::info("Loaded {}", options.trace_path);
        }
        state = updated;
        server.WaitForRequests(UPDATE_INTERVAL);
    }
    const bool listener_failed = server.ListenerFailed();
    server.Stop();

    int32_t exit_code = EXIT_CODE_OK;
    if(listener_failed)
    {
        spdlog::error("The HTTP listener stopped unexpectedly.");
        exit_code = EXIT_CODE_ERROR;
    }
    else if(state == View::ProviderState::kError)
    {
        spdlog::error("Could not load '{}'.", options.trace_path);
        exit_code = EXIT_CODE_ERROR;
    }
    return exit_code;
}

static int32_t
run(const ServerOptions& options)
{
    int32_t exit_code = EXIT_CODE_ERROR;
    if(!is_loopback_host(options.host))
    {
        spdlog::error(
            "The unauthenticated server only accepts loopback listen addresses.");
        return exit_code;
    }
    const std::string        config_path = View::get_application_config_path(true);
    rocprofvis_controller_t* controller =
        rocprofvis_controller_alloc(options.trace_path.c_str(), config_path.c_str());
    if(!controller)
    {
        spdlog::error("Could not open '{}' as a trace.", options.trace_path);
    }
    else
    {
        View::DataProviderJson::TraceKind trace_kind =
            View::DataProviderJson::TraceKind::kSystem;
        if(!get_trace_kind(controller, trace_kind))
        {
            spdlog::error("'{}' is not a supported system or compute trace.",
                          options.trace_path);
            rocprofvis_controller_free(controller);
        }
        else
        {
            // The provider owns the controller from here, and frees it on failure.
            View::DataProvider provider;
            if(!provider.FetchTrace(controller, options.trace_path))
            {
                spdlog::error("Could not start loading '{}'.", options.trace_path);
            }
            else
            {
                TraceServer server(provider, trace_kind, options.allowed_origin);
                if(!server.Start(options.host, options.port))
                {
                    spdlog::error("Could not listen on {}:{}.", options.host,
                                  options.port);
                }
                else
                {
                    spdlog::info("Serving {} at http://{}:{}/api/", options.trace_path,
                                 options.host, options.port);
                    exit_code = serve(provider, server, options);
                }
            }
        }
    }
    return exit_code;
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
