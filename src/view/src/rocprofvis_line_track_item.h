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

class LineTrackItem;
class TimePixelTransform;
class TimelineSelection;

struct HighlightYRange
{
    float max_limit;
    float min_limit;
};

class LineTrackProjectSettings : public ProjectSetting
{
public:
    LineTrackProjectSettings(const std::string& project_id, LineTrackItem& track_item);
    ~LineTrackProjectSettings() override;
    void            ToJson() override;
    bool            Valid() const override;
    bool            BoxPlot() const;
    bool            BoxPlotStripes() const;
    bool            Highlight() const;
    HighlightYRange HighlightRange() const;
    std::array<bool, AnalysisTrackStatistics::Counter::kCounterCount> ShowAnalysis() const;

private:
    LineTrackItem& m_track_item;
};

class LineTrackItem : public TrackItem
{
    friend LineTrackProjectSettings;

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
        std::string m_units;

        EditableTextField m_text_field;
    };

public:
    LineTrackItem(DataProvider& dp, uint64_t track_id,
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
    virtual void RenderMetaAreaOptions() override;

private:
    void   UpdateMetadata();
    ImVec2 MapToUI(double x, double y, ImVec2& c_position, ImVec2& c_size,
                   double scale_y);
    bool   ExtractPointsFromData();
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

    LineTrackProjectSettings m_linetrack_project_settings;
    std::array<bool, AnalysisTrackStatistics::Counter::kCounterCount> m_show_analysis;
    bool                                                              m_highlight_y_range;
    HighlightYRange m_highlight_y_limits;
    bool            m_show_boxplot;
    bool            m_show_boxplot_stripes;
};

}  // namespace View
}  // namespace RocProfVis
