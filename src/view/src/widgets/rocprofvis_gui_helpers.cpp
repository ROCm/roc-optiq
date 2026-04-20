// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_gui_helpers.h"
#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "spdlog/spdlog.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb-image/stb_image.h"
// imgui_internal.h provides ImGui::RegisterUserTexture/UnregisterUserTexture, which are required
// to make a non-font ImTextureData visible to the active backend's per-frame texture loop
// (see imgui.cpp UpdateTexturesEndFrame). The API is marked experimental in 1.92.x but is the
// only sanctioned path for portable user textures, and is honored by both imgui_impl_opengl3
// and imgui_impl_vulkan via ImGuiBackendFlags_RendererHasTextures.
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace RocProfVis
{

namespace View
{

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
    // Hand the GPU-side textures back to ImGui's backend for cleanup. The backend will free
    // its native handle on the next render pass and then transition Status to Destroyed.
    // We deliberately do NOT IM_DELETE the ImTextureData here: doing so before the backend
    // has had a frame to honor WantDestroy would leak the GPU resource. EmbeddedImage instances
    // live for application lifetime, so the small per-instance leak (sizeof(ImTextureData) plus
    // any pixel staging buffer ImGui frees in its dtor) is acceptable and bounded.
    auto release = [](ImTextureData*& tex) {
        if(tex == nullptr) return;
        if(ImGui::GetCurrentContext() != nullptr)
        {
            tex->SetStatus(ImTextureStatus_WantDestroy);
            ImGui::UnregisterUserTexture(tex);
        }
        tex = nullptr;
    };
    release(m_tex_normal);
    release(m_tex_inverted);

    if(m_pixels)
    {
        stbi_image_free(m_pixels);
    }
}

void
EmbeddedImage::EnsureTexturesUploaded() const
{
    if(m_tex_attempted) return;
    if(!Valid()) return;
    if(ImGui::GetCurrentContext() == nullptr) return;  // No context yet; try again next frame.

    m_tex_attempted = true;

    // Build two pixel variants in one pass:
    //   normal:   original RGB, alpha forced to 0 for near-white background pixels
    //   inverted: (255 - R, 255 - G, 255 - B), same alpha mask
    // Memory: 2 * W * H * 4 bytes. For the AMD/ROCm OptIQ logo (1759x1104) this is ~16 MB on
    // the GPU, which is negligible compared to the per-frame draw cost we are eliminating.
    constexpr unsigned char BG_THRESHOLD = 240;
    const int               pixel_count  = m_width * m_height;

    auto make_tex = [&](bool invert) -> ImTextureData* {
        ImTextureData* tex = IM_NEW(ImTextureData)();
        tex->Create(ImTextureFormat_RGBA32, m_width, m_height);
        unsigned char* dst = static_cast<unsigned char*>(tex->GetPixels());
        if(dst == nullptr)
        {
            IM_DELETE(tex);
            return nullptr;
        }
        for(int i = 0; i < pixel_count; ++i)
        {
            const unsigned char* src = m_pixels + i * 4;
            const bool is_bg =
                src[3] == 0 || (src[0] >= BG_THRESHOLD && src[1] >= BG_THRESHOLD &&
                                src[2] >= BG_THRESHOLD);
            if(is_bg)
            {
                dst[i * 4 + 0] = 0;
                dst[i * 4 + 1] = 0;
                dst[i * 4 + 2] = 0;
                dst[i * 4 + 3] = 0;
            }
            else
            {
                dst[i * 4 + 0] = invert ? static_cast<unsigned char>(255 - src[0]) : src[0];
                dst[i * 4 + 1] = invert ? static_cast<unsigned char>(255 - src[1]) : src[1];
                dst[i * 4 + 2] = invert ? static_cast<unsigned char>(255 - src[2]) : src[2];
                dst[i * 4 + 3] = src[3];
            }
        }
        tex->SetStatus(ImTextureStatus_WantCreate);
        ImGui::RegisterUserTexture(tex);
        return tex;
    };

    m_tex_normal   = make_tex(false);
    m_tex_inverted = make_tex(true);

    if(m_tex_normal == nullptr || m_tex_inverted == nullptr)
    {
        spdlog::warn("EmbeddedImage: failed to allocate GPU texture variant ({}x{})",
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
EmbeddedImage::Render(ImVec2 top_left, float target_width, bool invert_colors) const
{
    if(!Valid()) return;

    EnsureTexturesUploaded();

    ImTextureData* tex = invert_colors ? m_tex_inverted : m_tex_normal;
    if(tex == nullptr) return;

    const float  scale  = target_width / static_cast<float>(m_width);
    const ImVec2 bottom_right(top_left.x + target_width,
                              top_left.y + static_cast<float>(m_height) * scale);
    ImGui::GetWindowDrawList()->AddImage(tex->GetTexRef(), top_left, bottom_right);
}

ImVec2
MeasureLoadingIndicatorDots(float dot_radius, int num_dots,
                                              float spacing)
{
    // Calculate total width needed
    float total_width = (num_dots * (dot_radius * 2.0f)) + ((num_dots - 1) * spacing);
    return ImVec2(total_width, dot_radius * 2.0f);
}

void
RenderLoadingIndicatorDots(float dot_radius, int num_dots,
                                             float spacing, ImU32 color, float speed)
{
    // Calculate total width needed
    float  total_width = MeasureLoadingIndicatorDots(dot_radius, num_dots, spacing).x;
    ImVec2 pos         = ImGui::GetCursorScreenPos();
    ImVec2 size(total_width, dot_radius * 2.0f);

    ImGui::Dummy(size);  // Reserve space

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const float t         = (float) ImGui::GetTime();

    for(int i = 0; i < num_dots; ++i)
    {
        float current_dot_x = pos.x + dot_radius + i * (dot_radius * 2.0f + spacing);
        float current_dot_y = pos.y + dot_radius;

        // Offset each dot's animation phase
        float phase = (t * speed) - (i * (2.0f * PI / (float) num_dots) *
                                     0.5f);  // Adjust 0.5f for phase spread

        float alpha_multiplier = (sinf(phase) + 1.0f) * 0.5f;

        // Sharpen the pulse a bit
        alpha_multiplier = std::clamp(alpha_multiplier * 1.5f - 0.25f, 0.0f, 1.0f);

        ImU32        current_color = color;
        unsigned int alpha         = (current_color >> IM_COL32_A_SHIFT) & 0xFF;
        alpha                      = static_cast<unsigned int>(alpha * alpha_multiplier);
        current_color = (current_color & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);

        draw_list->AddCircleFilled(ImVec2(current_dot_x, current_dot_y), dot_radius,
                                   current_color, 12);
    }
}

void
RenderLoadingIndicator(ImU32 color, const char* window_id, float dot_radius, int num_dots,
                       float dot_spacing, float anim_speed)
{
    ImVec2 pos = ImGui::GetCursorPos();

    if(window_id)
    {
        // Create an overlay child window to display the loading indicator if requested
        ImGui::SetCursorPos(ImVec2(0, 0));
        // set transparent background for the overlay window
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImGui::BeginChild(window_id, ImGui::GetWindowSize(), ImGuiChildFlags_None);
    }

    ImVec2 dot_size   = MeasureLoadingIndicatorDots(dot_radius, num_dots, dot_spacing);
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 view_rect  = ImGui::GetWindowSize();
    ImVec2 center_pos = ImVec2(window_pos.x + (view_rect.x - dot_size.x) * 0.5f,
                               window_pos.y + (view_rect.y - dot_size.y) * 0.5f);

    ImGui::SetCursorScreenPos(center_pos);
    RenderLoadingIndicatorDots(dot_radius, num_dots, dot_spacing, color, anim_speed);

    if(window_id)
    {
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    ImGui::SetCursorPos(pos);
}

bool
IconButton(const char* icon, ImFont* icon_font, ImVec2 size,
           const char* tooltip, ImVec2 tooltip_padding, bool frameless,
           ImVec2 frame_padding, ImU32 bg_color, ImU32 bg_color_hover,
           ImU32 bg_color_active, const char* id)
{
    if(id && strlen(id) > 0)
    {
        ImGui::PushID(id);
    }
    else
    {
        ImGui::PushID(icon);
    }
    if(frameless)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(0, 0, 0, 0));
    }
    else
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, frame_padding);
        ImGui::PushStyleColor(ImGuiCol_Button, bg_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bg_color_hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, bg_color_active);
    }
    ImGui::PushFont(icon_font);
    bool clicked = ImGui::Button(icon, size);
    ImGui::PopFont();
    if(tooltip && strlen(tooltip) > 0)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, tooltip_padding);
        if(BeginItemTooltipStyled())
        {
            ImGui::TextUnformatted(tooltip);
            EndTooltipStyled();
        }
        ImGui::PopStyleVar();
    }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    ImGui::PopID();
    return clicked;
}

