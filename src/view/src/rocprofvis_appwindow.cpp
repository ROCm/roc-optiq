// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_appwindow.h"
#include "imgui.h"
#include "implot.h"
#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
#    include "nfd.h"
#endif
#include "ImGuiFileDialog.h"

#include "rocprofvis_controller.h"
#include "rocprofvis_events.h"
#include "rocprofvis_project.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_hotkey_manager.h"
#include "rocprofvis_settings_panel.h"
#include "rocprofvis_version.h"
#include "rocprofvis_utils.h"
#include "rocprofvis_root_view.h"
#include "rocprofvis_trace_view.h"
#include "rocprofvis_view_module.h"
#include "widgets/rocprofvis_debug_window.h"
#include "widgets/rocprofvis_dialog.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_widget.h"
#include "widgets/rocprofvis_notification_manager.h"
#include <algorithm>
#include <filesystem>
#include <sstream>
#include <utility>

namespace RocProfVis
{
namespace View
{

constexpr ImVec2      FILE_DIALOG_SIZE       = ImVec2(480.0f, 360.0f);
constexpr const char* FILE_DIALOG_NAME       = "ChooseFileDlgKey";
constexpr const char* TAB_CONTAINER_SRC_NAME = "MainTabContainer";
constexpr const char* ABOUT_DIALOG_NAME      = "About##_dialog";
constexpr const char* APP_SHUTDOWN_NOTIFICATION_ID = "provider_cleanup_app_shutdown";
constexpr const char* SHUTDOWN_DIALOG_NAME = "Closing Traces##_shutdown";
constexpr float WELCOME_CONTENT_MAX_EM   = 92.0f;
constexpr float WELCOME_ACTION_HEIGHT_EM = 4.6f;
constexpr float WELCOME_CARD_HEIGHT_EM   = 7.25f;

const std::vector<std::string> TRACE_EXTENSIONS   = { "db", "rpd", "yaml" };
const std::vector<std::string> PROJECT_EXTENSIONS = { "rpv" };
const std::vector<std::string> ALL_EXTENSIONS     = { "db", "rpd", "yaml", "rpv" };
constexpr const char* SUPPORTED_FILE_TYPES_HINT   = "Supported types: .db, .rpd, .yaml, .rpv";

constexpr float STATUS_BAR_HEIGHT = 30.0f;

constexpr const char* CLEANUP_MESSAGE = "Waiting for requests to finish cleanup...";
constexpr const char* CLOSING_MESSAGE = "Closing...";

// For testing DataProvider
void
RenderProviderTest(DataProvider& provider);

namespace
{
struct WelcomeLink
{
    const char* title;
    const char* description;
    const char* url;
    Colors      color;
};

ImU32
WelcomeColor(SettingsManager& settings, Colors color, float alpha)
{
    return ApplyAlpha(settings.GetColor(color), alpha);
}

void
DrawWelcomeBackdrop(ImDrawList* draw_list, const ImVec2& page_pos, const ImVec2& page_size,
                    bool is_dark_mode)
{
    const ImVec2 page_max(page_pos.x + page_size.x, page_pos.y + page_size.y);
    draw_list->AddRectFilled(page_pos, page_max,
                             is_dark_mode ? IM_COL32(3, 6, 14, 255)
                                          : IM_COL32(243, 247, 252, 255));

    if(is_dark_mode)
    {
        draw_list->AddRectFilledMultiColor(
            page_pos, page_max, IM_COL32(18, 23, 38, 210), IM_COL32(4, 8, 18, 250),
            IM_COL32(3, 7, 16, 255), IM_COL32(14, 20, 32, 238));
        draw_list->AddCircleFilled(ImVec2(page_pos.x + page_size.x * 0.22f,
                                          page_pos.y + page_size.y * 0.24f),
                                   page_size.x * 0.26f, IM_COL32(74, 138, 220, 24), 72);
        draw_list->AddCircleFilled(ImVec2(page_pos.x + page_size.x * 0.72f,
                                          page_pos.y + page_size.y * 0.40f),
                                   page_size.x * 0.23f, IM_COL32(34, 205, 198, 18), 72);
    }
    else
    {
        draw_list->AddRectFilledMultiColor(
            page_pos, page_max, IM_COL32(255, 255, 255, 218),
            IM_COL32(232, 241, 250, 245), IM_COL32(238, 247, 250, 250),
            IM_COL32(255, 255, 255, 230));
        draw_list->AddCircleFilled(ImVec2(page_pos.x + page_size.x * 0.24f,
                                          page_pos.y + page_size.y * 0.22f),
                                   page_size.x * 0.28f, IM_COL32(75, 146, 220, 28), 72);
        draw_list->AddCircleFilled(ImVec2(page_pos.x + page_size.x * 0.74f,
                                          page_pos.y + page_size.y * 0.42f),
                                   page_size.x * 0.24f, IM_COL32(40, 190, 205, 24), 72);
    }

    const float grid_step = std::max(36.0f, ImGui::GetFontSize() * 3.4f);
    const ImU32 grid_color = is_dark_mode ? IM_COL32(255, 255, 255, 7)
                                          : IM_COL32(28, 58, 92, 10);
    for(float x = page_pos.x; x < page_max.x; x += grid_step)
    {
        draw_list->AddLine(ImVec2(x, page_pos.y), ImVec2(x, page_max.y), grid_color);
    }
    for(float y = page_pos.y; y < page_max.y; y += grid_step)
    {
        draw_list->AddLine(ImVec2(page_pos.x, y), ImVec2(page_max.x, y), grid_color);
    }
}

void
DrawWelcomePill(SettingsManager& settings, const char* label)
{
    const float  font_size = ImGui::GetFontSize();
    const ImVec2 text_size = ImGui::CalcTextSize(label);
    const ImVec2 padding(font_size * 0.55f, font_size * 0.24f);
    const ImVec2 size(text_size.x + padding.x * 2.0f,
                      text_size.y + padding.y * 2.0f);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList*  draw_list = ImGui::GetWindowDrawList();

    draw_list->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                             ApplyAlpha(settings.GetColor(Colors::kAccentRed), 0.16f),
                             font_size * 0.45f);
    draw_list->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                       ApplyAlpha(settings.GetColor(Colors::kAccentRed), 0.55f),
                       font_size * 0.45f);
    draw_list->AddText(ImVec2(pos.x + padding.x, pos.y + padding.y),
                       settings.GetColor(Colors::kAccentRed), label);
    ImGui::Dummy(size);
}

void
DrawWelcomeHeroFeature(SettingsManager& settings, const char* title,
                       const char* description, Colors color, float width)
{
    const float  font_size = ImGui::GetFontSize();
    const ImVec2 size(width, font_size * 4.4f);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool   is_dark =
        settings.GetUserSettings().display_settings.use_dark_mode;
    ImDrawList*  draw_list = ImGui::GetWindowDrawList();
    const float  rounding = settings.GetDefaultStyle().FrameRounding + font_size * 0.22f;
    const ImU32  accent   = settings.GetColor(color);

    draw_list->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                             WelcomeColor(settings, Colors::kBgFrame,
                                          is_dark ? 0.28f : 0.62f),
                             rounding);
    draw_list->AddRectFilledMultiColor(
        pos, ImVec2(pos.x + size.x, pos.y + size.y),
        ApplyAlpha(accent, is_dark ? 0.16f : 0.11f), IM_COL32(0, 0, 0, 0),
        IM_COL32(0, 0, 0, 0), ApplyAlpha(accent, is_dark ? 0.09f : 0.06f));
    draw_list->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                       WelcomeColor(settings, Colors::kBorderColor,
                                    is_dark ? 0.42f : 0.72f),
                       rounding);
    draw_list->AddCircleFilled(ImVec2(pos.x + font_size * 1.05f,
                                      pos.y + font_size * 1.12f),
                               font_size * 0.23f, accent);

    const ImVec2 text_pos(pos.x + font_size * 1.75f, pos.y + font_size * 0.64f);
    draw_list->PushClipRect(text_pos,
                            ImVec2(pos.x + size.x - font_size * 0.75f,
                                   pos.y + size.y - font_size * 0.35f), true);
    draw_list->AddText(text_pos, settings.GetColor(Colors::kTextMain), title);
    draw_list->AddText(ImVec2(text_pos.x, text_pos.y + font_size * 1.35f),
                       settings.GetColor(Colors::kTextDim), description);
    draw_list->PopClipRect();

    ImGui::Dummy(size);
}

