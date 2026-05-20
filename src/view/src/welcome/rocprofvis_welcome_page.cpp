// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_welcome_page.h"

#include "imgui.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "widgets/rocprofvis_gui_helpers.h"

#include <algorithm>
#include <filesystem>
#include <list>
#include <utility>

namespace RocProfVis
{
namespace View
{

namespace
{
constexpr float WELCOME_CONTENT_MAX_EM     = 92.0f;
constexpr float WELCOME_ACTION_HEIGHT_EM   = 4.6f;
constexpr float WELCOME_RESOURCE_HEIGHT_EM = 7.25f;

constexpr const char* SUPPORTED_FILE_TYPES_HINT =
    "Supported types: .db, .rpd, .yaml, .rpv";

struct ResourceLink
{
    const char* title;
    const char* description;
    const char* url;
    Colors      accent;
};

constexpr ResourceLink kResourceLinks[] = {
    { "ROCm Optiq Documentation",
      "Workflows, installation, and feature guides.",
      "https://rocm.docs.amd.com/projects/roc-optiq/en/latest/",
      Colors::kEventHighlight },
    { "ROCm Optiq on GitHub",
      "Source, issues, and ongoing development.",
      "https://github.com/ROCm/roc-optiq",
      Colors::kLineChartColor },
    { "ROCm Systems",
      "Profile system-wide traces with ROCm Systems.",
      "https://github.com/rocm/rocm-systems",
      Colors::kTextSuccess },
    { "ROCm Platform",
      "AMD's open software stack for ROCm.",
      "https://www.amd.com/en/products/software/rocm.html",
      Colors::kComparisonGreater },
};

// Draws the welcome page background: solid theme fill, soft accent circles,
// faint grid, and a stylized two-wedge logo behind the content.
void
DrawBackdrop(SettingsManager& settings, ImVec2 page_pos, ImVec2 page_size)
{
    ImDrawList*  draw_list = ImGui::GetWindowDrawList();
    const ImVec2 page_max(page_pos.x + page_size.x, page_pos.y + page_size.y);
    const float  font_size = ImGui::GetFontSize();
    const bool   is_dark =
        settings.GetUserSettings().display_settings.use_dark_mode;

    draw_list->AddRectFilled(page_pos, page_max, settings.GetColor(Colors::kBgMain));
    // Tint the page slightly darker in light mode so white panels read as elevated.
    if(!is_dark)
        draw_list->AddRectFilled(
            page_pos, page_max,
            ApplyAlpha(settings.GetColor(Colors::kBorderColor), 0.55f));

    // Soft accent circles add depth; higher alpha for light mode.
    const float circle_alpha = is_dark ? 0.04f : 0.08f;
    draw_list->AddCircleFilled(
        ImVec2(page_pos.x + page_size.x * 0.22f, page_pos.y + page_size.y * 0.24f),
        page_size.x * 0.26f,
        ApplyAlpha(settings.GetColor(Colors::kAccentRed), circle_alpha), 72);
    draw_list->AddCircleFilled(
        ImVec2(page_pos.x + page_size.x * 0.72f, page_pos.y + page_size.y * 0.40f),
        page_size.x * 0.23f,
        ApplyAlpha(settings.GetColor(Colors::kLineChartColorAlt), circle_alpha), 72);

    const float grid_step  = std::max(36.0f, font_size * 3.4f);
    const ImU32 grid_color = settings.GetColor(Colors::kGridColor);
    for(float x = page_pos.x; x < page_max.x; x += grid_step)
        draw_list->AddLine(ImVec2(x, page_pos.y), ImVec2(x, page_max.y), grid_color);
    for(float y = page_pos.y; y < page_max.y; y += grid_step)
        draw_list->AddLine(ImVec2(page_pos.x, y), ImVec2(page_max.x, y), grid_color);

    // Stylized two-wedge logo (coordinates derived from the SVG asset).
    static constexpr ImVec2 kLogoTop[] = {
        { 197.2f, 13.7f }, { 174.7f, 13.7f }, { 161.0f, 0.0f }, { 210.9f, 0.0f },
        { 210.9f, 49.9f }, { 197.2f, 36.2f }, { 197.2f, 13.7f },
    };
    static constexpr ImVec2 kLogoBottom[] = {
        { 174.7f, 36.2f }, { 174.7f, 16.4f }, { 160.6f, 30.6f }, { 160.6f, 50.3f },
        { 180.3f, 50.3f }, { 194.4f, 36.2f }, { 174.7f, 36.2f },
    };
    constexpr float kMinX = 160.6f, kMaxX = 210.9f, kMaxY = 50.3f;
    const float     scale = std::min(page_size.x * 0.42f / (kMaxX - kMinX),
                                 page_size.y * 0.56f / kMaxY);
    const ImVec2    origin(
        page_pos.x + page_size.x * 0.62f - (kMaxX - kMinX) * scale * 0.48f -
            kMinX * scale,
        page_pos.y + page_size.y * 0.58f - kMaxY * scale * 0.50f);

    auto draw_polygon = [&](const ImVec2* src, ImU32 color) {
        ImVec2 pts[7];
        for(int i = 0; i < 7; ++i)
            pts[i] = ImVec2(origin.x + src[i].x * scale, origin.y + src[i].y * scale);
        draw_list->AddConvexPolyFilled(pts, 7, color);
    };

    draw_polygon(kLogoTop,
                 ApplyAlpha(settings.GetColor(Colors::kAccentRed),
                            is_dark ? 0.10f : 0.16f));
    draw_polygon(kLogoBottom,
                 ApplyAlpha(settings.GetColor(Colors::kLineChartColorAlt),
                            is_dark ? 0.20f : 0.25f));
}

// Rounded clickable card with accent dot, title, optional subtitle, and an
// optional accent footer. Used for resource links.
bool
DrawResourceCard(SettingsManager& settings, const ResourceLink& link, ImVec2 size)
{
    const float  font_size = ImGui::GetFontSize();
    const ImVec2 pos       = ImGui::GetCursorScreenPos();
    const ImVec2 bottom_right(pos.x + size.x, pos.y + size.y);

    ImGui::PushID(link.title);
    ImGui::InvisibleButton("card", size);
    const bool clicked = ImGui::IsItemClicked();
    const bool hovered = ImGui::IsItemHovered();

    ImDrawList* draw_list  = ImGui::GetWindowDrawList();
    const ImU32 accent_col = settings.GetColor(link.accent);
    const float rounding   = settings.GetDefaultStyle().FrameRounding + font_size * 0.2f;
    const bool  is_dark    = settings.GetUserSettings().display_settings.use_dark_mode;

    const float bg_alpha     = is_dark ? (hovered ? 0.75f : 0.55f)
                                       : (hovered ? 0.92f : 0.66f);
    const float accent_alpha = hovered ? 0.14f : 0.08f;
    const float border_alpha = is_dark ? 0.44f : 0.72f;

    draw_list->AddRectFilled(
        pos, bottom_right,
        ApplyAlpha(settings.GetColor(Colors::kBgFrame), bg_alpha), rounding);
    draw_list->AddRectFilled(
        pos, bottom_right, ApplyAlpha(accent_col, accent_alpha), rounding);
    draw_list->AddRect(
        pos, bottom_right,
        hovered ? accent_col
                : ApplyAlpha(settings.GetColor(Colors::kBorderColor), border_alpha),
        rounding);

    // Accent dot in the top-left corner of the card.
    const float  dot_r = font_size * 0.24f;
    const ImVec2 dot_pos(pos.x + font_size * 1.12f, pos.y + font_size * 1.20f);
    draw_list->AddCircleFilled(dot_pos, dot_r, accent_col);

    const ImVec2 text_pos(pos.x + font_size * 1.85f, pos.y + font_size * 0.72f);
    draw_list->PushClipRect(
        text_pos, ImVec2(bottom_right.x - font_size * 0.65f, bottom_right.y), true);
    draw_list->AddText(text_pos, settings.GetColor(Colors::kTextMain), link.title);
    draw_list->AddText(ImVec2(text_pos.x, text_pos.y + font_size * 1.40f),
                       settings.GetColor(Colors::kTextDim), link.description);
    draw_list->AddText(
        ImVec2(text_pos.x, bottom_right.y - font_size * 1.55f),
        settings.GetColor(Colors::kAccentRed), "Open resource");
    draw_list->PopClipRect();

    if(hovered) SetTooltipStyled("%s", link.url);
    ImGui::PopID();
    return clicked;
}

// Primary action row used in the Start tile.
bool
DrawPrimaryAction(SettingsManager& settings, const char* id, const char* title,
                  const char* subtitle, float width)
{
    const float  font_size = ImGui::GetFontSize();
    const ImVec2 size(width, font_size * WELCOME_ACTION_HEIGHT_EM);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 bottom_right(pos.x + size.x, pos.y + size.y);

    ImGui::PushID(id);
    ImGui::InvisibleButton("action", size);
    const bool clicked = ImGui::IsItemClicked();
    const bool hovered = ImGui::IsItemHovered();

    ImDrawList* draw_list  = ImGui::GetWindowDrawList();
    const float rounding   = settings.GetDefaultStyle().ChildRounding + font_size * 0.18f;
    const ImU32 accent_col = settings.GetColor(Colors::kAccentRed);
    const bool  is_dark =
        settings.GetUserSettings().display_settings.use_dark_mode;

    const float bg_alpha = is_dark ? (hovered ? 0.30f : 0.22f)
                                   : (hovered ? 0.45f : 0.35f);

    draw_list->AddRectFilled(pos, bottom_right, ApplyAlpha(accent_col, bg_alpha),
                             rounding);
    draw_list->AddRect(pos, bottom_right, accent_col, rounding);

    const ImVec2 text_pos(pos.x + font_size * 1.05f, pos.y + font_size * 0.85f);
    draw_list->PushClipRect(
        ImVec2(text_pos.x, pos.y),
        ImVec2(bottom_right.x - font_size * 0.8f, bottom_right.y), true);
    draw_list->AddText(text_pos, settings.GetColor(Colors::kTextMain), title);
    draw_list->AddText(ImVec2(text_pos.x, text_pos.y + font_size * 1.35f),
                       settings.GetColor(Colors::kTextDim), subtitle);
    draw_list->PopClipRect();

    ImGui::PopID();
    return clicked;
}

// Opens a styled child container for a welcome page section with title and
// optional subtitle, followed by a divider.
void
BeginTile(SettingsManager& settings, const char* id, const char* title,
          const char* subtitle)
{
    const float font_size = ImGui::GetFontSize();
    const bool  is_dark =
        settings.GetUserSettings().display_settings.use_dark_mode;
    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          ApplyAlpha(settings.GetColor(Colors::kBgPanel), 0.55f));
    ImGui::PushStyleColor(ImGuiCol_Border,
                          ApplyAlpha(settings.GetColor(Colors::kBorderColor),
                                     is_dark ? 0.55f : 0.82f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,
                        settings.GetDefaultStyle().ChildRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(font_size * 1.15f, font_size * 0.85f));
    ImGui::BeginChild(id, ImVec2(0.0f, 0.0f),
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::PushFont(NULL, settings.GetFontManager().GetFontSize(FontSize::kMedLarge));
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextMain));
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    ImGui::PopFont();

