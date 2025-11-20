// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

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
    FontManager(const FontManager&)            = delete;
    FontManager& operator=(const FontManager&) = delete;

    /*
     * Called to initialize the font manager. Should be once called after ImGui context is
     * created, but before the first frame.
     */
    bool Init();

    const std::vector<ImFont*> GetAvailableFonts() const;
    ImFont*                    GetFont(FontType font_type);
    ImFont*                    GetIconFont(FontType font_type);
    ImFont*                    GetFontByIndex(int idx);
    ImFont*                    GetIconFontByIndex(int idx);
    int                        GetDPIScaledFontIndex();

    void SetFontSize(int idx);

private:
    std::vector<ImFont*> m_fonts;
    std::vector<ImFont*> m_icon_fonts;
    std::vector<ImFont*> m_all_fonts;
    std::vector<ImFont*> m_all_icon_fonts;
};

}  // namespace View
}  // namespace RocProfVis
