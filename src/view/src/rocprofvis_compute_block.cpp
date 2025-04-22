// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_compute_block.h"

namespace RocProfVis
{
namespace View
{

constexpr int WINDOW_PADDING_DEFAULT = 8;
constexpr int LEVEL_HISTORY_LIMIT = 5;

ComputeBlockDiagramNavHelper::ComputeBlockDiagramNavHelper()
: m_current_level(kGPULevel)
{}

block_diagram_level_t
ComputeBlockDiagramNavHelper::Current()
{
    return m_current_level;
}

bool RocProfVis::View::ComputeBlockDiagramNavHelper::BackAvailable()
{
    return !m_previous_levels.empty();
}

bool RocProfVis::View::ComputeBlockDiagramNavHelper::ForwardAvailable()
{
    return !m_next_levels.empty();
}

void ComputeBlockDiagramNavHelper::GoBack()
{
    if (!m_previous_levels.empty())
    {
        m_next_levels.push_back(m_current_level);
        m_current_level = m_previous_levels.back();
        m_previous_levels.pop_back();
    }
}

void ComputeBlockDiagramNavHelper::GoForward()
{
    if (!m_next_levels.empty())
    {
        m_previous_levels.push_back(m_current_level);
        m_current_level = m_next_levels.back();        
        m_next_levels.pop_back();
    }
}

void ComputeBlockDiagramNavHelper::Go(block_diagram_level_t level)
{
    if (m_current_level != level)
    {
        m_previous_levels.push_back(m_current_level);
        m_current_level = level;
        m_next_levels = {};
        if (m_previous_levels.size() > LEVEL_HISTORY_LIMIT)
        {
            m_previous_levels.pop_front();
        }
    }
}

ComputeBlockDiagram::ComputeBlockDiagram()
: m_content_region(ImVec2(-1, -1))
, m_content_region_center(ImVec2(-1, -1))
, m_draw_list(nullptr)
, m_navigation(nullptr)
{
    m_navigation = std::make_unique<ComputeBlockDiagramNavHelper>();
}

ComputeBlockDiagram::~ComputeBlockDiagram() 
{}

bool ComputeBlockDiagram::BlockButton(const char* name, const char* tooltip, bool navigation, 
                                                    ImVec2 rel_pos, ImVec2 rel_size)
{
    ImVec2 abs_size(rel_size.x * m_content_region.x, rel_size.y * m_content_region.y);
    ImVec2 abs_pos(m_content_region_center.x + m_content_region.x * rel_pos.x - abs_size.x / 2, m_content_region_center.y + m_content_region.y * rel_pos.y - abs_size.y / 2);

    ImGui::SetCursorScreenPos(abs_pos);
    ImGui::SetNextItemAllowOverlap();
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImGui::GetStyleColorVec4(ImGuiCol_Button));
    bool pressed = ImGui::Button(std::string("##").append(name).c_str(), abs_size);
    ImGui::PopStyleColor();
    if(ImGui::IsItemHovered())
    {
        m_draw_list->AddRect(abs_pos, ImVec2(abs_pos.x + abs_size.x, abs_pos.y + abs_size.y), IM_COL32(0, 0, 0, 255));
    }
    if(strlen(tooltip) > 0)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(WINDOW_PADDING_DEFAULT, WINDOW_PADDING_DEFAULT));
        ImGui::SetItemTooltip(tooltip);
        ImGui::PopStyleVar();
    }
    ImGui::SetCursorScreenPos(ImVec2(abs_pos.x + 0.01f * m_content_region.x, abs_pos.y + 0.01f * m_content_region.y));
    ImGui::Text(name);
    if (navigation)
    {
        m_draw_list->AddTriangleFilled(
            ImVec2(abs_pos.x + abs_size.x - 0.02f * m_content_region.x,
                   abs_pos.y + 0.01f * m_content_region.y),
            ImVec2(abs_pos.x + abs_size.x - 0.01f * m_content_region.x,
                   abs_pos.y + 0.01f * m_content_region.y),
            ImVec2(abs_pos.x + abs_size.x - 0.01f * m_content_region.x,
                   abs_pos.y + 0.02f * m_content_region.y),
            IM_COL32(0, 0, 0, 255));
    }
    return pressed;
}

