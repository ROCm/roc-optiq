// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_markdown.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>

using namespace RocProfVis::View;

namespace
{

std::string
plain(std::string_view text, uint32_t flags = MARKDOWN_SELECTABLE)
{
    MarkdownView view;
    view.SetText(text, flags);
    return view.GetPlainText();
}

constexpr const char* BULLET = "\xE2\x80\xA2 ";

}  // namespace

TEST_CASE("Emphasis stays literal when it does not wrap a word", "[markdown]")
{
    CHECK(plain("a * b") == "a * b");
    CHECK(plain("float*") == "float*");
    CHECK(plain("snake_case") == "snake_case");
    CHECK(plain("*bold*") == "bold");
    CHECK(plain("**strong**") == "strong");
    CHECK(plain("~~gone~~") == "gone");
}

TEST_CASE("Lists keep nesting, numbers, and task boxes", "[markdown]")
{
    CHECK(plain("- a\n- b") == std::string(BULLET) + "a\n" + BULLET + "b");
    CHECK(plain("1. a\n9. b") == "1. a\n2. b");
    CHECK(plain("- [x] done\n- [ ] todo") == "[x] done\n[ ] todo");

    const std::string nested = plain("1. a\n   1. b");
    CHECK(nested == "1. a\n1. b");
}

TEST_CASE("A tab indents a nested list", "[markdown]")
{
    const std::string text = plain("- a\n\t- b");
    CHECK(text == std::string(BULLET) + "a\n" + BULLET + "b");
    CHECK(text.find('\t') == std::string::npos);
}

TEST_CASE("Tables drop extra cells and pad short rows", "[markdown]")
{
    CHECK(plain("| a | b |\n|---|---|\n| 1 | 2 |\n") == "a\tb\n1\t2");
    CHECK(plain("| a | b |\n|---|---|\n| 1 |\n") == "a\tb\n1\t");
    CHECK(plain("| a | b |\n|---|---|\n| 1 | 2 | 3 |\n") == "a\tb\n1\t2");
}

TEST_CASE("Fenced code inside a quote keeps the code text", "[markdown]")
{
    CHECK(plain("> ```\n> code\n> ```\n") == "code");
}

TEST_CASE("Four spaces start a code block", "[markdown]")
{
    CHECK(plain("    int x;") == "int x;");
}

TEST_CASE("A setext heading is not a rule", "[markdown]")
{
    CHECK(plain("Title\n=====\n") == "Title");
    CHECK(plain("---\n") == "");
    CHECK(plain("para\n---\n") == "para");
}

TEST_CASE("Escapes and HTML entities are expanded", "[markdown]")
{
    CHECK(plain("\\*star\\*") == "*star*");
    CHECK(plain("a &amp; b") == "a & b");
    CHECK(plain("&lt;") == "<");
}

TEST_CASE("Plain text drops carriage returns", "[markdown]")
{
    CHECK(plain("a\r\nb\r", MARKDOWN_PLAIN | MARKDOWN_SELECTABLE) == "a\nb");
}

TEST_CASE("Unmatched markers parse within a time bound", "[markdown]")
{
    std::string stars;
    std::string ticks;
    std::string brackets;
    std::string names;
    stars.reserve(40 * 3);
    for(int32_t i = 0; i < 40; ++i)
    {
        stars += "*a ";
        ticks += "`a ";
        brackets += "[a ";
        names += "_Z3foov ";
    }

    const auto   start = std::chrono::steady_clock::now();
    MarkdownView view;
    view.SetText(stars);
    view.SetText(ticks);
    view.SetText(brackets);
    view.SetText(names);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    CHECK(elapsed < std::chrono::milliseconds(250));
    CHECK(view.GetPlainText().find("_Z3foov") != std::string::npos);
}

TEST_CASE("Select all copies the drawn text and clears", "[markdown]")
{
    MarkdownView view;
    view.SetText("**hello**\n\nworld");
    view.SelectAll();
    CHECK(view.HasSelection());
    CHECK(view.GetSelectedText() == "hello\nworld");
    view.ClearSelection();
    CHECK_FALSE(view.HasSelection());
}
