// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_gui_helpers.h"
#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "spdlog/spdlog.h"
#include "widgets/rocprofvis_notification_manager.h"
#include "widgets/rocprofvis_widget.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

namespace RocProfVis
{

namespace View
{

// Font size the UI was laid out against. Sizes authored at this font render at
// 1:1; ImGui's DPI font scaling then scales them via ImGui::GetFontSize().
constexpr float BASE_DESIGN_FONT_SIZE = 13.0f;

namespace
{
int
StringResizeCallback(ImGuiInputTextCallbackData* data)
{
    if(data->EventFlag == ImGuiInputTextFlags_CallbackResize)
    {
        std::string* str = static_cast<std::string*>(data->UserData);
        str->resize(static_cast<size_t>(data->BufTextLen));
        data->Buf = str->data();
    }
    return 0;
}
}  // namespace

bool
InputTextString(const char* id, std::string& str, ImGuiInputTextFlags flags)
{
    str.reserve(std::max(str.size() + 1, static_cast<size_t>(256)));
    return ImGui::InputText(id, str.data(), str.capacity() + 1,
                            flags | ImGuiInputTextFlags_CallbackResize,
                            StringResizeCallback, static_cast<void*>(&str));
}

bool
InputTextStringWithHint(const char* id, const char* hint, std::string& str,
                        ImGuiInputTextFlags flags)
{
    str.reserve(std::max(str.size() + 1, static_cast<size_t>(256)));
    return ImGui::InputTextWithHint(id, hint, str.data(), str.capacity() + 1,
                                    flags | ImGuiInputTextFlags_CallbackResize,
                                    StringResizeCallback, static_cast<void*>(&str));
}

bool
InputTextMultilineString(const char* id, std::string& str, const ImVec2& size,
                         ImGuiInputTextFlags flags)
{
    str.reserve(std::max(str.size() + 1, static_cast<size_t>(256)));
    return ImGui::InputTextMultiline(id, str.data(), str.capacity() + 1, size,
                                     flags | ImGuiInputTextFlags_CallbackResize,
                                     StringResizeCallback, static_cast<void*>(&str));
}

ImVec2
MeasureLoadingIndicatorDots(float dot_radius, int num_dots,
                                              float spacing)
{
    // Calculate total width needed
    float total_width = (num_dots * (dot_radius * 2.0f)) + ((num_dots - 1) * spacing);
    return ImVec2(total_width, dot_radius * 2.0f);
}

void
RenderLoadingIndicatorDots(float dot_radius, int num_dots,
                                             float spacing, ImU32 color, float speed)
{
    // Calculate total width needed
    float  total_width = MeasureLoadingIndicatorDots(dot_radius, num_dots, spacing).x;
    ImVec2 pos         = ImGui::GetCursorScreenPos();
    ImVec2 size(total_width, dot_radius * 2.0f);

    ImGui::Dummy(size);  // Reserve space

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const float t         = (float) ImGui::GetTime();

    for(int i = 0; i < num_dots; ++i)
    {
        float current_dot_x = pos.x + dot_radius + i * (dot_radius * 2.0f + spacing);
        float current_dot_y = pos.y + dot_radius;

        // Offset each dot's animation phase
        float phase = (t * speed) - (i * (2.0f * PI / (float) num_dots) *
                                     0.5f);  // Adjust 0.5f for phase spread

        float alpha_multiplier = (sinf(phase) + 1.0f) * 0.5f;

        // Sharpen the pulse a bit
        alpha_multiplier = std::clamp(alpha_multiplier * 1.5f - 0.25f, 0.0f, 1.0f);

        ImU32        current_color = color;
        unsigned int alpha         = (current_color >> IM_COL32_A_SHIFT) & 0xFF;
        alpha                      = static_cast<unsigned int>(alpha * alpha_multiplier);
        current_color = (current_color & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);

        draw_list->AddCircleFilled(ImVec2(current_dot_x, current_dot_y), dot_radius,
                                   current_color, 12);
    }
}

void
RenderLoadingIndicator(ImU32 color, const char* window_id,
                       LoadingIndicatorCentering centering, float dot_radius,
                       int num_dots, float dot_spacing, float anim_speed)
{
    if(window_id)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        // Create an overlay child window to display the loading indicator if requested
        ImGui::SetNextWindowPos(ImGui::GetWindowPos() + style.WindowPadding);

        // set transparent background for the overlay window
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImGui::BeginChild(window_id, ImGui::GetWindowSize() - style.WindowPadding * 2.0f, ImGuiChildFlags_None);
    }

