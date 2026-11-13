// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_welcome_page.h"

#include "imgui.h"
#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_hotkey_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_widget.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <list>
#include <string>
#include <utility>

namespace RocProfVis
{
namespace View
{

namespace
{
// Welcome-page-wide font bumps: the body uses the kMedLarge tier as its default
// (instead of kMedium) and the page hero title scales kLarge up by this factor.
constexpr float WELCOME_HEADER_TITLE_SCALE   = 1.2f;

// Page layout, all em units multiplied against ImGui::GetFontSize().
constexpr float WELCOME_CONTENT_MAX_EM       = 92.0f;
constexpr float WELCOME_MAIN_COL_MIN_EM      = 30.0f;
constexpr float WELCOME_MAIN_COL_FRACTION    = 0.38f;
constexpr float WELCOME_COLUMN_GAP_EM        = 0.72f;
constexpr float WELCOME_BODY_SPACING_EM      = 0.95f;
constexpr float WELCOME_EDGE_MARGIN_MIN_EM   = 1.35f;
constexpr float WELCOME_EDGE_MARGIN_MAX_EM   = 2.4f;
constexpr float WELCOME_EDGE_MARGIN_FRACTION = 0.035f;
constexpr float WELCOME_SECTION_GAP_EM       = 0.35f;
constexpr float WELCOME_PANEL_BG_ALPHA       = 0.55f;

// Tile container.
constexpr float WELCOME_TILE_PAD_X_EM           = 1.15f;
constexpr float WELCOME_TILE_PAD_Y_EM           = 0.85f;
constexpr float WELCOME_TILE_DIVIDER_THICKNESS  = 1.0f;
constexpr float WELCOME_TILE_BORDER_ALPHA_LIGHT = 0.82f;

// Primary action button (Start tile).
constexpr float WELCOME_ACTION_HEIGHT_EM     = 4.6f;
constexpr float WELCOME_ACTION_TEXT_X_EM     = 1.05f;
constexpr float WELCOME_ACTION_TEXT_Y_EM     = 0.85f;
constexpr float WELCOME_ACTION_RIGHT_PAD_EM  = 0.8f;
constexpr float WELCOME_ACTION_LINE_GAP_EM   = 1.35f;
constexpr float WELCOME_ACTION_SPACING_EM    = 0.25f;

// Resource card.
constexpr float WELCOME_CARD_HEIGHT_EM            = 6.2f;
constexpr float WELCOME_CARD_SPACING_EM           = 0.45f;
constexpr float WELCOME_CARD_ROUNDING_EM          = 0.2f;
constexpr float WELCOME_CARD_DOT_X_EM             = 1.12f;
constexpr float WELCOME_CARD_DOT_Y_EM             = 1.20f;
constexpr float WELCOME_CARD_DOT_RADIUS_EM        = 0.24f;
constexpr float WELCOME_CARD_TITLE_X_EM           = 1.85f;
constexpr float WELCOME_CARD_TITLE_Y_EM           = 0.72f;
constexpr float WELCOME_CARD_LINE_GAP_EM          = 1.30f;
constexpr float WELCOME_CARD_FOOTER_GAP_EM        = 1.65f;
constexpr float WELCOME_CARD_GITHUB_RIGHT_EM      = 1.10f;
constexpr float WELCOME_CARD_TITLE_CLIP_PAD_EM    = 0.6f;
constexpr float WELCOME_CARD_BG_ALPHA_DARK_IDLE  = 0.55f;
constexpr float WELCOME_CARD_BG_ALPHA_LIGHT_IDLE = 0.66f;
constexpr float WELCOME_CARD_BORDER_ALPHA_DARK    = 0.44f;
constexpr float WELCOME_CARD_BORDER_ALPHA_LIGHT   = 0.72f;

// Recent files list.
constexpr float WELCOME_RECENT_SPACING_EM = 0.55f;
constexpr float WELCOME_RECENT_HOVER_ALPHA = 0.28f;
constexpr float WELCOME_RECENT_ACTIVE_ALPHA = 0.40f;

// Tips & tricks.
constexpr float WELCOME_TIP_SPACING_EM = 0.35f;
constexpr float WELCOME_TIP_NAV_GAP_EM = 0.45f;

// Backdrop logo.
constexpr float WELCOME_LOGO_WIDTH_FRACTION     = 0.42f;
constexpr float WELCOME_LOGO_HEIGHT_FRACTION    = 0.56f;
constexpr float WELCOME_LOGO_OFFSET_X_FRACTION  = 0.62f;
constexpr float WELCOME_LOGO_OFFSET_Y_FRACTION  = 0.58f;
constexpr float WELCOME_LOGO_PIVOT_X            = 0.48f;
constexpr float WELCOME_LOGO_PIVOT_Y            = 0.50f;
constexpr float WELCOME_LOGO_ALPHA_TOP_DARK     = 0.12f;
constexpr float WELCOME_LOGO_ALPHA_TOP_LIGHT    = 0.14f;
constexpr float WELCOME_LOGO_ALPHA_BOTTOM_DARK  = 0.16f;
constexpr float WELCOME_LOGO_ALPHA_BOTTOM_LIGHT = 0.18f;
constexpr int   WELCOME_LOGO_POLYGON_VERTICES   = 6;

#ifdef ROCPROFVIS_PERFETTO_ENABLED
constexpr const char* SUPPORTED_FILE_TYPES_HINT =
    "Supported types: .db, .rpd, .yaml, .rpv, .json, .proto, .pftrace";
#else
constexpr const char* SUPPORTED_FILE_TYPES_HINT =
    "Supported types: .db, .rpd, .yaml, .rpv";
#endif

struct ResourceGroup
{
    const char* title;
    const char* description;
    const char* documentation_url;
    const char* github_url;
};

constexpr ResourceGroup kResourceGroups[] = {
    { "ROCm Optiq",
      "Workflow, installation, and feature guides.",
      "https://rocm.docs.amd.com/projects/roc-optiq/en/latest/",
      "https://github.com/ROCm/roc-optiq" },
    { "ROCm Systems Profiler",
      "System-wide traces, timelines, and runtime activity analysis.",
      "https://rocm.docs.amd.com/projects/rocprofiler-systems/en/latest/",
      "https://github.com/ROCm/rocm-systems/tree/develop/projects/rocprofiler-systems" },
    { "ROCm Compute Profiler",
      "Kernel, memory, and hardware counter analysis guides.",
      "https://rocm.docs.amd.com/projects/rocprofiler-compute/en/latest/",
      "https://github.com/ROCm/rocm-systems/tree/develop/projects/rocprofiler-compute" },
};

// One Welcome page tip. A {name} in the text stands for the key bound to the
// hotkey action whose settings key is name, so a remapped shortcut reads the
// way the user set it.
struct WelcomeTip
{
    const char* topic;
    const char* text;
};

// Topics alternate so that consecutive launches show different areas. Every
// menu and button name here is the label the UI shows.
constexpr WelcomeTip kWelcomeTips[] = {
    { "Getting started", "Drag a .db, .rpd, or .rpv file onto the window to open it." },
    { "Navigation",
      "{zoom_in} and {zoom_out} zoom at the mouse and {pan_left} and {pan_right} pan. "
      "Hold {speed_boost} to move twice as fast." },
    { "Large traces",
      "Start with Show Minimap on the toolbar. It maps event density across the whole "
      "trace, so you can go straight to where the work is." },
    { "Time ranges",
      "Hold {region_select} and drag on the timeline to select a range. "
      "{zoom_to_selection} zooms to it and {clear_selection} clears it." },
    { "Idle time",
      "Gaps between kernels on a busy GPU queue are time the GPU sat idle. Check the CPU "
      "thread tracks at the same moment to see why." },
    { "Bookmarks",
      "{bookmark_save_1} saves the view as bookmark 1 and {bookmark_restore_1} jumps "
      "back. Bookmark both ends of a slowdown to flip between them." },
    { "Top Events",
      "Look at total duration, not just the average: a short kernel called thousands of "
      "times can cost more than one slow kernel." },
    { "Tracks",
      "Hover a track name to see its full name, its track ID, and how many events or "
      "samples it holds." },
    { "Time ranges",
      "Select a few events and press {toggle_mark} to make their span the time range. "
      "The Event Table and Top Events then cover only that span." },
    { "Roofline",
      "Kernels left of the roofline's ridge point are memory-bound; kernels to the right "
      "of it are compute-bound." },
    { "Streams and queues",
      "Compare HIP stream tracks with HSA queue tracks: streams show what your code "
      "asked for, queues show what the GPU actually ran." },
    { "Event Table",
      "The advanced filter takes conditions like duration > 2000 to pull out outliers. "
      "Time columns are in nanoseconds." },
    { "Topology",
      "Right-click a track name and choose Reveal in topology to find it in the System "
      "Topology sidebar." },
    { "Event details",
      "Event Details lists an event's flow links and call stack. Right-click an entry "
      "and choose Go To Event to follow the chain." },
    { "Metrics", "Hover a metric name in Table View to read what it measures." },
    { "Sharing",
      "With a time range selected, Edit > Save Trace Selection writes just that range to "
      "a smaller trace that opens faster." },
    { "Search",
      "Search for several names at once by quoting each one, like \"gemm\"\"memcpy\"." },
    { "Tables",
      "Right-click a column header to hide columns or size them to fit." },
    { "Projects",
      "File > Save As writes a .rpv project that keeps your track layout, bookmarks, and "
      "annotations." },
    { "Measure",
      "The Measure button on the toolbar drops two rulers you can snap to event edges, "
      "to time the gap between two events exactly." },
    { "Kernels",
      "In the Kernel Selection Table, filter names with LIKE %gemm% and metrics with > "
      "or <, then click Apply Filters." },
    { "Queue utilization",
      "The utilization pill on a queue track covers the visible time, or only the "
      "selected range when one is set." },
    { "Flow arrows",
      "Flow Render Style on the toolbar switches arrows between fan, which shows what "
      "one event led to, and chain, which follows them in order." },
    { "Annotations",
      "Right-click the timeline and choose Add Annotation to leave a note on a track for "
      "whoever opens the project next." },
    { "Metrics",
      "In Table View, right-click a metric and choose Send metric to kernel details to "
      "compare it across every kernel." },
    { "Exporting",
      "Right-click a row in the Event Table or Top Events and choose Export To File to "
      "save it as CSV." },
    { "Tracks",
      "Track Options applies to every selected track at once. Compact Mode fits tall "
      "flame graphs into less space." },
    { "Counters",
      "Click the min or max value beside a counter track to set its scale, and the reset "
      "button to put it back." },
    { "Annotations",
      "Use Lock Position on an annotation so it can't be dragged by accident." },
    { "Roofline",
      "Turn on Scale kernel marker size to duration so the kernels that cost the most "
      "stand out." },
    { "Timeline",
      "Hover a dense cluster of events to see how many of each kind it holds." },
    { "Baseline Comparison",
      "Set a percentage threshold to highlight only the differences above it." },
    { "Topology",
      "Hiding a process in the System Topology sidebar hides every queue, stream, and "
      "thread under it." },
    { "Baseline Comparison",
      "Compare two kernels in the same workload, not just two runs of it." },
    { "Presets",
      "Pin the metrics you check on every kernel and save them as a preset to bring them "
      "back in one click." },
    { "Shortcuts",
      "Every shortcut in these tips can be changed in Edit > Preferences, and the tips "
      "show your keys." },
    { "Anywhere",
      "Optiq doesn't need ROCm installed. Copy a trace to any Windows, Linux, or macOS "
      "machine and open it there." },
    { "Command line",
      "roc-optiq -f trace.db opens a trace from a terminal, and --file-dialog imgui "
      "helps when running over SSH." },
    { "Cleanup",
      "File > Database > Full Cleanup removes the metadata Optiq adds to a trace file." },
};
constexpr size_t WELCOME_TIP_COUNT = sizeof(kWelcomeTips) / sizeof(kWelcomeTips[0]);

// The key bound to a hotkey action, found by its settings key. Empty when no
// action has that key or nothing is bound to it.
std::string
BoundKeyText(const std::string& action_key)
{
    for(size_t i = 0; i < kHotkeyActionCount; ++i)
    {
        const HotkeyActionId action = static_cast<HotkeyActionId>(i);
        if(action_key != HotkeyManager::GetActionInfo(action).key)
        {
            continue;
        }
        const HotkeyBinding binding = HotkeyManager::GetInstance().GetBinding(action);
        const ImGuiKeyChord chord =
            binding.primary != ImGuiKey_None ? binding.primary : binding.alternate;
        return chord == ImGuiKey_None ? std::string()
                                      : HotkeyManager::KeyChordToString(chord);
    }
    return std::string();
}

// Fills in a tip's {name} keys. False when one of them is unbound, so the tip is
// skipped rather than asking the user to press "None".
bool
ExpandTip(const WelcomeTip& tip, std::string& out)
{
    out.clear();
    for(const char* c = tip.text; *c != '\0'; ++c)
    {
        const char* close = *c == '{' ? std::strchr(c, '}') : nullptr;
        if(close == nullptr)
        {
            out += *c;
            continue;
        }
        const std::string key = BoundKeyText(std::string(c + 1, close));
        if(key.empty())
        {
            return false;
        }
        out += key;
        c = close;
    }
    return true;
}

// Stretches the card being drawn by exactly extra pixels. A card ends at its
// last item's bottom edge, which is one item spacing above the cursor.
void
ExtendCard(float extra)
{
    if(extra <= 0.0f)
    {
        return;
    }
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y +
                         extra);
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
}

// Dim wrapped paragraph used for subtitles and hint text.
void
DrawDimParagraph(SettingsManager& settings, const char* text, float wrap_width)
{
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap_width);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

// Theme fill plus a faded centered ROCm wedge logo behind the content.
void
DrawBackdrop(SettingsManager& settings, ImVec2 page_pos, ImVec2 page_size)
{
    ImDrawList*  draw_list = ImGui::GetWindowDrawList();
    const ImVec2 page_max(page_pos.x + page_size.x, page_pos.y + page_size.y);
    const bool   is_dark =
        settings.GetUserSettings().display_settings.use_dark_mode;

    draw_list->AddRectFilled(page_pos, page_max, settings.GetColor(Colors::kBgMain));

    // Tint the page in light mode so panels read as elevated.
    if(!is_dark)
    {
        draw_list->AddRectFilled(
            page_pos, page_max,
            ApplyAlpha(settings.GetColor(Colors::kBorderColor),
                       WELCOME_PANEL_BG_ALPHA));
    }

    // Two-wedge logo polygon coordinates (derived from the SVG asset).
    static constexpr ImVec2 kLogoTop[WELCOME_LOGO_POLYGON_VERTICES] = {
        { 197.2f, 13.7f }, { 174.7f, 13.7f }, { 161.0f, 0.0f }, { 210.9f, 0.0f },
        { 210.9f, 49.9f }, { 197.2f, 36.2f },
    };
    static constexpr ImVec2 kLogoBottom[WELCOME_LOGO_POLYGON_VERTICES] = {
        { 174.7f, 36.2f }, { 174.7f, 16.4f }, { 160.6f, 30.6f }, { 160.6f, 50.3f },
        { 180.3f, 50.3f }, { 194.4f, 36.2f },
    };
    constexpr float kMinX = 160.6f;
    constexpr float kMaxX = 210.9f;
    constexpr float kMaxY = 50.3f;
    const float     scale = std::min(
        page_size.x * WELCOME_LOGO_WIDTH_FRACTION / (kMaxX - kMinX),
        page_size.y * WELCOME_LOGO_HEIGHT_FRACTION / kMaxY);
    const ImVec2    origin(
        page_pos.x + page_size.x * WELCOME_LOGO_OFFSET_X_FRACTION -
            (kMaxX - kMinX) * scale * WELCOME_LOGO_PIVOT_X - kMinX * scale,
        page_pos.y + page_size.y * WELCOME_LOGO_OFFSET_Y_FRACTION -
            kMaxY * scale * WELCOME_LOGO_PIVOT_Y);

    auto draw_polygon = [&](const ImVec2* src, ImU32 color)
    {
        ImVec2 pts[WELCOME_LOGO_POLYGON_VERTICES];
        for(int i = 0; i < WELCOME_LOGO_POLYGON_VERTICES; ++i)
        {
            pts[i] = ImVec2(origin.x + src[i].x * scale, origin.y + src[i].y * scale);
        }
        draw_list->AddConvexPolyFilled(pts, WELCOME_LOGO_POLYGON_VERTICES, color);
    };

    draw_polygon(kLogoTop,
                 ApplyAlpha(settings.GetColor(Colors::kAccentHover),
                            is_dark ? WELCOME_LOGO_ALPHA_TOP_DARK
                                    : WELCOME_LOGO_ALPHA_TOP_LIGHT));
    draw_polygon(kLogoBottom,
                 ApplyAlpha(settings.GetColor(Colors::kAccent),
                            is_dark ? WELCOME_LOGO_ALPHA_BOTTOM_DARK
                                    : WELCOME_LOGO_ALPHA_BOTTOM_LIGHT));
}

// Product card. Each text link routes to its own URL; the card body itself is
// inert so a stray click on the panel does nothing.
void
DrawResourceGroup(SettingsManager& settings, const ResourceGroup& group, float width)
{
    const float  font_size = ImGui::GetFontSize();
    const ImVec2 size(width, font_size * WELCOME_CARD_HEIGHT_EM);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 bottom_right(pos.x + size.x, pos.y + size.y);

    ImGui::PushID(group.title);

    // Bottom row: "Open documentation" on the left, "GitHub" on the right.
    const float link_y = pos.y + size.y - font_size * WELCOME_CARD_FOOTER_GAP_EM;

    const char*  docs_text = "Open documentation";
    const ImVec2 docs_size = ImGui::CalcTextSize(docs_text);
    const ImVec2 docs_pos(pos.x + font_size * WELCOME_CARD_TITLE_X_EM, link_y);

    const char*  github_text = "GitHub";
    const ImVec2 github_size = ImGui::CalcTextSize(github_text);
    const ImVec2 github_pos(
        bottom_right.x - github_size.x - font_size * WELCOME_CARD_GITHUB_RIGHT_EM,
        link_y);

    // The links are submitted as real items so hover and clicks are suppressed
    // whenever a modal or another window covers the page. A raw rect test would
    // let both leak through to whatever is drawn underneath.
    ImGui::SetCursorScreenPos(docs_pos);
    const bool docs_clicked = ImGui::InvisibleButton("docs", docs_size);
    const bool docs_hovered = ImGui::IsItemHovered();

    ImGui::SetCursorScreenPos(github_pos);
    const bool github_clicked = ImGui::InvisibleButton("github", github_size);
    const bool github_hovered = ImGui::IsItemHovered();

    // Reserve the card's space; the body outside the links stays inert.
    ImGui::SetCursorScreenPos(pos);
    ImGui::Dummy(size);

    ImDrawList* draw_list  = ImGui::GetWindowDrawList();
    const ImU32 accent_col = settings.GetColor(Colors::kAccent);
    const float rounding   = settings.GetDefaultStyle().FrameRounding +
                             font_size * WELCOME_CARD_ROUNDING_EM;
    const bool  is_dark    = settings.GetUserSettings().display_settings.use_dark_mode;

    const float bg_alpha     = is_dark ? WELCOME_CARD_BG_ALPHA_DARK_IDLE
                                       : WELCOME_CARD_BG_ALPHA_LIGHT_IDLE;
    const float border_alpha = is_dark ? WELCOME_CARD_BORDER_ALPHA_DARK
                                       : WELCOME_CARD_BORDER_ALPHA_LIGHT;

    draw_list->AddRectFilled(
        pos, bottom_right,
        ApplyAlpha(settings.GetColor(Colors::kBgFrame), bg_alpha), rounding);
    draw_list->AddRect(
        pos, bottom_right,
        ApplyAlpha(settings.GetColor(Colors::kBorderColor), border_alpha),
        rounding);

    // Accent dot.
    const float  dot_r = font_size * WELCOME_CARD_DOT_RADIUS_EM;
    const ImVec2 dot_pos(pos.x + font_size * WELCOME_CARD_DOT_X_EM,
                         pos.y + font_size * WELCOME_CARD_DOT_Y_EM);
    draw_list->AddCircleFilled(dot_pos, dot_r, accent_col);

    // Title and description on the left.
    const ImVec2 title_pos(pos.x + font_size * WELCOME_CARD_TITLE_X_EM,
                           pos.y + font_size * WELCOME_CARD_TITLE_Y_EM);
    draw_list->PushClipRect(
        title_pos,
        ImVec2(bottom_right.x - font_size * WELCOME_CARD_TITLE_CLIP_PAD_EM,
               bottom_right.y),
        true);
    draw_list->AddText(title_pos, settings.GetColor(Colors::kTextMain), group.title);
    draw_list->AddText(
        ImVec2(title_pos.x, title_pos.y + font_size * WELCOME_CARD_LINE_GAP_EM),
        settings.GetColor(Colors::kTextDim), group.description);
    draw_list->PopClipRect();

    draw_list->AddText(docs_pos, settings.GetColor(Colors::kAccent), docs_text);
    draw_list->AddText(github_pos, settings.GetColor(Colors::kAccent), github_text);

    if(github_hovered)
    {
        SetTooltipStyled("%s", group.github_url);
    }
    else if(docs_hovered)
    {
        SetTooltipStyled("%s", group.documentation_url);
    }

    ImGui::PopID();

    if(github_clicked)
    {
        open_url(group.github_url);
    }
    else if(docs_clicked)
    {
        open_url(group.documentation_url);
    }
}

// Primary action row used in the Start tile.
bool
DrawPrimaryAction(SettingsManager& settings, const char* id, const char* title,
                  const char* subtitle, const char* tooltip, float width)
{
    const float  font_size = ImGui::GetFontSize();
    const ImVec2 size(width, font_size * WELCOME_ACTION_HEIGHT_EM);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 bottom_right(pos.x + size.x, pos.y + size.y);

    ImGui::PushID(id);
    ImGui::InvisibleButton("action", size);
    const bool clicked = ImGui::IsItemClicked();
    const bool hovered = ImGui::IsItemHovered();
    const bool active  = ImGui::IsItemActive();
    if(hovered && tooltip)
    {
        SetTooltipStyled("%s", tooltip);
    }

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const float rounding  = settings.GetDefaultStyle().FrameRounding;
    const ImU32 bg_col = active   ? settings.GetColor(Colors::kAccentActive)
                           : hovered ? settings.GetColor(Colors::kAccentHover)
                                     : settings.GetColor(Colors::kAccent);
    const ImU32 border_col =
        hovered || active ? settings.GetColor(Colors::kAccentHover)
                          : settings.GetColor(Colors::kAccent);

    draw_list->AddRectFilled(pos, bottom_right, bg_col, rounding);
    draw_list->AddRect(pos, bottom_right, border_col, rounding);

    const ImVec2 text_pos(pos.x + font_size * WELCOME_ACTION_TEXT_X_EM,
                          pos.y + font_size * WELCOME_ACTION_TEXT_Y_EM);
    draw_list->PushClipRect(
        ImVec2(text_pos.x, pos.y),
        ImVec2(bottom_right.x - font_size * WELCOME_ACTION_RIGHT_PAD_EM,
               bottom_right.y),
        true);
    draw_list->AddText(text_pos, settings.GetColor(Colors::kTextOnAccent), title);
    draw_list->AddText(
        ImVec2(text_pos.x, text_pos.y + font_size * WELCOME_ACTION_LINE_GAP_EM),
        settings.GetColor(Colors::kTextOnAccent), subtitle);
    draw_list->PopClipRect();

    ImGui::PopID();
    return clicked;
}

// Begins one outer card (box). The FlexContainer keeps each item transparent,
// so the card paints its own panel background + border. Pair with EndCard.
void
BeginCard(SettingsManager& settings, const char* id)
{
    const float font_size = ImGui::GetFontSize();
    const bool  is_dark =
        settings.GetUserSettings().display_settings.use_dark_mode;
    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          ApplyAlpha(settings.GetColor(Colors::kBgPanel),
                                     WELCOME_PANEL_BG_ALPHA));
    ImGui::PushStyleColor(ImGuiCol_Border,
                          ApplyAlpha(settings.GetColor(Colors::kBorderColor),
                                     is_dark ? WELCOME_PANEL_BG_ALPHA
                                             : WELCOME_TILE_BORDER_ALPHA_LIGHT));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,
                        settings.GetDefaultStyle().ChildRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(font_size * WELCOME_TILE_PAD_X_EM,
                               font_size * WELCOME_TILE_PAD_Y_EM));
    ImGui::BeginChild(id, ImVec2(0.0f, 0.0f),
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
}

void
EndCard()
{
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

// A section title (+ optional subtitle) and divider, drawn inside a card. A
// single card can stack several sections (e.g. Start then Recent).
void
SectionHeader(SettingsManager& settings, const char* title,
              const char* subtitle = nullptr)
{
    const float font_size = ImGui::GetFontSize();

    ImGui::PushFont(NULL, settings.GetFontManager().GetFontSize(FontSize::kLarge));
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextMain));
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    ImGui::PopFont();

