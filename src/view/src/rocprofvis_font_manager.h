// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "imgui.h"
#include <array>
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

    const std::vector<float> GetAvailableSizes() const;
    ImFont*                  GetFont(FontType font_type);
    ImFont*                  GetIconFont(FontType font_type);
    float                    GetFontSize(FontType font_type) const;
    int                      GetDPIScaledFontIndex();
    void                     SetFontSize(int idx);

    static constexpr int kNumTypes = static_cast<int>(FontType::__kLastFont);

private:

    ImFont* m_text_font = nullptr;
    ImFont* m_icon_font = nullptr;
    std::array<float, kNumTypes> m_sizes{};
    std::vector<float> m_available_sizes;
};

}  // namespace View
}  // namespace RocProfVis
