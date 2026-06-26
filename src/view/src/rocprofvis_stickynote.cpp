// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_stickynote.h"
#include "icons/rocprovfis_icon_defines.h"
#include "imgui.h"
#include "rocprofvis_click_manager.h"
#include "rocprofvis_hotkey_manager.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include <algorithm>
#include <cfloat>
#include <spdlog/spdlog.h>
#include <string>

namespace RocProfVis
{
namespace View
{

namespace
{
constexpr float kExpandedHeaderHeight = 36.0f;
constexpr float kExpandedMinWidth     = 160.0f;
constexpr float kExpandedMinHeight    = 96.0f;
constexpr float kWindowBorderSize     = 1.0f;
constexpr float kNoteMargin           = 14.0f;
constexpr float kHeaderButtonGap      = 8.0f;
constexpr float kHeaderButtonMinSize  = 34.0f;
constexpr float kHeaderButtonPadding  = 14.0f;
constexpr float kAccentStripeWidth    = 3.0f;
constexpr float kAccentStripeAlpha    = 0.78f;
constexpr float kHeaderSeparatorAlpha = 0.42f;
constexpr float kIconHoverAlpha       = 0.12f;
constexpr float kIconActiveAlpha      = 0.22f;
constexpr float kMarkerRoundingScale  = 0.6f;
constexpr float kMarkerShadowOffsetX  = 2.0f;
constexpr float kMarkerShadowOffsetY  = 3.0f;
constexpr float kUnplacedCoord        = -1.0f;
constexpr float kTitleInputPadX       = 4.0f;

// Grows the backing std::string on demand so ImGui input fields can edit it in
// place without a fixed char buffer.
int
GrowStringCallback(ImGuiInputTextCallbackData* data)
{
    if(data->EventFlag == ImGuiInputTextFlags_CallbackResize)
    {
        std::string* str = static_cast<std::string*>(data->UserData);
        str->resize(static_cast<size_t>(data->BufTextLen));
        data->Buf = str->data();
    }
    return 0;
}

bool
InlineInputText(const char* id, const char* hint, std::string& str,
                ImGuiInputTextFlags flags)
{
    flags |= ImGuiInputTextFlags_CallbackResize;
    return ImGui::InputTextWithHint(id, hint, str.data(), str.capacity() + 1, flags,
                                    GrowStringCallback, &str);
}

bool
InlineInputTextMultiline(const char* id, std::string& str, const ImVec2& size,
                         ImGuiInputTextFlags flags)
{
    flags |= ImGuiInputTextFlags_CallbackResize;
    return ImGui::InputTextMultiline(id, str.data(), str.capacity() + 1, size, flags,
                                     GrowStringCallback, &str);
}

// Styles an input to read like plain text: no frame, no background.
void
PushPlainTextInputStyle(SettingsManager& settings)
{
    ImU32 transparent = settings.GetColor(Colors::kTransparent);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, transparent);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, transparent);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, transparent);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
}

void
PopPlainTextInputStyle()
{
    ImGui::PopStyleVar(1);
    ImGui::PopStyleColor(3);
}
}  // namespace

static int s_unique_id_counter = 0;
StickyNote::StickyNote(double time_ns, float y_offset, const ImVec2& size,
                       const std::string& text, const std::string& title, double v_min,
                       double v_max, uint64_t track_id, bool is_minimized)
: m_time_ns(time_ns)
, m_y_offset(y_offset)
, m_track_id(track_id)
, m_size(size)
, m_text(text)
, m_title(title)
, m_id(s_unique_id_counter)
, m_is_visible(true)
, m_v_min_x(v_min)
, m_v_max_x(v_max)
, m_is_minimized(is_minimized)
{
    ++s_unique_id_counter;
}

