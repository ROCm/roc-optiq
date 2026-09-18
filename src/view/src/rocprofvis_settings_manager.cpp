// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_settings_manager.h"
#include "rocprofvis_hotkey_manager.h"
#include "imgui.h"
#include "implot.h"
#ifdef ROCPROFVIS_ENABLE_AGENTIC_PROFILING
#    include "remote/rocprofvis_secret_store.h"
#endif
#include "rocprofvis_core.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_json_utils.h"
#include "rocprofvis_settings_panel.h"
#include "rocprofvis_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <set>

namespace RocProfVis
{
namespace View
{

// Theme color tables must follow the Colors enum order.
// One accent, cool neutrals. Azure drives every interactive state - chrome,
// selection, focus, links - so nothing competes for "this is active", and the
// warm end of the spectrum is reserved entirely for meaning: amber for warning,
// red for error, and the minimap's ember density ramp.
//
// The neutral ramp carries a slight blue cast rather than being pure grey. Pure
// grey next to a saturated azure reads as dirty; matching the cast lets the
// accent sit on the surface instead of floating above it.
constexpr std::array DARK_THEME_COLORS = {
    IM_COL32(27, 29, 35, 255),     // Colors::kMetaDataColor
    IM_COL32(42, 45, 54, 255),     // Colors::kMetaDataColorSelected
    IM_COL32(50, 53, 63, 255),     // Colors::kMetaDataSeparator
    IM_COL32(0, 0, 0, 0),          // Colors::kTransparent
    IM_COL32(255, 100, 110, 255),  // Colors::kTextError
    IM_COL32(80, 216, 152, 255),   // Colors::kTextSuccess
    IM_COL32(255, 198, 84, 255),   // Colors::kTextWarning
    IM_COL32(44, 126, 196, 250),   // Colors::kFlameChartColor
    IM_COL32(150, 154, 172, 28),   // Colors::kGridColor
    IM_COL32(84, 34, 40, 255),     // Colors::kGridRed
    IM_COL32(72, 154, 252, 255),   // Colors::kSelectionBorder
    IM_COL32(72, 154, 252, 48),    // Colors::kSelection
    IM_COL32(140, 144, 160, 255),  // Colors::kBoundBox
    IM_COL32(20, 21, 26, 255),     // Colors::kFillerColor
    IM_COL32(142, 146, 162, 130),  // Colors::kScrollBarColor
    IM_COL32(72, 154, 252, 32),    // Colors::kHighlightChart
    IM_COL32(20, 21, 26, 255),     // Colors::kRulerBgColor
    IM_COL32(226, 228, 236, 255),  // Colors::kRulerTextColor
    IM_COL32(148, 152, 166, 255),  // Colors::kScrubberNumberColor
    IM_COL32(72, 154, 252, 212),   // Colors::kArrowColor
    IM_COL32(52, 55, 65, 255),     // Colors::kBorderColor
    IM_COL32(41, 43, 51, 255),     // Colors::kSplitterColor
    IM_COL32(20, 21, 26, 255),     // Colors::kBgMain
    IM_COL32(27, 29, 35, 255),     // Colors::kBgPanel
    IM_COL32(35, 37, 44, 255),     // Colors::kBgFrame
    IM_COL32(44, 47, 56, 255),     // Colors::kComboFill
    IM_COL32(56, 142, 250, 255),   // Colors::kAccent
    IM_COL32(92, 168, 255, 255),   // Colors::kAccentHover
    IM_COL32(36, 116, 216, 255),   // Colors::kAccentActive
    IM_COL32(35, 37, 44, 255),     // Colors::kTabAccent
    IM_COL32(45, 48, 57, 255),     // Colors::kTabAccentHover
    IM_COL32(30, 32, 39, 255),     // Colors::kTabAccentActive
    IM_COL32(50, 53, 63, 255),     // Colors::kBorderGray
    IM_COL32(240, 241, 246, 255),  // Colors::kTextMain
    IM_COL32(148, 152, 166, 255),  // Colors::kTextDim
    IM_COL32(255, 255, 255, 10),   // Colors::kScrollBg
    IM_COL32(142, 146, 162, 120),  // Colors::kScrollGrab
    IM_COL32(25, 27, 33, 255),     // Colors::kTableHeaderBg
    IM_COL32(50, 53, 63, 255),     // Colors::kTableBorderStrong
    IM_COL32(38, 40, 48, 255),     // Colors::kTableBorderLight
    IM_COL32(23, 25, 30, 255),     // Colors::kTableRowBg
    IM_COL32(28, 30, 37, 255),     // Colors::kTableRowBgAlt
    IM_COL32(34, 36, 43, 255),     // Colors::kTableBorderInner
    IM_COL32(45, 48, 57, 255),     // Colors::kTableBorderOuter
    IM_COL32(41, 43, 51, 255),     // Colors::kPanelBorderSubtle
    IM_COL32(72, 154, 252, 235),   // Colors::kEventHighlight
    IM_COL32(104, 224, 184, 235),  // Colors::kEventSearchHighlight
    IM_COL32(72, 154, 252, 62),    // Colors::kAreaOfInterest
    IM_COL32(44, 150, 190, 175),   // Colors::kLineChartColor
    IM_COL32(39, 42, 50, 255),     // Colors::kButton
    IM_COL32(49, 52, 62, 255),     // Colors::kButtonHovered
    IM_COL32(60, 64, 75, 255),     // Colors::kButtonActive
    IM_COL32(166, 124, 28, 255),   // Colors::kBgWarning
    IM_COL32(170, 46, 54, 255),    // Colors::kBgError
    IM_COL32(32, 130, 88, 255),    // Colors::kBgSuccess
    IM_COL32(57, 60, 70, 255),     // Colors::kStickyNoteYellow
    IM_COL32(72, 222, 236, 175),   // Colors::kLineChartColorAlt
    IM_COL32(255, 72, 80, 56),     // Colors::kTrackWarningBand
    IM_COL32(48, 58, 92, 255),     // Colors::kMinimapBin1
    IM_COL32(78, 58, 122, 255),    // Colors::kMinimapBin2
    IM_COL32(120, 58, 132, 255),   // Colors::kMinimapBin3
    IM_COL32(168, 56, 104, 255),   // Colors::kMinimapBin4
    IM_COL32(214, 68, 66, 255),    // Colors::kMinimapBin5
    IM_COL32(242, 134, 50, 255),   // Colors::kMinimapBin6
    IM_COL32(252, 214, 146, 255),  // Colors::kMinimapBin7
    IM_COL32(72, 74, 84, 255),     // Colors::kMinimapBinCounter1
    IM_COL32(98, 101, 112, 255),   // Colors::kMinimapBinCounter2
    IM_COL32(124, 127, 139, 255),  // Colors::kMinimapBinCounter3
    IM_COL32(150, 153, 164, 255),  // Colors::kMinimapBinCounter4
    IM_COL32(176, 179, 190, 255),  // Colors::kMinimapBinCounter5
    IM_COL32(202, 205, 214, 255),  // Colors::kMinimapBinCounter6
    IM_COL32(228, 230, 237, 255),  // Colors::kMinimapBinCounter7
    IM_COL32(20, 21, 26, 255),     // Colors::kMinimapBg
    IM_COL32(11, 12, 16, 178),     // Colors::kLoadingScreenColor
    IM_COL32(255, 255, 255, 255),  // Colors::kTextOnAccent
    IM_COL32(196, 124, 252, 255),  // Colors::kMeasurementColor
    IM_COL32(27, 29, 35, 242),     // Colors::kMeasurementLabelBg
    IM_COL32(64, 68, 80, 200),     // Colors::kMeasurementLabelEdge
    IM_COL32(240, 241, 246, 255),  // Colors::kMeasurementLabelText
    IM_COL32(255, 255, 255, 120),  // Colors::kMeasurementNotch
    IM_COL32(44, 78, 116, 255),    // Colors::kComparisonBase
    IM_COL32(26, 112, 110, 255),   // Colors::kComparisonTarget
    IM_COL32(170, 130, 54, 255),   // Colors::kComparisonLesser
    IM_COL32(98, 68, 138, 255),    // Colors::kComparisonGreater

    // Centralized from view widgets (kept in Colors enum order):
    IM_COL32(20, 21, 26, 255),     // Colors::kMemChartBg
    IM_COL32(27, 29, 35, 245),     // Colors::kMemChartPanel
    IM_COL32(35, 37, 44, 245),     // Colors::kMemChartPanelAlt
    IM_COL32(56, 60, 71, 220),     // Colors::kMemChartBorder
    IM_COL32(72, 154, 252, 255),   // Colors::kMemChartBorderHot
    IM_COL32(240, 241, 246, 255),  // Colors::kMemChartTextMain
    IM_COL32(148, 152, 166, 255),  // Colors::kMemChartTextDim
    IM_COL32(72, 206, 202, 235),   // Colors::kMemChartRead
    IM_COL32(228, 196, 92, 235),   // Colors::kMemChartWrite
    IM_COL32(178, 142, 230, 235),  // Colors::kMemChartAtomic
    IM_COL32(112, 212, 128, 255),  // Colors::kMemChartUtil
    IM_COL32(232, 190, 76, 255),   // Colors::kMemChartHit
    IM_COL32(240, 92, 104, 255),   // Colors::kMemChartStall
    IM_COL32(0, 0, 0, 92),         // Colors::kMemChartShadow
    IM_COL32(240, 214, 92, 250),   // Colors::kStickyNoteBg
    IM_COL32(196, 158, 44, 235),   // Colors::kStickyNoteBorder
    IM_COL32(234, 204, 86, 252),   // Colors::kStickyNoteHeader
    IM_COL32(0, 0, 0, 112),        // Colors::kStickyNoteShadow
    IM_COL32(46, 38, 10, 255),     // Colors::kStickyNoteText
    IM_COL32(112, 92, 40, 255),    // Colors::kStickyNoteTextMuted
    IM_COL32(176, 130, 24, 235),   // Colors::kStickyNoteAccent
    IM_COL32(176, 130, 24, 255),   // Colors::kStickyNoteResize
    IM_COL32(150, 96, 24, 255),    // Colors::kStickyNoteResizeActive
    IM_COL32(200, 16, 32, 150),    // Colors::kBannerFill
    IM_COL32(255, 255, 255, 40),   // Colors::kBannerBorder
    IM_COL32(255, 255, 255, 255),  // Colors::kBannerText
    IM_COL32(228, 228, 228, 255),  // Colors::kDebugNavBarBg
    IM_COL32(140, 144, 158, 255),  // Colors::kLogTrace
    IM_COL32(142, 178, 216, 255),  // Colors::kLogDebug
    IM_COL32(220, 220, 228, 255),  // Colors::kLogInfo
    IM_COL32(238, 196, 92, 255),   // Colors::kLogWarning
    IM_COL32(240, 104, 112, 255),  // Colors::kLogError
    IM_COL32(255, 78, 86, 255),    // Colors::kLogCritical
    // This must follow the ordering of Colors enum.
};

// Light mirrors the dark ramp exactly: a soft off-white canvas with pure-white
// panels floating on it, hairline dividers, and the same single azure accent
// stepped deeper so it holds contrast against white.
constexpr std::array LIGHT_THEME_COLORS = {
    IM_COL32(255, 255, 255, 255),  // Colors::kMetaDataColor
    IM_COL32(233, 236, 242, 255),  // Colors::kMetaDataColorSelected
    IM_COL32(222, 225, 233, 255),  // Colors::kMetaDataSeparator
    IM_COL32(0, 0, 0, 0),          // Colors::kTransparent
    IM_COL32(198, 32, 42, 255),    // Colors::kTextError
    IM_COL32(22, 138, 88, 255),    // Colors::kTextSuccess
    IM_COL32(168, 108, 0, 255),    // Colors::kTextWarning
    IM_COL32(96, 168, 232, 240),   // Colors::kFlameChartColor
    IM_COL32(100, 104, 124, 26),   // Colors::kGridColor
    IM_COL32(252, 226, 226, 255),  // Colors::kGridRed
    IM_COL32(14, 110, 236, 255),   // Colors::kSelectionBorder
    IM_COL32(14, 110, 236, 42),    // Colors::kSelection
    IM_COL32(132, 136, 150, 255),  // Colors::kBoundBox
    IM_COL32(244, 245, 248, 255),  // Colors::kFillerColor
    IM_COL32(138, 142, 158, 120),  // Colors::kScrollBarColor
    IM_COL32(14, 110, 236, 26),    // Colors::kHighlightChart
    IM_COL32(255, 255, 255, 255),  // Colors::kRulerBgColor
    IM_COL32(26, 27, 31, 255),     // Colors::kRulerTextColor
    IM_COL32(108, 112, 122, 255),  // Colors::kScrubberNumberColor
    IM_COL32(14, 110, 236, 200),   // Colors::kArrowColor
    IM_COL32(222, 225, 233, 255),  // Colors::kBorderColor
    IM_COL32(228, 230, 236, 255),  // Colors::kSplitterColor
    IM_COL32(244, 245, 248, 255),  // Colors::kBgMain
    IM_COL32(255, 255, 255, 255),  // Colors::kBgPanel
    IM_COL32(240, 241, 245, 255),  // Colors::kBgFrame
    IM_COL32(234, 237, 244, 255),  // Colors::kComboFill
    IM_COL32(14, 110, 236, 255),   // Colors::kAccent
    IM_COL32(52, 136, 246, 255),   // Colors::kAccentHover
    IM_COL32(8, 88, 196, 255),     // Colors::kAccentActive
    IM_COL32(255, 255, 255, 255),  // Colors::kTabAccent
    IM_COL32(233, 236, 242, 255),  // Colors::kTabAccentHover
    IM_COL32(245, 246, 250, 255),  // Colors::kTabAccentActive
    IM_COL32(222, 225, 233, 255),  // Colors::kBorderGray
    IM_COL32(26, 27, 31, 255),     // Colors::kTextMain
    IM_COL32(108, 112, 122, 255),  // Colors::kTextDim
    IM_COL32(0, 0, 0, 8),          // Colors::kScrollBg
    IM_COL32(118, 122, 138, 110),  // Colors::kScrollGrab
    IM_COL32(242, 243, 247, 255),  // Colors::kTableHeaderBg
    IM_COL32(213, 216, 224, 255),  // Colors::kTableBorderStrong
    IM_COL32(232, 234, 241, 255),  // Colors::kTableBorderLight
    IM_COL32(255, 255, 255, 255),  // Colors::kTableRowBg
    IM_COL32(247, 248, 251, 255),  // Colors::kTableRowBgAlt
    IM_COL32(237, 239, 245, 255),  // Colors::kTableBorderInner
    IM_COL32(222, 225, 233, 255),  // Colors::kTableBorderOuter
    IM_COL32(228, 230, 236, 255),  // Colors::kPanelBorderSubtle
    IM_COL32(14, 110, 236, 220),   // Colors::kEventHighlight
    IM_COL32(26, 152, 116, 220),   // Colors::kEventSearchHighlight
    IM_COL32(14, 110, 236, 36),    // Colors::kAreaOfInterest
    IM_COL32(36, 146, 196, 155),   // Colors::kLineChartColor
    IM_COL32(240, 241, 245, 255),  // Colors::kButton
    IM_COL32(231, 234, 240, 255),  // Colors::kButtonHovered
    IM_COL32(220, 223, 231, 255),  // Colors::kButtonActive
    IM_COL32(250, 212, 120, 255),  // Colors::kBgWarning
    IM_COL32(250, 156, 156, 255),  // Colors::kBgError
    IM_COL32(150, 226, 180, 255),  // Colors::kBgSuccess
    IM_COL32(255, 244, 182, 255),  // Colors::kStickyNoteYellow
    IM_COL32(14, 176, 200, 155),   // Colors::kLineChartColorAlt
    IM_COL32(235, 64, 72, 52),     // Colors::kTrackWarningBand
    IM_COL32(198, 212, 234, 255),  // Colors::kMinimapBin1
    IM_COL32(162, 154, 216, 255),  // Colors::kMinimapBin2
    IM_COL32(186, 118, 188, 255),  // Colors::kMinimapBin3
    IM_COL32(216, 104, 138, 255),  // Colors::kMinimapBin4
    IM_COL32(232, 106, 86, 255),   // Colors::kMinimapBin5
    IM_COL32(244, 152, 62, 255),   // Colors::kMinimapBin6
    IM_COL32(250, 206, 128, 255),  // Colors::kMinimapBin7
    IM_COL32(227, 229, 235, 255),  // Colors::kMinimapBinCounter1
    IM_COL32(203, 206, 214, 255),  // Colors::kMinimapBinCounter2
    IM_COL32(179, 182, 192, 255),  // Colors::kMinimapBinCounter3
    IM_COL32(153, 157, 168, 255),  // Colors::kMinimapBinCounter4
    IM_COL32(127, 131, 142, 255),  // Colors::kMinimapBinCounter5
    IM_COL32(101, 105, 117, 255),  // Colors::kMinimapBinCounter6
    IM_COL32(75, 79, 90, 255),     // Colors::kMinimapBinCounter7
    IM_COL32(250, 251, 253, 255),  // Colors::kMinimapBg
    IM_COL32(0, 0, 0, 56),         // Colors::kLoadingScreenColor
    IM_COL32(255, 255, 255, 255),  // Colors::kTextOnAccent
    IM_COL32(140, 52, 214, 255),   // Colors::kMeasurementColor
    IM_COL32(255, 255, 255, 244),  // Colors::kMeasurementLabelBg
    IM_COL32(213, 216, 224, 220),  // Colors::kMeasurementLabelEdge
    IM_COL32(26, 27, 31, 255),     // Colors::kMeasurementLabelText
    IM_COL32(80, 80, 92, 130),     // Colors::kMeasurementNotch
    IM_COL32(206, 226, 248, 255),  // Colors::kComparisonBase
    IM_COL32(198, 234, 228, 255),  // Colors::kComparisonTarget
    IM_COL32(248, 226, 172, 255),  // Colors::kComparisonLesser
    IM_COL32(224, 208, 244, 255),  // Colors::kComparisonGreater

    // Centralized from view widgets (kept in Colors enum order):
    IM_COL32(250, 251, 253, 255),  // Colors::kMemChartBg
    IM_COL32(255, 255, 255, 246),  // Colors::kMemChartPanel
    IM_COL32(244, 246, 250, 246),  // Colors::kMemChartPanelAlt
    IM_COL32(218, 221, 229, 210),  // Colors::kMemChartBorder
    IM_COL32(14, 110, 236, 255),   // Colors::kMemChartBorderHot
    IM_COL32(26, 27, 31, 255),     // Colors::kMemChartTextMain
    IM_COL32(108, 112, 122, 255),  // Colors::kMemChartTextDim
    IM_COL32(0, 128, 150, 235),    // Colors::kMemChartRead
    IM_COL32(164, 124, 0, 235),    // Colors::kMemChartWrite
    IM_COL32(120, 76, 186, 235),   // Colors::kMemChartAtomic
    IM_COL32(54, 140, 26, 255),    // Colors::kMemChartUtil
    IM_COL32(172, 126, 0, 255),    // Colors::kMemChartHit
    IM_COL32(200, 52, 66, 255),    // Colors::kMemChartStall
    IM_COL32(60, 64, 88, 32),      // Colors::kMemChartShadow
    IM_COL32(255, 245, 186, 250),  // Colors::kStickyNoteBg
    IM_COL32(214, 176, 66, 230),   // Colors::kStickyNoteBorder
    IM_COL32(255, 236, 158, 250),  // Colors::kStickyNoteHeader
    IM_COL32(60, 64, 88, 34),      // Colors::kStickyNoteShadow
    IM_COL32(56, 46, 14, 255),     // Colors::kStickyNoteText
    IM_COL32(120, 100, 50, 255),   // Colors::kStickyNoteTextMuted
    IM_COL32(190, 146, 34, 235),   // Colors::kStickyNoteAccent
    IM_COL32(190, 146, 34, 255),   // Colors::kStickyNoteResize
    IM_COL32(150, 100, 24, 255),   // Colors::kStickyNoteResizeActive
    IM_COL32(200, 16, 32, 150),    // Colors::kBannerFill
    IM_COL32(255, 255, 255, 40),   // Colors::kBannerBorder
    IM_COL32(255, 255, 255, 255),  // Colors::kBannerText
    IM_COL32(228, 228, 228, 255),  // Colors::kDebugNavBarBg
    IM_COL32(122, 126, 136, 255),  // Colors::kLogTrace
    IM_COL32(48, 104, 158, 255),   // Colors::kLogDebug
    IM_COL32(48, 48, 52, 255),     // Colors::kLogInfo
    IM_COL32(160, 112, 0, 255),    // Colors::kLogWarning
    IM_COL32(188, 40, 44, 255),    // Colors::kLogError
    IM_COL32(168, 12, 20, 255),    // Colors::kLogCritical
    // This must follow the ordering of Colors enum.
};
// Event bar fills, in the same hue order as origin/main. Labels on top come
// from ContrastingTextColor(), which flips at 0.55 Rec.709 luma, so each wheel
// is kept wholly on one side of that line: every bar in a wheel then carries
// the same label color instead of the label flipping bar to bar. The normal
// wheels sit at 0.39-0.50 (dark, white labels) and 0.59-0.76 (light, dark
// labels); the highlight wheels below are all above 0.55.
//
// Both are near opaque - these bars are the densest thing on screen and
// translucency against the track background is what read as washed out.
const std::vector<ImU32> DARK_FLAME_COLORS = {
    IM_COL32(38, 116, 200, 250),  IM_COL32(18, 138, 120, 250),
    IM_COL32(152, 122, 30, 250),  IM_COL32(182, 78, 122, 250),
    IM_COL32(44, 126, 176, 250),  IM_COL32(186, 96, 40, 250),
    IM_COL32(40, 142, 86, 250),   IM_COL32(168, 116, 28, 250),
    IM_COL32(104, 104, 198, 250), IM_COL32(192, 108, 56, 250)
};
const std::vector<ImU32> LIGHT_FLAME_COLORS = {
    IM_COL32(96, 166, 230, 235),  IM_COL32(76, 190, 164, 235),
    IM_COL32(228, 196, 92, 235),  IM_COL32(226, 134, 178, 235),
    IM_COL32(104, 186, 232, 235), IM_COL32(238, 148, 84, 235),
    IM_COL32(80, 190, 136, 235),  IM_COL32(232, 172, 78, 235),
    IM_COL32(152, 152, 236, 235), IM_COL32(240, 154, 100, 235)
};
// Highlighted events keep the Okabe-Ito hue spacing of the original palette so
// the set stays distinguishable to colorblind viewers, but every entry is
// brighter and more saturated than its counterpart in the normal wheel: on a
// dark timeline that jump in luminance is what reads as "highlighted".
const std::vector<ImU32> DARK_HIGHLIGHTED_EVENT_COLORS = {
    IM_COL32(72, 178, 240, 245),  IM_COL32(40, 204, 156, 245),
    IM_COL32(240, 224, 92, 245),  IM_COL32(226, 142, 190, 245),
    IM_COL32(120, 202, 244, 245), IM_COL32(248, 152, 72, 245),
    IM_COL32(48, 212, 124, 245),  IM_COL32(244, 180, 40, 245),
    IM_COL32(160, 162, 248, 245), IM_COL32(252, 168, 88, 245)
};
const std::vector<ImU32> LIGHT_HIGHLIGHTED_EVENT_COLORS = {
    IM_COL32(86, 176, 242, 235),  IM_COL32(54, 200, 156, 235),
    IM_COL32(246, 230, 74, 235),  IM_COL32(232, 144, 192, 235),
    IM_COL32(120, 204, 246, 235), IM_COL32(250, 154, 70, 235),
    IM_COL32(52, 214, 126, 235),  IM_COL32(246, 182, 42, 235),
    IM_COL32(162, 164, 250, 235), IM_COL32(252, 170, 88, 235)
};
inline constexpr const char* FLAME_DARK_COLORMAP_NAME    = "flame_dark";
inline constexpr const char* FLAME_LIGHT_COLORMAP_NAME   = "flame_light";
inline constexpr const char* CONTRAST_DARK_COLORMAP_NAME = "contrast_dark";
inline constexpr const char* CONTRAST_LIGHT_COLORMAP_NAME = "contrast_light";
inline constexpr const char* SETTINGS_FILE_NAME           = "settings_application.json";

// Corner radius scale, one value per elevation tier. Every rounded surface in
// the app reads from this so controls, panels and floating windows share a
// single curvature rhythm instead of each widget picking its own radius.
inline constexpr float RADIUS_CONTROL  = 7.0f;   // buttons, inputs, tabs, grabs
inline constexpr float RADIUS_SURFACE  = 10.0f;  // child panels, images
inline constexpr float RADIUS_FLOATING = 12.0f;  // windows, popups, menus

inline constexpr float       COMPACT_EVENT_HEIGHT         = 6.0f;
inline constexpr float       EVENT_LEVEL_VERTICAL_MARGIN  = 6.0f;
inline constexpr float       EVENT_LEVEL_SPACING          = 1.0f;

SettingsManager&
SettingsManager::GetInstance()
{
    static SettingsManager instance;
    return instance;
}

void
SettingsManager::ApplyColorStyling()
{
    ImGuiStyle& style = ImGui::GetStyle();

    // A focus ring is "the accent at low opacity" in either theme, so tints that
    // are purely a relationship to a themed color are derived here rather than
    // being duplicated into both theme tables.
    const auto theme = [this](Colors color) {
        return ImGui::ColorConvertU32ToFloat4(GetColor(color));
    };
    const auto fade = [](ImVec4 color, float alpha) {
        color.w = alpha;
        return color;
    };

    ImVec4 bgMain          = theme(Colors::kBgMain);
    ImVec4 bgPanel         = theme(Colors::kBgPanel);
    ImVec4 bgFrame         = theme(Colors::kBgFrame);
    ImVec4 accent          = theme(Colors::kAccent);
    ImVec4 accentHover     = theme(Colors::kAccentHover);
    ImVec4 accentActive    = theme(Colors::kAccentActive);
    ImVec4 tabAccent       = theme(Colors::kTabAccent);
    ImVec4 tabAccentHover  = theme(Colors::kTabAccentHover);
    ImVec4 tabAccentActive = theme(Colors::kTabAccentActive);
    ImVec4 borderGray      = theme(Colors::kBorderGray);
    ImVec4 textMain        = theme(Colors::kTextMain);
    ImVec4 textDim         = theme(Colors::kTextDim);
    ImVec4 scrollBg        = theme(Colors::kScrollBg);
    ImVec4 scrollGrab      = theme(Colors::kScrollGrab);
    ImVec4 button          = theme(Colors::kButton);
    ImVec4 buttonHovered   = theme(Colors::kButtonHovered);
    ImVec4 buttonActive    = theme(Colors::kButtonActive);
    ImVec4 hairline        = theme(Colors::kPanelBorderSubtle);
    ImVec4 selection       = theme(Colors::kSelectionBorder);
    ImVec4 transparent     = theme(Colors::kTransparent);
    ImVec4 scrim           = theme(Colors::kLoadingScreenColor);

    // Window. Popups sit on the panel tier rather than the canvas tier, so a
    // menu or tooltip lifts off the window behind it without needing a shadow.
    style.Colors[ImGuiCol_WindowBg]     = bgMain;
    style.Colors[ImGuiCol_ChildBg]      = bgPanel;
    style.Colors[ImGuiCol_PopupBg]      = bgPanel;
    style.Colors[ImGuiCol_Border]       = borderGray;
    style.Colors[ImGuiCol_BorderShadow] = transparent;

    // Frame
    style.Colors[ImGuiCol_FrameBg]         = button;
    style.Colors[ImGuiCol_FrameBgHovered]  = buttonHovered;
    style.Colors[ImGuiCol_FrameBgActive]   = buttonActive;
    style.Colors[ImGuiCol_InputTextCursor] = accent;

    // Title bar. Kept on the neutral tiers: a saturated accent-colored title
    // bar fights every panel docked beneath it.
    style.Colors[ImGuiCol_TitleBg]          = bgPanel;
    style.Colors[ImGuiCol_TitleBgActive]    = bgFrame;
    style.Colors[ImGuiCol_TitleBgCollapsed] = bgPanel;

    // Menu bar
    style.Colors[ImGuiCol_MenuBarBg] = bgPanel;

    // Table styling
    style.Colors[ImGuiCol_TableHeaderBg]     = theme(Colors::kTableHeaderBg);
    style.Colors[ImGuiCol_TableBorderStrong] = theme(Colors::kTableBorderStrong);
    style.Colors[ImGuiCol_TableBorderLight]  = theme(Colors::kTableBorderLight);
    style.Colors[ImGuiCol_TableRowBg]        = theme(Colors::kTableRowBg);
    style.Colors[ImGuiCol_TableRowBgAlt]     = theme(Colors::kTableRowBgAlt);

    // Scrollbar. The track stays a whisper so the inset grab is what the eye
    // tracks; it gains opacity rather than changing hue on interaction.
    style.Colors[ImGuiCol_ScrollbarBg]          = scrollBg;
    style.Colors[ImGuiCol_ScrollbarGrab]        = scrollGrab;
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = fade(scrollGrab, 0.72f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]  = fade(scrollGrab, 0.92f);

    // Checkboxes, radio buttons. The box picks up an accent wash when ticked,
    // but the mark itself stays full accent rather than going light-on-accent:
    // RadioButton draws its dot with CheckMark over a plain FrameBg circle, so
    // a light mark would leave every radio button invisible.
    style.Colors[ImGuiCol_CheckMark]          = accent;
    style.Colors[ImGuiCol_CheckboxSelectedBg] = fade(accent, 0.22f);

    // Slider
    style.Colors[ImGuiCol_SliderGrab]       = accent;
    style.Colors[ImGuiCol_SliderGrabActive] = accentActive;

    // Buttons
    style.Colors[ImGuiCol_Button]        = button;
    style.Colors[ImGuiCol_ButtonHovered] = buttonHovered;
    style.Colors[ImGuiCol_ButtonActive]  = buttonActive;

    // Tabs. Unselected tabs are left unfilled so the strip reads as one
    // surface; the selected tab is the only one that lifts, marked by the
    // accent overline.
    style.Colors[ImGuiCol_Tab]                = transparent;
    style.Colors[ImGuiCol_TabHovered]         = tabAccentHover;
    style.Colors[ImGuiCol_TabActive]          = tabAccent;
    style.Colors[ImGuiCol_TabUnfocused]       = transparent;
    style.Colors[ImGuiCol_TabUnfocusedActive] = tabAccentActive;
    style.Colors[ImGuiCol_TabSelectedOverline] = accent;
    style.Colors[ImGuiCol_TabDimmedSelectedOverline] = fade(accent, 0.4f);
    style.Colors[ImGuiCol_UnsavedMarker]             = accent;

    // Headers - shared by CollapsingHeader, TreeNode, Selectable and MenuItem.
    // A progressive accent wash rather than a solid accent fill: ImGui cannot
    // recolor the row's text per state, so a solid fill would leave dark text
    // sitting on saturated azure.
    style.Colors[ImGuiCol_Header]        = fade(accent, 0.22f);
    style.Colors[ImGuiCol_HeaderHovered] = fade(accent, 0.12f);
    style.Colors[ImGuiCol_HeaderActive]  = fade(accent, 0.32f);

    // Separator, resize grip. The grip is invisible until reached for; the
    // window corner is draggable whether or not it is advertised.
    style.Colors[ImGuiCol_Separator]         = hairline;
    style.Colors[ImGuiCol_SeparatorHovered]  = fade(accent, 0.5f);
    style.Colors[ImGuiCol_SeparatorActive]   = accent;
    style.Colors[ImGuiCol_ResizeGrip]        = transparent;
    style.Colors[ImGuiCol_ResizeGripHovered] = fade(accent, 0.45f);
    style.Colors[ImGuiCol_ResizeGripActive]  = accent;

    // Text
    style.Colors[ImGuiCol_Text]           = textMain;
    style.Colors[ImGuiCol_TextDisabled]   = textDim;
    style.Colors[ImGuiCol_TextLink]       = accent;
    style.Colors[ImGuiCol_TextSelectedBg] = fade(selection, 0.3f);
    style.Colors[ImGuiCol_TreeLines]      = hairline;

    // Drag and drop
    style.Colors[ImGuiCol_DragDropTarget]   = accent;
    style.Colors[ImGuiCol_DragDropTargetBg] = fade(accent, 0.12f);

    // Navigation highlight
    style.Colors[ImGuiCol_NavCursor]             = fade(accent, 0.8f);
    style.Colors[ImGuiCol_NavWindowingHighlight] = fade(accent, 0.7f);
    style.Colors[ImGuiCol_NavWindowingDimBg]     = scrim;

    // Plot colors
    style.Colors[ImGuiCol_PlotLines]            = accent;
    style.Colors[ImGuiCol_PlotLinesHovered]     = accentHover;
    style.Colors[ImGuiCol_PlotHistogram]        = accent;
    style.Colors[ImGuiCol_PlotHistogramHovered] = accentHover;

    // Modal window dim. Shares the loading scrim so every "the app is busy or
    // blocked" overlay dims the app by the same amount.
    style.Colors[ImGuiCol_ModalWindowDimBg] = scrim;

    // ImPlot geometry. The per-chart call sites already push their own colors,
    // but nothing sets these, so every plot has been drawing ImPlot's default
    // boxed frame with heavy grid lines and 10px padding on each edge.
    ImPlotStyle& plot_style      = ImPlot::GetStyle();
    plot_style.PlotBorderSize    = 0.0f;
    plot_style.LineWeight        = 1.6f;
    plot_style.MarkerSize        = 3.5f;
    plot_style.MarkerWeight      = 1.5f;
    plot_style.FillAlpha         = 0.28f;
    plot_style.MinorAlpha        = 0.18f;
    plot_style.MajorGridSize     = ImVec2(1.0f, 1.0f);
    plot_style.MinorGridSize     = ImVec2(1.0f, 1.0f);
    plot_style.MajorTickLen      = ImVec2(6.0f, 6.0f);
    plot_style.MinorTickLen      = ImVec2(3.0f, 3.0f);
    plot_style.PlotPadding       = ImVec2(6.0f, 6.0f);
    plot_style.LabelPadding      = ImVec2(4.0f, 4.0f);
    plot_style.LegendPadding     = ImVec2(8.0f, 8.0f);
    plot_style.AnnotationPadding = ImVec2(4.0f, 3.0f);

    plot_style.Colors[ImPlotCol_FrameBg]      = transparent;
    plot_style.Colors[ImPlotCol_PlotBg]       = transparent;
    plot_style.Colors[ImPlotCol_PlotBorder]   = transparent;
    plot_style.Colors[ImPlotCol_AxisGrid]     = theme(Colors::kGridColor);
    plot_style.Colors[ImPlotCol_LegendBg]     = bgPanel;
    plot_style.Colors[ImPlotCol_LegendBorder] = hairline;
}

FontManager&
SettingsManager::GetFontManager()
{
    return m_font_manager;
}

void
SettingsManager::SerializeDisplaySettings(jt::Json& json)
{
    jt::Json& ds = json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_DISPLAY];
    ds[JSON_KEY_SETTINGS_DISPLAY_DARK_MODE] =
        m_usersettings.display_settings.use_dark_mode;
    ds[JSON_KEY_SETTINGS_DISPLAY_FONT_SIZE] =
        m_usersettings.display_settings.font_size_index;
    ds[JSON_KEY_SETTINGS_DISPLAY_NODE_COLORS] =
        m_usersettings.display_settings.show_node_colors;
    ds[JSON_KEY_SETTINGS_DISPLAY_COMPACT_SIDEBAR] =
        m_usersettings.display_settings.compact_sidebar;
}

void
SettingsManager::DeserializeDisplaySettings(jt::Json& json)
{
    if(json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_DISPLAY].isObject())
    {
        jt::Json& ds = json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_DISPLAY];
        if(ds[JSON_KEY_SETTINGS_DISPLAY_DARK_MODE].isBool())
        {
            m_usersettings.display_settings.use_dark_mode =
                ds[JSON_KEY_SETTINGS_DISPLAY_DARK_MODE].getBool();
        }
        if(ds[JSON_KEY_SETTINGS_DISPLAY_FONT_SIZE].isLong())
        {
            m_usersettings.display_settings.font_size_index =
                GetFontManager().ClampFontSizeIndex(
                    static_cast<int>(ds[JSON_KEY_SETTINGS_DISPLAY_FONT_SIZE].getLong()));
        }
        if(ds[JSON_KEY_SETTINGS_DISPLAY_NODE_COLORS].isBool())
        {
            m_usersettings.display_settings.show_node_colors =
                ds[JSON_KEY_SETTINGS_DISPLAY_NODE_COLORS].getBool();
        }
        if(ds[JSON_KEY_SETTINGS_DISPLAY_COMPACT_SIDEBAR].isBool())
        {
            m_usersettings.display_settings.compact_sidebar =
                ds[JSON_KEY_SETTINGS_DISPLAY_COMPACT_SIDEBAR].getBool();
        }
    }
}

