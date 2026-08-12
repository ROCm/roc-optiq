// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_widget.h"
#include "imgui.h"
#include "rocprofvis_core.h"
#include "rocprofvis_debug_window.h"
#include "rocprofvis_utils.h"
#include "rocprofvis_settings_manager.h"
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
