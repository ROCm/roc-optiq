// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "protocol/rocprofvis_protocol.h"
#include "rocprofvis_server.h"
#include "rocprofvis_utils.h"
#include "rocprofvis_version.h"

#include "spdlog/spdlog.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

namespace
{

constexpr const char* DEFAULT_HOST            = "127.0.0.1";
constexpr size_t      TOKEN_BYTES             = 24;
constexpr uint64_t    BYTES_PER_GIGABYTE      = 1024ull * 1024 * 1024;
constexpr uint64_t    DEFAULT_MAX_UPLOAD_GB   = 256;

RocProfVis::Server::Server* g_server = nullptr;

void
handle_stop_signal(int)
{
    if(g_server)
    {
        g_server->RequestStop();
    }
}

std::string
generate_token()
{
    std::random_device                      device;
    std::uniform_int_distribution<uint32_t> distribution(0, 255);
    std::string                             token;
    char                                    digits[3];
    for(size_t i = 0; i < TOKEN_BYTES; i++)
    {
        std::snprintf(digits, sizeof(digits), "%02x", distribution(device));
        token += digits;
    }
    return token;
}

// Readable by this user only: whoever can read it can open every trace the
// server can.
bool
write_token_file(const std::filesystem::path& path, const std::string& token)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::filesystem::remove(path, error);
    std::ofstream file(path, std::ios::trunc);
    file << token << "\n";
    file.close();
    std::filesystem::permissions(
        path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace, error);
    return file.good() && !error;
}

void
print_usage()
{
    std::printf(
        "Usage: roc-optiq-server [options]\n"
        "\n"
        "Serves the traces on this machine to roc-optiq clients.\n"
        "\n"
        "Options:\n"
        "  --host <address>      Address to listen on (default %s). From another machine,\n"
        "                        tunnel over SSH (ssh -L %d:localhost:%d <host>) or serve\n"
        "                        TLS on a reachable address.\n"
        "  --port <port>         Port to listen on (default %d, 0 picks a free one)\n"
        "  --token <token>       Access token clients must present (default: a new random\n"
        "                        one, printed and saved in the config directory)\n"
        "  --no-auth             Accept any client that can connect\n"
        "  --tls-cert <file>     PEM certificate; with --tls-key, serve optiqs:// (TLS)\n"
        "  --tls-key <file>      PEM private key for --tls-cert\n"
        "  --upload-dir <dir>    Where uploaded traces go (default <config>/uploads)\n"
        "  --max-upload-gb <n>   Largest upload accepted (default %llu, 0 for no limit)\n"
        "  --no-upload           Refuse uploads\n"
        "  --session-timeout <s> How long a dropped client's traces are kept for it to\n"
        "                        reconnect (default 600)\n"
        "  --allow-profiler      Let clients run profilers on this machine\n"
        "  --allow-scripts       Let clients run Python analysis scripts on this machine\n"
        "  --config <dir>        Directory for per-trace metadata and the token (default:\n"
        "                        the roc-optiq config directory of this user)\n"
        "  --log-level <lvl>     trace, debug, info, warn or error (default info)\n"
        "  -v, --version         Print the version and exit\n"
        "  -h, --help            Print this help and exit\n",
        DEFAULT_HOST, RocProfVis::View::DEFAULT_SERVER_PORT,
        RocProfVis::View::DEFAULT_SERVER_PORT, RocProfVis::View::DEFAULT_SERVER_PORT,
        static_cast<unsigned long long>(DEFAULT_MAX_UPLOAD_GB));
}

}  // namespace

