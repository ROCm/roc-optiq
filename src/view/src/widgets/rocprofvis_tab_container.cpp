// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_tab_container.h"
#include "rocprofvis_gui_helpers.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_render_scheduler.h"
#include <algorithm>
#include <utility>

namespace RocProfVis
{
namespace View
{

// Tab-strip layout constants (pixels, or fractions of the font size where noted).
inline constexpr float TAB_HEIGHT_PADDING    = 4.0f;   // extra tab height over the frame height
inline constexpr float TAB_PADDING_X         = 12.0f;  // horizontal padding inside a tab / chip
inline constexpr float TAB_GAP               = 2.0f;   // gap between tabs
inline constexpr float TAB_STRIP_LEFT_MARGIN = 6.0f;   // inset before the first tab
inline constexpr float TAB_MIN_WIDTH         = 90.0f;  // min tab width before eliding
inline constexpr float TAB_MAX_WIDTH         = 240.0f; // max tab width
inline constexpr float TAB_CLOSE_SIZE_RATIO  = 0.5f;   // close glyph size as a fraction of the font
inline constexpr float TAB_CLOSE_HIT_PADDING = 10.0f;  // extra width reserved for the close button
inline constexpr float TAB_CARET_SIZE_RATIO  = 0.6f;   // group caret size as a fraction of the font

TabContainer::TabContainer()
: m_active_tab_index(s_invalid_index)
, m_set_active_tab_index(s_invalid_index)
, m_allow_tool_tips(true)
, m_enable_send_close_event(false)
, m_enable_send_change_event(false)
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
    SettingsManager& settings = SettingsManager::GetInstance();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgFrame));
    ImGui::BeginChild(m_widget_name.c_str(), ImVec2(0, 0), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);

    if(!m_tabs.empty())
    {
        // Honor a programmatic selection (SetActiveTab / ReorderTabs) and keep the
        // active index valid.
        if(m_set_active_tab_index >= 0 && m_set_active_tab_index < static_cast<int>(m_tabs.size()))
        {
            if(m_active_tab_index != m_set_active_tab_index)
            {
                m_active_tab_index = m_set_active_tab_index;
                if(m_enable_send_change_event)
                {
                    SendEvent(RocEvents::kTabSelected, m_tabs[m_active_tab_index].m_id);
                }
            }
        }
        m_set_active_tab_index = s_invalid_index;
        if(m_active_tab_index < 0 || m_active_tab_index >= static_cast<int>(m_tabs.size()))
        {
            m_active_tab_index = 0;
            if(m_enable_send_change_event)
            {
                SendEvent(RocEvents::kTabSelected, m_tabs[0].m_id);
            }
        }

        RenderTabStrip();

        // Render the active tab's content beneath the strip.
        if(m_active_tab_index >= 0 && m_active_tab_index < static_cast<int>(m_tabs.size()))
        {
            const TabItem&             active = m_tabs[m_active_tab_index];
            std::shared_ptr<RocWidget> widget = active.m_widget;
            if(widget)
            {
                ImGui::PushID(active.m_id.c_str());
                widget->Render();
                ImGui::PopID();
            }
        }

        // Deferred close confirmation (must run after interaction so a removal does
        // not invalidate indices mid-frame).
        m_confirmation_dialog->Render();
        if(m_pending_to_remove != s_invalid_index)
        {
            ShowCloseTabConfirm(m_pending_to_remove);
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void
TabContainer::RenderTabStrip()
{
    SettingsManager& settings = SettingsManager::GetInstance();
    ImDrawList*      draw     = ImGui::GetWindowDrawList();

    const ImGuiStyle& style        = settings.GetDefaultStyle();
    const float       font_h       = ImGui::GetFontSize();
    const float       tab_h        = ImGui::GetFrameHeight() + TAB_HEIGHT_PADDING;
    const float       rounding     = style.FrameRounding;
    const ImDrawFlags corner_flags = ImDrawFlags_RoundCornersTop;
    const float       pad_x        = TAB_PADDING_X;
    const float       gap          = TAB_GAP;
    const float       left_margin  = TAB_STRIP_LEFT_MARGIN;
    const float       close_sz     = font_h * TAB_CLOSE_SIZE_RATIO;
    const float       close_w      = close_sz + TAB_CLOSE_HIT_PADDING;
    const float       min_w        = TAB_MIN_WIDTH;
    const float       max_w        = TAB_MAX_WIDTH;
    const float       arrow_w      = font_h * TAB_CARET_SIZE_RATIO;

    const ImU32 col_text  = settings.GetColor(Colors::kTextMain);
    const ImU32 col_tab   = settings.GetColor(Colors::kButton);
    const ImU32 col_hover = settings.GetColor(Colors::kButtonHovered);
    const ImU32 col_bgsel = settings.GetColor(Colors::kBgPanel);

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float  y0     = origin.y;
    const float  y1     = origin.y + tab_h;

    const size_t       n = m_tabs.size();
    std::vector<float> tab_left(n, 0.0f);
    std::vector<float> tab_right(n, 0.0f);

    // A "slot" is a top-level draggable unit: a single ungrouped tab, or a whole
    // group (chip + its member tabs).
    struct Slot
    {
        std::string group_id;  // empty for a single ungrouped tab
        size_t      first;
        size_t      last;
        float       x0;
        float       x1;
    };
    std::vector<Slot> slots;

    // Interaction results, applied after the layout loop (mutating m_tabs mid-loop
    // is unsafe).
    int         want_select = s_invalid_index;
    int         want_close  = s_invalid_index;
    std::string pressed_tab;
    std::string pressed_group;
    bool        pressed_group_collapsed = false;
    float       pressed_x0              = 0.0f;

    // Geometry of the currently dragged element, captured during layout so a floating
    // "picked up" ghost can be drawn after the strip.
    bool        has_ghost     = false;
    float       ghost_w       = 0.0f;
    ImU32       ghost_col     = 0;
    std::string ghost_label;
    bool        ghost_is_chip = false;

    // Draws one tab at cursor_x and returns the advanced cursor_x.
    auto draw_tab = [&](size_t idx, float cx) -> float {
        TabItem& tab    = m_tabs[idx];
        bool     active = (static_cast<int>(idx) == m_active_tab_index);

        float text_w = ImGui::CalcTextSize(tab.m_label.c_str()).x;
        float w      = text_w + pad_x * 2.0f + (tab.m_can_close ? close_w : 0.0f);
        w            = std::max(min_w, std::min(w, max_w));
        float x0     = cx;
        float x1     = cx + w;
        tab_left[idx]  = x0;
        tab_right[idx] = x1;

        // Body hitbox (excludes the close-button region so they do not overlap).
        float body_w = tab.m_can_close ? (w - close_w) : w;
        ImGui::SetCursorScreenPos(ImVec2(x0, y0));
        ImGui::InvisibleButton(("##tab_" + tab.m_id).c_str(), ImVec2(body_w, tab_h));
        bool hovered = ImGui::IsItemHovered();
        if(ImGui::IsItemActivated())
        {
            pressed_tab = tab.m_id;
            want_select = static_cast<int>(idx);
            pressed_x0  = x0;
        }
        if(hovered && m_allow_tool_tips)
        {
            SetTooltipStyled("%s", tab.m_id.c_str());
        }
        if(m_tab_context_menu_callback)
        {
            // Match the app's menu padding so the context menu is consistent with the
            // rest of the UI (it is opened from inside the tab strip, so it does not
            // otherwise inherit the menu-bar style).
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.0f, 6.0f));
            if(ImGui::BeginPopupContextItem(("##tabctx_" + tab.m_id).c_str()))
            {
                m_tab_context_menu_callback(tab.m_id);
                ImGui::EndPopup();
            }
            ImGui::PopStyleVar(3);
        }

        // The dragged tab renders as a faded placeholder gap; the floating ghost
        // (drawn after the strip) is the visible one that follows the cursor.
        bool is_dragged = m_drag_active && m_drag_kind == 1 && tab.m_id == m_drag_id;
        if(is_dragged)
        {
            draw->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1),
                                ApplyAlpha(col_tab, 0.35f), rounding, corner_flags);
            has_ghost     = true;
            ghost_w       = w;
            ghost_label   = tab.m_label;
            ghost_is_chip = false;
            ghost_col     = tab.m_group_color;
            return x1 + gap;
        }

        // Background (+ group tint).
        ImU32 bg = active ? col_bgsel : (hovered ? col_hover : col_tab);
        draw->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), bg, rounding, corner_flags);
        if(tab.m_group_color != 0)
        {
            float wash = active ? 0.30f : (hovered ? 0.24f : 0.16f);
            draw->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1),
                                ApplyAlpha(tab.m_group_color, wash), rounding, corner_flags);
        }

        // Label (clipped to the available area).
        float  label_avail = body_w - pad_x * 2.0f;
        ImVec4 clip(x0 + pad_x, y0, x0 + pad_x + std::max(label_avail, 0.0f), y1);
        draw->AddText(nullptr, 0.0f, ImVec2(x0 + pad_x, y0 + (tab_h - font_h) * 0.5f),
                      col_text, tab.m_label.c_str(), nullptr, 0.0f, &clip);

        // Close button.
        if(tab.m_can_close)
        {
            float cxb = x1 - pad_x - close_sz;
            float cyb = y0 + (tab_h - close_sz) * 0.5f;
            ImGui::SetCursorScreenPos(ImVec2(x0 + body_w, y0));
            ImGui::InvisibleButton(("##close_" + tab.m_id).c_str(), ImVec2(close_w, tab_h));
            bool x_hover = ImGui::IsItemHovered();
            if(ImGui::IsItemClicked(ImGuiMouseButton_Left))
            {
                want_close = static_cast<int>(idx);
            }
            ImU32 xcol = x_hover ? col_text : ApplyAlpha(col_text, 0.65f);
            draw->AddLine(ImVec2(cxb, cyb), ImVec2(cxb + close_sz, cyb + close_sz), xcol, 1.5f);
            draw->AddLine(ImVec2(cxb + close_sz, cyb), ImVec2(cxb, cyb + close_sz), xcol, 1.5f);
        }
        return x1 + gap;
    };

    float  cursor_x = origin.x + left_margin;
    size_t i        = 0;
    while(i < n)
    {
        TabItem& t0      = m_tabs[i];
        bool     grouped = t0.m_group_color != 0 && !t0.m_group_id.empty();
        if(grouped)
        {
            size_t j = i;
            while(j + 1 < n && m_tabs[j + 1].m_group_color != 0 &&
                  m_tabs[j + 1].m_group_id == t0.m_group_id)
            {
                j++;
            }
            const std::string group_id = t0.m_group_id;
            bool  collapsed = m_collapsed_groups.find(group_id) != m_collapsed_groups.end();
            ImU32 chip_col  = t0.m_group_color;
            std::string chip_lbl =
                t0.m_group_label.empty() ? std::string("Group") : t0.m_group_label;
            float chip_text = ImGui::CalcTextSize(chip_lbl.c_str()).x;
            float chip_w    = chip_text + pad_x * 1.6f + arrow_w;
            float chip_x0   = cursor_x;

            ImGui::SetCursorScreenPos(ImVec2(chip_x0, y0));
            ImGui::InvisibleButton(("##chip_" + group_id).c_str(), ImVec2(chip_w, tab_h));
            if(ImGui::IsItemActivated())
            {
                pressed_group           = group_id;
                pressed_group_collapsed = collapsed;
                pressed_x0              = chip_x0;
            }
            if(m_chip_context_menu_callback)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.0f, 6.0f));
                if(ImGui::BeginPopupContextItem(("##chipctx_" + group_id).c_str()))
                {
                    m_chip_context_menu_callback(group_id);
                    ImGui::EndPopup();
                }
                ImGui::PopStyleVar(3);
            }

            bool chip_dragged = m_drag_active && m_drag_kind == 2 && group_id == m_drag_id;
            if(chip_dragged)
            {
                has_ghost     = true;
                ghost_w       = chip_w;
                ghost_label   = chip_lbl;
                ghost_is_chip = true;
                ghost_col     = chip_col;
            }
            draw->AddRectFilled(ImVec2(chip_x0, y0), ImVec2(chip_x0 + chip_w, y1),
                                chip_dragged ? ApplyAlpha(chip_col, 0.35f) : chip_col, rounding,
                                corner_flags);
            // Collapse/expand caret.
            float ax = chip_x0 + pad_x * 0.5f;
            float ay = y0 + tab_h * 0.5f;
            if(collapsed)
            {
                draw->AddTriangleFilled(ImVec2(ax, ay - arrow_w * 0.35f),
                                        ImVec2(ax, ay + arrow_w * 0.35f),
                                        ImVec2(ax + arrow_w * 0.5f, ay), IM_COL32_WHITE);
            }
            else
            {
                draw->AddTriangleFilled(ImVec2(ax, ay - arrow_w * 0.2f),
                                        ImVec2(ax + arrow_w * 0.6f, ay - arrow_w * 0.2f),
                                        ImVec2(ax + arrow_w * 0.3f, ay + arrow_w * 0.3f),
                                        IM_COL32_WHITE);
            }
            draw->AddText(ImVec2(chip_x0 + pad_x * 0.5f + arrow_w, y0 + (tab_h - font_h) * 0.5f),
                          IM_COL32_WHITE, chip_lbl.c_str());
            cursor_x = chip_x0 + chip_w + gap;

            Slot slot;
            slot.group_id = group_id;
            slot.first    = i;
            slot.last     = j;
            slot.x0       = chip_x0;
            if(collapsed)
            {
                slot.x1 = chip_x0 + chip_w;
                draw->AddRectFilled(ImVec2(chip_x0, y1 - 2.0f), ImVec2(chip_x0 + chip_w, y1),
                                    chip_col, 1.0f);
            }
            else
            {
                for(size_t k = i; k <= j; k++)
                {
                    cursor_x = draw_tab(k, cursor_x);
                }
                slot.x1 = tab_right[j];
                draw->AddRectFilled(ImVec2(chip_x0, y1 - 2.0f), ImVec2(tab_right[j], y1),
                                    chip_col, 1.0f);
            }
            slots.push_back(slot);
            i = j + 1;
        }
        else
        {
            float x0 = cursor_x;
            cursor_x = draw_tab(i, cursor_x);
            Slot slot;
            slot.first = i;
            slot.last  = i;
            slot.x0    = x0;
            slot.x1    = tab_right[i];
            slots.push_back(slot);
            i++;
        }
    }

    // Reserve the strip height so the active content flows underneath.
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(0.0f, tab_h + 6.0f));

    // Floating "picked up" copy of the dragged tab/chip so the drag is clearly
    // visible (it follows the cursor, lifted with a shadow and outline).
    if(m_drag_active && has_ghost)
    {
        float gx  = ImGui::GetMousePos().x - m_drag_grab_dx;
        float gy  = y0 - 3.0f;
        float gx1 = gx + ghost_w;
        float gy1 = gy + tab_h;
        draw->AddRectFilled(ImVec2(gx + 2.0f, gy + 3.0f), ImVec2(gx1 + 2.0f, gy1 + 3.0f),
                            IM_COL32(0, 0, 0, 70), rounding);
        ImU32 body = ghost_is_chip ? ghost_col : col_bgsel;
        draw->AddRectFilled(ImVec2(gx, gy), ImVec2(gx1, gy1), body, rounding);
        if(!ghost_is_chip && ghost_col != 0)
        {
            draw->AddRectFilled(ImVec2(gx, gy), ImVec2(gx1, gy1),
                                ApplyAlpha(ghost_col, 0.30f), rounding);
        }
        draw->AddRect(ImVec2(gx, gy), ImVec2(gx1, gy1), col_text, rounding, 0, 1.5f);
        ImU32 lcol = ghost_is_chip ? IM_COL32_WHITE : col_text;
        draw->AddText(ImVec2(gx + pad_x, gy + (tab_h - font_h) * 0.5f), lcol,
                      ghost_label.c_str());
    }

    // Apply close (takes precedence over select/drag).
    if(want_close != s_invalid_index)
    {
        if(SettingsManager::GetInstance().GetUserSettings().dont_ask_before_tab_closing)
        {
            RemoveTab(want_close);
        }
        else
        {
            m_pending_to_remove = want_close;
        }
        pressed_tab.clear();
        want_select = s_invalid_index;
    }
    else if(want_select != s_invalid_index && m_active_tab_index != want_select)
    {
        m_active_tab_index = want_select;
        if(m_enable_send_change_event)
        {
            SendEvent(RocEvents::kTabSelected, m_tabs[want_select].m_id);
        }
        RenderScheduler::GetInstance().RequestRender();
    }

    // Begin a drag on press.
    if(m_drag_kind == 0)
    {
        if(!pressed_tab.empty())
        {
            m_drag_kind    = 1;
            m_drag_id      = pressed_tab;
            m_drag_active  = false;
            m_drag_grab_dx = ImGui::GetMousePos().x - pressed_x0;
        }
        else if(!pressed_group.empty())
        {
            m_drag_kind                = 2;
            m_drag_id                  = pressed_group;
            m_drag_active              = false;
            m_drag_group_was_collapsed = pressed_group_collapsed;
            m_drag_grab_dx             = ImGui::GetMousePos().x - pressed_x0;
        }
    }

    if(m_drag_kind != 0)
    {
        if(!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            // Released: finalize.
            if(m_drag_kind == 2)
            {
                if(!m_drag_active)
                {
                    // A click without a drag toggles the group's collapsed state.
                    if(m_collapsed_groups.count(m_drag_id) > 0)
                    {
                        m_collapsed_groups.erase(m_drag_id);
                    }
                    else
                    {
                        m_collapsed_groups.insert(m_drag_id);
                    }
                }
                else if(!m_drag_group_was_collapsed)
                {
                    m_collapsed_groups.erase(m_drag_id);  // restore after a group drag
                }
            }
            if(m_drag_active && m_tabs_reordered_callback)
            {
                m_tabs_reordered_callback();
            }
            m_drag_kind   = 0;
            m_drag_active = false;
            m_drag_id.clear();
            RenderScheduler::GetInstance().RequestRender();
        }
        else if(ImGui::IsMouseDragging(ImGuiMouseButton_Left, 5.0f))
        {
            m_drag_active   = true;
            float mouse_x   = ImGui::GetMousePos().x;

            if(m_drag_kind == 2)
            {
                m_collapsed_groups.insert(m_drag_id);  // collapse while dragging a group
                int ds = -1;
                for(int s = 0; s < static_cast<int>(slots.size()); s++)
                {
                    if(slots[s].group_id == m_drag_id)
                    {
                        ds = s;
                        break;
                    }
                }
                if(ds >= 0)
                {
                    int target = ds;
                    if(mouse_x < slots[ds].x0 && ds > 0)
                    {
                        target = ds - 1;
                    }
                    else if(mouse_x > slots[ds].x1 && ds < static_cast<int>(slots.size()) - 1)
                    {
                        target = ds + 1;
                    }
                    if(target != ds)
                    {
                        std::vector<int> order(slots.size());
                        for(int s = 0; s < static_cast<int>(slots.size()); s++)
                        {
                            order[s] = s;
                        }
                        std::swap(order[ds], order[target]);
                        std::vector<std::string> id_order;
                        for(int s : order)
                        {
                            for(size_t k = slots[s].first; k <= slots[s].last; k++)
                            {
                                id_order.push_back(m_tabs[k].m_id);
                            }
                        }
                        ReorderTabs(id_order);
                    }
                }
            }
            else  // dragging a single tab
            {
                int d = -1;
                for(size_t k = 0; k < m_tabs.size(); k++)
                {
                    if(m_tabs[k].m_id == m_drag_id)
                    {
                        d = static_cast<int>(k);
                        break;
                    }
                }
                if(d >= 0)
                {
                    bool grouped =
                        m_tabs[d].m_group_color != 0 && !m_tabs[d].m_group_id.empty();
                    if(grouped)
                    {
                        // Reorder within the group's contiguous run (keeps groups intact).
                        int gi = d;
                        int gj = d;
                        while(gi > 0 && m_tabs[gi - 1].m_group_color != 0 &&
                              m_tabs[gi - 1].m_group_id == m_tabs[d].m_group_id)
                        {
                            gi--;
                        }
                        while(gj + 1 < static_cast<int>(m_tabs.size()) &&
                              m_tabs[gj + 1].m_group_color != 0 &&
                              m_tabs[gj + 1].m_group_id == m_tabs[d].m_group_id)
                        {
                            gj++;
                        }
                        int target = d;
                        if(mouse_x < tab_left[d] && d > gi)
                        {
                            target = d - 1;
                        }
                        else if(mouse_x > tab_right[d] && d < gj)
                        {
                            target = d + 1;
                        }
                        if(target != d)
                        {
                            std::vector<std::string> id_order;
                            for(size_t k = 0; k < m_tabs.size(); k++)
                            {
                                id_order.push_back(m_tabs[k].m_id);
                            }
                            std::swap(id_order[d], id_order[target]);
                            ReorderTabs(id_order);
                        }
                    }
                    else
                    {
                        // Ungrouped tab: reorder its slot among the top-level slots.
                        int ds = -1;
                        for(int s = 0; s < static_cast<int>(slots.size()); s++)
                        {
                            if(slots[s].group_id.empty() &&
                               slots[s].first == static_cast<size_t>(d))
                            {
                                ds = s;
                                break;
                            }
                        }
                        if(ds >= 0)
                        {
                            int target = ds;
                            if(mouse_x < slots[ds].x0 && ds > 0)
                            {
                                target = ds - 1;
                            }
                            else if(mouse_x > slots[ds].x1 &&
                                    ds < static_cast<int>(slots.size()) - 1)
                            {
                                target = ds + 1;
                            }
                            if(target != ds)
                            {
                                std::vector<int> order(slots.size());
                                for(int s = 0; s < static_cast<int>(slots.size()); s++)
                                {
                                    order[s] = s;
                                }
                                std::swap(order[ds], order[target]);
                                std::vector<std::string> id_order;
                                for(int s : order)
                                {
                                    for(size_t k = slots[s].first; k <= slots[s].last; k++)
                                    {
                                        id_order.push_back(m_tabs[k].m_id);
                                    }
                                }
                                ReorderTabs(id_order);
                            }
                        }
                    }
                }
            }
            RenderScheduler::GetInstance().RequestRender();
        }
    }
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
    auto it = std::find_if(m_tabs.begin(), m_tabs.end(),
                           [&id](const TabItem& tab) { return tab.m_id == id; });
    if(it != m_tabs.end())
    {
        int index = static_cast<int>(std::distance(m_tabs.begin(), it));

        if(m_enable_send_close_event)
        {
            // notify the event manager of the tab removal
            SendEvent(RocEvents::kTabClosed, it->m_id);
        }

        m_tabs.erase(it);

        if(m_active_tab_index == index)
        {
            // If the active tab was closed, reset to invalid index
            // (Render loop will handle selecting a new active tab)
            m_active_tab_index = s_invalid_index;
        }
        else if(m_active_tab_index > index)
        {
            // Adjust active tab index
            m_active_tab_index--;
        }
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
            // (Render loop will handle selecting a new active tab)
            m_active_tab_index = s_invalid_index;
        }
        else if(m_active_tab_index > index)
        {
            // Adjust active tab index
            m_active_tab_index--;
        }
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