void
StickyNote::EnsureBound(const TrackLayout& layout)
{
    if(m_track_id != INVALID_TRACK_ID || !layout.track_at) return;

    // In the unbound state m_y_offset is an absolute Y; convert it to relative.
    uint64_t track_id    = INVALID_TRACK_ID;
    float    track_top_y = 0.0f;
    if(layout.track_at(m_y_offset, track_id, track_top_y))
    {
        m_track_id = track_id;
        m_y_offset -= track_top_y;
    }
}

float
StickyNote::ResolveAnchorY(const TrackLayout& layout) const
{
    if(m_dragging) return m_drag_abs_y;

    float track_top_y = 0.0f;
    if(m_track_id != INVALID_TRACK_ID && layout.top_of &&
       layout.top_of(m_track_id, track_top_y))
    {
        return track_top_y + m_y_offset;
    }
    return m_y_offset;
}

void
StickyNote::RenderTimeIndicator(ImDrawList*                         draw_list,
                                const ImVec2&                       window_position,
                                std::shared_ptr<TimePixelTransform> tpt) const
{
    if(!draw_list || !tpt) return;

    constexpr float kLineThickness = 1.5f;
    constexpr float kLineAlpha     = 0.85f;

    SettingsManager& settings = SettingsManager::GetInstance();
    ImU32            color =
        ApplyAlpha(settings.GetColor(Colors::kStickyNoteAccent), kLineAlpha);

    // Span the full visible track area (the draw list's clip rect) rather than
    // keying off the scrolled content origin, which only covers the top sliver.
    const float  line_x   = window_position.x + tpt->TimeToPixel(m_time_ns);
    const ImVec2 clip_min = draw_list->GetClipRectMin();
    const ImVec2 clip_max = draw_list->GetClipRectMax();
    draw_list->AddLine(ImVec2(line_x, clip_min.y), ImVec2(line_x, clip_max.y), color,
                       kLineThickness);
}

double
StickyNote::GetTimeNs() const
{
    return m_time_ns;
}
float
StickyNote::GetYOffset() const
{
    return m_y_offset;
}
double
StickyNote::GetVMinX() const
{
    return m_v_min_x;
}
double
StickyNote::GetVMaxX() const
{
    return m_v_max_x;
}

ImVec2
StickyNote::GetSize() const
{
    return m_size;
}
const std::string&
StickyNote::GetText() const
{
    return m_text;
}

int
StickyNote::GetID() const
{
    return m_id;
}
const std::string&
StickyNote::GetTitle() const
{
    return m_title;
}

void
StickyNote::SetVisibility(bool visible)
{
    m_is_visible = visible;
}

bool
StickyNote::IsVisible() const
{
    return m_is_visible;
}

void
StickyNote::SetText(std::string text)
{
    m_text = std::move(text);
}
void
StickyNote::SetTitle(std::string title)
{
    m_title = std::move(title);
}
void
StickyNote::BeginInlineEdit()
{
    m_is_minimized        = false;
    m_request_focus       = true;
    m_focus_input         = true;
    m_editing_title       = true;
    m_provisional         = true;
    m_seen_focus          = false;
    m_expanded_screen_pos = ImVec2(kUnplacedCoord, kUnplacedCoord);
}
bool
StickyNote::Render(ImDrawList* draw_list, const ImVec2& window_position,
                   std::shared_ptr<TimePixelTransform> tpt, const TrackLayout& layout)
{
    if(!tpt)
    {
        spdlog::error(
            "StickyNote::Render: conversion_manager shared_ptr is null, cannot render");
        return false;
    }
    EnsureBound(layout);

    float  x          = tpt->TimeToPixel(m_time_ns);
    float  y          = ResolveAnchorY(layout);
    ImVec2 anchor_pos = ImVec2(window_position.x + x, window_position.y + y);

    // The anchor marker is always shown so the note's timeline location stays
    // visible even when expanded.
    bool marker_hovered = RenderAnchorMarker(draw_list, anchor_pos);

    bool window_hovered = false;
    if(!m_is_minimized)
    {
        window_hovered = RenderExpandedWindow(anchor_pos);
    }

    bool pair_hovered = marker_hovered || window_hovered;

    if((pair_hovered || m_dragging) && draw_list)
    {
        RenderTimeIndicator(draw_list, window_position, tpt);
    }

    TimelineFocusManager::GetInstance().RequestLayerFocus(
        pair_hovered ? Layer::kInteractiveLayer : Layer::kNone);
    return pair_hovered;
}