    ImVec2 dot_size   = MeasureLoadingIndicatorDots(dot_radius, num_dots, dot_spacing);
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 view_rect  = ImGui::GetWindowSize();
    ImVec2 draw_pos   = ImGui::GetCursorScreenPos();

    if(centering == kCenterHorizontal || centering == kCenterBoth)
    {
        draw_pos.x = window_pos.x + (view_rect.x - dot_size.x) * 0.5f;
    }
    if(centering == kCenterVertical || centering == kCenterBoth)
    {
        draw_pos.y = window_pos.y + (view_rect.y - dot_size.y) * 0.5f;
    }

    if(centering != kCenterNone)
    {
        //needed to position dummy in RenderLoadingIndicatorDots()
        ImGui::SetCursorScreenPos(draw_pos); 
    }
    RenderLoadingIndicatorDots(dot_radius, num_dots, dot_spacing, color, anim_speed);

    if(window_id)
    {
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
}
 

ImU32
ApplyAlpha(ImU32 color, float alpha)
{
    ImVec4 rgba = ImGui::ColorConvertU32ToFloat4(color);
    rgba.w      = std::clamp(alpha, 0.0f, 1.0f);
    return ImGui::ColorConvertFloat4ToU32(rgba);
}

ImVec4
ThemeColor(SettingsManager& settings, Colors color, float alpha)
{
    ImVec4 rgba = ImGui::ColorConvertU32ToFloat4(settings.GetColor(color));
    rgba.w      = std::clamp(rgba.w * alpha, 0.0f, 1.0f);
    return rgba;
}

void
PushComboStyles()
{
    SettingsManager& settings = SettingsManager::GetInstance();
    ImGui::PushStyleColor(ImGuiCol_FrameBg, settings.GetColor(Colors::kComboFill));
}

void
PopComboStyles()
{
    ImGui::PopStyleColor();
}

ImVec2
GetResponsiveWindowSize(ImVec2 desired_size, ImVec2 min_size, float viewport_margin)
{
    const float scale = ImGui::GetFontSize() / BASE_DESIGN_FONT_SIZE;

    ImVec2 result(desired_size.x > 0.0f ? desired_size.x * scale : desired_size.x,
                  desired_size.y > 0.0f ? desired_size.y * scale : desired_size.y);
    const ImVec2 scaled_min(min_size.x > 0.0f ? min_size.x * scale : min_size.x,
                            min_size.y > 0.0f ? min_size.y * scale : min_size.y);

    if(result.x > 0.0f && scaled_min.x > 0.0f)
    {
        result.x = std::max(result.x, scaled_min.x);
    }
    if(result.y > 0.0f && scaled_min.y > 0.0f)
    {
        result.y = std::max(result.y, scaled_min.y);
    }

    if(const ImGuiViewport* viewport = ImGui::GetMainViewport())
    {
        const float margin = std::max(0.0f, viewport_margin * scale);
        const ImVec2 max_size(std::max(0.0f, viewport->WorkSize.x - margin * 2.0f),
                              std::max(0.0f, viewport->WorkSize.y - margin * 2.0f));
        if(result.x > 0.0f && max_size.x > 0.0f)
        {
            result.x = std::min(result.x, max_size.x);
        }
        if(result.y > 0.0f && max_size.y > 0.0f)
        {
            result.y = std::min(result.y, max_size.y);
        }
    }

    return result;
}

bool
IconButton(const char* icon, ImFont* icon_font, ImVec2 size, const char* tooltip,
           bool frameless, ImVec2 frame_padding, ImU32 bg_color, ImU32 bg_color_hover,
           ImU32 bg_color_active, const char* id)
{
    if(id && strlen(id) > 0)
    {
        ImGui::PushID(id);
    }
    else
    {
        ImGui::PushID(icon);
    }
    if(frameless)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(0, 0, 0, 0));
    }
    else
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, frame_padding);
        ImGui::PushStyleColor(ImGuiCol_Button, bg_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bg_color_hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, bg_color_active);
    }
    ImGui::PushFont(icon_font, 0.0f);
    bool clicked = ImGui::Button(icon, size);
    ImGui::PopFont();
    if(tooltip && strlen(tooltip) > 0 && BeginItemTooltipStyled())
    {
        ImGui::TextUnformatted(tooltip);
        EndTooltipStyled();
    }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    ImGui::PopID();
    return clicked;
}

