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
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

constexpr float      BASE_FONT_SIZE       = 15.0f;
constexpr float      MIN_USER_FONT_SIZE   = 13.0f;
constexpr float      MAX_USER_FONT_SIZE   = 18.0f;
constexpr std::array FONT_AVAILABLE_SIZES = { 9.0f,  10.0f, 11.0f, 12.0f, 13.0f,
                                              14.0f, 15.0f, 16.0f, 17.0f, 18.0f,
                                              19.0f, 20.0f, 21.0f, 22.0f, 23.0f,
                                              25.0f, 27.0f, 29.0f, 31.0f, 35.0f };

// Step offsets applied to the user-selected base index. Indexed by FontSize:
// kSmall is one step below the base, kMedium is the base (what the user
// picks in Settings), and kMedLarge / kLarge bump up from there.
static constexpr int kSizeOffsets[FontManager::kNumSizes] = {
    -1,  // FontSize::kSmall
     0,  // FontSize::kMedium (== FontSize::kDefault, == user base)
     1,  // FontSize::kMedLarge
     2,  // FontSize::kLarge
};

static_assert(static_cast<int>(FontSize::kSmall)    == 0, "FontSize order changed");
static_assert(static_cast<int>(FontSize::kMedium)   == 1, "FontSize order changed");
static_assert(static_cast<int>(FontSize::kMedLarge) == 2, "FontSize order changed");
static_assert(static_cast<int>(FontSize::kLarge)    == 3, "FontSize order changed");
static_assert(static_cast<int>(FontSize::kDefault)  ==
                  static_cast<int>(FontSize::kMedium),
              "FontSize::kDefault should map to the user's base size");

static int
GetClosestFontSizeIndex(const std::vector<float>& available_sizes, float font_size)
{
    int best_index = 0;

    if(!available_sizes.empty())
    {
        float best_delta = std::abs(available_sizes[0] - font_size);
        for(int i = 1; i < static_cast<int>(available_sizes.size()); ++i)
        {
            float delta = std::abs(available_sizes[i] - font_size);
            if(delta < best_delta)
            {
                best_delta = delta;
                best_index = i;
            }
        }
    }

    return best_index;
}

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
FontManager::GetFontSizeIndex(float font_size) const
{
    return GetClosestFontSizeIndex(m_available_sizes, font_size);
}

int
FontManager::GetDefaultFontSizeIndex() const
{
    return GetClosestFontSizeIndex(m_available_sizes, BASE_FONT_SIZE);
}

int
FontManager::ClampFontSizeIndex(int idx) const
{
    int min_idx = GetFontSizeIndex(MIN_USER_FONT_SIZE);
    int max_idx = GetFontSizeIndex(MAX_USER_FONT_SIZE);
    return std::max(min_idx, std::min(idx, max_idx));
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

    // ImGui's default font and its base size for the next frame. Everything
    // not pushing an explicit font/size renders at FontSize::kDefault, so the
    // user's slider value matches what they see in the UI.
    ImGui::GetIO().FontDefault               = m_text_font;
    ImGui::GetStyle()._NextFrameFontSizeBase = m_sizes[static_cast<int>(FontSize::kDefault)];

    EventManager::GetInstance()->AddEvent(
        std::make_shared<RocEvent>(static_cast<int>(RocEvents::kFontSizeChanged)));
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