bool
StickyNote::RenderAnchorMarker(ImDrawList* draw_list, const ImVec2& marker_pos)
{
    SettingsManager& settings     = SettingsManager::GetInstance();
    ImU32            bg_color     = settings.GetColor(Colors::kStickyNoteBg);
    ImU32            border_color = settings.GetColor(Colors::kStickyNoteBorder);
    ImU32            shadow_color = settings.GetColor(Colors::kStickyNoteShadow);
    ImU32            text_color   = settings.GetColor(Colors::kStickyNoteText);
    ImU32            icon_hover_color =
        ApplyAlpha(settings.GetColor(Colors::kStickyNoteText), kIconHoverAlpha);
    ImU32 icon_active_color =
        ApplyAlpha(settings.GetColor(Colors::kStickyNoteText), kIconActiveAlpha);
    const float rounding =
        settings.GetDefaultStyle().ChildRounding * kMarkerRoundingScale;

    ImGui::SetCursorScreenPos(marker_pos);
    std::string child_id = "StickyButtonArea##" + std::to_string(m_id);
    ImGui::BeginChild(child_id.c_str(), m_size, false, ImGuiWindowFlags_None);

    ImFont* icon_font = settings.GetFontManager().GetFont(FontType::kIcon);
    ImGui::PushFont(icon_font);
    ImVec2      icon_size = ImGui::CalcTextSize(ICON_STICKY_NOTE);
    ImVec2      padding   = ImGui::GetStyle().FramePadding;
    const float btn_size =
        std::max(icon_size.x + padding.x * 2.0f, icon_size.y + padding.y * 2.0f);
    ImVec2 btn_max = ImVec2(marker_pos.x + btn_size, marker_pos.y + btn_size);
    bool   hovered = ImGui::IsMouseHoveringRect(marker_pos, btn_max);

    if(draw_list)
    {
        draw_list->AddRectFilled(
            ImVec2(marker_pos.x + kMarkerShadowOffsetX,
                   marker_pos.y + kMarkerShadowOffsetY),
            ImVec2(btn_max.x + kMarkerShadowOffsetX, btn_max.y + kMarkerShadowOffsetY),
            shadow_color, rounding);
        draw_list->AddRectFilled(marker_pos, btn_max, bg_color, rounding);
        draw_list->AddRect(marker_pos, btn_max, border_color, rounding);
    }

    ImGui::PushStyleColor(ImGuiCol_Button, settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, icon_hover_color);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, icon_active_color);
    ImGui::PushStyleColor(ImGuiCol_Text, text_color);
    ImGui::Button((std::string(ICON_STICKY_NOTE) + "##" + std::to_string(m_id)).c_str(),
                  ImVec2(btn_size, btn_size));
    // Clicking a minimized marker expands the note; re-seed its window position.
    if(m_is_minimized && ImGui::IsItemHovered() &&
       IsMouseReleasedWithDragCheck(ImGuiMouseButton_Left))
    {
        m_expanded_screen_pos = ImVec2(kUnplacedCoord, kUnplacedCoord);
        m_is_minimized        = false;
    }
    // Pressing an expanded note's marker steals window focus on mouse-down;
    // request refocus while held so the empty-note auto-discard doesn't fire.
    if(!m_is_minimized && ImGui::IsItemActive())
    {
        m_request_focus = true;
    }
    ImGui::PopStyleColor(4);
    ImGui::PopFont();
    ImGui::EndChild();

    return hovered;
}