bool
IsMouseReleasedWithDragCheck(ImGuiMouseButton button, float drag_threshold)
{
    if(ImGui::IsMouseReleased(button))
    {
        ImVec2 drag_delta = ImGui::GetMouseDragDelta(button);
        if((drag_delta.x * drag_delta.x + drag_delta.y * drag_delta.y) <
           (drag_threshold * drag_threshold))
        {
            // this is a click, not a drag
            return true;
        }
    }
    //the user is dragging. 
    return false;
}

std::pair<bool, bool>
InputTextWithClear(const char* id, const char* hint, char* buf,
                                     size_t buf_size, ImFont* icon_font, ImU32 bg_color,
                                     const ImGuiStyle& style, float width)
{
    bool input_cleared = false;
    ImGui::BeginGroup();
    ImGui::SetNextItemAllowOverlap();
    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, bg_color);
    ImGui::SetNextItemWidth(width);
    bool input_changed =
        ImGui::InputTextWithHint("##input_text_with_clear", hint, buf, buf_size);
    ImGui::PopStyleColor();
    if(strlen(buf) > 0)
    {
        ImGui::PushFont(icon_font, 0.0f);
        if(width >= ImGui::CalcTextSize(ICON_X_CIRCLED).x + 2 * style.FramePadding.x)
        {
            ImGui::SameLine();
            ImGui::SetCursorScreenPos(
                ImVec2(ImGui::GetItemRectMax().x - 2 * style.FramePadding.x -
                           ImGui::CalcTextSize(ICON_X_CIRCLED).x,
                       ImGui::GetCursorScreenPos().y));
            ImGui::PopFont();
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, style.FramePadding);
            input_cleared =
                IconButton(ICON_X_CIRCLED, icon_font, ImVec2(0, 0), "Clear", false,
                           style.FramePadding, bg_color, bg_color, bg_color);
            ImGui::PopStyleVar();
        }
        else
        {
            ImGui::PopFont();
        }
    }
    ImGui::PopID();
    ImGui::EndGroup();
    return std::make_pair(input_changed, input_cleared);
}

void
SetTooltipStyled(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    SettingsManager& settings = SettingsManager::GetInstance();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        settings.GetDefaultStyle().WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,
                        settings.GetDefaultStyle().FrameRounding);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, settings.GetColor(Colors::kBgFrame));
    ImGui::SetTooltipV(fmt, args);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    va_end(args);
}

void
BeginTooltipStyled()
{
    SettingsManager& settings = SettingsManager::GetInstance();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        settings.GetDefaultStyle().WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,
                        settings.GetDefaultStyle().FrameRounding);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, settings.GetColor(Colors::kBgFrame));
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextMain));
    ImGui::BeginTooltip();
}

bool
BeginItemTooltipStyled()
{
    SettingsManager& settings = SettingsManager::GetInstance();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        settings.GetDefaultStyle().WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,
                        settings.GetDefaultStyle().FrameRounding);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, settings.GetColor(Colors::kBgFrame));
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextMain));
    if(ImGui::BeginItemTooltip())
    {
        return true;
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    return false;
}