bool
IsMouseReleasedWithDragCheck(ImGuiMouseButton button, float drag_threshold)
{
    if(ImGui::IsMouseReleased(button))
    {
        ImVec2 drag_delta = ImGui::GetMouseDragDelta(button);
        if((drag_delta.x * drag_delta.x + drag_delta.y * drag_delta.y) <
           (drag_threshold * drag_threshold))
        {
            // this is a click, not a drag
            return true;
        }
    }
    //the user is dragging. 
    return false;
}

std::pair<bool, bool>
InputTextWithClear(const char* id, const char* hint, char* buf,
                                     size_t buf_size, ImFont* icon_font, ImU32 bg_color,
                                     const ImGuiStyle& style, float width)
{
    bool input_cleared = false;
    ImGui::BeginGroup();
    ImGui::SetNextItemAllowOverlap();
    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, bg_color);
    ImGui::SetNextItemWidth(width);
    bool input_changed =
        ImGui::InputTextWithHint("##input_text_with_clear", hint, buf, buf_size);
    ImGui::PopStyleColor();
    if(strlen(buf) > 0)
    {
        ImGui::PushFont(icon_font);
        if(width >= ImGui::CalcTextSize(ICON_X_CIRCLED).x + 2 * style.FramePadding.x)
        {
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetItemRectMax().x - 2 * style.FramePadding.x -
                                 ImGui::CalcTextSize(ICON_X_CIRCLED).x);
            ImGui::PopFont();
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, style.FramePadding);
            input_cleared = IconButton(ICON_X_CIRCLED, icon_font, ImVec2(0, 0), "Clear",
                                       style.WindowPadding, false, style.FramePadding,
                                       bg_color, bg_color, bg_color);
            ImGui::PopStyleVar();
        }
        else
        {
            ImGui::PopFont();
        }
    }
    ImGui::PopID();
    ImGui::EndGroup();
    return std::make_pair(input_changed, input_cleared);
}

