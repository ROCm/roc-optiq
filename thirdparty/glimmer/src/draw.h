#pragma once

#include "types.h"
#include "renderer.h"

namespace glimmer
{
    struct StyleDescriptor;
    struct FourSidedBorder;
    struct ColorGradient;

    IntersectRects ComputeIntersectRects(ImRect rect, ImVec2 startpos, ImVec2 endpos);
    RectBreakup ComputeRectBreakups(ImRect rect, float amount);

    void DrawBorderRect(ImVec2 startpos, ImVec2 endpos, const FourSidedBorder& border, uint32_t bgcolor, IRenderer& renderer);
    void DrawBoxShadow(ImVec2 startpos, ImVec2 endpos, const StyleDescriptor& style, IRenderer& renderer);
    void DrawBackground(ImVec2 startpos, ImVec2 endpos, const StyleDescriptor& style, IRenderer& renderer);
    void DrawBackground(ImVec2 startpos, ImVec2 endpos, uint32_t bgcolor, const ColorGradient& gradient, const FourSidedBorder& border, IRenderer& renderer);

    void DrawText(ImVec2 startpos, ImVec2 endpos, const ImRect& textrect, std::string_view text, bool disabled,
        const StyleDescriptor& style, IRenderer& renderer, std::optional<int32_t> txtflags = std::nullopt);
    void DrawSymbol(ImVec2 startpos, ImVec2 size, ImVec2 padding, SymbolIcon symbol, uint32_t outlineColor, uint32_t fillColor, float thickness, IRenderer& renderer);
    SymbolIcon GetSymbolIcon(std::string_view name);

    template <typename ItrT>
    void DrawLinearGradient(ImVec2 initpos, ImVec2 endpos, float angle, ImGuiDir dir,
        ItrT start, ItrT end, bool roundedBorder, IRenderer& renderer)
    {
        if (!roundedBorder)
        {
            auto width = endpos.x - initpos.x;
            auto height = endpos.y - initpos.y;

            if (dir == ImGuiDir::ImGuiDir_Left)
            {
                for (auto it = start; it != end; ++it)
                {
                    auto extent = width * it->pos;
                    renderer.DrawRectGradient(initpos, initpos + ImVec2{ extent, height },
                        it->from, it->to, DIR_Horizontal);
                    initpos.x += extent;
                }
            }
            else if (dir == ImGuiDir::ImGuiDir_Down)
            {
                for (auto it = start; it != end; ++it)
                {
                    auto extent = height * it->pos;
                    renderer.DrawRectGradient(initpos, initpos + ImVec2{ width, extent },
                        it->from, it->to, DIR_Vertical);
                    initpos.y += extent;
                }
            }
        }
        else
        {

        }
    }
}
