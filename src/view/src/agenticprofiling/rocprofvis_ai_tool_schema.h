// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "json.h"

#include <string>

namespace RocProfVis
{
namespace View
{

/**
 * @brief The description of the tool set that goes to the model, kept apart
 * from the code that runs it.
 *
 * Nothing here touches view state or the data model: it builds JSON out of
 * string literals, which is what makes it safe to call from the HTTP worker
 * thread while the UI thread carries on drawing.
 *
 * These descriptions are the only instructions the model gets about what each
 * tool is for, so they are part of the product's behaviour rather than
 * incidental text. A tool body registered in one of the *_tools.cpp handler
 * tables without a matching entry here is unreachable, and one added here
 * without a body comes back to the model as an unknown tool.
 */

// The tool schema sent with every request. Thread-safe: reads no view state.
jt::Json BuildAssistantToolsJson();

// The line the panel shows under the transcript while a named tool runs.
std::string AssistantToolStatusLabel(const std::string& tool_name);

// Every registered tool name, for telling the model when it invents one.
std::string AssistantToolNameList();

}  // namespace View
}  // namespace RocProfVis
