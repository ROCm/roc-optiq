// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_image_helpers.h"
#include "spdlog/spdlog.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb-image/stb_image.h"
#include <utility>

namespace RocProfVis
{
namespace View
{

CreateGuiTextureRGBA32Fn GuiTexture::s_create_texture    = nullptr;
DestroyGuiTextureFn      GuiTexture::s_destroy_texture   = nullptr;
void*                    GuiTexture::s_texture_user_data = nullptr;

void
GuiTexture::SetBackend(CreateGuiTextureRGBA32Fn create_texture,
                       DestroyGuiTextureFn      destroy_texture,
                       void*                    user_data)
{
    s_create_texture    = create_texture;
    s_destroy_texture   = destroy_texture;
    s_texture_user_data = user_data;
}

GuiTexture::~GuiTexture()
{
    Destroy();
}

GuiTexture::GuiTexture(GuiTexture&& other) noexcept
: m_texture_id(other.m_texture_id)
, m_width(other.m_width)
, m_height(other.m_height)
{
    other.m_texture_id = ImTextureID_Invalid;
    other.m_width      = 0;
    other.m_height     = 0;
}

GuiTexture&
GuiTexture::operator=(GuiTexture&& other) noexcept
{
    if(this != &other)
    {
        Destroy();
        m_texture_id       = other.m_texture_id;
        m_width            = other.m_width;
        m_height           = other.m_height;
        other.m_texture_id = ImTextureID_Invalid;
        other.m_width      = 0;
        other.m_height     = 0;
    }
    return *this;
}

bool
GuiTexture::CreateRGBA32(const unsigned char* pixels, int width, int height)
{
    Destroy();
    if(!pixels || width <= 0 || height <= 0 || !s_create_texture)
    {
        return false;
    }

    m_texture_id = s_create_texture(s_texture_user_data, pixels, width, height);
    if(m_texture_id == ImTextureID_Invalid)
    {
        return false;
    }

    m_width  = width;
    m_height = height;
    return true;
}

void
GuiTexture::Destroy()
{
    if(m_texture_id != ImTextureID_Invalid)
    {
        if(s_destroy_texture)
        {
            s_destroy_texture(s_texture_user_data, m_texture_id);
        }
        m_texture_id = ImTextureID_Invalid;
        m_width      = 0;
        m_height     = 0;
    }
}

bool
GuiTexture::Valid() const
{
    return m_texture_id != ImTextureID_Invalid && m_width > 0 && m_height > 0;
}

ImTextureID
GuiTexture::GetTextureID() const
{
    return m_texture_id;
}

int
GuiTexture::GetWidth() const
{
    return m_width;
}

int
GuiTexture::GetHeight() const
{
    return m_height;
}

void
GuiTexture::Render(ImVec2 top_left, float target_width, ImU32 tint) const
{
    if(!Valid()) return;

    const float target_height =
        target_width * static_cast<float>(m_height) / static_cast<float>(m_width);
    const ImVec2 bottom_right(top_left.x + target_width, top_left.y + target_height);
    ImGui::GetWindowDrawList()->AddImage(m_texture_id, top_left, bottom_right,
                                         ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                                         tint);
}

void
GuiTexture::RenderCover(ImVec2 top_left, ImVec2 size, ImU32 tint) const
{
    if(!Valid() || size.x <= 0.0f || size.y <= 0.0f) return;

    ImVec2 uv0(0.0f, 0.0f);
    ImVec2 uv1(1.0f, 1.0f);
    const float image_aspect  = static_cast<float>(m_width) / static_cast<float>(m_height);
    const float target_aspect = size.x / size.y;
    if(target_aspect > image_aspect)
    {
        const float visible_v = image_aspect / target_aspect;
        uv0.y                 = (1.0f - visible_v) * 0.5f;
        uv1.y                 = 1.0f - uv0.y;
    }
    else
    {
        const float visible_u = target_aspect / image_aspect;
        uv0.x                 = (1.0f - visible_u) * 0.5f;
        uv1.x                 = 1.0f - uv0.x;
    }

    ImGui::GetWindowDrawList()->AddImage(m_texture_id, top_left,
                                         ImVec2(top_left.x + size.x,
                                                top_left.y + size.y),
                                         uv0, uv1, tint);
}

EmbeddedImage::EmbeddedImage(const unsigned char* data, int data_len)
{
    int channels = 0;
    m_pixels =
        stbi_load_from_memory(data, data_len, &m_width, &m_height, &channels, STBI_rgb_alpha);
    if(!Valid())
    {
        spdlog::warn("EmbeddedImage: failed to load image ({} bytes): {}", data_len,
                     stbi_failure_reason());
    }
}

EmbeddedImage::~EmbeddedImage()
{
    if(m_pixels)
    {
        stbi_image_free(m_pixels);
    }
}

EmbeddedImage::EmbeddedImage(EmbeddedImage&& other) noexcept
: m_width(other.m_width)
, m_height(other.m_height)
, m_pixels(other.m_pixels)
, m_texture(std::move(other.m_texture))
, m_create_texture_attempted(other.m_create_texture_attempted)
{
    other.m_width                    = 0;
    other.m_height                   = 0;
    other.m_pixels                   = nullptr;
    other.m_create_texture_attempted = false;
}

EmbeddedImage&
EmbeddedImage::operator=(EmbeddedImage&& other) noexcept
{
    if(this != &other)
    {
        m_texture.Destroy();
        if(m_pixels)
        {
            stbi_image_free(m_pixels);
        }

        m_width                    = other.m_width;
        m_height                   = other.m_height;
        m_pixels                   = other.m_pixels;
        m_texture                  = std::move(other.m_texture);
        m_create_texture_attempted = other.m_create_texture_attempted;

        other.m_width                    = 0;
        other.m_height                   = 0;
        other.m_pixels                   = nullptr;
        other.m_create_texture_attempted = false;
    }
    return *this;
}

void
EmbeddedImage::EnsureTextureCreated() const
{
    if(m_create_texture_attempted) return;
    if(!Valid()) return;

    m_create_texture_attempted = true;
    if(!m_texture.CreateRGBA32(m_pixels, m_width, m_height))
    {
        spdlog::warn("EmbeddedImage: failed to create renderer texture ({}x{})",
                     m_width, m_height);
    }
}

bool
EmbeddedImage::Valid() const
{
    return m_pixels != nullptr && m_width > 0 && m_height > 0;
}

int
EmbeddedImage::GetWidth() const
{
    return m_width;
}

int
EmbeddedImage::GetHeight() const
{
    return m_height;
}

unsigned char*
EmbeddedImage::GetPixels()
{
    return m_pixels;
}

const unsigned char*
EmbeddedImage::GetPixel(int x, int y) const
{
    if(!Valid() || x < 0 || x >= m_width || y < 0 || y >= m_height)
        return nullptr;
    return m_pixels + 4 * (y * m_width + x);
}

void
EmbeddedImage::Render(ImVec2 top_left, float target_width, ImU32 tint) const
{
    if(!Valid()) return;

    EnsureTextureCreated();
    m_texture.Render(top_left, target_width, tint);
}

void
EmbeddedImage::RenderCover(ImVec2 top_left, ImVec2 size, ImU32 tint) const
{
    if(!Valid()) return;

    EnsureTextureCreated();
    m_texture.RenderCover(top_left, size, tint);
}

}  // namespace View
}  // namespace RocProfVis
