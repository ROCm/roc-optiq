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
constexpr std::array DARK_THEME_COLORS = {
    IM_COL32(34, 37, 48, 255),     // Colors::kMetaDataColor
    IM_COL32(39, 43, 56, 255),     // Colors::kMetaDataColorSelected
    IM_COL32(50, 59, 76, 255),     // Colors::kMetaDataSeparator
    IM_COL32(0, 0, 0, 0),          // Colors::kTransparent
    IM_COL32(244, 96, 110, 255),   // Colors::kTextError
    IM_COL32(120, 220, 144, 255),  // Colors::kTextSuccess
    IM_COL32(255, 199, 64, 255),   // Colors::kTextWarning
    IM_COL32(120, 162, 255, 220),  // Colors::kFlameChartColor
    IM_COL32(120, 130, 150, 32),   // Colors::kGridColor
    IM_COL32(142, 176, 236, 255),  // Colors::kGridRed
    IM_COL32(106, 164, 232, 255),  // Colors::kSelectionBorder
    IM_COL32(106, 164, 232, 52),   // Colors::kSelection
    IM_COL32(140, 148, 168, 255),  // Colors::kBoundBox
    IM_COL32(29, 30, 38, 255),     // Colors::kFillerColor
    IM_COL32(70, 82, 104, 255),    // Colors::kScrollBarColor
    IM_COL32(120, 162, 255, 38),   // Colors::kHighlightChart
    IM_COL32(29, 30, 38, 255),     // Colors::kRulerBgColor
    IM_COL32(228, 232, 244, 255),  // Colors::kRulerTextColor
    IM_COL32(145, 156, 174, 255),  // Colors::kScrubberNumberColor
    IM_COL32(78, 152, 220, 210),   // Colors::kArrowColor
    IM_COL32(50, 59, 76, 255),     // Colors::kBorderColor
    IM_COL32(50, 56, 72, 255),     // Colors::kSplitterColor
    IM_COL32(29, 30, 38, 255),     // Colors::kBgMain
    IM_COL32(34, 37, 48, 255),     // Colors::kBgPanel
    IM_COL32(39, 43, 56, 255),     // Colors::kBgFrame
    IM_COL32(48, 56, 76, 255),     // Colors::kComboFill
    IM_COL32(106, 164, 232, 255),  // Colors::kAccent
    IM_COL32(140, 190, 245, 255),  // Colors::kAccentHover
    IM_COL32(78, 132, 202, 255),   // Colors::kAccentActive
    IM_COL32(34, 37, 48, 255),     // Colors::kTabAccent
    IM_COL32(44, 50, 66, 255),     // Colors::kTabAccentHover
    IM_COL32(39, 43, 56, 255),     // Colors::kTabAccentActive
    IM_COL32(50, 59, 76, 255),     // Colors::kBorderGray
    IM_COL32(238, 243, 255, 255),  // Colors::kTextMain
    IM_COL32(145, 156, 174, 255),  // Colors::kTextDim
    IM_COL32(29, 30, 38, 255),     // Colors::kScrollBg
    IM_COL32(70, 82, 104, 255),    // Colors::kScrollGrab
    IM_COL32(32, 34, 44, 255),     // Colors::kTableHeaderBg
    IM_COL32(50, 59, 76, 255),     // Colors::kTableBorderStrong
    IM_COL32(39, 43, 56, 255),     // Colors::kTableBorderLight
    IM_COL32(32, 36, 47, 255),     // Colors::kTableRowBg
    IM_COL32(38, 42, 54, 255),     // Colors::kTableRowBgAlt
    IM_COL32(34, 37, 48, 255),     // Colors::kTableBorderInner
    IM_COL32(44, 50, 64, 255),     // Colors::kTableBorderOuter
    IM_COL32(40, 45, 58, 255),     // Colors::kPanelBorderSubtle
    IM_COL32(106, 164, 232, 230),  // Colors::kEventHighlight
    IM_COL32(130, 210, 178, 230),  // Colors::kEventSearchHighlight
    IM_COL32(106, 164, 232, 85),   // Colors::kAreaOfInterest
    IM_COL32(120, 162, 255, 120),  // Colors::kLineChartColor
    IM_COL32(44, 52, 70, 255),     // Colors::kButton
    IM_COL32(56, 66, 88, 255),     // Colors::kButtonHovered
    IM_COL32(66, 80, 108, 255),    // Colors::kButtonActive
    IM_COL32(180, 160, 60, 255),   // Colors::kBgWarning
    IM_COL32(160, 60, 60, 255),    // Colors::kBgError
    IM_COL32(60, 160, 60, 255),    // Colors::kBgSuccess
    IM_COL32(62, 74, 96, 255),     // Colors::kStickyNoteYellow
    IM_COL32(47, 214, 220, 120),   // Colors::kLineChartColorAlt
    IM_COL32(255, 0, 0, 64),       // Colors::kTrackWarningBand
    IM_COL32(60, 80, 120, 255),    // Colors::kMinimapBin1
    IM_COL32(60, 0, 80, 255),      // Colors::kMinimapBin2
    IM_COL32(100, 0, 120, 255),    // Colors::kMinimapBin3
    IM_COL32(140, 20, 40, 255),    // Colors::kMinimapBin4
    IM_COL32(200, 50, 0, 255),     // Colors::kMinimapBin5
    IM_COL32(240, 120, 0, 255),    // Colors::kMinimapBin6
    IM_COL32(255, 240, 180, 255),  // Colors::kMinimapBin7
    IM_COL32(80, 80, 80, 255),     // Colors::kMinimapBinCounter1
    IM_COL32(110, 110, 110, 255),  // Colors::kMinimapBinCounter2
    IM_COL32(140, 140, 140, 255),  // Colors::kMinimapBinCounter3
    IM_COL32(170, 170, 170, 255),  // Colors::kMinimapBinCounter4
    IM_COL32(190, 190, 190, 255),  // Colors::kMinimapBinCounter5
    IM_COL32(210, 210, 210, 255),  // Colors::kMinimapBinCounter6
    IM_COL32(230, 230, 230, 255),  // Colors::kMinimapBinCounter7
    IM_COL32(29, 30, 38, 255),     // Colors::kMinimapBg
    IM_COL32(10, 12, 18, 170),     // Colors::kLoadingScreenColor
    IM_COL32(255, 255, 255, 255),  // Colors::kTextOnAccent
    IM_COL32(0, 200, 255, 255),    // Colors::kMeasurementColor
    IM_COL32(30, 30, 30, 240),     // Colors::kMeasurementLabelBg
    IM_COL32(70, 70, 70, 200),     // Colors::kMeasurementLabelEdge
    IM_COL32(255, 255, 255, 255),  // Colors::kMeasurementLabelText
    IM_COL32(255, 255, 255, 120),  // Colors::kMeasurementNotch
    IM_COL32(42, 82, 118, 255),    // Colors::kComparisonBase
    IM_COL32(26, 116, 112, 255),   // Colors::kComparisonTarget
    IM_COL32(205, 170, 82, 255),   // Colors::kComparisonLesser
    IM_COL32(92, 62, 132, 255),    // Colors::kComparisonGreater

    // Centralized from view widgets (kept in Colors enum order):
    IM_COL32(29, 30, 38, 255),     // Colors::kMemChartBg
    IM_COL32(34, 37, 48, 245),     // Colors::kMemChartPanel
    IM_COL32(39, 43, 56, 245),     // Colors::kMemChartPanelAlt
    IM_COL32(62, 116, 168, 220),   // Colors::kMemChartBorder
    IM_COL32(78, 152, 220, 255),   // Colors::kMemChartBorderHot
    IM_COL32(238, 243, 255, 255),  // Colors::kMemChartTextMain
    IM_COL32(145, 156, 174, 255),  // Colors::kMemChartTextDim
    IM_COL32(47, 214, 220, 235),   // Colors::kMemChartRead
    IM_COL32(225, 203, 78, 235),   // Colors::kMemChartWrite
    IM_COL32(184, 139, 226, 235),  // Colors::kMemChartAtomic
    IM_COL32(129, 231, 79, 255),   // Colors::kMemChartUtil
    IM_COL32(231, 196, 65, 255),   // Colors::kMemChartHit
    IM_COL32(235, 82, 98, 255),    // Colors::kMemChartStall
    IM_COL32(0, 0, 0, 85),         // Colors::kMemChartShadow
    IM_COL32(240, 214, 92, 250),   // Colors::kStickyNoteBg
    IM_COL32(193, 154, 40, 235),   // Colors::kStickyNoteBorder
    IM_COL32(232, 200, 78, 252),   // Colors::kStickyNoteHeader
    IM_COL32(0, 0, 0, 110),        // Colors::kStickyNoteShadow
    IM_COL32(48, 40, 12, 255),     // Colors::kStickyNoteText
    IM_COL32(112, 92, 40, 255),    // Colors::kStickyNoteTextMuted
    IM_COL32(176, 130, 24, 235),   // Colors::kStickyNoteAccent
    IM_COL32(176, 130, 24, 255),   // Colors::kStickyNoteResize
    IM_COL32(150, 96, 24, 255),    // Colors::kStickyNoteResizeActive
    IM_COL32(200, 16, 32, 150),    // Colors::kBannerFill
    IM_COL32(255, 255, 255, 40),   // Colors::kBannerBorder
    IM_COL32(255, 255, 255, 255),  // Colors::kBannerText
    IM_COL32(228, 228, 228, 255),  // Colors::kDebugNavBarBg
    IM_COL32(150, 150, 150, 255),  // Colors::kLogTrace
    IM_COL32(150, 180, 210, 255),  // Colors::kLogDebug
    IM_COL32(220, 220, 220, 255),  // Colors::kLogInfo
    IM_COL32(235, 195, 90, 255),   // Colors::kLogWarning
    IM_COL32(235, 110, 110, 255),  // Colors::kLogError
    IM_COL32(255, 80, 80, 255),    // Colors::kLogCritical
    // This must follow the ordering of Colors enum.
};