void
DrawWelcomeBackgroundLogo(ImDrawList* draw_list, const ImVec2& page_pos,
                          const ImVec2& page_size, bool is_dark_mode)
{
    static constexpr ImVec2 kTopRightShape[] = {
        ImVec2(197.2f, 13.7f), ImVec2(174.7f, 13.7f),
        ImVec2(161.0f, 0.0f),  ImVec2(210.9f, 0.0f),
        ImVec2(210.9f, 49.9f), ImVec2(197.2f, 36.2f),
        ImVec2(197.2f, 13.7f),
    };
    static constexpr ImVec2 kBottomLeftShape[] = {
        ImVec2(174.7f, 36.2f), ImVec2(174.7f, 16.4f),
        ImVec2(160.6f, 30.6f), ImVec2(160.6f, 50.3f),
        ImVec2(180.3f, 50.3f), ImVec2(194.4f, 36.2f),
        ImVec2(174.7f, 36.2f),
    };

    constexpr float kLogoMinX = 160.6f;
    constexpr float kLogoMaxX = 210.9f;
    constexpr float kLogoMinY = 0.0f;
    constexpr float kLogoMaxY = 50.3f;
    constexpr float kLogoW    = kLogoMaxX - kLogoMinX;
    constexpr float kLogoH    = kLogoMaxY - kLogoMinY;

    const float scale = std::min(page_size.x * 0.42f / kLogoW,
                                 page_size.y * 0.56f / kLogoH);
    const ImVec2 logo_size(kLogoW * scale, kLogoH * scale);
    const ImVec2 origin(page_pos.x + page_size.x * 0.62f - logo_size.x * 0.48f -
                            (kLogoMinX * scale),
                        page_pos.y + page_size.y * 0.58f - logo_size.y * 0.50f -
                            (kLogoMinY * scale));

    auto transform = [&](const ImVec2& p, const ImVec2& offset) {
        return ImVec2(origin.x + p.x * scale + offset.x,
                      origin.y + p.y * scale + offset.y);
    };

    auto draw_poly = [&](const ImVec2* src, int count, const ImVec2& offset,
                         ImU32 color) {
        ImVec2 pts[7];
        for(int i = 0; i < count; ++i)
        {
            pts[i] = transform(src[i], offset);
        }
        draw_list->AddConvexPolyFilled(pts, count, color);
    };

    const ImU32 glow_color =
        is_dark_mode ? IM_COL32(70, 190, 220, 18) : IM_COL32(60, 130, 210, 18);
    const float glow = scale * 0.9f;
    const ImVec2 glow_offsets[] = {
        ImVec2(-glow, 0.0f), ImVec2(glow, 0.0f),
        ImVec2(0.0f, -glow), ImVec2(0.0f, glow),
        ImVec2(-glow, -glow), ImVec2(glow, glow),
    };
    for(const ImVec2& offset : glow_offsets)
    {
        draw_poly(kTopRightShape, 7, offset, glow_color);
        draw_poly(kBottomLeftShape, 7, offset, glow_color);
    }

    const ImU32 top_fill =
        is_dark_mode ? IM_COL32(64, 130, 210, 80) : IM_COL32(70, 145, 220, 46);
    const ImU32 bottom_fill =
        is_dark_mode ? IM_COL32(38, 190, 205, 76) : IM_COL32(44, 180, 205, 42);
    draw_poly(kTopRightShape, 7, ImVec2(0.0f, 0.0f), top_fill);
    draw_poly(kBottomLeftShape, 7, ImVec2(0.0f, 0.0f), bottom_fill);

    auto draw_outline = [&](const ImVec2* src, int count, ImU32 color) {
        for(int i = 0; i + 1 < count; ++i)
        {
            draw_list->AddLine(transform(src[i], ImVec2(0.0f, 0.0f)),
                               transform(src[i + 1], ImVec2(0.0f, 0.0f)),
                               color, std::max(1.0f, scale * 0.08f));
        }
    };
    const ImU32 outline =
        is_dark_mode ? IM_COL32(120, 220, 245, 88) : IM_COL32(80, 145, 220, 58);
    draw_outline(kTopRightShape, 7, outline);
    draw_outline(kBottomLeftShape, 7, outline);
}

bool
DrawWelcomeAction(SettingsManager& settings, const char* title, const char* description,
                  const char* id, float width, bool enabled = true, bool primary = false)
{
    const float font_size = ImGui::GetFontSize();
    const ImVec2 size(width, font_size * WELCOME_ACTION_HEIGHT_EM);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::PushID(id);
    ImGui::InvisibleButton("action", size);
    const bool clicked = enabled && ImGui::IsItemClicked();
    const bool hovered = enabled && ImGui::IsItemHovered();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    const float rounding = settings.GetDefaultStyle().ChildRounding + font_size * 0.18f;
    const bool is_dark = settings.GetUserSettings().display_settings.use_dark_mode;
    const ImVec2 draw_pos(pos.x, pos.y + (hovered ? -1.0f : 0.0f));
    const ImVec2 draw_max(draw_pos.x + size.x, draw_pos.y + size.y);
    const ImU32 accent = settings.GetColor(primary ? Colors::kAccentRed
                                                   : Colors::kLineChartColor);
    const float bg_alpha = primary ? (is_dark ? 0.35f : 0.90f)
                                   : (is_dark ? 0.23f : 0.68f);

    draw_list->AddRectFilled(ImVec2(draw_pos.x + 2.0f, draw_pos.y + 3.0f),
                             ImVec2(draw_max.x + 2.0f, draw_max.y + 3.0f),
                             is_dark ? IM_COL32(0, 0, 0, 72)
                                     : IM_COL32(74, 98, 130, 18),
                             rounding);
    draw_list->AddRectFilled(draw_pos, draw_max,
                             primary ? ApplyAlpha(accent, hovered ? 0.26f : 0.20f)
                                     : WelcomeColor(settings, Colors::kBgFrame,
                                                    hovered ? bg_alpha + 0.15f : bg_alpha),
                             rounding);
    if(primary)
    {
        draw_list->AddRectFilledMultiColor(
            draw_pos, draw_max, ApplyAlpha(accent, hovered ? 0.36f : 0.26f),
            WelcomeColor(settings, Colors::kBgFrame, is_dark ? 0.34f : 0.82f),
            WelcomeColor(settings, Colors::kBgFrame, is_dark ? 0.30f : 0.78f),
            ApplyAlpha(accent, hovered ? 0.22f : 0.15f));
    }
    draw_list->AddLine(ImVec2(pos.x + rounding * 0.5f, pos.y + 1.0f),
                       ImVec2(pos.x + size.x - rounding * 0.5f, pos.y + 1.0f),
                       IM_COL32(255, 255, 255, hovered ? 36 : 18), 1.0f);
    draw_list->AddRect(draw_pos, draw_max,
                       hovered || primary ? accent
                                          : WelcomeColor(settings, Colors::kBorderColor,
                                                         is_dark ? 0.45f : 0.82f),
                       rounding);
    draw_list->AddRectFilled(draw_pos, ImVec2(draw_pos.x + 3.0f, draw_max.y),
                             ApplyAlpha(accent, enabled ? 0.92f : 0.24f),
                             rounding, ImDrawFlags_RoundCornersLeft);

    const float icon_size = font_size * 1.65f;
    const ImVec2 icon_pos(draw_pos.x + font_size * 1.05f,
                          draw_pos.y + (size.y - icon_size) * 0.5f);
    draw_list->AddRectFilled(icon_pos,
                             ImVec2(icon_pos.x + icon_size, icon_pos.y + icon_size),
                             ApplyAlpha(accent, enabled ? 0.18f : 0.08f),
                             font_size * 0.35f);
    draw_list->AddRect(icon_pos, ImVec2(icon_pos.x + icon_size, icon_pos.y + icon_size),
                       ApplyAlpha(accent, enabled ? 0.64f : 0.18f),
                       font_size * 0.35f);
    draw_list->AddCircleFilled(ImVec2(icon_pos.x + icon_size * 0.5f,
                                      icon_pos.y + icon_size * 0.5f),
                               icon_size * 0.16f,
                               ApplyAlpha(accent, enabled ? 0.82f : 0.20f));

    const ImVec2 text_pos(icon_pos.x + icon_size + font_size * 0.75f,
                          draw_pos.y + font_size * 0.85f);
    const float text_clip_right = draw_pos.x + size.x - font_size * 0.8f;
    draw_list->PushClipRect(ImVec2(text_pos.x, draw_pos.y),
                            ImVec2(text_clip_right, draw_pos.y + size.y), true);
    draw_list->AddText(text_pos,
                       enabled ? settings.GetColor(Colors::kTextMain)
                               : settings.GetColor(Colors::kTextDim),
                       title);
    draw_list->AddText(ImVec2(text_pos.x, text_pos.y + font_size * 1.35f),
                       settings.GetColor(Colors::kTextDim), description);
    draw_list->PopClipRect();

    ImGui::PopID();
    return clicked;
}

bool
DrawWelcomeRecentRow(SettingsManager& settings, const std::string& file, float width)
{
    const float font_size = ImGui::GetFontSize();
    const ImVec2 size(width, font_size * 2.30f);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::PushID(file.c_str());
    ImGui::InvisibleButton("recent_row", size);
    const bool clicked = ImGui::IsItemClicked();
    const bool hovered = ImGui::IsItemHovered();
    const bool is_dark =
        settings.GetUserSettings().display_settings.use_dark_mode;
    const std::filesystem::path fpath(file);
    const std::string fname =
        fpath.filename().empty() ? file : fpath.filename().string();
    const std::string parent =
        fpath.parent_path().empty() ? "" : fpath.parent_path().filename().string();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const float rounding = settings.GetDefaultStyle().FrameRounding + font_size * 0.2f;

    draw_list->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                             WelcomeColor(settings, Colors::kBgFrame,
                                          hovered ? (is_dark ? 0.42f : 0.86f)
                                                  : (is_dark ? 0.22f : 0.55f)),
                             rounding);
    draw_list->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                       hovered ? settings.GetColor(Colors::kLineChartColor)
                               : WelcomeColor(settings, Colors::kBorderColor,
                                              is_dark ? 0.34f : 0.62f),
                       rounding);
    draw_list->AddCircleFilled(ImVec2(pos.x + font_size * 0.95f,
                                      pos.y + size.y * 0.50f),
                               font_size * 0.27f,
                               WelcomeColor(settings, Colors::kLineChartColor,
                                            hovered ? 0.95f : 0.65f));

    const ImVec2 text_pos(pos.x + font_size * 1.75f, pos.y + font_size * 0.34f);
    draw_list->PushClipRect(text_pos,
                            ImVec2(pos.x + size.x - font_size * 0.55f,
                                   pos.y + size.y), true);
    draw_list->AddText(text_pos, settings.GetColor(Colors::kTextMain), fname.c_str());
    if(!parent.empty())
    {
        draw_list->AddText(ImVec2(text_pos.x, text_pos.y + font_size * 1.05f),
                           settings.GetColor(Colors::kTextDim), parent.c_str());
    }
    draw_list->PopClipRect();
    if(hovered) SetTooltipStyled("%s", file.c_str());
    ImGui::PopID();
    return clicked;
}

