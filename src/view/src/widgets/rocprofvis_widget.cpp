// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_widget.h"
#include "icons/rocprovfis_icon_defines.h"
#include "imgui.h"
#include "rocprofvis_core.h"
#include "rocprofvis_debug_window.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "widgets/rocprofvis_gui_helpers.h"

#include <algorithm>
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
void
SplitContainerBase::Render()
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
        ImGui::BeginChild(m_first_name.c_str(), GetFirstChildSize(available_size),
                          m_first->m_child_flags, m_first->m_window_flags);
        if(m_first->m_item) m_first->m_item->Render();
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
        if(m_second->m_item) m_second->m_item->Render();
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
    if(m_first && m_first->m_visible)
    {
        left_col_width           = available_width * m_split_ratio;
        float max_left_col_width = (m_second && m_second->m_visible)
                                       ? (available_width - m_second_min_size)
                                       : available_width;
        if(m_first_min_size >= max_left_col_width)
        {
            left_col_width = m_first_min_size;
        }
        else
        {
            left_col_width =
                std::clamp(left_col_width, m_first_min_size, max_left_col_width);
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
                                 std::max(m_first_min_size / available_width,
                                          1.0f - m_second_min_size / available_width));
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
    if(m_second && m_second->m_visible)
    {
        float available_size = available_width;
        top_row_height       = available_size * m_split_ratio;
        top_row_height =
            std::clamp(top_row_height, m_first_min_size,
                       std::max(m_first_min_size, available_size - m_second_min_size));
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
                                 std::max(m_first_min_size / available_height,
                                          1.0f - m_second_min_size / available_height));
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

//------------------------------------------------------------------
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

    m_confirmation_dialog->Show(
        "Confirm Closing Tab",
        "Are you sure you want to close the tab: " + m_tabs[removing_tab_index].m_label +
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
                const TabItem&    tab = m_tabs[i];
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
                    if(SettingsManager::GetInstance()
                           .GetUserSettings()
                           .dont_ask_before_tab_closing)
                    {
                        m_index_to_remove = static_cast<int>(i);
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

LayoutItem::Ptr
LayoutItem::CreateFromWidget(std::shared_ptr<RocWidget> widget, float w, float h)
{
    Ptr item     = std::make_shared<LayoutItem>(w, h);
    item->m_item = widget;
    return item;
}

//------------------------------------------------------------------
EditableTextField::EditableTextField(std::string id)
: m_id(std::move(id))
{}

void
EditableTextField::Render()
{
    ImGui::PushID(m_id.c_str());
    if(m_editing_mode)
    {
        DrawEditingText();
    }
    else
    {
        DrawPlainText();
    }
    ImGui::PopID();
}

void
EditableTextField::RevertToDefault()
{
    // FIXME: Assuming default is an empty string
    // Think about best way to handle revert button pressed
    m_text = "";
    if(m_on_text_commit)
    {
        m_on_text_commit(m_text);
    }
}

void
EditableTextField::ShowResetButton(bool is_button_shown)
{
    m_show_reset_button = is_button_shown;
}

void
EditableTextField::DrawPlainText()
{
    SettingsManager& settings = SettingsManager::GetInstance();
    if(m_show_reset_button)
    {
        if(IconButton(ICON_ARROWS_CYCLE,
                      settings.GetFontManager().GetIconFont(FontType::kDefault)))
        {
            RevertToDefault();
        }
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                            settings.GetDefaultStyle().WindowPadding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,
                            settings.GetDefaultStyle().FrameRounding);
        if(ImGui::BeginItemTooltip())
        {
            ImGui::TextUnformatted("Revert To Default");
            ImGui::TextUnformatted(m_reset_tooltip.c_str());
            ImGui::EndTooltip();
        }
        ImGui::PopStyleVar(2);
        ImGui::SameLine();
    }
    // Draw the text as a button to avoid the background
    // and prevent clicks from being registered.
    ImGui::PushStyleColor(ImGuiCol_Button, 0);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 0);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

    ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x -
                         ImGui::CalcTextSize(m_text.c_str()).x -
                         ImGui::GetStyle().WindowPadding.x);
    if(ImGui::Button(m_text.c_str()))
    {
        m_editing_mode           = true;
        m_request_keyboard_focus = true;
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        settings.GetDefaultStyle().WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,
                        settings.GetDefaultStyle().FrameRounding);
    if(ImGui::BeginItemTooltip())
    {
        ImGui::TextUnformatted(m_tooltip_text.c_str());
        ImGui::EndTooltip();
    }
    ImGui::PopStyleVar(2);
    if(ImGui::IsItemHovered())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
}

