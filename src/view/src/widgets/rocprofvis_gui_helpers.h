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

void
RenderLoadingIndicator(ImU32 color, const char* window_id = nullptr,
                       float dot_radius = 5.0f, int num_dots = 3,
                       float dot_spacing = 5.0f, float anim_speed = 5.0f);

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

using CreateGuiTextureRGBA32Fn = ImTextureID (*)(void* user_data,
                                                 const unsigned char* pixels,
                                                 int width,
                                                 int height);
using DestroyGuiTextureFn      = void (*)(void* user_data, ImTextureID texture_id);

// Owns a renderer texture that can be created and destroyed at any time while the
// app backend is active.
class GuiTexture
{
public:
    GuiTexture() = default;
    ~GuiTexture();

    GuiTexture(const GuiTexture&)            = delete;
    GuiTexture& operator=(const GuiTexture&) = delete;
    GuiTexture(GuiTexture&& other) noexcept;
    GuiTexture& operator=(GuiTexture&& other) noexcept;

    static void SetBackend(CreateGuiTextureRGBA32Fn create_texture,
                           DestroyGuiTextureFn      destroy_texture,
                           void*                    user_data);

    bool CreateRGBA32(const unsigned char* pixels, int width, int height);
    void Destroy();

    bool        Valid() const;
    ImTextureID GetTextureID() const;
    int         GetWidth() const;
    int         GetHeight() const;

    void Render(ImVec2 top_left, float target_width) const;

private:
    // Renderer-provided callback used to allocate an RGBA8 texture for ImGui draw calls.
    static CreateGuiTextureRGBA32Fn s_create_texture;
    // Renderer-provided callback used to release textures created by s_create_texture.
    static DestroyGuiTextureFn s_destroy_texture;
    // Opaque owner/context pointer passed back into the renderer texture callbacks.
    static void* s_texture_user_data;

    ImTextureID m_texture_id = ImTextureID_Invalid;
    int         m_width      = 0;
    int         m_height     = 0;
};

// Decodes an in-memory image (PNG/JPG/etc. via stb_image) into RGBA8 pixels.
// The decoded CPU pixels are retained for callers that need them outside of ImGui
// (e.g. to feed glfwSetWindowIcon). Render() lazily creates a GuiTexture.
class EmbeddedImage
{
public:
    EmbeddedImage(const unsigned char* data, int data_len);
    ~EmbeddedImage();

    EmbeddedImage(const EmbeddedImage&)            = delete;
    EmbeddedImage& operator=(const EmbeddedImage&) = delete;
    EmbeddedImage(EmbeddedImage&& other) noexcept;
    EmbeddedImage& operator=(EmbeddedImage&& other) noexcept;

    bool                 Valid() const;
    int                  GetWidth() const;
    int                  GetHeight() const;
    const unsigned char* GetPixel(int x, int y) const;
    unsigned char*       GetPixels();

    void Render(ImVec2 top_left, float target_width) const;

private:
    // Lazy GPU upload on first Render(). Prevents repeated backend calls after failure.
    void EnsureTextureCreated() const;

    int                m_width  = 0;
    int                m_height = 0;
    unsigned char*     m_pixels = nullptr;
    mutable GuiTexture m_texture;
    mutable bool       m_create_texture_attempted = false;
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
