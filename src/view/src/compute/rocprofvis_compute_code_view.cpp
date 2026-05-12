// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_code_view.h"
#include "rocprofvis_font_manager.h"

namespace RocProfVis
{
namespace View
{

ComputeCodeView::ComputeCodeView()
: RocWidget()
, m_settings(SettingsManager::GetInstance())
{}

void
ComputeCodeView::Render()
{
    ImGui::PushFont(m_settings.GetFontManager().GetFont(FontType::kCode), 0.0f);

    ImGui::BeginChild("CodeBlock", ImVec2(0, 300), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::TextUnformatted(m_test_code.c_str());

    ImGui::EndChild();

    ImGui::PopFont();
}

}  // namespace View
}  // namespace RocProfVis
