// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include <catch2/catch_test_macros.hpp>

#include <stdint.h>

#include <string>
#include <vector>

#include "rocprofvis_mw_http.h"
#include "rocprofvis_mw_websocket.h"

/*
 * Tests for the HTTP and WebSocket codecs behind the network adapter.
 *
 * These are the hand-written parts of the transport, so they are exercised
 * directly rather than through a live socket: the failures worth catching here
 * are framing and parsing mistakes, which a running server would only surface
 * as a hang or a dropped connection.
 */

namespace Http      = RocProfVis::Middleware::Http;
namespace WebSocket = RocProfVis::Middleware::WebSocket;

/*
 * Frame a payload the way a client does: always masked, optionally as one
 * fragment of a larger message.
 */
static std::string
MaskedFrame(WebSocket::Opcode opcode, const std::string& payload, bool is_final = true)
{
    const uint8_t mask[4] = { 0x37, 0xFA, 0x21, 0x3D };

    std::string frame;
    frame += static_cast<char>((is_final ? 0x80 : 0x00) |
                               static_cast<uint8_t>(opcode));

    size_t length = payload.size();
    if(length < 126)
    {
        frame += static_cast<char>(0x80 | static_cast<uint8_t>(length));
    }
    else if(length <= UINT16_MAX)
    {
        frame += static_cast<char>(0x80 | 126);
        frame += static_cast<char>(static_cast<uint8_t>((length >> 8) & 0xFF));
        frame += static_cast<char>(static_cast<uint8_t>(length & 0xFF));
    }
    else
    {
        frame += static_cast<char>(0x80 | 127);
        for(int shift = 56; shift >= 0; shift -= 8)
        {
            frame += static_cast<char>(
                static_cast<uint8_t>((static_cast<uint64_t>(length) >> shift) & 0xFF));
        }
    }

    for(size_t i = 0; i < 4; i++)
    {
        frame += static_cast<char>(mask[i]);
    }
    for(size_t i = 0; i < payload.size(); i++)
    {
        frame += static_cast<char>(static_cast<uint8_t>(payload[i]) ^ mask[i % 4]);
    }
    return frame;
}

static std::string
PostRequest(const std::string& target, const std::string& body)
{
    return "POST " + target +
           " HTTP/1.1\r\n"
           "Host: localhost\r\n"
           "Content-Type: application/json\r\n"
           "Content-Length: " +
           std::to_string(body.size()) + "\r\n\r\n" + body;
}