void
EndTooltipStyled()
{
    ImGui::EndTooltip();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void
ElidedText(const char* text, float available_width, float tooltip_width,
           Alignment alignment, bool imgui_AlignTextToFramePadding)
{
    ImGuiStyle       style      = ImGui::GetStyle();
    SettingsManager& settings   = SettingsManager::GetInstance();
    float            text_width = ImGui::CalcTextSize(text).x;
    ImVec2           elide_size = ImGui::CalcTextSize(" [...]");
    float  scroll_bar_width     = (ImGui::GetScrollMaxY() != 0.0f) ? style.ScrollbarSize : 0.0f;
    bool   elide                = text_width + scroll_bar_width > available_width;
    ImVec2 elide_pos;
    // Dynamically sized containers do not adapt to clip rect...
    // Use a window to restrict our size and provide sizing hint to client.
    // Do not take input to avoid interfering with client.
    ImGui::BeginChild("elided",
                      ImVec2(available_width, imgui_AlignTextToFramePadding
                                                  ? ImGui::GetFrameHeightWithSpacing()
                                                  : ImGui::GetFontSize()),
                      ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);

    if(imgui_AlignTextToFramePadding)
    {
        ImGui::AlignTextToFramePadding();
    }
    if(elide)
    {
        ImGui::PushClipRect(ImGui::GetCursorScreenPos(),
                            ImGui::GetCursorScreenPos() +
                                ImVec2(available_width - scroll_bar_width - elide_size.x,
                                       ImGui::GetFrameHeightWithSpacing()),
                            true);
    }
    else if(alignment == Alignment_Right)
    {
        ImGui::SetCursorPosX(available_width - text_width);
    }
    else if(alignment == Alignment_Center)
    {
        CenterNextTextItem(text);
    }
    ImGui::TextUnformatted(text);
    if(elide)
    {
        ImGui::PopClipRect();
        ImGui::SameLine(available_width - scroll_bar_width - elide_size.x);
        elide_pos = ImGui::GetCursorScreenPos();
        ImGui::TextUnformatted(" [...]");
    }
    ImGui::EndChild();
    if(elide)
    {
        ImGui::SetCursorScreenPos(elide_pos);
        ImGui::InvisibleButton("elide_hover", elide_size);
        if(tooltip_width > 0.0f)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                                settings.GetDefaultIMGUIStyle().WindowPadding);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,
                                settings.GetDefaultStyle().FrameRounding);
            if(ImGui::BeginItemTooltip())
            {
                ImGui::PushTextWrapPos(tooltip_width);
                ImGui::TextWrapped("%s", text);
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
            ImGui::PopStyleVar(2);
        }
    }
}

std::string
ElideWithEllipsis(const std::string& text, float max_width, size_t max_chars)
{
    std::string out       = text.substr(0, max_chars);
    bool        truncated = text.size() > max_chars;
    while(!out.empty() && ImGui::CalcTextSize((out + "...").c_str()).x > max_width)
    {
        out.pop_back();
        truncated = true;
    }
    if(truncated)
    {
        out += "...";
    }
    return out;
}

void
CenterNextTextItem(const char* text)
{
    CenterNextItem(ImGui::CalcTextSize(text).x);
}

void
CenterNextItem(float width)
{
    float cx    = ImGui::GetCursorPosX();
    float avail = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX(cx + (avail - width) * 0.5f);
}

bool
XButton(const char* id, const char * tool_tip_label, SettingsManager* settings)
{
    bool clicked = false;

    if(!settings)
    {
        settings = &SettingsManager::GetInstance();
    }

    ImGui::PushStyleColor(ImGuiCol_Button, settings->GetColor(Colors::kTransparent));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          settings->GetColor(Colors::kTransparent));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          settings->GetColor(Colors::kTransparent));
    ImGui::PushStyleVarX(ImGuiStyleVar_FramePadding, 0);
    ImGui::PushFont(settings->GetFontManager().GetFont(FontType::kIcon), 0.0f);
    if(id && strlen(id) > 0)
    {
        ImGui::PushID(id);
    }

    clicked = ImGui::SmallButton(ICON_X_CIRCLED);

    if(id && strlen(id) > 0)
    {
        ImGui::PopID();
    }

    ImGui::PopFont();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    if(tool_tip_label && ImGui::IsItemHovered())
         SetTooltipStyled(tool_tip_label);
    return clicked;
}