void
SetTooltipStyled(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    SettingsManager& settings = SettingsManager::GetInstance();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        settings.GetDefaultStyle().WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,
                        settings.GetDefaultStyle().FrameRounding);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, settings.GetColor(Colors::kBgFrame));
    ImGui::SetTooltipV(fmt, args);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    va_end(args);
}

void
BeginTooltipStyled()
{
    SettingsManager& settings = SettingsManager::GetInstance();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        settings.GetDefaultStyle().WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,
                        settings.GetDefaultStyle().FrameRounding);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, settings.GetColor(Colors::kBgFrame));
    ImGui::BeginTooltip();
}

bool
BeginItemTooltipStyled()
{
    SettingsManager& settings = SettingsManager::GetInstance();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        settings.GetDefaultStyle().WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,
                        settings.GetDefaultStyle().FrameRounding);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, settings.GetColor(Colors::kBgFrame));
    if(ImGui::BeginItemTooltip())
    {
        return true;
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    return false;
}

void
EndTooltipStyled()
{
    ImGui::EndTooltip();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void
ElidedText(const char* text, float available_width, float tooltip_width,
           Alignment alignment, bool imgui_AlignTextToFramePadding)
{
    ImGuiStyle       style      = ImGui::GetStyle();
    SettingsManager& settings   = SettingsManager::GetInstance();
    float            text_width = ImGui::CalcTextSize(text).x;
    ImVec2           elide_size = ImGui::CalcTextSize(" [...]");
    float  scroll_bar_width     = (ImGui::GetScrollMaxY() != 0.0f) ? style.ScrollbarSize : 0.0f;
    bool   elide                = text_width + scroll_bar_width > available_width;
    ImVec2 elide_pos;
    // Dynamically sized containers do not adapt to clip rect...
    // Use a window to restrict our size and provide sizing hint to client.
    // Do not take input to avoid interfering with client.
    ImGui::BeginChild("elided",
                      ImVec2(available_width, imgui_AlignTextToFramePadding
                                                  ? ImGui::GetFrameHeightWithSpacing()
                                                  : ImGui::GetFontSize()),
                      ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);

    if(imgui_AlignTextToFramePadding)
    {
        ImGui::AlignTextToFramePadding();
    }
    if(elide)
    {
        ImGui::PushClipRect(ImGui::GetCursorScreenPos(),
                            ImGui::GetCursorScreenPos() +
                                ImVec2(available_width - scroll_bar_width - elide_size.x,
                                       ImGui::GetFrameHeightWithSpacing()),
                            true);
    }
    else if(alignment == Alignment_Right)
    {
        ImGui::SetCursorPosX(available_width - text_width);
    }
    else if(alignment == Alignment_Center)
    {
        CenterNextTextItem(text);
    }
    ImGui::TextUnformatted(text);
    if(elide)
    {
        ImGui::PopClipRect();
        ImGui::SameLine(available_width - scroll_bar_width - elide_size.x);
        elide_pos = ImGui::GetCursorScreenPos();
        ImGui::TextUnformatted(" [...]");
    }
    ImGui::EndChild();
    if(elide)
    {
        ImGui::SetCursorScreenPos(elide_pos);
        ImGui::InvisibleButton("elide_hover", elide_size);
        if(tooltip_width > 0.0f)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                                settings.GetDefaultIMGUIStyle().WindowPadding);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,
                                settings.GetDefaultStyle().FrameRounding);
            if(ImGui::BeginItemTooltip())
            {
                ImGui::PushTextWrapPos(tooltip_width);
                ImGui::TextWrapped("%s", text);
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
            ImGui::PopStyleVar(2);
        }
    }
}

