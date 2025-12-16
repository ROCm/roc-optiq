// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_tab_container.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_event_manager.h"
#include <algorithm>

namespace RocProfVis
{
namespace View
{

TabContainer::TabContainer()
: m_active_tab_index(s_invalid_index)
, m_set_active_tab_index(s_invalid_index)
, m_allow_tool_tips(true)
, m_enable_send_close_event(false)
, m_enable_send_change_event(false)
, m_index_to_remove(s_invalid_index)
, m_pending_to_remove(s_invalid_index)
, m_confirmation_dialog(std::make_unique<ConfirmationDialog>(
      SettingsManager::GetInstance().GetUserSettings().dont_ask_before_tab_closing))
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
TabContainer::ShowCloseTabConfirm(int removing_tab_index)
{
    auto confirm = [this, removing_tab_index]() {
        m_pending_to_remove = s_invalid_index;
        RemoveTab(removing_tab_index);
    };
    auto cancel = [this]() { m_pending_to_remove = s_invalid_index; };

    m_confirmation_dialog->Show("Confirm Closing tab",
                                "Are you sure you want to close the tab: " +
                                m_tabs[removing_tab_index].m_label +
                                "? Any unsaved data will be lost.",
                                confirm, cancel);
}

void
TabContainer::SendEvent(RocEvents event, const std::string& tab_id)
{
    std::shared_ptr<TabEvent> e = std::make_shared<TabEvent>(
        static_cast<int>(event), tab_id,
        m_event_source_name.empty() ? m_widget_name : m_event_source_name);
    EventManager::GetInstance()->AddEvent(e);
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
    if(!m_tabs.empty())
    {
        if(ImGui::BeginTabBar("Tabs"))
        {
            for(size_t i = 0; i < m_tabs.size(); ++i)
            {
                const TabItem&     tab = m_tabs[i];
                ImGuiTabItemFlags flags =
                    (i == m_set_active_tab_index || i == m_pending_to_remove)
                        ? ImGuiTabItemFlags_SetSelected
                        : 0;

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
                    // Show tooltip for the active tab if header is hovered
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

                // Show tooltip for inactive tabs if header is hovered
                if(!tab_visible && ImGui::IsItemHovered())
                {
                    if(m_allow_tool_tips)
                    {
                        ImGui::SetTooltip("%s", tab.m_id.c_str());
                    }
                }

                if(p_open && !is_open)
                {
                    if(SettingsManager::GetInstance().GetUserSettings().dont_ask_before_tab_closing)
                    {
                        m_index_to_remove   = static_cast<int>(i);
                    }
                    else
                    {
                        m_pending_to_remove = static_cast<int>(i);
                    }
                }
            }
            ImGui::EndTabBar();
        }

        m_confirmation_dialog->Render();

        // Check if the active tab has changed
        if(m_active_tab_index != new_selected_tab)
        {
            m_active_tab_index = new_selected_tab;
            if(new_selected_tab < m_tabs.size() && m_enable_send_change_event)
            {
                SendEvent(RocEvents::kTabSelected, m_tabs[new_selected_tab].m_id);
            }
        }

        // Clear the set active tab index
        m_set_active_tab_index = s_invalid_index;

        // Remove the tab if it was closed
        if(m_index_to_remove != s_invalid_index)
        {
            RemoveTab(m_index_to_remove);
        }

        // Show confirm dialog if user option set
        if(m_pending_to_remove != s_invalid_index)
        {
            ShowCloseTabConfirm(m_pending_to_remove);
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
            SendEvent(RocEvents::kTabClosed, it->m_id);
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
            SendEvent(RocEvents::kTabClosed, m_tabs[index].m_id);
        }

        m_tabs.erase(m_tabs.begin() + index);
        if(m_active_tab_index == index)
        {
            // If the active tab was closed, reset to invalid index
            m_active_tab_index = s_invalid_index;
        }
        m_index_to_remove = s_invalid_index;
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

}  // namespace View
}  // namespace RocProfVis