void
SectionTitle(const char* text, bool large, SettingsManager* settings)
{
    if(!settings)
    {
        settings = &SettingsManager::GetInstance();
    }

    FontSize font_size = large ? FontSize::kLarge : FontSize::kMedLarge;
    ImGui::PushFont(settings->GetFontManager().GetFont(FontType::kDefault),
                    settings->GetFontManager().GetFontSize(font_size));
    ImGui::SeparatorText(text);
    ImGui::PopFont();
}


void
VerticalSeparator(SettingsManager* settings)
{
    if(!settings)
    {
        settings = &SettingsManager::GetInstance();
    }
    auto style = settings->GetDefaultStyle();
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(style.ItemSpacing.x, 0));
    ImGui::SameLine();

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float       height    = ImGui::GetFrameHeight();
    ImVec2      p         = ImGui::GetCursorScreenPos();
    draw_list->AddLine(ImVec2(p.x, p.y), ImVec2(p.x, p.y + height),
                       settings->GetColor(Colors::kMetaDataSeparator), 2.0f);
    ImGui::Dummy(ImVec2(style.ItemSpacing.x, 0));
    ImGui::SameLine();
}

float
TableRowHeight()
{
    return ImGui::GetTextLineHeight() + ImGui::GetStyle().CellPadding.y * 2.0f;
}

namespace
{
Colors
PanelCardBackground(PanelCardTone tone)
{
    Colors bg = Colors::kBgPanel;
    if(tone == PanelCardTone::kFrame)
    {
        bg = Colors::kBgFrame;
    }
    else if(tone == PanelCardTone::kMain)
    {
        bg = Colors::kBgMain;
    }
    return bg;
}
}  // namespace

void
BeginPanelCard(const char* id, PanelCardTone tone, ImVec2 padding, bool bordered,
               SettingsManager* settings)
{
    if(!settings)
    {
        settings = &SettingsManager::GetInstance();
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          settings->GetColor(PanelCardBackground(tone)));
    ImGui::PushStyleColor(ImGuiCol_Border,
                          settings->GetColor(Colors::kPanelBorderSubtle));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, PANEL_CARD_ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, bordered ? 1.0f : 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);

    ImGuiChildFlags child_flags = ImGuiChildFlags_AutoResizeY;
    if(bordered)
    {
        child_flags |= ImGuiChildFlags_Borders;
    }
    ImGui::BeginChild(id, ImVec2(0.0f, 0.0f), child_flags,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
}

