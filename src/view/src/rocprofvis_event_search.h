// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "imgui.h"
#include "widgets/rocprofvis_infinite_scroll_table.h"
#include <vector>

namespace RocProfVis
{
namespace View
{

class EventSearch : public InfiniteScrollTable
{
public:
    EventSearch(DataProvider& dp, std::shared_ptr<TimelineSelection> timeline_selection);
    void Update() override;
    void Render() override;

    void Show();
    void Search();
    void Clear();
    void SetWidth(float width);

    char*  TextInput();
    size_t TextInputLimit() const;
    bool   FocusTextInput();
    bool   Searched() const;
    float  Width() const;
#ifdef IMGUI_ENABLE_TEST_ENGINE
    size_t GetResultCountForTest() const { return m_table_model().GetTableTotalRowCount(m_table_type); }
    bool   RequestPendingForTest() const { return m_data_provider.IsRequestPending(m_request_id); }
#endif

private:
    void FormatData() const override;
    void IndexColumns() override;
    void RowSelected(const ImGuiMouseButton mouse_button) override;

    bool  Open() const;
    float Height() const;

    bool  m_should_open;
    bool  m_should_close;
    bool  m_focus_text_input;
    bool  m_search_deferred;
    bool  m_searched;
    float m_width;

    char m_text_input[256];
};

}  // namespace View
}  // namespace RocProfVis