TEST_CASE("The handshake reply matches the specification's worked example")
{
    /*
     * RFC 6455 section 1.3 pairs this key with this accept value, so it
     * pins the SHA-1 and base64 behind the handshake to a known answer.
     */
    REQUIRE(WebSocket::AcceptKey("dGhlIHNhbXBsZSBub25jZQ==") ==
            "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
}

TEST_CASE("The digest behind the handshake holds at block boundaries")
{
    /*
     * SHA-1 pads to a 64-byte block with the length in the last 8 bytes, so
     * inputs either side of that boundary take different padding paths. Every
     * digest is 20 bytes, which base64 always spells in 28 characters.
     */
    REQUIRE(WebSocket::AcceptKey("").size() == 28);
    REQUIRE(WebSocket::AcceptKey(std::string(64, 'a')).size() == 28);
    REQUIRE(WebSocket::AcceptKey(std::string(1024, 'z')).size() == 28);

    /* Distinct keys must not collide into the same accept value. */
    REQUIRE(WebSocket::AcceptKey("a") != WebSocket::AcceptKey("b"));
    REQUIRE(WebSocket::AcceptKey(std::string(20, 'a')) !=
            WebSocket::AcceptKey(std::string(21, 'a')));
}

TEST_CASE("A masked text frame decodes to its payload")
{
    std::string payload = "{\"method\":\"session.info\"}";
    std::string buffer  = MaskedFrame(WebSocket::Opcode::kText, payload);

    WebSocket::Opcode opcode   = WebSocket::Opcode::kBinary;
    bool              is_final = false;
    std::string       decoded;
    size_t            consumed = 0;

    REQUIRE(WebSocket::Decode(buffer, opcode, is_final, decoded, consumed) ==
            WebSocket::DecodeStatus::kFrame);
    REQUIRE(opcode == WebSocket::Opcode::kText);
    REQUIRE(is_final);
    REQUIRE(decoded == payload);
    REQUIRE(consumed == buffer.size());
}

TEST_CASE("Payload length encodings are handled at their boundaries")
{
    /* 125 is the last inline length; 126 and 65536 switch encoding. */
    const size_t lengths[] = { 0, 1, 125, 126, 127, 65535, 65536, 70000 };

    for(size_t length : lengths)
    {
        std::string payload(length, 'x');
        std::string buffer = MaskedFrame(WebSocket::Opcode::kText, payload);

        WebSocket::Opcode opcode   = WebSocket::Opcode::kBinary;
        bool              is_final = false;
        std::string       decoded;
        size_t            consumed = 0;

        INFO("payload length " << length);
        REQUIRE(WebSocket::Decode(buffer, opcode, is_final, decoded, consumed) ==
                WebSocket::DecodeStatus::kFrame);
        REQUIRE(decoded.size() == length);
        REQUIRE(decoded == payload);
        REQUIRE(consumed == buffer.size());
    }
}

TEST_CASE("Frames the server sends round trip through their own length encodings")
{
    const size_t lengths[] = { 0, 125, 126, 65535, 65536 };

    for(size_t length : lengths)
    {
        std::string payload(length, 'y');
        std::string encoded;
        WebSocket::Encode(WebSocket::Opcode::kText, payload, encoded);

        INFO("payload length " << length);

        /* A server frame is never masked, and carries the whole message. */
        REQUIRE((static_cast<uint8_t>(encoded[0]) & 0x80) != 0);
        REQUIRE((static_cast<uint8_t>(encoded[1]) & 0x80) == 0);
        REQUIRE(encoded.size() >= payload.size());
        REQUIRE(encoded.compare(encoded.size() - payload.size(), payload.size(),
                                payload) == 0);
    }
}

TEST_CASE("A partial frame asks for more rather than guessing")
{
    std::string whole = MaskedFrame(WebSocket::Opcode::kText, std::string(300, 'a'));

    /* Every truncation of a frame must be reported as incomplete. */
    for(size_t prefix = 0; prefix < whole.size(); prefix++)
    {
        WebSocket::Opcode opcode   = WebSocket::Opcode::kText;
        bool              is_final = false;
        std::string       decoded;
        size_t            consumed = 0;

        INFO("prefix length " << prefix);
        REQUIRE(WebSocket::Decode(whole.substr(0, prefix), opcode, is_final, decoded,
                                  consumed) == WebSocket::DecodeStatus::kIncomplete);
    }
}

TEST_CASE("An unmasked client frame is refused")
{
    /* A client that does not mask is in violation of the specification. */
    std::string buffer;
    buffer += static_cast<char>(0x81);
    buffer += static_cast<char>(0x02);
    buffer += "hi";

    WebSocket::Opcode opcode   = WebSocket::Opcode::kText;
    bool              is_final = false;
    std::string       decoded;
    size_t            consumed = 0;

    REQUIRE(WebSocket::Decode(buffer, opcode, is_final, decoded, consumed) ==
            WebSocket::DecodeStatus::kMalformed);
}

TEST_CASE("A message split across frames is reported one frame at a time")
{
    std::string buffer = MaskedFrame(WebSocket::Opcode::kText, "{\"me", false) +
                         MaskedFrame(WebSocket::Opcode::kContinuation, "thod\":", false) +
                         MaskedFrame(WebSocket::Opcode::kContinuation, "\"x\"}", true);

    std::string       joined;
    WebSocket::Opcode first_opcode = WebSocket::Opcode::kBinary;
    bool              saw_final    = false;
    size_t            frames       = 0;

    while(!buffer.empty())
    {
        WebSocket::Opcode opcode   = WebSocket::Opcode::kBinary;
        bool              is_final = false;
        std::string       payload;
        size_t            consumed = 0;

        REQUIRE(WebSocket::Decode(buffer, opcode, is_final, payload, consumed) ==
                WebSocket::DecodeStatus::kFrame);
        if(frames == 0)
        {
            first_opcode = opcode;
        }
        joined += payload;
        saw_final = is_final;
        frames++;
        buffer.erase(0, consumed);
    }

    REQUIRE(frames == 3);
    REQUIRE(first_opcode == WebSocket::Opcode::kText);
    REQUIRE(saw_final);
    REQUIRE(joined == "{\"method\":\"x\"}");
}

TEST_CASE("Control frames decode alongside data frames")
{
    std::string buffer = MaskedFrame(WebSocket::Opcode::kPing, "keepalive") +
                         MaskedFrame(WebSocket::Opcode::kClose, std::string());

    WebSocket::Opcode opcode   = WebSocket::Opcode::kText;
    bool              is_final = false;
    std::string       payload;
    size_t            consumed = 0;

    REQUIRE(WebSocket::Decode(buffer, opcode, is_final, payload, consumed) ==
            WebSocket::DecodeStatus::kFrame);
    REQUIRE(opcode == WebSocket::Opcode::kPing);
    REQUIRE(payload == "keepalive");
    buffer.erase(0, consumed);

    REQUIRE(WebSocket::Decode(buffer, opcode, is_final, payload, consumed) ==
            WebSocket::DecodeStatus::kFrame);
    REQUIRE(opcode == WebSocket::Opcode::kClose);
}

TEST_CASE("A close frame carries its status code in network order")
{
    std::string encoded;
    WebSocket::EncodeClose(1002, "malformed frame", encoded);

    /* Two header bytes, then the code, then the reason. */
    REQUIRE(encoded.size() > 4);
    uint16_t code = static_cast<uint16_t>(
        (static_cast<uint8_t>(encoded[2]) << 8) | static_cast<uint8_t>(encoded[3]));
    REQUIRE(code == 1002);
    REQUIRE(encoded.find("malformed frame") != std::string::npos);
}

TEST_CASE("A complete HTTP request parses into its parts")
{
    std::string body   = "{\"method\":\"session.info\"}";
    std::string buffer = PostRequest("/rpc", body);

    Http::request_t request;
    size_t          consumed    = 0;
    uint16_t        status_code = 0;

    REQUIRE(Http::Parse(buffer, request, consumed, status_code) ==
            Http::ParseStatus::kComplete);
    REQUIRE(request.method == "POST");
    REQUIRE(request.target == "/rpc");
    REQUIRE(request.body == body);
    REQUIRE(consumed == buffer.size());
}

TEST_CASE("Header names are matched without regard to case")
{
    std::string buffer =
        "GET /ws HTTP/1.1\r\n"
        "HOST: localhost\r\n"
        "Content-Type: application/json\r\n"
        "\r\n";

    Http::request_t request;
    size_t          consumed    = 0;
    uint16_t        status_code = 0;

    REQUIRE(Http::Parse(buffer, request, consumed, status_code) ==
            Http::ParseStatus::kComplete);
    REQUIRE(Http::Header(request, "host") == "localhost");
    REQUIRE(Http::Header(request, "Host") == "localhost");
    REQUIRE(Http::Header(request, "CONTENT-TYPE") == "application/json");
    REQUIRE(Http::Header(request, "absent").empty());
}

TEST_CASE("A request arriving in pieces is only parsed once it is whole")
{
    std::string body   = "{\"method\":\"trace.status\"}";
    std::string buffer = PostRequest("/rpc", body);

    for(size_t prefix = 0; prefix < buffer.size(); prefix++)
    {
        Http::request_t request;
        size_t          consumed    = 0;
        uint16_t        status_code = 0;

        INFO("prefix length " << prefix);
        REQUIRE(Http::Parse(buffer.substr(0, prefix), request, consumed, status_code) ==
                Http::ParseStatus::kIncomplete);
    }
}

TEST_CASE("Pipelined requests are consumed one at a time")
{
    std::string first  = PostRequest("/rpc", "{\"id\":1}");
    std::string second = PostRequest("/rpc", "{\"id\":22}");
    std::string buffer = first + second;

    Http::request_t request;
    size_t          consumed    = 0;
    uint16_t        status_code = 0;

    REQUIRE(Http::Parse(buffer, request, consumed, status_code) ==
            Http::ParseStatus::kComplete);
    REQUIRE(request.body == "{\"id\":1}");
    REQUIRE(consumed == first.size());
    buffer.erase(0, consumed);

    REQUIRE(Http::Parse(buffer, request, consumed, status_code) ==
            Http::ParseStatus::kComplete);
    REQUIRE(request.body == "{\"id\":22}");
    REQUIRE(consumed == buffer.size());
}

TEST_CASE("Requests the parser will not serve report a status to answer with")
{
    Http::request_t request;
    size_t          consumed    = 0;
    uint16_t        status_code = 0;

    /* Chunked bodies are not implemented, and must not be read as empty. */
    std::string chunked =
        "POST /rpc HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n";
    REQUIRE(Http::Parse(chunked, request, consumed, status_code) ==
            Http::ParseStatus::kUnsupported);
    REQUIRE(status_code == 411);

    std::string no_version = "POST\r\nHost: x\r\n\r\n";
    REQUIRE(Http::Parse(no_version, request, consumed, status_code) ==
            Http::ParseStatus::kMalformed);
    REQUIRE(status_code == 400);

    std::string bad_length =
        "POST /rpc HTTP/1.1\r\nHost: x\r\nContent-Length: abc\r\n\r\n";
    REQUIRE(Http::Parse(bad_length, request, consumed, status_code) ==
            Http::ParseStatus::kMalformed);
    REQUIRE(status_code == 400);

    /* A body larger than the adapter will buffer is refused up front. */
    std::string huge =
        "POST /rpc HTTP/1.1\r\nHost: x\r\nContent-Length: 999999999\r\n\r\n";
    REQUIRE(Http::Parse(huge, request, consumed, status_code) ==
            Http::ParseStatus::kTooLarge);
    REQUIRE(status_code == 413);
}

TEST_CASE("A WebSocket upgrade is recognised the way browsers send it")
{
    std::string buffer =
        "GET /ws HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Upgrade: websocket\r\n"
        "Connection: keep-alive, Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";

    Http::request_t request;
    size_t          consumed    = 0;
    uint16_t        status_code = 0;

    REQUIRE(Http::Parse(buffer, request, consumed, status_code) ==
            Http::ParseStatus::kComplete);
    REQUIRE(Http::IsWebSocketUpgrade(request));

    /* A plain GET to the same path is not an upgrade. */
    std::string plain = "GET /ws HTTP/1.1\r\nHost: localhost\r\n\r\n";
    REQUIRE(Http::Parse(plain, request, consumed, status_code) ==
            Http::ParseStatus::kComplete);
    REQUIRE(!Http::IsWebSocketUpgrade(request));

    /* An upgrade without a key cannot be answered and is not accepted. */
    std::string keyless =
        "GET /ws HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n\r\n";
    REQUIRE(Http::Parse(keyless, request, consumed, status_code) ==
            Http::ParseStatus::kComplete);
    REQUIRE(!Http::IsWebSocketUpgrade(request));
}

TEST_CASE("Responses carry the headers a browser client needs")
{
    std::string body     = "{\"ok\":true}";
    std::string response = Http::Response(200, "application/json", body);

    REQUIRE(response.find("HTTP/1.1 200 OK\r\n") == 0);
    REQUIRE(response.find("Content-Length: " + std::to_string(body.size())) !=
            std::string::npos);
    REQUIRE(response.find("Access-Control-Allow-Origin: *") != std::string::npos);

    /* The body must follow the blank line untouched. */
    size_t separator = response.find("\r\n\r\n");
    REQUIRE(separator != std::string::npos);
    REQUIRE(response.substr(separator + 4) == body);
}

TEST_CASE("The upgrade response quotes the accept key back")
{
    std::string accept   = WebSocket::AcceptKey("dGhlIHNhbXBsZSBub25jZQ==");
    std::string response = Http::UpgradeResponse(accept);

    REQUIRE(response.find("HTTP/1.1 101 Switching Protocols\r\n") == 0);
    REQUIRE(response.find("Upgrade: websocket") != std::string::npos);
    REQUIRE(response.find("Sec-WebSocket-Accept: " + accept) != std::string::npos);
    REQUIRE(response.find("\r\n\r\n") == response.size() - 4);
}