void
ComputeBlockDiagram::Link(ImVec2 rel_left, ImVec2 rel_right, ImGuiDir direction)
{
    ImVec2 abs_left(m_content_region_center.x + m_content_region.x * rel_left.x, m_content_region_center.y + m_content_region.y * rel_left.y);
    ImVec2 abs_right(m_content_region_center.x + m_content_region.x * rel_right.x, m_content_region_center.y + m_content_region.y * rel_right.y);
    m_draw_list->AddLine(abs_left, abs_right, IM_COL32(0, 0, 0, 255));
    if (direction == ImGuiDir_Left || direction == ImGuiDir_None)
    {
        m_draw_list->AddTriangleFilled(
        abs_left,
        ImVec2(abs_left.x + 0.01f * m_content_region.x,
                abs_left.y - 0.01f * m_content_region.y),
        ImVec2(abs_left.x + 0.01f * m_content_region.x,
                abs_left.y + 0.01f * m_content_region.y),
        IM_COL32(0, 0, 0, 255));
    }
    if (direction == ImGuiDir_Right || direction == ImGuiDir_None)
    {
        m_draw_list->AddTriangleFilled(
        abs_right,
        ImVec2(abs_right.x - 0.01f * m_content_region.x,
                abs_right.y - 0.01f * m_content_region.y),
        ImVec2(abs_right.x - 0.01f * m_content_region.x,
                abs_right.y + 0.01f * m_content_region.y),
        IM_COL32(0, 0, 0, 255));
    }
}

void ComputeBlockDiagram::RenderGPULevel()
{
    BlockButton("GPU", "");
    BlockButton("CP", "Command processor", false, ImVec2(0, -0.4f), ImVec2(0.9f, 0.1f));
    BlockButton("CPC", "Command processor packet processor", false, ImVec2(-0.2175f, -0.4f), ImVec2(0.4075f, 0.05f));
    BlockButton("CPF", "Command processor fetcher", false, ImVec2(0.2175f, -0.4f), ImVec2(0.4075f, 0.05f));
    if (BlockButton("Fabric", "", true, ImVec2(0, 0.4f), ImVec2(0.9f, 0.1f)))
    {
        m_navigation->Go(kCacheLevel);
    }
    if (BlockButton("L2", "L2 cache", true, ImVec2(0, 0), ImVec2(0.1f, 0.6f)))
    {
        m_navigation->Go(kCacheLevel);
    }

    for (int row = 0; row < 2; row++) 
    {
        for (int col = 0; col < 2; col++)
        {
            ImGui::PushID(row * 2 + col);
            if(BlockButton("SE", "Shader engine", true, ImVec2(0.275f - col * 0.55f, 0.165f - row * 0.33f), ImVec2(0.35f, 0.27f)))
            {
                m_navigation->Go(kShaderEngineLevel);
            }
            ImGui::PopID();
        }
    }
}

void ComputeBlockDiagram::RenderShaderEngineLevel()
{
    BlockButton("SE", "Shader engine");
    BlockButton("SPI", "Workgroup manager", false, ImVec2(0.35f, -0.25f), ImVec2(0.2f, 0.4f));
    if (BlockButton("sL1D", "Scaler L1 data cache", true, ImVec2(0.35f, 0.1f), ImVec2(0.2f, 0.2f)))
    {
        m_navigation->Go(kCacheLevel);
    }
    if (BlockButton("L1I", "L1 instruction cache", true, ImVec2(0.35f, 0.35f), ImVec2(0.2f, 0.2f)))
    {
        m_navigation->Go(kCacheLevel);
    }

    for (int row = 0; row < 8; row ++)
    {
        for (int col = 0; col < 2; col ++)
        {
            ImGui::PushID(row * 2 + col);
            if(BlockButton("CU", "Compute unit", true, ImVec2(0.05f - col * 0.35f, 0.416f - row * 0.1188f), ImVec2(0.3f, 0.0688f)))
            {
                m_navigation->Go(kComputeUnitLevel);
            }
            ImGui::PopID();
        }
    }
}

