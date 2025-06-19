// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_widget.h"
#include "imgui.h"
#include "rocprofvis_core.h"
#include "rocprofvis_debug_window.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_settings.h"
#include "rocprofvis_utils.h"

#include <iostream>
#include <sstream>

using namespace RocProfVis::View;

LayoutItem::LayoutItem()
: m_width(0)
, m_height(0)
, m_bg_color(0)
, m_item(nullptr)
, m_item_spacing(ImVec2(0, 0))
, m_window_padding(ImVec2(0, 0))
, m_child_window_flags(ImGuiChildFlags_Borders)
{}

LayoutItem::LayoutItem(float w, float h)
: m_width(w)
, m_height(h)
, m_bg_color(0)
, m_item(nullptr)
, m_item_spacing(ImVec2(0, 0))
, m_window_padding(ImVec2(0, 0))
, m_child_window_flags(ImGuiChildFlags_Borders)
{}

//------------------------------------------------------------------
RocWidget::~RocWidget() { spdlog::info("RocWidget object destroyed"); }

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
RocCustomWidget::RocCustomWidget(const std::function<void()>& callback)
: m_callback(callback)
{}

void
RocCustomWidget::SetCallback(const std::function<void()>& callback)
{
    m_callback = callback;
}
void
RocCustomWidget::Render()
{
    if(m_callback)
    {
        m_callback();
    }
}

//------------------------------------------------------------------
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
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, m_children[i].m_item_spacing);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, m_children[i].m_window_padding);
        ImGui::PushStyleColor(
            ImGuiCol_ChildBg,
            Settings::GetInstance().GetColor(static_cast<int>(Colors::kFillerColor)));

        ImGui::BeginChild(ImGui::GetID(i),
                          ImVec2(m_children[i].m_width, m_children[i].m_height),
                          m_children[i].m_child_window_flags);
        if(m_children[i].m_item)
        {
            m_children[i].m_item->Render();
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }
}