bool
StickyNote::RenderExpandedWindow(const ImVec2& anchor_pos)
{
    SettingsManager& settings     = SettingsManager::GetInstance();
    ImU32            bg_color     = settings.GetColor(Colors::kStickyNoteBg);
    ImU32            border_color = settings.GetColor(Colors::kStickyNoteBorder);
    ImU32            header_color = settings.GetColor(Colors::kStickyNoteHeader);
    ImU32            icon_hover_color =
        ApplyAlpha(settings.GetColor(Colors::kStickyNoteText), kIconHoverAlpha);
    ImU32 icon_active_color =
        ApplyAlpha(settings.GetColor(Colors::kStickyNoteText), kIconActiveAlpha);
    ImU32 text_color       = settings.GetColor(Colors::kStickyNoteText);
    ImU32 muted_text_color = settings.GetColor(Colors::kStickyNoteTextMuted);
    ImU32 accent_color     = settings.GetColor(Colors::kStickyNoteAccent);

    const float  rounding    = settings.GetDefaultStyle().ChildRounding;
    const ImVec2 sticky_size = m_size;

    // Seed the first position from the marker anchor, then ImGui owns it.
    if(!IsExpandedPlaced())
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2               anchor   = anchor_pos;
        anchor.x = std::clamp(anchor.x, viewport->WorkPos.x,
                              viewport->WorkPos.x + viewport->WorkSize.x - sticky_size.x);
        anchor.y = std::clamp(anchor.y, viewport->WorkPos.y,
                              viewport->WorkPos.y + viewport->WorkSize.y - sticky_size.y);
        m_expanded_screen_pos = anchor;
    }

    // A pending focus request (e.g. clicking the note's own anchor) refocuses
    // the window next frame, so it must not count as abandoning the note.
    const bool refocusing = m_request_focus;
    if(m_request_focus)
    {
        ImGui::SetNextWindowFocus();
        m_request_focus = false;
    }
    ImGui::SetNextWindowPos(m_expanded_screen_pos, ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(sticky_size, ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(kExpandedMinWidth, kExpandedMinHeight),
                                        ImVec2(FLT_MAX, FLT_MAX));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, rounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, kWindowBorderSize);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, bg_color);
    ImGui::PushStyleColor(ImGuiCol_Border, border_color);

    const ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoSavedSettings;

    std::string window_id = "StickyNoteWindow##" + std::to_string(m_id);
    bool        visible   = ImGui::Begin(window_id.c_str(), nullptr, window_flags);

    bool hovered = false;
    if(visible)
    {
        const ImVec2 win_pos  = ImGui::GetWindowPos();
        const ImVec2 win_size = ImGui::GetWindowSize();

        ImDrawList*  note_draw_list = ImGui::GetWindowDrawList();
        const ImVec2 header_max =
            ImVec2(win_pos.x + win_size.x, win_pos.y + kExpandedHeaderHeight);
        note_draw_list->AddRectFilled(win_pos, header_max, header_color, rounding,
                                      ImDrawFlags_RoundCornersTop);
        note_draw_list->AddRectFilled(
            win_pos, ImVec2(win_pos.x + kAccentStripeWidth, win_pos.y + win_size.y),
            ApplyAlpha(accent_color, kAccentStripeAlpha), rounding,
            ImDrawFlags_RoundCornersLeft);
        note_draw_list->AddLine(ImVec2(win_pos.x, header_max.y),
                                ImVec2(win_pos.x + win_size.x, header_max.y),
                                ApplyAlpha(border_color, kHeaderSeparatorAlpha));

        ImFont* action_icon_font = settings.GetFontManager().GetFont(FontType::kIcon);
        ImGui::PushFont(action_icon_font);
        ImVec2 close_icon_size = ImGui::CalcTextSize(ICON_X_CIRCLED);
        ImVec2 trash_icon_size = ImGui::CalcTextSize(ICON_TRASH_CAN);
        ImGui::PopFont();
        const float action_btn_size = std::max(
            {kHeaderButtonMinSize, close_icon_size.x + kHeaderButtonPadding,
             trash_icon_size.x + kHeaderButtonPadding});
        const float action_btn_y = (kExpandedHeaderHeight - action_btn_size) * 0.5f;

        // The title is plain text by default so the header stays draggable;
        // a click (not a drag) on it switches to an inline input.
        const float buttons_width = action_btn_size * 2.0f + kHeaderButtonGap;
        const float title_width   = std::max(
            0.0f, win_size.x - buttons_width - kHeaderButtonGap - kNoteMargin * 2.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, text_color);
        if(m_editing_title)
        {
            ImGui::SetCursorPos(ImVec2(
                kNoteMargin - kTitleInputPadX,
                (kExpandedHeaderHeight - ImGui::GetFrameHeight()) * 0.5f));
            ImGui::SetNextItemWidth(title_width + kTitleInputPadX);
            PushPlainTextInputStyle(settings);
            if(m_focus_input)
            {
                ImGui::SetKeyboardFocusHere();
                m_focus_input = false;
            }
            InlineInputText(("##title_" + std::to_string(m_id)).c_str(), "Title", m_title,
                            ImGuiInputTextFlags_EnterReturnsTrue);
            if(ImGui::IsItemDeactivated())
            {
                m_editing_title = false;
            }
            PopPlainTextInputStyle();
        }
        else
        {
            ImGui::SetCursorPos(ImVec2(
                kNoteMargin, (kExpandedHeaderHeight - ImGui::GetTextLineHeight()) * 0.5f));
            if(m_title.empty())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, muted_text_color);
                ImGui::TextUnformatted("Title");
                ImGui::PopStyleColor();
            }
            else
            {
                ElidedText(m_title.c_str(), title_width);
            }

            const ImVec2 title_min(win_pos.x + kNoteMargin, win_pos.y);
            const ImVec2 title_max(title_min.x + title_width,
                                   win_pos.y + kExpandedHeaderHeight);
            // IsWindowHovered ignores clicks landing on a window drawn on top.
            if(ImGui::IsWindowHovered() &&
               ImGui::IsMouseHoveringRect(title_min, title_max) &&
               IsMouseReleasedWithDragCheck(ImGuiMouseButton_Left))
            {
                m_editing_title = true;
                m_focus_input   = true;
            }
        }
        ImGui::PopStyleColor();

        // Glyph color for the header buttons; IconButton handles the (transparent)
        // frame and hover/active backgrounds, with zero frame padding so the icon
        // stays centered as the font scales.
        ImGui::PushStyleColor(ImGuiCol_Text, muted_text_color);
        const ImU32 transparent = settings.GetColor(Colors::kTransparent);
        const ImVec2 action_btn(action_btn_size, action_btn_size);

        ImGui::SetCursorPos(ImVec2(
            win_size.x - action_btn_size * 2.0f - kHeaderButtonGap - kNoteMargin,
            action_btn_y));
        if(IconButton(ICON_TRASH_CAN, action_icon_font, action_btn, nullptr, false,
                      ImVec2(0.0f, 0.0f), transparent, icon_hover_color,
                      icon_active_color,
                      ("delete_" + std::to_string(m_id)).c_str()))
        {
            m_pending_delete = true;
        }

        ImGui::SetCursorPos(
            ImVec2(win_size.x - action_btn_size - kNoteMargin, action_btn_y));
        if(IconButton(ICON_X_CIRCLED, action_icon_font, action_btn, nullptr, false,
                      ImVec2(0.0f, 0.0f), transparent, icon_hover_color,
                      icon_active_color, ("close_" + std::to_string(m_id)).c_str()))
        {
            if(m_provisional && m_title.empty() && m_text.empty())
            {
                m_pending_delete = true;
            }
            else
            {
                m_is_minimized = true;
            }
        }
        ImGui::PopStyleColor();

        // Inline-editable body filling the remaining space.
        const float body_y      = kExpandedHeaderHeight + kNoteMargin;
        const float body_height = std::max(0.0f, win_size.y - body_y - kNoteMargin);
        ImGui::SetCursorPos(ImVec2(kNoteMargin, body_y));
        ImGui::PushStyleColor(ImGuiCol_Text, text_color);
        PushPlainTextInputStyle(settings);
        InlineInputTextMultiline(
            ("##body_" + std::to_string(m_id)).c_str(), m_text,
            ImVec2(win_size.x - kNoteMargin * 2.0f, body_height),
            ImGuiInputTextFlags_AllowTabInput);
        bool body_active = ImGui::IsItemActive();
        PopPlainTextInputStyle();
        ImGui::PopStyleColor();

        // Placeholder hint while the body is empty and not being edited.
        if(m_text.empty() && !body_active)
        {
            note_draw_list->AddText(
                ImVec2(win_pos.x + kNoteMargin + ImGui::GetStyle().FramePadding.x,
                       win_pos.y + body_y + ImGui::GetStyle().FramePadding.y),
                muted_text_color, "Write a note...");
        }

        hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        // A provisional (just-created) note commits once anything is typed, and
        // is discarded if the user clicks away while it is still empty.
        if(m_provisional)
        {
            bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
            if(focused)
            {
                m_seen_focus = true;
            }

            if(!m_title.empty() || !m_text.empty())
            {
                m_provisional = false;
            }
            else if(m_seen_focus && !focused && !refocusing)
            {
                m_pending_delete = true;
            }
        }

        m_expanded_screen_pos = win_pos;
        m_size                = win_size;
    }
    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    return hovered;
}