void
CenterNextTextItem(const char* text)
{
    CenterNextItem(ImGui::CalcTextSize(text).x);
}

void
CenterNextItem(float width)
{
    float cx    = ImGui::GetCursorPosX();
    float avail = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX(cx + (avail - width) * 0.5f);
}

bool
XButton(const char* id, const char * tool_tip_label, SettingsManager* settings)
{
    bool clicked = false;

    if(!settings)
    {
        settings = &SettingsManager::GetInstance();
    }

    ImGui::PushStyleColor(ImGuiCol_Button, settings->GetColor(Colors::kTransparent));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          settings->GetColor(Colors::kTransparent));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          settings->GetColor(Colors::kTransparent));
    ImGui::PushStyleVarX(ImGuiStyleVar_FramePadding, 0);
    ImGui::PushFont(settings->GetFontManager().GetIconFont(FontType::kDefault));
    if(id && strlen(id) > 0)
    {
        ImGui::PushID(id);
    }

    clicked = ImGui::SmallButton(ICON_X_CIRCLED);

    if(id && strlen(id) > 0)
    {
        ImGui::PopID();
    }

    ImGui::PopFont();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    if(tool_tip_label && ImGui::IsItemHovered())
         SetTooltipStyled(tool_tip_label);
    return clicked;
}

