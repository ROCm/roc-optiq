// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_minimap.h"
#include "imgui.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_events.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_timeline_view.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace RocProfVis
{
namespace View
{

Minimap::Minimap(DataProvider& dp, TimelineView* tv)
: m_data_width(0)
, m_data_height(0)
, m_data_valid(false)
, m_raw_min_value(0.0)
, m_raw_max_value(0.0)
, m_data_provider(dp)
, m_timeline_view(tv)
{
    m_downsampled_data.resize(MINIMAP_SIZE * MINIMAP_SIZE, 0.0);
}
void
Minimap::UpdateData()
{
    const auto& map    = m_data_provider.GetMiniMap();
    auto        tracks = m_data_provider.GetTrackInfoList();
    if(map.empty() || tracks.empty()) return;

    size_t w = 0;
    for(const auto& [id, val] : map)
    {
        const auto& [vec, valid] = val;
        if(valid) w = std::max(w, vec.size());
    }

    if(w == 0) return;

    std::vector<double> data;
    data.reserve(tracks.size() * w);

    for(const auto* t : tracks)
    {
        auto                       it      = map.find(t->id);
        const std::vector<double>* vec_ptr = nullptr;

        if(it != map.end())
        {
            const auto& [vec, valid] = it->second;
            if(valid) vec_ptr = &vec;
        }

        if(vec_ptr)
        {
            data.insert(data.end(), vec_ptr->begin(), vec_ptr->end());
            data.resize(data.size() + (w - vec_ptr->size()), 0.0);
        }
        else
        {
            data.resize(data.size() + w, 0.0);
        }
    }

    SetData(data, w, tracks.size());
}
void
Minimap::SetData(const std::vector<double>& data, size_t width, size_t height)
{
    if(data.empty() || width == 0 || height == 0)
    {
        m_data_valid = false;
        return;
    }
    BinData(data, width, height);
    NormalizeRawData();
    m_data_valid = true;
}

void
Minimap::BinData(const std::vector<double>& input, size_t w, size_t h)
{
    if(input.size() < w * h)
    {
        m_data_valid = false;
        return;
    }

    m_data_width  = std::min(w, MINIMAP_SIZE);
    m_data_height = std::min(h, MINIMAP_SIZE);
    m_downsampled_data.assign(m_data_width * m_data_height, 0.0);

    double sx = (double) w / m_data_width;
    double sy = (double) h / m_data_height;

    for(size_t y = 0; y < m_data_height; ++y)
    {
        for(size_t x = 0; x < m_data_width; ++x)
        {
            double sum   = 0.0;
            int    count = 0;
            size_t sy0 = (size_t) (y * sy), sy1 = std::min((size_t) ((y + 1) * sy), h);
            size_t sx0 = (size_t) (x * sx), sx1 = std::min((size_t) ((x + 1) * sx), w);

            for(size_t iy = sy0; iy < sy1; ++iy)
                for(size_t ix = sx0; ix < sx1; ++ix)
                {
                    sum += input[iy * w + ix];
                    count++;
                }
            if(count > 0) m_downsampled_data[y * m_data_width + x] = sum / count;
        }
    }
}

void
Minimap::NormalizeRawData()
{
    if(m_downsampled_data.empty()) return;

    m_raw_min_value = std::numeric_limits<double>::max();
    m_raw_max_value = std::numeric_limits<double>::lowest();
    for(double v : m_downsampled_data)
        if(v != 0)
        {
            m_raw_min_value = std::min(m_raw_min_value, v);
            m_raw_max_value = std::max(m_raw_max_value, v);
        }

    double range = m_raw_max_value - m_raw_min_value;
    if(range > 0)
        for(double& v : m_downsampled_data)
            if(v != 0) v = (v - m_raw_min_value) / range;
}

ImU32
Minimap::GetColor(double v) const
{
    if(v == 0)
        return SettingsManager::GetInstance()
                       .GetUserSettings()
                       .display_settings.use_dark_mode
                   ? IM_COL32(0, 0, 0, 255)
                   : IM_COL32(255, 255, 255, 255);
    return SettingsManager::GetInstance().GetMinimapColor(
        std::sqrt(std::clamp(v, 0.0, 1.0)));
}

void
Minimap::Render()
{
    if(!m_data_valid || m_downsampled_data.empty()) return;

    SettingsManager& sm = SettingsManager::GetInstance();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, sm.GetColor(Colors::kBgPanel));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);

    float pad = 8.0f, legend_w = 60.0f,
          title_h = ImGui::GetTextLineHeightWithSpacing() + pad * 2;
    ImVec2 avail  = ImGui::GetContentRegionAvail();

    if(ImGui::BeginChild("Minimap", avail, true))
    {
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2      pos = ImGui::GetWindowPos();

        ImGui::SetCursorPos(ImVec2(pad, pad));
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(sm.GetColor(Colors::kTextMain)),
                           "Mini Map");

        ImVec2 map_pos(pos.x + pad, pos.y + title_h);
        ImVec2 map_size(avail.x - legend_w - pad * 3, avail.y - title_h - pad * 2);

        if(map_size.x > 0 && map_size.y > 0)
        {
            float bw = map_size.x / m_data_width, bh = map_size.y / m_data_height;
            for(size_t y = 0; y < m_data_height; ++y)
                for(size_t x = 0; x < m_data_width; ++x)
                    if(double v = m_downsampled_data[y * m_data_width + x])
                        dl->AddRectFilled(
                            ImVec2(map_pos.x + x * bw, map_pos.y + y * bh),
                            ImVec2(map_pos.x + x * bw + bw, map_pos.y + y * bh + bh),
                            GetColor(v));

            if(m_timeline_view)
            {
                ViewCoords vc = m_timeline_view->GetViewCoords();
                double     t0 = m_data_provider.GetStartTime();
                double     tr = m_data_provider.GetEndTime() - t0;

                ImGui::SetCursorScreenPos(map_pos);
                if(ImGui::InvisibleButton("Hit", map_size) && tr > 0)
                {
                    ImVec2 m     = ImGui::GetMousePos();
                    double bin_t = tr / m_data_width;
                    size_t idx   = std::clamp(
                        (size_t) ((m.x - map_pos.x) / map_size.x * m_data_width),
                        (size_t) 0, m_data_width - 1);
                    double v_min = t0 + idx * bin_t;
                    float  th    = vc.y + m_timeline_view->GetGraphSize().y;
                    EventManager::GetInstance()->AddEvent(
                        std::make_shared<NavigationEvent>(
                            v_min, v_min + bin_t, ((m.y - map_pos.y) / map_size.y) * th,
                            false));
                }
            }
        }
        ImGui::SetCursorPos(ImVec2(avail.x - legend_w - pad, title_h));
        RenderLegend(legend_w, avail.y - title_h - pad * 2);
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void
Minimap::RenderLegend(float w, float h)
{
    if(h <= 0 || w <= 0) return;
    SettingsManager& sm  = SettingsManager::GetInstance();
    ImDrawList*      dl  = ImGui::GetWindowDrawList();
    ImVec2           pos = ImGui::GetCursorScreenPos();

    float bar_w = 15.0f, txt_h = ImGui::CalcTextSize("1.0").y, gap = 4.0f;
    float bar_h = h - (txt_h * 2) - (gap * 2), bar_x = pos.x + (w - bar_w) * 0.5f,
          bar_y = pos.y + txt_h + gap;

    for(int i = 0; i < 100; ++i)
    {
        float t = i / 100.0f, y0 = bar_y + bar_h - (i * (bar_h / 100.0f)),
              y1 = y0 - (bar_h / 100.0f);
        dl->AddRectFilled(ImVec2(bar_x, y1), ImVec2(bar_x + bar_w, y0),
                          sm.GetMinimapColor(std::sqrt(t)));
    }
    dl->AddRect(ImVec2(bar_x, bar_y), ImVec2(bar_x + bar_w, bar_y + bar_h),
                sm.GetColor(Colors::kBorderColor));

    ImFont* font = sm.GetFontManager().GetFont(FontType::kSmall);
    dl->AddText(font, font->FontSize,
                ImVec2(bar_x + (bar_w - ImGui::CalcTextSize("1.0").x) * 0.5f, pos.y),
                sm.GetColor(Colors::kTextMain), "1.0");
    dl->AddText(font, font->FontSize,
                ImVec2(bar_x + (bar_w - ImGui::CalcTextSize("0.0").x) * 0.5f,
                       bar_y + bar_h + gap),
                sm.GetColor(Colors::kTextMain), "0.0");
}

}  // namespace View
}  // namespace RocProfVis