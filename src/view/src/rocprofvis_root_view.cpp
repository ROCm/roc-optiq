// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_root_view.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_gui_helpers.h"

namespace RocProfVis
{
namespace View
{

RootView::RootView()
: m_settings_manager(SettingsManager::GetInstance())
{}

RootView::~RootView() {}

std::shared_ptr<RocWidget>
RootView::GetToolbar()
{
    return nullptr;
}

void
RootView::RenderEditMenuOptions()
{}

void
RootView::RenderLoadingScreen(const char* progress_label)
{
    ImVec2 content_region = ImGui::GetContentRegionAvail();
    ImVec2 window_pos     = ImGui::GetWindowPos();
    ImVec2 cursor_pos     = ImGui::GetCursorPos();

    ImVec2 overlay_min = ImVec2(window_pos.x + cursor_pos.x, window_pos.y + cursor_pos.y);
    ImVec2 overlay_max =
        ImVec2(overlay_min.x + content_region.x, overlay_min.y + content_region.y);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(overlay_min, overlay_max,
                             m_settings_manager.GetColor(Colors::kLoadingScreenColor));

    // Calculate center position for loading indicator
    ImVec2 center_screen = ImVec2(overlay_min.x + content_region.x * 0.5f,
                                  overlay_min.y + content_region.y * 0.5f);

    const char* label      = "Please wait...";
    ImVec2      label_size = ImGui::CalcTextSize(label);

    ImVec2 progress_label_size = ImGui::CalcTextSize(progress_label);

    float item_spacing = 10.0f;

    float dot_radius  = 5.0f;
    int   num_dots    = 3;
    float dot_spacing = 5.0f;
    float anim_speed  = 5.0f;

    ImVec2 dot_size = MeasureLoadingIndicatorDots(dot_radius, num_dots, dot_spacing);

    // Calculate total height and starting Y position
    float total_height =
        label_size.y + dot_size.y + progress_label_size.y + item_spacing * 2;
    float start_y = center_screen.y - total_height * 0.5f;

    // Draw label centered using ImGui text
    ImVec2 label_pos = ImVec2(center_screen.x - label_size.x * 0.5f, start_y);
    ImGui::SetCursorScreenPos(label_pos);
    ImGui::PushStyleColor(ImGuiCol_Text, m_settings_manager.GetColor(Colors::kTextMain));
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();

    // Draw dots centered
    ImVec2 dot_pos = ImVec2(center_screen.x - dot_size.x * 0.5f,
                            start_y + label_size.y + item_spacing);
    ImGui::SetCursorScreenPos(dot_pos);
    RenderLoadingIndicatorDots(dot_radius, num_dots, dot_spacing,
                               IM_COL32(85, 85, 85, 255), anim_speed);

    // Draw progress label centered using ImGui text
    ImVec2 progress_pos = ImVec2(center_screen.x - progress_label_size.x * 0.5f,
                                 start_y + label_size.y + dot_size.y + item_spacing * 2);
    ImGui::SetCursorScreenPos(progress_pos);
    ImGui::PushStyleColor(ImGuiCol_Text, m_settings_manager.GetColor(Colors::kTextMain));
    ImGui::TextUnformatted(progress_label);
    ImGui::PopStyleColor();
}

}  // namespace View
}  // namespace RocProfVis