void
DrawWelcomeLinkCard(SettingsManager& settings, const WelcomeLink& link, const char* id,
                    float width)
{
    const float  font_size = ImGui::GetFontSize();
    const ImVec2 size(width, font_size * WELCOME_CARD_HEIGHT_EM);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool   is_dark = settings.GetUserSettings().display_settings.use_dark_mode;
    ImGui::PushID(id);
    ImGui::InvisibleButton("resource_card", size);
    const bool clicked = ImGui::IsItemClicked();
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const float rounding = settings.GetDefaultStyle().ChildRounding + font_size * 0.12f;
    const ImU32 accent = settings.GetColor(link.color);

    draw_list->AddRectFilled(ImVec2(pos.x + 2.0f, pos.y + 3.0f),
                             ImVec2(pos.x + size.x + 2.0f, pos.y + size.y + 3.0f),
                             is_dark ? IM_COL32(0, 0, 0, 62)
                                     : IM_COL32(72, 98, 130, 16),
                             rounding);
    draw_list->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                             WelcomeColor(settings, Colors::kBgFrame,
                                          hovered ? (is_dark ? 0.48f : 0.92f)
                                                  : (is_dark ? 0.28f : 0.66f)),
                             rounding);
    draw_list->AddRectFilledMultiColor(
        pos, ImVec2(pos.x + size.x, pos.y + size.y),
        ApplyAlpha(accent, hovered ? 0.17f : 0.10f), IM_COL32(0, 0, 0, 0),
        IM_COL32(0, 0, 0, 0), ApplyAlpha(accent, hovered ? 0.11f : 0.06f));
    draw_list->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                       hovered ? accent
                               : WelcomeColor(settings, Colors::kBorderColor,
                                              is_dark ? 0.44f : 0.72f),
                       rounding);

    draw_list->AddCircleFilled(ImVec2(pos.x + font_size * 1.12f,
                                      pos.y + font_size * 1.20f),
                               font_size * 0.24f, accent);
    const ImVec2 text_pos(pos.x + font_size * 1.85f, pos.y + font_size * 0.72f);
    draw_list->PushClipRect(text_pos,
                            ImVec2(pos.x + size.x - font_size * 0.75f,
                                   pos.y + size.y - font_size * 0.45f), true);
    draw_list->AddText(text_pos, settings.GetColor(Colors::kTextMain), link.title);
    draw_list->AddText(ImVec2(text_pos.x, text_pos.y + font_size * 1.45f),
                       settings.GetColor(Colors::kTextDim), link.description);
    draw_list->AddText(ImVec2(text_pos.x, pos.y + size.y - font_size * 1.55f),
                       settings.GetColor(Colors::kAccentRed), "Open resource");
    draw_list->PopClipRect();

    if(clicked) open_url(link.url);
    if(hovered) SetTooltipStyled("%s", link.url);
    ImGui::PopID();
}

void
BeginWelcomeTile(SettingsManager& settings, const char* id, const char* title,
                 const char* subtitle, bool is_dark_mode, float width = 0.0f)
{
    const float font_size = ImGui::GetFontSize();
    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          ApplyAlpha(settings.GetColor(Colors::kBgPanel),
                                     is_dark_mode ? 0.42f : 0.66f));
    ImGui::PushStyleColor(ImGuiCol_Border,
                          ApplyAlpha(settings.GetColor(Colors::kBorderColor), 0.55f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,
                        settings.GetDefaultStyle().ChildRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(font_size * 1.15f, font_size * 0.85f));
    ImGui::BeginChild(id, ImVec2(width, 0.0f),
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if(title && title[0] != '\0')
    {
        ImFont* tile_font = settings.GetFontManager().GetFont(FontType::kMedLarge);
        if(tile_font) ImGui::PushFont(tile_font);
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextMain));
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();
        if(tile_font) ImGui::PopFont();
        if(subtitle && subtitle[0] != '\0')
        {
            ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
            ImGui::TextUnformatted(subtitle);
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
        }

        const ImVec2 sep_pos = ImGui::GetCursorScreenPos();
        const float  sep_w   = ImGui::GetContentRegionAvail().x;
        ImGui::GetWindowDrawList()->AddLine(
            sep_pos, ImVec2(sep_pos.x + sep_w, sep_pos.y),
            ApplyAlpha(settings.GetColor(Colors::kBorderColor), 0.55f), 1.0f);
        ImGui::Dummy(ImVec2(sep_w, font_size * 0.38f));
    }
}

void
EndWelcomeTile()
{
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}
}  // namespace

AppWindow* AppWindow::s_instance = nullptr;

AppWindow*
AppWindow::GetInstance()
{
    if(!s_instance)
    {
        s_instance = new AppWindow();
    }

    return s_instance;
}

void
AppWindow::DestroyInstance()
{
    if(s_instance)
    {
        delete s_instance;
        s_instance = nullptr;
    }
}

AppWindow::AppWindow()
: m_main_view(nullptr)
, m_settings_panel(nullptr)
, m_tab_container(nullptr)
, m_default_padding(0.0f, 0.0f)
, m_default_spacing(0.0f, 0.0f)
, m_open_about_dialog(false)
, m_tabclosed_event_token(static_cast<EventManager::SubscriptionToken>(-1))
, m_tabselected_event_token(static_cast<EventManager::SubscriptionToken>(-1))
#ifdef ROCPROFVIS_DEVELOPER_MODE
, m_show_debug_window(false)
, m_show_provider_test_widow(false)
, m_show_metrics(false)
#endif
, m_confirmation_dialog(std::make_unique<ConfirmationDialog>(
      SettingsManager::GetInstance().GetUserSettings().dont_ask_before_exit))
, m_message_dialog(std::make_unique<MessageDialog>())
, m_tool_bar_index(0)
, m_is_fullscreen(false)
, m_file_dialog_preference(kRocProfVisViewFileDialog_Auto)
, m_use_native_file_dialog(false)
, m_init_file_dialog(false)
#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
, m_is_native_file_dialog_open(false)
#endif
, m_disable_app_interaction(false)
, m_shutdown_requested(false)
, m_exit_notification_sent(false)
, m_restore_fullscreen_later(false)
, m_next_provider_cleanup_id(0)
{}

AppWindow::~AppWindow()
{
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kTabClosed),
                                             m_tabclosed_event_token);
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kTabSelected),
                                             m_tabselected_event_token);
    for(auto& job : m_provider_cleanup_jobs)
    {
        if(job.future.valid())
        {
            job.future.get();
        }
    }
    m_provider_cleanup_jobs.clear();
    m_projects.clear();
}

bool
AppWindow::Init()
{
    std::string config_path = get_application_config_path(true);

    std::filesystem::path ini_path     = std::filesystem::path(config_path) / "imgui.ini";
    ImGuiIO&              io           = ImGui::GetIO();
    static std::string    ini_path_str = ini_path.string();
    io.IniFilename                     = ini_path_str.c_str();

    ImPlot::CreateContext();

    SettingsManager& settings = SettingsManager::GetInstance();
    bool             result   = settings.Init();
    if(result)
    {
        m_settings_panel = std::make_unique<SettingsPanel>(settings);
    }
    else
    {
        spdlog::warn("Failed to initialize SettingsManager");
    }

    LayoutItem status_bar_item(-1, STATUS_BAR_HEIGHT);
    status_bar_item.m_item = std::make_shared<RocWidget>();
    LayoutItem main_area_item(-1, -STATUS_BAR_HEIGHT);
    LayoutItem tool_bar_item(-1, 0);
    tool_bar_item.m_child_flags = ImGuiChildFlags_AutoResizeY;

    m_tab_container = std::make_shared<TabContainer>();
    m_tab_container->SetEventSourceName(TAB_CONTAINER_SRC_NAME);
    m_tab_container->EnableSendCloseEvent(true);
    m_tab_container->EnableSendChangeEvent(true);

    main_area_item.m_item = std::make_shared<RocCustomWidget>([this]() {
        if(m_shutdown_requested)
        {
            RenderShutdownState();
        }
        else if(m_tab_container && !m_tab_container->GetTabs().empty())
        {
            m_tab_container->Render();
        }
        else
        {
            RenderEmptyState();
        }
    });

    std::vector<LayoutItem> layout_items;
    layout_items.push_back(tool_bar_item);
    m_tool_bar_index = static_cast<int>(layout_items.size() - 1);
    layout_items.push_back(main_area_item);
    layout_items.push_back(status_bar_item);
    m_main_view = std::make_shared<VFixedContainer>(layout_items);

    m_default_padding = ImGui::GetStyle().WindowPadding;
    m_default_spacing = ImGui::GetStyle().ItemSpacing;

    auto new_tab_closed_handler = [this](std::shared_ptr<RocEvent> e) {
        this->HandleTabClosed(e);
    };
    m_tabclosed_event_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTabClosed), new_tab_closed_handler);

    auto new_tab_selected_handler = [this](std::shared_ptr<RocEvent> e) {
        this->HandleTabSelectionChanged(e);
    };

    m_tabselected_event_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTabSelected), new_tab_selected_handler);

    ConfigureFileDialogBackend();

    return result;
}

void
AppWindow::ConfigureFileDialogBackend()
{
    bool want_native = false;
    switch(m_file_dialog_preference)
    {
        case kRocProfVisViewFileDialog_ImGui:
            want_native = false;
            break;
        case kRocProfVisViewFileDialog_Native:
            want_native = true;
            break;
        case kRocProfVisViewFileDialog_Auto:
        default:
#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
            want_native = !is_remote_display_session();
#else
            want_native = false;
#endif
            break;
    }

#ifndef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
    if(want_native)
    {
        spdlog::warn("--file-dialog=native requested but native dialog was "
                     "not compiled in; using ImGui.");
        want_native = false;
    }
#else
    if(want_native)
    {
        nfdresult_t nfd_result = NFD_Init();
        if(nfd_result != NFD_OKAY)
        {
            const char* err = NFD_GetError();
            spdlog::warn("NFD_Init failed ({}); falling back to in-process "
                         "ImGui file dialog.",
                         err ? err : "unknown");
            NFD_ClearError();
            want_native = false;
        }
        else
        {
            NFD_Quit();
        }
    }
#endif

    m_use_native_file_dialog.store(want_native);
    spdlog::info("File dialog backend: {}",
                 want_native ? "system file dialog" : "in-process ImGuiFileDialog");
}