void
StickyNote::RenderDragGhost(ImDrawList* draw_list, const ImVec2& screen_pos) const
{
    if(!draw_list) return;

    // Mirrors the minimized-note marker drawn in Render().
    SettingsManager& settings     = SettingsManager::GetInstance();
    ImU32            bg_color     = settings.GetColor(Colors::kStickyNoteBg);
    ImU32            border_color = settings.GetColor(Colors::kStickyNoteBorder);
    ImU32            shadow_color = settings.GetColor(Colors::kStickyNoteShadow);
    ImU32            text_color   = settings.GetColor(Colors::kStickyNoteText);
    const float rounding =
        settings.GetDefaultStyle().ChildRounding * kMarkerRoundingScale;

    ImFont* icon_font = settings.GetFontManager().GetFont(FontType::kIcon);
    ImGui::PushFont(icon_font);
    ImVec2 icon_size      = ImGui::CalcTextSize(ICON_STICKY_NOTE);
    float  icon_font_size = ImGui::GetFontSize();
    ImGui::PopFont();

    ImVec2      padding = ImGui::GetStyle().FramePadding;
    const float btn_size =
        std::max(icon_size.x + padding.x * 2.0f, icon_size.y + padding.y * 2.0f);
    ImVec2 box_max = ImVec2(screen_pos.x + btn_size, screen_pos.y + btn_size);

    draw_list->AddRectFilled(
        ImVec2(screen_pos.x + kMarkerShadowOffsetX, screen_pos.y + kMarkerShadowOffsetY),
        ImVec2(box_max.x + kMarkerShadowOffsetX, box_max.y + kMarkerShadowOffsetY),
        shadow_color, rounding);
    draw_list->AddRectFilled(screen_pos, box_max, bg_color, rounding);
    draw_list->AddRect(screen_pos, box_max, border_color, rounding);

    ImVec2 text_pos = ImVec2(screen_pos.x + (btn_size - icon_size.x) * 0.5f,
                             screen_pos.y + (btn_size - icon_size.y) * 0.5f);
    draw_list->AddText(icon_font, icon_font_size, text_pos, text_color,
                       ICON_STICKY_NOTE);
}