void
SettingsManager::SaveSettingsJson()
{
    jt::Json settings_json;
    settings_json[JSON_KEY_VERSION] = "1.0";

    SerializeInternalSettings(settings_json);
    SerializeDisplaySettings(settings_json);
    SerializeUnitSettings(settings_json);
    SerializeOtherSettings(settings_json);
    SerializeHotkeySettings(settings_json);
    SerializeProfilerSettings(settings_json);
    SerializeAssistantSettings(settings_json);
    SerializeAppWindowSettings(settings_json);

    std::ofstream out_file(m_json_path);
    if(out_file.is_open())
    {
        out_file << settings_json.toStringPretty();
        out_file.close();
    }
}

void
SettingsManager::LoadSettingsJson()
{
    std::ifstream in_file(m_json_path);
    if(!in_file.is_open()) return;

    std::string json_str((std::istreambuf_iterator<char>(in_file)),
                         std::istreambuf_iterator<char>());
    in_file.close();

    std::pair<jt::Json::Status, jt::Json> result = jt::Json::parse(json_str);
    if(result.first != jt::Json::success || !result.second.isObject()) return;

    if(result.second[JSON_KEY_GROUP_SETTINGS].isObject())
    {
        DeserializeInternalSettings(result.second);
        DeserializeDisplaySettings(result.second);
        DeserializeUnitSettings(result.second);
        DeserializeOtherSettings(result.second);
        DeserializeHotkeySettings(result.second);
        DeserializeProfilerSettings(result.second);
        DeserializeAssistantSettings(result.second);
        DeserializeAppWindowSettings(result.second);
#ifdef ROCPROFVIS_ENABLE_AGENTIC_PROFILING
        MigrateLegacyAssistantToken();
#endif
    }
    else
    {
        spdlog::warn("Settings file failed to load");
    }
}

