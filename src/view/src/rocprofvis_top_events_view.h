// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

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
                       std::optional<uint64_t> source_index = std::nullopt);
        ~TopEventsTable();

        // Pooled render: own collapsing header and row-based sizing.
        void Render() override;
        // Compare render: sized card body only; the parent draws the category header.
        void RenderBody(const ImVec2& size);
        // Unclamped height to show all rows plus chrome.
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
        std::optional<uint64_t>                 m_source_index;
    };

    struct Category
    {
        std::unique_ptr<TopEventsTable> table_a;
        std::unique_ptr<TopEventsTable> table_b;  // compare mode only
        const char*                     header = nullptr;
    };

    void RenderSourceBadge(size_t source_index);

    DataProvider&           m_data_provider;
    bool                    m_compare_mode;
    std::array<Category, 5> m_categories;
};

}  // namespace View
}  // namespace RocProfVis
