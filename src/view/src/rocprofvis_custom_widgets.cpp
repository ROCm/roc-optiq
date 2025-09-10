// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_custom_widgets.h"

namespace RocProfVis
{
namespace View
{

void
RocProfVisCustomWidget::WithPadding(float left, float right, float top, float bottom, float height,
                                    const std::function<void()>& content)
{
    // Top padding
    if(top > 0.0f) ImGui::Dummy(ImVec2(0, top));

    // Left padding
    if(left > 0.0f)
    {
        ImGui::Dummy(ImVec2(left, 0));
        ImGui::SameLine(0.0f, 0.0f);
    }

    // Calculate width for the child: available width minus right padding
    float content_width = ImGui::GetContentRegionAvail().x - right;
    if(content_width < 0.0f) content_width = 0.0f;


    ImGui::BeginChild("##padded_child", ImVec2(content_width, height), false,
                      ImGuiChildFlags_AutoResizeY);
    content();
    ImGui::EndChild();

    // Right padding
    if(right > 0.0f)
    {
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::Dummy(ImVec2(right, 0));
    }

    // Bottom padding
    if(bottom > 0.0f) ImGui::Dummy(ImVec2(0, bottom));
}

}  // namespace View
}  // namespace RocProfVis
