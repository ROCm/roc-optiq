// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "imgui.h"

namespace RocProfVis
{
namespace View
{

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

}  // namespace View
}  // namespace RocProfVis