void
EndPanelCard()
{
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

void
PanelFieldLabel(const char* text, bool align_to_frame, SettingsManager* settings)
{
    if(!settings)
    {
        settings = &SettingsManager::GetInstance();
    }
    if(align_to_frame)
    {
        ImGui::AlignTextToFramePadding();
    }
    ImGui::PushStyleColor(ImGuiCol_Text, settings->GetColor(Colors::kTextDim));
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

void
PanelIcon(const char* glyph, Colors color, SettingsManager* settings)
{
    if(!settings)
    {
        settings = &SettingsManager::GetInstance();
    }
    ImGui::PushFont(settings->GetFontManager().GetFont(FontType::kIcon),
                    ImGui::GetFontSize());
    ImGui::PushStyleColor(ImGuiCol_Text, settings->GetColor(color));
    ImGui::TextUnformatted(glyph);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

bool
AccentButton(const char* label, ImVec2 size, SettingsManager* settings)
{
    if(!settings)
    {
        settings = &SettingsManager::GetInstance();
    }
    return ColoredButton(label, settings->GetColor(Colors::kAccent),
                         settings->GetColor(Colors::kAccentHover),
                         settings->GetColor(Colors::kAccentActive),
                         settings->GetColor(Colors::kTextOnAccent), nullptr, size);
}

bool
ColoredButton(const char* label, ImU32 color, ImU32 hovered_color, ImU32 active_color,
              ImU32 text_color, const char* tooltip, ImVec2 size)
{
    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered_color);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, active_color);
    ImGui::PushStyleColor(ImGuiCol_Text, text_color);
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(4);
    if(tooltip && strlen(tooltip) > 0 && BeginItemTooltipStyled())
    {
        ImGui::TextUnformatted(tooltip);
        EndTooltipStyled();
    }
    return clicked;
}

void
RenderRemoteDownloadPopup(const char* popup_id, const char* file_name,
                          uint64_t downloaded, uint64_t total, bool finished,
                          bool& show)
{
    if(!show)
    {
        return;
    }

    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = ImGui::GetStyle();

    PopUpStyle popup_style;
    popup_style.PushPopupStyles();
    popup_style.PushTitlebarColors();
    popup_style.CenterPopup();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::SetNextWindowSize(ImVec2(440.0f, 0.0f));

    if(ImGui::BeginPopupModal(popup_id, nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, 4.0f));

        BeginPanelCard("##remote_dl_header", PanelCardTone::kFrame, ImVec2(16.0f, 10.0f), true,
                       &settings);
        {
            PanelIcon(ICON_ARROW_DOWN, Colors::kAccent, &settings);
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            ImGui::BeginGroup();
            ImGui::PushFont(nullptr, settings.GetFontManager().GetFontSize(FontSize::kMedLarge));
            ImGui::TextUnformatted("Remote Download");
            ImGui::PopFont();
            ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
            ImGui::TextUnformatted("Fetching the trace over SSH.");
            ImGui::PopStyleColor();
            ImGui::EndGroup();
        }
        EndPanelCard();

        BeginPanelCard("##remote_dl_body", PanelCardTone::kPanel, ImVec2(14.0f, 10.0f), true,
                       &settings);
        {
            ImGui::TextWrapped("%s", file_name ? file_name : "");
            ImGui::Spacing();
            if(total > 0)
            {
                float frac = static_cast<float>(downloaded) / static_cast<float>(total);
                if(frac > 1.0f)
                {
                    frac = 1.0f;
                }
                std::string label = std::to_string(downloaded / 1024) + " / " +
                                    std::to_string(total / 1024) + " KiB";
                ImGui::ProgressBar(frac, ImVec2(-FLT_MIN, 0.0f), label.c_str());
            }
            else
            {
                PanelFieldLabel("Starting...", false, &settings);
            }
        }
        EndPanelCard();

        if(finished)
        {
            ImGui::CloseCurrentPopup();
            show = false;
        }

        ImGui::PopStyleVar();  // ItemSpacing
        ImGui::EndPopup();
    }
    else
    {
        // Popup not actually open (e.g. dismissed); clear the flag so it can be
        // reopened on the next download.
        show = false;
    }

    ImGui::PopStyleVar(2);  // WindowPadding, WindowRounding
    popup_style.PopStyles();
}

#ifdef ROCPROFVIS_ENABLE_INTERNAL_BANNER

void
DrawInternalBuildBanner(const char* text /*= "Internal Build"*/)
{
    if(!text || !*text) return;

    ImDrawList*   dl   = ImGui::GetForegroundDrawList();
    const ImVec2& disp = ImGui::GetIO().DisplaySize;

    // Parameters. Scale with the font so the banner tracks ImGui's DPI font scaling.
    const float            ui_scale         = ImGui::GetFontSize() / BASE_DESIGN_FONT_SIZE;
    const float            ribbon_thickness = 20.0f * ui_scale;
    const float            min_base_length  = 150.0f * ui_scale;
    SettingsManager& settings    = SettingsManager::GetInstance();
    const ImU32      col_fill     = settings.GetColor(Colors::kBannerFill);
    const ImU32      col_border   = settings.GetColor(Colors::kBannerBorder);
    const ImU32      col_text     = settings.GetColor(Colors::kBannerText);

    // use precomputed cos/sin for 45 degrees to avoid trig calls
    static constexpr float c_45 = 0.70710678118f;
    static constexpr float s_45 = 0.70710678118f;

    // Measure text first
    ImVec2 ts = ImGui::CalcTextSize(text);

    // Required ribbon length so text fits
    const float desired_length = ts.x * 2.0f;
    const float ribbon_length =
        (desired_length > min_base_length) ? desired_length : min_base_length;

    const float half_len   = ribbon_length * 0.5f;
    const float half_thick = ribbon_thickness * 0.5f;

    // Center a rotated rectangle so it visually emerges from the top-right corner
    ImVec2 center = ImVec2(disp.x - half_len * 0.5f, half_len * 0.5f);

    // Axis‑aligned rect (local space before rotation)
    ImVec2 local[4] = { ImVec2(-half_len, -half_thick), ImVec2(half_len, -half_thick),
                        ImVec2(half_len, half_thick), ImVec2(-half_len, half_thick) };

    ImVec2 quad[4];
    for(int i = 0; i < 4; ++i)
    {
        const ImVec2& p = local[i];
        quad[i].x       = center.x + p.x * c_45 - p.y * s_45;
        quad[i].y       = center.y + p.x * s_45 + p.y * c_45;
    }

    dl->AddConvexPolyFilled(quad, 4, col_fill);
    dl->AddPolyline(quad, 4, col_border, true, 1.0f);

    // Add text at unrotated local position (centered), then rotate vertices
    ImVec2 text_local_pos(-ts.x * 0.5f, -ts.y * 0.5f);
    int    v_start = dl->VtxBuffer.Size;
    dl->AddText(nullptr, ImGui::GetFontSize(),
                ImVec2(center.x + text_local_pos.x, center.y + text_local_pos.y),
                col_text, text);
    int v_end = dl->VtxBuffer.Size;

    for(int i = v_start; i < v_end; ++i)
    {
        ImDrawVert&   v = dl->VtxBuffer[i];
        const ImVec2  p = v.pos - center;
        v.pos.x         = center.x + p.x * c_45 - p.y * s_45;
        v.pos.y         = center.y + p.x * s_45 + p.y * c_45;
    }
}
#endif // ROCPROFVIS_ENABLE_INTERNAL_BANNER

