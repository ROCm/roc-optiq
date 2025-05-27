// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_compute_roofline.h"

namespace RocProfVis
{
namespace View
{

ComputeRooflineView::ComputeRooflineView(std::string owner_id, std::shared_ptr<ComputeDataProvider> data_provider) 
: m_roofline(nullptr)
, m_owner_id(owner_id)
{
    m_roofline = std::make_unique<ComputeMetricRoofline>(data_provider, kRooflineGroupByKernel);
}

ComputeRooflineView::~ComputeRooflineView() {}

void ComputeRooflineView::Update()
{
    if (m_roofline)
    {
        m_roofline->Update();
    }
}

void ComputeRooflineView::RenderMenuBar()
{
    ImVec2 cursor_position = ImGui::GetCursorScreenPos();
    ImVec2 content_region = ImGui::GetContentRegionAvail();
    ImGui::GetWindowDrawList()->AddRectFilled(
    ImVec2(cursor_position.x, cursor_position.y),
    ImVec2(cursor_position.x + content_region.x,
            cursor_position.y + ImGui::GetFrameHeightWithSpacing()),
    ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg]), 
    0.0f);

    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));
    if (ImGui::Button("View"))
    {
        ImGui::OpenPopup("compute_view_menu");
    }
    ImGui::PopStyleColor();
    ImGui::SetNextWindowPos(ImGui::GetCursorScreenPos());
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    if (ImGui::BeginPopup("compute_view_menu"))
    {
        if (ImGui::MenuItem("Group by Dispatch", "", m_roofline->GetGroupMode() == kRooflineGroupByDispatch))
        {
            m_roofline->SetGroupMode(kRooflineGroupByDispatch);
        }
        if(ImGui::MenuItem("Group by Kernel", "", m_roofline->GetGroupMode() == kRooflineGroupByKernel))
        {
            m_roofline->SetGroupMode(kRooflineGroupByKernel);
        }

        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
}

void ComputeRooflineView::Render()
{
    RenderMenuBar();

    ImGui::BeginChild("compute_roofline", ImVec2(-1, -1), ImGuiChildFlags_Borders);
    m_roofline->Render();
    ImGui::EndChild();
}

}  // namespace View
}  // namespace RocProfVis
