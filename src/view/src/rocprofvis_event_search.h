// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "imgui.h"
#include "widgets/rocprofvis_infinite_scroll_table.h"

namespace RocProfVis
{
namespace View
{

class EventSearch : public InfiniteScrollTable
{
public:
    EventSearch(DataProvider& dp, std::shared_ptr<TimelineSelection> timeline_selection);
    ~EventSearch() override;

    void Update() override;
    void Render() override;

    // Setters...
    void Show();
    void Search();
    void Clear();
    void ToggleOptions();
    void SetWidth(float width);

    // Getters...
    char*  TextInput();
    size_t TextInputLimit() const;
    bool   FocusTextInput();
    bool   Searched() const;
    bool   Advanced() const;
    float  Width() const;

private:
    void ResetOptions();
    void FormatData() const override;
    void IndexColumns() override;
    void RowSelected(const ImGuiMouseButton mouse_button) override;

    // Advanced options...
    bool m_show_options;
    bool m_include_substrings;
    bool m_include_category;
    bool m_partial_matching;
    bool m_respect_range_selection;

    bool  m_should_open;
    bool  m_should_close;
    bool  m_is_open;
    bool  m_focus_text_input;
    bool  m_search_deferred;
    bool  m_searched;
    bool  m_options_changed;
    bool  m_advanced_active;
    float m_width;

    char m_text_input[256];

    EventManager::SubscriptionToken m_time_range_changed_token;
};

}  // namespace View
}  // namespace RocProfVis
