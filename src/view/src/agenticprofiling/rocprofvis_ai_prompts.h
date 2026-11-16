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
 * Assembled from a shared base plus the body for whichever kind of trace is in
 * front. The base is everything that is about how to talk to a person rather
 * than about what a trace contains, so voice, honesty and the shape of an
 * answer stay identical across both and are edited in one place.
 *
 * Rebuilt per call rather than cached, because it is assembled once per HTTP
 * round on a worker thread and the copy is dwarfed by the request it goes into.
 */
std::string AssistantSystemPrompt(bool is_compute);

/**
 * @brief The whole system message of the follow-up request, which writes the
 * buttons under an answer once it is on screen.
 *
 * That request carries nothing but the question and the answer, so this is all
 * it knows about what the analyst can do. It is split by trace kind like the
 * main prompt: a follow-up the analyst cannot act on only earns an apology when
 * it is clicked.
 */
std::string AssistantNextStepsPrompt(bool is_compute);

}  // namespace View
}  // namespace RocProfVis
