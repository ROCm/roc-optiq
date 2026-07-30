// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

#include <map>
#include <string>

namespace RocProfVis
{
namespace Middleware
{
namespace Http
{

/*
 * Enough HTTP/1.1 to accept a JSON request and hand back a JSON response, or
 * to recognise a WebSocket upgrade.
 *
 * Only Content-Length bodies are understood; chunked transfer encoding is
 * rejected rather than mishandled. That is sufficient for a JSON API, and a
 * client sending chunked gets a clear status instead of a truncated read.
 */

struct request_t
{
    std::string method;
    std::string target;
    /* Header names are lowercased on the way in so lookups are unambiguous. */
    std::map<std::string, std::string> headers;
    std::string                        body;
};

enum class ParseStatus
{
    /* A whole request was parsed and consumed from the buffer. */
    kComplete,
    /* The buffer holds part of a request; read more and try again. */
    kIncomplete,
    kMalformed,
    /* Well formed, but larger than the adapter is willing to buffer. */
    kTooLarge,
    /* Well formed, but using a feature this parser does not implement. */
    kUnsupported
};

/*
 * Parse one request from the front of buffer. On kComplete, consumed says how
 * many bytes to drop. On any failing status, status_code carries the HTTP
 * status to answer with.
 */
ParseStatus Parse(const std::string& buffer, request_t& out, size_t& consumed,
                  uint16_t& status_code);

/* Case-insensitive header lookup; returns an empty string when absent. */
std::string Header(const request_t& request, const std::string& name);

/* True when the request asks to be upgraded to a WebSocket. */
bool IsWebSocketUpgrade(const request_t& request);

/*
 * Build a complete response. The body is sent as-is with the given content
 * type; CORS headers are always included so a browser frontend served from
 * another origin can call the API.
 */
std::string Response(uint16_t status_code, const std::string& content_type,
                     const std::string& body);

/* Build the 101 reply that completes a WebSocket handshake. */
std::string UpgradeResponse(const std::string& accept_key);

/* Reason phrase for a status code, for the response line. */
char const* ReasonPhrase(uint16_t status_code);

}  // namespace Http
}  // namespace Middleware
}  // namespace RocProfVis
