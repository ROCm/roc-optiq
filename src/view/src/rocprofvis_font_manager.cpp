// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_font_manager.h"
#include "icons/rocprofvis_icon_data.h"
#include "icons/rocprovfis_icon_defines.h"
#include "imgui.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_settings_manager.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

constexpr float      BASE_FONT_SIZE       = 13.0f;
constexpr std::array FONT_AVAILABLE_SIZES = { 7.0f,  8.0f,  9.0f,  10.0f, 11.0f,
                                              12.0f, 13.0f, 14.0f, 15.0f, 16.0f,
                                              17.0f, 18.0f, 19.0f, 20.0f, 21.0f,
                                              23.0f, 25.0f, 27.0f, 29.0f, 33.0f };

// Size offsets applied to the base index to produce kSmall/kMedium/kMedLarge/kLarge.
static constexpr int kSizeOffsets[FontManager::kNumSizes] = { -1, 0, 1, 2 };

FontManager::FontManager() {}

FontManager::~FontManager() {}

int
FontManager::GetDPIScaledFontIndex()
{
    constexpr float DPI_EXPONENT =
        0.75f;  // Adjust as needed. Higher values increase size more rapidly.

    float scaled_size =
        BASE_FONT_SIZE * std::pow(SettingsManager::GetInstance().GetDPI(), DPI_EXPONENT);

    // Find the index of the available size closest to scaled_size.
    int best_index = 0;
    for(int i = 1; i < static_cast<int>(m_available_sizes.size()); ++i)
    {
        if(std::abs(m_available_sizes[i] - scaled_size) <
           std::abs(m_available_sizes[i - 1] - scaled_size))
            best_index = i;
    }
    return best_index;
}

void
FontManager::SetFontSize(int idx)
{
    if(m_available_sizes.empty())
        return;
    idx = std::max(0, std::min(idx, static_cast<int>(m_available_sizes.size()) - 1));

    for(int i = 0; i < kNumSizes; ++i)
    {
        int size_idx = std::max(0, std::min(idx + kSizeOffsets[i],
                                            static_cast<int>(m_available_sizes.size()) - 1));
        m_sizes[i] = m_available_sizes[size_idx];
    }

    // Set the default font and its base size for the next frame.
    ImGui::GetIO().FontDefault                   = m_text_font;
    ImGui::GetStyle()._NextFrameFontSizeBase     = m_sizes[static_cast<int>(FontType::kDefault)];

    EventManager::GetInstance()->AddEvent(
        std::make_shared<RocEvent>(static_cast<int>(RocEvents::kFontSizeChanged)));
}

bool
FontManager::Init()
{
    ImGuiIO& io = ImGui::GetIO();

#ifdef _WIN32
    const char* font_paths[] = { "C:\\Windows\\Fonts\\arial.ttf" };
#elif __APPLE__
    const char* font_paths[] = {
        // macOS system fonts
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/HelveticaNeue.ttc",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Supplemental/Verdana.ttf",
        "/Library/Fonts/Arial.ttf",
        "/Library/Fonts/Microsoft/Arial.ttf",
        // SF Pro (newer macOS)
        "/System/Library/Fonts/SFNSDisplay.ttf",
        "/System/Library/Fonts/SFNS.ttf"
    };
#else
    const char* font_paths[] = {
        // Ubuntu / Debian
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf",
        // RedHat 8, Oracle 8
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        // RedHat 9 / 10, Oracle 9 / 10
        "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
        "/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf"
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

    int default_idx = static_cast<int>(std::distance(
        FONT_AVAILABLE_SIZES.begin(),
        std::find(FONT_AVAILABLE_SIZES.begin(), FONT_AVAILABLE_SIZES.end(), BASE_FONT_SIZE)));

    if(default_idx >= static_cast<int>(m_available_sizes.size()))
        default_idx = 6; // fallback to index of 13.0f
    SetFontSize(default_idx);

    // Don't call Build() - ImGui 1.92+ backend handles font atlas building automatically.
    return true;
}

const std::vector<float>
FontManager::GetAvailableSizes() const
{
    return m_available_sizes;
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