std::filesystem::path
SettingsManager::GetStandardConfigPath()
{
    std::filesystem::path config_dir = get_application_config_path(true);
    return config_dir / SETTINGS_FILE_NAME;
}

void
SettingsManager::ApplyUserDisplaySettings(const UserSettings& old_settings)
{
    (void) old_settings;  // currently unused
    if(m_usersettings.display_settings.use_dark_mode)
    {
        m_color_store = &DARK_THEME_COLORS;
        ImGui::StyleColorsDark();
        ImPlot::StyleColorsDark();
    }
    else
    {
        m_color_store = &LIGHT_THEME_COLORS;
        ImGui::StyleColorsLight();
        ImPlot::StyleColorsLight();
    }
    ApplyColorStyling();

    m_usersettings.display_settings.font_size_index =
        GetFontManager().ClampFontSizeIndex(m_usersettings.display_settings.font_size_index);
    GetFontManager().SetFontSize(m_usersettings.display_settings.font_size_index);
}

void
SettingsManager::ApplyUserUnitSettings(const UserSettings& old_settings)
{
    // Notify views when time labels need to be rebuilt.
    if(old_settings.unit_settings.time_format != m_usersettings.unit_settings.time_format)
    {
        EventManager::GetInstance()->AddEvent(
            std::make_shared<RocEvent>(static_cast<int>(RocEvents::kTimeFormatChanged)));
    }
}