    if(subtitle && subtitle[0])
    {
        DrawDimParagraph(settings, subtitle, ImGui::GetContentRegionAvail().x);
    }

    const ImVec2 sep_pos   = ImGui::GetCursorScreenPos();
    const float  sep_width = ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddLine(
        sep_pos, ImVec2(sep_pos.x + sep_width, sep_pos.y),
        ApplyAlpha(settings.GetColor(Colors::kBorderColor), WELCOME_PANEL_BG_ALPHA),
        WELCOME_TILE_DIVIDER_THICKNESS);
    ImGui::Dummy(ImVec2(sep_width, font_size * WELCOME_SECTION_GAP_EM));
}
}  // namespace

WelcomePage::WelcomePage(OpenFileFn on_open_file, OpenRecentFileFn on_open_recent)
    : m_on_open_file(std::move(on_open_file))
    , m_on_open_recent(std::move(on_open_recent))
    , m_tip_index(0)
    , m_column_heights{ 0.0f, 0.0f }
    , m_column_stretch{ 0.0f, 0.0f }
    , m_side_by_side(false)
{
    // Two flex cards: left = Start + Recent, right = Resources. The grow ratios
    // keep Resources the wider panel; the min widths (set per-frame in Render)
    // drive the wrap to a single stacked column when the window is narrow.
    auto left  = std::make_shared<RocCustomWidget>([this]() { RenderLeftCard(); });
    auto right = std::make_shared<RocCustomWidget>([this]() { RenderResourcesCard(); });
    m_flex.items = {
        { "welcome_left", left, 0.0f, 0.0f, WELCOME_MAIN_COL_FRACTION },
        { "welcome_right", right, 0.0f, 0.0f, 1.0f - WELCOME_MAIN_COL_FRACTION },
    };

    // The page is built once per launch, so this is where a launch moves on to
    // a new tip. Starting one before the saved tip and stepping forward lands on
    // it, or on the next showable one if its keys are unbound.
    const size_t saved =
        SettingsManager::GetInstance().GetInternalSettings().next_welcome_tip %
        WELCOME_TIP_COUNT;
    m_tip_index = (saved + WELCOME_TIP_COUNT - 1) % WELCOME_TIP_COUNT;
    StepTip(true);
}