void ComputeBlockDiagram::RenderComputeUnitLevel()
{
    BlockButton("CU", "Compute unit");
    BlockButton("Scheduler", "", false, ImVec2(-0.075f, -0.4f), ImVec2(0.75f, 0.1f));
    if(BlockButton("vL1D", "Vector L1 data cache", true, ImVec2(0.4f, 0), ImVec2(0.1f, 0.9f)))
    {
        m_navigation->Go(kCacheLevel);
    }
    BlockButton("VALU", "Vector arithmetic logic unit", false, ImVec2(-0.395f, 0.075f), ImVec2(0.11f, 0.75f));
    BlockButton("SALU", "Scaler arithmetic logic unit", false, ImVec2(-0.235f, 0.075f), ImVec2(0.11f, 0.75f));
    BlockButton("MFMA", "Matrix fused multiply-add", false, ImVec2(-0.075f, 0.075f), ImVec2(0.11f, 0.75f));
    BlockButton("Branch", "", false, ImVec2(0.085f, 0.075f), ImVec2(0.11f, 0.75f));
    BlockButton("LDS", "Local data share", false, ImVec2(0.245f, 0.075f), ImVec2(0.11f, 0.75f));
}

void ComputeBlockDiagram::RenderCacheLevel()
{
    BlockButton("GPU", "", false, ImVec2(-0.125f, 0),ImVec2(0.75f, 1));
    BlockButton("CP", "Command processor", false, ImVec2(-0.35f, -0.275f), ImVec2(0.2f, 0.35f));
    BlockButton("CPC", "Command processor packet processor", false, ImVec2(-0.35f, -0.35f), ImVec2(0.1f, 0.1f));
    BlockButton("CPF", "Command processor fetcher", false, ImVec2(-0.35f, -0.20f), ImVec2(0.1f, 0.1f));
    if (BlockButton("SE", "Shader engine", true, ImVec2(-0.325f, 0.2f), ImVec2(0.25f, 0.5f)))
    {
        m_navigation->Go(kShaderEngineLevel);
    }
    if (BlockButton("CU", "Compute unit", true, ImVec2(-0.325f, 0.05f), ImVec2(0.2f, 0.15f)))
    {
        m_navigation->Go(kComputeUnitLevel);
    }

    BlockButton("vL1D", "Vector L1 data cache", false, ImVec2(-0.325f, 0.05f), ImVec2(0.15f, 0.1f));
    BlockButton("sL1D", "Scaler L1 data cache", false, ImVec2(-0.325f, 0.2f), ImVec2(0.15f, 0.1f));
    BlockButton("L1I", "L1 instruction cache", false, ImVec2(-0.325f, 0.35f), ImVec2(0.15f, 0.1f));
    BlockButton("L2", "L2 cache", false, ImVec2(-0.05f, 0), ImVec2(0.1f, 0.9f));
    BlockButton("Fabric", "", false, ImVec2(0.15f, 0), ImVec2(0.1f, 0.55f));
    BlockButton("HBM", "", false, ImVec2(0.375f, -0.2f), ImVec2(0.15f, 0.15f));
    BlockButton("PCIe", "", false, ImVec2(0.375f, 0), ImVec2(0.15f, 0.15f));
    BlockButton("CPU DRAM", "", false, ImVec2(0.375f, 0.2f), ImVec2(0.15f, 0.15f));

    Link(ImVec2(-0.3f, -0.35f), ImVec2(-0.1f, -0.35f)); // CPC - L2
    Link(ImVec2(-0.3f, -0.2f), ImVec2(-0.1f, -0.2f)); // CPF - L2
    Link(ImVec2(-0.25f, 0.025f), ImVec2(-0.1f, 0.025f), ImGuiDir_Left); // vL1D - L2
    Link(ImVec2(-0.25f, 0.075f), ImVec2(-0.1f, 0.075f), ImGuiDir_Right);
    Link(ImVec2(-0.25f, 0.175f), ImVec2(-0.1f, 0.175f), ImGuiDir_Left); // sL1D - L2
    Link(ImVec2(-0.25f, 0.225f), ImVec2(-0.1f, 0.225f), ImGuiDir_Right);
    Link(ImVec2(-0.25f, 0.325f), ImVec2(-0.1f, 0.325f), ImGuiDir_Left); // L1I - L2
    Link(ImVec2(-0.25f, 0.375f), ImVec2(-0.1f, 0.375f), ImGuiDir_Right);
    Link(ImVec2(0, 0.025f), ImVec2(0.1f, 0.025f), ImGuiDir_Left); // L2 - Fabric
    Link(ImVec2(0, -0.025f), ImVec2(0.1f, -0.025f), ImGuiDir_Right);
    Link(ImVec2(0.2f, 0.225f), ImVec2(0.3f, 0.225f), ImGuiDir_Left); // Fabric - HBM
    Link(ImVec2(0.2f, 0.175f), ImVec2(0.3f, 0.175f), ImGuiDir_Right);
    Link(ImVec2(0.2f, 0.025f), ImVec2(0.3f, 0.025f), ImGuiDir_Left); // Fabric - PCIe
    Link(ImVec2(0.2f, -0.025f), ImVec2(0.3f, -0.025f), ImGuiDir_Right);
    Link(ImVec2(0.2f, -0.175f), ImVec2(0.3f, -0.175f), ImGuiDir_Left); // Fabric - PCIe
    Link(ImVec2(0.2f, -0.225f), ImVec2(0.3f, -0.225f), ImGuiDir_Right);
}

