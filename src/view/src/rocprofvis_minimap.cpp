// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_minimap.h"
#include "imgui.h"
#include "rocprofvis_controller_enums.h"
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
, m_show_events(true)
, m_show_counters(true)
, m_raw_min_value(0.0)
, m_raw_max_value(0.0)
, m_data_provider(dp)
, m_timeline_view(tv)
, m_event_global_max(0.0)
, m_last_normalize_global(true)
{
    UpdateColorCache();
    m_event_global_max =
        m_data_provider.DataModel().GetTimeline().GetHistogramMaxValueGlobal();
    m_last_normalize_global =
        m_data_provider.DataModel().GetTimeline().IsNormalizeGlobal();
}
void
Minimap::UpdateColorCache()
{
    SettingsManager& sm = SettingsManager::GetInstance();
    m_event_color_bins  = {
        sm.GetColor(Colors::kMinimapBin1), sm.GetColor(Colors::kMinimapBin2),
        sm.GetColor(Colors::kMinimapBin3), sm.GetColor(Colors::kMinimapBin4),
        sm.GetColor(Colors::kMinimapBin5), sm.GetColor(Colors::kMinimapBin6),
        sm.GetColor(Colors::kMinimapBin7)
    };
    m_counter_color_bins = { sm.GetColor(Colors::kMinimapBinCounter1),
                             sm.GetColor(Colors::kMinimapBinCounter2),
                             sm.GetColor(Colors::kMinimapBinCounter3),
                             sm.GetColor(Colors::kMinimapBinCounter4),
                             sm.GetColor(Colors::kMinimapBinCounter5),
                             sm.GetColor(Colors::kMinimapBinCounter6),
                             sm.GetColor(Colors::kMinimapBinCounter7) };
}

void
Minimap::UpdateData()
{
    const auto& map    = m_data_provider.DataModel().GetTimeline().GetMiniMap();
    auto        tracks = m_data_provider.DataModel().GetTimeline().GetTrackList();
    if(map.empty() || tracks.empty()) return;

    size_t width = std::get<0>(map.begin()->second).size();

    if(width == 0) return;

    size_t height = tracks.size();

    // Make copy needed to modify and render data without ruining source.
    std::vector<std::vector<double>> data(height, std::vector<double>(width, 0.0));

    for(size_t i = 0; i < tracks.size(); ++i)
    {
        const auto* t  = tracks[i];
        auto        it = map.find(t->id);
        if(it != map.end() && std::get<1>(it->second))
        {
            const auto& vec = std::get<0>(it->second);
            // Copy data to the corresponding row
            std::copy(vec.begin(), vec.end(), data[i].begin());

            // Normalize sample tracks locally for minimap visualization
            if(t->track_type == kRPVControllerTrackTypeSamples &&
               t->max_value > t->min_value)
            {
                double range = t->max_value - t->min_value;
                for(double& val : data[i])
                {
                    val = (val - t->min_value) / range;
                }
            }
        }
    }

    SetData(data);
}
void
Minimap::SetData(const std::vector<std::vector<double>>& data)
{
    if(data.empty() || data.front().empty())
    {
        m_data_valid = false;
        return;
    }

    const size_t height = data.size();
    const size_t width  = data.front().size();
    for(const auto& row : data)
    {
        if(row.size() != width)
        {
            m_data_valid = false;
            return;
        }
    }

    BinData(data);
    NormalizeRawData();
    m_data_valid = true;
}

void
Minimap::BinData(const std::vector<std::vector<double>>& input)
{
    if(input.empty() || input.front().empty())
    {
        m_data_valid = false;
        return;
    }

    const size_t h = input.size();
    const size_t w = input.front().size();
    for(const auto& row : input)
    {
        if(row.size() != w)
        {
            m_data_valid = false;
            return;
        }
    }

    m_data_width  = std::min(w, MINIMAP_SIZE);
    m_data_height = std::min(h, MINIMAP_SIZE);
    m_downsampled_data.assign(m_data_height, std::vector<double>(m_data_width, 0.0));

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
                    sum += input[iy][ix];
                    count++;
                }
            if(count > 0) m_downsampled_data[y][x] = sum / count;
        }
    }
}

