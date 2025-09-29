// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_widget.h"
#include "imgui.h"
#include "rocprofvis_core.h"
#include "rocprofvis_debug_window.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"

#include <iostream>
#include <sstream>

namespace RocProfVis
{
namespace View
{

void
WithPadding(float left, float right, float top, float bottom,
            const std::function<void()>& content)
{
    if(top > 0.0f) ImGui::Dummy(ImVec2(0, top));

    // No border flags for invisible borders
    if(ImGui::BeginTable("##padding_table", 3, ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("LeftPad", ImGuiTableColumnFlags_WidthFixed, left);
        ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("RightPad", ImGuiTableColumnFlags_WidthFixed, right);

        ImGui::TableNextRow();

        // Left padding
        ImGui::TableSetColumnIndex(0);
        if(left > 0.0f) ImGui::Dummy(ImVec2(left, 0));

        // Content
        ImGui::TableSetColumnIndex(1);
        ImGui::BeginGroup();
        content();
        ImGui::EndGroup();

        // Right padding
        ImGui::TableSetColumnIndex(2);
        if(right > 0.0f) ImGui::Dummy(ImVec2(right, 0));

        ImGui::EndTable();
    }

    if(bottom > 0.0f) ImGui::Dummy(ImVec2(0, bottom));
}

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

const std::string&
RocWidget::GetWidgetName() const
{
    return m_widget_name;
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
HSplitContainer::HSplitContainer(LayoutItem::Ptr left, LayoutItem::Ptr right)
: m_left(left)
, m_right(right)
, m_resize_grip_size(4.0f)
, m_left_min_width(100.0f)
, m_right_min_width(100.0f)
, m_split_ratio(0.25)  // Initial split ratio
, m_optimal_height(0.0f)
{
    m_widget_name = GenUniqueName("HSplitContainer");
    m_left_name   = GenUniqueName("LeftColumn");
    m_handle_name = GenUniqueName("##ResizeHandle");
    m_right_name  = GenUniqueName("RightColumn");
}

void
HSplitContainer::SetLeft(LayoutItem::Ptr left)
{
    m_left = left;
}

void
HSplitContainer::setRight(LayoutItem::Ptr right)
{
    m_right = right;
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
}

float 
HSplitContainer::GetOptimalHeight() const
{
    return m_optimal_height;
}

void
HSplitContainer::Render()
{
    // Get available space
    ImVec2 total_size = ImGui::GetContentRegionAvail();
    ImVec2 window_pos = ImGui::GetWindowPos();

    float available_width = total_size.x - m_resize_grip_size;
    float left_col_width  = 0.0f;

    if(m_right->m_visible)
    {
        left_col_width = available_width * m_split_ratio;
        left_col_width       = clamp(left_col_width, m_left_min_width,
                                     available_width - m_right_min_width);  // std::clamp?
    }

    float col_height = 0;  // expand to fill height
    // Start Left Column
    if(m_left->m_visible)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, m_left->m_item_spacing);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, m_left->m_window_padding);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, m_left->m_bg_color);
        ImGui::BeginChild(m_left_name.c_str(), ImVec2(left_col_width, col_height),
                          m_left->m_child_flags, m_left->m_window_flags);
        if(m_left->m_item)
        {
            m_left->m_item->Render();
        }
        ImGui::EndChild();
        m_optimal_height = ImGui::GetItemRectSize().y;

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);

        ImGui::SameLine();
    }



    if(m_left->m_visible && m_right->m_visible)
    {
        // Create a resize handle between columns
        ImGui::Selectable(m_handle_name.c_str(), false,
                          ImGuiSelectableFlags_AllowDoubleClick,
                          ImVec2(m_resize_grip_size, total_size.y));

        // Draw a custom background for the splitter area
        ImVec2 splitter_min = ImGui::GetItemRectMin();
        ImVec2 splitter_max = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddRectFilled(
            splitter_min, splitter_max,
            SettingsManager::GetInstance().GetColor(Colors::kSplitterColor));

        // Change cursor appearance when hovering over the resize handle
        if(ImGui::IsItemHovered())
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }

        // Enable resizing by dragging (hold the mouse down)
        if(ImGui::IsItemActive())
        {
            ImVec2 mouse_pos = ImGui::GetMousePos();
            float  mouse_x   = mouse_pos.x - window_pos.x;
            float  new_ratio = (mouse_x - (m_resize_grip_size / 2)) / available_width;
            new_ratio        = clamp(new_ratio, m_left_min_width / available_width,
                                     1.0f - m_right_min_width / available_width);
            m_split_ratio    = new_ratio;
        }

        ImGui::SameLine();
    }
    
    if(m_right->m_visible)
    {
        // Start Right Column (Fills Remaining Space)
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, m_right->m_item_spacing);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, m_right->m_window_padding);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, m_right->m_bg_color);
        ImGui::BeginChild(m_right_name.c_str(), ImVec2(-1, col_height),
                          m_right->m_child_flags, m_right->m_window_flags);
        if(m_right->m_item)
        {
            m_right->m_item->Render();
        }
        ImGui::EndChild();
        m_optimal_height = std::max(m_optimal_height, ImGui::GetItemRectSize().y);

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }
    
}

