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

    void HandleTrackSelectionChanged();

private:
    class TopEventsTable : public MultiTrackTable
    {
    public:
        TopEventsTable(DataProvider& dp, TableType table_type,
                       rocprofvis_controller_table_type_t request_table_type,
                       uint64_t                           request_id,
                       std::shared_ptr<TimelineSelection> timeline_selection);
        ~TopEventsTable();

    private:
        enum DurationColumns
        {
            kDurationTotal = 0,
            kDurationAvg,
            kDurationMin,
            kDurationMax,
            kNumDurationColumns
        };

        void IndexColumns() override;
        void FormatData() const override;
        void FilterSelectedTracksForTableType(
            const std::vector<uint64_t>& selected_track_ids,
            std::vector<uint64_t>&       filtered_track_ids) const override;

        std::array<size_t, kNumDurationColumns> m_duration_column_indices;
    };
    struct Section
    {
        const char*                     heading;
        std::unique_ptr<TopEventsTable> table;
    };

    std::array<Section, 5> m_sections;
    DataProvider&          m_data_provider;
};

}  // namespace View
}  // namespace RocProfVis
