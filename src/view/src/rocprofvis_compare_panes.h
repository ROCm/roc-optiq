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
class VSplitContainer;
struct CompareSourceInfo;
struct TrackInfo;

// Compare sources are ordered as the project opened them, which is also what
// TrackInfo::file_id counts.
constexpr size_t COMPARE_SOURCE_A     = 0;
constexpr size_t COMPARE_SOURCE_B     = 1;
constexpr size_t COMPARE_SOURCE_COUNT = 2;

// Matches CompareSourceInfo::id, for labels built before the sources are known.
constexpr const char* COMPARE_SOURCE_LABEL[COMPARE_SOURCE_COUNT] = { "A", "B" };
// What each source is to the comparison.
constexpr const char* COMPARE_SOURCE_ROLE[COMPARE_SOURCE_COUNT] = { "Baseline",
                                                                    "Target" };

// Both sources start with an equal share of the width.
constexpr float COMPARE_EVEN_SPLIT = 1.0f / COMPARE_SOURCE_COUNT;

/* Whether the trace was opened as an A/B compare project. The side by side
 * layouts need both sources, a plain trace has none.
 */
bool
IsCompareTrace(const TraceDataModel& model);

/* Theme color that marks a source: the baseline/target pair of the compute
 * comparison, so both kinds of compare read the same way.
 */
ImU32
CompareSourceColor(size_t source_index, SettingsManager& settings);

/* Small chip with the source letter, styled like the track pills and tinted
 * with the source's color; hovering it names the file. It takes one text line
 * and sits on the line like text, so on a line of framed items call
 * ImGui::AlignTextToFramePadding() before it. A source with no id (a track of a
 * single trace) draws nothing.
 */
void
RenderCompareSourceBadge(const CompareSourceInfo& source, SettingsManager& settings);
float
CompareSourceBadgeWidth(const CompareSourceInfo& source);
// Track forms for the timeline and the sidebar; no-ops outside compare.
void
RenderCompareSourceBadge(const TrackInfo* track_info, SettingsManager& settings);
float
CompareSourceBadgeWidth(const TrackInfo* track_info);

/* Change from a to b relative to a, e.g. "+12.3%" or "-58.1%". Empty when a is
 * zero, since nothing grows by a percentage from nothing.
 */
std::string
FormatRelativeChange(double a, double b);

/* The change from a to b on one line: an arrow, the relative change and, when
 * given, the signed difference. Tinted with the compute comparison's
 * lesser/greater colors; "No change" or "New" when that is what happened.
 */
void
RenderCompareChange(double a, double b, SettingsManager& settings,
                    const std::string& difference = "");

/* Side by side layout for the two sources. Each pane draws its own card, so the
 * split items are borderless and only inset the cards.
 */
std::shared_ptr<HSplitContainer>
MakeCompareSplit(std::shared_ptr<RocWidget> pane_a, std::shared_ptr<RocWidget> pane_b);
/* The same with A above B, for content too wide to share the width.
 */
std::shared_ptr<VSplitContainer>
MakeCompareStack(std::shared_ptr<RocWidget> pane_a, std::shared_ptr<RocWidget> pane_b);

// "1 event", "3 events".
std::string
CountLabel(uint64_t count, const char* singular, const char* plural);

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