ImU32
SettingsManager::GetColor(Colors color) const
{
    return (*m_color_store)[static_cast<int>(color)];
}

const std::vector<ImU32>&
SettingsManager::GetColorWheel() const
{
    return m_usersettings.display_settings.use_dark_mode ? DARK_FLAME_COLORS
                                                         : LIGHT_FLAME_COLORS;
}

const std::vector<ImU32>&
SettingsManager::GetHighlightedEventColorWheel() const
{
    return m_usersettings.display_settings.use_dark_mode ? DARK_HIGHLIGHTED_EVENT_COLORS
                                                         : LIGHT_HIGHLIGHTED_EVENT_COLORS;
}

const char*
SettingsManager::GetFlameColormapName() const
{
    return m_usersettings.display_settings.use_dark_mode ? FLAME_DARK_COLORMAP_NAME
                                                         : FLAME_LIGHT_COLORMAP_NAME;
}

const char*
SettingsManager::GetContrastColormapName() const
{
    return m_usersettings.display_settings.use_dark_mode ? CONTRAST_DARK_COLORMAP_NAME
                                                         : CONTRAST_LIGHT_COLORMAP_NAME;
}

SettingsManager::SettingsManager()
: m_color_store(nullptr)
, m_usersettings_default(
      { DisplaySettings{ false, 6, true, false }, UnitSettings{ TimeFormat::kTimecode },
        false, false, LOG_VIEWER_MAX_ENTRIES_DEFAULT,
        LogViewerSettings{ LOG_VIEWER_DEFAULT_LEVEL_MASK, true, false, false, false },
        AssistantSettings{}, false })
