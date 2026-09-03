// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <string>

namespace RocProfVis
{
namespace View
{

/**
 * @brief The standing instructions the model is given before every round.
 *
 * This is product behaviour rather than incidental text: it is what decides
 * how the assistant reads a trace, what it goes looking for, how it talks to
 * the user, and what it refuses to guess at. It lives in its own file so it
 * can be read and revised as prose, without scrolling past the turn machinery
 * or the ImGui layout that happen to share the panel.
 *
 * Rebuilt per call rather than cached, because it is assembled once per HTTP
 * round on a worker thread and the copy is dwarfed by the request it goes into.
 */
std::string AssistantSystemPrompt();

}  // namespace View
}  // namespace RocProfVis
