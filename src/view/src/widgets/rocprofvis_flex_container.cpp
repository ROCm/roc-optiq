// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_flex_container.h"
#include "rocprofvis_settings_manager.h"

#include <algorithm>
#include <imgui.h>

namespace RocProfVis
{
namespace View
{

// Helpers

struct FlexRow
{
    size_t start = 0;
    size_t count = 0;
    float  min_w = 0.0f;
    float  grow  = 0.0f;
};

// FlexContainer


void
FlexContainer::Render()
{
    if(items.empty()) return;

    float avail_width  = ImGui::GetContentRegionAvail().x;
    float avail_height = ImGui::GetContentRegionAvail().y;

    // ---- Step 1: bin items into rows ----

    std::vector<FlexRow> rows;
    rows.push_back({});

    for(size_t i = 0; i < items.size(); i++)
    {
        FlexRow& cur  = rows.back();
        float gap_w   = (cur.count > 0) ? gap : 0.0f;
        float needed  = cur.min_w + gap_w + items[i].min_width;

        if(cur.count > 0 && needed > avail_width)
            rows.push_back({i, 0, 0.0f, 0.0f});

        FlexRow& target = rows.back();
        if(target.count > 0) target.min_w += gap;
        target.min_w += items[i].min_width;
        target.grow  += items[i].flex_grow;
        target.count++;
    }

    // ---- Step 2: figure out default row height ----

    size_t num_rows      = rows.size();
    float  row_gap_total = gap * static_cast<float>(num_rows > 1 ? num_rows - 1 : 0);
    float  row_height    = std::max(avail_height, min_row_height);
    row_height           = (row_height - row_gap_total) / static_cast<float>(num_rows);

    // ---- Step 3: render ----

    for(size_t ri = 0; ri < num_rows; ri++)
    {
        FlexRow& row       = rows[ri];
        float    row_gaps  = gap * static_cast<float>(row.count > 1 ? row.count - 1 : 0);
        float    min_sum   = row.min_w - row_gaps;
        float    free      = std::max(0.0f, avail_width - min_sum - row_gaps);

        for(size_t ci = 0; ci < row.count; ci++)
        {
            FlexItem& item = items[row.start + ci];

            float w = item.min_width;
            if(row.grow > 0.0f)
                w += free * (item.flex_grow / row.grow);

            float h = (item.height > 0.0f) ? item.height : row_height;

            ImGui::PushStyleColor(ImGuiCol_ChildBg,
                SettingsManager::GetInstance().GetColor(Colors::kFillerColor));

            ImGui::BeginChild(ImGui::GetID(&item), ImVec2(w, h),
                              ImGuiChildFlags_Borders);
            if(item.widget) item.widget->Render();
            ImGui::EndChild();

            ImGui::PopStyleColor();

            if(ci + 1 < row.count) ImGui::SameLine(0.0f, gap);
        }

        if(ri + 1 < num_rows) ImGui::Dummy(ImVec2(0.0f, gap));
    }
}

}  // namespace View
}  // namespace RocProfVis