constexpr std::array LIGHT_THEME_COLORS = {
    IM_COL32(255, 255, 255, 255),  // Colors::kMetaDataColor
    IM_COL32(238, 240, 244, 255),  // Colors::kMetaDataColorSelected
    IM_COL32(228, 231, 236, 255),  // Colors::kMetaDataSeparator
    IM_COL32(0, 0, 0, 0),          // Colors::kTransparent
    IM_COL32(214, 56, 64, 255),    // Colors::kTextError
    IM_COL32(36, 150, 82, 255),    // Colors::kTextSuccess
    IM_COL32(176, 118, 0, 255),    // Colors::kTextWarning
    IM_COL32(88, 132, 245, 225),   // Colors::kFlameChartColor
    IM_COL32(140, 150, 170, 28),   // Colors::kGridColor
    IM_COL32(120, 162, 220, 255),  // Colors::kGridRed
    IM_COL32(54, 132, 214, 255),   // Colors::kSelectionBorder
    IM_COL32(54, 132, 214, 34),    // Colors::kSelection
    IM_COL32(140, 148, 168, 255),  // Colors::kBoundBox
    IM_COL32(247, 248, 250, 255),  // Colors::kFillerColor
    IM_COL32(190, 196, 208, 255),  // Colors::kScrollBarColor
    IM_COL32(56, 124, 244, 32),    // Colors::kHighlightChart
    IM_COL32(255, 255, 255, 255),  // Colors::kRulerBgColor
    IM_COL32(20, 24, 32, 255),     // Colors::kRulerTextColor
    IM_COL32(86, 92, 108, 255),    // Colors::kScrubberNumberColor
    IM_COL32(56, 124, 244, 200),   // Colors::kArrowColor
    IM_COL32(228, 231, 236, 255),  // Colors::kBorderColor
    IM_COL32(228, 232, 240, 255),  // Colors::kSplitterColor
    IM_COL32(247, 248, 250, 255),  // Colors::kBgMain
    IM_COL32(255, 255, 255, 255),  // Colors::kBgPanel
    IM_COL32(244, 246, 250, 255),  // Colors::kBgFrame
    IM_COL32(232, 240, 251, 255),  // Colors::kComboFill
    IM_COL32(54, 132, 214, 255),   // Colors::kAccent
    IM_COL32(88, 164, 232, 255),   // Colors::kAccentHover
    IM_COL32(32, 102, 180, 255),   // Colors::kAccentActive
    IM_COL32(238, 240, 244, 255),  // Colors::kTabAccent
    IM_COL32(244, 246, 250, 255),  // Colors::kTabAccentHover
    IM_COL32(228, 231, 236, 255),  // Colors::kTabAccentActive
    IM_COL32(228, 231, 236, 255),  // Colors::kBorderGray
    IM_COL32(20, 24, 32, 255),     // Colors::kTextMain
    IM_COL32(106, 112, 128, 255),  // Colors::kTextDim
    IM_COL32(247, 248, 250, 255),  // Colors::kScrollBg
    IM_COL32(190, 196, 208, 255),  // Colors::kScrollGrab
    IM_COL32(245, 246, 248, 255),  // Colors::kTableHeaderBg
    IM_COL32(214, 218, 226, 255),  // Colors::kTableBorderStrong
    IM_COL32(232, 235, 240, 255),  // Colors::kTableBorderLight
    IM_COL32(253, 254, 255, 255),  // Colors::kTableRowBg
    IM_COL32(248, 250, 253, 255),  // Colors::kTableRowBgAlt
    IM_COL32(244, 246, 250, 255),  // Colors::kTableBorderInner
    IM_COL32(220, 224, 232, 255),  // Colors::kTableBorderOuter
    IM_COL32(236, 239, 244, 255),  // Colors::kPanelBorderSubtle
    IM_COL32(54, 132, 214, 220),   // Colors::kEventHighlight
    IM_COL32(72, 174, 136, 220),   // Colors::kEventSearchHighlight
    IM_COL32(54, 132, 214, 35),    // Colors::kAreaOfInterest
    IM_COL32(88, 132, 245, 105),   // Colors::kLineChartColor
    IM_COL32(232, 238, 248, 255),  // Colors::kButton
    IM_COL32(222, 230, 242, 255),  // Colors::kButtonHovered
    IM_COL32(212, 222, 238, 255),  // Colors::kButtonActive
    IM_COL32(250, 250, 100, 255),  // Colors::kBgWarning
    IM_COL32(250, 100, 100, 255),  // Colors::kBgError
    IM_COL32(100, 250, 100, 255),  // Colors::kBgSuccess
    IM_COL32(255, 244, 182, 255),  // Colors::kStickyNoteYellow
    IM_COL32(42, 190, 196, 105),   // Colors::kLineChartColorAlt
    IM_COL32(255, 0, 0, 64),       // Colors::kTrackWarningBand
    IM_COL32(180, 200, 220, 255),  // Colors::kMinimapBin1
    IM_COL32(150, 100, 180, 255),  // Colors::kMinimapBin2
    IM_COL32(180, 60, 140, 255),   // Colors::kMinimapBin3
    IM_COL32(220, 80, 80, 255),    // Colors::kMinimapBin4
    IM_COL32(240, 120, 40, 255),   // Colors::kMinimapBin5
    IM_COL32(255, 160, 60, 255),   // Colors::kMinimapBin6
    IM_COL32(255, 200, 120, 255),  // Colors::kMinimapBin7
    IM_COL32(230, 230, 230, 255),  // Colors::kMinimapBinCounter1
    IM_COL32(210, 210, 210, 255),  // Colors::kMinimapBinCounter2
    IM_COL32(190, 190, 190, 255),  // Colors::kMinimapBinCounter3
    IM_COL32(170, 170, 170, 255),  // Colors::kMinimapBinCounter4
    IM_COL32(140, 140, 140, 255),  // Colors::kMinimapBinCounter5
    IM_COL32(110, 110, 110, 255),  // Colors::kMinimapBinCounter6
    IM_COL32(80, 80, 80, 255),     // Colors::kMinimapBinCounter7
    IM_COL32(247, 250, 254, 255),  // Colors::kMinimapBg
    IM_COL32(0, 0, 0, 60),         // Colors::kLoadingScreenColor
    IM_COL32(255, 255, 255, 255),  // Colors::kTextOnAccent
    IM_COL32(0, 160, 220, 255),    // Colors::kMeasurementColor
    IM_COL32(240, 240, 240, 240),  // Colors::kMeasurementLabelBg
    IM_COL32(180, 180, 180, 200),  // Colors::kMeasurementLabelEdge
    IM_COL32(20, 20, 20, 255),     // Colors::kMeasurementLabelText
    IM_COL32(80, 80, 80, 120),     // Colors::kMeasurementNotch
    IM_COL32(203, 230, 252, 255),  // Colors::kComparisonBase
    IM_COL32(194, 235, 230, 255),  // Colors::kComparisonTarget
    IM_COL32(248, 224, 166, 255),  // Colors::kComparisonLesser
    IM_COL32(224, 206, 244, 255),  // Colors::kComparisonGreater

    // Centralized from view widgets (kept in Colors enum order):
    IM_COL32(248, 251, 255, 255),  // Colors::kMemChartBg
    IM_COL32(255, 255, 255, 246),  // Colors::kMemChartPanel
    IM_COL32(241, 247, 255, 246),  // Colors::kMemChartPanelAlt
    IM_COL32(91, 139, 184, 205),   // Colors::kMemChartBorder
    IM_COL32(38, 132, 214, 255),   // Colors::kMemChartBorderHot
    IM_COL32(25, 38, 56, 255),     // Colors::kMemChartTextMain
    IM_COL32(92, 106, 126, 255),   // Colors::kMemChartTextDim
    IM_COL32(0, 132, 155, 235),    // Colors::kMemChartRead
    IM_COL32(168, 128, 0, 235),    // Colors::kMemChartWrite
    IM_COL32(124, 78, 190, 235),   // Colors::kMemChartAtomic
    IM_COL32(58, 145, 26, 255),    // Colors::kMemChartUtil
    IM_COL32(177, 130, 0, 255),    // Colors::kMemChartHit
    IM_COL32(204, 55, 70, 255),    // Colors::kMemChartStall
    IM_COL32(76, 95, 128, 35),     // Colors::kMemChartShadow
    IM_COL32(255, 245, 186, 250),  // Colors::kStickyNoteBg
    IM_COL32(214, 176, 66, 230),   // Colors::kStickyNoteBorder
    IM_COL32(255, 236, 158, 250),  // Colors::kStickyNoteHeader
    IM_COL32(76, 95, 128, 35),     // Colors::kStickyNoteShadow
    IM_COL32(56, 46, 14, 255),     // Colors::kStickyNoteText
    IM_COL32(120, 100, 50, 255),   // Colors::kStickyNoteTextMuted
    IM_COL32(190, 146, 34, 235),   // Colors::kStickyNoteAccent
    IM_COL32(190, 146, 34, 255),   // Colors::kStickyNoteResize
    IM_COL32(150, 100, 24, 255),   // Colors::kStickyNoteResizeActive
    IM_COL32(200, 16, 32, 150),    // Colors::kBannerFill
    IM_COL32(255, 255, 255, 40),   // Colors::kBannerBorder
    IM_COL32(255, 255, 255, 255),  // Colors::kBannerText
    IM_COL32(228, 228, 228, 255),  // Colors::kDebugNavBarBg
    IM_COL32(120, 120, 120, 255),  // Colors::kLogTrace
    IM_COL32(60, 110, 160, 255),   // Colors::kLogDebug
    IM_COL32(40, 40, 40, 255),     // Colors::kLogInfo
    IM_COL32(170, 120, 0, 255),    // Colors::kLogWarning
    IM_COL32(190, 40, 40, 255),    // Colors::kLogError
    IM_COL32(200, 0, 0, 255),      // Colors::kLogCritical
    // This must follow the ordering of Colors enum.
};
// Same hue order as origin/main, desaturated for the redesign.
const std::vector<ImU32> DARK_FLAME_COLORS = {
    IM_COL32(82, 154, 210, 220),  IM_COL32(72, 174, 156, 220),
    IM_COL32(208, 188, 116, 220), IM_COL32(204, 142, 174, 220),
    IM_COL32(118, 184, 220, 220), IM_COL32(212, 148, 102, 220),
    IM_COL32(106, 188, 144, 220), IM_COL32(206, 170, 102, 220),
    IM_COL32(160, 162, 224, 220), IM_COL32(216, 156, 116, 220)
};
const std::vector<ImU32> LIGHT_FLAME_COLORS = {
    IM_COL32(96, 154, 208, 200),  IM_COL32(72, 166, 148, 200),
    IM_COL32(214, 192, 124, 200), IM_COL32(202, 142, 174, 200),
    IM_COL32(118, 178, 218, 200), IM_COL32(214, 148, 102, 200),
    IM_COL32(94, 178, 138, 200),  IM_COL32(212, 168, 102, 200),
    IM_COL32(158, 160, 222, 200), IM_COL32(220, 156, 118, 200)
};
const std::vector<ImU32> DARK_HIGHLIGHTED_EVENT_COLORS = {
    IM_COL32(50, 145, 210, 215),  IM_COL32(0, 158, 115, 215),
    IM_COL32(240, 228, 66, 215),  IM_COL32(204, 121, 167, 215),
    IM_COL32(86, 180, 233, 215),  IM_COL32(235, 130, 45, 215),
    IM_COL32(0, 204, 102, 215),   IM_COL32(230, 159, 0, 215),
    IM_COL32(153, 153, 255, 215), IM_COL32(255, 153, 51, 215)
};
const std::vector<ImU32> LIGHT_HIGHLIGHTED_EVENT_COLORS = {
    IM_COL32(50, 145, 210, 220),  IM_COL32(0, 158, 115, 220),
    IM_COL32(240, 228, 66, 220),  IM_COL32(204, 121, 167, 220),
    IM_COL32(86, 180, 233, 220),  IM_COL32(235, 130, 45, 220),
    IM_COL32(0, 204, 102, 220),   IM_COL32(230, 159, 0, 220),
    IM_COL32(153, 153, 255, 220), IM_COL32(255, 153, 51, 220)
};
inline constexpr const char* FLAME_DARK_COLORMAP_NAME    = "flame_dark";
inline constexpr const char* FLAME_LIGHT_COLORMAP_NAME   = "flame_light";
inline constexpr const char* CONTRAST_DARK_COLORMAP_NAME = "contrast_dark";
inline constexpr const char* CONTRAST_LIGHT_COLORMAP_NAME = "contrast_light";
inline constexpr const char* SETTINGS_FILE_NAME           = "settings_application.json";
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

    ImVec4 bgMain    = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kBgMain));
    ImVec4 bgPanel   = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kBgPanel));
    ImVec4 bgFrame   = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kBgFrame));
    ImVec4 accent = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kAccent));
    ImVec4 accentHover =
        ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kAccentHover));
    ImVec4 accentActive =
        ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kAccentActive));
    ImVec4 tabAccent = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kTabAccent));
    ImVec4 tabAccentHover =
        ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kTabAccentHover));
    ImVec4 tabAccentActive =
        ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kTabAccentActive));
    ImVec4 borderGray = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kBorderGray));
    ImVec4 textMain   = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kTextMain));
    ImVec4 textDim    = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kTextDim));
    ImVec4 scrollBg   = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kScrollBg));
    ImVec4 scrollGrab = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kScrollGrab));
    ImVec4 button     = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kButton));
    ImVec4 buttonHovered =
        ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kButtonHovered));
    ImVec4 buttonActive = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kButtonActive));

    // Window
    style.Colors[ImGuiCol_WindowBg]     = bgMain;
    style.Colors[ImGuiCol_ChildBg]      = bgPanel;
    style.Colors[ImGuiCol_PopupBg]      = bgPanel;
    style.Colors[ImGuiCol_Border]       = borderGray;
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

    // Frame
    style.Colors[ImGuiCol_FrameBg]        = button;
    style.Colors[ImGuiCol_FrameBgHovered] = buttonHovered;
    style.Colors[ImGuiCol_FrameBgActive]  = buttonActive;

    // Title bar
    style.Colors[ImGuiCol_TitleBg]          = bgPanel;
    style.Colors[ImGuiCol_TitleBgActive]    = accent;
    style.Colors[ImGuiCol_TitleBgCollapsed] = borderGray;

    // Menu bar
    style.Colors[ImGuiCol_MenuBarBg] = bgPanel;

    // Table styling
    ImVec4 tableHeaderBg =
        ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kTableHeaderBg));
    ImVec4 tableBorderStrong =
        ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kTableBorderStrong));
    ImVec4 tableBorderLight =
        ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kTableBorderLight));
    ImVec4 tableRowBg = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kTableRowBg));
    ImVec4 tableRowBgAlt =
        ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kTableRowBgAlt));

    style.Colors[ImGuiCol_TableHeaderBg]     = tableHeaderBg;
    style.Colors[ImGuiCol_TableBorderStrong] = tableBorderStrong;
    style.Colors[ImGuiCol_TableBorderLight]  = tableBorderLight;
    style.Colors[ImGuiCol_TableRowBg]        = tableRowBg;
    style.Colors[ImGuiCol_TableRowBgAlt]     = tableRowBgAlt;

    // Scrollbar
    style.Colors[ImGuiCol_ScrollbarBg]          = scrollBg;
    style.Colors[ImGuiCol_ScrollbarGrab]        = scrollGrab;
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = buttonHovered;
    style.Colors[ImGuiCol_ScrollbarGrabActive]  = buttonActive;

    // Checkboxes, radio buttons
    style.Colors[ImGuiCol_CheckMark] = accent;

    // Slider
    style.Colors[ImGuiCol_SliderGrab]       = accent;
    style.Colors[ImGuiCol_SliderGrabActive] = accentActive;

    // Buttons
    style.Colors[ImGuiCol_Button]        = button;
    style.Colors[ImGuiCol_ButtonHovered] = buttonHovered;
    style.Colors[ImGuiCol_ButtonActive]  = buttonActive;

    // Tabs
    style.Colors[ImGuiCol_Tab]                = bgFrame;
    style.Colors[ImGuiCol_TabHovered]         = tabAccentHover;
    style.Colors[ImGuiCol_TabActive]          = tabAccent;
    style.Colors[ImGuiCol_TabUnfocused]       = bgFrame;
    style.Colors[ImGuiCol_TabUnfocusedActive] = tabAccentActive;
    style.Colors[ImGuiCol_TabSelectedOverline] = accent;
    style.Colors[ImGuiCol_TabDimmedSelectedOverline] = accentActive;

    // Headers (collapsing, selectable, etc)
    style.Colors[ImGuiCol_Header]        = tabAccent;
    style.Colors[ImGuiCol_HeaderHovered] = tabAccentHover;
    style.Colors[ImGuiCol_HeaderActive]  = accent;

    // Separator, resize grip
    style.Colors[ImGuiCol_Separator]         = borderGray;
    style.Colors[ImGuiCol_SeparatorHovered]  = accentHover;
    style.Colors[ImGuiCol_SeparatorActive]   = accentActive;
    style.Colors[ImGuiCol_ResizeGrip]        = tabAccent;
    style.Colors[ImGuiCol_ResizeGripHovered] = tabAccentHover;
    style.Colors[ImGuiCol_ResizeGripActive]  = accentActive;

    // Text
    style.Colors[ImGuiCol_Text]         = textMain;
    style.Colors[ImGuiCol_TextDisabled] = textDim;

    // Drag and drop
    style.Colors[ImGuiCol_DragDropTarget] = accent;

    // Navigation highlight
    style.Colors[ImGuiCol_NavHighlight] = accentHover;

    // Plot colors
    style.Colors[ImGuiCol_PlotLines]            = accent;
    style.Colors[ImGuiCol_PlotLinesHovered]     = accentHover;
    style.Colors[ImGuiCol_PlotHistogram]        = accent;
    style.Colors[ImGuiCol_PlotHistogramHovered] = accentHover;

    // Modal window dim
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.7f);
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
        AssistantSettings{} })
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

    // Set sizes and rounding
    style.CellPadding       = ImVec2(12, 8);
    style.FrameBorderSize   = 0.0f;
    style.WindowBorderSize  = 1.0f;
    style.TabBorderSize     = 0.0f;
    style.FrameRounding     = 6.0f;
    style.GrabRounding      = 6.0f;
    style.TabRounding       = 6.0f;
    style.WindowRounding    = 12.0f;
    style.ScrollbarRounding = 8.0f;
    style.ScrollbarSize     = 14.0f;
    style.FramePadding      = ImVec2(12, 6);
    style.ItemSpacing       = ImVec2(10, 8);
    style.ItemInnerSpacing  = ImVec2(8, 6);
    style.WindowPadding     = ImVec2(8, 8);
    style.ChildRounding     = 10.0f;
    style.PopupRounding     = 10.0f;
    style.GrabMinSize       = 12.0f;
    style.IndentSpacing     = 18.0f;

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
