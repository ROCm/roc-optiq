// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_code_table.h"

#include "widgets/rocprofvis_gui_helpers.h"

namespace RocProfVis
{
namespace View
{

ComputeCodeTable::ComputeCodeTable(const char* id, int32_t columns, ImGuiTableFlags flags,
                                   const ImVec2& window_padding)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, window_padding);
    m_open = ImGui::BeginTable(id, columns, flags);
    if(m_open)
    {
        ImGui::TableSetupScrollFreeze(0, 1);
    }
}

ComputeCodeTable::~ComputeCodeTable()
{
    if(m_row_styles_pushed)
    {
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
    }
    if(m_open)
    {
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

void
ComputeCodeTable::BeginRows(ImU32 background_color)
{
    IM_ASSERT(m_open && !m_row_styles_pushed);
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,
                        ImVec2(ImGui::GetStyle().CellPadding.x, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_Header, background_color);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, background_color);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, background_color);
    m_row_styles_pushed = true;
}

void
ComputeCodeTable::RenderCenteredHeader(int column, const char* label)
{
    if(ImGui::TableSetColumnIndex(column))
    {
        CenterNextTextItem(label);
        ImGui::TableHeader(label);
    }
}

void
ComputeCodeTable::RenderHeaderWithTooltip(int column, const char* label,
                                          const HeaderTooltipText& tooltip,
                                          void (*render_user_content)())
{
    if(!ImGui::TableSetColumnIndex(column))
    {
        return;
    }
    ImGui::TableHeader(label);
    if(!ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
    {
        return;
    }

    BeginTooltipStyled();
    if(tooltip.user_description)
    {
        ImGui::TextUnformatted(tooltip.user_description);
    }
    if(render_user_content)
    {
        render_user_content();
    }
#ifdef ROCPROFVIS_DEVELOPER_MODE
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("Developer information");
    ImGui::TextUnformatted(tooltip.developer_information);
#endif
    EndTooltipStyled();
}

}  // namespace View
}  // namespace RocProfVis
