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
    kMainText,
    kIcon,
    // Used to get the size of the enum, insert new fonts before this line
    __kLastFont,
    kDefault = kMainText
};

enum class FontSize
{
    kSmall,
    kMedium,
    kMedLarge,
    kLarge,
    // Used to get the size of the enum, insert new fonts before this line
    __kLastSize,
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

    ImFont* GetFont(FontType font_type);
    float   GetFontSize(FontSize font_type) const;

    // User-controllable font size (points), snapped to the available sizes.
    float GetMinUserFontSize() const;
    float GetMaxUserFontSize() const;
    float GetFontSizeAt(int idx) const;
    int   GetFontSizeIndex(float font_size) const;
    int   GetDefaultFontSizeIndex() const;
    int   ClampFontSizeIndex(int idx) const;
    void  SetFontSize(int idx);

    static constexpr int kNumSizes = static_cast<int>(FontSize::__kLastSize);

private:

    ImFont* m_text_font = nullptr;
    ImFont* m_icon_font = nullptr;
    std::array<float, kNumSizes> m_sizes{};
    std::vector<float> m_available_sizes;
};

}  // namespace View
}  // namespace RocProfVis
