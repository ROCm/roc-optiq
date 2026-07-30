// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

#include <string>

namespace RocProfVis
{
namespace Middleware
{
namespace WebSocket
{

/*
 * The slice of RFC 6455 this adapter needs: the handshake reply, and a codec
 * for text, ping, pong, and close frames.
 *
 * Binary frames are not produced; the protocol is JSON text either way. Frames
 * arriving from a client are always masked, and frames sent to one never are,
 * which the codec assumes rather than negotiates.
 */

/* Frame kinds this adapter acts on. Anything else is dropped. */
enum class Opcode : uint8_t
{
    kContinuation = 0x0,
    kText         = 0x1,
    kBinary       = 0x2,
    kClose        = 0x8,
    kPing         = 0x9,
    kPong         = 0xA
};

enum class DecodeStatus
{
    /* A whole frame was decoded and consumed from the buffer. */
    kFrame,
    /* The buffer holds part of a frame; read more and try again. */
    kIncomplete,
    /* The frame violates the protocol and the connection should close. */
    kMalformed
};

/*
 * Derive the Sec-WebSocket-Accept value for a client's Sec-WebSocket-Key. The
 * transform is fixed by the specification and is a handshake formality rather
 * than a security measure.
 */
std::string AcceptKey(const std::string& client_key);

/*
 * Decode one frame from the front of buffer.
 *
 * On kFrame, opcode and payload describe it and consumed says how many bytes
 * to drop from the buffer. A fragmented message arrives as a kText or kBinary
 * frame with is_final false, followed by kContinuation frames; the caller
 * joins them.
 */
DecodeStatus Decode(const std::string& buffer, Opcode& opcode, bool& is_final,
                    std::string& payload, size_t& consumed);

/* Encode a frame to send to a client. Never masked, never fragmented. */
void Encode(Opcode opcode, const std::string& payload, std::string& out);

/*
 * Encode a close frame carrying a status code and reason, as required when
 * refusing a message rather than closing abruptly.
 */
void EncodeClose(uint16_t status_code, const std::string& reason, std::string& out);

}  // namespace WebSocket
}  // namespace Middleware
}  // namespace RocProfVis