void
EditableTextField::DrawEditingText()
{
    if(m_edit_buf != m_text)
    {
        m_edit_buf = m_text;
    }

    auto resize_callback = [](ImGuiInputTextCallbackData* data) -> int {
        if(data->EventFlag == ImGuiInputTextFlags_CallbackResize)
        {
            std::string* str = static_cast<std::string*>(data->UserData);
            str->resize(static_cast<size_t>(data->BufTextLen));
            data->Buf = str->data();
        }
        return 0;
    };

    float width = ImGui::CalcTextSize(m_edit_buf.c_str()).x;
    ImGui::SetNextItemWidth(width);

    // make edit field on the same level as the textfield
    ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionMax().x -
                                   ImGui::CalcTextSize(m_text.c_str()).x -
                                   ImGui::GetStyle().WindowPadding.x,
                               ImGui::GetCursorPosY() - 2.0f));

    if(m_request_keyboard_focus)
    {
        ImGui::SetKeyboardFocusHere();
        m_request_keyboard_focus = false;
    }

    bool enter_pressed = ImGui::InputText(
        ("##" + m_id).c_str(), m_edit_buf.data(), m_edit_buf.capacity() + 1,
        ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue |
            ImGuiInputTextFlags_CallbackResize,
        resize_callback, static_cast<void*>(&m_edit_buf));

    if(enter_pressed)
    {
        AcceptEdit();
    }
    if(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsItemHovered())
    {
        AcceptEdit();
    }
    if(ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        m_editing_mode = false;
    }
}

void
EditableTextField::SetText(std::string text, std::string tooltip,
                           std::string reset_tooltip)
{
    m_text          = std::move(text);
    m_tooltip_text  = std::move(tooltip);
    m_reset_tooltip = std::move(reset_tooltip);
}

float
EditableTextField::ButtonSize() const
{
    ImFont* icon_font =
        SettingsManager::GetInstance().GetFontManager().GetIconFont(FontType::kDefault);
    ImGui::PushFont(icon_font);
    float size = ImGui::CalcTextSize(ICON_ARROWS_CYCLE).x;
    ImGui::PopFont();
    return size;
}

void
EditableTextField::AcceptEdit()
{
    m_editing_mode = false;
    m_text         = m_edit_buf;
    if(m_on_text_commit)
    {
        m_on_text_commit(m_text);
    }
}
void
EditableTextField::SetOnTextCommit(const std::function<void(const std::string&)>& cb)
{
    m_on_text_commit = cb;
}

//------------------------------------------------------------------
PopUpStyle::PopUpStyle()
: m_style_var_count(0)
, m_color_count(0)
{}

PopUpStyle::~PopUpStyle() { PopStyles(); }

void
PopUpStyle::PushPopupStyles()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 16));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 12));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);

    m_style_var_count = 6;
}

void
PopUpStyle::CenterPopup()
{
    ImVec2 center_pos = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center_pos, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
}

void
PopUpStyle::PushTitlebarColors()
{
    SettingsManager& settings   = SettingsManager::GetInstance();
    ImU32            grey_color = settings.GetColor(Colors::kBorderGray);

    ImGui::PushStyleColor(ImGuiCol_TitleBg, grey_color);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, grey_color);
    ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, grey_color);

    m_color_count += 3;
}

void
PopUpStyle::PopStyles()
{
    if(m_style_var_count > 0)
    {
        ImGui::PopStyleVar(m_style_var_count);
        m_style_var_count = 0;
    }
    if(m_color_count > 0)
    {
        ImGui::PopStyleColor(m_color_count);
        m_color_count = 0;
    }
}

}  // namespace View
}  // namespace RocProfVis
