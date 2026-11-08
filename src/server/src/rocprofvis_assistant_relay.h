// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "json.h"

#include <cstdint>

namespace RocProfVis
{
namespace Server
{

/*
 * Ask Optiq for the web client. The browser runs the conversation and the
 * tools, against the same API as the rest of the client. The server holds what
 * a browser page cannot: the desktop app's endpoint settings and stored key,
 * the assistant's instructions and tool schema, and an HTTPS client that is not
 * bound by CORS. Neither call touches the data provider, so both run on the
 * HTTP worker thread that received them.
 */

// The endpoint and model from the desktop settings, whether a key is stored for
// it, and the tools the model will be offered. Never includes the key.
jt::Json
AssistantStatus();

// Saves body.endpoint_url and body.model to the desktop settings file and
// body.token to the OS credential store, where the desktop app's Settings >
// Assistant page keeps them; body.clear_token forgets the key. Fields left out
// are unchanged. Returns the new status.
jt::Json
AssistantSave(jt::Json body, int32_t& status_out);

// Sends one round of a conversation to the model. body.messages are the user,
// assistant and tool messages in the chat API's shape; the system prompt and
// tool schema are added here. body.final asks for the written answer with
// tools off.
jt::Json
AssistantChat(jt::Json body, int32_t& status_out);

}  // namespace Server
}  // namespace RocProfVis
