// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_split_containers.h"
#include "rocprofvis_settings_manager.h"
namespace RocProfVis
{
namespace View
{

VFixedContainer::VFixedContainer() { m_widget_name = GenUniqueName("VFixedContainer"); }

VFixedContainer::VFixedContainer(std::vector<LayoutItem>& items)
: m_children(items)
{
    m_widget_name = GenUniqueName("VFixedContainer");
}

bool
VFixedContainer::SetAt(int index, const LayoutItem& item)
{
    if(index < m_children.size() && index > 0)
    {
        m_children[index] = item;
        return true;
    }
    return false;
}

const LayoutItem*
VFixedContainer::GetAt(int index) const
{
    if(index < m_children.size() && index >= 0)
    {
        return &m_children[index];
    }
    return nullptr;
}

LayoutItem*
VFixedContainer::GetMutableAt(int index)
{
    if(index < m_children.size() && index >= 0)
    {
        return &m_children[index];
    }
    return nullptr;
}

size_t
VFixedContainer::ItemCount()
{
    return m_children.size();
}

void
VFixedContainer::Render()
{
    size_t len = m_children.size();
    for(size_t i = 0; i < len; ++i)
    {
        if(!m_children[i].m_visible || !m_children[i].m_item)
        {
            continue;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, m_children[i].m_item_spacing);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, m_children[i].m_window_padding);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, SettingsManager::GetInstance().GetColor(
                                                    Colors::kFillerColor));

        ImGui::BeginChild(ImGui::GetID(static_cast<int>(i)),
                          ImVec2(m_children[i].m_width, m_children[i].m_height),
                          m_children[i].m_child_flags, m_children[i].m_window_flags);
        m_children[i].m_item->Render();
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }
}

//------------------------------------------------------------------
void SplitContainerBase::Render()
{
    ImVec2 total_size = ImGui::GetContentRegionAvail();
    ImVec2 window_pos = ImGui::GetWindowPos();

    float available_size = GetAvailableSize(total_size);

    // Render first child
    if(m_first && m_first->m_visible)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, m_first->m_item_spacing);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, m_first->m_window_padding);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, m_first->m_bg_color);
        ImGui::BeginChild(m_first_name.c_str(),
                          GetFirstChildSize(available_size),
                          m_first->m_child_flags, m_first->m_window_flags);
        if(m_first->m_item)
            m_first->m_item->Render();
        ImGui::EndChild();
        m_optimal_size = GetItemSize();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        AddSameLine();
    }

    // Render splitter
    if(m_first && m_first->m_visible && m_second && m_second->m_visible)
    {
        bool fill_active = false;
        ImGui::Selectable(m_handle_name.c_str(), false,
                          ImGuiSelectableFlags_AllowDoubleClick,
                          GetSplitterSize(total_size));
        ImVec2 splitter_min = ImGui::GetItemRectMin();
        ImVec2 splitter_max = ImGui::GetItemRectMax();
        if(ImGui::IsItemHovered())
        {
            SetCursor();
            fill_active = true;
        }
        if(ImGui::IsItemActive())
        {
            ImVec2 mouse_pos = ImGui::GetMousePos();
            UpdateSplitRatio(mouse_pos, window_pos, available_size);
            fill_active = true;
        }
        ImGui::GetWindowDrawList()->AddRectFilled(
            splitter_min, splitter_max,
            SettingsManager::GetInstance().GetColor(
                fill_active ? Colors::kAccentRedActive : Colors::kSplitterColor));
        AddSameLine();
    }

    // Render second child
    if(m_second && m_second->m_visible)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, m_second->m_item_spacing);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, m_second->m_window_padding);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, m_second->m_bg_color);
        ImGui::BeginChild(m_second_name.c_str(), GetSecondChildSize(),
                          m_second->m_child_flags, m_second->m_window_flags);
        if(m_second->m_item)
            m_second->m_item->Render();
        ImGui::EndChild();
        m_optimal_size = std::max(m_optimal_size, GetItemSize());
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }
}

float
SplitContainerBase::GetMinSize()
{
    return m_first_min_size + m_resize_grip_size + m_second_min_size;
}

void
SplitContainerBase::SetFirst(LayoutItem::Ptr first)
{
    m_first = first;
};

void
SplitContainerBase::SetSecond(LayoutItem::Ptr second)
{
    m_second = second;
};

void
SplitContainerBase::SetMinFirstSize(float size)
{
    m_first_min_size = size;
};

void
SplitContainerBase::SetMinSecondSize(float size)
{
    m_second_min_size = size;
};

//------------------------------------------------------------------
HSplitContainer::HSplitContainer(LayoutItem::Ptr left, LayoutItem::Ptr right)
: SplitContainerBase(left, right, 4.0f, 100.0f, 100.0f, 0.25f)
{
    m_widget_name = GenUniqueName("HSplitContainer");
    m_first_name  = GenUniqueName("LeftColumn");
    m_handle_name = GenUniqueName("##ResizeHandle");
    m_second_name = GenUniqueName("RightColumn");
};

