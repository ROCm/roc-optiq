// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "imgui.h"
#include "widgets/rocprofvis_infinite_scroll_table.h"
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

class TimelineSelection;

class MultiTrackTable : public InfiniteScrollTable
{
public:
    MultiTrackTable(DataProvider&                      dp,
                    std::shared_ptr<TimelineSelection> timeline_selection,
                    TableType table_type = TableType::kEventTable);

    ~MultiTrackTable();
    
    void Render() override;
    void Update() override;

    void HandleTrackSelectionChanged();

private:
    // Important columns in the table
    enum ImportantColumns
    {
        kId,
        kName,
        kTrackId,
        kStreamId,
        kNumImportantColumns
    };

    void FormatData() const override;
    void IndexColumns() override;
    void RowSelected(const ImGuiMouseButton mouse_button) override;
    void RenderContextMenu();
    bool XButton(const char* id) const;

    std::shared_ptr<TimelineSelection> m_timeline_selection;
    bool                               m_defer_track_selection_changed;

    // Keep track of currently selected tracks for this table type
    std::vector<uint64_t> m_selected_tracks;

    bool m_open_context_menu;

    std::vector<size_t> m_important_column_idxs;
};

}  // namespace View
}  // namespace RocProfVis
