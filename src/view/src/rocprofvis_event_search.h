// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

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
    EventSearch(DataProvider& dp);
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

private:
    enum ImportantColumns
    {
        kId,
        kName,
        kTrackId,
        kStreamId,
        kNumImportantColumns
    };

    void FormatData() override;
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

    std::vector<size_t> m_important_column_idxs;
};

}  // namespace View
}  // namespace RocProfVis
