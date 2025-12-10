// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

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
    , m_split_ratio(split_ratio) {};
    virtual ~SplitContainerBase() = default;
    virtual void Render() override;

    void SetSplit(float ratio) { m_split_ratio = ratio; };

protected:
    virtual float  GetAvailableSize(const ImVec2& total_size) = 0;
    virtual void   SetCursor()                                = 0;
    virtual ImVec2 GetFirstChildSize(float available_width)   = 0;
    virtual ImVec2 GetSecondChildSize()                       = 0;
    virtual void   UpdateSplitRatio(const ImVec2& mouse_pos, const ImVec2& window_pos,
                                    float available_size)     = 0;
    virtual ImVec2 GetSplitterSize(const ImVec2& total_size)  = 0;
    virtual void   AddSameLine()                              = 0;
    virtual float  GetItemSize()                              = 0;

    void SetFirst(LayoutItem::Ptr first);
    void SetSecond(LayoutItem::Ptr second);

    void SetMinFirstSize(float size);
    void SetMinSecondSize(float size);

    LayoutItem::Ptr m_first;
    LayoutItem::Ptr m_second;

    float m_first_min_size;
    float m_second_min_size;

    float m_split_ratio;
    float m_resize_grip_size;

    std::string m_first_name;
    std::string m_second_name;
    std::string m_handle_name;

    float m_optimal_size = 0.0f;
};

class HSplitContainer : public SplitContainerBase
{
public:
    HSplitContainer(LayoutItem::Ptr left, LayoutItem::Ptr right);
    void SetLeft(LayoutItem::Ptr left);
    void SetRight(LayoutItem::Ptr right);

    void  SetMinLeftWidth(float width);
    void  SetMinRightWidth(float width);
    float GetOptimalHeight() const;

private:
    float  GetAvailableSize(const ImVec2& total_size) override;
    void   SetCursor() override;
    ImVec2 GetFirstChildSize(float available_width) override;
    ImVec2 GetSecondChildSize() override;
    void   UpdateSplitRatio(const ImVec2& mouse_pos, const ImVec2& window_pos,
                            float available_width) override;
    ImVec2 GetSplitterSize(const ImVec2& total_size) override;
    void   AddSameLine() override;
    float  GetItemSize() override;
};

class VSplitContainer : public SplitContainerBase
{
public:
    VSplitContainer(LayoutItem::Ptr top, LayoutItem::Ptr bottom);

    void SetTop(LayoutItem::Ptr top);
    void SetBottom(LayoutItem::Ptr bottom);

    void  SetMinTopHeight(float height);
    void  SetMinBottomHeight(float height);
    float GetOptimalWidth() const;

private:
    float  GetAvailableSize(const ImVec2& total_size) override;
    void   SetCursor() override;
    ImVec2 GetFirstChildSize(float available_width) override;
    ImVec2 GetSecondChildSize() override;
    void   UpdateSplitRatio(const ImVec2& mouse_pos, const ImVec2& window_pos,
                            float available_height) override;

    ImVec2 GetSplitterSize(const ImVec2& total_size) override;
    void   AddSameLine() override {};  // No same line for vertical split
    float  GetItemSize() override;
};

}  // namespace View
}  // namespace RocProfVis
