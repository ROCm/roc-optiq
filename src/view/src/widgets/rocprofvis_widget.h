// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_events.h"
#include "widgets/rocprofvis_dialog.h"
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
    using Ptr    = std::shared_ptr<LayoutItem>;
    LayoutItem() = default;
    LayoutItem(float w, float h)
    : m_width(w)
    , m_height(h) {};

    static Ptr CreateFromWidget(std::shared_ptr<RocWidget> widget, float w = 0,
                                float h = 0);

    std::shared_ptr<RocWidget> m_item    = nullptr;  // Widget that this item will render
    float                      m_height  = 0;
    float                      m_width   = 0;
    bool                       m_visible = true;

    int32_t m_bg_color         = 0;
    bool    m_inherit_bg_color = false;

    ImVec2 m_item_spacing   = ImVec2(0, 0);
    ImVec2 m_window_padding = ImVec2(0, 0);

    ImGuiChildFlags  m_child_flags  = ImGuiChildFlags_Borders;
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

struct TabItem
{
    std::string                m_label;
    std::string                m_id;
    std::shared_ptr<RocWidget> m_widget;
    bool                       m_can_close;

    // Chrome-style tab-group decoration. When m_group_color is non-zero this tab
    // is drawn as part of a project group: tinted, underlined, and (for a run of
    // adjacent tabs sharing m_group_id) covered by a spanning colored bar labeled
    // with m_group_label. Zeroed for ungrouped tabs and generic sub-tab bars.
    ImU32       m_group_color = 0;
    std::string m_group_id;
    std::string m_group_label;
};

class PopUpStyle
{
public:
    PopUpStyle();
    ~PopUpStyle();
    
    // Push all popup style variables (RAII - automatically pops on destruction)
    void PushPopupStyles();
    
    // Center the popup window (call before BeginPopupModal)
    void CenterPopup();
    
    // Push titlebar colors using grey from settings
    void PushTitlebarColors();
    
    // Manually pop styles if needed before destruction
    void PopStyles();

private:
    int m_style_var_count = 0;
    int m_color_count = 0;
};

}  // namespace View
}  // namespace RocProfVis
