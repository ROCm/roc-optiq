// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_mw_websocket.h"

#include <stdint.h>
#include <string.h>

#include <string>

namespace RocProfVis
{
namespace Middleware
{
namespace WebSocket
{

/*
 * Appended to a client's key before hashing. Fixed by RFC 6455 so that a
 * server which does not speak the protocol cannot accidentally produce a
 * valid-looking reply.
 */
static char const* const HANDSHAKE_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static constexpr size_t SHA1_DIGEST_BYTES = 20;
static constexpr size_t SHA1_BLOCK_BYTES  = 64;

/* Payload length encodings: 126 means a 16-bit length follows, 127 a 64-bit. */
static constexpr uint8_t PAYLOAD_LENGTH_16_BIT = 126;
static constexpr uint8_t PAYLOAD_LENGTH_64_BIT = 127;
static constexpr size_t  MASK_BYTES            = 4;

static constexpr uint8_t FIN_BIT      = 0x80;
static constexpr uint8_t OPCODE_MASK  = 0x0F;
static constexpr uint8_t MASK_BIT     = 0x80;
static constexpr uint8_t LENGTH_MASK  = 0x7F;

/*
 * A frame this adapter refuses to buffer. Requests are small; a client sending
 * anything near this is either broken or hostile.
 */
static constexpr uint64_t MAX_FRAME_PAYLOAD = 16ULL * 1024ULL * 1024ULL;

struct sha1_state_t
{
    uint32_t digest[5];
    uint64_t total_bits;
    uint8_t  block[SHA1_BLOCK_BYTES];
    size_t   block_length;
};

static uint32_t
RotateLeft(uint32_t value, uint32_t bits)
{
    return (value << bits) | (value >> (32 - bits));
}

static void
Sha1Init(sha1_state_t& state)
{
    state.digest[0]    = 0x67452301;
    state.digest[1]    = 0xEFCDAB89;
    state.digest[2]    = 0x98BADCFE;
    state.digest[3]    = 0x10325476;
    state.digest[4]    = 0xC3D2E1F0;
    state.total_bits   = 0;
    state.block_length = 0;
}

static void
Sha1Compress(sha1_state_t& state)
{
    uint32_t schedule[80];
    for(size_t i = 0; i < 16; i++)
    {
        schedule[i] = (static_cast<uint32_t>(state.block[i * 4 + 0]) << 24) |
                      (static_cast<uint32_t>(state.block[i * 4 + 1]) << 16) |
                      (static_cast<uint32_t>(state.block[i * 4 + 2]) << 8) |
                      (static_cast<uint32_t>(state.block[i * 4 + 3]));
    }
    for(size_t i = 16; i < 80; i++)
    {
        schedule[i] = RotateLeft(schedule[i - 3] ^ schedule[i - 8] ^ schedule[i - 14] ^
                                     schedule[i - 16],
                                 1);
    }

    uint32_t a = state.digest[0];
    uint32_t b = state.digest[1];
    uint32_t c = state.digest[2];
    uint32_t d = state.digest[3];
    uint32_t e = state.digest[4];

    for(size_t i = 0; i < 80; i++)
    {
        uint32_t mixed = 0;
        uint32_t round = 0;
        if(i < 20)
        {
            mixed = (b & c) | ((~b) & d);
            round = 0x5A827999;
        }
        else if(i < 40)
        {
            mixed = b ^ c ^ d;
            round = 0x6ED9EBA1;
        }
        else if(i < 60)
        {
            mixed = (b & c) | (b & d) | (c & d);
            round = 0x8F1BBCDC;
        }
        else
        {
            mixed = b ^ c ^ d;
            round = 0xCA62C1D6;
        }

        uint32_t next = RotateLeft(a, 5) + mixed + e + round + schedule[i];
        e             = d;
        d             = c;
        c             = RotateLeft(b, 30);
        b             = a;
        a             = next;
    }

    state.digest[0] += a;
    state.digest[1] += b;
    state.digest[2] += c;
    state.digest[3] += d;
    state.digest[4] += e;
}

static void
Sha1Update(sha1_state_t& state, const uint8_t* data, size_t length)
{
    state.total_bits += static_cast<uint64_t>(length) * 8;
    for(size_t i = 0; i < length; i++)
    {
        state.block[state.block_length] = data[i];
        state.block_length++;
        if(state.block_length == SHA1_BLOCK_BYTES)
        {
            Sha1Compress(state);
            state.block_length = 0;
        }
    }
}

static void
Sha1Finish(sha1_state_t& state, uint8_t digest[SHA1_DIGEST_BYTES])
{
    uint64_t total_bits = state.total_bits;

    uint8_t terminator = 0x80;
    Sha1Update(state, &terminator, 1);

    uint8_t zero = 0x00;
    while(state.block_length != SHA1_BLOCK_BYTES - sizeof(uint64_t))
    {
        Sha1Update(state, &zero, 1);
    }

    /* The length is appended raw, so it must not extend total_bits again. */
    for(int shift = 56; shift >= 0; shift -= 8)
    {
        state.block[state.block_length] =
            static_cast<uint8_t>((total_bits >> shift) & 0xFF);
        state.block_length++;
    }
    Sha1Compress(state);

    for(size_t i = 0; i < SHA1_DIGEST_BYTES; i++)
    {
        digest[i] = static_cast<uint8_t>((state.digest[i / 4] >> (24 - (i % 4) * 8)) & 0xFF);
    }
}

static std::string
Base64(const uint8_t* data, size_t length)
{
    static char const* const ALPHABET =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string encoded;
    encoded.reserve(((length + 2) / 3) * 4);

    size_t index = 0;
    while(index + 2 < length)
    {
        uint32_t triple = (static_cast<uint32_t>(data[index]) << 16) |
                          (static_cast<uint32_t>(data[index + 1]) << 8) |
                          static_cast<uint32_t>(data[index + 2]);
        encoded += ALPHABET[(triple >> 18) & 0x3F];
        encoded += ALPHABET[(triple >> 12) & 0x3F];
        encoded += ALPHABET[(triple >> 6) & 0x3F];
        encoded += ALPHABET[triple & 0x3F];
        index += 3;
    }

    size_t remaining = length - index;
    if(remaining == 1)
    {
        uint32_t triple = static_cast<uint32_t>(data[index]) << 16;
        encoded += ALPHABET[(triple >> 18) & 0x3F];
        encoded += ALPHABET[(triple >> 12) & 0x3F];
        encoded += "==";
    }
    else if(remaining == 2)
    {
        uint32_t triple = (static_cast<uint32_t>(data[index]) << 16) |
                          (static_cast<uint32_t>(data[index + 1]) << 8);
        encoded += ALPHABET[(triple >> 18) & 0x3F];
        encoded += ALPHABET[(triple >> 12) & 0x3F];
        encoded += ALPHABET[(triple >> 6) & 0x3F];
        encoded += '=';
    }
    return encoded;
}

std::string
AcceptKey(const std::string& client_key)
{
    std::string salted = client_key + HANDSHAKE_GUID;

    sha1_state_t state;
    Sha1Init(state);
    Sha1Update(state, reinterpret_cast<const uint8_t*>(salted.data()), salted.size());

    uint8_t digest[SHA1_DIGEST_BYTES];
    Sha1Finish(state, digest);
    return Base64(digest, SHA1_DIGEST_BYTES);
}

DecodeStatus
Decode(const std::string& buffer, Opcode& opcode, bool& is_final, std::string& payload,
       size_t& consumed)
{
    consumed = 0;
    payload.clear();

    /* Every frame starts with the flags byte and the first length byte. */
    if(buffer.size() < 2)
    {
        return DecodeStatus::kIncomplete;
    }

    uint8_t first  = static_cast<uint8_t>(buffer[0]);
    uint8_t second = static_cast<uint8_t>(buffer[1]);

    is_final       = (first & FIN_BIT) != 0;
    opcode         = static_cast<Opcode>(first & OPCODE_MASK);
    bool is_masked = (second & MASK_BIT) != 0;

    /* A client that does not mask is in violation and cannot be trusted. */
    if(!is_masked)
    {
        return DecodeStatus::kMalformed;
    }

    uint64_t length = second & LENGTH_MASK;
    size_t   cursor = 2;

    if(length == PAYLOAD_LENGTH_16_BIT)
    {
        if(buffer.size() < cursor + sizeof(uint16_t))
        {
            return DecodeStatus::kIncomplete;
        }
        length = (static_cast<uint64_t>(static_cast<uint8_t>(buffer[cursor])) << 8) |
                 static_cast<uint64_t>(static_cast<uint8_t>(buffer[cursor + 1]));
        cursor += sizeof(uint16_t);
    }
    else if(length == PAYLOAD_LENGTH_64_BIT)
    {
        if(buffer.size() < cursor + sizeof(uint64_t))
        {
            return DecodeStatus::kIncomplete;
        }
        length = 0;
        for(size_t i = 0; i < sizeof(uint64_t); i++)
        {
            length = (length << 8) |
                     static_cast<uint64_t>(static_cast<uint8_t>(buffer[cursor + i]));
        }
        cursor += sizeof(uint64_t);
    }

    if(length > MAX_FRAME_PAYLOAD)
    {
        return DecodeStatus::kMalformed;
    }

    if(buffer.size() < cursor + MASK_BYTES)
    {
        return DecodeStatus::kIncomplete;
    }
    uint8_t mask[MASK_BYTES];
    for(size_t i = 0; i < MASK_BYTES; i++)
    {
        mask[i] = static_cast<uint8_t>(buffer[cursor + i]);
    }
    cursor += MASK_BYTES;

    if(buffer.size() < cursor + length)
    {
        return DecodeStatus::kIncomplete;
    }

    payload.resize(static_cast<size_t>(length));
    for(uint64_t i = 0; i < length; i++)
    {
        payload[static_cast<size_t>(i)] = static_cast<char>(
            static_cast<uint8_t>(buffer[cursor + static_cast<size_t>(i)]) ^
            mask[i % MASK_BYTES]);
    }

    consumed = cursor + static_cast<size_t>(length);
    return DecodeStatus::kFrame;
}

void
Encode(Opcode opcode, const std::string& payload, std::string& out)
{
    out.clear();
    out.reserve(payload.size() + 10);

    out += static_cast<char>(FIN_BIT | static_cast<uint8_t>(opcode));

    size_t length = payload.size();
    if(length < PAYLOAD_LENGTH_16_BIT)
    {
        out += static_cast<char>(static_cast<uint8_t>(length));
    }
    else if(length <= UINT16_MAX)
    {
        out += static_cast<char>(PAYLOAD_LENGTH_16_BIT);
        out += static_cast<char>(static_cast<uint8_t>((length >> 8) & 0xFF));
        out += static_cast<char>(static_cast<uint8_t>(length & 0xFF));
    }
    else
    {
        out += static_cast<char>(PAYLOAD_LENGTH_64_BIT);
        for(int shift = 56; shift >= 0; shift -= 8)
        {
            out += static_cast<char>(
                static_cast<uint8_t>((static_cast<uint64_t>(length) >> shift) & 0xFF));
        }
    }

    out += payload;
}

void
EncodeClose(uint16_t status_code, const std::string& reason, std::string& out)
{
    std::string payload;
    payload += static_cast<char>(static_cast<uint8_t>((status_code >> 8) & 0xFF));
    payload += static_cast<char>(static_cast<uint8_t>(status_code & 0xFF));
    payload += reason;
    Encode(Opcode::kClose, payload, out);
}

}  // namespace WebSocket
}  // namespace Middleware
}  // namespace RocProfVis
