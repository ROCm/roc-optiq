// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_debug_window.h"

#include "imgui.h"
#include "rocprofvis_settings_manager.h"

using namespace RocProfVis::View;

namespace
{
constexpr float DEBUG_WINDOW_DEFAULT_WIDTH  = 400.0f;
constexpr float DEBUG_WINDOW_DEFAULT_HEIGHT = 300.0f;
constexpr float DEBUG_WINDOW_NAV_HEIGHT     = 30.0f;
constexpr float DEBUG_WINDOW_NAV_PADDING_X  = 10.0f;
constexpr float DEBUG_WINDOW_NAV_PADDING_Y  = 2.0f;
constexpr float DEBUG_WINDOW_NAV_ITEM_SPACING = 4.0f;
constexpr float DEBUG_WINDOW_CONTENT_PADDING  = 4.0f;
}  // namespace

DebugWindow* DebugWindow::s_instance = nullptr;

DebugWindow*
DebugWindow::GetInstance()
{
    if(!s_instance)
    {
        s_instance = new DebugWindow();
    }

    return s_instance;
}

DebugWindow::DebugWindow()
{
    LayoutItem nav_bar(0, DEBUG_WINDOW_NAV_HEIGHT);
    nav_bar.m_item = std::make_shared<RocCustomWidget>([this]() { this->RenderNav(); });
    nav_bar.m_window_padding = ImVec2(DEBUG_WINDOW_NAV_PADDING_X, DEBUG_WINDOW_NAV_PADDING_Y);
    nav_bar.m_item_spacing   = ImVec2(DEBUG_WINDOW_NAV_ITEM_SPACING, DEBUG_WINDOW_NAV_ITEM_SPACING);
    nav_bar.m_bg_color       =
        ImColor(SettingsManager::GetInstance().GetColor(Colors::kDebugNavBarBg));

    LayoutItem content(0, 0.0f);
    content.m_item = std::make_shared<RocCustomWidget>([this]() { this->RenderMessages(); });
    content.m_window_padding = ImVec2(DEBUG_WINDOW_CONTENT_PADDING, DEBUG_WINDOW_CONTENT_PADDING);
    content.m_child_flags    = ImGuiChildFlags_None;

    std::vector<LayoutItem> layout_items;
    layout_items.push_back(nav_bar);
    layout_items.push_back(content);

    m_main_container = std::make_shared<VFixedContainer>(layout_items);
}

void
DebugWindow::RenderNav()
{
    if(ImGui::Button("Clear"))
    {
        ClearTransient();
    }
}

void
DebugWindow::RenderMessages()
{
    ImGui::TextUnformatted("Debug Messages");
    ImGui::Separator();
    for(const std::string& message : m_transient_debug_messages)
    {
        ImGui::TextUnformatted(message.c_str());
    }
}

void
DebugWindow::Render()
{
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::SetNextWindowSize(ImVec2(DEBUG_WINDOW_DEFAULT_WIDTH, DEBUG_WINDOW_DEFAULT_HEIGHT),
                              ImGuiCond_FirstUseEver);
    ImGui::Begin("Debug Window", nullptr, ImGuiWindowFlags_None);
    m_main_container->Render();
    ImGui::End();

    ImGui::PopStyleVar(2);
}

void
DebugWindow::AddDebugMessage(const std::string& message)
{
    m_transient_debug_messages.push_back(message);
}

void
DebugWindow::ClearTransient()
{
    m_transient_debug_messages.clear();
}
