// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_compare_panes.h"
#include "rocprofvis_multi_track_table.h"
#include "widgets/rocprofvis_widget.h"
#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace RocProfVis
{
namespace View
{

class DataProvider;
class TimelineSelection;

class TopEventsView : public RocWidget
{
public:
    TopEventsView(DataProvider&                      data_provider,
                  std::shared_ptr<TimelineSelection> timeline_selection);
    ~TopEventsView();

    void Update() override;
    void Render() override;

    void HandleTrackSelectionChanged(uint64_t track_id, bool selected);
    void HandleTimeRangeSelectionChanged(double start_ns, double end_ns);

private:
    class TopEventsTable : public MultiTrackTable
    {
    public:
        TopEventsTable(DataProvider& dp, TableType table_type,
                       rocprofvis_controller_table_type_t request_table_type,
                       uint64_t                           request_id,
                       std::shared_ptr<TimelineSelection> timeline_selection,
                       rocprofvis_dm_event_operation_t op, const char* header,
                       std::optional<uint64_t> source_file_id = std::nullopt);
        ~TopEventsTable();

        // Draws its own collapsing header, sized to the rows it holds.
        void Render() override;
        // Height that shows every row plus the table chrome, unclamped.
        float ContentHeight() const;

        void HandleTrackSelectionChanged(uint64_t track_id, bool selected) override;

        bool Visible() const;

    private:
        enum DurationColumns
        {
            kDurationTotal = 0,
            kDurationAvg,
            kDurationMin,
            kDurationMax,
            kNumDurationColumns
        };

        bool IncludeTrack(uint64_t track_id) const override;
        void IndexColumns() override;
        void FormatData() const override;

        size_t Rows() const;

        std::array<size_t, kNumDurationColumns> m_duration_column_indices;
        rocprofvis_dm_event_operation_t         m_op;
        const char*                             m_header;
        bool                                    m_visible;
    };

    // One event category, holding a single table or an A/B pair in compare mode.
    struct Category
    {
        std::array<std::unique_ptr<TopEventsTable>, COMPARE_SOURCE_COUNT> tables;
        const char* header = nullptr;
    };

    static constexpr size_t CATEGORY_COUNT = 5;

    static bool AnyVisible(const Category& category);

    // Draws one category header with the A/B cards of that category below it.
    void RenderCategory(Category& category);
    void RenderSourceTitle(size_t source_index);

    DataProvider&                        m_data_provider;
    bool                                 m_compare_mode;
    std::array<Category, CATEGORY_COUNT> m_categories;
};

}  // namespace View
}  // namespace RocProfVis