void ComputeBlockDiagram::Render()
{ 
    ImVec2 content_region = ImGui::GetContentRegionAvail();
    ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
    float  min_dim = std::min(content_region.x, content_region.y);
    m_content_region = ImVec2(min_dim * 0.8f, min_dim * 0.8f);
    m_content_region_center = ImVec2(cursor_pos.x + content_region.x / 2, cursor_pos.y + content_region.y / 2);
    m_draw_list = ImGui::GetWindowDrawList();

    switch(m_navigation->Current())
    {
        case kGPULevel: 
            RenderGPULevel(); 
            break;
        case kShaderEngineLevel:
            RenderShaderEngineLevel();
            break;
        case kComputeUnitLevel:
            RenderComputeUnitLevel();
            break;
        case kCacheLevel: 
            RenderCacheLevel();
            break;
    }
}

ComputeBlockView::ComputeBlockView()
: m_block_diagram(nullptr)
, m_container(nullptr)
{
    m_block_diagram = std::make_shared<ComputeBlockDiagram>();

    LayoutItem left;
    left.m_item = m_block_diagram;
    LayoutItem right;
    right.m_item = std::make_shared<RocWidget>();
    m_container = std::make_shared<HSplitContainer>(left, right);
    m_container->SetSplit(0.5f);
}

ComputeBlockView::~ComputeBlockView() {}

void ComputeBlockView::RenderMenuBar()
{
    ImVec2 cursor_position = ImGui::GetCursorScreenPos();
    ImVec2 content_region = ImGui::GetContentRegionAvail();
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(cursor_position.x, cursor_position.y),
        ImVec2(cursor_position.x + content_region.x,
               cursor_position.y + ImGui::GetFrameHeightWithSpacing()),
        ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg]), 
        0.0f);

    ImGui::BeginDisabled(!m_block_diagram->m_navigation->BackAvailable());
    if(ImGui::ArrowButton("Compute_Block_View_Back", ImGuiDir_Left))
    {
        m_block_diagram->m_navigation->GoBack();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!m_block_diagram->m_navigation->ForwardAvailable());
    if(ImGui::ArrowButton("Compute_Block_View_Forward", ImGuiDir_Right))
    {
        m_block_diagram->m_navigation->GoForward();
    }
    ImGui::EndDisabled();
}

void ComputeBlockView::Render()
{
    RenderMenuBar();
    if(m_container)
    {
        m_container->Render();
        return;
    }
}

}  // namespace View
}  // namespace RocProfVis