, m_usersettings(m_usersettings_default)
, m_appwindowsettings({ AppWindowSettings{ true, true, true, true, false } })
, m_json_path(GetStandardConfigPath())
{}

SettingsManager::~SettingsManager() { SaveSettingsJson(); }

bool
SettingsManager::Init()
{
    bool result = false;
    InitStyling();
    result = m_font_manager.Init();
    LoadSettingsJson();
    ApplyUserSettings(m_usersettings_default);
    return result;
}

UserSettings&
SettingsManager::GetUserSettings()
{
    return m_usersettings;
}

const UserSettings&
SettingsManager::GetDefaultUserSettings() const
{
    return m_usersettings_default;
}

void
SettingsManager::ApplyUserSettings(const UserSettings& old_settings, bool save_json)
{
    ApplyUserDisplaySettings(old_settings);
    ApplyUserUnitSettings(old_settings);
    if(save_json)
    {
        SaveSettingsJson();
    }
}

void
SettingsManager::InitStyling()
{
    ImGuiStyle& style     = ImGui::GetStyle();
    m_default_imgui_style = style;  // Store the default ImGui style.

    // Set sizes and rounding. Padding and spacing are deliberately left alone:
    // the sidebar rows and the timeline tracks both derive their height from
    // GetFrameHeight(), so changing FramePadding here shifts a layout that is
    // pixel-aligned across two panels.
    style.CellPadding       = ImVec2(12, 8);
    style.FrameBorderSize   = 0.0f;
    style.WindowBorderSize  = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.TabBorderSize     = 0.0f;
    style.FrameRounding     = RADIUS_CONTROL;
    style.TabRounding       = RADIUS_CONTROL;
    // Rounding is clamped to half the grab's smaller side, so a radius above
    // GrabMinSize turns the slider knob into a true pill instead of a
    // rounded rectangle.
    style.GrabRounding      = RADIUS_SURFACE;
    style.MenuItemRounding  = RADIUS_CONTROL;
    style.WindowRounding    = RADIUS_FLOATING;
    style.PopupRounding     = RADIUS_FLOATING;
    style.ChildRounding     = RADIUS_SURFACE;
    style.ImageRounding     = RADIUS_SURFACE;
    style.FramePadding      = ImVec2(12, 6);
    style.ItemSpacing       = ImVec2(10, 8);
    style.ItemInnerSpacing  = ImVec2(8, 6);
    style.WindowPadding     = ImVec2(8, 8);
    style.GrabMinSize       = 12.0f;
    style.IndentSpacing     = 18.0f;

    // Scrollbars read as a floating pill rather than a gutter: a narrower bar,
    // with the grab inset from the track on every side.
    style.ScrollbarSize     = 12.0f;
    style.ScrollbarRounding = 6.0f;
    style.ScrollbarPadding  = 2.0f;

    // Hairline rules everywhere a divider is needed, and a single-pixel accent
    // strip over the selected tab.
    style.SeparatorSize           = 1.0f;
    style.SeparatorTextBorderSize = 1.0f;
    style.TabBarBorderSize        = 1.0f;
    style.TabBarOverlineSize      = 2.0f;

    // Disabled controls recede further than ImGui's default 0.60 so a greyed
    // row is unmistakably inert rather than merely dim.
    style.DisabledAlpha = 0.42f;

    // Centered window titles, and a tighter tessellation budget so the larger
    // corner radii above stay smooth instead of showing facets.
    style.WindowTitleAlign          = ImVec2(0.5f, 0.5f);
    style.CircleTessellationMaxError = 0.15f;

    m_default_style = style;  // Store the customized style.

    const auto add_flame_colormap = [](const char* name,
                                       const std::vector<ImU32>& flame_colors) {
        std::vector<ImU32> colormap;
        colormap.reserve(flame_colors.size() + 1);
        for(const ImU32& flame_color : flame_colors)
        {
            colormap.push_back(255 << IM_COL32_A_SHIFT | flame_color);
        }
        colormap.push_back(IM_COL32(235, 98, 98, 255));
        ImPlot::AddColormap(name, colormap.data(), static_cast<int>(colormap.size()));
    };

    add_flame_colormap(FLAME_DARK_COLORMAP_NAME, DARK_FLAME_COLORS);
    add_flame_colormap(FLAME_LIGHT_COLORMAP_NAME, LIGHT_FLAME_COLORS);

    const std::vector<ImU32> contrast_dark_colormap = {
        IM_COL32(255, 255, 255, 255), IM_COL32(255, 255, 255, 255)
    };
    const std::vector<ImU32> contrast_light_colormap = {
        IM_COL32(25, 25, 25, 255), IM_COL32(25, 25, 25, 255)
    };
    ImPlot::AddColormap(CONTRAST_DARK_COLORMAP_NAME, contrast_dark_colormap.data(),
                        static_cast<int>(contrast_dark_colormap.size()));
    ImPlot::AddColormap(CONTRAST_LIGHT_COLORMAP_NAME, contrast_light_colormap.data(),
                        static_cast<int>(contrast_light_colormap.size()));
}