int
main(int argc, char** argv)
{
    RocProfVis::Server::server_options_t options;
    options.host             = DEFAULT_HOST;
    options.port             = RocProfVis::View::DEFAULT_SERVER_PORT;
    options.max_upload_bytes = DEFAULT_MAX_UPLOAD_GB * BYTES_PER_GIGABYTE;
    spdlog::level::level_enum log_level  = spdlog::level::info;
    bool                      no_auth    = false;
    bool                      no_upload  = false;

    for(int i = 1; i < argc; i++)
    {
        std::string argument  = argv[i];
        bool        has_value = i + 1 < argc;
        if(argument == "-h" || argument == "--help")
        {
            print_usage();
            return EXIT_SUCCESS;
        }
        else if(argument == "-v" || argument == "--version")
        {
            std::printf("roc-optiq-server %d.%d.%d (protocol %lld)\n", ROCPROFVIS_VERSION_MAJOR,
                        ROCPROFVIS_VERSION_MINOR, ROCPROFVIS_VERSION_PATCH,
                        static_cast<long long>(RocProfVis::View::PROTOCOL_VERSION));
            return EXIT_SUCCESS;
        }
        else if(argument == "--host" && has_value)
        {
            options.host = argv[++i];
        }
        else if(argument == "--port" && has_value)
        {
            options.port = std::atoi(argv[++i]);
        }
        else if(argument == "--token" && has_value)
        {
            options.token = argv[++i];
        }
        else if(argument == "--no-auth")
        {
            no_auth = true;
        }
        else if(argument == "--tls-cert" && has_value)
        {
            options.tls_certificate = argv[++i];
        }
        else if(argument == "--tls-key" && has_value)
        {
            options.tls_key = argv[++i];
        }
        else if(argument == "--upload-dir" && has_value)
        {
            options.upload_directory = argv[++i];
        }
        else if(argument == "--max-upload-gb" && has_value)
        {
            options.max_upload_bytes = std::strtoull(argv[++i], nullptr, 10) * BYTES_PER_GIGABYTE;
        }
        else if(argument == "--no-upload")
        {
            no_upload = true;
        }
        else if(argument == "--session-timeout" && has_value)
        {
            options.session_timeout_seconds =
                static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        }
        else if(argument == "--allow-profiler")
        {
            options.allow_profiler = true;
        }
        else if(argument == "--allow-scripts")
        {
            options.allow_scripts = true;
        }
        else if(argument == "--config" && has_value)
        {
            options.config_path = argv[++i];
        }
        else if(argument == "--log-level" && has_value)
        {
            log_level = spdlog::level::from_str(argv[++i]);
        }
        else
        {
            std::fprintf(stderr, "Unknown or incomplete option: %s\n\n", argument.c_str());
            print_usage();
            return EXIT_FAILURE;
        }
    }

    spdlog::set_level(log_level);
    if(options.config_path.empty())
    {
        options.config_path = RocProfVis::View::get_application_config_path(true);
    }
    if(options.upload_directory.empty() && !no_upload)
    {
        options.upload_directory =
            (std::filesystem::path(options.config_path) / "uploads").string();
    }
    if(no_upload)
    {
        options.upload_directory.clear();
    }
    if(options.tls_certificate.empty() != options.tls_key.empty())
    {
        spdlog::error("--tls-cert and --tls-key go together");
        return EXIT_FAILURE;
    }
    if(no_auth)
    {
        options.token.clear();
    }
    else if(options.token.empty())
    {
        options.token = generate_token();
    }

    RocProfVis::Server::Server server(options);
    if(!server.IsValid())
    {
        spdlog::error("Cannot serve TLS with {} and {}", options.tls_certificate, options.tls_key);
        return EXIT_FAILURE;
    }
    if(!server.Bind())
    {
        spdlog::error("Cannot listen on {}:{}", options.host, options.port);
        return EXIT_FAILURE;
    }

    const bool  tls     = !options.tls_certificate.empty();
    const char* scheme  = tls ? "optiqs" : "optiq";
    std::filesystem::path token_file =
        std::filesystem::path(options.config_path) /
        RocProfVis::View::server_token_file_name(server.GetPort());
    spdlog::info("roc-optiq-server {}.{}.{} listening on {}://{}:{}{}", ROCPROFVIS_VERSION_MAJOR,
                 ROCPROFVIS_VERSION_MINOR, ROCPROFVIS_VERSION_PATCH, tls ? "wss" : "ws",
                 options.host, server.GetPort(), RocProfVis::View::PROTOCOL_WEBSOCKET_PATH);
    if(options.token.empty())
    {
        spdlog::warn("Authentication is off: anyone who can reach this port can read every "
                     "trace this user can");
    }
    else
    {
        if(!write_token_file(token_file, options.token))
        {
            spdlog::warn("Could not save the access token to {}", token_file.string());
        }
        spdlog::info("Access token: {}", options.token);
        spdlog::info("Open a trace with: roc-optiq -f '{}://localhost:{}/path/to/trace.db?token={}'",
                     scheme, server.GetPort(), options.token);
    }
    if(!tls && options.host != DEFAULT_HOST && options.host != "localhost")
    {
        spdlog::warn("Serving on {} without TLS: traces and the token cross the network in "
                     "the clear",
                     options.host);
    }
    if(options.allow_profiler)
    {
        spdlog::info("Clients may run profilers on this machine");
    }
    if(options.allow_scripts)
    {
#ifdef ROCPROFVIS_ENABLE_SCRIPTING
        spdlog::info("Clients may run Python scripts on this machine");
#else
        spdlog::warn("--allow-scripts has no effect: this server was built without scripting");
#endif
    }

    g_server = &server;
    std::signal(SIGINT, handle_stop_signal);
    std::signal(SIGTERM, handle_stop_signal);
    bool listened = server.Listen();
    g_server      = nullptr;
    if(!options.token.empty())
    {
        std::error_code error;
        std::filesystem::remove(token_file, error);
    }
    spdlog::info("roc-optiq-server stopped");
    return listened ? EXIT_SUCCESS : EXIT_FAILURE;
}
