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
    enum FilterMode
    {
        kNone     = 0,
        kBasic    = 1 << 0,
        kAdvanced = 1 << 1,
    };

    MultiTrackTable(DataProvider& dp, TableType table_type,
                    rocprofvis_controller_table_type_t        request_table_type,
                    uint64_t                                  request_id,
                    const std::function<const TablesModel&()> table_model,
                    const std::function<TablesModel&()>       table_model_mutable,
                    std::shared_ptr<TimelineSelection>        timeline_selection,
                    int                                available_filter_modes    = kNone,
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
    virtual void UpdateFetchParams(std::shared_ptr<TableRequestParams>& params) const override;
    void         FormatData() const override;
    void         IndexColumns() override;
    void         RowSelected(const ImGuiMouseButton mouse_button) override;

    // Subset of selected tracks applicable to this table type
    std::vector<uint64_t> m_included_tracks;

private:
    void FetchSelectionData();
    void ApplyFilter(bool reset);

    std::vector<std::string> m_group_by_choices;
    std::vector<const char*> m_group_by_choices_ptr;
    int                      m_group_by_selection_index;

    int           m_available_filter_modes;
    FilterMode    m_filter_mode;
    FilterOptions m_filter_advanced;
    std::string   m_filter_store;
};

}  // namespace View
}  // namespace RocProfVis