const ImGuiStyle&
SettingsManager::GetDefaultIMGUIStyle() const
{
    return m_default_imgui_style;
}

const ImGuiStyle&
SettingsManager::GetDefaultStyle() const
{
    return m_default_style;
}

InternalSettings&
SettingsManager::GetInternalSettings()
{
    return m_internalsettings;
}

AppWindowSettings&
SettingsManager::GetAppWindowSettings()
{
    return m_appwindowsettings;
}

void
SettingsManager::AddRecentFile(const std::string& file_path)
{
    RemoveRecentFile(file_path);
    m_internalsettings.recent_files.emplace_front(file_path);
    if(m_internalsettings.recent_files.size() > MAX_RECENT_FILES)
    {
        m_internalsettings.recent_files.pop_back();
    }
}

void
SettingsManager::RemoveRecentFile(const std::string& file_path)
{
    auto pos = std::find(m_internalsettings.recent_files.begin(),
                         m_internalsettings.recent_files.end(), file_path);
    if(pos != m_internalsettings.recent_files.end())
    {
        m_internalsettings.recent_files.erase(pos);
    }
}

void
SettingsManager::ClearRecentFiles()
{
    m_internalsettings.recent_files.clear();
}

void
SettingsManager::SerializeInternalSettings(jt::Json& json)
{
    jt::Json& is = json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_INTERNAL];
    int       i  = 0;
    for(const std::string& file : m_internalsettings.recent_files)
    {
        is[JSON_KEY_SETTINGS_INTERNAL_RECENT_FILES][i++] = file;
    }
}

void
SettingsManager::DeserializeInternalSettings(jt::Json& json)
{
    jt::Json& is = json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_INTERNAL];
    if(is[JSON_KEY_SETTINGS_INTERNAL_RECENT_FILES].isArray())
    {
        for(jt::Json& entry : is[JSON_KEY_SETTINGS_INTERNAL_RECENT_FILES].getArray())
        {
            if(entry.isString())
            {
                m_internalsettings.recent_files.emplace_back(entry.getString());
            }
        }
    }
}