//------------------------------------------------------------------
HSplitContainer::HSplitContainer(const LayoutItem& l, const LayoutItem& r)
: m_left(l)
, m_right(r)
, m_resize_grip_size(4.0f)
, m_left_min_width(100.0f)
, m_right_min_width(100.0f)
, m_left_col_width(200.0f)
, m_split_ratio(0.25)  // Initial split ratio
, m_dirty(true)
{
    m_widget_name = GenUniqueName("HSplitContainer");
    m_left_name   = GenUniqueName("LeftColumn");
    m_handle_name = GenUniqueName("##ResizeHandle");
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
HSplitContainer::SetSplit(float ratio)
{
    m_split_ratio = ratio;
    m_dirty       = true;
}

void
HSplitContainer::Render()
{
    // Get available space
    ImVec2 total_size = ImGui::GetContentRegionAvail();
    ImVec2 window_pos = ImGui::GetWindowPos();

    if(m_dirty)
    {
        float split           = clamp(m_split_ratio, 0.0f, 1.0f);
        float available_width = total_size.x - m_resize_grip_size;
        m_left_col_width      = std::floor(available_width * split);

        m_left_col_width = clamp(m_left_col_width, m_left_min_width,
                                 available_width - m_right_min_width);

        m_dirty = false;
    }

    float col_height = 0;  // expand to fill height

    // Start Left Column
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, m_left.m_item_spacing);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, m_left.m_window_padding);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, Settings::GetInstance().GetColor(
                                                static_cast<int>(Colors::kFillerColor)));
    ImGui::BeginChild(m_left_name.c_str(), ImVec2(m_left_col_width, col_height),
                      m_left.m_child_window_flags);
    if(m_left.m_item)
    {
        m_left.m_item->Render();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::PopStyleVar(2);

    ImGui::SameLine();

    // Create a resize handle between columns
    ImGui::Selectable(m_handle_name.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick,
                      ImVec2(m_resize_grip_size, total_size.y));

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
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, m_right.m_item_spacing);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, m_right.m_window_padding);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, Settings::GetInstance().GetColor(
                                                static_cast<int>(Colors::kFillerColor)));
    ImGui::BeginChild(m_right_name.c_str(), ImVec2(-1, col_height),
                      m_right.m_child_window_flags);
    if(m_right.m_item)
    {
        m_right.m_item->Render();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

//------------------------------------------------------------------
VSplitContainer::VSplitContainer(const LayoutItem& t, const LayoutItem& b)
: m_top(t)
, m_bottom(b)
, m_resize_grip_size(4.0f)
, m_top_min_height(200.0f)
, m_bottom_min_height(100.0f)
, m_top_row_height(0.0f)
, m_split_ratio(0.6)  // Initial split ratio
, m_dirty(true)
{
    m_widget_name = GenUniqueName("VSplitContainer");
    m_top_name    = GenUniqueName("TopRow");
    m_handle_name = GenUniqueName("##ResizeHandle");
    m_bottom_name = GenUniqueName("BottomRow");
}

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
VSplitContainer::SetSplit(float ratio)
{
    m_split_ratio = ratio;
    m_dirty       = true;
}

void
VSplitContainer::Render()
{
    // Get available space
    ImVec2 total_size = ImGui::GetContentRegionAvail();
    ImVec2 window_pos = ImGui::GetWindowPos();

    float col_width = 0;  // expand to fill height

    if(m_dirty)
    {
        float split            = clamp(m_split_ratio, 0.0f, 1.0f);
        float available_height = total_size.y - m_resize_grip_size;
        m_top_row_height       = std::floor(available_height * split);

        m_top_row_height = clamp(m_top_row_height, m_top_min_height,
                                 available_height - m_bottom_min_height);
        m_dirty          = false;
    }

    // Start Top Row
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, m_top.m_item_spacing);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, m_top.m_window_padding);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Settings::GetInstance().GetColor(
                                                static_cast<int>(Colors::kFillerColor)));
    ImGui::BeginChild(m_top_name.c_str(), ImVec2(col_width, m_top_row_height),
                      m_top.m_child_window_flags);
    if(m_top.m_item)
    {
        m_top.m_item->Render();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    // Create a resize handle between columns
    ImGui::Selectable(m_handle_name.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick,
                      ImVec2(total_size.x, m_resize_grip_size));

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
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, m_bottom.m_item_spacing);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, m_bottom.m_window_padding);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, Settings::GetInstance().GetColor(
                                                static_cast<int>(Colors::kFillerColor)));
    ImGui::BeginChild(m_bottom_name.c_str(), ImVec2(col_width, 0),
                      m_bottom.m_child_window_flags);
    if(m_bottom.m_item)
    {
        m_bottom.m_item->Render();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

//------------------------------------------------------------------
TabContainer::TabContainer()
: m_active_tab_index(-1)
, m_set_active_tab_index(-1)
, m_allow_tool_tips(true)
{
    m_widget_name = GenUniqueName("TabContainer");
}

TabContainer::~TabContainer() { m_tabs.clear(); }

void
TabContainer::Update()
{
    // Update logic for each tab
    for(auto& tab : m_tabs)
    {
        if(tab.m_widget)
        {
            tab.m_widget->Update();
        }
    }
}

void
TabContainer::Render()
{
    ImGui::BeginChild(m_widget_name.c_str(), ImVec2(0, 0), ImGuiChildFlags_None);

    int new_selected_tab = m_active_tab_index;
    int index_to_remove  = -1;
    if(!m_tabs.empty())
    {
        if(ImGui::BeginTabBar("Tabs"))
        {
            for(size_t i = 0; i < m_tabs.size(); ++i)
            {
                const TabItem&    tab = m_tabs[i];
                ImGuiTabItemFlags flags =
                    (i == m_set_active_tab_index) ? ImGuiTabItemFlags_SetSelected : 0;

                bool  is_open = true;
                bool* p_open  = &is_open;

                // Close button
                if(!tab.m_can_close)
                {
                    p_open = nullptr;
                }
                if(ImGui::BeginTabItem(tab.m_label.c_str(), p_open, flags))
                {
                    // show tooltip for the active tab if header is hovered
                    if(m_allow_tool_tips && ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("%s", tab.m_id.c_str());
                    }

                    new_selected_tab = i;
                    if(tab.m_widget)
                    {
                        tab.m_widget->Render();
                    }
                    ImGui::EndTabItem();
                }
                // show tooltip for inactive tabs if header is hovered
                else if(ImGui::IsItemHovered())
                {
                    if(m_allow_tool_tips) {
                        ImGui::SetTooltip("%s", tab.m_id.c_str());
                    }
                }

                if(p_open && !is_open)
                {
                    index_to_remove = i;
                }
            }
            ImGui::EndTabBar();
        }

        // Check if the active tab has changed
        if(m_active_tab_index != new_selected_tab)
        {
            m_active_tab_index = new_selected_tab;
            // Todo: add event to notify of the change
        }

        // clear the set active tab index
        m_set_active_tab_index = -1;

        // Remove the tab if it was closed
        if(index_to_remove != -1)
        {
            RemoveTab(index_to_remove);
        }
    }
    ImGui::EndChild();
}

void
TabContainer::AddTab(const TabItem& tab)
{
    m_tabs.push_back(tab);
}

// Remove a tab
void
TabContainer::RemoveTab(const std::string& id)
{
    auto it = std::remove_if(m_tabs.begin(), m_tabs.end(),
                             [&id](const TabItem& tab) { return tab.m_id == id; });
    if(it != m_tabs.end())
    {
        // notify the event manager of the tab removal
        std::shared_ptr<TabClosedEvent> e = std::make_shared<TabClosedEvent>(
            static_cast<int>(RocEvents::kTabClosed), it->m_id);
        EventManager::GetInstance()->AddEvent(e);

        m_tabs.erase(it, m_tabs.end());
    }
}

void
TabContainer::RemoveTab(int index)
{
    if(index >= 0 && index < static_cast<int>(m_tabs.size()))
    {
        // notify the event manager of the tab removal
        std::shared_ptr<TabClosedEvent> e = std::make_shared<TabClosedEvent>(
            static_cast<int>(RocEvents::kTabClosed), m_tabs[index].m_id);
        EventManager::GetInstance()->AddEvent(e);

        m_tabs.erase(m_tabs.begin() + index);
    }
}

// Set the active tab by index
void
TabContainer::SetActiveTab(int index)
{
    if(index >= 0 && index < static_cast<int>(m_tabs.size()))
    {
        m_set_active_tab_index = index;
    }
}

// Set the active tab by ID
void
TabContainer::SetActiveTab(const std::string& id)
{
    auto it = std::find_if(m_tabs.begin(), m_tabs.end(),
                           [&id](const TabItem& tab) { return tab.m_id == id; });
    if(it != m_tabs.end())
    {
        m_set_active_tab_index = std::distance(m_tabs.begin(), it);
    }
}

void
TabContainer::SetAllowToolTips(bool allow_tool_tips)
{
    m_allow_tool_tips = allow_tool_tips;
}

bool
TabContainer::GetAllowToolTips() const
{
    return m_allow_tool_tips;
}

// Gets a read only list of tabs.
const std::vector<const TabItem*>
TabContainer::GetTabs()
{
    std::vector<const TabItem*> tabs;
    for (TabItem& tab : m_tabs)
    {
        const TabItem* t = &tab;
        tabs.push_back(t);
    }
    return tabs;
}
