// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace jt
{
class Json;
}

namespace RocProfVis
{
namespace View
{

/*
 * The frames the client/server protocol sends, all binary WebSocket frames,
 * told apart by the first byte:
 *   '{'  a JSON message
 *   'Z'  a JSON message, zlib-compressed: 'Z', raw size (u64 LE), zlib stream
 *   'F'  a file chunk, server to client: 'F', trace id (u32 LE), request id
 *        (u64 LE), bytes
 *   'U'  an upload chunk, client to server: 'U', upload id (u64 LE), bytes
 *   'P'  a piece of a larger frame, more to follow: 'P', bytes
 *   'E'  the last piece: 'E', bytes; the pieces' bytes joined are the frame
 * Senders compress messages of COMPRESSION_THRESHOLD bytes or more; a browser
 * can inflate them with DecompressionStream("deflate").
 *
 * Over TLS the server sends any frame larger than MAX_TLS_FRAME_SIZE in
 * pieces: cpp-httplib before 0.64 hands an unmasked WebSocket message to the
 * TLS stream in one write, which sends one TLS record and drops the rest.
 */
constexpr char   FRAME_TAG_JSON         = '{';
constexpr char   FRAME_TAG_COMPRESSED   = 'Z';
constexpr char   FRAME_TAG_FILE_CHUNK   = 'F';
constexpr char   FRAME_TAG_UPLOAD_CHUNK = 'U';
constexpr char   FRAME_TAG_PIECE        = 'P';
constexpr char   FRAME_TAG_LAST_PIECE   = 'E';
constexpr size_t COMPRESSION_THRESHOLD  = 16 * 1024;
// The most plaintext one TLS record carries.
constexpr size_t MAX_TLS_FRAME_SIZE = 16 * 1024;

std::string encode_frame(const jt::Json& message);
// False for a chunk frame or input that does not decode to a JSON object.
bool        decode_frame(const std::string& frame, jt::Json& message);

std::string encode_file_chunk(uint32_t trace_id, uint64_t request_id, const char* data,
                              size_t size);
// data points into frame, which must outlive it.
bool        decode_file_chunk(const std::string& frame, uint32_t& trace_id,
                              uint64_t& request_id, const char*& data, size_t& size);
std::string encode_upload_chunk(uint64_t upload_id, const char* data, size_t size);
bool        decode_upload_chunk(const std::string& frame, uint64_t& upload_id,
                                const char*& data, size_t& size);

// The frame as pieces of at most max_size bytes each, or the frame alone when
// it fits.
std::vector<std::string> split_frame(const std::string& frame, size_t max_size);

/**
 * @brief Joins the pieces of split frames back together, on one connection.
 */
class FrameAssembler
{
public:
    // True when frame now holds a whole frame to handle: one that was never
    // split, or the joined pieces once the last arrives.
    bool Add(std::string& frame);
    // Drops a partly received frame, as when its connection is lost.
    void Reset();

private:
    std::string m_pending;
};

}  // namespace View
}  // namespace RocProfVis