void
AppWindow::SetFileDialogPreference(rocprofvis_view_file_dialog_preference_t pref)
{
    m_file_dialog_preference = pref;
}

void
AppWindow::SetNotificationCallback(std::function<void(int)> callback)
{
    m_notification_callback = std::move(callback);
}

const std::string&
AppWindow::GetMainTabSourceName() const
{
    return m_tab_container->GetEventSourceName();
}

void
AppWindow::SetTabLabel(const std::string& label, const std::string& id)
{
    m_tab_container->SetTabLabel(label, id);
}

void
AppWindow::ShowCloseConfirm()
{
    if(m_shutdown_requested)
    {
        RequestExitIfProviderCleanupsComplete();
        return;
    }

    if(m_tab_container->GetTabs().size() == 0 ||
       SettingsManager::GetInstance().GetUserSettings().dont_ask_before_exit)
    {
        BeginAppShutdown();
        return;
    }

    // Only show the dialog if there are open tabs
    ShowConfirmationDialog(
        "Confirm Close",
        "Are you sure you want to close the application? Any "
        "unsaved data will be lost.",
        [this]() { BeginAppShutdown(); });
}

void
AppWindow::SetFullscreenState(bool is_fullscreen)
{
    m_is_fullscreen = is_fullscreen;
}

bool
AppWindow::GetFullscreenState() const
{
    return m_is_fullscreen;
}

void
AppWindow::ShowConfirmationDialog(const std::string& title, const std::string& message,
                                  std::function<void()> on_confirm_callback) const
{
    m_confirmation_dialog->Show(title, message, on_confirm_callback);
}

void
AppWindow::ShowMessageDialog(const std::string& title, const std::string& message) const
{
    m_message_dialog->Show(title, message);
}

void
AppWindow::ShowSaveFileDialog(const std::string& title, const std::vector<FileFilter>& file_filters,
                              const std::string&               initial_path,
                              std::function<void(std::string)> callback)
{
#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
    if(m_use_native_file_dialog.load())
    {
        (void)title;
        ShowNativeFileDialog(file_filters, initial_path, callback, true);
        return;
    }
#endif
    ShowImGuiFileDialog(title, file_filters, initial_path, true, callback);
}

void
AppWindow::ShowOpenFileDialog(const std::string& title, const std::vector<FileFilter>& file_filters,
                              const std::string&               initial_path,
                              std::function<void(std::string)> callback)
{
#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
    if(m_use_native_file_dialog.load())
    {
        (void)title;
        ShowNativeFileDialog(file_filters, initial_path, callback, false);
        return;
    }
#endif
    ShowImGuiFileDialog(title, file_filters, initial_path, false, callback);
}

Project*
AppWindow::GetProject(const std::string& id)
{
    Project* project = nullptr;
    if(m_projects.count(id) > 0)
    {
        project = m_projects[id].get();
    }
    return project;
}

Project*
AppWindow::GetCurrentProject()
{
    Project*       project    = nullptr;
    const TabItem* active_tab = m_tab_container->GetActiveTab();
    if(active_tab)
    {
        project = GetProject(active_tab->m_id);
    }
    return project;
}

void
AppWindow::BeginAppShutdown()
{
    if(m_shutdown_requested)
    {
        RequestExitIfProviderCleanupsComplete();
        return;
    }

    m_shutdown_requested      = true;
    m_disable_app_interaction = true;

    NotificationManager::GetInstance().ShowPersistent(
        APP_SHUTDOWN_NOTIFICATION_ID,
        "Closing traces... " + std::to_string(m_provider_cleanup_jobs.size()) +
            " cleanup job(s) remaining",
        NotificationLevel::Info);

    for(auto& item : m_projects)
    {
        if(item.second)
        {
            item.second->Close();
            DetachProjectProviderCleanup(*item.second,
                                         ProviderCleanupReason::kAppShutdown);
        }
    }

#ifdef ROCPROFVIS_DEVELOPER_MODE
    StartProviderCleanup(m_test_data_provider.DetachCleanupWork(),
                         "developer data provider",
                         ProviderCleanupReason::kAppShutdown);
#endif

    m_projects.clear();
    if(m_main_view)
    {
        m_main_view->GetMutableAt(m_tool_bar_index)->m_item = nullptr;
    }

    if(!m_provider_cleanup_jobs.empty())
    {
        NotificationManager::GetInstance().ShowPersistent(
            APP_SHUTDOWN_NOTIFICATION_ID, "Closing traces...",
            NotificationLevel::Info);
    }

    RequestExitIfProviderCleanupsComplete();
}

void
AppWindow::DetachProjectProviderCleanup(Project& project, ProviderCleanupReason reason)
{
    std::shared_ptr<RootView> root_view =
        std::dynamic_pointer_cast<RootView>(project.GetView());
    if(!root_view)
    {
        return;
    }

    std::optional<DataProviderCleanupWork> cleanup_work =
        root_view->DetachProviderCleanup();
    if(cleanup_work)
    {
        StartProviderCleanup(std::move(*cleanup_work), project.GetName(), reason);
    }
}

void
AppWindow::StartProviderCleanup(DataProviderCleanupWork cleanup_work,
                                const std::string&    label,
                                ProviderCleanupReason reason)
{
    if(cleanup_work.requests.empty() && !cleanup_work.controller)
    {
        return;
    }

    const std::string cleanup_label =
        label.empty() ? cleanup_work.trace_file_path : label;
    ProviderCleanupJob job;
    job.label           = cleanup_label;
    job.reason          = reason;
    job.notification_id = "provider_cleanup_" +
                          std::to_string(++m_next_provider_cleanup_id);

    const std::string message =
        "Closing trace: " + cleanup_label + ", canceling " +
        std::to_string(cleanup_work.requests.size()) + " request(s)";
    NotificationManager::GetInstance().ShowPersistent(job.notification_id, message,
                                                      NotificationLevel::Info);

    job.future = std::async(
        std::launch::async,
        [cleanup_work = std::move(cleanup_work)]() mutable {
            return DataProvider::CleanupDetachedResources(std::move(cleanup_work));
        });
    m_provider_cleanup_jobs.push_back(std::move(job));
}

void
AppWindow::UpdateProviderCleanups()
{
    for(auto it = m_provider_cleanup_jobs.begin(); it != m_provider_cleanup_jobs.end();)
    {
        if(it->future.valid() &&
           it->future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            DataProviderCleanupResult result = it->future.get();
            spdlog::info("Provider cleanup completed for {} ({} request(s))",
                         result.trace_file_path.empty() ? it->label
                                                        : result.trace_file_path,
                         result.request_count);
            NotificationManager::GetInstance().Hide(it->notification_id);
            it = m_provider_cleanup_jobs.erase(it);
        }
        else
        {
            ++it;
        }
    }

    if(m_shutdown_requested)
    {
        if(m_provider_cleanup_jobs.empty())
        {
            NotificationManager::GetInstance().Hide(APP_SHUTDOWN_NOTIFICATION_ID);
        }
        RequestExitIfProviderCleanupsComplete();
    }
}

void
AppWindow::RequestExitIfProviderCleanupsComplete()
{
    if(!m_shutdown_requested || m_exit_notification_sent ||
       !m_provider_cleanup_jobs.empty())
    {
        return;
    }

    m_exit_notification_sent = true;
    m_disable_app_interaction = false;
    if(m_notification_callback)
    {
        m_notification_callback(
            rocprofvis_view_notification_t::kRocProfVisViewNotification_Exit_App);
    }
}

void
AppWindow::Update()
{
#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
    UpdateNativeFileDialog();
#endif
    UpdateProviderCleanups();
    if(m_shutdown_requested)
    {
        return;
    }

    HotkeyManager::GetInstance().ProcessInput();
    EventManager::GetInstance()->DispatchEvents();
    DebugWindow::GetInstance()->ClearTransient();
    m_tab_container->Update();
#ifdef ROCPROFVIS_DEVELOPER_MODE
    m_test_data_provider.Update();
#endif
}

void
AppWindow::Render()
{
    Update();

#ifdef ROCPROFVIS_ENABLE_INTERNAL_BANNER
    DrawInternalBuildBanner("Evaluation Build");
#endif
#ifdef IMGUI_HAS_VIEWPORT
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
#else
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
#endif

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    ImGui::Begin("Main Window", nullptr,
                 ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14, m_default_spacing.y));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 6));
    if(ImGui::BeginMenuBar())
    {
        Project* project = GetCurrentProject();
        RenderFileMenu(project);
        RenderEditMenu(project);
        RenderViewMenu(project);
        RenderHelpMenu();
#ifdef ROCPROFVIS_DEVELOPER_MODE
        RenderDeveloperMenu();
#endif
        ImGui::EndMenuBar();
    }
    ImGui::PopStyleVar(3);  // ItemSpacing, WindowPadding, FramePadding

    if(m_main_view)
    {
        m_main_view->Render();
    }

    if(m_open_about_dialog)
    {
        ImGui::OpenPopup(ABOUT_DIALOG_NAME);
        m_open_about_dialog = false;  // Reset the flag after opening the dialog
    }
    RenderAboutDialog();  // Popup dialogs need to be rendered as part of the main window
    m_confirmation_dialog->Render();
    m_message_dialog->Render();
    m_settings_panel->Render();

    ImGui::End();
    // Pop ImGuiStyleVar_ItemSpacing, ImGuiStyleVar_WindowPadding,
    // ImGuiStyleVar_WindowRounding
    ImGui::PopStyleVar(3);

    RenderFileDialog();
#ifdef ROCPROFVIS_DEVELOPER_MODE
    RenderDebugOuput();
#endif

    // render notifications last
    NotificationManager::GetInstance().Render();

    RenderDisableScreen();
}