    if(subtitle && subtitle[0])
    {
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
        ImGui::TextUnformatted(subtitle);
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    }

    const ImVec2 sep_pos   = ImGui::GetCursorScreenPos();
    const float  sep_width = ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddLine(
        sep_pos, ImVec2(sep_pos.x + sep_width, sep_pos.y),
        ApplyAlpha(settings.GetColor(Colors::kBorderColor), 0.55f), 1.0f);
    ImGui::Dummy(ImVec2(sep_width, font_size * 0.38f));
}

void
EndTile()
{
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}
}  // namespace

WelcomePage::WelcomePage(OpenFileFn on_open_file, OpenRecentFileFn on_open_recent)
    : m_on_open_file(std::move(on_open_file))
    , m_on_open_recent(std::move(on_open_recent))
{
}

void
WelcomePage::Render()
{
    SettingsManager& settings  = SettingsManager::GetInstance();
    const float      font_size = ImGui::GetFontSize();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("welcome_page", ImVec2(0.0f, 0.0f), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImVec2 page_pos  = ImGui::GetWindowPos();
    const ImVec2 page_size = ImGui::GetWindowSize();
    DrawBackdrop(settings, page_pos, page_size);

    // Center a fixed-max-width content column with a responsive compact split.
    const float edge_margin = std::max(font_size * 1.35f,
                                       std::min(font_size * 2.4f, page_size.x * 0.035f));
    const float content_width =
        std::min(std::max(0.0f, page_size.x - edge_margin * 2.0f),
                 font_size * WELCOME_CONTENT_MAX_EM);
    const bool is_compact = content_width < font_size * 62.0f;

    ImGui::SetCursorPos(ImVec2((page_size.x - content_width) * 0.5f, edge_margin));
    ImGui::BeginChild("welcome_body", ImVec2(content_width, 0.0f), false,
                      ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
                          ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(font_size * 0.95f, font_size * 0.95f));

    RenderHeader(content_width);

    const float column_gap = font_size * 0.72f;
    const float main_col_w = is_compact
                                 ? content_width
                                 : std::max(font_size * 30.0f, content_width * 0.38f);
    const float side_col_w = is_compact
                                 ? content_width
                                 : std::max(0.0f, content_width - main_col_w - column_gap);

    std::string recent_file_to_open;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kTransparent));
    ImGui::BeginChild("welcome_main_col", ImVec2(main_col_w, 0.0f),
                      ImGuiChildFlags_AutoResizeY,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    RenderStartTile();
    ImGui::Dummy(ImVec2(0.0f, font_size * 0.30f));
    RenderRecentTile(recent_file_to_open);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    if(!is_compact) ImGui::SameLine(0.0f, column_gap);
    else            ImGui::Dummy(ImVec2(0.0f, font_size * 0.30f));

    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kTransparent));
    ImGui::BeginChild("welcome_side_col", ImVec2(side_col_w, 0.0f),
                      ImGuiChildFlags_AutoResizeY,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    RenderResourcesTile();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::PopStyleVar();  // welcome_body ItemSpacing
    ImGui::EndChild();     // welcome_body
    ImGui::EndChild();     // welcome_page
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // Defer the open until after we have torn down all the welcome children,
    // because opening a file can immediately replace this view with a project tab.
    if(!recent_file_to_open.empty())
    {
        m_on_open_recent(recent_file_to_open);
    }
}

