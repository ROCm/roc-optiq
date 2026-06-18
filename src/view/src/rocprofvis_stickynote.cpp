// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_stickynote.h"
#include "icons/rocprovfis_icon_defines.h"
#include "imgui.h"
#include "rocprofvis_click_manager.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_events.h"
#include "rocprofvis_hotkey_manager.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include <algorithm>
#include <cfloat>
#include <spdlog/spdlog.h>

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
}  // namespace

static int s_unique_id_counter = 0;
StickyNote::StickyNote(double time_ns, float y_offset, const ImVec2& size,
                       const std::string& text, const std::string& title,
                       const std::string& project_id, double v_min, double v_max,
                       uint64_t track_id, bool is_minimized)
: m_time_ns(time_ns)
, m_y_offset(y_offset)
, m_track_id(track_id)
, m_size(size)
, m_text(text)
, m_title(title)
, m_project_id(project_id)
, m_id(s_unique_id_counter)
, m_is_visible(true)
, m_v_min_x(v_min)
, m_v_max_x(v_max)
, m_is_minimized(is_minimized)
{
    s_unique_id_counter = s_unique_id_counter + 1;
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

    const float line_x   = window_position.x + tpt->TimeToPixel(m_time_ns);
    const float y_top    = window_position.y;
    const float y_bottom = window_position.y + tpt->GetGraphSizeY();
    draw_list->AddLine(ImVec2(line_x, y_top), ImVec2(line_x, y_bottom), color,
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
StickyNote::SetText(std::string title)
{
    m_text = title;
}
void
StickyNote::SetTitle(std::string title)
{
    m_title = title;
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
    SettingsManager& settings     = SettingsManager::GetInstance();
    ImU32            bg_color     = settings.GetColor(Colors::kStickyNoteBg);
    ImU32            border_color = settings.GetColor(Colors::kStickyNoteBorder);
    ImU32            header_color = settings.GetColor(Colors::kStickyNoteHeader);
    ImU32            shadow_color = settings.GetColor(Colors::kStickyNoteShadow);
    // Overlay derived from the note text color so hover/active stays in palette.
    ImU32 icon_hover_color =
        ApplyAlpha(settings.GetColor(Colors::kStickyNoteText), kIconHoverAlpha);
    ImU32 icon_active_color =
        ApplyAlpha(settings.GetColor(Colors::kStickyNoteText), kIconActiveAlpha);
    ImU32 text_color       = settings.GetColor(Colors::kStickyNoteText);
    ImU32 muted_text_color = settings.GetColor(Colors::kStickyNoteTextMuted);
    ImU32 accent_color     = settings.GetColor(Colors::kStickyNoteAccent);

    const float rounding = settings.GetDefaultStyle().ChildRounding;

    float  x           = tpt->TimeToPixel(m_time_ns);
    float  y           = ResolveAnchorY(layout);
    ImVec2 sticky_pos  = ImVec2(window_position.x + x, window_position.y + y);
    ImVec2 sticky_size = m_size;

    if(m_is_minimized)
    {
        ImGui::SetCursorScreenPos(sticky_pos);
        std::string child_id = "StickyButtonArea##" + std::to_string(m_id);
        ImGui::BeginChild(child_id.c_str(), m_size, false, ImGuiWindowFlags_None);

        ImFont* icon_font = SettingsManager::GetInstance().GetFontManager().GetFont(
            FontType::kIcon);
        ImGui::PushFont(icon_font);
        ImVec2 icon_size = ImGui::CalcTextSize(ICON_STICKY_NOTE);
        ImVec2 padding   = ImGui::GetStyle().FramePadding;
        const float minimized_btn_size =
            std::max(icon_size.x + padding.x * 2.0f, icon_size.y + padding.y * 2.0f);
        ImVec2 btn_max = ImVec2(sticky_pos.x + minimized_btn_size,
                                sticky_pos.y + minimized_btn_size);
        bool hovered = ImGui::IsMouseHoveringRect(sticky_pos, btn_max);
        if((hovered || m_dragging) && draw_list)
        {
            RenderTimeIndicator(draw_list, window_position, tpt);
        }
        if(draw_list)
        {
            draw_list->AddRectFilled(ImVec2(sticky_pos.x + 2.0f, sticky_pos.y + 3.0f),
                                     ImVec2(btn_max.x + 2.0f, btn_max.y + 3.0f),
                                     shadow_color, rounding * 0.6f);
            draw_list->AddRectFilled(sticky_pos, btn_max, bg_color, rounding * 0.6f);
            draw_list->AddRect(sticky_pos, btn_max, border_color, rounding * 0.6f);
        }
        ImGui::PushStyleColor(ImGuiCol_Button, settings.GetColor(Colors::kTransparent));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, icon_hover_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, icon_active_color);
        ImGui::PushStyleColor(ImGuiCol_Text, text_color);
        ImGui::Button(
            (std::string(ICON_STICKY_NOTE) + "##" + std::to_string(m_id)).c_str(),
            ImVec2(minimized_btn_size, minimized_btn_size));
        if(ImGui::IsItemHovered() && IsMouseReleasedWithDragCheck(ImGuiMouseButton_Left))
        {
            m_expanded_screen_pos = ImVec2(-1, -1);
            m_is_minimized        = false;
        }
        ImGui::PopStyleColor(4);
        ImGui::PopFont();

        bool blocks_timeline_input = hovered;
        if(blocks_timeline_input)
        {
            TimelineFocusManager::GetInstance().RequestLayerFocus(
                Layer::kInteractiveLayer);
        }
        else
        {
            TimelineFocusManager::GetInstance().RequestLayerFocus(Layer::kNone);
        }

        ImGui::EndChild();
        return blocks_timeline_input;
    }
    else
    {
        // Floating window: ImGui owns move/resize; we only seed the first
        // position from the marker anchor, then track it live.
        if(m_expanded_screen_pos.x < 0 || m_expanded_screen_pos.y < 0)
        {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImVec2               anchor   = sticky_pos;
            anchor.x = std::clamp(anchor.x, viewport->WorkPos.x,
                                  viewport->WorkPos.x + viewport->WorkSize.x - sticky_size.x);
            anchor.y = std::clamp(anchor.y, viewport->WorkPos.y,
                                  viewport->WorkPos.y + viewport->WorkSize.y - sticky_size.y);
            m_expanded_screen_pos = anchor;
        }

        ImGui::SetNextWindowPos(m_expanded_screen_pos, ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(sticky_size, ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(kExpandedMinWidth, kExpandedMinHeight), ImVec2(FLT_MAX, FLT_MAX));

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

        bool blocks_timeline_input = false;
        if(visible)
        {
            const ImVec2 win_pos  = ImGui::GetWindowPos();
            const ImVec2 win_size = ImGui::GetWindowSize();

            ImDrawList*  note_draw_list = ImGui::GetWindowDrawList();
            const ImVec2 header_max     = ImVec2(win_pos.x + win_size.x,
                                                 win_pos.y + kExpandedHeaderHeight);
            note_draw_list->AddRectFilled(win_pos, header_max, header_color, rounding,
                                          ImDrawFlags_RoundCornersTop);
            note_draw_list->AddRectFilled(
                win_pos, ImVec2(win_pos.x + kAccentStripeWidth, win_pos.y + win_size.y),
                ApplyAlpha(accent_color, kAccentStripeAlpha), rounding,
                ImDrawFlags_RoundCornersLeft);
            note_draw_list->AddLine(ImVec2(win_pos.x, header_max.y),
                                    ImVec2(win_pos.x + win_size.x, header_max.y),
                                    ApplyAlpha(border_color, kHeaderSeparatorAlpha));

            ImGui::SetCursorPos(
                ImVec2(kNoteMargin,
                       (kExpandedHeaderHeight - ImGui::GetTextLineHeight()) * 0.5f));
            ImGui::PushStyleColor(ImGuiCol_Text, text_color);
            ImGui::TextUnformatted(m_title.c_str());
            ImGui::PopStyleColor();

            ImFont* action_icon_font =
                SettingsManager::GetInstance().GetFontManager().GetFont(FontType::kIcon);
            ImGui::PushFont(action_icon_font);
            ImVec2      edit_icon_size  = ImGui::CalcTextSize(ICON_EDIT);
            ImVec2      close_icon_size = ImGui::CalcTextSize(ICON_X_CIRCLED);
            const float action_btn_size =
                std::max({kHeaderButtonMinSize, edit_icon_size.x + kHeaderButtonPadding,
                          close_icon_size.x + kHeaderButtonPadding});
            const float action_btn_y = (kExpandedHeaderHeight - action_btn_size) * 0.5f;

            ImGui::SetCursorPos(ImVec2(win_size.x - action_btn_size * 2.0f - kNoteMargin -
                                           kHeaderButtonGap,
                                       action_btn_y));
            ImGui::PushStyleColor(ImGuiCol_Button, settings.GetColor(Colors::kTransparent));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, icon_hover_color);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, icon_active_color);
            ImGui::PushStyleColor(ImGuiCol_Text, muted_text_color);
            if(ImGui::Button((std::string(ICON_EDIT) + "##edit_" + std::to_string(m_id))
                                 .c_str(),
                             ImVec2(action_btn_size, action_btn_size)))
            {
                EventManager::GetInstance()->AddEvent(
                    std::make_shared<StickyNoteEvent>(m_id, m_project_id));
            }
            ImGui::PopStyleColor(4);

            ImGui::SetCursorPos(
                ImVec2(win_size.x - action_btn_size - kNoteMargin, action_btn_y));
            ImGui::PushStyleColor(ImGuiCol_Button, settings.GetColor(Colors::kTransparent));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, icon_hover_color);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, icon_active_color);
            ImGui::PushStyleColor(ImGuiCol_Text, muted_text_color);
            if(ImGui::Button(
                   (std::string(ICON_X_CIRCLED) + "##close_" + std::to_string(m_id)).c_str(),
                   ImVec2(action_btn_size, action_btn_size)))
            {
                m_is_minimized = true;
            }
            ImGui::PopStyleColor(4);
            ImGui::PopFont();

            // Scroll only the note body; the header/actions stay pinned.
            const float body_y      = kExpandedHeaderHeight + kNoteMargin;
            const float body_height = std::max(0.0f, win_size.y - body_y - kNoteMargin);
            ImGui::SetCursorPos(ImVec2(kNoteMargin, body_y));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kTransparent));
            ImGui::BeginChild("StickyNoteBody",
                              ImVec2(win_size.x - kNoteMargin * 2.0f, body_height), false);
            ImGui::PushStyleColor(ImGuiCol_Text, text_color);
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() +
                                   ImGui::GetContentRegionAvail().x);
            ImGui::TextUnformatted(m_text.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
            ImGui::EndChild();
            ImGui::PopStyleColor();

            blocks_timeline_input =
                ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
            if(blocks_timeline_input && draw_list)
            {
                RenderTimeIndicator(draw_list, window_position, tpt);
            }

            m_expanded_screen_pos = win_pos;
            m_size                = win_size;
        }
        ImGui::End();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);

        TimelineFocusManager::GetInstance().RequestLayerFocus(
            blocks_timeline_input ? Layer::kInteractiveLayer : Layer::kNone);
        return blocks_timeline_input;
    }
}