void
AppWindow::RenderEmptyState()
{
    SettingsManager&              settings     = SettingsManager::GetInstance();
    const InternalSettings&       internal     = settings.GetInternalSettings();
    const std::list<std::string>& recent_files = internal.recent_files;
    const float                   font_size    = ImGui::GetFontSize();
    const bool                    is_dark_mode =
        settings.GetUserSettings().display_settings.use_dark_mode;
    std::string recent_file_to_open;

    static const WelcomeLink resource_links[] = {
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

    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("welcome_page", ImVec2(0.0f, 0.0f), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImVec2 page_pos  = ImGui::GetWindowPos();
    const ImVec2 page_size = ImGui::GetWindowSize();
    ImDrawList*  draw_list = ImGui::GetWindowDrawList();
    DrawWelcomeBackdrop(draw_list, page_pos, page_size, is_dark_mode);
    DrawWelcomeBackgroundLogo(draw_list, page_pos, page_size, is_dark_mode);

    const float edge_margin =
        std::max(font_size * 1.35f, std::min(font_size * 2.4f, page_size.x * 0.035f));
    const float max_content_w = font_size * WELCOME_CONTENT_MAX_EM;
    const float content_w     = std::min(
        std::max(0.0f, page_size.x - edge_margin * 2.0f), max_content_w);
    const bool is_compact = content_w < font_size * 62.0f;
    ImGui::SetCursorPos(ImVec2((page_size.x - content_w) * 0.5f, edge_margin));
    ImGui::BeginChild("welcome_body", ImVec2(content_w, 0.0f), false,
                      ImGuiWindowFlags_NoBackground);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(font_size * 0.95f, font_size * 0.95f));

    const ImVec2 hero_padding(font_size * 1.85f, font_size * 1.55f);
    const float  hero_text_w =
        is_compact ? content_w - hero_padding.x * 2.0f : content_w * 0.55f;
    const float hero_feature_w =
        is_compact ? hero_text_w
                   : std::max(font_size * 18.0f,
                              content_w - hero_text_w - hero_padding.x * 3.0f);

    const ImVec2 hero_pos = ImGui::GetCursorScreenPos();

    const float hero_h =
        is_compact ? font_size * 17.0f : font_size * 12.4f;
    const float hero_round = settings.GetDefaultStyle().ChildRounding + font_size * 0.35f;
    draw_list->AddRectFilled(ImVec2(hero_pos.x + 3.0f, hero_pos.y + 4.0f),
                             ImVec2(hero_pos.x + content_w + 3.0f,
                                    hero_pos.y + hero_h + 4.0f),
                             is_dark_mode ? IM_COL32(0, 0, 0, 90)
                                          : IM_COL32(70, 98, 130, 18),
                             hero_round);
    draw_list->AddRectFilled(hero_pos, ImVec2(hero_pos.x + content_w,
                                              hero_pos.y + hero_h),
                             WelcomeColor(settings, Colors::kBgPanel,
                                          is_dark_mode ? 0.50f : 0.70f),
                             hero_round);
    draw_list->AddRectFilledMultiColor(
        hero_pos, ImVec2(hero_pos.x + content_w, hero_pos.y + hero_h),
        WelcomeColor(settings, Colors::kAccentRed, is_dark_mode ? 0.18f : 0.11f),
        WelcomeColor(settings, Colors::kLineChartColor, is_dark_mode ? 0.16f : 0.10f),
        IM_COL32(0, 0, 0, 0), WelcomeColor(settings, Colors::kBgFrame,
                                           is_dark_mode ? 0.28f : 0.42f));
    draw_list->AddRect(hero_pos, ImVec2(hero_pos.x + content_w, hero_pos.y + hero_h),
                       WelcomeColor(settings, Colors::kBorderColor,
                                    is_dark_mode ? 0.54f : 0.82f),
                       hero_round);

    const ImVec2 hero_text_origin(hero_pos.x + hero_padding.x,
                                  hero_pos.y + hero_padding.y);

    ImGui::SetCursorScreenPos(hero_text_origin);
    DrawWelcomePill(settings, "ROCm Optiq Beta");

    ImGui::SetCursorScreenPos(
        ImVec2(hero_text_origin.x, ImGui::GetCursorScreenPos().y + font_size * 0.35f));
    if(ImFont* title_font = settings.GetFontManager().GetFont(FontType::kLarge))
    {
        ImGui::PushFont(title_font);
        ImGui::TextUnformatted("Visualize GPU performance with clarity");
        ImGui::PopFont();
    }
    else
    {
        ImGui::TextUnformatted("Visualize GPU performance with clarity");
    }

    ImGui::SetCursorScreenPos(
        ImVec2(hero_text_origin.x, ImGui::GetCursorScreenPos().y + font_size * 0.25f));
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + hero_text_w);
    ImGui::TextUnformatted(
        "Open a ROCm Systems or Compute Profiler trace and move from first glance "
        "to bottleneck insight with timelines, utilization views, annotations, and "
        "project sessions.");
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();

    const float feature_card_h = font_size * 4.4f;
    if(is_compact)
    {
        ImGui::SetCursorScreenPos(
            ImVec2(hero_text_origin.x, ImGui::GetCursorScreenPos().y + font_size * 0.55f));
        DrawWelcomeHeroFeature(settings, "ROCm Systems Profiler",
                               "System-wide timelines, runtime activity, and GPU queues.",
                               Colors::kLineChartColor, hero_text_w);
        ImGui::SetCursorScreenPos(
            ImVec2(hero_text_origin.x, ImGui::GetCursorScreenPos().y + font_size * 0.32f));
        DrawWelcomeHeroFeature(settings, "ROCm Compute Profiler",
                               "Kernel, memory, and hardware counter analysis.",
                               Colors::kEventHighlight, hero_text_w);
    }
    else
    {
        const float feature_x =
            hero_pos.x + content_w - hero_feature_w - hero_padding.x;
        const float feature_total_h = feature_card_h * 2.0f + font_size * 0.5f;
        const float feature_y =
            hero_pos.y + (hero_h - feature_total_h) * 0.5f;
        ImGui::SetCursorScreenPos(ImVec2(feature_x, feature_y));
        DrawWelcomeHeroFeature(settings, "ROCm Systems Profiler",
                               "System-wide timelines, runtime activity, and GPU queues.",
                               Colors::kLineChartColor, hero_feature_w);
        ImGui::SetCursorScreenPos(
            ImVec2(feature_x, feature_y + feature_card_h + font_size * 0.5f));
        DrawWelcomeHeroFeature(settings, "ROCm Compute Profiler",
                               "Kernel, memory, and hardware counter analysis.",
                               Colors::kEventHighlight, hero_feature_w);
    }

    ImGui::SetCursorScreenPos(ImVec2(hero_pos.x, hero_pos.y + hero_h));

    ImGui::Dummy(ImVec2(0.0f, font_size * 0.45f));

    const float column_gap = font_size * 0.72f;
    const float main_col_w = is_compact ? content_w
                                        : std::max(font_size * 30.0f,
                                                   content_w * 0.38f);
    const float side_col_w = is_compact ? content_w
                                        : std::max(0.0f,
                                                   content_w - main_col_w - column_gap);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
    ImGui::BeginChild("welcome_main_col", ImVec2(main_col_w, 0.0f),
                      ImGuiChildFlags_AutoResizeY,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    BeginWelcomeTile(settings, "tile_start", "Start",
                     "Choose a workflow or drop a trace onto the app window.",
                     is_dark_mode);
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                            ImVec2(0.0f, font_size * 0.25f));
        const float inner_w = ImGui::GetContentRegionAvail().x;
        if(DrawWelcomeAction(settings, "Open Trace File...",
                             "Open a .db, .rpd, or .yaml file.",
                             "open_trace", inner_w, true, true))
        {
            HandleOpenFile();
        }
        if(ImGui::IsItemHovered())
        {
            SetTooltipStyled("%s", SUPPORTED_FILE_TYPES_HINT);
        }
        DrawWelcomeAction(settings, "Drag & Drop",
                          "Drop a trace file onto the window.",
                          "drag_drop", inner_w, false);
        ImGui::PopStyleVar();
    }
    EndWelcomeTile();

    ImGui::Dummy(ImVec2(0.0f, font_size * 0.30f));

    BeginWelcomeTile(settings, "tile_recent", "Recent",
                     "Resume a previous trace or project.",
                     is_dark_mode);
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
                                ImVec2(0.0f, font_size * 0.45f));
            int shown = 0;
            for(const std::string& file : recent_files)
            {
                if(shown++ >= static_cast<int>(MAX_RECENT_FILES)) break;

                if(DrawWelcomeRecentRow(settings, file, inner_w))
                {
                    recent_file_to_open = file;
                }
            }
            ImGui::PopStyleVar();
        }
    }
    EndWelcomeTile();

    ImGui::EndChild();  // welcome_main_col
    ImGui::PopStyleColor();

    if(!is_compact)
    {
        ImGui::SameLine(0.0f, column_gap);
    }
    else
    {
        ImGui::Dummy(ImVec2(0.0f, font_size * 0.30f));
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
    ImGui::BeginChild("welcome_side_col", ImVec2(side_col_w, 0.0f),
                      ImGuiChildFlags_AutoResizeY,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    BeginWelcomeTile(settings, "tile_resources", "Resources",
                     "Jump into documentation, source, and ROCm ecosystem pages.",
                     is_dark_mode);
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                            ImVec2(font_size * 0.55f, font_size * 0.30f));
        const float resource_gap = font_size * 0.55f;
        const float inner_w      = ImGui::GetContentRegionAvail().x;
        const bool two_col_resources = inner_w > font_size * 38.0f;
        const float resource_width =
            two_col_resources ? (inner_w - resource_gap) * 0.5f : inner_w;
        constexpr size_t resource_count =
            sizeof(resource_links) / sizeof(resource_links[0]);
        for(size_t i = 0; i < resource_count; ++i)
        {
            DrawWelcomeLinkCard(settings, resource_links[i], resource_links[i].title,
                                resource_width);
            if(two_col_resources && i % 2 == 0 && i + 1 < resource_count)
            {
                ImGui::SameLine(0.0f, resource_gap);
            }
        }
        ImGui::PopStyleVar();
    }
    EndWelcomeTile();

    ImGui::EndChild();  // welcome_side_col
    ImGui::PopStyleColor();

    ImGui::PopStyleVar();   // ItemSpacing for welcome_body
    ImGui::EndChild();      // welcome_body
    ImGui::EndChild();      // welcome_page
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    if(!recent_file_to_open.empty())
    {
        HandleOpenRecentFile(recent_file_to_open);
    }
}

