// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_mw_net.h"

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <string>
#include <vector>

namespace RocProfVis
{
namespace Middleware
{
namespace Net
{

/*
 * What the platform's socket calls actually take. socket_t is the widened form
 * the header exposes so callers need no platform headers of their own.
 */
#if defined(_WIN32)
typedef SOCKET native_socket_t;
#else
typedef int native_socket_t;
#endif

static native_socket_t
ToNative(socket_t connection)
{
    return static_cast<native_socket_t>(connection);
}

/* Backlog for listen(); the adapter serves a handful of clients at most. */
static constexpr int LISTEN_BACKLOG = 16;

static constexpr uint32_t MS_PER_SECOND      = 1000;
static constexpr uint32_t MICROSECONDS_PER_MS = 1000;

socket_t
InvalidSocket(void)
{
#if defined(_WIN32)
    return static_cast<socket_t>(INVALID_SOCKET);
#else
    return -1;
#endif
}

/* Most recent socket error, in whichever numbering the platform uses. */
static int
LastError(void)
{
#if defined(_WIN32)
    return WSAGetLastError();
#else
    return errno;
#endif
}

static bool
IsWouldBlock(int error)
{
#if defined(_WIN32)
    return error == WSAEWOULDBLOCK;
#else
    return error == EWOULDBLOCK || error == EAGAIN || error == EINTR;
#endif
}

static std::string
DescribeError(const std::string& what, int error)
{
    return what + " failed (error " + std::to_string(error) + ")";
}

static bool
SetNonBlocking(native_socket_t connection)
{
#if defined(_WIN32)
    u_long non_blocking = 1;
    return ioctlsocket(connection, FIONBIO, &non_blocking) == 0;
#else
    int flags = fcntl(connection, F_GETFL, 0);
    return (flags >= 0) && (fcntl(connection, F_SETFL, flags | O_NONBLOCK) == 0);
#endif
}

/* Close a native handle, for the failure paths before one becomes a socket_t. */
static void
CloseNative(native_socket_t connection)
{
#if defined(_WIN32)
    closesocket(connection);
#else
    close(connection);
#endif
}

static bool
IsValidNative(native_socket_t connection)
{
#if defined(_WIN32)
    return connection != INVALID_SOCKET;
#else
    return connection >= 0;
#endif
}

bool
Startup(std::string& error_message)
{
#if defined(_WIN32)
    WSADATA data;
    int     started = WSAStartup(MAKEWORD(2, 2), &data);
    if(started != 0)
    {
        error_message = DescribeError("WSAStartup", started);
        return false;
    }
#else
    (void) error_message;
#endif
    return true;
}

void
Shutdown(void)
{
#if defined(_WIN32)
    WSACleanup();
#endif
}

socket_t
Listen(const std::string& host, uint16_t port, std::string& error_message)
{
    socket_t listener = InvalidSocket();

    native_socket_t raw = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(!IsValidNative(raw))
    {
        error_message = DescribeError("socket", LastError());
        return listener;
    }

    /*
     * Without this a listener lingers in TIME_WAIT after a restart and the
     * next bind is refused, which makes the server look broken when it is
     * merely impatient.
     */
    int reuse = 1;
    setsockopt(raw, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse),
               sizeof(reuse));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port   = htons(port);
    if(inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1)
    {
        error_message = "'" + host + "' is not an IPv4 address";
        CloseNative(raw);
        return listener;
    }

    if(bind(raw, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) != 0)
    {
        error_message = DescribeError("bind to " + host + ":" + std::to_string(port),
                                      LastError());
        CloseNative(raw);
        return listener;
    }

    if(listen(raw, LISTEN_BACKLOG) != 0)
    {
        error_message = DescribeError("listen", LastError());
        CloseNative(raw);
        return listener;
    }

    if(!SetNonBlocking(raw))
    {
        error_message = DescribeError("set non-blocking", LastError());
        CloseNative(raw);
        return listener;
    }

