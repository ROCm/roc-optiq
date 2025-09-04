// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_font_manager.h"
#include "icons/rocprofvis_icon_data.h"
#include "icons/rocprovfis_icon_defines.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

FontManager::FontManager()
: m_font_sizes(
      { 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 20, 21, 23, 25, 27, 29, 33 })
{}

FontManager::~FontManager() {}

std::vector<float>
FontManager::GetFontSizes()
{
    // Return a vector of font sizes
    return m_font_sizes;
}
ImFont*
FontManager::GetIconFontByIndex(int idx)
{
    if(idx < 0 || idx >= static_cast<int>(m_all_icon_fonts.size())) return nullptr;
    return m_all_icon_fonts[idx];
}

int
FontManager::GetCurrentFontSizeIndex()
{
    // Return the font size index for the default font type
    return m_font_size_indices[static_cast<int>(FontType::kDefault)];
}
ImFont*
FontManager::GetFontByIndex(int idx)
{
    if(idx < 0 || idx >= static_cast<int>(m_all_fonts.size())) return nullptr;
    return m_all_fonts[idx];
}

int
FontManager::GetFontSizeIndexForDPI(float dpi)
{
    // DPI returns the dots per inch of the display. Essentially, it is a scaling factor.

    float base_size = 13.0f;
    float scaled_size =
        base_size *
        std::sqrt(dpi);  // Scale the base size by the square root of DPI. SQRT needed to
                         // prevent scaling from becoming comical on higher res monitors.

    // Find the index of the font size closest to scaled_size
    int best_index = 0;
    for(int i = 1; i < static_cast<int>(std::size(m_font_sizes)); ++i)
    {
        if(std::abs(m_font_sizes[i] - scaled_size) <
           std::abs(m_font_sizes[best_index] - scaled_size))
            best_index = i;
    }

    return best_index;
}

void
FontManager::SetFontSize(int size_index)
{
    constexpr int num_types = static_cast<int>(FontType::__kLastFont);

    if(num_types == 0 || m_all_fonts.empty()) return;
    if(size_index < 0 || size_index >= static_cast<int>(m_all_fonts.size())) return;

    for(int i = 0; i < num_types; ++i)
    {
        m_font_size_indices[i] = size_index;
        m_fonts[i]             = m_all_fonts[size_index];
        m_icon_fonts[i]        = m_all_icon_fonts[size_index];
    }

    ImGui::GetIO().FontDefault = m_fonts[static_cast<int>(FontType::kDefault)];
}
bool
FontManager::Init()
{
    ImGuiIO& io = ImGui::GetIO();
    m_fonts.clear();

    const int     num_sizes = static_cast<int>(m_font_sizes.size());
    constexpr int num_types = static_cast<int>(FontType::__kLastFont);

#ifdef _WIN32
    const char* font_paths[] = { "C:\\Windows\\Fonts\\arial.ttf" };
#else
    const char* font_paths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf"
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
    m_all_fonts.clear();
    m_all_fonts.resize(num_sizes, nullptr);
    m_font_size_indices.resize(num_types, 6);  // Default to index 6 (12pt)
    m_fonts.resize(num_types);
    m_icon_fonts.resize(num_types);

    ImFontConfig config;
    config.FontDataOwnedByAtlas = false;

    // Load all font sizes once
    for(int sz = 0; sz < num_sizes; ++sz)
    {
        ImFont* font = nullptr;
        if(font_path) font = io.Fonts->AddFontFromFileTTF(font_path, m_font_sizes[sz]);
        if(!font) font = io.Fonts->AddFontDefault();
        m_all_fonts[sz] = font;
    }

    // Load all icon fonts
    m_all_icon_fonts.clear();
    m_all_icon_fonts.resize(num_sizes, nullptr);

    for(int sz = 0; sz < num_sizes; ++sz)
    {
        ImFont* icon_font = io.Fonts->AddFontFromMemoryCompressedTTF(
            &icon_font_compressed_data, icon_font_compressed_size, m_font_sizes[sz],
            &config, icon_ranges);
        m_all_icon_fonts[sz] = icon_font;
    }

    // Set m_fonts to currently selected size for each FontType
    for(int type = 0; type < num_types; ++type)
    {
        int sz_idx    = m_font_size_indices[type];
        m_fonts[type] = m_all_fonts[sz_idx];

        // Load icon font for each type at the selected size
        m_icon_fonts[type] = io.Fonts->AddFontFromMemoryCompressedTTF(
            &icon_font_compressed_data, icon_font_compressed_size, m_font_sizes[sz_idx],
            &config, icon_ranges);
    }

    // Set ImGui's default font
    io.FontDefault = m_fonts[static_cast<int>(FontType::kDefault)];
    return io.Fonts->Build();
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