void
StickyNote::RenderDragGhost(ImDrawList* draw_list, const ImVec2& screen_pos) const
{
    if(!draw_list) return;

    // Mirrors the minimized-note marker drawn in Render().
    constexpr float kShadowOffsetX = 2.0f;
    constexpr float kShadowOffsetY = 3.0f;
    constexpr float kRoundingScale = 0.6f;

    SettingsManager& settings     = SettingsManager::GetInstance();
    ImU32            bg_color     = settings.GetColor(Colors::kStickyNoteBg);
    ImU32            border_color = settings.GetColor(Colors::kStickyNoteBorder);
    ImU32            shadow_color = settings.GetColor(Colors::kStickyNoteShadow);
    ImU32            text_color   = settings.GetColor(Colors::kStickyNoteText);
    const float rounding = settings.GetDefaultStyle().ChildRounding * kRoundingScale;

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
        ImVec2(screen_pos.x + kShadowOffsetX, screen_pos.y + kShadowOffsetY),
        ImVec2(box_max.x + kShadowOffsetX, box_max.y + kShadowOffsetY), shadow_color,
        rounding);
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
    // Expanded notes live in their own floating window, which owns move/resize.
    if(!m_is_minimized) return false;

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

    if((dragged_id == -1 || dragged_id == m_id) && !m_dragging &&
       ImGui::IsMouseHoveringRect(icon_pos, drag_max) &&
       ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
       !HotkeyManager::GetInstance().IsActionHeld(HotkeyActionId::kRegionSelect))
    {
        m_dragging    = true;
        m_drag_offset = ImVec2(mouse_pos.x - icon_pos.x, mouse_pos.y - icon_pos.y);
        dragged_id    = m_id;
        TimelineFocusManager::GetInstance().RequestLayerFocus(Layer::kInteractiveLayer);
    }

    if(m_dragging && mouse_down)
    {
        float  new_x       = mouse_pos.x - window_position.x - m_drag_offset.x;
        float  new_y       = mouse_pos.y - window_position.y - m_drag_offset.y;
        ImVec2 window_size = ImGui::GetWindowSize();
        new_x = std::clamp(new_x, 0.0f, window_size.x - drag_w);
        new_y = std::clamp(new_y, 0.0f, window_size.y - drag_h);

        m_time_ns = conversion_manager->PixelToTime(new_x);
        // Re-anchor to the track under the cursor, storing a track-relative
        // offset so the note follows reorder / collapse / resize.
        uint64_t track_id    = INVALID_TRACK_ID;
        float    track_top_y = 0.0f;
        if(layout.track_at && layout.track_at(new_y, track_id, track_top_y))
        {
            m_track_id = track_id;
            m_y_offset = new_y - track_top_y;
        }
        else
        {
            m_y_offset = new_y;
        }
        m_v_max_x = conversion_manager->GetVMaxX();
        m_v_min_x = conversion_manager->GetVMinX();
        return true;
    }

    if(m_dragging && mouse_released)
    {
        m_dragging = false;
        dragged_id = -1;
        TimelineFocusManager::GetInstance().RequestLayerFocus(Layer::kNone);
    }

    return m_dragging;
}

}  // namespace View
}  // namespace RocProfVis