void
SettingsManager::SerializeOtherSettings(jt::Json& json)
{
    jt::Json& os = json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_OTHER];

    os[JSON_KEY_SETTINGS_DONT_ASK_BEFORE_EXIT] = m_usersettings.dont_ask_before_exit;
    os[JSON_KEY_SETTINGS_DONT_ASK_BEFORE_TAB_CLOSE] = m_usersettings.dont_ask_before_tab_closing;
    os[JSON_KEY_SETTINGS_LINUX_DRAG_REPAIR]         = m_usersettings.linux_drag_repair;
    os[JSON_KEY_SETTINGS_LOG_VIEWER_MAX_ENTRIES] = m_usersettings.log_viewer_max_entries;

    os[JSON_KEY_SETTINGS_LOG_VIEWER_LEVEL_MASK]    = m_usersettings.log_viewer.level_mask;
    os[JSON_KEY_SETTINGS_LOG_VIEWER_AUTO_SCROLL]   = m_usersettings.log_viewer.auto_scroll;
    os[JSON_KEY_SETTINGS_LOG_VIEWER_USE_REGEX]     = m_usersettings.log_viewer.use_regex;
    os[JSON_KEY_SETTINGS_LOG_VIEWER_RELATIVE_TIME] = m_usersettings.log_viewer.relative_time;
    os[JSON_KEY_SETTINGS_LOG_VIEWER_VISIBLE]       = m_usersettings.log_viewer.visible;
}

void
SettingsManager::DeserializeOtherSettings(jt::Json& json)
{
    jt::Json& os = json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_OTHER];
    if(os[JSON_KEY_SETTINGS_DONT_ASK_BEFORE_EXIT].isBool())
    {
        m_usersettings.dont_ask_before_exit =
            static_cast<bool>(os[JSON_KEY_SETTINGS_DONT_ASK_BEFORE_EXIT].getBool());
    }
    if(os[JSON_KEY_SETTINGS_DONT_ASK_BEFORE_TAB_CLOSE].isBool())
    {
        m_usersettings.dont_ask_before_tab_closing =
            static_cast<bool>(os[JSON_KEY_SETTINGS_DONT_ASK_BEFORE_TAB_CLOSE].getBool());
    }
    if(os[JSON_KEY_SETTINGS_LINUX_DRAG_REPAIR].isBool())
    {
        m_usersettings.linux_drag_repair =
            static_cast<bool>(os[JSON_KEY_SETTINGS_LINUX_DRAG_REPAIR].getBool());
    }
    if(os[JSON_KEY_SETTINGS_LOG_VIEWER_MAX_ENTRIES].isLong())
    {
        int value = static_cast<int>(os[JSON_KEY_SETTINGS_LOG_VIEWER_MAX_ENTRIES].getLong());
        m_usersettings.log_viewer_max_entries =
            std::clamp(value, LOG_VIEWER_MAX_ENTRIES_MIN, LOG_VIEWER_MAX_ENTRIES_MAX);
    }
    if(os[JSON_KEY_SETTINGS_LOG_VIEWER_LEVEL_MASK].isLong())
    {
        m_usersettings.log_viewer.level_mask =
            static_cast<int>(os[JSON_KEY_SETTINGS_LOG_VIEWER_LEVEL_MASK].getLong());
    }
    if(os[JSON_KEY_SETTINGS_LOG_VIEWER_AUTO_SCROLL].isBool())
    {
        m_usersettings.log_viewer.auto_scroll =
            static_cast<bool>(os[JSON_KEY_SETTINGS_LOG_VIEWER_AUTO_SCROLL].getBool());
    }
    if(os[JSON_KEY_SETTINGS_LOG_VIEWER_USE_REGEX].isBool())
    {
        m_usersettings.log_viewer.use_regex =
            static_cast<bool>(os[JSON_KEY_SETTINGS_LOG_VIEWER_USE_REGEX].getBool());
    }
    if(os[JSON_KEY_SETTINGS_LOG_VIEWER_RELATIVE_TIME].isBool())
    {
        m_usersettings.log_viewer.relative_time =
            static_cast<bool>(os[JSON_KEY_SETTINGS_LOG_VIEWER_RELATIVE_TIME].getBool());
    }
    if(os[JSON_KEY_SETTINGS_LOG_VIEWER_VISIBLE].isBool())
    {
        m_usersettings.log_viewer.visible =
            static_cast<bool>(os[JSON_KEY_SETTINGS_LOG_VIEWER_VISIBLE].getBool());
    }
}

void
SettingsManager::SerializeUnitSettings(jt::Json& json)
{
    jt::Json& us = json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_UNITS];
    us[JSON_KEY_SETTINGS_UNITS_TIME_FORMAT] =
        static_cast<int>(m_usersettings.unit_settings.time_format);
}

void
SettingsManager::DeserializeUnitSettings(jt::Json& json)
{
    jt::Json& us = json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_UNITS];
    if(us[JSON_KEY_SETTINGS_UNITS_TIME_FORMAT].isLong())
    {
        m_usersettings.unit_settings.time_format =
            static_cast<TimeFormat>(us[JSON_KEY_SETTINGS_UNITS_TIME_FORMAT].getLong());
    }
}

const float
SettingsManager::GetEventLevelHeight() const
{
    return std::ceil(ImGui::GetTextLineHeight() + EVENT_LEVEL_VERTICAL_MARGIN + EVENT_LEVEL_SPACING);
}

const float
SettingsManager::GetEventLevelCompactHeight() const
{
    return COMPACT_EVENT_HEIGHT;
}

const float
SettingsManager::GetEventLevelSpacing() const
{
    return EVENT_LEVEL_SPACING;
}

void
SettingsManager::SerializeHotkeySettings(jt::Json& json)
{
    auto& hk_mgr = HotkeyManager::GetInstance();
    jt::Json& hs = json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_HOTKEYS];

    for(size_t i = 0; i < kHotkeyActionCount; ++i)
    {
        HotkeyActionId action_id = static_cast<HotkeyActionId>(i);
        const auto&    info      = HotkeyManager::GetActionInfo(action_id);
        HotkeyBinding  binding   = hk_mgr.GetBinding(action_id);

        if(binding.primary != info.default_binding.primary ||
           binding.alternate != info.default_binding.alternate)
        {
            jt::Json entry;
            entry["primary"]   = HotkeyManager::KeyChordToString(binding.primary);
            entry["alternate"] = HotkeyManager::KeyChordToString(binding.alternate);
            hs[info.key]       = entry;
        }
    }
}

void
SettingsManager::DeserializeHotkeySettings(jt::Json& json)
{
    jt::Json& hs = json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_HOTKEYS];
    if(!hs.isObject())
        return;

    auto& hk_mgr = HotkeyManager::GetInstance();

    for(size_t i = 0; i < kHotkeyActionCount; ++i)
    {
        HotkeyActionId action_id = static_cast<HotkeyActionId>(i);
        const auto&    info      = HotkeyManager::GetActionInfo(action_id);
        jt::Json&      value     = hs[info.key];

        if(!value.isObject())
            continue;

        HotkeyBinding binding = info.default_binding;
        if(value["primary"].isString())
        {
            binding.primary = HotkeyManager::StringToKeyChord(
                value["primary"].getString());
        }
        if(value["alternate"].isString())
        {
            binding.alternate = HotkeyManager::StringToKeyChord(
                value["alternate"].getString());
        }

        hk_mgr.SetBinding(action_id, binding);
    }
}

void
SettingsManager::SaveHotkeySettings()
{
    SaveSettingsJson();
}

void
SettingsManager::SaveProfilerSettings()
{
    SaveSettingsJson();
}

ProfilerSettings&
SettingsManager::GetProfilerSettings()
{
    return m_profilersettings;
}

void
SettingsManager::SerializeProfilerSettings(jt::Json& json)
{
    jt::Json& ps = json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_PROFILER];
    ps[JSON_KEY_SETTINGS_PROFILER_OUTPUT_DIR] = m_profilersettings.profiler_output_directory;
    ps[JSON_KEY_SETTINGS_PROFILER_AUTO_LOAD] = m_profilersettings.auto_load_trace;
    ps["last_preset_name"] = m_profilersettings.last_preset_name;
    ps["last_profiler_id"] = m_profilersettings.last_profiler_id;
    ps["last_ssh_connection_id"] = m_profilersettings.last_ssh_connection_id;

    int rt_idx = 0;
    for (auto const& t : m_profilersettings.recent_targets)
    {
        ps["recent_targets"][rt_idx++] = t;
    }
}