bool
StickyNote::HandleDrag(const ImVec2&                       window_position,
                       std::shared_ptr<TimePixelTransform> conversion_manager,
                       int& dragged_id, const TrackLayout& layout)
{
    if(!conversion_manager)
    {
        spdlog::error("StickyNote::HandleDrag: conversion_manager shared_ptr is null");
        return false;
    }
    // The anchor marker can be dragged whether the note is minimized or expanded;
    // it moves the note's timeline position, independent of the expanded window's
    // own screen position.
    EnsureBound(layout);

    float x = conversion_manager->TimeToPixel(m_time_ns);
    float y = ResolveAnchorY(layout);

    ImVec2  icon_pos  = ImVec2(window_position.x + x, window_position.y + y);
    ImFont* icon_font =
        SettingsManager::GetInstance().GetFontManager().GetFont(FontType::kIcon);
    ImGui::PushFont(icon_font, 0.0f);
    ImVec2 icon_size = ImGui::CalcTextSize(ICON_STICKY_NOTE);
    ImGui::PopFont();

    ImVec2 padding  = ImGui::GetStyle().FramePadding;
    float  drag_w   = icon_size.x + padding.x * 2.0f;
    float  drag_h   = icon_size.y + padding.y * 2.0f;
    ImVec2 drag_max = ImVec2(icon_pos.x + drag_w, icon_pos.y + drag_h);

    ImVec2 mouse_pos      = ImGui::GetMousePos();
    bool   mouse_down     = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool   mouse_released = ImGui::IsMouseReleased(ImGuiMouseButton_Left);

    // The anchor draws to the timeline draw list, so gate it on the timeline
    // being hovered; this is false when a window or popup covers the cursor.
    const bool timeline_hovered =
        ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

    if((dragged_id == INVALID_STICKY_ID || dragged_id == m_id) && !m_dragging &&
       timeline_hovered && ImGui::IsMouseHoveringRect(icon_pos, drag_max) &&
       ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
       !HotkeyManager::GetInstance().IsActionHeld(HotkeyActionId::kRegionSelect))
    {
        m_dragging    = true;
        m_drag_offset = ImVec2(mouse_pos.x - icon_pos.x, mouse_pos.y - icon_pos.y);
        m_drag_abs_y  = y;
        dragged_id    = m_id;
        TimelineFocusManager::GetInstance().RequestLayerFocus(Layer::kInteractiveLayer);
    }

    if(m_dragging && mouse_down)
    {
        float  new_x       = mouse_pos.x - window_position.x - m_drag_offset.x;
        float  new_y       = mouse_pos.y - window_position.y - m_drag_offset.y;
        ImVec2 window_size = ImGui::GetWindowSize();
        new_x = std::clamp(new_x, 0.0f, window_size.x - drag_w);

        // Keep the anchor inside the visible track viewport so it cannot be
        // dragged off-screen; the timeline auto-scrolls to reveal more instead.
        float min_y = 0.0f;
        float max_y = window_size.y - drag_h;
        if(layout.view_max_y > layout.view_min_y)
        {
            min_y = layout.view_min_y;
            max_y = layout.view_max_y - drag_h;
        }
        new_y = std::clamp(new_y, min_y, std::max(min_y, max_y));

        m_time_ns = conversion_manager->PixelToTime(new_x);
        // Re-anchor only on drop (track_at scans every graph); hold an absolute Y.
        m_drag_abs_y = new_y;
        m_v_max_x    = conversion_manager->GetVMaxX();
        m_v_min_x    = conversion_manager->GetVMinX();
        return true;
    }

    if(m_dragging && mouse_released)
    {
        // Bind to the track under the drop, keeping a track-relative offset.
        uint64_t track_id    = INVALID_TRACK_ID;
        float    track_top_y = 0.0f;
        if(layout.track_at && layout.track_at(m_drag_abs_y, track_id, track_top_y))
        {
            m_track_id = track_id;
            m_y_offset = m_drag_abs_y - track_top_y;
        }
        else
        {
            m_y_offset = m_drag_abs_y;
        }

        m_dragging = false;
        dragged_id = INVALID_STICKY_ID;
        TimelineFocusManager::GetInstance().RequestLayerFocus(Layer::kNone);
    }

    return m_dragging;
}

}  // namespace View
}  // namespace RocProfVis