void
WelcomePage::RenderHeader(float content_width)
{
    SettingsManager& settings = SettingsManager::GetInstance();
    const float      font_size = ImGui::GetFontSize();

    ImGui::PushFont(NULL, settings.GetFontManager().GetFontSize(FontSize::kLarge));
    ImGui::TextUnformatted("Welcome to ROCm Optiq");
    ImGui::PopFont();

    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + content_width);
    ImGui::TextUnformatted(
        "Unified visualization and analysis for ROCm Systems Profiler traces and "
        "ROCm Compute Profiler data. Identify bottlenecks, understand hardware "
        "utilization, and optimize GPU workloads.");
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, font_size * 0.35f));
}

void
WelcomePage::RenderStartTile()
{
    SettingsManager& settings  = SettingsManager::GetInstance();
    const float      font_size = ImGui::GetFontSize();

    BeginTile(settings, "tile_start", "Start",
              "Choose a workflow or drop a trace onto the app window.");
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                            ImVec2(0.0f, font_size * 0.25f));
        const float inner_w = ImGui::GetContentRegionAvail().x;
        if(DrawPrimaryAction(settings, "open_trace", "Open Trace File...",
                             "Open a .db, .rpd, or .yaml file.", inner_w))
        {
            m_on_open_file();
        }
        if(ImGui::IsItemHovered()) SetTooltipStyled("%s", SUPPORTED_FILE_TYPES_HINT);

        ImGui::Dummy(ImVec2(0.0f, font_size * 0.35f));
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + inner_w);
        ImGui::TextUnformatted(
            "Or drag and drop a trace file onto the window.");
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }
    EndTile();
}

