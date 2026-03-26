// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "imgui.h"
#include <utility>

namespace RocProfVis
{
namespace View
{

class SettingsManager;

constexpr float PI = 3.14159265358979323846f;  // Define PI constant

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

bool
IsMouseReleasedWithDragCheck(ImGuiMouseButton button, float drag_threshold = 5.0f);

std::pair<bool, bool>
InputTextWithClear(const char* id, const char* hint, char* buf, size_t buf_size,
                   ImFont* icon_font, ImU32 bg_color, const ImGuiStyle& style,
                   float width = 0);

void
SetTooltipStyled(const char* fmt, ...);

void
BeginTooltipStyled();

bool
BeginItemTooltipStyled();

void
EndTooltipStyled();

enum Alignment
{
    Alignment_Left,
    Alignment_Center,
    Alignment_Right,
};

void
ElidedText(const char* text, float available_width, float tooltip_width = 0.0f,
           Alignment alignment                     = Alignment_Left,
           bool      imgui_AlignTextToFramePadding = false);

class EmbeddedImage
{
public:
    EmbeddedImage(const unsigned char* data, int data_len);
    ~EmbeddedImage();

    EmbeddedImage(const EmbeddedImage&)            = delete;
    EmbeddedImage& operator=(const EmbeddedImage&) = delete;

    bool                 Valid() const;
    int                  GetWidth() const;
    int                  GetHeight() const;
    const unsigned char* GetPixel(int x, int y) const;
    unsigned char*       GetPixels();

    void Render(ImVec2 top_left, float target_width, bool invert_colors = false) const;

private:
    int            m_width  = 0;
    int            m_height = 0;
    unsigned char* m_pixels = nullptr;
};

void
CenterNextTextItem(const char* text);

void
CenterNextItem(float width);

bool
XButton(const char* id = nullptr, const char* tool_tip_label = nullptr,
        SettingsManager* settings = nullptr);

void
SectionTitle(const char* text, bool large = true, SettingsManager* settings = nullptr);

void
VerticalSeparator(SettingsManager* settings = nullptr);

#ifdef ROCPROFVIS_ENABLE_INTERNAL_BANNER
void
DrawInternalBuildBanner(const char* text = "Internal Build");
#endif

}  // namespace View
}  // namespace RocProfVis
