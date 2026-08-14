// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_raw_track_data.h"
#include "rocprofvis_track_item.h"
#include "rocprofvis_time_to_pixel.h"
#include <memory>
#include "widgets/rocprofvis_editable_textfield.h"
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

class CounterTrackOptions;
class LineTrackItem;
class TimePixelTransform;
class TimelineSelection;
class TimelineTrackOptions;

class LineTrackItem : public TrackItem
{
    friend CounterTrackOptions;

    class VerticalLimits
    {
    public:
        VerticalLimits(std::string field_id);
        void Render();

        void Init(double value, std::string unit);

        double             Value() const;
        const std::string& StrValue() const;
        const std::string& CompactValue() const;
        float              ButtonSize() const;

    private:
        void        UpdateValue(double value);
        std::string FormatValue(double value);
        double      ProcessUserInput(std::string_view input);

        double      m_value;
        double      m_default_value;
        std::string m_formatted_default;

        std::string m_formatted_str;
        std::string m_compact_str;
        std::string m_edit_str;
        std::string m_units;

        EditableTextField m_text_field;
    };

public:
    LineTrackItem(DataProvider& dp, uint64_t track_id,
                  TimelineTrackOptions&               track_options,
                  std::shared_ptr<TimePixelTransform> time_to_pixel_manager,
                  std::shared_ptr<TimelineSelection>  timeline_selection);
    ~LineTrackItem();

    void         Update() override;
    bool         ReleaseData() override;
    virtual void UpdateMetaScaleAreaSize() override;
    virtual void UpdateMaxMetaScaleAreaSize() override;

protected:
    virtual void RenderMetaAreaScale() override;
    virtual void RenderChart(float graph_width) override;

private:
    void   UpdateMetadata();
    ImVec2 MapToUI(double x, double y, ImVec2& c_position, ImVec2& c_size,
                   double scale_y);
    float  CalculatePlotHeight() const;
    // Fills out_ticks with interior Y-axis values, count based on plot_height.
    void   GenerateYAxisTicks(float plot_height, std::vector<double>& out_ticks) const;
    // Refreshes m_grid_ticks only when the track height or Y range has changed.
    void   UpdateYAxisTicks();
    void   ExtractPointsFromData() override;
    float  CalculateMissingX(float x1, float y1, float x2, float y2, float known_y);
    void   BoxPlotRender(float graph_width);
    void   RenderHighlightBand(ImDrawList* draw_list, const ImVec2& cursor_position,
                               const ImVec2& content_size, double scale_y);

    std::vector<TraceCounter> m_data;

    VerticalLimits m_min_y;
    VerticalLimits m_max_y;
    std::string    m_units;

    DataProvider& m_dp;
    float         m_vertical_padding;

    std::array<Pill*, AnalysisTrackStatistics::Counter::kCounterCount> m_pills_analysis;
    // User configurable options. Underlying object is owned by TrackItem.
    // May be null; guard before use.
    CounterTrackOptions* m_counter_options;

    // Cached interior Y-axis tick values; see UpdateYAxisTicks().
    std::vector<double> m_grid_ticks;
    float               m_cached_ticks_height = -1.0f;
    double              m_cached_ticks_min    = 0.0;
    double              m_cached_ticks_max    = 0.0;
    float               m_cached_ticks_line_h = -1.0f;
};

}  // namespace View
}  // namespace RocProfVis
