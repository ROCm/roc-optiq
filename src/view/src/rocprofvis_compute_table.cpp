// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "rocprofvis_compute_table.h"

using namespace RocProfVis::View;

ComputeTableView::ComputeTableView(std::shared_ptr<ComputeDataProvider> data_provider) {}

ComputeTableView::~ComputeTableView() {}

void ComputeTableView::RenderMenuBar()
{
    ImVec2 cursor_position = ImGui::GetCursorScreenPos();
    ImVec2 content_region = ImGui::GetContentRegionAvail();
    ImGui::GetWindowDrawList()->AddRectFilled(
    ImVec2(cursor_position.x, cursor_position.y),
    ImVec2(cursor_position.x + content_region.x,
            cursor_position.y + ImGui::GetFrameHeightWithSpacing()),
    ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg]), 
    0.0f);

    ImGui::SetCursorScreenPos(ImVec2(cursor_position.x + ImGui::GetContentRegionAvail().x - 350, cursor_position.y));
    ImGui::Text("Search");
    char searchTerm[32] = "";
    ImGui::SameLine();
    ImGui::SetNextItemWidth(350 - ImGui::CalcTextSize("Search").x);
    ImGui::InputText("##compute_table_search_bar", searchTerm, 32);
}

void ComputeTableView::Render()
{
    RenderMenuBar();
    if (ImGui::BeginTabBar("compute_table_tab_bar", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Category A"))
        {
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Category B"))
        {
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Category C"))
        {
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Category D"))
        {
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}
