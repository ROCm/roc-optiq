// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "imgui.h"
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

class HSplitContainer;
class SettingsManager;
class TraceDataModel;
struct CompareSourceInfo;

// Compare sources are ordered as the project opened them, which is also what
// TrackInfo::file_id counts.
constexpr size_t COMPARE_SOURCE_A     = 0;
constexpr size_t COMPARE_SOURCE_B     = 1;
constexpr size_t COMPARE_SOURCE_COUNT = 2;

// Matches CompareSourceInfo::id, for labels built before the sources are known.
constexpr const char* COMPARE_SOURCE_LABEL[COMPARE_SOURCE_COUNT] = { "A", "B" };

// Both sources start with an equal share of the width.
constexpr float COMPARE_EVEN_SPLIT = 1.0f / COMPARE_SOURCE_COUNT;

/* Client ids that keep the two sources' requests for one controller table type
 * apart. They start at one because zero is what the pooled, single source
 * request ids use.
 */
constexpr uint64_t COMPARE_CLIENT_ID[COMPARE_SOURCE_COUNT] = { 1, 2 };

/* Whether the trace was opened as an A/B compare project. The side by side
 * layouts need both sources, a plain trace has none.
 */
bool
IsCompareTrace(const TraceDataModel& model);

/* Side by side layout for the two sources. Each pane draws its own card, so the
 * split items are borderless and only inset the cards.
 */
std::shared_ptr<HSplitContainer>
MakeCompareSplit(std::shared_ptr<RocWidget> pane_a, std::shared_ptr<RocWidget> pane_b);

/* Bordered, padded surface that holds one source's content, styled like the
 * panels it sits in. Not BeginPanelCard: that one is the dialog look, with a
 * fixed rounding, an automatic height and no scrolling. Pair with
 * EndCompareCard.
 */
void
BeginCompareCard(const char* id, SettingsManager& settings,
                 const ImVec2&    size         = ImVec2(0.0f, 0.0f),
                 ImGuiWindowFlags window_flags = ImGuiWindowFlags_None);
void
EndCompareCard();

/* Title row of a compare card: the source badge, the elided source name, an
 * optional right aligned summary, then the separator above the card contents.
 */
void
RenderCompareCardTitle(const CompareSourceInfo& source, SettingsManager& settings,
                       const std::string& summary = "");

/* Union of eligible group-by columns from the two sources, A then B-only.
 * Shared names keep the raw column text; names that exist on only one source
 * are tagged (A) or (B) in labels_out. names_out is what the query uses.
 */
void
BuildCompareGroupByChoices(const std::vector<std::string>& columns_a,
                           const std::vector<std::string>& columns_b,
                           std::vector<std::string>&       names_out,
                           std::vector<std::string>&       labels_out);

}  // namespace View
}  // namespace RocProfVis
