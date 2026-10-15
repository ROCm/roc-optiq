// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

namespace RocProfVis
{
namespace View
{
constexpr ImWchar icon_ranges[] = {
    0xE03E, 0xE03E,
    0xE06C, 0xE06C,
    0xE08E, 0xE08E,
    0xE094, 0xE094,
    0xE0EA, 0xE0EA,
    0xE136, 0xE136,
    0xE138, 0xE138,
    0xE13A, 0xE13A,
    0xE150, 0xE150,
    0xE158, 0xE158,
    0xE1C8, 0xE1C8,
    0xE1CA, 0xE1CA,
    0xE1D4, 0xE1D4,
    0xE1D6, 0xE1D6,
    0xE220, 0xE220,
    0xE224, 0xE224,
    0xE230, 0xE230,
    0xE23A, 0xE23A,
    0xE248, 0xE248,
    0xE24A, 0xE24A,
    0xE256, 0xE256,
    0xE266, 0xE266,
    0xE270, 0xE270,
    0xE280, 0xE280,
    0xE2C2, 0xE2C2,
    0xE2E2, 0xE2E2,
    0xE2F0, 0xE2F0,
    0xE2FA, 0xE2FA,
    0xE306, 0xE306,
    0xE310, 0xE310,
    0xE31A, 0xE31A,
    0xE348, 0xE348,
    0xE34C, 0xE34C,
    0xE3B4, 0xE3B4,
    0xE3D4, 0xE3D4,
    0xE3E2, 0xE3E2,
    0xE434, 0xE434,
    0xE4A6, 0xE4A6,
    0xE4F6, 0xE4F6,
    0xE4F8, 0xE4F8,
    0xE5DC, 0xE5DC,
    0xE5DE, 0xE5DE,
    0xE67C, 0xE67C,
    0xE69A, 0xE69A,
    0xE6A2, 0xE6A2,
    0xE6B8, 0xE6B8,
    0xEAE2, 0xEAE2,
    0xEAE8, 0xEAE8,
    0xEAF0, 0xEAF0,
    0xEB5A, 0xEB5A,
    0
};

inline constexpr const char* ICON_CHEVRON_DOWN  = u8"\uE136";
inline constexpr const char* ICON_CHEVRON_LEFT  = u8"\uE138";
inline constexpr const char* ICON_CHEVRON_RIGHT = u8"\uE13A";
inline constexpr const char* ICON_X_CIRCLED     = u8"\uE4F8";
inline constexpr const char* ICON_DOCUMENT      = u8"\uE23A";
inline constexpr const char* ICON_FOLDER        = u8"\uE24A";
inline constexpr const char* ICON_HOME          = u8"\uE2C2";
inline constexpr const char* ICON_LOCKED        = u8"\uE2FA";
inline constexpr const char* ICON_UNLOCKED      = u8"\uE306";
inline constexpr const char* ICON_EYE           = u8"\uE220";
inline constexpr const char* ICON_GEAR          = u8"\uE270";
inline constexpr const char* ICON_CHAIN         = u8"\uE2E2";
inline constexpr const char* ICON_ELLIPSIS      = u8"\uE434";
inline constexpr const char* ICON_ADD_NOTE      = u8"\uE34C";
inline constexpr const char* ICON_ARROWS_EXPAND = u8"\uE310";
inline constexpr const char* ICON_ARROWS_SHRINK = u8"\uE1D6";
inline constexpr const char* ICON_COMPASS       = u8"\uE1C8";
inline constexpr const char* ICON_CHART_BAR     = u8"\uE150";
inline constexpr const char* ICON_ARCHIVE       = u8"\uE248";
inline constexpr const char* ICON_DELETE        = u8"\uE4F6";
inline constexpr const char* ICON_EYE_SLASH     = u8"\uE224";
inline constexpr const char* ICON_TREE          = u8"\uE67C";
inline constexpr const char* ICON_GRID          = u8"\uEAE2";
inline constexpr const char* ICON_ARROW_DOWN    = u8"\uE03E";
inline constexpr const char* ICON_ARROW_UP      = u8"\uE08E";
inline constexpr const char* ICON_EDIT          = u8"\uE3B4";
inline constexpr const char* ICON_TRASH_CAN     = u8"\uE4A6";
inline constexpr const char* ICON_ARROW_IN_BOX  = u8"\uE5DC";
inline constexpr const char* ICON_EXPAND        = u8"\uE69A";
inline constexpr const char* ICON_FUNNEL        = u8"\uE266";
inline constexpr const char* ICON_OPEN          = u8"\uE256";
inline constexpr const char* ICON_ARROWS_CYCLE  = u8"\uE094";
inline constexpr const char* ICON_EYE_THIN      = u8"\uE224";
inline constexpr const char* ICON_LIST          = u8"\uE2F0";
inline constexpr const char* ICON_STICKY_NOTE   = u8"\uE348";
inline constexpr const char* ICON_CHART_PIE     = u8"\uE158";
inline constexpr const char* ICON_COPY          = u8"\uE1CA";
inline constexpr const char* ICON_CROP          = u8"\uE1D4";
inline constexpr const char* ICON_ARROW_FORWARD = u8"\uE06C";

inline constexpr const char* ICON_PLUS            = u8"\uE3D4";
inline constexpr const char* ICON_BOOKMARK_ADD    = u8"\uE0EA";
inline constexpr const char* ICON_FLOW_FAN        = u8"\uE280";
inline constexpr const char* ICON_PIN             = u8"\uE3E2";
inline constexpr const char* ICON_PLUGS_CONNECTED = u8"\uEB5A";
inline constexpr const char* ICON_OPEN_FILE      = u8"\uE5DE";
inline constexpr const char* ICON_TERMINAL       = u8"\uEAE8";
inline constexpr const char* ICON_SAVE           = u8"\uE248";
inline constexpr const char* ICON_EXPORT         = u8"\uEAF0";
inline constexpr const char* ICON_FILE           = u8"\uE230";
inline constexpr const char* ICON_MEASURE        = u8"\uE6B8";
inline constexpr const char* ICON_MINIMAP        = u8"\uE31A";
inline constexpr const char* ICON_SPARKLE        = u8"\uE6A2";

}  // namespace View
}  // namespace RocProfVis