void
WelcomePage::Render()
{
    SettingsManager& settings  = SettingsManager::GetInstance();
    const float      font_size = ImGui::GetFontSize();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("welcome_page", ImVec2(0.0f, 0.0f), false,
                      ImGuiWindowFlags_NoBackground);

    const ImVec2 page_pos  = ImGui::GetWindowPos();
    const ImVec2 page_size = ImGui::GetWindowSize();
    DrawBackdrop(settings, page_pos, page_size);

    // Center a max-width column; the FlexContainer collapses to one stacked
    // column on its own once the cards no longer fit side by side.
    const float edge_margin =
        std::max(font_size * WELCOME_EDGE_MARGIN_MIN_EM,
                 std::min(font_size * WELCOME_EDGE_MARGIN_MAX_EM,
                          page_size.x * WELCOME_EDGE_MARGIN_FRACTION));
    const float content_width =
        std::min(std::max(0.0f, page_size.x - edge_margin * 2.0f),
                 font_size * WELCOME_CONTENT_MAX_EM);

    ImGui::SetCursorPos(ImVec2((page_size.x - content_width) * 0.5f, edge_margin));
    ImGui::BeginChild("welcome_body", ImVec2(content_width, 0.0f),
                      ImGuiChildFlags_AutoResizeY,
                      ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
                          ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PushFont(NULL, settings.GetFontManager().GetFontSize(FontSize::kMedLarge));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(font_size * WELCOME_BODY_SPACING_EM,
                               font_size * WELCOME_BODY_SPACING_EM));

    RenderHeader();

    // Drive the two-card body through the FlexContainer (font-relative sizing).
    m_recent_file_to_open.clear();
    m_flex.gap                = font_size * WELCOME_COLUMN_GAP_EM;
    m_flex.items[0].min_width = font_size * WELCOME_MAIN_COL_MIN_EM;
    m_flex.items[1].min_width = font_size * WELCOME_MAIN_COL_MIN_EM;
    // The flex keeps both cards on one row only while both minimum widths fit.
    m_side_by_side = m_flex.items[0].min_width + m_flex.gap +
                         m_flex.items[1].min_width <=
                     ImGui::GetContentRegionAvail().x;
    m_flex.Render();

    ImGui::PopStyleVar();  // welcome_body ItemSpacing
    ImGui::PopFont();      // welcome_body default font bump
    ImGui::EndChild();     // welcome_body
    ImGui::Dummy(ImVec2(0.0f, edge_margin));
    ImGui::EndChild();     // welcome_page
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // Defer the open until we've torn down the welcome children; opening a
    // file can immediately replace this view with a project tab.
    if(!m_recent_file_to_open.empty())
    {
        m_on_open_recent(m_recent_file_to_open);
    }
}