//------------------------------------------------------------------
VSplitContainer::VSplitContainer(LayoutItem::Ptr top, LayoutItem::Ptr bottom)
: m_top(top)
, m_bottom(bottom)
, m_resize_grip_size(4.0f)
, m_top_min_height(200.0f)
, m_bottom_min_height(100.0f)
, m_split_ratio(0.6f)  // Initial split ratio
{
    m_widget_name = GenUniqueName("VSplitContainer");
    m_top_name    = GenUniqueName("TopRow");
    m_handle_name = GenUniqueName("##ResizeHandle");
    m_bottom_name = GenUniqueName("BottomRow");
}

void
VSplitContainer::SetTop(LayoutItem::Ptr top)
{
    m_top = top;
}

void
VSplitContainer::SetBottom(LayoutItem::Ptr bottom)
{
    m_bottom = bottom;
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
}

void
VSplitContainer::Render()
{
    // Get available space
    ImVec2 total_size = ImGui::GetContentRegionAvail();
    ImVec2 window_pos = ImGui::GetWindowPos();

    float col_width = 0.0f;  // expand to fill height
    float available_height = 0.0f;
    float top_row_height   = 0.0f;
    if (m_bottom->m_visible)
    {
        float available_height = total_size.y - m_resize_grip_size;
        top_row_height = available_height * m_split_ratio;
        top_row_height = clamp(
            top_row_height, m_top_min_height,
            available_height - m_bottom_min_height);  //? Why we dont use here std::clamp? 
    }
    

    // Start Top Row
    if(m_top->m_visible)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, m_top->m_item_spacing);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, m_top->m_window_padding);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, SettingsManager::GetInstance().GetColor(
                                                    Colors::kFillerColor));
        ImGui::BeginChild(m_top_name.c_str(), ImVec2(col_width, top_row_height),
                          m_top->m_child_flags, m_top->m_window_flags);
        if(m_top->m_item)
        {
            m_top->m_item->Render();
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }
    
    if(m_bottom->m_visible && m_top->m_visible)  // TODO: to much ifs
    {
        // Create a resize handle between columns
        ImGui::Selectable(m_handle_name.c_str(), false,
                          ImGuiSelectableFlags_AllowDoubleClick,
                          ImVec2(total_size.x, m_resize_grip_size));

        // Draw a custom background for the splitter area
        ImVec2 splitter_min = ImGui::GetItemRectMin();
        ImVec2 splitter_max = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddRectFilled(
            splitter_min, splitter_max,
            SettingsManager::GetInstance().GetColor(Colors::kSplitterColor));

        // Change cursor appearance when hovering over the resize handle
        if(ImGui::IsItemHovered())
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        }

        // Enable resizing by dragging (hold the mouse down)
        if(ImGui::IsItemActive())
        {
            ImVec2 mouse_pos = ImGui::GetMousePos();
            float  mouse_y   = mouse_pos.y - window_pos.y;
            float  new_ratio = (mouse_y - (m_resize_grip_size / 2)) / available_height;
            new_ratio        = clamp(new_ratio, m_top_min_height / available_height,
                                     1.0f - m_bottom_min_height / available_height);
            m_split_ratio    = new_ratio;
        }
    }    

    // Start Bottom Row (Fills Remaining Space)
    if(m_bottom->m_visible)  // think to redesign to avoid alot of ifs
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, m_bottom->m_item_spacing);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, m_bottom->m_window_padding);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, SettingsManager::GetInstance().GetColor(
                                                    Colors::kFillerColor));
        ImGui::BeginChild(m_bottom_name.c_str(), ImVec2(col_width, 0),
                          m_bottom->m_child_flags, m_bottom->m_window_flags);
        if(m_bottom->m_item)
        {
            m_bottom->m_item->Render();
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }    
}

//------------------------------------------------------------------
TabContainer::TabContainer()
: m_active_tab_index(-1)
, m_set_active_tab_index(-1)
, m_allow_tool_tips(true)
, m_enable_send_close_event(false)
, m_enable_send_change_event(false)
{
    m_widget_name = GenUniqueName("TabContainer");
}

TabContainer::~TabContainer() { m_tabs.clear(); }

