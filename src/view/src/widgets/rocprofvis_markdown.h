// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// Markdown, or plain text, drawn in any Dear ImGui window and wrapped to the
// available width. Parsing, layout, drawing, and copy/paste selection live in
// this one widget. Selection is not a separate layer: the view that parses the
// text also owns the selection and the clipboard copy.
//
// Covered: ATX and setext headings, paragraphs, soft and hard breaks, bullet,
// numbered, and task lists (nested; a tab counts as indent), block quotes,
// fenced code, code indented by four spaces, thematic breaks, tables,
// emphasis, strikethrough, inline code, links, autolinks, and bare URLs. An
// image is drawn as a link whose text is the alt text, or the image URL when
// the alt text is empty. Backslash escapes and the common HTML entities are
// expanded. Other HTML is shown as text.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "imgui.h"

namespace RocProfVis
{
namespace View
{

// Draw the source as written, wrapped, with Markdown left unparsed.
constexpr uint32_t MARKDOWN_PLAIN = 1u << 0;
// Drag, double-click word, triple-click line, Ctrl+A, and Ctrl+C.
constexpr uint32_t MARKDOWN_SELECTABLE = 1u << 1;

// Heading sizes as multiples of the body font size, h1 through h6.
constexpr float    MARKDOWN_HEADING_SCALE_H1 = 1.5f;
constexpr float    MARKDOWN_HEADING_SCALE_H2 = 1.3f;
constexpr float    MARKDOWN_HEADING_SCALE_H3 = 1.15f;
constexpr float    MARKDOWN_HEADING_SCALE_H4 = 1.0f;
constexpr float    MARKDOWN_HEADING_SCALE_H5 = 1.0f;
constexpr float    MARKDOWN_HEADING_SCALE_H6 = 1.0f;
constexpr uint32_t MARKDOWN_HEADING_LEVELS   = 6;

// Fonts left null use the font that is current when the view is drawn. Bold
// and italic are then thickened and slanted. Colors left at 0 follow ImGui.
struct markdown_style_t
{
    ImFont* font        = nullptr;
    ImFont* bold_font   = nullptr;
    ImFont* italic_font = nullptr;
    ImFont* code_font   = nullptr;
    // 0 takes the current font size.
    float font_size = 0.0f;
    // Heading sizes as multiples of font_size, h1 first.
    float heading_scale[MARKDOWN_HEADING_LEVELS] = {
        MARKDOWN_HEADING_SCALE_H1, MARKDOWN_HEADING_SCALE_H2, MARKDOWN_HEADING_SCALE_H3,
        MARKDOWN_HEADING_SCALE_H4, MARKDOWN_HEADING_SCALE_H5, MARKDOWN_HEADING_SCALE_H6
    };

    ImU32 text_color      = 0;
    ImU32 heading_color   = 0;
    ImU32 link_color      = 0;
    ImU32 code_color      = 0;
    ImU32 code_background = 0;
    ImU32 quote_color     = 0;
    // Quote bars, rules, and table borders.
    ImU32 line_color        = 0;
    ImU32 header_background = 0;
    ImU32 selection_color   = 0;

    // Asked for every link while drawing and again on click. `activate` is
    // false when the view only wants to know whether the link can be opened;
    // return true to draw it as a link. `activate` is true on click; open it
    // then and return true. Return false, including when `activate` is false,
    // to draw the target as plain text. Unset, nothing is drawn as a link.
    std::function<bool(const std::string& url, bool activate)> open_link;
};

// One text's parse, layout, and selection. Parsing runs when the text changes
// and layout when the width or fonts change, so drawing every frame is cheap.
// Copy and paste stay here, on the same text the parser produced.
class MarkdownView
{
public:
    MarkdownView();
    ~MarkdownView();
    MarkdownView(const MarkdownView&)            = delete;
    MarkdownView& operator=(const MarkdownView&) = delete;

    // `flags` is MARKDOWN_PLAIN, MARKDOWN_SELECTABLE, or both.
    void SetText(std::string_view text, uint32_t flags = MARKDOWN_SELECTABLE);
    // Draws at the cursor across the available width as one ImGui item, so
    // BeginPopupContextItem() works on the next line. Returns true on the
    // frame Ctrl+C copied the selection.
    bool Render(const markdown_style_t& style = markdown_style_t());

    bool        HasSelection() const;
    std::string GetSelectedText() const;
    // The text as drawn, with the Markdown markers removed.
    const std::string& GetPlainText() const;
    void               SelectAll();
    void               ClearSelection();
    void               Copy() const;

    // One switch over every view's selection, for an app-wide preference.
    // The view that owns the selection is also tracked here, so only one
    // selection is active in the app.
    static void SetSelectionEnabled(bool enabled);
    static bool IsSelectionEnabled();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    static bool     s_selection_enabled;
    static uint64_t s_active_view;
    static uint64_t s_view_count;
};

// Render for text that has no owner. The view lives in a cache keyed by `id`
// on the current ID stack and is dropped once it stops being drawn.
bool
render_markdown(const char* id, std::string_view text,
                uint32_t                flags = MARKDOWN_SELECTABLE,
                const markdown_style_t& style = markdown_style_t());

}  // namespace View
}  // namespace RocProfVis
