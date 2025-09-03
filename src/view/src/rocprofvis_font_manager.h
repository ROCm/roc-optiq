// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "imgui.h"
#include <vector>

namespace RocProfVis
{
namespace View
{

enum class FontType
{
    kSmall,
    kMedium,
    kMedLarge,
    kLarge,
    // Used to get the size of the enum, insert new fonts before this line
    __kLastFont,
    kDefault = kMedium
};

class FontManager
{
public:
    FontManager();
    ~FontManager();
    FontManager(const FontManager&)                  = delete;
    FontManager&       operator=(const FontManager&) = delete;
    std::vector<float> GetFontSizes();
    ImFont*            GetIconFontByIndex(int idx);

    /*
     * Called to initialize the font manager. Should be once called after ImGui context is
     * created, but before the first frame.
     */
    bool Init();

    ImFont* GetFont(FontType font_type);
    ImFont* GetIconFont(FontType font_type);

    void    SetFontSize(int size_index);
    int     GetCurrentFontSizeIndex();
    ImFont* GetFontByIndex(int idx);
    int     GetFontSizeIndexForDPI(float dpi);

private:
    std::vector<ImFont*> m_fonts;
    std::vector<float>   m_font_sizes;
    std::vector<ImFont*> m_icon_fonts;
    std::vector<ImFont*> m_all_fonts;
    std::vector<int>     m_font_size_indices;
    std::vector<ImFont*> m_all_icon_fonts;
};

}  // namespace View
}  // namespace RocProfVis