void
SettingsManager::DeserializeProfilerSettings(jt::Json& json)
{
    jt::Json& ps = json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_PROFILER];
    if(ps[JSON_KEY_SETTINGS_PROFILER_OUTPUT_DIR].isString())
    {
        m_profilersettings.profiler_output_directory = ps[JSON_KEY_SETTINGS_PROFILER_OUTPUT_DIR].getString();
    }
    if(ps[JSON_KEY_SETTINGS_PROFILER_AUTO_LOAD].isBool())
    {
        m_profilersettings.auto_load_trace = ps[JSON_KEY_SETTINGS_PROFILER_AUTO_LOAD].getBool();
    }
    if(ps["last_preset_name"].isString())
    {
        m_profilersettings.last_preset_name = ps["last_preset_name"].getString();
    }
    if(ps["last_profiler_id"].isString())
    {
        m_profilersettings.last_profiler_id = ps["last_profiler_id"].getString();
    }
    if(ps["last_ssh_connection_id"].isString())
    {
        m_profilersettings.last_ssh_connection_id = ps["last_ssh_connection_id"].getString();
    }
    if(ps["recent_targets"].isArray())
    {
        m_profilersettings.recent_targets.clear();
        for (jt::Json& item : ps["recent_targets"].getArray())
        {
            if (item.isString())
            {
                m_profilersettings.recent_targets.push_back(item.getString());
            }
        }
    }
}

// The API keys are deliberately absent: those live in the credential store.
void
SettingsManager::SerializeAssistantSettings(jt::Json& json)
{
    jt::Json& as = json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_ASSISTANT];
    int32_t   i  = 0;
    for(const AssistantProvider& provider : m_usersettings.assistant.providers)
    {
        jt::Json& entry = as[JSON_KEY_SETTINGS_ASSISTANT_PROVIDERS][i++];
        entry[JSON_KEY_SETTINGS_ASSISTANT_NAME]         = provider.name;
        entry[JSON_KEY_SETTINGS_ASSISTANT_ENDPOINT_URL] = provider.endpoint_url;
        entry[JSON_KEY_SETTINGS_ASSISTANT_MODEL]        = provider.model;
    }
    as[JSON_KEY_SETTINGS_ASSISTANT_ACTIVE] =
        static_cast<int>(m_usersettings.assistant.active);
}

// Also upgrades the pre-provider single-endpoint shape.
/*
 * Makes every endpoint name distinct, and gives an unnamed one a name.
 *
 * The name is the credential-store key, so two endpoints sharing one would
 * share a key: the token entered for one host would be posted to the other's.
 * Nothing in the UI creates a duplicate, but a hand-edited settings file can,
 * and that is exactly the case where the mistake is invisible. Suffixing is
 * better than dropping the endpoint, which would silently lose a configuration.
 */
void
SettingsManager::MakeAssistantProviderNamesUnique()
{
    std::set<std::string> taken;
    for(AssistantProvider& provider : m_usersettings.assistant.providers)
    {
        if(provider.name.empty())
        {
            provider.name = ASSISTANT_DEFAULT_PROVIDER_NAME;
        }
        if(taken.insert(provider.name).second)
        {
            continue;
        }
        const std::string base = provider.name;
        for(size_t suffix = 2;; ++suffix)
        {
            const std::string candidate = base + " (" + std::to_string(suffix) + ")";
            if(taken.insert(candidate).second)
            {
                spdlog::warn("Assistant endpoint \"{}\" was duplicated; renamed to \"{}\" "
                             "so the two do not share one stored key",
                             base, candidate);
                provider.name = candidate;
                break;
            }
        }
    }
}

void
SettingsManager::DeserializeAssistantSettings(jt::Json& json)
{
    jt::Json& as = json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_ASSISTANT];

    if(as[JSON_KEY_SETTINGS_ASSISTANT_PROVIDERS].isArray())
    {
        m_usersettings.assistant.providers.clear();
        for(jt::Json& entry : as[JSON_KEY_SETTINGS_ASSISTANT_PROVIDERS].getArray())
        {
            if(!entry.isObject())
            {
                continue;
            }
            AssistantProvider provider;
            if(entry[JSON_KEY_SETTINGS_ASSISTANT_NAME].isString())
            {
                provider.name = entry[JSON_KEY_SETTINGS_ASSISTANT_NAME].getString();
            }
            if(entry[JSON_KEY_SETTINGS_ASSISTANT_ENDPOINT_URL].isString())
            {
                provider.endpoint_url =
                    entry[JSON_KEY_SETTINGS_ASSISTANT_ENDPOINT_URL].getString();
            }
            if(entry[JSON_KEY_SETTINGS_ASSISTANT_MODEL].isString())
            {
                provider.model = entry[JSON_KEY_SETTINGS_ASSISTANT_MODEL].getString();
            }
            m_usersettings.assistant.providers.push_back(provider);
        }
        for(AssistantProvider& provider : m_usersettings.assistant.providers)
        {
            ApplyAssistantEndpointDefaults(provider);
        }
        MakeAssistantProviderNamesUnique();
    }
    else if(as[JSON_KEY_SETTINGS_ASSISTANT_ENDPOINT_URL].isString())
    {
        // Written before routes were configurable: fold the single endpoint
        // into the list under the default name, so the key already in the
        // credential store keeps working.
        AssistantProvider provider;
        provider.name         = ASSISTANT_DEFAULT_PROVIDER_NAME;
        provider.endpoint_url = as[JSON_KEY_SETTINGS_ASSISTANT_ENDPOINT_URL].getString();
        if(as[JSON_KEY_SETTINGS_ASSISTANT_MODEL].isString())
        {
            provider.model = as[JSON_KEY_SETTINGS_ASSISTANT_MODEL].getString();
        }
        ApplyAssistantEndpointDefaults(provider);
        m_usersettings.assistant.providers.clear();
        m_usersettings.assistant.providers.push_back(provider);
    }

    m_usersettings.assistant.active = 0;
    if(as[JSON_KEY_SETTINGS_ASSISTANT_ACTIVE].isLong())
    {
        const int64_t active = as[JSON_KEY_SETTINGS_ASSISTANT_ACTIVE].getLong();
        if(active > 0 &&
           static_cast<size_t>(active) < m_usersettings.assistant.providers.size())
        {
            m_usersettings.assistant.active = static_cast<size_t>(active);
        }
    }
}

const AssistantProvider*
SettingsManager::GetActiveAssistantProvider() const
{
    const std::vector<AssistantProvider>& providers = m_usersettings.assistant.providers;
    if(m_usersettings.assistant.active >= providers.size())
    {
        return nullptr;
    }
    return &providers[m_usersettings.assistant.active];
}

void
SettingsManager::SerializeAppWindowSettings(jt::Json& json)
{
    jt::Json& aw = json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_APP_WINDOW];
    aw[JSON_KEY_SETTINGS_APP_WINDOW_TOOLBAR]       = m_appwindowsettings.show_toolbar;
    aw[JSON_KEY_SETTINGS_APP_WINDOW_DETAILS_PANEL] =
        m_appwindowsettings.show_details_panel;
    aw[JSON_KEY_SETTINGS_APP_WINDOW_SIDEBAR]   = m_appwindowsettings.show_sidebar;
    aw[JSON_KEY_SETTINGS_APP_WINDOW_HISTOGRAM] = m_appwindowsettings.show_histogram;
    aw[JSON_KEY_SETTINGS_APP_WINDOW_SUMMARY]   = m_appwindowsettings.show_summary;
}

void
SettingsManager::DeserializeAppWindowSettings(jt::Json& json)
{
    jt::Json& aw = json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_APP_WINDOW];
    m_appwindowsettings.show_toolbar =
        JsonUtils::GetBool(aw, JSON_KEY_SETTINGS_APP_WINDOW_TOOLBAR,
                           m_appwindowsettings.show_toolbar);
    m_appwindowsettings.show_details_panel =
        JsonUtils::GetBool(aw, JSON_KEY_SETTINGS_APP_WINDOW_DETAILS_PANEL,
                           m_appwindowsettings.show_details_panel);
    m_appwindowsettings.show_sidebar =
        JsonUtils::GetBool(aw, JSON_KEY_SETTINGS_APP_WINDOW_SIDEBAR,
                           m_appwindowsettings.show_sidebar);
    m_appwindowsettings.show_histogram =
        JsonUtils::GetBool(aw, JSON_KEY_SETTINGS_APP_WINDOW_HISTOGRAM,
                           m_appwindowsettings.show_histogram);
    m_appwindowsettings.show_summary =
        JsonUtils::GetBool(aw, JSON_KEY_SETTINGS_APP_WINDOW_SUMMARY,
                           m_appwindowsettings.show_summary);
}

}  // namespace View
}  // namespace RocProfVis
