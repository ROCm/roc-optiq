// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_font_manager.h"
#include "icons/rocprofvis_icon_data.h"
#include "icons/rocprovfis_icon_defines.h"
#include "imgui.h"
#include "rocprofvis_event_manager.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iterator>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

constexpr float      BASE_FONT_SIZE       = 15.0f;
constexpr float      MIN_USER_FONT_SIZE   = 12.0f;
constexpr float      MAX_USER_FONT_SIZE   = 20.0f;
constexpr std::array FONT_AVAILABLE_SIZES = { 9.0f,  10.0f, 11.0f, 12.0f, 13.0f,
                                              14.0f, 15.0f, 16.0f, 17.0f, 18.0f,
                                              19.0f, 20.0f, 21.0f, 22.0f, 23.0f,
                                              25.0f, 27.0f, 29.0f, 31.0f, 35.0f };

// Offsets applied to the user-selected base index (kMedium) to produce
// kXSmall/kSmall/kMedium/kMedLarge/kLarge.
static constexpr int kSizeOffsets[FontManager::kNumSizes] = { -5, -1, 0, 1, 2 };

FontManager::FontManager() {}

FontManager::~FontManager() {}

float
FontManager::GetMinUserFontSize() const
{
    return MIN_USER_FONT_SIZE;
}

float
FontManager::GetMaxUserFontSize() const
{
    return MAX_USER_FONT_SIZE;
}

float
FontManager::GetFontSizeAt(int idx) const
{
    if(m_available_sizes.empty())
        return BASE_FONT_SIZE;
    idx = std::max(0, std::min(idx, static_cast<int>(m_available_sizes.size()) - 1));
    return m_available_sizes[idx];
}

int
FontManager::GetClosestFontSizeIndex(float font_size) const
{
    if(m_available_sizes.empty())
        return 0;

    auto it = std::lower_bound(m_available_sizes.begin(), m_available_sizes.end(), font_size);
    if(it == m_available_sizes.begin())
        return 0;
    if(it == m_available_sizes.end())
        return static_cast<int>(m_available_sizes.size()) - 1;

    // Snap to whichever of the two neighbours is closer.
    auto prev = std::prev(it);
    if((font_size - *prev) <= (*it - font_size))
        return static_cast<int>(std::distance(m_available_sizes.begin(), prev));
    return static_cast<int>(std::distance(m_available_sizes.begin(), it));
}

int
FontManager::GetFontSizeIndex(float font_size) const
{
    return GetClosestFontSizeIndex(font_size);
}

int
FontManager::GetDefaultFontSizeIndex() const
{
    return GetClosestFontSizeIndex(BASE_FONT_SIZE);
}

int
FontManager::ClampFontSizeIndex(int idx) const
{
    return std::clamp(idx, GetFontSizeIndex(MIN_USER_FONT_SIZE),
                      GetFontSizeIndex(MAX_USER_FONT_SIZE));
}

void
FontManager::SetFontSize(int idx)
{
    if(m_available_sizes.empty())
        return;
    idx = ClampFontSizeIndex(idx);

    for(int i = 0; i < kNumSizes; ++i)
    {
        int size_idx = std::max(0, std::min(idx + kSizeOffsets[i],
                                            static_cast<int>(m_available_sizes.size()) - 1));
        m_sizes[i] = m_available_sizes[size_idx];
    }

    // Set the default font and its base size for the next frame. The
    // kFontSizeChanged event is fired from Update() once the new size takes
    // effect, so subscribers recalculate against the applied font size.
    ImGui::GetIO().FontDefault               = m_text_font;
    ImGui::GetStyle()._NextFrameFontSizeBase = m_sizes[static_cast<int>(FontSize::kDefault)];
}

void
FontManager::Update()
{
    // Both user-selected sizes (applied next frame via _NextFrameFontSizeBase)
    // and ImGui's automatic DPI scaling change the font size without notifying
    // anyone. Detect the effective change here and fire a single event so every
    // subscriber stays in sync.
    const float font_size = ImGui::GetFontSize();
    if(std::abs(font_size - m_last_font_size) > 0.01f)
    {
        m_last_font_size = font_size;
        EventManager::GetInstance()->AddEvent(
            std::make_shared<RocEvent>(static_cast<int>(RocEvents::kFontSizeChanged)));
    }
}

