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

FontManager::FontManager() {}

FontManager::~FontManager() {}

ImFont*
FontManager::GetIconFontByIndex(int idx)
{
    if(idx < 0 || idx >= static_cast<int>(m_all_icon_fonts.size())) return nullptr;
    return m_all_icon_fonts[idx];
}

ImFont*
FontManager::GetFontByIndex(int idx)
{
    if(idx < 0 || idx >= static_cast<int>(m_all_fonts.size())) return nullptr;
    return m_all_fonts[idx];
}

int
FontManager::GetDPIScaledFontIndex()
{
    constexpr float DPI_EXPONENT =
        0.75f;  // Adjust as needed. Higher values increase size more rapidly.

    float           scaled_size =
        BASE_FONT_SIZE * std::pow(SettingsManager::GetInstance().GetDPI(), DPI_EXPONENT); 
 

    // Find the index of the font size closest to scaled_size
    int best_index = 0;
    for(int i = 1; i < m_all_fonts.size(); i++)
    {
        if(std::abs(m_all_fonts[i]->LegacySize - scaled_size) <
           std::abs(m_all_fonts[i - 1]->LegacySize - scaled_size))
            best_index = i;
    }

    return best_index;
}

void
FontManager::SetFontSize(int idx)
{
    constexpr int num_types = static_cast<int>(FontType::__kLastFont);

    if(num_types == 0 || m_all_fonts.empty()) return;
    if(idx < 0 || idx >= static_cast<int>(m_all_fonts.size())) return;
 
    static const int offsets[] = { -1, 0, 1, 2 };

    m_fonts.resize(num_types);
    m_icon_fonts.resize(num_types);

    for(int i = 0; i < num_types; ++i)
    {
        int font_idx = idx + offsets[i];
        font_idx =
            std::max(0, std::min(font_idx, static_cast<int>(m_all_fonts.size()) - 1));
        m_fonts[i]      = m_all_fonts[font_idx];
        m_icon_fonts[i] = m_all_icon_fonts[font_idx];
    }

    ImGui::GetIO().FontDefault = m_fonts[static_cast<int>(FontType::kDefault)];
    EventManager::GetInstance()->AddEvent(
        std::make_shared<RocEvent>(static_cast<int>(RocEvents::kFontSizeChanged)));
}

bool
FontManager::Init()
{
    ImGuiIO& io = ImGui::GetIO();
    m_fonts.clear();

    const int num_types = static_cast<int>(FontType::__kLastFont);

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

    // Prepare storage
    m_all_fonts.resize(FONT_AVAILABLE_SIZES.size(), nullptr);
    m_fonts.resize(num_types);
    m_icon_fonts.resize(num_types);

    ImFontConfig config;
    config.FontDataOwnedByAtlas = false;

    // Load all font sizes once
    for(int sz = 0; sz < FONT_AVAILABLE_SIZES.size(); ++sz)
    {
        ImFont* font = nullptr;
        if(font_path)
        {
            font = io.Fonts->AddFontFromFileTTF(font_path, FONT_AVAILABLE_SIZES[sz]);
        }
        else
        {
            ImFontConfig fallback_config;
            fallback_config.SizePixels = FONT_AVAILABLE_SIZES[sz];
            font                       = io.Fonts->AddFontDefault(&fallback_config);
        }
        m_all_fonts[sz] = font;
    }

    // Load all icon fonts
    m_all_icon_fonts.resize(FONT_AVAILABLE_SIZES.size(), nullptr);

    for(int sz = 0; sz < FONT_AVAILABLE_SIZES.size(); ++sz)
    {
        ImFont* icon_font = io.Fonts->AddFontFromMemoryCompressedTTF(
            &icon_font_compressed_data, icon_font_compressed_size,
            FONT_AVAILABLE_SIZES[sz], &config, icon_ranges);
        m_all_icon_fonts[sz] = icon_font;
    }

    // In ImGui 1.92+, the backend handles font atlas building automatically
    // via ImGuiBackendFlags_RendererHasTextures. Don't call Build() here.
    // The Vulkan backend will build the atlas when it's ready.
    return true;
}

const std::vector<ImFont*>
FontManager::GetAvailableFonts() const
{
    return m_all_fonts;
}

ImFont*
FontManager::GetFont(FontType font_type)
{
    if(static_cast<int>(font_type) < 0 ||
       static_cast<int>(font_type) >= static_cast<int>(FontType::__kLastFont))
    {
        return nullptr;  // Invalid font type
    }
    return m_fonts[static_cast<int>(font_type)];
}

ImFont*
FontManager::GetIconFont(FontType font_type)
{
    if(static_cast<int>(font_type) < 0 ||
       static_cast<int>(font_type) >= static_cast<int>(FontType::__kLastFont))
    {
        return nullptr;  // Invalid font type
    }
    return m_icon_fonts[static_cast<int>(font_type)];
}

}  // namespace View
}  // namespace RocProfVis