void
Minimap::NormalizeRawData()
{
    if(m_downsampled_data.empty() || m_downsampled_data.front().empty()) return;

    // Ensure tracks and downsampled data sizes match to avoid out-of-bounds access
    auto tracks = m_data_provider.DataModel().GetTimeline().GetTrackList();
    if(tracks.size() != m_downsampled_data.size())
    {
        return;
    }

    m_raw_min_value = std::numeric_limits<double>::max();
    m_raw_max_value = std::numeric_limits<double>::lowest();
    for(size_t i = 0; i < m_downsampled_data.size(); ++i)
    {
        if(tracks[i]->track_type == kRPVControllerTrackTypeEvents)
        {
            for(double v : m_downsampled_data[i])
            {
                if(v != 0)
                {
                    m_raw_min_value = std::min(m_raw_min_value, v);
                    m_raw_max_value = std::max(m_raw_max_value, v);
                }
            }
        }
    }

    m_event_global_max =
        m_data_provider.DataModel().GetTimeline().GetHistogramMaxValueGlobal();
    
    double global_max = m_data_provider.DataModel().GetTimeline().GetMinimapGlobalMax();
    double global_min = m_data_provider.DataModel().GetTimeline().GetMinimapGlobalMin();

    bool   is_global = m_data_provider.DataModel().GetTimeline().IsNormalizeGlobal();
    
    double range;
    double min_val;

    if(is_global)
    {
        range   = global_max - global_min;
        min_val = global_min; 
    }
    else
    {
        range   = m_raw_max_value - m_raw_min_value;
        min_val = m_raw_min_value;
    }

    std::cout << m_event_global_max << "\n";
    double x =  m_raw_max_value - m_raw_min_value;
    int    count  = 0;

    for(auto& row : m_downsampled_data)
    {
        for(double& v : row)
        {
            if(tracks[count]->track_type == kRPVControllerTrackTypeEvents)
            {
                if(v != 0)
                {
                    // Bin into 1-7
                    int bin = 7;
                    if(range > 0)
                    {
                        double t = (v - min_val) / range;
                        bin      = 1 + static_cast<int>(t * 6.999);
                    }
                    v = static_cast<double>(bin);
                }
            }
            else
            {
                if(v != 0)
                {
                    int bin = 1 + static_cast<int>(v * 6.999);
                    v       = static_cast<double>(bin);
                }
            }
        }
        count++;
    }
}

ImU32
Minimap::GetColor(double v, rocprofvis_controller_track_type_t type) const
{
    SettingsManager& sm = SettingsManager::GetInstance();
    if(type == kRPVControllerTrackTypeSamples && !m_show_counters)
        return sm.GetColor(Colors::kMinimapBg);
    if(type == kRPVControllerTrackTypeEvents && !m_show_events)
        return sm.GetColor(Colors::kMinimapBg);

    int bin = static_cast<int>(v);
    bin     = std::clamp(bin, 1, 7);

    if(type == kRPVControllerTrackTypeSamples)
    {
        return m_counter_color_bins[bin - 1];
    }
    else
    {
        return m_event_color_bins[bin - 1];
    }
}

