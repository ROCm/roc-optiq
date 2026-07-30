// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include <signal.h>
#include <stdint.h>
#include <stdlib.h>

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/spdlog.h"

#include "rocprofvis_middleware.h"

#include "rocprofvis_mw_http.h"
#include "rocprofvis_mw_net.h"
#include "rocprofvis_mw_websocket.h"

/*
 * HTTP and WebSocket adapter for the middleware.
 *
 * Speaks the same JSON protocol as the stdio adapter over two transports:
 *
 *   POST /rpc   one JSON request in the body, one JSON response back
 *   GET  /ws    upgrade to a WebSocket and exchange the same documents as
 *               text frames, which avoids a connection per request and lets a
 *               client keep several fetches in flight
 *
 * One session is shared by every client, because a session owns the open trace
 * and a trace is far too expensive to hold per connection. Requests are
 * therefore served strictly one at a time by a single-threaded loop, which is
 * also what the session requires. Concurrency comes from the controller's
 * worker threads: issue a fetch without wait_ms and poll it, rather than
 * blocking the loop -- a large wait_ms stalls every other client for its
 * duration.
 *
 * The listener binds to loopback unless told otherwise. The protocol can open
 * any trace path and write a CSV to any path, so exposing it beyond this
 * machine hands those abilities to whoever can reach the port.
 */

static constexpr uint16_t DEFAULT_PORT = 8378;

static char const* const DEFAULT_HOST = "127.0.0.1";

/* How long the loop parks in select() when nothing is ready. */
static constexpr uint32_t POLL_INTERVAL_MS = 200;

static constexpr size_t READ_CHUNK_BYTES = 64ULL * 1024ULL;

/* Cap on a message reassembled from WebSocket fragments. */
static constexpr size_t MAX_MESSAGE_BYTES = 16ULL * 1024ULL * 1024ULL;

static constexpr uint16_t WS_CLOSE_NORMAL         = 1000;
static constexpr uint16_t WS_CLOSE_PROTOCOL_ERROR = 1002;
static constexpr uint16_t WS_CLOSE_TOO_LARGE      = 1009;

static char const* const CONTENT_TYPE_JSON = "application/json";

/* Set from a signal handler, so it must stay a plain flag. */
static volatile sig_atomic_t g_stop_requested = 0;

static void
handle_stop_signal(int signal_number)
{
    (void) signal_number;
    g_stop_requested = 1;
}

enum class Mode
{
    kHttp,
    kWebSocket
};

/* One client, and whatever of its request is buffered so far. */
struct connection_t
{
    RocProfVis::Middleware::Net::socket_t socket;
    Mode                                  mode;
    std::string                           input;
    std::string                           output;
    /* Close once output has drained. */
    bool closing;

    /* Reassembly state for a fragmented WebSocket message. */
    std::string                                 fragment;
    RocProfVis::Middleware::WebSocket::Opcode   fragment_opcode;
    bool                                        fragmenting;
};

struct options_t
{
    std::string host;
    uint16_t    port;
    bool        verbose;
    bool        show_help;
    bool        invalid;
};

static options_t
parse_options(int argc, char** argv)
{
    options_t options;
    options.host      = DEFAULT_HOST;
    options.port      = DEFAULT_PORT;
    options.verbose   = false;
    options.show_help = false;
    options.invalid   = false;

    for(int i = 1; i < argc; i++)
    {
        std::string argument = argv[i];
        if(argument == "--help" || argument == "-h")
        {
            options.show_help = true;
        }
        else if(argument == "--verbose")
        {
            options.verbose = true;
        }
        else if((argument == "--host" || argument == "--port") && (i + 1) < argc)
        {
            i++;
            if(argument == "--host")
            {
                options.host = argv[i];
            }
            else
            {
                char*              end   = nullptr;
                unsigned long long value = strtoull(argv[i], &end, 10);
                if(end == nullptr || *end != '\0' || value == 0 || value > UINT16_MAX)
                {
                    std::cerr << "port must be between 1 and " << UINT16_MAX << "\n";
                    options.invalid = true;
                }
                else
                {
                    options.port = static_cast<uint16_t>(value);
                }
            }
        }
        else
        {
            std::cerr << "unrecognised argument '" << argument << "'\n";
            options.invalid = true;
        }
    }
    return options;
}

