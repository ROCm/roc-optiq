// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_protocol_frame.h"
#include "rocprofvis_protocol.h"

#include "json.h"
#include "spdlog/spdlog.h"
#include "zlib.h"

#include <algorithm>

namespace RocProfVis
{
namespace View
{

namespace
{

constexpr size_t COMPRESSED_HEADER_SIZE    = 1 + sizeof(uint64_t);
constexpr size_t FILE_CHUNK_HEADER_SIZE    = 1 + sizeof(uint32_t) + sizeof(uint64_t);
constexpr size_t UPLOAD_CHUNK_HEADER_SIZE  = 1 + sizeof(uint64_t);
// A compressed frame claiming more than a WebSocket message may carry is not
// inflated, so a few bytes cannot ask for gigabytes.
constexpr uint64_t MAX_MESSAGE_SIZE = 1ull << 30;

void
write_little_endian(std::string& buffer, uint64_t value, size_t bytes)
{
    for(size_t i = 0; i < bytes; i++)
    {
        buffer.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
    }
}

uint64_t
read_little_endian(const std::string& buffer, size_t offset, size_t bytes)
{
    uint64_t value = 0;
    for(size_t i = 0; i < bytes; i++)
    {
        value |= static_cast<uint64_t>(static_cast<unsigned char>(buffer[offset + i]))
                 << (8 * i);
    }
    return value;
}

}  // namespace

std::string
encode_frame(const jt::Json& message)
{
    std::string payload = encode_message(message);
    if(payload.size() < COMPRESSION_THRESHOLD)
    {
        return payload;
    }

    uLongf      compressed_size = compressBound(static_cast<uLong>(payload.size()));
    std::string frame;
    frame.reserve(COMPRESSED_HEADER_SIZE + compressed_size);
    frame.push_back(FRAME_TAG_COMPRESSED);
    write_little_endian(frame, payload.size(), sizeof(uint64_t));
    frame.resize(COMPRESSED_HEADER_SIZE + compressed_size);
    int result = compress2(reinterpret_cast<Bytef*>(&frame[COMPRESSED_HEADER_SIZE]),
                           &compressed_size, reinterpret_cast<const Bytef*>(payload.data()),
                           static_cast<uLong>(payload.size()), Z_BEST_SPEED);
    if(result != Z_OK)
    {
        spdlog::warn("Could not compress a {} byte message, sending it plain", payload.size());
        return payload;
    }
    frame.resize(COMPRESSED_HEADER_SIZE + compressed_size);
    return frame;
}

bool
decode_frame(const std::string& frame, jt::Json& message)
{
    bool result = false;
    if(!frame.empty() && frame[0] == FRAME_TAG_JSON)
    {
        result = decode_message(frame, message);
    }
    else if(frame.size() > COMPRESSED_HEADER_SIZE && frame[0] == FRAME_TAG_COMPRESSED)
    {
        const uint64_t raw_size = read_little_endian(frame, 1, sizeof(uint64_t));
        if(raw_size == 0 || raw_size > MAX_MESSAGE_SIZE)
        {
            spdlog::warn("Discarding a compressed message that claims {} bytes", raw_size);
            return false;
        }
        std::string payload(static_cast<size_t>(raw_size), '\0');
        uLongf      payload_size = static_cast<uLongf>(raw_size);
        int         status       = uncompress(
            reinterpret_cast<Bytef*>(payload.data()), &payload_size,
            reinterpret_cast<const Bytef*>(frame.data() + COMPRESSED_HEADER_SIZE),
            static_cast<uLong>(frame.size() - COMPRESSED_HEADER_SIZE));
        if(status == Z_OK && payload_size == raw_size)
        {
            result = decode_message(payload, message);
        }
        else
        {
            spdlog::warn("Discarding a compressed message that does not inflate ({})",
                         status);
        }
    }
    return result;
}

std::string
encode_file_chunk(uint32_t trace_id, uint64_t request_id, const char* data, size_t size)
{
    std::string frame;
    frame.reserve(FILE_CHUNK_HEADER_SIZE + size);
    frame.push_back(FRAME_TAG_FILE_CHUNK);
    write_little_endian(frame, trace_id, sizeof(uint32_t));
    write_little_endian(frame, request_id, sizeof(uint64_t));
    frame.append(data, size);
    return frame;
}

bool
decode_file_chunk(const std::string& frame, uint32_t& trace_id, uint64_t& request_id,
                  const char*& data, size_t& size)
{
    bool result = frame.size() >= FILE_CHUNK_HEADER_SIZE && frame[0] == FRAME_TAG_FILE_CHUNK;
    if(result)
    {
        trace_id   = static_cast<uint32_t>(read_little_endian(frame, 1, sizeof(uint32_t)));
        request_id = read_little_endian(frame, 1 + sizeof(uint32_t), sizeof(uint64_t));
        data       = frame.data() + FILE_CHUNK_HEADER_SIZE;
        size       = frame.size() - FILE_CHUNK_HEADER_SIZE;
    }
    return result;
}

std::string
encode_upload_chunk(uint64_t upload_id, const char* data, size_t size)
{
    std::string frame;
    frame.reserve(UPLOAD_CHUNK_HEADER_SIZE + size);
    frame.push_back(FRAME_TAG_UPLOAD_CHUNK);
    write_little_endian(frame, upload_id, sizeof(uint64_t));
    frame.append(data, size);
    return frame;
}

bool
decode_upload_chunk(const std::string& frame, uint64_t& upload_id, const char*& data,
                    size_t& size)
{
    bool result =
        frame.size() >= UPLOAD_CHUNK_HEADER_SIZE && frame[0] == FRAME_TAG_UPLOAD_CHUNK;
    if(result)
    {
        upload_id = read_little_endian(frame, 1, sizeof(uint64_t));
        data      = frame.data() + UPLOAD_CHUNK_HEADER_SIZE;
        size      = frame.size() - UPLOAD_CHUNK_HEADER_SIZE;
    }
    return result;
}

std::vector<std::string>
split_frame(const std::string& frame, size_t max_size)
{
    std::vector<std::string> pieces;
    if(frame.size() <= max_size || max_size < 2)
    {
        pieces.push_back(frame);
    }
    else
    {
        const size_t slice = max_size - 1;
        for(size_t offset = 0; offset < frame.size(); offset += slice)
        {
            const size_t size = std::min(slice, frame.size() - offset);
            std::string  piece;
            piece.reserve(size + 1);
            piece.push_back(offset + size < frame.size() ? FRAME_TAG_PIECE
                                                         : FRAME_TAG_LAST_PIECE);
            piece.append(frame, offset, size);
            pieces.push_back(std::move(piece));
        }
    }
    return pieces;
}

bool
FrameAssembler::Add(std::string& frame)
{
    bool complete = true;
    if(!frame.empty() && frame[0] == FRAME_TAG_PIECE)
    {
        if(m_pending.size() + frame.size() > MAX_MESSAGE_SIZE)
        {
            spdlog::warn("Discarding a split message larger than {} bytes", MAX_MESSAGE_SIZE);
            m_pending.clear();
        }
        else
        {
            m_pending.append(frame, 1, std::string::npos);
        }
        complete = false;
    }
    else if(!frame.empty() && frame[0] == FRAME_TAG_LAST_PIECE)
    {
        m_pending.append(frame, 1, std::string::npos);
        frame.swap(m_pending);
        m_pending.clear();
    }
    return complete;
}

void
FrameAssembler::Reset()
{
    m_pending.clear();
}

}  // namespace View
}  // namespace RocProfVis
