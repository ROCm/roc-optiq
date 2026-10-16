// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "imgui.h"
#include "widgets/rocprofvis_infinite_scroll_table.h"

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

    using FilterSubmitCallback = std::function<void(const MultiTrackTable&)>;
    using SortSyncCallback     = std::function<void(const MultiTrackTable&)>;

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
                    const std::string&      friendly_name  = "",
                    const std::string&      no_data_text   = "",
                    std::optional<uint64_t> source_file_id = std::nullopt);

    ~MultiTrackTable();

    void Render() override;
    void Update() override;

    // Draws the title row and the table body inside one card. Requires a header
    // renderer; pass a zero size to fill the available region.
    void RenderCard(const ImVec2& size);

    // Filter form driving an A/B pair. Submitting it fetches this table and
    // hands the same options to the other one through the submit callback.
    // column_names is the query text; column_labels is what the combo shows.
    void RenderSharedFilterControls(const std::vector<std::string>& column_names,
                                    const std::vector<std::string>& column_labels);
    void ApplySharedFiltersFrom(const MultiTrackTable& source);

    // Last ungrouped header's group-by candidates. Empty until the first
    // ungrouped fetch. Compare mode unions this with the peer table.
    const std::vector<std::string>& EligibleGroupByColumns() const;
    void SetFilterSubmitCallback(const FilterSubmitCallback& callback);

    // Adopts the peer table's sort; a no-op when it already matches, which breaks the echo.
    void ApplySharedSortFrom(const MultiTrackTable& source);
    void SetSortSyncCallback(const SortSyncCallback& callback);

    void SetDisplaySummary(bool display);
    // Draws the title row of the card, see RenderCard.
    void SetHeaderRenderer(std::function<void()> renderer);

    uint64_t GetTotalRowCount() const;
    size_t   GetIncludedTrackCount() const;

    virtual void HandleTrackSelectionChanged(uint64_t track_id, bool selected);
    virtual void HandleTimeRangeSelectionChanged(double start_ns, double end_ns);

protected:
    virtual bool IncludeTrack(uint64_t track_id) const;
    virtual void UpdateFetchParams(std::shared_ptr<TableRequestParams>& params) const override;
    void         FormatData() const override;
    void         IndexColumns() override;
    void         RowSelected(const ImGuiMouseButton mouse_button) override;
    void         OnSortChanged() override;
    void         AdjustFilterForRequest(FilterOptions& filter) const override;

    // Subset of selected tracks applicable to this table type
    std::vector<uint64_t> m_included_tracks;

    // Set in compare mode: only tracks from this source feed the table.
    std::optional<uint64_t> m_source_file_id;

private:
    void FetchSelectionData();
    void ApplyFilter(bool reset);
    void SubmitFilters();
    // Moves the staged group-by and filter into the applied options, keeping the
    // two from being sent together.
    void CommitAdvancedFilter();
    void RebuildEligibleGroupByColumns();
    bool HasEligibleGroupByColumn(const std::string& name) const;

    bool                  m_display_summary;
    FilterSubmitCallback  m_filter_submit_callback;
    SortSyncCallback      m_sort_sync_callback;
    std::function<void()> m_header_renderer;

    std::vector<std::string> m_group_by_choices;
    std::vector<const char*> m_group_by_choices_ptr;
    int                      m_group_by_selection_index;
    // Ungrouped header columns that can be sent as group_by. Kept across
    // grouped fetches so a later request still knows the original schema.
    std::vector<std::string> m_eligible_group_by_columns;

    int           m_available_filter_modes;
    FilterMode    m_filter_mode;
    FilterOptions m_filter_advanced;
    std::string   m_filter_store;
};

}  // namespace View
}  // namespace RocProfVis