void
TabContainer::SetTabGroup(const std::string& id, ImU32 color, const std::string& group_id,
                          const std::string& group_label)
{
    for(TabItem& tab : m_tabs)
    {
        if(tab.m_id == id)
        {
            tab.m_group_color = color;
            tab.m_group_id    = (color != 0) ? group_id : std::string();
            tab.m_group_label = (color != 0) ? group_label : std::string();
            break;
        }
    }
}

void
TabContainer::SetTabContextMenuCallback(std::function<void(const std::string&)> callback)
{
    m_tab_context_menu_callback = std::move(callback);
}

void
TabContainer::SetChipContextMenuCallback(std::function<void(const std::string&)> callback)
{
    m_chip_context_menu_callback = std::move(callback);
}

void
TabContainer::SetTabsReorderedCallback(std::function<void()> callback)
{
    m_tabs_reordered_callback = std::move(callback);
}

void
TabContainer::ReorderTabs(const std::vector<std::string>& ordered_ids)
{
    // Preserve the active tab across the reorder by id.
    std::string active_id;
    if(m_active_tab_index >= 0 && m_active_tab_index < static_cast<int>(m_tabs.size()))
    {
        active_id = m_tabs[m_active_tab_index].m_id;
    }

    std::vector<TabItem> reordered;
    reordered.reserve(m_tabs.size());
    std::vector<bool> taken(m_tabs.size(), false);
    for(const std::string& id : ordered_ids)
    {
        for(size_t i = 0; i < m_tabs.size(); i++)
        {
            if(!taken[i] && m_tabs[i].m_id == id)
            {
                reordered.push_back(std::move(m_tabs[i]));
                taken[i] = true;
                break;
            }
        }
    }
    // Append tabs not present in ordered_ids, preserving their relative order.
    for(size_t i = 0; i < m_tabs.size(); i++)
    {
        if(!taken[i])
        {
            reordered.push_back(std::move(m_tabs[i]));
        }
    }
    m_tabs = std::move(reordered);

    // Re-resolve the active tab in its new position.
    m_active_tab_index = s_invalid_index;
    if(!active_id.empty())
    {
        for(size_t i = 0; i < m_tabs.size(); i++)
        {
            if(m_tabs[i].m_id == active_id)
            {
                m_active_tab_index = static_cast<int>(i);
                break;
            }
        }
    }
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
