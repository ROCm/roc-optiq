// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "imgui.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_events.h"
#include "rocprofvis_settings.h"
#include "widgets/rocprofvis_widget.h"
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

class InfiniteScrollTable : public RocWidget
{
public:
    InfiniteScrollTable(DataProvider& dp, TableType table_type = TableType::kEventTable);

    void Update() override;
    void Render() override;
    void SetTableType(TableType type) { m_table_type = type; }

    void HandleTrackSelectionChanged(std::shared_ptr<TrackSelectionChangedEvent> e);

private:

    void RenderLoadingIndicator();

    std::vector<char> m_filter;

    TableType m_table_type;  // Type of table (e.g., EventTable, SampleTable)
    rocprofvis_controller_table_type_t m_req_table_type;

    DataProvider& m_data_provider;
    Settings&     m_settings;

    int m_fetch_chunk_size;
    int m_fetch_pad_items;
    int m_fetch_threshold_items;

    // Internal state flags below
    bool     m_skip_data_fetch;
    uint64_t m_last_total_row_count;

    ImVec2 m_last_table_size;
    std::shared_ptr<TrackSelectionChangedEvent> m_track_selection_event_to_handle;
    
    // Keep track of currently selected tracks for this table type
    std::vector<uint64_t> m_selected_tracks;

};

}  // namespace View
}  // namespace RocProfVis