static void
print_usage(void)
{
    std::cout
        << "roc-optiq-middleware-http - JSON middleware over HTTP and WebSocket\n\n"
        << "  --host <address>  IPv4 address to bind (default " << DEFAULT_HOST << ")\n"
        << "  --port <number>   port to listen on (default " << DEFAULT_PORT << ")\n"
        << "  --verbose         log every request\n"
        << "  --help            show this message\n\n"
        << "Endpoints:\n"
        << "  POST /rpc         one JSON request, one JSON response\n"
        << "  GET  /ws          upgrade to a WebSocket carrying the same protocol\n"
        << "  GET  /health      liveness check\n\n"
        << "Binding anywhere other than loopback exposes trace opening and CSV\n"
        << "export, which read and write arbitrary paths, to the network.\n";
}

/* Body for GET /, so a browser hitting the port sees what it is talking to. */
static std::string
describe_endpoints(void)
{
    return "{\"service\":\"roc-optiq-middleware\",\"transports\":[\"http\",\"websocket\"]"
           ",\"endpoints\":{\"rpc\":\"POST /rpc\",\"websocket\":\"GET /ws\""
           ",\"health\":\"GET /health\"}"
           ",\"hint\":\"send a session.info request to /rpc to discover methods\"}";
}

static std::string
error_document(const std::string& code, const std::string& message)
{
    return "{\"ok\":false,\"error\":{\"code\":\"" + code + "\",\"message\":\"" + message +
           "\"}}";
}

/* Path portion of a request target, with any query string removed. */
static std::string
path_of(const std::string& target)
{
    size_t query = target.find('?');
    return (query == std::string::npos) ? target : target.substr(0, query);
}

static std::string
execute(rocprofvis_mw_session_t* session, const std::string& request, bool verbose)
{
    if(verbose)
    {
        spdlog::info("request: {}", request);
    }

    std::string reply;
    char*       response = rocprofvis_mw_request(session, request.c_str());
    if(response == nullptr)
    {
        reply = error_document("internal_error", "request failed");
    }
    else
    {
        reply = response;
        rocprofvis_mw_string_free(response);
    }
    return reply;
}

/*
 * Handle one parsed HTTP request. Returns false when the connection has been
 * upgraded and the remaining bytes belong to the WebSocket codec.
 */
static void
serve_http(connection_t& connection, const RocProfVis::Middleware::Http::request_t& request,
           rocprofvis_mw_session_t* session, bool verbose)
{
    namespace Http      = RocProfVis::Middleware::Http;
    namespace WebSocket = RocProfVis::Middleware::WebSocket;

    std::string path = path_of(request.target);

    if(request.method == "OPTIONS")
    {
        connection.output += Http::Response(204, CONTENT_TYPE_JSON, std::string());
        return;
    }

    if(path == "/ws")
    {
        if(!Http::IsWebSocketUpgrade(request))
        {
            connection.output += Http::Response(
                426, CONTENT_TYPE_JSON,
                error_document("upgrade_required", "GET /ws requires a WebSocket upgrade"));
            return;
        }

        std::string accept = WebSocket::AcceptKey(Http::Header(request, "sec-websocket-key"));
        connection.output += Http::UpgradeResponse(accept);
        connection.mode = Mode::kWebSocket;
        spdlog::info("websocket connected");
        return;
    }

    if(path == "/health")
    {
        connection.output += Http::Response(200, CONTENT_TYPE_JSON, "{\"ok\":true}");
        return;
    }

    if(path == "/" || path.empty())
    {
        connection.output += Http::Response(200, CONTENT_TYPE_JSON, describe_endpoints());
        return;
    }

    if(path != "/rpc")
    {
        connection.output += Http::Response(
            404, CONTENT_TYPE_JSON,
            error_document("not_found", "no endpoint at " + path));
        return;
    }

    if(request.method != "POST")
    {
        connection.output += Http::Response(
            405, CONTENT_TYPE_JSON,
            error_document("method_not_allowed", "/rpc accepts POST"));
        return;
    }

    connection.output +=
        Http::Response(200, CONTENT_TYPE_JSON, execute(session, request.body, verbose));
}

