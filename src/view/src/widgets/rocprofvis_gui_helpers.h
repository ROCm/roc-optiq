// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "imgui.h"
#include <utility>

namespace RocProfVis
{
namespace View
{

void
RenderLoadingIndicatorDots(float dot_radius, int num_dots, float spacing,
                           ImU32 color, float speed);
ImVec2
MeasureLoadingIndicatorDots(float dot_radius, int num_dots, float spacing);

bool
IconButton(const char* icon, ImFont* icon_font, ImVec2 size = ImVec2(0, 0),
           const char* tooltip = nullptr, ImVec2 tooltip_padding = ImVec2(0, 0),
           bool frameless = true, ImVec2 frame_padding = ImVec2(0, 0),
           ImU32 bg_color        = IM_COL32(0, 0, 0, 0),
           ImU32 bg_color_hover  = IM_COL32(0, 0, 0, 0),
           ImU32 bg_color_active = IM_COL32(0, 0, 0, 0), const char* id = nullptr);

std::pair<bool, bool>
InputTextWithClear(const char* id, const char* hint, char* buf, size_t buf_size,
                   ImFont* icon_font, ImU32 bg_color, const ImGuiStyle& style,
                   float width = 0);

#ifdef ROCPROFVIS_ENABLE_INTERNAL_BANNER
void
DrawInternalBuildBanner(const char* text = "Internal Build");
#endif

}  // namespace View
}  // namespace RocProfVis
