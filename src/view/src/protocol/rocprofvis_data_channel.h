// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <string>

namespace jt
{
class Json;
}

namespace RocProfVis
{
namespace View
{

/**
 * @brief One open trace on a roc-optiq-server, as seen by a DataProvider.
 *
 * Messages are protocol objects (see rocprofvis_protocol.h) scoped to this trace;
 * the implementation adds and strips the trace id. Send and Receive are called
 * from the UI thread only, while messages arrive on the connection's reader
 * thread, so implementations queue them.
 */
class IDataChannel
{
public:
    virtual ~IDataChannel() = default;

    // False once the connection is gone; the message is dropped.
    virtual bool Send(jt::Json& message) = 0;

    // Takes the next message the server sent for this trace. False when none is
    // waiting.
    virtual bool Receive(jt::Json& message) = 0;

    // The file the server produces for request_id (an exported table, a trimmed
    // trace) is written to local_path as it arrives, before that request's
    // response is delivered. The response then carries "file_ok".
    virtual void ReceiveFile(uint64_t request_id, const std::string& local_path) = 0;

    virtual bool IsConnected() const = 0;

    // Releases the trace on the server. No messages are delivered afterwards.
    virtual void Close() = 0;
};

}  // namespace View
}  // namespace RocProfVis