/* Drain whole HTTP requests out of a connection's input buffer. */
static void
pump_http(connection_t& connection, rocprofvis_mw_session_t* session, bool verbose)
{
    namespace Http = RocProfVis::Middleware::Http;

    while(connection.mode == Mode::kHttp && !connection.closing)
    {
        Http::request_t request;
        size_t          consumed    = 0;
        uint16_t        status_code = 200;

        Http::ParseStatus status =
            Http::Parse(connection.input, request, consumed, status_code);
        if(status == Http::ParseStatus::kIncomplete)
        {
            break;
        }
        if(status != Http::ParseStatus::kComplete)
        {
            connection.output += Http::Response(
                status_code, CONTENT_TYPE_JSON,
                error_document("bad_request", Http::ReasonPhrase(status_code)));
            connection.closing = true;
            break;
        }

        connection.input.erase(0, consumed);
        serve_http(connection, request, session, verbose);
    }
}

/* Drain whole WebSocket frames out of a connection's input buffer. */
static void
pump_websocket(connection_t& connection, rocprofvis_mw_session_t* session, bool verbose)
{
    namespace WebSocket = RocProfVis::Middleware::WebSocket;

    while(!connection.closing)
    {
        WebSocket::Opcode opcode   = WebSocket::Opcode::kText;
        bool              is_final = false;
        std::string       payload;
        size_t            consumed = 0;

        WebSocket::DecodeStatus status =
            WebSocket::Decode(connection.input, opcode, is_final, payload, consumed);
        if(status == WebSocket::DecodeStatus::kIncomplete)
        {
            break;
        }
        if(status == WebSocket::DecodeStatus::kMalformed)
        {
            std::string frame;
            WebSocket::EncodeClose(WS_CLOSE_PROTOCOL_ERROR, "malformed frame", frame);
            connection.output += frame;
            connection.closing = true;
            break;
        }

        connection.input.erase(0, consumed);

        if(opcode == WebSocket::Opcode::kClose)
        {
            std::string frame;
            WebSocket::EncodeClose(WS_CLOSE_NORMAL, std::string(), frame);
            connection.output += frame;
            connection.closing = true;
            break;
        }

        if(opcode == WebSocket::Opcode::kPing)
        {
            std::string frame;
            WebSocket::Encode(WebSocket::Opcode::kPong, payload, frame);
            connection.output += frame;
            continue;
        }

        if(opcode == WebSocket::Opcode::kPong)
        {
            continue;
        }

        /*
         * Data frames may arrive split; a message is complete only once a
         * frame with the final bit set has been seen.
         */
        if(opcode == WebSocket::Opcode::kContinuation)
        {
            if(!connection.fragmenting)
            {
                std::string frame;
                WebSocket::EncodeClose(WS_CLOSE_PROTOCOL_ERROR,
                                       "continuation without a start", frame);
                connection.output += frame;
                connection.closing = true;
                break;
            }
            connection.fragment += payload;
        }
        else
        {
            connection.fragment        = payload;
            connection.fragment_opcode = opcode;
            connection.fragmenting     = true;
        }

        if(connection.fragment.size() > MAX_MESSAGE_BYTES)
        {
            std::string frame;
            WebSocket::EncodeClose(WS_CLOSE_TOO_LARGE, "message too large", frame);
            connection.output += frame;
            connection.closing = true;
            break;
        }

        if(!is_final)
        {
            continue;
        }

        std::string message;
        message.swap(connection.fragment);
        connection.fragmenting = false;

        std::string reply = execute(session, message, verbose);
        std::string frame;
        WebSocket::Encode(WebSocket::Opcode::kText, reply, frame);
        connection.output += frame;
    }
}

/* Push as much of a connection's output as the socket will take. */
static bool
flush_output(connection_t& connection)
{
    namespace Net = RocProfVis::Middleware::Net;

    bool healthy = true;
    while(!connection.output.empty())
    {
        size_t            sent   = 0;
        Net::TransferStatus status = Net::Send(connection.socket, connection.output.data(),
                                               connection.output.size(), sent);
        if(status == Net::TransferStatus::kWouldBlock)
        {
            break;
        }
        if(status != Net::TransferStatus::kOk)
        {
            healthy = false;
            break;
        }
        if(sent == 0)
        {
            break;
        }
        connection.output.erase(0, sent);
    }
    return healthy;
}