inline constexpr float MENU_ICON_COLUMN_EM = 1.0f;
inline constexpr float MENU_ICON_GAP_EM    = 0.7f;

static float
MenuIconColumnWidth()
{
    return ImGui::GetFontSize() * MENU_ICON_COLUMN_EM;
}

// Pads the label with leading spaces to leave room for the left-aligned icon.
static std::string
MenuLabelWithIconPadding(const char* label)
{
    const float offset  = MenuIconColumnWidth() + ImGui::GetFontSize() * MENU_ICON_GAP_EM;
    const float space_w = ImGui::CalcTextSize(" ").x;
    const int   pad     = space_w > 0.0f ? static_cast<int>(std::ceil(offset / space_w)) : 1;
    std::string padded(static_cast<size_t>(std::max(pad, 1)), ' ');
    padded += label;
    return padded;
}

// Pass the parent menu's draw list, captured before BeginMenu opens a submenu
// and retargets the current window's draw list.
static void
DrawMenuItemIcon(ImDrawList* draw_list, const char* icon, const ImVec2& row_start, bool enabled)
{
    if(!icon || icon[0] == '\0')
        return;

    ImFont*      icon_font = SettingsManager::GetInstance().GetFontManager().GetFont(FontType::kIcon);
    const float  font_size = ImGui::GetFontSize();
    const ImVec2 icon_size = icon_font->CalcTextSizeA(font_size, FLT_MAX, -1.0f, icon);
    const ImVec2 pos(row_start.x, row_start.y + (font_size - icon_size.y) * 0.5f);
    const ImU32  color = ImGui::GetColorU32(enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled);

    draw_list->AddText(icon_font, font_size, pos, color, icon);
}

bool
IconMenuItem(const char* icon, const char* label, bool enabled, bool selected)
{
    ImDrawList*  draw_list    = ImGui::GetWindowDrawList();
    const ImVec2 row_start    = ImGui::GetCursorScreenPos();
    std::string  padded_label = MenuLabelWithIconPadding(label);

    bool clicked = ImGui::MenuItem(padded_label.c_str(), nullptr, selected, enabled);
    DrawMenuItemIcon(draw_list, icon, row_start, enabled);

    if(clicked)
        ImGui::CloseCurrentPopup();
    return clicked;
}

bool
IconBeginMenu(const char* icon, const char* label)
{
    ImDrawList*  draw_list    = ImGui::GetWindowDrawList();
    const ImVec2 row_start    = ImGui::GetCursorScreenPos();
    std::string  padded_label = MenuLabelWithIconPadding(label);

    bool open = ImGui::BeginMenu(padded_label.c_str());
    DrawMenuItemIcon(draw_list, icon, row_start, true);

    return open;
}

