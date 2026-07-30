// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_middleware.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <new>
#include <string>

#include "rocprofvis_mw_session.h"

using RocProfVis::Middleware::PROTOCOL_VERSION;
using RocProfVis::Middleware::Session;

/*
 * Copy a std::string onto the C heap for handoff across the ABI. Returns
 * nullptr on allocation failure rather than throwing, since this boundary is
 * declared noexcept in practice and may be crossed by a C or FFI caller.
 */
static char*
AllocCString(const std::string& text)
{
    char* buffer = static_cast<char*>(malloc(text.size() + 1));
    if(buffer != nullptr)
    {
        memcpy(buffer, text.c_str(), text.size());
        buffer[text.size()] = '\0';
    }
    return buffer;
}

extern "C"
{

rocprofvis_mw_session_t*
rocprofvis_mw_session_alloc(void)
{
    return reinterpret_cast<rocprofvis_mw_session_t*>(new(std::nothrow) Session());
}

void
rocprofvis_mw_session_free(rocprofvis_mw_session_t* session)
{
    delete reinterpret_cast<Session*>(session);
}

char*
rocprofvis_mw_request(rocprofvis_mw_session_t* session, char const* request_json)
{
    char* response = nullptr;
    if(session != nullptr && request_json != nullptr)
    {
        response =
            AllocCString(reinterpret_cast<Session*>(session)->Execute(request_json));
    }
    return response;
}

void
rocprofvis_mw_string_free(char* text)
{
    free(text);
}

uint32_t
rocprofvis_mw_protocol_version(void)
{
    return PROTOCOL_VERSION;
}

}  // extern "C"
