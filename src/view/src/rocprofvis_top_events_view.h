// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_multi_track_table.h"
#include "widgets/rocprofvis_widget.h"
#include <array>
#include <memory>
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
                       rocprofvis_dm_event_operation_t op, const char* header);
        ~TopEventsTable();

        void Render() override;
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

    std::array<std::unique_ptr<TopEventsTable>, 5> m_tables;
};

}  // namespace View
}  // namespace RocProfVis