void
AppWindow::RenderShutdownState()
{
    ImGui::OpenPopup(SHUTDOWN_DIALOG_NAME);

    const float dpi = SettingsManager::GetInstance().GetDPI();
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
               viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
        ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(360.0f * dpi, 0.0f), ImGuiCond_Always);

    PopUpStyle ps;
    ps.PushPopupStyles();
    ps.CenterPopup();
    if(ImGui::BeginPopupModal(SHUTDOWN_DIALOG_NAME, nullptr,
                              ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoCollapse))
    {
        if(m_provider_cleanup_jobs.empty())
        {
            CenterNextTextItem(CLOSING_MESSAGE);
            ImGui::TextUnformatted(CLOSING_MESSAGE);
        }
        else
        {
            CenterNextTextItem(CLEANUP_MESSAGE);
            ImGui::TextUnformatted(CLEANUP_MESSAGE);
            ImGui::Spacing();
            // Draw indicator dots to show that the app is still responsive
            RenderLoadingIndicator(SettingsManager::GetInstance().GetColor(Colors::kTextMain),
                                   nullptr, kCenterHorizontal);
            ImGui::Spacing();
            const std::string remaining_message =
                "Cleanup jobs remaining: " +
                std::to_string(m_provider_cleanup_jobs.size());
            CenterNextTextItem(remaining_message.c_str());
            ImGui::TextUnformatted(remaining_message.c_str());
        }
        ImGui::EndPopup();
    }
    ps.PopStyles();
}

void
AppWindow::RenderFileDialog()
{
    if(!ImGuiFileDialog::Instance()->IsOpened(FILE_DIALOG_NAME))
    {
        return;  // No file dialog is opened, nothing to render
    }

    // Set Itemspacing to values from original default ImGui style
    // custom values to break the 3rd party file dialog implementation
    // especially the cell padding
    auto defaultStyle = SettingsManager::GetInstance().GetDefaultIMGUIStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, defaultStyle.ItemSpacing);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, defaultStyle.WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, defaultStyle.CellPadding);

    if(m_init_file_dialog)
    {
        // Basically ImGuiCond_Appearing, except overwrite confirmation is a popup
        // ontop of dialog which triggers ImGuiCond_Appearing, thus flag cannot be used.
        ImGui::SetNextWindowPos(
            ImVec2(m_default_spacing.x, m_default_spacing.y + ImGui::GetFrameHeight()));
        ImGui::SetNextWindowSize(FILE_DIALOG_SIZE);
        m_init_file_dialog = false;
    }

    if(ImGuiFileDialog::Instance()->Display(FILE_DIALOG_NAME))
    {
        if(ImGuiFileDialog::Instance()->IsOk())
        {
            m_file_dialog_callback(
                std::filesystem::path(ImGuiFileDialog::Instance()->GetFilePathName())
                    .string());
        }
        ImGuiFileDialog::Instance()->Close();
    }
    ImGui::PopStyleVar(3);
}

void
AppWindow::OpenFile(std::string file_path)
{
    spdlog::info("Opening file: {}", file_path);

    std::unique_ptr<Project> project = std::make_unique<Project>();
    switch(project->Open(file_path))
    {
        case Project::OpenResult::Success:
        {
            TabItem tab =
                TabItem{ project->GetName(), project->GetID(), project->GetView(), true };
            m_tab_container->AddTab(std::move(tab));
            m_projects[project->GetID()] = std::move(project);
            SettingsManager::GetInstance().AddRecentFile(file_path);
            break;
        }
        case Project::OpenResult::Duplicate:
        {
            // File is already opened, just switch to that tab
            m_tab_container->SetActiveTab(file_path);
            break;
        }
        default:
        {
            SettingsManager::GetInstance().RemoveRecentFile(file_path);
            break;
        }
    }
}

void
AppWindow::HandleOpenRecentFile(const std::string& file_path)
{
    if(!std::filesystem::exists(file_path))
    {
        SettingsManager::GetInstance().RemoveRecentFile(file_path);
        ShowMessageDialog("Recent File Not Found",
                          "This recent file could not be found and was removed from the list:\n\n" +
                              file_path);
        return;
    }
    OpenFile(file_path);
}

void
AppWindow::RenderDisableScreen()
{
    if(m_shutdown_requested)
    {
        return;
    }

    if(m_disable_app_interaction)
    {
        ImGui::OpenPopup("GhostModal");
    }

    // Use a modal popup to disable interaction with the rest of the UI
    if(ImGui::IsPopupOpen("GhostModal"))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        if(ImGui::BeginPopupModal("GhostModal", NULL,
                                  ImGuiWindowFlags_NoDecoration |
                                      ImGuiWindowFlags_NoBackground))
        {
            ImGui::SetWindowSize(ImVec2(1, 1));  // As small as possible
            if(!m_disable_app_interaction)
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);
    }
}

