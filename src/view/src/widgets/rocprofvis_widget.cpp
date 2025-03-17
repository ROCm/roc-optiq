#include "rocprofvis_widget.h"
#include "../rocprofvis_utils.h"
#include "imgui.h"
#include <iostream>
#include <sstream>

using namespace RocProfVis::View;

LayoutItem::LayoutItem()
: m_width(0)
, m_height(0)
, m_bg_color(0)
, m_name("")
, m_item(nullptr)
{}

LayoutItem::LayoutItem(float w, float h)
: m_width(w)
, m_height(h)
, m_bg_color(0)
, m_name("")
, m_item(nullptr)
{}

//------------------------------------------------------------------
RocWidget::~RocWidget() { std::cout << "RocWidget object destroyed" << std::endl; }

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

//------------------------------------------------------------------
VFixedContainer::VFixedContainer() {}

VFixedContainer::VFixedContainer(std::vector<LayoutItem>& items)
: m_children(items)
{}

void
VFixedContainer::Render()
{
    size_t len = m_children.size();
    for(size_t i = 0; i < len; ++i)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, m_children[i].m_bg_color);
        ImGui::BeginChild(ImGui::GetID(i),
                          ImVec2(m_children[i].m_width, m_children[i].m_height),
                          ImGuiChildFlags_Borders);
        if(m_children[i].m_item)
        {
            m_children[i].m_item->Render();
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
}

//------------------------------------------------------------------
HSplitContainer::HSplitContainer(const LayoutItem& l, const LayoutItem& r)
: m_left(l)
, m_right(r)
, m_resize_grip_size(4.0f)
, m_left_min_width(100.0f)
, m_right_min_width(100.0f)
, m_left_col_width(200.0f)  // Initial width
{
    m_left_name   = GenUniqueName("LeftColumn");
    m_handle_name = GenUniqueName("ResizeHandle");
    m_right_name  = GenUniqueName("RightColumn");
}

void
HSplitContainer::SetLeft(const LayoutItem& l)
{
    m_left = l;
}

void
HSplitContainer::setRight(const LayoutItem& r)
{
    m_right = r;
}

void
HSplitContainer::SetMinLeftWidth(float width)
{
    m_left_min_width = width;
}

void
HSplitContainer::SetMinRightWidth(float width)
{
    m_right_min_width = width;
}

void
HSplitContainer::Render()
{
    // Get available space
    ImVec2 total_size = ImGui::GetContentRegionAvail();
    ImVec2 window_pos = ImGui::GetWindowPos();

    float col_height = -1;  // expand to fill height

    // Start Left Column
    ImGui::PushStyleColor(ImGuiCol_ChildBg, m_left.m_bg_color);
    ImGui::BeginChild(m_left_name.c_str(), ImVec2(m_left_col_width, col_height), true);
    if(m_left.m_item)
    {
        m_left.m_item->Render();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine();

    // Create a resize handle between columns
    ImGui::InvisibleButton(m_handle_name.c_str(), ImVec2(m_resize_grip_size, col_height));

    // Change cursor appearance when hovering over the resize handle
    if(ImGui::IsItemHovered())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }

    // Enable resizing by dragging (hold the mouse down)
    if(ImGui::IsItemActive())
    {
        ImVec2 mouse_pos = ImGui::GetMousePos();
        // convert to local space
        ImVec2 ml = ImVec2(mouse_pos.x - window_pos.x, mouse_pos.y - window_pos.y);
        // Minimum and maximum width for the left column
        m_left_col_width = clamp(ml.x, m_left_min_width,
                                 total_size.x - m_resize_grip_size - m_right_min_width);
    }

    ImGui::SameLine();

    // Start Right Column (Fills Remaining Space)
    ImGui::PushStyleColor(ImGuiCol_ChildBg, m_right.m_bg_color);
    ImGui::BeginChild(m_right_name.c_str(), ImVec2(-1, col_height), true);
    if(m_right.m_item)
    {
        m_right.m_item->Render();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

//------------------------------------------------------------------
VSplitContainer::VSplitContainer(const LayoutItem& t, const LayoutItem& b)
: m_top(t)
, m_bottom(b)
, m_resize_grip_size(4.0f)
, m_top_min_height(200.0f)
, m_bottom_min_height(100.0f)
, m_top_row_height(600.0f)  // Initial height
{}

void
VSplitContainer::SetTop(const LayoutItem& t)
{
    m_top = t;
}

void
VSplitContainer::SetBottom(const LayoutItem& b)
{
    m_bottom = b;
}

void
VSplitContainer::SetMinTopHeight(float height)
{
    m_top_min_height = height;
}

void
VSplitContainer::SetMinBottomHeight(float height)
{
    m_bottom_min_height = height;
}

void
VSplitContainer::Render()
{
    // Get available space
    ImVec2 total_size = ImGui::GetContentRegionAvail();
    ImVec2 window_pos = ImGui::GetWindowPos();

    float col_width = -1;  // expand to fill height

    // Start Top Row
    ImGui::PushStyleColor(ImGuiCol_ChildBg, m_top.m_bg_color);
    ImGui::BeginChild("TopRow", ImVec2(col_width, m_top_row_height), true);
    if(m_top.m_item)
    {
        m_top.m_item->Render();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Create a resize handle between columns
    if(ImGui::InvisibleButton("ResizeHandle", ImVec2(col_width, m_resize_grip_size)))
    {
    }

    // Change cursor appearance when hovering over the resize handle
    if(ImGui::IsItemHovered())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }

    // Enable resizing by dragging (hold the mouse down)
    if(ImGui::IsItemActive())
    {
        ImVec2 mouse_pos = ImGui::GetMousePos();
        // convert to local space
        ImVec2 ml = ImVec2(mouse_pos.x - window_pos.x, mouse_pos.y - window_pos.y);

        // Minimum and maximum height for the top row
        m_top_row_height = clamp(ml.y, m_top_min_height,
                                 total_size.y - m_resize_grip_size - m_bottom_min_height);
    }

    // Start Bottom Row (Fills Remaining Space)
    ImGui::PushStyleColor(ImGuiCol_ChildBg, m_bottom.m_bg_color);
    ImGui::BeginChild("BottomRow", ImVec2(col_width, -1), true);
    if(m_bottom.m_item)
    {
        m_bottom.m_item->Render();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}
