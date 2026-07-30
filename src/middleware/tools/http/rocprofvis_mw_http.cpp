// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_mw_http.h"

#include <stdint.h>
#include <stdlib.h>

#include <map>
#include <string>

namespace RocProfVis
{
namespace Middleware
{
namespace Http
{

/*
 * Caps on what the adapter will buffer for one request. Requests are small
 * JSON documents; these exist so a malformed or hostile client cannot make the
 * server allocate without bound.
 */
static constexpr size_t MAX_HEADER_BYTES = 32ULL * 1024ULL;
static constexpr size_t MAX_BODY_BYTES   = 8ULL * 1024ULL * 1024ULL;

static char const* const HEADER_TERMINATOR = "\r\n\r\n";
static char const* const LINE_TERMINATOR   = "\r\n";

static std::string
ToLower(const std::string& text)
{
    std::string lowered = text;
    for(size_t i = 0; i < lowered.size(); i++)
    {
        if(lowered[i] >= 'A' && lowered[i] <= 'Z')
        {
            lowered[i] = static_cast<char>(lowered[i] - 'A' + 'a');
        }
    }
    return lowered;
}

static std::string
Trim(const std::string& text)
{
    size_t first = text.find_first_not_of(" \t");
    if(first == std::string::npos)
    {
        return std::string();
    }
    size_t last = text.find_last_not_of(" \t");
    return text.substr(first, last - first + 1);
}

/* True when haystack contains needle, ignoring case. */
static bool
ContainsInsensitive(const std::string& haystack, const std::string& needle)
{
    return ToLower(haystack).find(ToLower(needle)) != std::string::npos;
}

ParseStatus
Parse(const std::string& buffer, request_t& out, size_t& consumed,
      uint16_t& status_code)
{
    consumed    = 0;
    status_code = 200;

    size_t header_end = buffer.find(HEADER_TERMINATOR);
    if(header_end == std::string::npos)
    {
        if(buffer.size() > MAX_HEADER_BYTES)
        {
            status_code = 431;
            return ParseStatus::kTooLarge;
        }
        return ParseStatus::kIncomplete;
    }

    size_t line_end = buffer.find(LINE_TERMINATOR);
    if(line_end == std::string::npos || line_end > header_end)
    {
        status_code = 400;
        return ParseStatus::kMalformed;
    }

    /* Request line: METHOD SP TARGET SP VERSION */
    std::string request_line   = buffer.substr(0, line_end);
    size_t      method_end     = request_line.find(' ');
    if(method_end == std::string::npos)
    {
        status_code = 400;
        return ParseStatus::kMalformed;
    }
    size_t target_end = request_line.find(' ', method_end + 1);
    if(target_end == std::string::npos)
    {
        status_code = 400;
        return ParseStatus::kMalformed;
    }

    out.method = request_line.substr(0, method_end);
    out.target = request_line.substr(method_end + 1, target_end - method_end - 1);
    out.headers.clear();
    out.body.clear();

    size_t cursor = line_end + 2;
    while(cursor < header_end)
    {
        size_t next = buffer.find(LINE_TERMINATOR, cursor);
        if(next == std::string::npos || next > header_end)
        {
            next = header_end;
        }

        std::string line  = buffer.substr(cursor, next - cursor);
        size_t      colon = line.find(':');
        if(colon != std::string::npos)
        {
            /*
             * Repeated headers are joined with a comma, which is how the
             * specification says a recipient may combine them.
             */
            std::string name  = ToLower(Trim(line.substr(0, colon)));
            std::string value = Trim(line.substr(colon + 1));
            std::map<std::string, std::string>::iterator existing =
                out.headers.find(name);
            if(existing == out.headers.end())
            {
                out.headers[name] = value;
            }
            else
            {
                existing->second += "," + value;
            }
        }
        cursor = next + 2;
    }

    if(!Header(out, "transfer-encoding").empty())
    {
        status_code = 411;
        return ParseStatus::kUnsupported;
    }

    size_t      body_start     = header_end + 4;
    std::string content_length = Header(out, "content-length");
    size_t      body_length    = 0;
    if(!content_length.empty())
    {
        char*              end    = nullptr;
        unsigned long long parsed = strtoull(content_length.c_str(), &end, 10);
        if(end == nullptr || *end != '\0')
        {
            status_code = 400;
            return ParseStatus::kMalformed;
        }
        if(parsed > MAX_BODY_BYTES)
        {
            status_code = 413;
            return ParseStatus::kTooLarge;
        }
        body_length = static_cast<size_t>(parsed);
    }

    if(buffer.size() < body_start + body_length)
    {
        return ParseStatus::kIncomplete;
    }

    out.body = buffer.substr(body_start, body_length);
    consumed = body_start + body_length;
    return ParseStatus::kComplete;
}

std::string
Header(const request_t& request, const std::string& name)
{
    std::string                                        value;
    std::map<std::string, std::string>::const_iterator found =
        request.headers.find(ToLower(name));
    if(found != request.headers.end())
    {
        value = found->second;
    }
    return value;
}

bool
IsWebSocketUpgrade(const request_t& request)
{
    /*
     * Both header values are lists in principle, and browsers send
     * "Connection: keep-alive, Upgrade", so match on containment.
     */
    return ContainsInsensitive(Header(request, "upgrade"), "websocket") &&
           ContainsInsensitive(Header(request, "connection"), "upgrade") &&
           !Header(request, "sec-websocket-key").empty();
}

char const*
ReasonPhrase(uint16_t status_code)
{
    char const* phrase = "OK";
    switch(status_code)
    {
        case 101: phrase = "Switching Protocols"; break;
        case 200: phrase = "OK"; break;
        case 204: phrase = "No Content"; break;
        case 400: phrase = "Bad Request"; break;
        case 403: phrase = "Forbidden"; break;
        case 404: phrase = "Not Found"; break;
        case 405: phrase = "Method Not Allowed"; break;
        case 411: phrase = "Length Required"; break;
        case 413: phrase = "Payload Too Large"; break;
        case 426: phrase = "Upgrade Required"; break;
        case 431: phrase = "Request Header Fields Too Large"; break;
        case 500: phrase = "Internal Server Error"; break;
        default: break;
    }
    return phrase;
}

std::string
Response(uint16_t status_code, const std::string& content_type, const std::string& body)
{
    std::string response;
    response.reserve(body.size() + 256);

    response += "HTTP/1.1 ";
    response += std::to_string(status_code);
    response += " ";
    response += ReasonPhrase(status_code);
    response += LINE_TERMINATOR;

    response += "Content-Type: ";
    response += content_type;
    response += LINE_TERMINATOR;

    response += "Content-Length: ";
    response += std::to_string(body.size());
    response += LINE_TERMINATOR;

    /*
     * A frontend served from a dev server is a different origin from this
     * process, so without these a browser refuses to show it the response.
     */
    response += "Access-Control-Allow-Origin: *" ;
    response += LINE_TERMINATOR;
    response += "Access-Control-Allow-Methods: GET, POST, OPTIONS";
    response += LINE_TERMINATOR;
    response += "Access-Control-Allow-Headers: Content-Type";
    response += LINE_TERMINATOR;
    response += "Access-Control-Max-Age: 86400";
    response += LINE_TERMINATOR;

    response += "Connection: keep-alive";
    response += LINE_TERMINATOR;
    response += LINE_TERMINATOR;

    response += body;
    return response;
}

std::string
UpgradeResponse(const std::string& accept_key)
{
    std::string response;
    response += "HTTP/1.1 101 ";
    response += ReasonPhrase(101);
    response += LINE_TERMINATOR;
    response += "Upgrade: websocket";
    response += LINE_TERMINATOR;
    response += "Connection: Upgrade";
    response += LINE_TERMINATOR;
    response += "Sec-WebSocket-Accept: ";
    response += accept_key;
    response += LINE_TERMINATOR;
    response += LINE_TERMINATOR;
    return response;
}

}  // namespace Http
}  // namespace Middleware
}  // namespace RocProfVis