void
WelcomePage::RenderHeader()
{
    SettingsManager& settings      = SettingsManager::GetInstance();
    const float      font_size     = ImGui::GetFontSize();
    const float      content_width = ImGui::GetContentRegionAvail().x;

    ImGui::PushFont(NULL,
                    settings.GetFontManager().GetFontSize(FontSize::kLarge) *
                        WELCOME_HEADER_TITLE_SCALE);
    ImGui::TextUnformatted("Welcome to ROCm Optiq");
    ImGui::PopFont();

    // First mention of each profiler on this page carries the trademark.
    DrawDimParagraph(
        settings,
        settings.GetFontManager().TrademarkText(
            "Unified visualization and analysis for ROCm\u2122 Systems Profiler traces "
            "and ROCm\u2122 Compute Profiler data. Identify bottlenecks, understand "
            "hardware utilization, and optimize GPU workloads."),
        content_width);

    ImGui::Dummy(ImVec2(0.0f, font_size * WELCOME_SECTION_GAP_EM));
}

void
WelcomePage::RenderLeftCard()
{
    SettingsManager& settings  = SettingsManager::GetInstance();
    const float      font_size = ImGui::GetFontSize();
    const float      start_y   = ImGui::GetCursorPosY();
    const float      stretch =
        m_side_by_side ? std::max(0.0f, m_column_heights[1] - m_column_heights[0])
                       : 0.0f;

    BeginCard(settings, "welcome_card_left");

    // Start section.
    SectionHeader(settings, "Start",
                  "Open a ROCm System or Compute Profiler data file to get started.");
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                            ImVec2(0.0f, font_size * WELCOME_ACTION_SPACING_EM));
        const float inner_w = ImGui::GetContentRegionAvail().x;
        if(DrawPrimaryAction(settings, "open_trace", "Open Trace File...",
                             "Open a .db, .rpd, .rpv or .yaml file.",
                             SUPPORTED_FILE_TYPES_HINT, inner_w))
        {
            m_on_open_file();
        }

        ImGui::Dummy(ImVec2(0.0f, font_size * WELCOME_SECTION_GAP_EM));
        DrawDimParagraph(settings,
                         "Or drag and drop a file into the window.", inner_w);
        ImGui::PopStyleVar();
    }

    // Space between the Start and Recent sections within the same card.
    ImGui::Dummy(ImVec2(0.0f, font_size * WELCOME_BODY_SPACING_EM));

    // Recent section.
    SectionHeader(settings, "Recent");
    {
        const std::list<std::string>& recent_files =
            settings.GetInternalSettings().recent_files;
        const float inner_w = ImGui::GetContentRegionAvail().x;
        if(recent_files.empty())
        {
            DrawDimParagraph(
                settings,
                "Recent projects and traces will appear here for quick access.",
                inner_w);
        }
        else
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                                ImVec2(0.0f, font_size * WELCOME_RECENT_SPACING_EM));
            ImGui::PushStyleColor(
                ImGuiCol_Header,
                ApplyAlpha(settings.GetColor(Colors::kAccent),
                           WELCOME_RECENT_HOVER_ALPHA));
            ImGui::PushStyleColor(
                ImGuiCol_HeaderHovered,
                ApplyAlpha(settings.GetColor(Colors::kAccentHover),
                           WELCOME_RECENT_HOVER_ALPHA));
            ImGui::PushStyleColor(
                ImGuiCol_HeaderActive,
                ApplyAlpha(settings.GetColor(Colors::kAccentActive),
                           WELCOME_RECENT_ACTIVE_ALPHA));
            size_t shown = 0;
            for(const std::string& file : recent_files)
            {
                if(shown >= MAX_RECENT_FILES)
                {
                    break;
                }
                ++shown;

                const std::filesystem::path fp(file);
                const std::string fname =
                    fp.filename().empty() ? file : fp.filename().string();

                ImGui::PushID(file.c_str());
                if(ImGui::Selectable(fname.c_str(), false,
                                     ImGuiSelectableFlags_None,
                                     ImVec2(inner_w, 0.0f)))
                {
                    m_recent_file_to_open = file;
                }
                if(ImGui::IsItemHovered())
                {
                    SetTooltipStyled("%s", file.c_str());
                }
                ImGui::PopID();
            }
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();
        }
    }

    EndCard();

    // The tips card sits one flex gap below, the same gap the columns keep from
    // each other, rather than the body's item spacing.
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y +
                         m_flex.gap);
    RenderTipsCard(stretch);
    m_column_heights[0] = ImGui::GetCursorPosY() - start_y - m_column_stretch[0];
    m_column_stretch[0] = stretch;
}

