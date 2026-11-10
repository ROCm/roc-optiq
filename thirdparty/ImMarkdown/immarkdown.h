// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// ImMarkdown draws Markdown, or plain text, in any Dear ImGui window, wrapped
// to the available width, with mouse selection and copy that can be switched
// off. It depends on nothing but Dear ImGui.
//
// One call per frame, with the state kept for you:
//
//     ImMarkdown::Text("##notes", markdown);
//
// or own the state when the text outlives a frame or needs a context menu:
//
//     ImMarkdown::View view;
//     view.SetText(markdown);
//     view.Render(style);
//     if(ImGui::BeginPopupContextItem("##copy")) { ... view.Copy(); ... }
//
// Markdown covered: ATX and setext headings, paragraphs with soft and hard
// breaks, bullet, numbered, and task lists (nested), block quotes, fenced
// code, thematic breaks, GitHub tables, emphasis, strong, strikethrough,
// inline code, links, autolinks and bare URLs, images (as their alt text),
// backslash escapes, and the common HTML entities. Other HTML shows as text.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "imgui.h"

namespace ImMarkdown
{

enum Flags : uint32_t
{
    kNone = 0,
    // Draw the text as written, wrapped, with no Markdown.
    kPlain = 1u << 0,
    // Drag, double- and triple-click selection, Ctrl+A, and Ctrl+C.
    kSelectable = 1u << 1,
};

// Fonts left null use the current font, and bold and italic are then drawn by
// thickening and slanting it. Colors left at 0 follow the ImGui style.
struct Style
{
    ImFont* font        = nullptr;
    ImFont* bold_font   = nullptr;
    ImFont* italic_font = nullptr;
    ImFont* code_font   = nullptr;
    // 0 takes the current font size.
    float font_size = 0.0f;
    // Heading sizes as multiples of font_size, h1 first.
    float heading_scale[6] = { 1.5f, 1.3f, 1.15f, 1.0f, 1.0f, 1.0f };

    ImU32 text_color        = 0;
    ImU32 heading_color     = 0;
    ImU32 link_color        = 0;
    ImU32 code_color        = 0;
    ImU32 code_background   = 0;
    ImU32 quote_color       = 0;
    // Quote bars, rules, and table borders.
    ImU32 line_color        = 0;
    ImU32 header_background = 0;
    ImU32 selection_color   = 0;

    // Called with a link's target when it is clicked. Unset, links are drawn
    // but do nothing.
    std::function<void(const std::string& url)> open_link;
};

// A text's parse, layout, and selection. Parsing happens when the text
// changes and layout when the width or fonts do, so drawing every frame is
// cheap.
class View
{
public:
    View();
    ~View();
    View(const View&)            = delete;
    View& operator=(const View&) = delete;

    void SetText(std::string_view text, uint32_t flags = kSelectable);
    // Draws at the cursor across the available width as one ImGui item, so
    // BeginPopupContextItem() works right after. Returns true on the frame
    // Ctrl+C copied the selection.
    bool Render(const Style& style = Style());

    bool        HasSelection() const;
    std::string GetSelectedText() const;
    // The text as drawn, with the Markdown markers gone.
    const std::string& GetPlainText() const;
    void               SelectAll();
    void               ClearSelection();
    void               Copy() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// View::Render for text that has no owner: the view lives in a cache keyed by
// id in the current ID stack and is dropped once it stops being drawn.
bool Text(const char* id, std::string_view text, uint32_t flags = kSelectable,
          const Style& style = Style());

// One switch over every view's selection, for an app-wide preference.
void SetSelectionEnabled(bool enabled);
bool IsSelectionEnabled();

}  // namespace ImMarkdown