void
TabContainer::SetEventSourceName(const std::string& source_name)
{
    m_event_source_name = source_name;
}

const std::string&
TabContainer::GetEventSourceName() const
{
    return m_event_source_name;
}

void
TabContainer::EnableSendCloseEvent(bool enable)
{
    m_enable_send_close_event = enable;
}

void
TabContainer::EnableSendChangeEvent(bool enable)
{
    m_enable_send_change_event = enable;
}

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
                bool tab_visible = false;
                ImGui::PushID(tab.m_id.c_str());
                if(ImGui::BeginTabItem(tab.m_label.c_str(), p_open, flags))
                {
                    tab_visible = true;
                    // show tooltip for the active tab if header is hovered
                    if(m_allow_tool_tips && ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("%s", tab.m_id.c_str());
                    }

                    new_selected_tab = static_cast<int>(i);
                    if(tab.m_widget)
                    {
                        tab.m_widget->Render();
                    }
                    ImGui::EndTabItem();
                }
                ImGui::PopID();
                // show tooltip for inactive tabs if header is hovered
                if(!tab_visible && ImGui::IsItemHovered())
                {
                    if(m_allow_tool_tips)
                    {
                        ImGui::SetTooltip("%s", tab.m_id.c_str());
                    }
                }

                if(p_open && !is_open)
                {
                    index_to_remove = static_cast<int>(i);
                }
            }
            ImGui::EndTabBar();
        }

        // Check if the active tab has changed
        if(m_active_tab_index != new_selected_tab)
        {
            m_active_tab_index = new_selected_tab;
            if(new_selected_tab < m_tabs.size() && m_enable_send_change_event)
            {
                std::shared_ptr<TabEvent> e = std::make_shared<TabEvent>(
                    static_cast<int>(RocEvents::kTabSelected),
                    m_tabs[new_selected_tab].m_id,
                    m_event_source_name.empty() ? m_widget_name : m_event_source_name);
                EventManager::GetInstance()->AddEvent(e);
            }
        }

        // clear the set active tab index
        m_set_active_tab_index = -1;

        // Remove the tab if it was closed
        if(index_to_remove != -1)
        {
            RemoveTab(index_to_remove);
            if(m_active_tab_index == index_to_remove)
            {
                // If the active tab was closed, reset to -1
                m_active_tab_index = -1;
            }
        }
    }
    ImGui::EndChild();
}

void
TabContainer::AddTab(const TabItem& tab)
{
    m_tabs.push_back(tab);
}

void
TabContainer::AddTab(TabItem&& tab)
{
    m_tabs.push_back(std::move(tab));
}

// Remove a tab
void
TabContainer::RemoveTab(const std::string& id)
{
    auto it = std::remove_if(m_tabs.begin(), m_tabs.end(),
                             [&id](const TabItem& tab) { return tab.m_id == id; });
    if(it != m_tabs.end())
    {
        if(m_enable_send_close_event)
        {
            // notify the event manager of the tab removal
            std::shared_ptr<TabEvent> e = std::make_shared<TabEvent>(
                static_cast<int>(RocEvents::kTabClosed), it->m_id,
                m_event_source_name.empty() ? m_widget_name : m_event_source_name);
            EventManager::GetInstance()->AddEvent(e);
        }

        m_tabs.erase(it, m_tabs.end());
    }
}

void
TabContainer::RemoveTab(int index)
{
    if(index >= 0 && index < static_cast<int>(m_tabs.size()))
    {
        if(m_enable_send_close_event)
        {
            // notify the event manager of the tab removal
            std::shared_ptr<TabEvent> e = std::make_shared<TabEvent>(
                static_cast<int>(RocEvents::kTabClosed), m_tabs[index].m_id,
                m_event_source_name.empty() ? m_widget_name : m_event_source_name);
            EventManager::GetInstance()->AddEvent(e);
        }

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
        m_set_active_tab_index = static_cast<int>(std::distance(m_tabs.begin(), it));
    }
}

void
TabContainer::SetTabLabel(const std::string& label, const std::string& id)
{
    auto it = std::find_if(m_tabs.begin(), m_tabs.end(),
                           [&id](const TabItem& tab) { return tab.m_id == id; });
    if(it != m_tabs.end())
    {
        it->m_label = label;
    }
}

const TabItem*
TabContainer::GetActiveTab() const
{
    if(m_active_tab_index >= 0 && m_active_tab_index < static_cast<int>(m_tabs.size()))
    {
        return &m_tabs[m_active_tab_index];
    }
    return nullptr;
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
    for(TabItem& tab : m_tabs)
    {
        const TabItem* t = &tab;
        tabs.push_back(t);
    }
    return tabs;
}

}
}
