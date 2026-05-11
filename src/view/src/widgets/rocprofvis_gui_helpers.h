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
enum class Colors;

constexpr float PI = 3.14159265358979323846f;  // Define PI constant

void
RenderLoadingIndicatorDots(float dot_radius, int num_dots, float spacing,
                           ImU32 color, float speed);
ImVec2
MeasureLoadingIndicatorDots(float dot_radius, int num_dots, float spacing);

enum LoadingIndicatorCentering
{
    kCenterNone,
    kCenterHorizontal,
    kCenterVertical,
    kCenterBoth,
};

/**
 * @brief Render a loading indicator with animated dots.
 *
 * @param color The color of the dots (including alpha for transparency).
 * @param window_id Optional ID for an overlay child window to render the indicator in.
 * If null, the indicator will be rendered in the current window. The overlay window will
 * be sized to fill the parent window.
 * @param centering How to center the indicator within the window (horizontal, vertical,
 * both, or none).
 * @param dot_radius The radius of each dot in the indicator.
 * @param num_dots The number of dots in the indicator.
 * @param dot_spacing The spacing between each dot.
 * @param anim_speed The speed of the dot animation.
 */
void
RenderLoadingIndicator(ImU32 color, const char* window_id = nullptr,
                       LoadingIndicatorCentering centering = kCenterBoth,
                       float dot_radius = 5.0f, int num_dots = 3,
                       float dot_spacing = 5.0f, float anim_speed = 5.0f);

ImU32
ApplyAlpha(ImU32 color, float alpha);

ImVec4
ThemeColor(SettingsManager& settings, Colors color, float alpha = 1.0f);

ImVec2
GetResponsiveWindowSize(ImVec2 desired_size, ImVec2 min_size = ImVec2(0.0f, 0.0f),
                        float viewport_margin = 32.0f);

// Push combo frame fill so dropdowns stand out from text inputs. Pair with
// PopComboStyles.
void
PushComboStyles();

void
PopComboStyles();

bool
IconButton(const char* icon, ImFont* icon_font, ImVec2 size = ImVec2(0, 0),
           const char* tooltip = nullptr, bool frameless = true,
           ImVec2 frame_padding = ImVec2(0, 0), ImU32 bg_color = IM_COL32(0, 0, 0, 0),
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

float
TableRowHeight();

#ifdef ROCPROFVIS_ENABLE_INTERNAL_BANNER
void
DrawInternalBuildBanner(const char* text = "Internal Build");
#endif

}  // namespace View
}  // namespace RocProfVis