void
AppWindow::RenderFileMenu(Project* project)
{
    bool is_open_file_dialog_open = ImGuiFileDialog::Instance()->IsOpened(FILE_DIALOG_NAME);
#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
    is_open_file_dialog_open = is_open_file_dialog_open || m_is_native_file_dialog_open.load();
#endif

    if(ImGui::BeginMenu("File"))
    {
        if(ImGui::MenuItem("Open", nullptr, false, !is_open_file_dialog_open))
        {
            HandleOpenFile();
        }
        if(ImGui::MenuItem("Save", nullptr, false,
                           !is_open_file_dialog_open && (project && project->IsProject())))
        {
            project->Save();
        }
        if(ImGui::MenuItem("Save As", nullptr, false,
                           project && project->GetTraceType() == Project::System &&
                               !is_open_file_dialog_open))
        {
            HandleSaveAsFile();
        }
        ImGui::Separator();

        {
            TraceView* trace_view = nullptr;
            bool       has_trace  = false;
            bool       cleanup_pending = false;
            if(project && project->GetTraceType() == Project::System)
            {
                trace_view = dynamic_cast<TraceView*>(project->GetView().get());
                has_trace  = (trace_view != nullptr);
                if(has_trace)
                {
                    cleanup_pending = trace_view->IsCleanupPending();
                }
            }

            std::string project_id = has_trace ? project->GetID() : "";
            auto start_cleanup = [this, trace_view, project_id](bool rebuild) {
                trace_view->CleanupDatabase(rebuild, [this, project_id]() {
                    m_tab_container->RemoveTab(project_id);
                });
            };

            bool submenu_enabled = has_trace && !cleanup_pending;
            if(ImGui::BeginMenu("Database", submenu_enabled))
            {
#ifdef ROCPROFVIS_DEVELOPER_MODE
                if(ImGui::MenuItem("Fast Cleanup"))
                {
                    start_cleanup(false);
                }
#endif
                if(ImGui::MenuItem("Full Cleanup"))
                {
                    ShowConfirmationDialog(
                        "Full Database Cleanup",
                        "This will remove all Optiq metadata present in the database, "
                        "including service tables, indexes, and rebuild "
                        "(VACUUM) the database file. This may take a while.\n\n"
                        "Continue?",
                        [start_cleanup]() { start_cleanup(true); });
                }
                ImGui::EndMenu();
            }
        }

        ImGui::Separator();
        const std::list<std::string>& recent_files =
            SettingsManager::GetInstance().GetInternalSettings().recent_files;
        if(ImGui::BeginMenu("Recent Files", !recent_files.empty()))
        {
            for(const std::string& file : recent_files)
            {
                if(ImGui::MenuItem(file.c_str(), nullptr))
                {
                    HandleOpenRecentFile(file);
                    break;
                }
            }
            ImGui::Separator();
            if(ImGui::MenuItem("Clear Recent Files"))
            {
                SettingsManager::GetInstance().ClearRecentFiles();
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if(ImGui::MenuItem("Exit"))
        {
            ShowCloseConfirm();
        }
        ImGui::EndMenu();
    }
}

void
AppWindow::RenderEditMenu(Project* project)
{
    if(ImGui::BeginMenu("Edit"))
    {
        if(project)
        {
            std::shared_ptr<RootView> root_view =
                std::dynamic_pointer_cast<RootView>(project->GetView());
            if(root_view)
            {
                root_view->RenderEditMenuOptions();
            }
        }
        if(ImGui::MenuItem("Preferences"))
        {
            m_settings_panel->Show();
        }
        ImGui::EndMenu();
    }
}

void
AppWindow::RenderViewMenu(Project* project)
{
    (void) project;

    if(ImGui::BeginMenu("View"))
    {
        AppWindowSettings& settings =
            SettingsManager::GetInstance().GetAppWindowSettings();
        if(ImGui::MenuItem("Show Tool Bar", nullptr, &settings.show_toolbar))
        {
            LayoutItem* tool_bar_item = m_main_view->GetMutableAt(m_tool_bar_index);
            if(tool_bar_item)
            {
                tool_bar_item->m_visible = settings.show_toolbar;
            }
        }
        if(ImGui::MenuItem("Fullscreen", "F11", m_is_fullscreen))
        {
            if(m_notification_callback)
            {
                m_notification_callback(
                    rocprofvis_view_notification_t::
                        kRocProfVisViewNotification_Toggle_Fullscreen);
            }
        }
        ImGui::SeparatorText("System Profiler Panels");
        if(ImGui::MenuItem("Show Advanced Details Panel", nullptr,
                           &settings.show_details_panel))
        {
            for(const auto& tab : m_tab_container->GetTabs())
            {
                auto trace_view_tab =
                    std::dynamic_pointer_cast<RocProfVis::View::TraceView>(tab->m_widget);
                if(trace_view_tab)
                    trace_view_tab->SetAnalysisViewVisibility(
                        settings.show_details_panel);
            }
        }
        if(ImGui::MenuItem("Show System Topology Panel", nullptr, &settings.show_sidebar))
        {
            for(const auto& tab : m_tab_container->GetTabs())
            {
                auto trace_view_tab =
                    std::dynamic_pointer_cast<RocProfVis::View::TraceView>(tab->m_widget);
                if(trace_view_tab)
                    trace_view_tab->SetSidebarViewVisibility(settings.show_sidebar);
            }
        }
        if(ImGui::MenuItem("Show Histogram", nullptr, &settings.show_histogram))
        {
            for(const auto& tab : m_tab_container->GetTabs())
            {
                auto trace_view_tab =
                    std::dynamic_pointer_cast<RocProfVis::View::TraceView>(tab->m_widget);
                if(trace_view_tab)
                    trace_view_tab->SetHistogramVisibility(settings.show_histogram);
            }
        }
        ImGui::MenuItem("Show Summary", nullptr, &settings.show_summary);
        ImGui::EndMenu();
    }
}

void
AppWindow::RenderHelpMenu()
{
    if(ImGui::BeginMenu("Help"))
    {
        if(ImGui::MenuItem("About"))
        {
            m_open_about_dialog = true;
        }
        ImGui::EndMenu();
    }
}

void
AppWindow::HandleOpenFile()
{
    std::vector<FileFilter> file_filters;

    FileFilter all_filter;
    all_filter.m_name       = "All Supported";
    all_filter.m_extensions = ALL_EXTENSIONS;

    FileFilter trace_filter;
    trace_filter.m_name       = "Traces";
    trace_filter.m_extensions = TRACE_EXTENSIONS;

    FileFilter project_filter;
    project_filter.m_name       = "Projects";
    project_filter.m_extensions = PROJECT_EXTENSIONS;

    file_filters.push_back(all_filter);
    file_filters.push_back(trace_filter);
    file_filters.push_back(project_filter);

    ShowOpenFileDialog(
        "Choose File", file_filters, "",
        [this](std::string file_path) -> void { this->OpenFile(file_path); });
}

void
AppWindow::HandleSaveAsFile()
{    
    Project* project = GetCurrentProject();
    if(project)
    {
        FileFilter trace_filter;
        trace_filter.m_name = "Projects";
        trace_filter.m_extensions = { "rpv" };

        std::vector<FileFilter> filters;
        filters.push_back(trace_filter);

        ShowSaveFileDialog(
            "Save as Project", filters, "",
            [project](std::string file_path) { project->SaveAs(file_path); });
    }
}

void
AppWindow::HandleTabClosed(std::shared_ptr<RocEvent> e)
{
    auto tab_closed_event = std::dynamic_pointer_cast<TabEvent>(e);
    auto project_it =
        tab_closed_event ? m_projects.find(tab_closed_event->GetTabId())
                         : m_projects.end();
    if(tab_closed_event && project_it != m_projects.end())
    {
        auto activeProject = GetCurrentProject();
        if(!activeProject)
        {
            spdlog::debug("No active project found after tab closed");
            m_main_view->GetMutableAt(m_tool_bar_index)->m_item = nullptr;
        }
        else
        {
            spdlog::debug("Active project found after tab closed: {}",
                          activeProject->GetName());
            std::shared_ptr<RootView> root_view =
                std::dynamic_pointer_cast<RootView>(activeProject->GetView());
            if(root_view)
            {
                m_main_view->GetMutableAt(m_tool_bar_index)->m_item =
                    root_view->GetToolbar();
            }
        }
        spdlog::debug("Tab closed: {}", tab_closed_event->GetTabId());
        project_it->second->Close();
        DetachProjectProviderCleanup(*project_it->second,
                                     ProviderCleanupReason::kTabClose);
        m_projects.erase(project_it);
    }
}

void
AppWindow::HandleTabSelectionChanged(std::shared_ptr<RocEvent> e)
{
    auto tab_selected_event = std::dynamic_pointer_cast<TabEvent>(e);
    if(tab_selected_event)
    {
        // Only handle the event if the tab source is the main tab source
        if(tab_selected_event->GetSourceId() == GetMainTabSourceName())
        {
            m_main_view->GetMutableAt(m_tool_bar_index)->m_item = nullptr;

            auto id = tab_selected_event->GetTabId();
            spdlog::debug("Tab selected: {}", id);
            auto project = GetProject(id);
            if(!project)
            {
                spdlog::warn("Project not found for tab: {}", id);
                return;
            }
            else
            {
                std::shared_ptr<RootView> root_view =
                    std::dynamic_pointer_cast<RootView>(project->GetView());
                if(root_view)
                {
                    m_main_view->GetMutableAt(m_tool_bar_index)->m_item =
                        root_view->GetToolbar();
                }
            }
        }
    }
}

void
AppWindow::RenderAboutDialog()
{
    static constexpr const char* NAME_LABEL = "ROCm (TM) Optiq";
    static constexpr const char* COPYRIGHT_LABEL =
        "Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.";
    static constexpr const char* DOC_LABEL = "ROCm (TM) Optiq Documentation";
    static constexpr const char* DOC_URL =
        "https://rocm.docs.amd.com/projects/roc-optiq/en/latest/";
    static const std::string VERSION_LABEL = []() {
        std::stringstream ss;
        ss << "Version " << ROCPROFVIS_VERSION_MAJOR << "." << ROCPROFVIS_VERSION_MINOR
           << "." << ROCPROFVIS_VERSION_PATCH;
        return ss.str();
    }();

    PopUpStyle popup_style;
    popup_style.PushPopupStyles();
    popup_style.PushTitlebarColors();
    popup_style.CenterPopup();

    ImGui::SetNextWindowSize(
        GetResponsiveWindowSize(ImVec2(580.0f, 0.0f), ImVec2(360.0f, 0.0f)));

    if(ImGui::BeginPopupModal(ABOUT_DIALOG_NAME, nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize |
                                  ImGuiWindowFlags_NoMove))
    {
        ImFont* large_font =
            SettingsManager::GetInstance().GetFontManager().GetFont(FontType::kLarge);
        if(large_font) ImGui::PushFont(large_font);

        ImGui::SetCursorPosX(
            (ImGui::GetWindowSize().x - ImGui::CalcTextSize(NAME_LABEL).x) * 0.5f);
        ImGui::TextUnformatted(NAME_LABEL);
        if(large_font) ImGui::PopFont();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::SetCursorPosX(
            (ImGui::GetWindowSize().x - ImGui::CalcTextSize(VERSION_LABEL.c_str()).x) *
            0.5f);
        ImGui::TextUnformatted(VERSION_LABEL.c_str());

        ImGui::Spacing();

        ImGui::SetCursorPosX(
            (ImGui::GetWindowSize().x - ImGui::CalcTextSize(COPYRIGHT_LABEL).x) * 0.5f);
        ImGui::TextUnformatted(COPYRIGHT_LABEL);

        ImGui::Spacing();

        ImGui::SetCursorPosX(
            (ImGui::GetWindowSize().x - ImGui::CalcTextSize(DOC_LABEL).x) * 0.5f);
        ImGui::TextLink(DOC_LABEL);
        if(ImGui::IsItemClicked())
        {
            open_url(DOC_URL);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float button_width =
            ImGui::CalcTextSize("Close").x + ImGui::GetStyle().FramePadding.x * 2;
        ImGui::SetCursorPosX(ImGui::GetWindowSize().x - button_width -
                             ImGui::GetStyle().ItemSpacing.x);
        if(ImGui::Button("Close"))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    popup_style.PopStyles();

 }

#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
void
AppWindow::UpdateNativeFileDialog()
{
    if(m_is_native_file_dialog_open)
    {
        if(m_file_dialog_future.valid() &&
           m_file_dialog_future.wait_for(std::chrono::seconds(0)) ==
               std::future_status::ready)
        {
            m_disable_app_interaction = false;
            std::string file_path     = m_file_dialog_future.get();
            if(!file_path.empty() && m_file_dialog_callback)
            {
                m_file_dialog_callback(file_path);
            }
            m_is_native_file_dialog_open = false;
            m_file_dialog_callback       = nullptr;

            if(m_restore_fullscreen_later)
            {
                // toggle fullscreen on if it should be restored after dialog closes
                if(!m_is_fullscreen && m_notification_callback)
                {
                    m_notification_callback(
                        rocprofvis_view_notification_t::
                            kRocProfVisViewNotification_Toggle_Fullscreen);
                }
                m_restore_fullscreen_later = false;
            }
        }
    }
}

void
AppWindow::ShowNativeFileDialog(const std::vector<FileFilter>&   file_filters,
                                const std::string&               initial_path,
                                std::function<void(std::string)> callback,
                                bool                             save_dialog)
{
    if(m_is_native_file_dialog_open)
    {
        return;
    }
    m_is_native_file_dialog_open = true;
    m_file_dialog_callback       = callback;
    m_disable_app_interaction    = true;

    if(m_is_fullscreen)
    {
        // toggle fullscreen off before opening native file dialog
        if(m_notification_callback)
        {
            m_restore_fullscreen_later = true;
            m_notification_callback(rocprofvis_view_notification_t::
                                        kRocProfVisViewNotification_Toggle_Fullscreen);
        }
    }

    auto dialog_task = [=]() -> std::string {
        nfdresult_t init_result = NFD_Init();
        if(init_result != NFD_OKAY)
        {
            const char* err = NFD_GetError();
            spdlog::error("NFD_Init failed at dialog open: {}",
                          err ? err : "unknown");
            NFD_ClearError();
            m_use_native_file_dialog.store(false);
            return std::string();
        }
        nfdu8char_t* outPath = nullptr;

        nfdu8filteritem_t*       filters = new nfdu8filteritem_t[file_filters.size()];
        std::vector<std::string> extension_stings;
        for(size_t i = 0; i < file_filters.size(); ++i)
        {
            std::string extensions_str;
            for(size_t j = 0; j < file_filters[i].m_extensions.size(); ++j)
            {
                extensions_str += file_filters[i].m_extensions[j];
                if(j < file_filters[i].m_extensions.size() - 1)
                {
                    extensions_str += ",";
                }
            }
            extension_stings.push_back(std::move(extensions_str));
        }
        for(size_t i = 0; i < file_filters.size(); ++i)
        {
            filters[i] = { file_filters[i].m_name.c_str(), extension_stings[i].c_str() };
        }

        nfdresult_t result;
        if(save_dialog)
        {
            nfdsavedialogu8args_t args = {};
            args.filterList            = filters;
            args.filterCount = static_cast<nfdfiltersize_t>(file_filters.size());
            if(!initial_path.empty())
            {
                args.defaultPath = initial_path.c_str();
            }
            result = NFD_SaveDialogU8_With(&outPath, &args);
        }
        else
        {
            nfdopendialogu8args_t args = {};
            args.filterList            = filters;
            args.filterCount = static_cast<nfdfiltersize_t>(file_filters.size());
            if(!initial_path.empty())
            {
                args.defaultPath = initial_path.c_str();
            }
            result = NFD_OpenDialogU8_With(&outPath, &args);
        }
        delete[] filters;
        std::string file_path;
        if(result == NFD_OKAY)
        {
            file_path = outPath;
            if(outPath)
            {
                std::filesystem::path p(file_path);
                if(!p.has_extension())
                {
                    file_path += "." + file_filters[0].m_extensions[0];
                }

                NFD_FreePathU8(outPath);
            }
        }
        else
        {
            spdlog::error("Error opening dialog: {}", NFD_GetError());
            if(outPath)
            {
                NFD_FreePathU8(outPath);
            }
            NFD_ClearError();
        }
        NFD_Quit();
        return file_path;
    };

#if defined(__APPLE__)
    // NSOpenPanel / NSSavePanel are AppKit objects and must be driven from the
    // main thread. Run synchronously here and hand the result to the existing
    // future-polling path via a ready promise.
    std::promise<std::string> dialog_promise;
    dialog_promise.set_value(dialog_task());
    m_file_dialog_future = dialog_promise.get_future();
#else
    m_file_dialog_future = std::async(std::launch::async, std::move(dialog_task));
#endif
}

#endif

void
AppWindow::ShowImGuiFileDialog(const std::string& title, const std::vector<FileFilter>& file_filters,
                          const std::string& initial_path, const bool& confirm_overwrite,
                          std::function<void(std::string)> callback)
{
    m_file_dialog_callback = callback;
    m_init_file_dialog     = true;

    std::stringstream filter_stream;
    for(const auto& filter : file_filters)
    {
        std::stringstream extensions;
        for(size_t i = 0; i < filter.m_extensions.size(); ++i)
        {
            extensions << "." << filter.m_extensions[i];
            if(i < filter.m_extensions.size() - 1)
            {
                extensions << ",";
            }
        }

        filter_stream << filter.m_name << " (" << extensions.str() << "){"
                      << extensions.str() << "}";
        if(&filter != &file_filters.back())
        {
            filter_stream << ",";
        }
    }

    IGFD::FileDialogConfig config;
    config.path  = initial_path;
    config.flags = confirm_overwrite
                       ? ImGuiFileDialogFlags_Default
                       : ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_HideColumnType;
    ImGuiFileDialog::Instance()->OpenDialog(FILE_DIALOG_NAME, title,
                                            filter_stream.str().c_str(), config);
}

#ifdef ROCPROFVIS_DEVELOPER_MODE
void
AppWindow::RenderDeveloperMenu()
{
    if(ImGui::BeginMenu("Developer Options"))
    {
        // Toggele ImGui's built-in metrics window
        if(ImGui::MenuItem("Show Metrics", nullptr, m_show_metrics))
        {
            m_show_metrics = !m_show_metrics;
        }
        // Toggle debug output window
        if(ImGui::MenuItem("Show Debug Output Window", nullptr, m_show_debug_window))
        {
            m_show_debug_window = !m_show_debug_window;
            if(m_show_debug_window)
            {
                ImGui::SetWindowFocus("Debug Window");
            }
        }
        // Open a file to test the DataProvider
        if(ImGui::MenuItem("Test Provider", nullptr))
        {
            std::vector<FileFilter> file_filters;
            FileFilter trace_filter;
            trace_filter.m_name       = "Traces";
            trace_filter.m_extensions = { "db", "rpd" };
            file_filters.push_back(trace_filter);
            ShowOpenFileDialog("Choose File", file_filters, "",
                               [this](std::string file_path) -> void {
                                   rocprofvis_controller_t* controller = rocprofvis_controller_alloc(file_path.c_str());
                                   if(controller)
                                   {
                                       this->m_test_data_provider.FetchTrace(controller, file_path);
                                       spdlog::info("Opening file: {}", file_path);
                                       m_show_provider_test_widow = true;
                                   }
                                   else
                                   {
                                       rocprofvis_controller_free(controller);
                                   }
                               });
        }
        ImGui::EndMenu();
    }
}

void
RenderProviderTest(DataProvider& provider)
{
    ImGui::Begin("Data Provider Test Window", nullptr, ImGuiWindowFlags_None);

    static char    track_index_buffer[64]     = "0";
    static char    end_track_index_buffer[64] = "1";  // for setting table track range
    static uint8_t group_id_counter           = 0;

    // Callback function to filter non-numeric characters
    auto NumericFilter = [](ImGuiInputTextCallbackData* data) -> int {
        if(data->EventChar < '0' || data->EventChar > '9')
        {
            // Allow backspace
            if(data->EventChar != '\b')
            {
                return 1;  // Block non-numeric characters
            }
        }
        return 0;  // Allow numeric characters
    };

    ImGui::InputText("Track index", track_index_buffer, IM_ARRAYSIZE(track_index_buffer),
                     ImGuiInputTextFlags_CallbackCharFilter, NumericFilter);

    int index = std::atoi(track_index_buffer);

    ImGui::Separator();
    ImGui::Text("Table Parameters");
    ImGui::InputText("End Track index", end_track_index_buffer,
                     IM_ARRAYSIZE(end_track_index_buffer),
                     ImGuiInputTextFlags_CallbackCharFilter, NumericFilter);

    static char row_start_buffer[64] = "-1";
    ImGui::InputText("Start Row", row_start_buffer, IM_ARRAYSIZE(row_start_buffer),
                     ImGuiInputTextFlags_CallbackCharFilter, NumericFilter);
    uint64_t start_row = std::atoi(row_start_buffer);

    static char row_count_buffer[64] = "-1";
    ImGui::InputText("Row Count", row_count_buffer, IM_ARRAYSIZE(row_count_buffer),
                     ImGuiInputTextFlags_CallbackCharFilter, NumericFilter);
    uint64_t row_count = std::atoi(row_count_buffer);

    TimelineModel& timeline = provider.DataModel().GetTimeline();
    if(ImGui::Button("Fetch Single Track Event Table"))
    {
        provider.FetchSingleTrackEventTable(index, timeline.GetStartTime(),
                                            timeline.GetEndTime(), "", "", "", start_row,
                                            row_count);
    }
    if(ImGui::Button("Fetch Multi Track Event Table"))
    {
        int                   end_index = std::atoi(end_track_index_buffer);
        std::vector<uint64_t> vect;
        for(int i = index; i < end_index; ++i)
        {
            vect.push_back(i);
        }
        provider.FetchMultiTrackEventTable(vect, timeline.GetStartTime(),
                                           timeline.GetEndTime(), "", "", "", start_row,
                                           row_count);
    }
    if(ImGui::Button("Print Event Table"))
    {
        provider.DataModel().GetTables().DumpTable(TableType::kEventTable);
    }

    if(ImGui::Button("Fetch Single Track Sample Table"))
    {
        provider.FetchSingleTrackSampleTable(index, timeline.GetStartTime(),
                                             timeline.GetEndTime(), "", start_row,
                                             row_count);
    }
    if(ImGui::Button("Fetch Multi Track Sample Table"))
    {
        int                   end_index = std::atoi(end_track_index_buffer);
        std::vector<uint64_t> vect;
        for(int i = index; i < end_index; ++i)
        {
            vect.push_back(i);
        }
        provider.FetchMultiTrackSampleTable(vect, timeline.GetStartTime(),
                                            timeline.GetEndTime(), "", start_row,
                                            row_count);
    }
    if(ImGui::Button("Print Sample Table"))
    {
        provider.DataModel().GetTables().DumpTable(TableType::kSampleTable);
    }

    ImGui::Separator();

    if(ImGui::Button("Fetch Track"))
    {
        provider.FetchTrack(index, timeline.GetStartTime(), timeline.GetEndTime(), 1000,
                            group_id_counter++);
    }

    if(ImGui::Button("Fetch Whole Track"))
    {
        provider.FetchWholeTrack(index, timeline.GetStartTime(), timeline.GetEndTime(),
                                 1000, group_id_counter++);
    }
    if(ImGui::Button("Delete Track"))
    {
        timeline.FreeTrackData(index);
    }
    if(ImGui::Button("Print Track"))
    {
        timeline.DumpTrack(index);
    }
    if(ImGui::Button("Print Track List"))
    {
        timeline.DumpMetaData();
    }

    ImGui::End();
}

void
AppWindow::RenderDebugOuput()
{
    if(m_show_metrics)
    {
        ImGui::ShowMetricsWindow(&m_show_metrics);
    }

    if(m_show_debug_window)
    {
        DebugWindow::GetInstance()->Render();
    }

    if(m_show_provider_test_widow)
    {
        RenderProviderTest(m_test_data_provider);
    }
}
#endif  // ROCPROFVIS_DEVELOPER_MODE

}  // namespace View
}  // namespace RocProfVis
