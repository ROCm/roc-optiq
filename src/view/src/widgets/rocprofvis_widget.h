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

    std::string GenUniqueName(std::string name);

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

    int32_t m_bg_color;

    ImVec2 m_item_spacing;
    ImVec2 m_window_padding;

    ImGuiChildFlags m_child_window_flags;
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

    bool   SetAt(int index, const LayoutItem& item);
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
    float m_left_col_width;
    float m_left_min_width;
    float m_right_min_width;

    LayoutItem m_left;
    LayoutItem m_right;
    float      m_resize_grip_size;

    std::string m_left_name;
    std::string m_right_name;
    std::string m_handle_name;

    float m_split_ratio;
    bool  m_dirty;
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
    float m_top_row_height;
    float m_top_min_height;
    float m_bottom_min_height;

    LayoutItem m_top;
    LayoutItem m_bottom;
    float      m_resize_grip_size;

    std::string m_top_name;
    std::string m_bottom_name;
    std::string m_handle_name;

    float m_split_ratio;
    bool  m_dirty;
};

}  // namespace View
}  // namespace RocProfVis