void
Minimap::Render()
{
    bool current_normalize_global =
        m_data_provider.DataModel().GetTimeline().IsNormalizeGlobal();
    if(m_last_normalize_global != current_normalize_global)
    {
        m_last_normalize_global = current_normalize_global;
        UpdateData();
    }

    SettingsManager& sm = SettingsManager::GetInstance();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, sm.GetColor(Colors::kBgPanel));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);

    float  pad         = 8.0f;
    float  legend_w    = 120.0f;
    float  top_padding = 5.0f;
    ImVec2 avail       = ImGui::GetContentRegionAvail();

    if(ImGui::BeginChild("Minimap", avail, true))
    {
        ImDrawList* draw_list       = ImGui::GetWindowDrawList();
        ImVec2      window_position = ImGui::GetWindowPos();

        ImGui::SetCursorPos(ImVec2(pad, pad));

        ImVec2 map_pos(window_position.x + pad, window_position.y + top_padding);
        ImVec2 map_size(avail.x - legend_w - pad * 3, avail.y - top_padding - pad * 2);

        // Fill background of minimap area with white (light mode) or black (dark mode)
        draw_list->AddRectFilled(map_pos,
                                 ImVec2(map_pos.x + map_size.x, map_pos.y + map_size.y),
                                 sm.GetColor(Colors::kMinimapBg));

        RenderMinimapData(draw_list, map_pos, map_size);

        // Draw border around minimap
        ImU32 border_color     = sm.GetColor(Colors::kBorderColor);
        float border_thickness = 1.5f;
        draw_list->AddRect(map_pos,
                           ImVec2(map_pos.x + map_size.x, map_pos.y + map_size.y),
                           border_color, 0.0f, 0, border_thickness);

        RenderViewport(draw_list, map_pos, map_size);
        HandleNavigation(map_pos, map_size);

        ImGui::SetCursorPos(ImVec2(avail.x - legend_w - pad, top_padding));
        RenderLegend(legend_w, avail.y - top_padding - pad * 2);
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void
Minimap::RenderMinimapData(ImDrawList* dl, ImVec2 map_pos, ImVec2 map_size)
{
    auto tracks = m_data_provider.DataModel().GetTimeline().GetTrackList();

    float bw = map_size.x / m_data_width, bh = map_size.y / m_data_height;
    for(size_t y = 0; y < m_data_height; ++y)
        for(size_t x = 0; x < m_data_width; ++x)
            if(double v = m_downsampled_data[y][x])
            {
                if(tracks[y]->track_type == kRPVControllerTrackTypeSamples)
                {
                    dl->AddRectFilled(
                        ImVec2(map_pos.x + x * bw, map_pos.y + y * bh),
                        ImVec2(map_pos.x + x * bw + bw, map_pos.y + y * bh + bh),
                        GetColor(v, kRPVControllerTrackTypeSamples));
                }
                else
                {
                    dl->AddRectFilled(
                        ImVec2(map_pos.x + x * bw, map_pos.y + y * bh),
                        ImVec2(map_pos.x + x * bw + bw, map_pos.y + y * bh + bh),
                        GetColor(v, kRPVControllerTrackTypeEvents));
                }
            }
}

void
Minimap::RenderViewport(ImDrawList* drawlist, ImVec2 map_pos, ImVec2 map_size)
{
    SettingsManager& settings    = SettingsManager::GetInstance();
    ViewCoords       view_coords = m_timeline_view->GetViewCoords();
    double start_time = m_data_provider.DataModel().GetTimeline().GetStartTime();
    double duration = m_data_provider.DataModel().GetTimeline().GetEndTime() - start_time;

    float start_ratio = (view_coords.v_min_x - start_time) / duration;
    float end_ratio   = (view_coords.v_max_x - start_time) / duration;
    float x_start     = map_pos.x + std::clamp(start_ratio, 0.0f, 1.0f) * map_size.x;
    float x_end       = map_pos.x + std::clamp(end_ratio, 0.0f, 1.0f) * map_size.x;

    float total_h = m_timeline_view->GetTotalTrackHeight();
    float view_h  = m_timeline_view->GetGraphSize().y;

    float y_start_ratio = 0.0f;
    float y_end_ratio   = 1.0f;

    if(total_h > 0)
    {
        y_start_ratio = view_coords.y / total_h;
        y_end_ratio   = (view_coords.y + view_h) / total_h;
    }

    float y_start = map_pos.y + std::clamp(y_start_ratio, 0.0f, 1.0f) * map_size.y;
    float y_end   = map_pos.y + std::clamp(y_end_ratio, 0.0f, 1.0f) * map_size.y;

    ImU32 border_col       = settings.GetColor(Colors::kTextMain);
    float border_thickness = 1.0f;
    float corner_len       = 10.0f;

    // Ensure corners don't overlap if viewport is tiny
    float w = x_end - x_start;
    float h = y_end - y_start;
    if(corner_len * 2 > w) corner_len = w * 0.5f;
    if(corner_len * 2 > h) corner_len = h * 0.5f;

    // Top-Left
    drawlist->AddLine(ImVec2(x_start, y_start), ImVec2(x_start + corner_len, y_start),
                      border_col, border_thickness);
    drawlist->AddLine(ImVec2(x_start, y_start), ImVec2(x_start, y_start + corner_len),
                      border_col, border_thickness);

    // Top-Right
    drawlist->AddLine(ImVec2(x_end, y_start), ImVec2(x_end - corner_len, y_start),
                      border_col, border_thickness);
    drawlist->AddLine(ImVec2(x_end, y_start), ImVec2(x_end, y_start + corner_len),
                      border_col, border_thickness);

    // Bottom-Left
    drawlist->AddLine(ImVec2(x_start, y_end), ImVec2(x_start + corner_len, y_end),
                      border_col, border_thickness);
    drawlist->AddLine(ImVec2(x_start, y_end), ImVec2(x_start, y_end - corner_len),
                      border_col, border_thickness);

    // Bottom-Right
    drawlist->AddLine(ImVec2(x_end, y_end), ImVec2(x_end - corner_len, y_end), border_col,
                      border_thickness);
    drawlist->AddLine(ImVec2(x_end, y_end), ImVec2(x_end, y_end - corner_len), border_col,
                      border_thickness);

    // Center Crosshair
    float cx           = (x_start + x_end) * 0.5f;
    float cy           = (y_start + y_end) * 0.5f;
    float crosshair_sz = 6.0f;  // Size of the cross

    drawlist->AddLine(ImVec2(cx - crosshair_sz, cy), ImVec2(cx + crosshair_sz, cy),
                      border_col, 1.0f);
    drawlist->AddLine(ImVec2(cx, cy - crosshair_sz), ImVec2(cx, cy + crosshair_sz),
                      border_col, 1.0f);
}

void
Minimap::HandleNavigation(ImVec2 map_pos, ImVec2 map_size)
{
    ViewCoords view_coords = m_timeline_view->GetViewCoords();
    double     start_time  = m_data_provider.DataModel().GetTimeline().GetStartTime();
    double end_time = m_data_provider.DataModel().GetTimeline().GetEndTime() - start_time;

    ImGui::SetCursorScreenPos(map_pos);
    ImGui::InvisibleButton("Hit", map_size);

    if(ImGui::IsItemClicked())
    {
        ImVec2 m = ImGui::GetMousePos();

        // Calculate time at mouse click
        float  mouse_ratio_x = std::clamp((m.x - map_pos.x) / map_size.x, 0.0f, 1.0f);
        double time_at_mouse = start_time + mouse_ratio_x * end_time;

        // Calculate Y position at mouse click
        float total_h = m_timeline_view->GetTotalTrackHeight();
        if(total_h == 0) total_h = m_timeline_view->GetGraphSize().y;

        float  mouse_ratio_y = std::clamp((m.y - map_pos.y) / map_size.y, 0.0f, 1.0f);
        double y_at_mouse    = mouse_ratio_y * total_h;

        // Get current view width (zoom level)
        double current_width = view_coords.v_max_x - view_coords.v_min_x;

        // Center the view on the clicked time/position
        double new_start = time_at_mouse - current_width * 0.5;
        double new_end   = time_at_mouse + current_width * 0.5;

        EventManager::GetInstance()->AddEvent(
            std::make_shared<NavigationEvent>(new_start, new_end, y_at_mouse,
                                              true));  // Center = true
    }
}

void
Minimap::RenderLegend(float w, float h)
{
    if(h <= 0 || w <= 0) return;
    SettingsManager& sm  = SettingsManager::GetInstance();
    ImDrawList*      dl  = ImGui::GetWindowDrawList();
    ImVec2           pos = ImGui::GetCursorScreenPos();

    float bar_w       = 10.0f;
    float text_height = ImGui::CalcTextSize("1.0").y;
    float checkbox_sz = ImGui::GetFrameHeight();
    float gap         = 4.0f;
    float bar_h       = h - (text_height * 2) - (gap * 3) - checkbox_sz;

    float bar_x1 = pos.x + (w * 0.25f) - (bar_w * 0.5f);
    float bar_x2 = pos.x + (w * 0.75f) - (bar_w * 0.5f);
    float bar_y  = pos.y + text_height + gap;

    float bin_h = bar_h / 7.0f;

    auto DimColor = [](ImU32 col) -> ImU32 {
        ImVec4 c = ImGui::ColorConvertU32ToFloat4(col);
        c.w *= 0.2f;
        return ImGui::ColorConvertFloat4ToU32(c);
    };

    for(int i = 0; i < 7; ++i)
    {
        // 0 is bottom (low value), 6 is top (high value)
        float y0  = bar_y + bar_h - (i * bin_h);
        float y1  = y0 - bin_h;
        ImU32 col = m_event_color_bins[i];
        if(!m_show_events) col = DimColor(col);
        dl->AddRectFilled(ImVec2(bar_x1, y1), ImVec2(bar_x1 + bar_w, y0), col);

        ImU32 col2 = m_counter_color_bins[i];
        if(!m_show_counters) col2 = DimColor(col2);
        dl->AddRectFilled(ImVec2(bar_x2, y1), ImVec2(bar_x2 + bar_w, y0), col2);
    }

    ImU32 border_col = sm.GetColor(Colors::kBorderColor);
    dl->AddRect(ImVec2(bar_x1, bar_y), ImVec2(bar_x1 + bar_w, bar_y + bar_h),
                !m_show_events ? DimColor(border_col) : border_col);
    dl->AddRect(ImVec2(bar_x2, bar_y), ImVec2(bar_x2 + bar_w, bar_y + bar_h),
                !m_show_counters ? DimColor(border_col) : border_col);

    ImFont* font = sm.GetFontManager().GetFont(FontType::kSmall);
    float   cx   = pos.x + w * 0.5f;

    const char* txt_max = "Max";
    ImVec2      sz_max  = ImGui::CalcTextSize(txt_max);
    dl->AddText(font, font->FontSize, ImVec2(cx - sz_max.x * 0.5f, pos.y),
                sm.GetColor(Colors::kTextMain), txt_max);

    const char* txt_min = "Min";
    ImVec2      sz_min  = ImGui::CalcTextSize(txt_min);
    dl->AddText(font, font->FontSize, ImVec2(cx - sz_min.x * 0.5f, bar_y + bar_h + gap),
                sm.GetColor(Colors::kTextMain), txt_min);

    // Checkboxes
    float chk_y = bar_y + bar_h + gap + text_height + gap;
    ImGui::SetCursorScreenPos(ImVec2(bar_x1 - (checkbox_sz - bar_w) * 0.5f, chk_y));
    ImGui::Checkbox("##events", &m_show_events);
    if(ImGui::BeginItemTooltip())
    {
        ImGui::Text("Show/Hide Event Density");
        ImGui::EndTooltip();
    }
    ImGui::SetCursorScreenPos(ImVec2(bar_x2 - (checkbox_sz - bar_w) * 0.5f, chk_y));
    ImGui::Checkbox("##counters", &m_show_counters);
    if(ImGui::BeginItemTooltip())
    {
        ImGui::Text("Show/Hide Counter Density");
        ImGui::EndTooltip();
    }

    // Helper to draw rotated text
    auto DrawRotatedText = [&](const char* text, ImVec2 center, bool disabled) {
        ImU32 text_col = disabled ? DimColor(sm.GetColor(Colors::kTextMain))
                                  : sm.GetColor(Colors::kTextMain);

        ImVec2 tsz      = ImGui::CalcTextSize(text);
        ImVec2 draw_pos = ImVec2(center.x - tsz.x * 0.5f, center.y - tsz.y * 0.5f);

        int v_start = dl->VtxBuffer.Size;
        dl->AddText(font, font->FontSize, draw_pos, text_col, text);
        int v_end = dl->VtxBuffer.Size;

        // Rotate 90� CCW: (x,y) -> (y, -x) relative to center
        for(int i = v_start; i < v_end; ++i)
        {
            ImDrawVert& v  = dl->VtxBuffer[i];
            float       dx = v.pos.x - center.x;
            float       dy = v.pos.y - center.y;
            v.pos.x        = center.x + dy;
            v.pos.y        = center.y - dx;
        }
    };

    // Event Density (Left of bar 1)
    DrawRotatedText("Event Density", ImVec2(bar_x1 - gap * 3.0f, bar_y + bar_h * 0.5f),
                    !m_show_events);

    // Counter Density (Left of bar 2)
    DrawRotatedText("Counter Density", ImVec2(bar_x2 - gap * 3.0f, bar_y + bar_h * 0.5f),
                    !m_show_counters);
}

}  // namespace View
}  // namespace RocProfVis
