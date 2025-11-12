// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller_types.h"
#include "rocprofvis_raw_track_data.h"
#include "rocprofvis_track_item.h"
#include "rocprofvis_view_structs.h"
#include "widgets/rocprofvis_widget.h"

#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

class LineTrackItem;

class LineTrackProjectSettings : public ProjectSetting
{
public:
    LineTrackProjectSettings(const std::string& project_id, LineTrackItem& track_item);
    ~LineTrackProjectSettings() override;
    void ToJson() override;
    bool Valid() const override;

    bool                        BoxPlot() const;
    bool                        Highlight() const;
    rocprofvis_color_by_value_t HighlightRange() const;

private:
    LineTrackItem& m_track_item;
};

class LineTrackItem : public TrackItem
{
    friend LineTrackProjectSettings;

    class VerticalLimits
    {
    public:
        VerticalLimits(double value, std::string field_id, std::string prefix);
        double             Value() const;
        const std::string& StrValue() const;
        const std::string& CompactValue() const;
        void               SetValue(double value);
        void               Render();
        float              ButtonSize() const;
        const std::string& Prefix();
    private:
        void UpdateValue(double value);
        std::string FormatValue(double value);
        double      ProcessUserInput(std::string_view input);

        double            m_value;
        double            m_default_value;
        std::string       m_formatted_default;

        std::string       m_formatted_str;
        std::string       m_compact_str;

        std::string       m_prefix;
        EditableTextField m_text_field;
    };

public:
    LineTrackItem(DataProvider& dp, uint64_t id, std::string name, float zoom,
                  double time_offset_ns, double& min_x, double& max_x, double scale_x,
                  float max_meta_area_width);
    ~LineTrackItem();

    bool ReleaseData() override;
    virtual float CalculateNewMetaAreaSize() override;

protected:
    virtual void  RenderMetaAreaScale() override;
    virtual void  RenderChart(float graph_width) override;
    virtual void  RenderMetaAreaOptions() override;

    void UpdateYScaleExtents();

private:
    ImVec2 MapToUI(rocprofvis_data_point_t& point, ImVec2& c_position, ImVec2& c_size,
                   double scale_x, float scale_y);
    bool   ExtractPointsFromData();
    float  CalculateMissingX(float x1, float y1, float x2, float y2, float known_y);
    void   LineTrackRender(float graph_width);
    void   BoxPlotRender(float graph_width);
    void   RenderTooltip(float tooltip_x, float tooltip_y);

    std::vector<rocprofvis_data_point_t> m_data;
    rocprofvis_color_by_value_t          m_color_by_value_digits;

    VerticalLimits m_min_y;
    VerticalLimits m_max_y;

    bool                                 m_is_color_value_existant;
    DataProvider&                        m_dp;
    bool                                 m_show_boxplot;
    LineTrackProjectSettings             m_project_settings;
};

}  // namespace View
}  // namespace RocProfVis