bool
CopyableTextUnformatted(
    const char* text, std::string_view unique_id, std::string_view notification,
    bool one_click_copy, bool context_menu,
    std::function<void(const char* value_to_copy)> menu_func)
{
    bool clicked = false;
    if(!unique_id.empty())
        ImGui::PushID(unique_id.data());

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

    if(ImGui::Button(text, ImVec2(0, 0)))
    {
        clicked = true;
        if(one_click_copy)
        {
            ImGui::SetClipboardText(text);
            if(!notification.empty())
            {
                NotificationManager::GetInstance().Show(notification.data(),
                                                        NotificationLevel::Info);
            }
        }
    }

    if(context_menu)
    {
        auto style = SettingsManager::GetInstance().GetDefaultStyle();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.WindowPadding);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, style.ItemSpacing);
        if(menu_func)
        {
            menu_func(text);
        }
        else if(ImGui::BeginPopupContextItem())
        {
            if(IconMenuItem(ICON_COPY, "Copy"))
            {
                ImGui::SetClipboardText(text);
                if(!notification.empty())
                {
                    NotificationManager::GetInstance().Show(notification.data(),
                                                            NotificationLevel::Info);
                }
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);
    }

    if(one_click_copy)
    {
        if(ImGui::IsItemHovered())
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    if(!unique_id.empty())
    {
        ImGui::PopID();
    }
    return clicked;
}

void
PositionCell(int col)
{
    if(col > 0)
        ImGui::TableSetColumnIndex(col);
    else
        ImGui::SameLine();
}

bool
RenderRowHitbox(const char* hitbox_id, int row, int column_count,
                CellMenuTarget& target, bool& open)
{
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));
    bool clicked = ImGui::Selectable(hitbox_id, false,
                                     ImGuiSelectableFlags_SpanAllColumns |
                                         ImGuiSelectableFlags_AllowOverlap,
                                     ImVec2(0.0f, 0.0f));
    bool hovered = clicked ||
                   ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
                                        ImGuiHoveredFlags_AllowWhenOverlappedByItem);
    if(ImGui::IsItemClicked(ImGuiMouseButton_Right))
    {
        int hovered_col = ImGui::TableGetHoveredColumn();
        target.row      = row;
        target.column =
            (hovered_col >= 0 && hovered_col < column_count) ? hovered_col : 0;
        open = true;
    }
    ImGui::PopStyleColor(3);
    return hovered;
}

void
CaptureCellRightClick(int col, int row, CellMenuTarget& target, bool& open)
{
    if(ImGui::IsItemClicked(ImGuiMouseButton_Right))
    {
        target.row    = row;
        target.column = col;
        open          = true;
    }
}

bool
BeginCellContextMenu(const char* popup_id)
{
    const ImGuiStyle& style = SettingsManager::GetInstance().GetDefaultStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, style.ItemSpacing);
    bool open = ImGui::BeginPopup(popup_id);
    if(!open)
        ImGui::PopStyleVar(2);
    return open;
}

void
EndCellContextMenu()
{
    ImGui::EndPopup();
    ImGui::PopStyleVar(2);
}

void
AddCopyRowCellMenuItems(const std::string* cells, int column_count, int column)
{
    if(IconMenuItem(ICON_COPY, "Copy Row Data"))
    {
        std::string row_text;
        for(int c = 0; c < column_count; c++)
        {
            if(c > 0) row_text += ',';
            row_text += cells[c];
        }
        ImGui::SetClipboardText(row_text.c_str());
        NotificationManager::GetInstance().Show(COPY_ROW_DATA_NOTIFICATION.data(),
                                                NotificationLevel::Info);
    }
    if(IconMenuItem(ICON_COPY, "Copy Cell Data"))
    {
        if(column >= 0 && column < column_count)
        {
            ImGui::SetClipboardText(cells[column].c_str());
            NotificationManager::GetInstance().Show(COPY_DATA_NOTIFICATION.data(),
                                                    NotificationLevel::Info);
        }
    }
}

}   // namespace View
}   // namespace RocProfVis
