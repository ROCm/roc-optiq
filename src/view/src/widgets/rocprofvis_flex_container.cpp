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

struct FlexRow
{
    size_t start = 0;
    size_t count = 0;
    float  min_w = 0.0f;
    float  grow  = 0.0f;
};

FlexItem*
FlexContainer::GetItem(const std::string& id)
{
    for(auto& item : items)
        if(item.id == id) return &item;
    return nullptr;
}

void
FlexContainer::Render()
{
    if(items.empty()) return;

    float avail_width = ImGui::GetContentRegionAvail().x;

    // bin items into rows

    std::vector<FlexRow> rows;
    rows.push_back({});

    for(size_t i = 0; i < items.size(); i++)
    {
        FlexRow& cur = rows.back();

        bool needs_new_row = items[i].full_row && cur.count > 0;
        if(!needs_new_row && cur.count > 0)
        {
            float gap_w  = gap;
            float needed = cur.min_w + gap_w + items[i].min_width;
            needs_new_row = needed > avail_width;
        }

        if(needs_new_row)
            rows.push_back({i, 0, 0.0f, 0.0f});

        FlexRow& target = rows.back();
        if(target.count > 0) target.min_w += gap;
        target.min_w += items[i].min_width;
        target.grow  += items[i].flex_grow;
        target.count++;

        if(items[i].full_row && i + 1 < items.size())
            rows.push_back({i + 1, 0, 0.0f, 0.0f});
    }

    size_t num_rows = rows.size();

    // render

    for(size_t row_index = 0; row_index < num_rows; row_index++)
    {
        FlexRow& row      = rows[row_index];
        float    row_gaps = gap * static_cast<float>(row.count > 1 ? row.count - 1 : 0);
        float    min_sum  = row.min_w - row_gaps;
        float    free     = std::max(0.0f, avail_width - min_sum - row_gaps);

        ImGui::PushID(static_cast<int>(row_index));
        ImGui::BeginChild("row", ImVec2(avail_width, 0),
                          ImGuiChildFlags_AutoResizeY,
                          ImGuiWindowFlags_NoScrollWithMouse);

        for(size_t column_index = 0; column_index < row.count; column_index++)
        {
            FlexItem& item = items[row.start + column_index];

            float w = item.min_width;
            if(row.grow > 0.0f)
                w += free * (item.flex_grow / row.grow);
            w = std::min(w, avail_width);

            ImGuiChildFlags item_flags = ImGuiChildFlags_None;
            float h = item.height;
            if(h <= 0.0f)
            {
                item_flags |= ImGuiChildFlags_AutoResizeY;
                h = 0.0f;
            }

            // Items are transparent; each widget paints its own card. In
            // subcomponent layout, draw a thin separator between siblings.
            SettingsManager& settings = SettingsManager::GetInstance();
            ImGui::PushStyleColor(ImGuiCol_ChildBg,
                                  settings.GetColor(Colors::kTransparent));

            ImGui::BeginChild(ImGui::GetID(&item), ImVec2(w, h), item_flags,
                              ImGuiWindowFlags_NoScrollWithMouse);
            if(item.widget) item.widget->Render();
            ImGui::EndChild();

            const ImVec2 item_min = ImGui::GetItemRectMin();
            const ImVec2 item_max = ImGui::GetItemRectMax();

            ImGui::PopStyleColor();

            if(subcomponent_layout && column_index + 1 < row.count)
            {
                const float sep_x = item_max.x + gap * 0.5f;
                const float pad   = 4.0f;
                ImGui::GetWindowDrawList()->AddLine(
                    ImVec2(sep_x, item_min.y + pad),
                    ImVec2(sep_x, item_max.y - pad),
                    settings.GetColor(Colors::kBorderColor));
            }

            if(column_index + 1 < row.count) ImGui::SameLine(0.0f, gap);
        }
        ImGui::EndChild();
        ImGui::PopID();

        if(row_index + 1 < num_rows) ImGui::Dummy(ImVec2(0.0f, gap));
    }
}

}  // namespace View
}  // namespace RocProfVis
