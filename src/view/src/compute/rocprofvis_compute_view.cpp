// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_view.h"
#include "rocprofvis_settings_manager.h"

namespace RocProfVis
{
namespace View
{

ComputeView::ComputeView()
: m_view_created(false)
{
    m_tool_bar = std::make_shared<RocCustomWidget>([this]() { this->RenderToolbar(); });
    m_widget_name = GenUniqueName("ComputeView");
}

ComputeView::~ComputeView() {}

void
ComputeView::Update()
{
    auto last_state = m_data_provider.GetState();
    m_data_provider.Update();

    if(!m_view_created)
    {
        CreateView();
        m_view_created = true;
    }

    auto new_state = m_data_provider.GetState();

    // new file loaded
    if(last_state != new_state && new_state == ProviderState::kReady)
    {
    }
}

void
ComputeView::CreateView()
{}

void
ComputeView::DestroyView()
{
    m_view_created = false;
}

bool
ComputeView::LoadTrace(rocprofvis_controller_t* controller, const std::string& file_path)
{
    bool result = false;
    result      = m_data_provider.FetchTrace(controller, file_path);
    return result;
}

void
ComputeView::Render()
{
    if(m_data_provider.GetState() == ProviderState::kLoading)
    {
        RenderLoadingScreen(m_data_provider.GetProgressMessage());
    }
}

std::shared_ptr<RocWidget>
ComputeView::GetToolbar()
{
    return m_tool_bar;
}

void
ComputeView::RenderEditMenuOptions()
{
    ImGui::MenuItem("Compute Edit Item");
    ImGui::Separator();
}

void
ComputeView::RenderToolbar()
{
    ImGuiStyle& style          = ImGui::GetStyle();
    ImVec2      frame_padding  = style.FramePadding;
    float       frame_rounding = style.FrameRounding;

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::BeginChild("Toolbar", ImVec2(-1, 0),
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_FrameStyle);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, frame_padding);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, frame_rounding);
    ImGui::AlignTextToFramePadding();

    ImGui::Button("Compute Toolbar", ImVec2(-1, 0));

    // pop content style
    ImGui::PopStyleVar(2);
    ImGui::EndChild();
    // pop child window style
    ImGui::PopStyleVar(2);
}

}  // namespace View
}  // namespace RocProfVis