    listener = static_cast<socket_t>(raw);
    return listener;
}

socket_t
Accept(socket_t listener)
{
    socket_t accepted = InvalidSocket();

    native_socket_t raw = accept(ToNative(listener), nullptr, nullptr);
    if(!IsValidNative(raw))
    {
        return accepted;
    }

    if(!SetNonBlocking(raw))
    {
        CloseNative(raw);
        return accepted;
    }

    /*
     * Responses are written as a single buffered blob, so Nagle only adds
     * latency waiting for a follow-up that is not coming.
     */
    int no_delay = 1;
    setsockopt(raw, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&no_delay),
               sizeof(no_delay));

    accepted = static_cast<socket_t>(raw);
    return accepted;
}

TransferStatus
Receive(socket_t connection, char* buffer, size_t capacity, size_t& received)
{
    received = 0;

#if defined(_WIN32)
    int transferred = recv(ToNative(connection), buffer, static_cast<int>(capacity), 0);
#else
    ssize_t transferred = recv(ToNative(connection), buffer, capacity, 0);
#endif

    if(transferred > 0)
    {
        received = static_cast<size_t>(transferred);
        return TransferStatus::kOk;
    }
    if(transferred == 0)
    {
        return TransferStatus::kClosed;
    }
    return IsWouldBlock(LastError()) ? TransferStatus::kWouldBlock : TransferStatus::kError;
}

TransferStatus
Send(socket_t connection, const char* buffer, size_t length, size_t& sent)
{
    sent = 0;

#if defined(_WIN32)
    int transferred = send(ToNative(connection), buffer, static_cast<int>(length), 0);
#else
    /*
     * A write to a peer that has already gone away raises SIGPIPE by default,
     * which would kill the process rather than fail the call.
     */
    ssize_t transferred = send(ToNative(connection), buffer, length, MSG_NOSIGNAL);
#endif

    if(transferred >= 0)
    {
        sent = static_cast<size_t>(transferred);
        return TransferStatus::kOk;
    }
    return IsWouldBlock(LastError()) ? TransferStatus::kWouldBlock : TransferStatus::kError;
}

void
Close(socket_t connection)
{
    if(connection != InvalidSocket())
    {
        CloseNative(ToNative(connection));
    }
}

bool
WaitForReadiness(std::vector<socket_t>& readable, std::vector<socket_t>& writable,
                 uint32_t timeout_ms)
{
    fd_set read_set;
    fd_set write_set;
    FD_ZERO(&read_set);
    FD_ZERO(&write_set);

    socket_t highest = 0;
    for(size_t i = 0; i < readable.size(); i++)
    {
        FD_SET(ToNative(readable[i]), &read_set);
        highest = (readable[i] > highest) ? readable[i] : highest;
    }
    for(size_t i = 0; i < writable.size(); i++)
    {
        FD_SET(ToNative(writable[i]), &write_set);
        highest = (writable[i] > highest) ? writable[i] : highest;
    }

    struct timeval timeout;
    timeout.tv_sec = static_cast<long>(timeout_ms / MS_PER_SECOND);
    timeout.tv_usec =
        static_cast<long>((timeout_ms % MS_PER_SECOND) * MICROSECONDS_PER_MS);

    int ready = select(static_cast<int>(highest) + 1, &read_set, &write_set, nullptr,
                       &timeout);
    if(ready < 0)
    {
        readable.clear();
        writable.clear();
        return IsWouldBlock(LastError());
    }

    std::vector<socket_t> ready_to_read;
    for(size_t i = 0; i < readable.size(); i++)
    {
        if(FD_ISSET(ToNative(readable[i]), &read_set))
        {
            ready_to_read.push_back(readable[i]);
        }
    }

    std::vector<socket_t> ready_to_write;
    for(size_t i = 0; i < writable.size(); i++)
    {
        if(FD_ISSET(ToNative(writable[i]), &write_set))
        {
            ready_to_write.push_back(writable[i]);
        }
    }

    readable.swap(ready_to_read);
    writable.swap(ready_to_write);
    return true;
}

}  // namespace Net
}  // namespace Middleware
}  // namespace RocProfVis
