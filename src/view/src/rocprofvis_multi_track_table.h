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
    MultiTrackTable(DataProvider& dp, TableType table_type,
                    rocprofvis_controller_table_type_t        request_table_type,
                    uint64_t                                  request_id,
                    const std::function<const TablesModel&()> table_model,
                    const std::function<TablesModel&()>       table_model_mutable,
                    bool                                      display_filters,
                    std::shared_ptr<TimelineSelection>        timeline_selection,
                    uint64_t                           default_sort_column_index = 1,
                    rocprofvis_controller_sort_order_t default_sort_order =
                        kRPVControllerSortOrderAscending,
                    const std::string& friendly_name = "",
                    const std::string& no_data_text  = "");

    ~MultiTrackTable();

    void Render() override;
    void Update() override;

    virtual void HandleTrackSelectionChanged(uint64_t track_id, bool selected);
    virtual void HandleTimeRangeSelectionChanged(double start_ns, double end_ns);

protected:
    virtual bool IncludeTrack(uint64_t track_id) const;
    void         FormatData() const override;
    void         IndexColumns() override;
    void         RowSelected(const ImGuiMouseButton mouse_button) override;

    // Subset of selected tracks applicable to this table type
    std::vector<uint64_t> m_included_tracks;

private:
    void FetchSelectionData();
    bool XButton(const char* id) const;

    bool m_retry_selection_fetch;
    bool m_display_filters;

    std::vector<std::string> m_group_by_choices;
    std::vector<const char*> m_group_by_choices_ptr;
    int                      m_group_by_selection_index;

    char m_filter_store[FILTER_SIZE];
};

}  // namespace View
}  // namespace RocProfVis
