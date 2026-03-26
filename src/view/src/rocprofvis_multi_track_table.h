// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "imgui.h"
#include "widgets/rocprofvis_infinite_scroll_table.h"
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

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
    void FormatData() const override;
    void IndexColumns() override;
    void RowSelected(const ImGuiMouseButton mouse_button) override;
    void RenderContextMenu();
    void CopyCellToClipboard(bool use_formatted_data);

    bool m_defer_track_selection_changed;

    // Keep track of currently selected tracks for this table type
    std::vector<uint64_t> m_selected_tracks;

    bool m_open_context_menu;
};

}  // namespace View
}  // namespace RocProfVis