void
WelcomePage::RenderRecentTile(std::string& recent_file_to_open)
{
    SettingsManager& settings  = SettingsManager::GetInstance();
    const float      font_size = ImGui::GetFontSize();
    const std::list<std::string>& recent_files =
        settings.GetInternalSettings().recent_files;

    BeginTile(settings, "tile_recent", "Recent", "");
    {
        const float inner_w = ImGui::GetContentRegionAvail().x;
        if(recent_files.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + inner_w);
            ImGui::TextUnformatted(
                "Recent projects and traces will appear here for quick access.");
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                                ImVec2(0.0f, font_size * 0.55f));
            int shown = 0;
            for(const std::string& file : recent_files)
            {
                if(shown++ >= static_cast<int>(MAX_RECENT_FILES)) break;
                const std::filesystem::path fp(file);
                const std::string fname =
                    fp.filename().empty() ? file : fp.filename().string();

                ImGui::PushID(file.c_str());
                if(ImGui::Selectable(fname.c_str(), false,
                                     ImGuiSelectableFlags_None,
                                     ImVec2(inner_w, 0.0f)))
                {
                    recent_file_to_open = file;
                }
                if(ImGui::IsItemHovered()) SetTooltipStyled("%s", file.c_str());
                ImGui::PopID();
            }
            ImGui::PopStyleVar();
        }
    }
    EndTile();
}

void
WelcomePage::RenderResourcesTile()
{
    SettingsManager& settings  = SettingsManager::GetInstance();
    const float      font_size = ImGui::GetFontSize();

    BeginTile(settings, "tile_resources", "Resources",
              "Jump into documentation, source, and ROCm ecosystem pages.");
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                            ImVec2(font_size * 0.55f, font_size * 0.30f));
        const float resource_gap = font_size * 0.55f;
        const float inner_w      = ImGui::GetContentRegionAvail().x;
        const bool  two_columns  = inner_w > font_size * 38.0f;
        const float card_width =
            two_columns ? (inner_w - resource_gap) * 0.5f : inner_w;
        constexpr size_t link_count =
            sizeof(kResourceLinks) / sizeof(kResourceLinks[0]);
        for(size_t i = 0; i < link_count; ++i)
        {
            const ImVec2 size(card_width, font_size * WELCOME_RESOURCE_HEIGHT_EM);
            if(DrawResourceCard(settings, kResourceLinks[i], size))
                open_url(kResourceLinks[i].url);

            if(two_columns && i % 2 == 0 && i + 1 < link_count)
                ImGui::SameLine(0.0f, resource_gap);
        }
        ImGui::PopStyleVar();
    }
    EndTile();
}

}  // namespace View
}  // namespace RocProfVis
