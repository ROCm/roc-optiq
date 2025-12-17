// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/rocprofvis_dialog.h"
#include "rocprofvis_events.h"

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

    static Ptr CreateFromWidget(std::shared_ptr<RocWidget> widget, float w = 0,
                                float h = 0);

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

struct TabItem
{
    std::string                m_label;
    std::string                m_id;
    std::shared_ptr<RocWidget> m_widget;
    bool                       m_can_close;
};

void
WithPadding(float left, float right, float top, float bottom,
            const std::function<void()>& content);

// Similar to ImGui::TextUnformatted(), but allows copying the text to the clipboard
// via left-click or a context menu.
// If multiple instances with the same text can appear within the same frame,
// the caller must provide a unique, stable identifier to avoid ID collisions.
// For items created in loops, the loop index or iterator is typically sufficient.
bool
CopyableTextUnformatted(const char* text, std::string_view unique_id = "",
                        std::string_view notification = "",
                        bool one_click_copy = false, bool context_menu = false);


inline constexpr std::string_view COPY_DATA_NOTIFICATION = "Cell data was copied";

}  // namespace View
}  // namespace RocProfVis
