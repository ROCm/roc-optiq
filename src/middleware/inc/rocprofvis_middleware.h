// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * ROCm Optiq middleware: a JSON request/response facade over the controller.
 *
 * The middleware owns a controller instance and translates coarse,
 * intent-level JSON messages into the controller's C ABI, so a frontend can
 * drive the existing controller and model layers without linking C++ or
 * speaking the property-bag ABI.
 *
 * The surface is deliberately transport agnostic: a session consumes a
 * request string and produces a response string. Adapters (stdio, HTTP,
 * WebSocket, in-process FFI) sit on top of this and add no protocol of their
 * own. See src/middleware/README.md for the message schema.
 */

/*
 * Opaque middleware session. One session owns at most one open trace.
 */
typedef struct rocprofvis_mw_session_t rocprofvis_mw_session_t;

/*
 * Allocate a session. The session starts with no trace open; issue a
 * "trace.open" request to load one.
 * @returns A valid session, or nullptr on allocation failure.
 */
rocprofvis_mw_session_t* rocprofvis_mw_session_alloc(void);

/*
 * Cancel any outstanding requests, close the trace, and free the session.
 * @param session The session to free. Passing nullptr is a no-op.
 */
void rocprofvis_mw_session_free(rocprofvis_mw_session_t* session);

/*
 * Execute a single JSON request and return the JSON response.
 *
 * This call never throws and never returns nullptr for a valid session: a
 * malformed or unknown request produces a well-formed JSON error response.
 * Requests that map onto controller futures return a request handle
 * immediately unless the caller supplies a "wait_ms" budget.
 *
 * @param session The session to execute against.
 * @param request_json A NUL-terminated JSON object.
 * @returns A heap-allocated NUL-terminated JSON object. The caller owns it and
 *          must release it with rocprofvis_mw_string_free. Returns nullptr only
 *          if session or request_json is nullptr, or on allocation failure.
 */
char* rocprofvis_mw_request(rocprofvis_mw_session_t* session, char const* request_json);

/*
 * Release a string returned by rocprofvis_mw_request.
 * @param text The string to free. Passing nullptr is a no-op.
 */
void rocprofvis_mw_string_free(char* text);

/*
 * Protocol version implemented by this build. Bumped when the message schema
 * changes in a way that is not backwards compatible. Also reported by the
 * "session.info" request.
 */
uint32_t rocprofvis_mw_protocol_version(void);

#ifdef __cplusplus
}
#endif