void
WelcomePage::RenderResourcesCard()
{
    SettingsManager& settings  = SettingsManager::GetInstance();
    const float      font_size = ImGui::GetFontSize();
    const float      start_y   = ImGui::GetCursorPosY();
    const float      stretch =
        m_side_by_side ? std::max(0.0f, m_column_heights[0] - m_column_heights[1])
                       : 0.0f;

    BeginCard(settings, "welcome_card_right");
    SectionHeader(settings, "Resources",
                  "Jump into documentation, source, and ROCm ecosystem pages.");
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                            ImVec2(0.0f, font_size * WELCOME_CARD_SPACING_EM));
        const float inner_w = ImGui::GetContentRegionAvail().x;
        for(const ResourceGroup& group : kResourceGroups)
        {
            DrawResourceGroup(settings, group, inner_w);
        }
        ExtendCard(stretch);
        ImGui::PopStyleVar();
    }
    EndCard();
    m_column_heights[1] = ImGui::GetCursorPosY() - start_y - m_column_stretch[1];
    m_column_stretch[1] = stretch;
}

void
WelcomePage::RenderTipsCard(float extra_height)
{
    SettingsManager& settings  = SettingsManager::GetInstance();
    const float      font_size = ImGui::GetFontSize();

    // A key can be unbound in Preferences while the page is up, which would
    // leave the tip on screen asking for a key that no longer does anything.
    std::string text;
    if(!ExpandTip(kWelcomeTips[m_tip_index], text))
    {
        StepTip(true);
        ExpandTip(kWelcomeTips[m_tip_index], text);
    }

    BeginCard(settings, "welcome_card_tips");
    SectionHeader(settings, "Tips & tricks");
    const float gap = font_size * WELCOME_TIP_NAV_GAP_EM;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(gap, font_size * WELCOME_TIP_SPACING_EM));

    // Topic on the left; previous, position, and next on the right of the row.
    const float row_x = ImGui::GetCursorPosX();
    const float row_w = ImGui::GetContentRegionAvail().x;
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kAccent));
    ImGui::TextUnformatted(kWelcomeTips[m_tip_index].topic);
    ImGui::PopStyleColor();

    char position[32];
    std::snprintf(position, sizeof(position), "%zu of %zu", m_tip_index + 1,
                  WELCOME_TIP_COUNT);
    const float button = ImGui::GetTextLineHeight();
    const float nav_w  = button * 2.0f + gap * 2.0f + ImGui::CalcTextSize(position).x;
    ImFont*     icons  = settings.GetFontManager().GetFont(FontType::kIcon);
    const ImU32 idle   = settings.GetColor(Colors::kTransparent);
    const ImU32 hover  = settings.GetColor(Colors::kButtonHovered);
    const ImU32 press  = settings.GetColor(Colors::kButtonActive);
    ImGui::SameLine(std::max(row_x, row_x + row_w - nav_w), 0.0f);
    const bool previous =
        IconButton(ICON_CHEVRON_LEFT, icons, ImVec2(button, button), "Previous tip",
                   false, ImVec2(0.0f, 0.0f), idle, hover, press, "welcome_tip_previous",
                   font_size);
    ImGui::SameLine(0.0f, gap);
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
    ImGui::TextUnformatted(position);
    ImGui::PopStyleColor();
    ImGui::SameLine(0.0f, gap);
    const bool next =
        IconButton(ICON_CHEVRON_RIGHT, icons, ImVec2(button, button), "Next tip", false,
                   ImVec2(0.0f, 0.0f), idle, hover, press, "welcome_tip_next", font_size);

    // Every tip takes the tallest one's height, so browsing never moves the
    // rest of the page.
    const float wrap_w  = ImGui::GetContentRegionAvail().x;
    float       tallest = 0.0f;
    std::string other;
    for(const WelcomeTip& tip : kWelcomeTips)
    {
        if(ExpandTip(tip, other))
        {
            tallest = std::max(
                tallest, ImGui::CalcTextSize(other.c_str(), nullptr, false, wrap_w).y);
        }
    }
    const float shown = ImGui::CalcTextSize(text.c_str(), nullptr, false, wrap_w).y;
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap_w);
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopTextWrapPos();
    ImGui::Dummy(ImVec2(0.0f, std::max(0.0f, tallest - shown)));

    ExtendCard(extra_height);
    ImGui::PopStyleVar();
    EndCard();

    if(previous)
    {
        StepTip(false);
    }
    else if(next)
    {
        StepTip(true);
    }
}

void
WelcomePage::StepTip(bool forward)
{
    std::string unused;
    for(size_t tried = 0; tried < WELCOME_TIP_COUNT; ++tried)
    {
        m_tip_index = forward ? (m_tip_index + 1) % WELCOME_TIP_COUNT
                              : (m_tip_index + WELCOME_TIP_COUNT - 1) % WELCOME_TIP_COUNT;
        if(ExpandTip(kWelcomeTips[m_tip_index], unused))
        {
            break;
        }
    }
    SettingsManager::GetInstance().GetInternalSettings().next_welcome_tip =
        static_cast<uint32_t>((m_tip_index + 1) % WELCOME_TIP_COUNT);
}

}  // namespace View
}  // namespace RocProfVis
