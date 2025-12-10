// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_types.h"
#include "rocprofvis_raw_track_data.h"
#include "rocprofvis_track_item.h"
#include "rocprofvis_view_structs.h"
#include "widgets/rocprofvis_widget.h"
#include "rocprofvis_time_to_pixel.h"
#include <memory>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

class LineTrackItem;
class TimePixelTransform;

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
    LineTrackItem(DataProvider& dp, uint64_t id, std::string name,
                  float max_meta_area_width, std::shared_ptr<TimePixelTransform> time_to_pixel_manager);
    ~LineTrackItem();

    bool          ReleaseData() override;
    virtual float CalculateNewMetaAreaSize() override;

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

    std::vector<rocprofvis_trace_counter_t> m_data;
    HighlightYRange                         m_highlight_y_limits;

    VerticalLimits           m_min_y;
    VerticalLimits           m_max_y;
    std::string              m_units;
    bool                     m_highlight_y_range;
    DataProvider&            m_dp;
    bool                     m_show_boxplot;
    bool                     m_show_boxplot_stripes;
    LineTrackProjectSettings m_linetrack_project_settings;
    float                    m_vertical_padding;
    std::shared_ptr<TimePixelTransform> m_tpt;
};

}  // namespace View
}  // namespace RocProfVis
