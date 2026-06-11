// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_widget.h"
#include "icons/rocprovfis_icon_defines.h"
#include "imgui.h"
#include "rocprofvis_core.h"
#include "rocprofvis_debug_window.h"
#include "rocprofvis_utils.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_notification_manager.h"
#include "rocprofvis_settings_manager.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>

namespace RocProfVis
{
namespace View
{

RocWidget::~RocWidget() { spdlog::debug("RocWidget object destroyed"); }

void
RocWidget::Render()
{}

std::string
RocWidget::GenUniqueName(std::string name)
{
    std::ostringstream oss;
    oss << name.c_str() << std::hex << reinterpret_cast<uintptr_t>(this);
    return oss.str();
}

const std::string&
RocWidget::GetWidgetName() const
{
    return m_widget_name;
}

//------------------------------------------------------------------
RocCustomWidget::RocCustomWidget(const std::function<void()>& callback)
: m_callback(callback)
{}

void
RocCustomWidget::SetCallback(const std::function<void()>& callback)
{
    m_callback = callback;
}
void
RocCustomWidget::Render()
{
    if(m_callback)
    {
        m_callback();
    }
}

LayoutItem::Ptr
LayoutItem::CreateFromWidget(std::shared_ptr<RocWidget> widget, float w, float h)
{
    Ptr item     = std::make_shared<LayoutItem>(w, h);
    item->m_item = widget;
    return item;
}


void
WithPadding(float left, float right, float top, float bottom,
            const std::function<void()>& content)
{
    if(top > 0.0f) ImGui::Dummy(ImVec2(0, top));

    // No border flags for invisible borders
    if(ImGui::BeginTable("##padding_table", 3, ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("LeftPad", ImGuiTableColumnFlags_WidthFixed, left);
        ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("RightPad", ImGuiTableColumnFlags_WidthFixed, right);

        ImGui::TableNextRow();

        // Left padding
        ImGui::TableSetColumnIndex(0);
        if(left > 0.0f) ImGui::Dummy(ImVec2(left, 0));

        // Content
        ImGui::TableSetColumnIndex(1);
        ImGui::BeginGroup();
        content();
        ImGui::EndGroup();

        // Right padding
        ImGui::TableSetColumnIndex(2);
        if(right > 0.0f) ImGui::Dummy(ImVec2(right, 0));

        ImGui::EndTable();
    }

    if(bottom > 0.0f) ImGui::Dummy(ImVec2(0, bottom));
}

bool
IconMenuItem(const char* icon, const char* label)
{
    ImFont* icon_font = SettingsManager::GetInstance().GetFontManager().GetFont(FontType::kIcon);

    bool clicked = ImGui::Selectable(("##menu_item" + std::string(label)).c_str(), false,
                                     ImGuiSelectableFlags_SpanAllColumns,
                                     ImVec2(0, ImGui::GetTextLineHeightWithSpacing()));
    ImGui::SameLine(0.0f, 0.0f);

    ImGui::BeginGroup();
    ImGui::PushFont(icon_font);
    ImGui::TextUnformatted(icon);
    ImGui::PopFont();
    ImGui::SameLine(0.f, ImGui::GetStyle().ItemSpacing.x);
    ImGui::TextUnformatted(label);
    ImGui::EndGroup();

    if(clicked)
        ImGui::CloseCurrentPopup();
    return clicked;
}

bool
IconBeginMenu(const char* icon, const char* label)
{
    ImFont* icon_font =
        SettingsManager::GetInstance().GetFontManager().GetFont(FontType::kIcon);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2      row_start = ImGui::GetCursorScreenPos();
    float       font_size = ImGui::GetFontSize();
    float       icon_w    = icon_font->CalcTextSizeA(font_size, FLT_MAX, -1.0f, icon).x;
    float       space_w   = ImGui::CalcTextSize(" ").x;
    float       gap       = ImGui::GetStyle().ItemInnerSpacing.x;

    int pad_spaces =
        space_w > 0.0f ? static_cast<int>(std::ceil((icon_w + gap) / space_w)) : 1;
    std::string padded_label(static_cast<size_t>(std::max(pad_spaces, 1)), ' ');
    padded_label += label;

    bool open = ImGui::BeginMenu(padded_label.c_str());

    draw_list->AddText(icon_font, font_size, row_start,
                       ImGui::GetColorU32(ImGuiCol_Text), icon);
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
        if (menu_func)
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

PopUpStyle::PopUpStyle()
: m_style_var_count(0)
, m_color_count(0)
{}

PopUpStyle::~PopUpStyle() { PopStyles(); }

void
PopUpStyle::PushPopupStyles()
{
    SettingsManager& settings = SettingsManager::GetInstance();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        settings.GetDefaultStyle().WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        settings.GetDefaultStyle().ItemSpacing);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,
                        settings.GetDefaultStyle().WindowRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        settings.GetDefaultStyle().FramePadding);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,
                        settings.GetDefaultStyle().FrameRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,
                        settings.GetDefaultStyle().ChildRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding,
                        settings.GetDefaultStyle().PopupRounding);
    m_style_var_count += 7;
}

void
PopUpStyle::CenterPopup()
{
    ImVec2 center_pos = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center_pos, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
}

void
PopUpStyle::PushTitlebarColors()
{
    SettingsManager& settings   = SettingsManager::GetInstance();
    ImU32            grey_color = settings.GetColor(Colors::kBorderGray);

    ImGui::PushStyleColor(ImGuiCol_TitleBg, grey_color);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, grey_color);
    ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, grey_color);

    m_color_count += 3;
}

void
PopUpStyle::PopStyles()
{
    if(m_style_var_count > 0)
    {
        ImGui::PopStyleVar(m_style_var_count);
        m_style_var_count = 0;
    }
    if(m_color_count > 0)
    {
        ImGui::PopStyleColor(m_color_count);
        m_color_count = 0;
    }
}

}  // namespace View
}  // namespace RocProfVis
