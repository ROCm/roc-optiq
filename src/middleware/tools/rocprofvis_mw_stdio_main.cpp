// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include <stdio.h>

#include <iostream>
#include <string>

#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/spdlog.h"

#include "rocprofvis_middleware.h"

/*
 * Newline-delimited JSON adapter for the middleware.
 *
 * Reads one JSON request object per line from stdin and writes one JSON
 * response object per line to stdout, flushing after each so an interactive
 * client is never left waiting on a buffer. Requests are served strictly in
 * order on a single session; concurrency comes from the controller's own
 * worker threads, which a client reaches by issuing a fetch and then polling
 * request.poll rather than by pipelining requests.
 *
 * The framing is deliberately trivial so that a frontend can drive this over a
 * child-process pipe with no dependency beyond a JSON parser. An HTTP or
 * WebSocket adapter would replace this file and nothing else.
 */
int
main(int argc, char** argv)
{
    (void) argc;
    (void) argv;

    /*
     * stdout carries the protocol and nothing else. The controller and model
     * log through spdlog's default logger, which writes to stdout, so the
     * default is replaced before any of their code can run; otherwise a log
     * line lands mid-stream and the client sees it as a malformed response.
     */
    spdlog::set_default_logger(spdlog::stderr_logger_mt("roc-optiq-middleware"));
    spdlog::set_level(spdlog::level::warn);

    /*
     * A trace payload can be many megabytes of JSON, so stdout is left fully
     * buffered and flushed explicitly per response.
     */
    std::ios::sync_with_stdio(false);

    rocprofvis_mw_session_t* session = rocprofvis_mw_session_alloc();
    if(session == nullptr)
    {
        std::cerr << "failed to allocate middleware session" << std::endl;
        return 1;
    }

    std::string line;
    while(std::getline(std::cin, line))
    {
        /* Tolerate CRLF input from a Windows client. */
        while(!line.empty() && (line.back() == '\r' || line.back() == '\n'))
        {
            line.pop_back();
        }
        if(line.empty())
        {
            continue;
        }

        char* response = rocprofvis_mw_request(session, line.c_str());
        if(response == nullptr)
        {
            std::cout << "{\"ok\":false,\"error\":{\"code\":\"internal_error\","
                         "\"message\":\"request failed\"}}\n";
        }
        else
        {
            std::cout << response << "\n";
            rocprofvis_mw_string_free(response);
        }
        std::cout.flush();
    }

    rocprofvis_mw_session_free(session);
    return 0;
}