int
main(int argc, char** argv)
{
    namespace Net = RocProfVis::Middleware::Net;

    options_t options = parse_options(argc, argv);
    if(options.show_help)
    {
        print_usage();
        return 0;
    }
    if(options.invalid)
    {
        return 1;
    }

    /*
     * The controller and model log through spdlog's default logger, which
     * writes to stdout. Everything this process says is diagnostics, so it all
     * goes to stderr and stdout stays free for anything piping the output.
     */
    spdlog::set_default_logger(spdlog::stderr_logger_mt("roc-optiq-middleware"));
    spdlog::set_level(options.verbose ? spdlog::level::info : spdlog::level::warn);

    std::string error_message;
    if(!Net::Startup(error_message))
    {
        spdlog::error("{}", error_message);
        return 1;
    }

    Net::socket_t listener = Net::Listen(options.host, options.port, error_message);
    if(listener == Net::InvalidSocket())
    {
        spdlog::error("{}", error_message);
        Net::Shutdown();
        return 1;
    }

    rocprofvis_mw_session_t* session = rocprofvis_mw_session_alloc();
    if(session == nullptr)
    {
        spdlog::error("failed to allocate middleware session");
        Net::Close(listener);
        Net::Shutdown();
        return 1;
    }

    signal(SIGINT, handle_stop_signal);
    signal(SIGTERM, handle_stop_signal);

    /* Always announced, since it is the one thing the operator has to know. */
    std::cerr << "roc-optiq-middleware listening on http://" << options.host << ":"
              << options.port << " (POST /rpc, GET /ws)" << std::endl;
    if(options.host != DEFAULT_HOST)
    {
        std::cerr << "warning: bound beyond loopback; trace open and CSV export "
                     "read and write arbitrary paths"
                  << std::endl;
    }

    std::vector<connection_t> connections;

    while(g_stop_requested == 0)
    {
        std::vector<Net::socket_t> readable;
        std::vector<Net::socket_t> writable;
        readable.push_back(listener);
        for(size_t i = 0; i < connections.size(); i++)
        {
            readable.push_back(connections[i].socket);
            if(!connections[i].output.empty())
            {
                writable.push_back(connections[i].socket);
            }
        }

        if(!Net::WaitForReadiness(readable, writable, POLL_INTERVAL_MS))
        {
            spdlog::error("select failed; shutting down");
            break;
        }

        bool listener_ready = false;
        for(size_t i = 0; i < readable.size(); i++)
        {
            if(readable[i] == listener)
            {
                listener_ready = true;
            }
        }

        if(listener_ready)
        {
            Net::socket_t accepted = Net::Accept(listener);
            while(accepted != Net::InvalidSocket())
            {
                connection_t connection;
                connection.socket          = accepted;
                connection.mode            = Mode::kHttp;
                connection.closing         = false;
                connection.fragmenting     = false;
                connection.fragment_opcode = RocProfVis::Middleware::WebSocket::Opcode::kText;
                connections.push_back(connection);
                accepted = Net::Accept(listener);
            }
        }

        for(size_t i = 0; i < connections.size(); i++)
        {
            connection_t& connection = connections[i];

            bool is_readable = false;
            for(size_t j = 0; j < readable.size(); j++)
            {
                if(readable[j] == connection.socket)
                {
                    is_readable = true;
                }
            }

            if(is_readable)
            {
                char                buffer[READ_CHUNK_BYTES];
                size_t              received = 0;
                Net::TransferStatus status =
                    Net::Receive(connection.socket, buffer, sizeof(buffer), received);
                if(status == Net::TransferStatus::kOk)
                {
                    connection.input.append(buffer, received);
                    if(connection.mode == Mode::kHttp)
                    {
                        pump_http(connection, session, options.verbose);
                    }
                    /*
                     * An upgrade switches mode mid-buffer, so the remaining
                     * bytes are frames and must be drained in the same pass.
                     */
                    if(connection.mode == Mode::kWebSocket)
                    {
                        pump_websocket(connection, session, options.verbose);
                    }
                }
                else if(status != Net::TransferStatus::kWouldBlock)
                {
                    connection.closing = true;
                    connection.output.clear();
                }
            }

            if(!flush_output(connection))
            {
                connection.closing = true;
                connection.output.clear();
            }
        }

        for(size_t i = connections.size(); i > 0; i--)
        {
            connection_t& connection = connections[i - 1];
            if(connection.closing && connection.output.empty())
            {
                Net::Close(connection.socket);
                connections.erase(connections.begin() + static_cast<long>(i - 1));
            }
        }
    }

    std::cerr << "shutting down" << std::endl;
    for(size_t i = 0; i < connections.size(); i++)
    {
        Net::Close(connections[i].socket);
    }
    rocprofvis_mw_session_free(session);
    Net::Close(listener);
    Net::Shutdown();
    return 0;
}