void
HSplitContainer::SetLeft(LayoutItem::Ptr left)
{
    SetFirst(left);
};

void
HSplitContainer::SetRight(LayoutItem::Ptr right)
{
    SetSecond(right);
};

void
HSplitContainer::SetMinLeftWidth(float width)
{
    SetMinFirstSize(width);
};

void
HSplitContainer::SetMinRightWidth(float width)
{
    SetMinSecondSize(width);
};

float
HSplitContainer::GetOptimalHeight() const
{
    return m_optimal_size;
};

float
HSplitContainer::GetAvailableSize(const ImVec2& total_size)
{
    return total_size.x - m_resize_grip_size;
};

void
HSplitContainer::SetCursor()
{
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
};

ImVec2
HSplitContainer::GetFirstChildSize(float available_width)
{
    float left_col_width = 0.0f;
    if (m_first && m_first->m_visible)
    {
        left_col_width = available_width * m_split_ratio;
        float max_left_col_width = (m_second && m_second->m_visible)
            ? (available_width - m_second_min_size)
            : available_width;
        if(m_first_min_size >= max_left_col_width)
        {
            left_col_width = m_first_min_size;
        }
        else
        {
            left_col_width = std::clamp(left_col_width, m_first_min_size,
                                        max_left_col_width);
        }
    }
    return ImVec2(left_col_width, 0);
}

ImVec2
HSplitContainer::GetSecondChildSize()
{
    return ImVec2(-1, 0);
}

void
HSplitContainer::UpdateSplitRatio(const ImVec2& mouse_pos, const ImVec2& window_pos,
                 float available_width)
{
    float mouse_x   = mouse_pos.x - window_pos.x;
    float new_ratio = (mouse_x - (m_resize_grip_size / 2)) / available_width;
    new_ratio       = std::clamp(new_ratio, m_first_min_size / available_width,
                                 1.0f - m_second_min_size / available_width);
    m_split_ratio   = new_ratio;
}

ImVec2
HSplitContainer::GetSplitterSize(const ImVec2& total_size)
{
    return ImVec2(m_resize_grip_size, total_size.y);
}

void
HSplitContainer::AddSameLine()
{
    ImGui::SameLine();
};

float
HSplitContainer::GetItemSize()
{
    return ImGui::GetItemRectSize().y;
};

//------------------------------------------------------------------
VSplitContainer::VSplitContainer(LayoutItem::Ptr top, LayoutItem::Ptr bottom)
: SplitContainerBase(top, bottom, 4.0f, 200.0f, 100.0f, 0.6f)
{
    m_widget_name = GenUniqueName("VSplitContainer");
    m_first_name  = GenUniqueName("TopRow");
    m_handle_name = GenUniqueName("##ResizeHandle");
    m_second_name = GenUniqueName("BottomRow");
};

void
VSplitContainer::SetTop(LayoutItem::Ptr top)
{
    SetFirst(top);
};

void
VSplitContainer::SetBottom(LayoutItem::Ptr bottom)
{
    SetSecond(bottom);
};

void
VSplitContainer::SetMinTopHeight(float height)
{
    SetMinFirstSize(height);
};

void
VSplitContainer::SetMinBottomHeight(float height)
{
    SetMinSecondSize(height);
};

float
VSplitContainer::GetOptimalWidth() const
{
    return m_optimal_size;
};

float
VSplitContainer::GetAvailableSize(const ImVec2& total_size)
{
    return total_size.y - m_resize_grip_size;
};

void
VSplitContainer::SetCursor()
{
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
};

ImVec2
VSplitContainer::GetFirstChildSize(float available_width)
{
    float top_row_height = 0.0f;
    if (m_second && m_second->m_visible)
    {
        float available_size = available_width;
        top_row_height         = available_size * m_split_ratio;
        top_row_height         = std::clamp(top_row_height, m_first_min_size,
                                       available_size - m_second_min_size);
    }
    return ImVec2(0, top_row_height);
}

ImVec2
VSplitContainer::GetSecondChildSize()
{
    return ImVec2(0, 0);
}

void
VSplitContainer::UpdateSplitRatio(const ImVec2& mouse_pos, const ImVec2& window_pos,
                 float available_height)
{
    float mouse_y   = mouse_pos.y - window_pos.y;
    float new_ratio = (mouse_y - (m_resize_grip_size / 2)) / available_height;
    new_ratio       = std::clamp(new_ratio, m_first_min_size / available_height,
                                 1.0f - m_second_min_size / available_height);
    m_split_ratio   = new_ratio;
}

ImVec2
VSplitContainer::GetSplitterSize(const ImVec2& total_size)
{
    return ImVec2(total_size.x, m_resize_grip_size);
}

float
VSplitContainer::GetItemSize()
{
    return ImGui::GetItemRectSize().x;
};

}  // namespace View
}  // namespace RocProfVis
