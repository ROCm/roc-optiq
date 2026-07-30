// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace RocProfVis
{
namespace Middleware
{
namespace Net
{

/*
 * Minimal non-blocking TCP for the HTTP/WebSocket adapter.
 *
 * Just enough of Winsock and BSD sockets to run a single-threaded accept loop,
 * with the platform difference confined to this file. Nothing here throws or
 * logs; every call reports its outcome to the caller.
 */

#if defined(_WIN32)
typedef uintptr_t socket_t;
#else
typedef int socket_t;
#endif

/* Returned by Listen and Accept when no socket could be produced. */
socket_t InvalidSocket(void);

/* Outcome of a transfer. Byte counts are reported separately. */
enum class TransferStatus
{
    kOk,
    /* Nothing to do right now; the caller should wait for readiness. */
    kWouldBlock,
    /* The peer closed its side in an orderly way. */
    kClosed,
    kError
};

/*
 * Initialise and tear down platform networking. Startup must succeed before
 * any other call here, and is a no-op outside Windows.
 */
bool Startup(std::string& error_message);
void Shutdown(void);

/*
 * Open a listening socket bound to host:port. The host is an address rather
 * than a name; "127.0.0.1" restricts access to this machine and "0.0.0.0"
 * accepts from anywhere. Returns InvalidSocket() and fills error_message on
 * failure.
 */
socket_t Listen(const std::string& host, uint16_t port, std::string& error_message);

/*
 * Take the next pending connection, already set non-blocking. Returns
 * InvalidSocket() when none is waiting, which is not an error.
 */
socket_t Accept(socket_t listener);

/* Read into buffer. Sets received to the byte count when the status is kOk. */
TransferStatus Receive(socket_t connection, char* buffer, size_t capacity,
                       size_t& received);

/*
 * Write as much of buffer as the socket will take. Sets sent to the byte count
 * when the status is kOk; a short write is normal and not an error.
 */
TransferStatus Send(socket_t connection, const char* buffer, size_t length, size_t& sent);

void Close(socket_t connection);

/*
 * Wait until one of the sockets is ready, or until timeout_ms elapses. The
 * caller passes the sockets to watch for readability and writability; on
 * return the vectors hold only those that are ready. Returns false on error.
 */
bool WaitForReadiness(std::vector<socket_t>& readable, std::vector<socket_t>& writable,
                      uint32_t timeout_ms);

}  // namespace Net
}  // namespace Middleware
}  // namespace RocProfVis
