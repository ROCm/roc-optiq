// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "imgui.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

class PaddingWrapper
{
    // Lightweight class to provide padding around ImGui content
public:
    static void WithPadding(float left, float right, float top, float bottom,
                             const std::function<void()>& content);
};

class RocWidget
{
public:
    virtual ~RocWidget();
    virtual void Render();
    virtual void Update() {}
    std::string  GenUniqueName(std::string name);

    const std::string& GetWidgetName() const;

protected:
    std::string m_widget_name;
};

class LayoutItem
{
public:
    LayoutItem();
    LayoutItem(float w, float h);

    std::shared_ptr<RocWidget> m_item;  // Widget that this item will render
    float                      m_height;
    float                      m_width;
    bool                       m_visible;

    int32_t m_bg_color;

    ImVec2 m_item_spacing;
    ImVec2 m_window_padding;

    ImGuiChildFlags  m_child_flags;
    ImGuiWindowFlags m_window_flags;
};

class RocCustomWidget : public RocWidget
{
public:
    RocCustomWidget(const std::function<void()>& callback);

    virtual void Render();
    void         SetCallback(const std::function<void()>& callback);

private:
    std::function<void()> m_callback;
};

class EmbeddedVSplitContainer : public RocWidget
{
public:
    EmbeddedVSplitContainer(const LayoutItem& t, const LayoutItem& b);

    virtual void Render();

    void SetTop(const LayoutItem& t);
    void SetBottom(const LayoutItem& b);

    void SetSplit(float ratio);
    void SetMinTopHeight(float height);
    void SetMinBottomHeight(float height);

private:
    float m_top_min_height    = 20.0f;
    float m_bottom_min_height = 20.0f;

    LayoutItem m_top;
    LayoutItem m_bottom;
    float      m_resize_grip_size = 6.0f;

    float m_split_ratio = 0.5f;

    // Drag state for smooth embedded split
    bool  m_dragging          = false;
    float m_drag_start_ratio  = 0.5f;
    float m_drag_start_mouse  = 0.0f;
    float m_drag_start_height = 0.0f;
};

class VFixedContainer : public RocWidget
{
public:
    VFixedContainer();
    VFixedContainer(std::vector<LayoutItem>& items);
    // Todo, add, insert, remove operations
    // ex: void AddItem(LayoutItem &item);

    bool              SetAt(int index, const LayoutItem& item);
    const LayoutItem* GetAt(int index) const;
    LayoutItem*       GetMutableAt(int index);

    size_t ItemCount();

    virtual void Render();

protected:
    std::vector<LayoutItem> m_children;
};

class HSplitContainer : public RocWidget
{
public:
    HSplitContainer(const LayoutItem& l, const LayoutItem& r);

    virtual void Render();
    void         SetLeft(const LayoutItem& l);
    void         setRight(const LayoutItem& r);

    void SetSplit(float ratio);
    void SetMinLeftWidth(float width);
    void SetMinRightWidth(float width);

private:
    float m_left_min_width;
    float m_right_min_width;

    LayoutItem m_left;
    LayoutItem m_right;
    float      m_resize_grip_size;

    std::string m_left_name;
    std::string m_right_name;
    std::string m_handle_name;

    float m_split_ratio;
};

class VSplitContainer : public RocWidget
{
public:
    VSplitContainer(const LayoutItem& t, const LayoutItem& b);

    virtual void Render();

    void SetTop(const LayoutItem& t);
    void SetBottom(const LayoutItem& b);

    void SetSplit(float ratio);
    void SetMinTopHeight(float height);
    void SetMinBottomHeight(float height);

private:
    float m_top_min_height;
    float m_bottom_min_height;

    LayoutItem m_top;
    LayoutItem m_bottom;
    float      m_resize_grip_size;

    std::string m_top_name;
    std::string m_bottom_name;
    std::string m_handle_name;

    float m_split_ratio;
};

struct TabItem
{
    std::string                m_label;
    std::string                m_id;
    std::shared_ptr<RocWidget> m_widget;
    bool                       m_can_close;
};

class TabContainer : public RocWidget
{
public:
    TabContainer();
    virtual ~TabContainer();

    virtual void Render();
    virtual void Update();

    void AddTab(const TabItem& tab);
    void RemoveTab(const std::string& id);
    void RemoveTab(int index);

    void SetActiveTab(int index);
    void SetActiveTab(const std::string& id);

    void SetTabLabel(const std::string& label, const std::string& id);

    const TabItem* GetActiveTab() const;

    void SetAllowToolTips(bool allow_tool_tips);
    bool GetAllowToolTips() const;

    const std::vector<const TabItem*> GetTabs();

    void               SetEventSourceName(const std::string& source_name);
    const std::string& GetEventSourceName() const;

    void EnableSendCloseEvent(bool enable);
    void EnableSendChangeEvent(bool enable);

private:
    std::vector<TabItem> m_tabs;
    int                  m_active_tab_index;  // index of the currently active tab
    int  m_set_active_tab_index;      // used to programmatically set the active tab
    bool m_allow_tool_tips;           // whether to show tooltips for tabs
    bool m_enable_send_close_event;   // enable sending close tab events
    bool m_enable_send_change_event;  // enable sending tab selection events

    // Friendly source name for events generated by this container. If empty, the
    // source name will be the m_widget_name
    std::string m_event_source_name;
};

}  // namespace View
}  // namespace RocProfVis