void
SectionTitle(const char* text, bool large, SettingsManager* settings)
{
    if(!settings)
    {
        settings = &SettingsManager::GetInstance();
    }

    FontType font_type = large ? FontType::kLarge : FontType::kMedLarge;
    ImGui::PushFont(settings->GetFontManager().GetFont(font_type));
    ImGui::SeparatorText(text);
    ImGui::PopFont();
}


void
VerticalSeparator(SettingsManager* settings)
{
    if(!settings)
    {
        settings = &SettingsManager::GetInstance();
    }
    auto style = settings->GetDefaultStyle();
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(style.ItemSpacing.x, 0));
    ImGui::SameLine();

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float       height    = ImGui::GetFrameHeight();
    ImVec2      p         = ImGui::GetCursorScreenPos();
    draw_list->AddLine(ImVec2(p.x, p.y), ImVec2(p.x, p.y + height),
                       settings->GetColor(Colors::kMetaDataSeparator), 2.0f);
    ImGui::Dummy(ImVec2(style.ItemSpacing.x, 0));
    ImGui::SameLine();
}

#ifdef ROCPROFVIS_ENABLE_INTERNAL_BANNER

void
DrawInternalBuildBanner(const char* text /*= "Internal Build"*/)
{
    if(!text || !*text) return;

    ImDrawList*   dl   = ImGui::GetForegroundDrawList();
    const ImVec2& disp = ImGui::GetIO().DisplaySize;

    // Parameters
    static constexpr float ribbon_thickness = 20.0f;
    static constexpr float min_base_length  = 150.0f;
    static constexpr ImU32 col_fill         = IM_COL32(200, 16, 32, 150);
    static constexpr ImU32 col_border       = IM_COL32(255, 255, 255, 40);
    static constexpr ImU32 col_text         = IM_COL32(255, 255, 255, 255);

    // use precomputed cos/sin for 45 degrees to avoid trig calls
    static constexpr float c_45 = 0.70710678118f;
    static constexpr float s_45 = 0.70710678118f;

    // Measure text first
    ImVec2 ts = ImGui::CalcTextSize(text);

    // Required ribbon length so text fits
    const float desired_length = ts.x * 2.0f;
    const float ribbon_length =
        (desired_length > min_base_length) ? desired_length : min_base_length;

    const float half_len   = ribbon_length * 0.5f;
    const float half_thick = ribbon_thickness * 0.5f;

    // Center a rotated rectangle so it visually emerges from the top-right corner
    ImVec2 center = ImVec2(disp.x - half_len * 0.5f, half_len * 0.5f);

    // Axis‑aligned rect (local space before rotation)
    ImVec2 local[4] = { ImVec2(-half_len, -half_thick), ImVec2(half_len, -half_thick),
                        ImVec2(half_len, half_thick), ImVec2(-half_len, half_thick) };

    ImVec2 quad[4];
    for(int i = 0; i < 4; ++i)
    {
        const ImVec2& p = local[i];
        quad[i].x       = center.x + p.x * c_45 - p.y * s_45;
        quad[i].y       = center.y + p.x * s_45 + p.y * c_45;
    }

    dl->AddConvexPolyFilled(quad, 4, col_fill);
    dl->AddPolyline(quad, 4, col_border, true, 1.0f);

    // Add text at unrotated local position (centered), then rotate vertices
    ImVec2 text_local_pos(-ts.x * 0.5f, -ts.y * 0.5f);
    int    v_start = dl->VtxBuffer.Size;
    dl->AddText(nullptr, ImGui::GetFontSize(),
                ImVec2(center.x + text_local_pos.x, center.y + text_local_pos.y),
                col_text, text);
    int v_end = dl->VtxBuffer.Size;

    for(int i = v_start; i < v_end; ++i)
    {
        ImDrawVert&   v = dl->VtxBuffer[i];
        const ImVec2  p = v.pos - center;
        v.pos.x         = center.x + p.x * c_45 - p.y * s_45;
        v.pos.y         = center.y + p.x * s_45 + p.y * c_45;
    }
}
#endif // ROCPROFVIS_ENABLE_INTERNAL_BANNER

}   // namespace View
}   // namespace RocProfVis
