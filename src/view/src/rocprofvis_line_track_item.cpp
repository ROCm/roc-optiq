// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_line_track_item.h"
#include "rocprofvis_click_manager.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_timeline_track_options.h"
#include "rocprofvis_utils.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace RocProfVis
{
namespace View
{

constexpr float DEFAULT_VERTICAL_PADDING = 2.0f;
constexpr float DEFAULT_LINE_THICKNESS   = 1.0f;

constexpr float Y_AXIS_TICK_MARK_LENGTH     = 4.0f;
constexpr float Y_AXIS_TICK_LABEL_GAP       = 8.0f;
// Matches the vertical minor grid lines drawn by the timeline (see RenderGrid).
constexpr float Y_AXIS_GRID_LINE_ALPHA      = 0.10f;
constexpr float Y_AXIS_LABEL_SPACING_FACTOR = 2.5f;
// Interior ticks/labels/grid lines only show above this height.
constexpr float Y_AXIS_LABEL_MIN_TRACK_HEIGHT = 2.0f * DEFAULT_TRACK_HEIGHT;

LineTrackItem::LineTrackItem(DataProvider& dp, uint64_t track_id,
                             TimelineTrackOptions&               track_options,
                             std::shared_ptr<TimePixelTransform> tpt,
                             std::shared_ptr<TimelineSelection>  timeline_selection)
: TrackItem(dp, track_id, track_options, tpt, timeline_selection)
, m_data({})
, m_min_y("edit_min")
, m_max_y("edit_max")
, m_dp(dp)
, m_vertical_padding(DEFAULT_VERTICAL_PADDING)
, m_pills_analysis({})
, m_counter_options(nullptr)
{
    if(!m_tpt)
    {
        spdlog::error("LineTrackItem: m_tpt shared_ptr is null, cannot construct");
        return;
    }
    UpdateMetadata();

    if(m_track_metadata)
    {
        m_track_statistics =
            m_data_provider.DataModel().GetAnalysis().RegisterTrack(*m_track_metadata);
        if(m_track_statistics)
        {
            for(size_t i = 0; i < AnalysisTrackStatistics::Counter::kCounterCount; i++)
            {
                m_pills_analysis[i] = AddPill();
                if(m_pills_analysis[i])
                {
                    m_pills_analysis[i]->SetAccentColor(
                        m_track_statistics->stats[i].accent_color);
                }
            }
        }
    }

    m_counter_options = dynamic_cast<CounterTrackOptions*>(m_options.get());
    if(m_counter_options)
    {
        for(size_t i = 0; i < AnalysisTrackStatistics::Counter::kCounterCount; i++)
        {
            if(m_pills_analysis[i])
            {
                m_pills_analysis[i]->SetVisible(m_counter_options->m_show_analysis[i]);
            }
        }
    }
    ROCPROFVIS_ASSERT(m_counter_options);
}

LineTrackItem::~LineTrackItem() {}

void
LineTrackItem::UpdateMetadata()
{
    if(m_track_metadata)
    {
        const CounterInfo* counter = m_data_provider.DataModel().GetTopology().GetCounter(
            m_track_metadata->topology.id.value);
        if(counter)
        {
            m_units = counter->units;
        }
        m_min_y.Init(0.0, m_units);  // Want to start at 0 by default.
        m_max_y.Init(m_units == "%" ? 100.0 : m_track_metadata->max_value, m_units);
    }
    else
    {
        spdlog::warn("Track info not found for track ID: {}", m_track_id);
    }
    // Ensure that min and max are not equal to allow rendering
    if(m_min_y.Value() == m_max_y.Value())
    {
        m_max_y.Init(m_min_y.Value() + 1.0, m_units);
    }
    UpdateMetaScaleAreaSize();
    UpdateMaxMetaScaleAreaSize();
}

void
LineTrackItem::RenderHighlightBand(ImDrawList* draw_list, const ImVec2& cursor_position,
                                   const ImVec2& content_size, double scale_y)
{
    if(m_counter_options)
    {
        float highlight_y_max =
            static_cast<float>(cursor_position.y + content_size.y -
                               (m_counter_options->m_highlight.range_max -
                                static_cast<float>(m_min_y.Value())) *
                                   scale_y);
        float highlight_y_min =
            static_cast<float>(cursor_position.y + content_size.y -
                               (m_counter_options->m_highlight.range_min -
                                static_cast<float>(m_min_y.Value())) *
                                   scale_y);

        highlight_y_max =
            std::max(cursor_position.y,
                     std::min(cursor_position.y + content_size.y, highlight_y_max));
        highlight_y_min =
            std::max(cursor_position.y,
                     std::min(cursor_position.y + content_size.y, highlight_y_min));

        draw_list->AddRectFilled(
            ImVec2(cursor_position.x, highlight_y_max),
            ImVec2(cursor_position.x + content_size.x, highlight_y_min),
            m_settings.GetColor(Colors::kTrackColorWarningBand));
    }
}

void
LineTrackItem::BoxPlotRender(float graph_width)
{
    const float plot_height = CalculatePlotHeight();
    const float chart_height = plot_height + m_vertical_padding * 2.0f;
    // Borderless children use zero WindowPadding; the shared rounded height keeps
    // this content region aligned with the meta-area scale.
    ImGui::BeginChild("LV", ImVec2(graph_width, chart_height), false,
                      ImGuiWindowFlags_NoMouseInputs);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImVec2 cursor_position = ImGui::GetCursorScreenPos();
    ImVec2 content_size    = ImGui::GetContentRegionAvail();

    cursor_position.y += m_vertical_padding;
    content_size.y = plot_height;

    double      scale_y         = content_size.y / (m_max_y.Value() - m_min_y.Value());
    const float bottom_of_chart = cursor_position.y + content_size.y;

    ImU32 base_fill_color   = m_settings.GetColor(Colors::kLineChartColor);
    ImU32 alt_fill_color    = m_settings.GetColor(Colors::kLineChartColorAlt);
    ImU32 transparent_color = m_settings.GetColor(Colors::kTransparent);
    ImU32 outline_color     = alt_fill_color;
    ImU32 accent            = m_settings.GetColor(Colors::kAccent);

    // Grid lines behind the data, matching the meta-area ticks.
    if(GetTrackHeight() >= Y_AXIS_LABEL_MIN_TRACK_HEIGHT)
    {
        UpdateYAxisTicks();
        const ImU32 grid_color =
            ApplyAlpha(m_settings.GetColor(Colors::kGridColor), Y_AXIS_GRID_LINE_ALPHA);
        for(double value : m_grid_ticks)
        {
            float grid_y = static_cast<float>(cursor_position.y + content_size.y -
                                              (value - m_min_y.Value()) * scale_y);
            draw_list->AddLine(ImVec2(cursor_position.x, grid_y),
                               ImVec2(cursor_position.x + content_size.x, grid_y),
                               grid_color);
        }
    }

    int hovered_idx = -1;
    size_t data_len = m_data.size();
    for(size_t i = 0; i < data_len; ++i)
    {
        ImVec2 point_start = MapToUI(m_data[i].m_start_ts, m_data[i].m_value,
                                     cursor_position, content_size, scale_y);
        ImVec2 point_end = MapToUI(m_data[i].m_end_ts, m_data[i].m_value, cursor_position,
                                   content_size, scale_y);

        if(data_len == 1)
        {
            // If there is only one box, make sure it is visible by giving it a minimum width
            point_end.x = std::max(point_end.x, point_start.x + 1.0f);
        }

        ImU32 fill_color = base_fill_color;
        if(m_counter_options)
        {
            if(m_counter_options->m_boxplot.enabled)
            {
                if(m_counter_options->m_boxplot.stripes && (i % 2))
                {
                    fill_color = alt_fill_color;
                }
            }
            else
            {
                fill_color = transparent_color;
            }
        }

        draw_list->AddRectFilled(ImVec2(point_start.x, point_start.y),
                                 ImVec2(point_end.x, bottom_of_chart), fill_color);
        draw_list->AddLine(point_start, point_end, outline_color, DEFAULT_LINE_THICKNESS);

        if(i + 1 < m_data.size())
        {
            // Map the start of the next box
            ImVec2 next_point_start =
                MapToUI(m_data[i + 1].m_start_ts, m_data[i + 1].m_value, cursor_position,
                        content_size, scale_y);

            draw_list->AddLine(point_end, next_point_start, outline_color,
                               DEFAULT_LINE_THICKNESS);
        }

        if(ImGui::IsMouseHoveringRect(ImVec2(point_start.x, 0.0f),
                                      ImVec2(point_end.x, bottom_of_chart)) &&
           ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows |
                                  ImGuiHoveredFlags_NoPopupHierarchy) &&
           TimelineFocusManager::GetInstance().GetFocusedLayer() == Layer::kNone)
        {
            hovered_idx = static_cast<int>(i);
        }
    }

    if(m_counter_options && m_counter_options->m_highlight.enabled)
    {
        RenderHighlightBand(draw_list, cursor_position, content_size, scale_y);
    }

    if(hovered_idx != -1)
    {
        auto&       hovered_item = m_data[hovered_idx];
        const auto& time_format  = m_settings.GetUserSettings().unit_settings.time_format;
        std::string start_str    = nanosecond_to_formatted_str(
            hovered_item.m_start_ts - m_tpt->GetMinX(), time_format, true);
        std::string dur_str = nanosecond_to_formatted_str(
            hovered_item.m_end_ts - hovered_item.m_start_ts, time_format, true);

        BeginTooltipStyled();
        ImGui::Text("Start: %s", start_str.c_str());
        ImGui::Text("Duration: %s", dur_str.c_str());
        ImGui::Text("Value: %.2f %s", hovered_item.m_value, m_units.c_str());
        EndTooltipStyled();

        // Map start and end points
        ImVec2 start_point = MapToUI(hovered_item.m_start_ts, hovered_item.m_value,
                                     cursor_position, content_size, scale_y);
        ImVec2 end_point   = MapToUI(hovered_item.m_end_ts, hovered_item.m_value,
                                     cursor_position, content_size, scale_y);

        // Draw a circle at the start
        draw_list->AddCircle(start_point, 4.0f, accent, 12, 3);

        // Draw a line from start to end
        draw_list->AddLine(start_point, end_point, accent,
                           DEFAULT_LINE_THICKNESS * 3);
    }

    ImGui::EndChild();
}

void
LineTrackItem::UpdateMetaScaleAreaSize()
{
    m_meta_area_scale_width =
        std::max({ ImGui::CalcTextSize(m_max_y.CompactValue().c_str()).x +
                       m_max_y.ButtonSize(),
                   ImGui::CalcTextSize(m_min_y.CompactValue().c_str()).x +
                       m_min_y.ButtonSize() }) +
        ImGui::GetStyle().ItemSpacing.x;
}

void
LineTrackItem::UpdateMaxMetaScaleAreaSize()
{
    // compact_number_format() will return no more than 8 characters...
    m_max_meta_area_scale_width = m_max_y.ButtonSize() +
                                  ImGui::CalcTextSize("XXXXXXXX").x +
                                  ImGui::GetStyle().ItemSpacing.x;
}

void
LineTrackItem::Update()
{
    if(m_track_statistics)
    {
        // Blue only while a time-range selection narrows the data.
        const bool ranged =
            m_timeline_selection && m_timeline_selection->HasValidTimeRangeSelection();
        for(size_t i = 0; i < m_pills_analysis.size(); i++)
        {
            if(m_pills_analysis[i])
            {
                m_pills_analysis[i]->SetTextColor(
                    ranged ? std::optional<Colors>(Colors::kAccent) : std::nullopt);
                if(m_track_statistics->state == AnalysisTrackStatistics::kReady &&
                   m_track_statistics_dirty)
                {
                    const AnalysisTrackStatistics::Stat& stat =
                        m_track_statistics->stats[i];
                    m_pills_analysis[i]->Activate();
                    m_pills_analysis[i]->SetLabel(stat.CompactValue());
                    m_pills_analysis[i]->SetExtendedLabel(stat.CompactLabel());
                    m_pills_analysis[i]->SetTooltip(stat.FullLabel());
                }
                else if(m_track_statistics->state < AnalysisTrackStatistics::kReady)
                {
                    m_pills_analysis[i]->Deactivate();
                }
            }
        }
    }
    if(m_options && m_options->Updated())
    {
        if(m_counter_options && m_track_statistics)
        {
            for(size_t i = 0; i < AnalysisTrackStatistics::Counter::kCounterCount; i++)
            {
                if(m_pills_analysis[i])
                {
                    m_pills_analysis[i]->SetVisible(m_counter_options->m_show_analysis[i]);
                }
            }
        }
    }

    TrackItem::Update();
}

bool
LineTrackItem::ReleaseData()
{
    if(TrackItem::ReleaseData())
    {
        m_data.clear();
        m_data = {};

        return true;
    }

    return false;
}

void
LineTrackItem::ExtractPointsFromData()
{
    const RawTrackData* rtd = m_data_provider.DataModel().GetTimeline().GetTrackData(m_track_id);

    // If no raw track data is found, this means the track was unloaded before the
    // response was processed
    if(!rtd)
    {
        spdlog::debug("No raw track data found for track {}", m_track_id);
        return;
    }

    const RawTrackSampleData* sample_track = dynamic_cast<const RawTrackSampleData*>(rtd);
    if(!sample_track)
    {
        spdlog::debug("Invalid track data type for track {}", m_track_id);
        m_request_state = TrackDataRequestState::kError;
        return;
    }

    if(sample_track->GetData().empty())
    {
        spdlog::debug("No data for track {}", m_track_id);
        return;
    }
    const std::vector<TraceCounter>& track_data = sample_track->GetData();

    m_data = track_data;
}

float
LineTrackItem::CalculateMissingX(float x_1, float y_1, float x_2, float y_2,
                                 float known_y)
{
    // Calculate slope (m)
    double m = (y_2 - y_1) / (x_2 - x_1);

    // Calculate y-intercept (b)
    double b = y_1 - m * x_1;

    // Calculate x for the given y
    double x = (known_y - b) / m;

    return static_cast<float>(x);
}

float
LineTrackItem::CalculatePlotHeight() const
{
    // ImGui truncates child-window sizes to whole pixels.
    const float chart_height = std::floor(m_track_content_height);
    return std::max(0.0f, chart_height - m_vertical_padding * 2.0f);
}

void
LineTrackItem::GenerateYAxisTicks(float plot_height, std::vector<double>& out_ticks) const
{
    out_ticks.clear();

    const double min_v = m_min_y.Value();
    const double max_v = m_max_y.Value();
    const double range = max_v - min_v;
    if(range <= 0.0 || plot_height <= 0.0f)
    {
        return;
    }

    // Equal segments keep the values centered: one interior value sits at the
    // midpoint and fills outward as the track grows.
    const float target_spacing =
        (ImGui::GetTextLineHeight() + Y_AXIS_TICK_LABEL_GAP) * Y_AXIS_LABEL_SPACING_FACTOR;
    const int segments = static_cast<int>(plot_height / target_spacing);
    if(segments < 2)
    {
        return;
    }

    const double step = range / segments;
    for(int i = 1; i < segments; ++i)
    {
        out_ticks.push_back(min_v + i * step);
    }
}

void
LineTrackItem::UpdateYAxisTicks()
{
    const float  plot_height = CalculatePlotHeight();
    const double min_v       = m_min_y.Value();
    const double max_v       = m_max_y.Value();
    const float  line_h      = ImGui::GetTextLineHeight();

    // Reuse the cached ticks unless the plot height, Y range, or text height
    // changed since the last frame.
    if(plot_height == m_cached_ticks_height && min_v == m_cached_ticks_min &&
       max_v == m_cached_ticks_max && line_h == m_cached_ticks_line_h)
    {
        return;
    }

    m_cached_ticks_height = plot_height;
    m_cached_ticks_min    = min_v;
    m_cached_ticks_max    = max_v;
    m_cached_ticks_line_h = line_h;

    GenerateYAxisTicks(plot_height, m_grid_ticks);
}

void
LineTrackItem::RenderMetaAreaScale()
{
    ImVec2      content_region = ImGui::GetContentRegionMax();
    const float label_x =
        content_region.x - m_meta_area_scale_width + m_metadata_padding.x;

    // Max value (top, editable).
    ImGui::SetCursorPos(ImVec2(label_x, m_metadata_padding.y));
    m_max_y.Render();

    // Min value (bottom, editable).
    ImVec2 min_size = ImGui::CalcTextSize(m_min_y.CompactValue().c_str());
    ImGui::SetCursorPos(
        ImVec2(label_x, content_region.y - min_size.y - m_metadata_padding.y));
    m_min_y.Render();

    // Anchor to the same plot region as BoxPlotRender so ticks line up with the
    // grid lines.
    ImDrawList*  draw_list    = ImGui::GetWindowDrawList();
    const ImVec2 win_pos      = ImGui::GetWindowPos();
    const float  plot_top     = win_pos.y + m_vertical_padding;
    const float  plot_height  = CalculatePlotHeight();
    const float  plot_bottom  = plot_top + plot_height;
    const float  tick_right_x = win_pos.x + ImGui::GetWindowSize().x;
    const double min_v        = m_min_y.Value();
    const double range        = m_max_y.Value() - min_v;

    const ImU32 tick_color   = m_settings.GetColor(Colors::kGridColor);
    const ImU32 label_color  = m_settings.GetColor(Colors::kTextDim);
    ImFont*     font         = ImGui::GetFont();
    const float font_size    = ImGui::GetFontSize();
    // Interior labels are right-aligned to the same edge as the min/max labels
    // (see EditableTextField::DrawPlainText) so they line up horizontally.
    const float label_right_x =
        win_pos.x + content_region.x - ImGui::GetStyle().WindowPadding.x;

    // Min/max tick marks.
    draw_list->AddLine(ImVec2(tick_right_x - Y_AXIS_TICK_MARK_LENGTH, plot_top),
                       ImVec2(tick_right_x, plot_top), tick_color);
    draw_list->AddLine(ImVec2(tick_right_x - Y_AXIS_TICK_MARK_LENGTH, plot_bottom),
                       ImVec2(tick_right_x, plot_bottom), tick_color);

    if(range > 0.0 && plot_height > 0.0f &&
       GetTrackHeight() >= Y_AXIS_LABEL_MIN_TRACK_HEIGHT)
    {
        UpdateYAxisTicks();
        for(double value : m_grid_ticks)
        {
            float y = plot_bottom -
                      static_cast<float>((value - min_v) / range) * plot_height;

            draw_list->AddLine(ImVec2(tick_right_x - Y_AXIS_TICK_MARK_LENGTH, y),
                               ImVec2(tick_right_x, y), tick_color);

            // Skip labels that would overlap the min/max labels.
            if(y > plot_top + font_size && y < plot_bottom - font_size)
            {
                std::string label   = compact_number_format(value);
                const float label_w = ImGui::CalcTextSize(label.c_str()).x;
                draw_list->AddText(font, font_size,
                                   ImVec2(label_right_x - label_w, y - font_size * 0.5f),
                                   label_color, label.c_str());
            }
        }
    }
}

void
LineTrackItem::RenderChart(float graph_width)
{
    BoxPlotRender(graph_width);
}

ImVec2
LineTrackItem::MapToUI(double x_in, double y_in, ImVec2& cursor_position,
                       ImVec2& content_size, double scaleY)
{
    ImVec2 container_pos = ImGui::GetWindowPos();

    double x = container_pos.x + m_tpt->RawTimeToPixel(x_in);
    double y = cursor_position.y + content_size.y - (y_in - m_min_y.Value()) * scaleY;

    return ImVec2(static_cast<float>(x), static_cast<float>(y));
}

LineTrackItem::VerticalLimits::VerticalLimits(std::string field_id)
: m_default_value(0.0)
, m_value(0.0)
, m_text_field(std::move(field_id))
{
    m_text_field.SetOnTextCommit([this](const std::string& committed_text) {
        // Empty string signals a revert-to-default in our usage
        if(committed_text.empty())
        {
            UpdateValue(m_default_value);
        }
        else if(committed_text != m_edit_str)
        {
            double processed = ProcessUserInput(committed_text);
            UpdateValue(processed);
        }
    });
}

double
LineTrackItem::VerticalLimits::Value() const
{
    return m_value;
}

const std::string&
LineTrackItem::VerticalLimits::StrValue() const
{
    return m_formatted_str;
}

const std::string&
LineTrackItem::VerticalLimits::CompactValue() const
{
    return m_compact_str;
}

float
LineTrackItem::VerticalLimits::ButtonSize() const
{
    return m_text_field.ButtonSize();
}

void
LineTrackItem::VerticalLimits::Init(double value, std::string units)
{
    m_default_value     = value;
    m_units             = units;
    m_formatted_default = FormatValue(value);
    if(!units.empty())
    {
        m_formatted_default += " " + units;
    }
    UpdateValue(value);
}

void
LineTrackItem::VerticalLimits::Render()
{
    m_text_field.Render();
}

void
LineTrackItem::VerticalLimits::UpdateValue(double value)
{
    m_text_field.ShowResetButton(value != m_default_value);
    m_value         = value;
    m_formatted_str = FormatValue(value);
    m_compact_str   = compact_number_format(value);
    // Units-free so the edit box shows "25.2M", not "25.2M bytes".
    m_edit_str = m_compact_str;
    if(!m_units.empty())
    {
        m_formatted_str += " " + m_units;
        m_compact_str += " " + m_units;
    }
    m_text_field.SetText(m_compact_str, m_formatted_str, m_formatted_default);
    m_text_field.SetEditText(m_edit_str);
}

std::string
LineTrackItem::VerticalLimits::FormatValue(double value)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << value;
    return oss.str();
}

double
LineTrackItem::VerticalLimits::ProcessUserInput(std::string_view input)
{
    double value = 0.0;
    if(parse_compact_number(input, m_units, value))
    {
        return value;
    }
    m_text_field.RevertToDefault();
    return m_default_value;
}

}  // namespace View
}  // namespace RocProfVis