bool
FontManager::Init()
{
    ImGuiIO& io = ImGui::GetIO();

#ifdef _WIN32
    const char* font_paths[] = {
        "C:\\Windows\\Fonts\\SegUIVar.ttf",
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "C:\\Windows\\Fonts\\segoeuib.ttf",
        "C:\\Windows\\Fonts\\arial.ttf"
    };
#elif __APPLE__
    const char* font_paths[] = {
        "/System/Library/Fonts/SFNS.ttf",
        "/System/Library/Fonts/SFNSDisplay.ttf",
        "/System/Library/Fonts/SFNSRounded.ttf",
        "/System/Library/Fonts/HelveticaNeue.ttc",
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Supplemental/Verdana.ttf",
        "/Library/Fonts/Arial.ttf",
        "/Library/Fonts/Microsoft/Arial.ttf"
    };
#else
    const char* font_paths[] = {
        "/usr/share/fonts/truetype/inter/Inter-Regular.ttf",
        "/usr/share/fonts/opentype/inter/Inter-Regular.otf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/opentype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/google-noto/NotoSans-Regular.ttf",
        // Ubuntu / Debian
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf",
        // RedHat 8, Oracle 8
        "/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        // RedHat 9 / 10, Oracle 9 / 10
        "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf"
    };
#endif

    const char* font_path = nullptr;
    for(const char* path : font_paths)
    {
        if(std::filesystem::exists(path))
        {
            font_path = path;
            break;
        }
    }

    m_available_sizes.assign(FONT_AVAILABLE_SIZES.begin(), FONT_AVAILABLE_SIZES.end());

    if(font_path)
    {
        m_text_font = io.Fonts->AddFontFromFileTTF(font_path, 0.0f);
    }
    else
    {
        ImFontConfig fallback_config;
        fallback_config.SizePixels = BASE_FONT_SIZE;
        m_text_font                = io.Fonts->AddFontDefault(&fallback_config);
    }

#ifdef _WIN32
    const char* code_font_paths[] = {
        "C:\\Windows\\Fonts\\CascadiaCode.ttf",
        "C:\\Windows\\Fonts\\CascadiaMono.ttf",
        "C:\\Windows\\Fonts\\consola.ttf",
        "C:\\Windows\\Fonts\\cour.ttf"
    };
#elif __APPLE__
    const char* code_font_paths[] = {
        "/System/Library/Fonts/SFMono-Regular.otf",
        "/Library/Fonts/Menlo.ttc",
        "/System/Library/Fonts/Menlo.ttc",
        "/Library/Fonts/Courier New.ttf"
    };
#else
    const char* code_font_paths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/dejavu-sans-mono-fonts/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/liberation-mono/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSansMono-Regular.ttf",
        "/usr/share/fonts/google-noto/NotoSansMono-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeMono.ttf"
    };
#endif

    const char* code_font_path = nullptr;
    for(const char* path : code_font_paths)
    {
        if(std::filesystem::exists(path))
        {
            code_font_path = path;
            break;
        }
    }

    if(code_font_path)
    {
        m_code_font = io.Fonts->AddFontFromFileTTF(code_font_path, 0.0f);
    }
    else
    {
        ImFontConfig mono_fallback_config;
        mono_fallback_config.SizePixels = BASE_FONT_SIZE;
        m_code_font                     = io.Fonts->AddFontDefault(&mono_fallback_config);
    }

    ImFontConfig icon_config;
    icon_config.FontDataOwnedByAtlas = false;
    m_icon_font = io.Fonts->AddFontFromMemoryCompressedTTF(
        &icon_font_compressed_data, icon_font_compressed_size, 0.0f, &icon_config, icon_ranges);

    SetFontSize(GetDefaultFontSizeIndex());

    // Don't call Build() - ImGui 1.92+ backend handles font atlas building automatically.
    return true;
}

ImFont*
FontManager::GetFont(FontType font_type)
{
    switch(font_type)
    {
        case FontType::kMainText:
            return m_text_font;
        case FontType::kIcon:
            return m_icon_font;
        case FontType::kCode: 
            return m_code_font;
        default:
            return m_text_font;
    }
}

float
FontManager::GetFontSize(FontSize font_size) const
{
    int idx = static_cast<int>(font_size);
    if(idx < 0 || idx >= kNumSizes)
        return BASE_FONT_SIZE;
    return m_sizes[idx];
}

}  // namespace View
}  // namespace RocProfVis
