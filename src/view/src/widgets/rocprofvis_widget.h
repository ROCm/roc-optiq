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
    using Ptr = std::shared_ptr<LayoutItem>;
    LayoutItem() = default;
    LayoutItem(float w, float h)
    : m_width(w)
    , m_height(h)
    {};

    static Ptr CreateFromWidget(std::shared_ptr<RocWidget> widget, float w = 0, float h = 0)
    {
        Ptr item = std::make_shared<LayoutItem>(w, h);
        item->m_item = widget;
        return item;
    }

    std::shared_ptr<RocWidget> m_item = nullptr;  // Widget that this item will render
    float                      m_height = 0;
    float                      m_width = 0;
    bool                       m_visible = true;

    int32_t m_bg_color = 0;

    ImVec2 m_item_spacing = ImVec2(0, 0);
    ImVec2 m_window_padding = ImVec2(0, 0);

    ImGuiChildFlags  m_child_flags = ImGuiChildFlags_Borders;
    ImGuiWindowFlags m_window_flags = ImGuiWindowFlags_None;
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

class SplitContainerBase : public RocWidget
{
public:
    SplitContainerBase(LayoutItem::Ptr first, LayoutItem::Ptr second, float grip_size,
                       float first_min_size, float second_min_size, float split_ratio)
    : m_first(first)
    , m_second(second)
    , m_resize_grip_size(grip_size)
    , m_first_min_size(first_min_size)
    , m_second_min_size(second_min_size)
    , m_split_ratio(split_ratio)
    {};
    virtual ~SplitContainerBase() = default;
    virtual void Render() override;

    void SetSplit(float ratio) { m_split_ratio = ratio; };

protected:
    virtual float  GetAvailableSize(const ImVec2& total_size) = 0;
    virtual void   SetCursor()                                = 0;
    virtual ImVec2 GetFirstChildSize(float available_width) = 0;
    virtual ImVec2 GetSecondChildSize() = 0;
    virtual void   UpdateSplitRatio(const ImVec2& mouse_pos, const ImVec2& window_pos, float available_size) = 0;
    virtual ImVec2 GetSplitterSize(const ImVec2& total_size) = 0;
    virtual void   AddSameLine() = 0;

    void SetFirst(LayoutItem::Ptr first) { m_first = first; };
    void SetSecond(LayoutItem::Ptr second) { m_second = second; };

    void SetMinFirstSize(float size) { m_first_min_size = size; };
    void SetMinSecondSize(float size) { m_second_min_size = size; };

    LayoutItem::Ptr m_first;
    LayoutItem::Ptr m_second;

    float m_first_min_size;
    float m_second_min_size;

    float m_split_ratio;
    float m_resize_grip_size;

    std::string m_first_name;
    std::string m_second_name;
    std::string m_handle_name;
};

class HSplitContainer : public SplitContainerBase
{
public:
    HSplitContainer(LayoutItem::Ptr left, LayoutItem::Ptr right)
        : SplitContainerBase(left, right, 4.0f, 100.0f, 100.0f, 0.25f)
        , m_optimal_height(0.0f)
    {
        m_widget_name = GenUniqueName("HSplitContainer");
        m_first_name  = GenUniqueName("LeftColumn");
        m_handle_name = GenUniqueName("##ResizeHandle");
        m_second_name = GenUniqueName("RightColumn");
    };
    void SetLeft(LayoutItem::Ptr left) { SetFirst(left); };
    void SetRight(LayoutItem::Ptr right) { SetSecond(right); };

    void  SetMinLeftWidth(float width) { SetMinFirstSize(width); };
    void  SetMinRightWidth(float width) { SetMinSecondSize(width); };
    float GetOptimalHeight() const { return m_optimal_height; };

private:
    float GetAvailableSize(const ImVec2& total_size) override;
    void SetCursor() override;
    ImVec2 GetFirstChildSize(float available_width) override;
    ImVec2 GetSecondChildSize() override;
    void UpdateSplitRatio(const ImVec2& mouse_pos, const ImVec2& window_pos,
                          float available_width) override;
    ImVec2 GetSplitterSize(const ImVec2& total_size) override;
    virtual void AddSameLine() override { ImGui::SameLine(); };

    float m_optimal_height;
};

class VSplitContainer : public SplitContainerBase
{
public:
    VSplitContainer(LayoutItem::Ptr top, LayoutItem::Ptr bottom)
        : SplitContainerBase(top, bottom, 4.0f, 200.0f, 100.0f, 0.6f)
    {
        m_widget_name = GenUniqueName("VSplitContainer");
        m_first_name  = GenUniqueName("TopRow");
        m_handle_name = GenUniqueName("##ResizeHandle");
        m_second_name = GenUniqueName("BottomRow");
    };

    void SetTop(LayoutItem::Ptr top) { SetFirst(top); };
    void SetBottom(LayoutItem::Ptr bottom) { SetSecond(bottom); };

    void SetMinTopHeight(float height) { SetMinFirstSize(height); };
    void SetMinBottomHeight(float height) { SetMinSecondSize(height); };
private:
    float GetAvailableSize(const ImVec2& total_size) override;
    void  SetCursor() override;
    ImVec2 GetFirstChildSize(float available_width) override;
    ImVec2 GetSecondChildSize() override;
    void UpdateSplitRatio(const ImVec2& mouse_pos, const ImVec2& window_pos,
                          float available_height) override;

    ImVec2 GetSplitterSize(const ImVec2& total_size) override;
    virtual void AddSameLine() override {};  // No same line for vertical split
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
    void AddTab(TabItem&& tab);
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

void
WithPadding(float left, float right, float top, float bottom,
            const std::function<void()>& content);

}  // namespace View
}  // namespace RocProfVis
