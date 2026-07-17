// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_line_track_item.h"
#include "rocprofvis_click_manager.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_track_options.h"
#include "rocprofvis_utils.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace RocProfVis
{
namespace View
{

constexpr float DEFAULT_VERTICAL_PADDING = 2.0f;
constexpr float DEFAULT_LINE_THICKNESS   = 1.0f;

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
                m_pills_analysis[i]->SetAccentColor(
                    m_track_statistics->stats[i].accent_color);
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
    float highlight_y_max = static_cast<float>(
        cursor_position.y + content_size.y -
        (m_counter_options->m_highlight.range_max - static_cast<float>(m_min_y.Value())) *
            scale_y);
    float highlight_y_min = static_cast<float>(
        cursor_position.y + content_size.y -
        (m_counter_options->m_highlight.range_min - static_cast<float>(m_min_y.Value())) *
            scale_y);

    highlight_y_max = std::max(
        cursor_position.y, std::min(cursor_position.y + content_size.y, highlight_y_max));
    highlight_y_min = std::max(
        cursor_position.y, std::min(cursor_position.y + content_size.y, highlight_y_min));

    draw_list->AddRectFilled(ImVec2(cursor_position.x, highlight_y_max),
                             ImVec2(cursor_position.x + content_size.x, highlight_y_min),
                             m_settings.GetColor(Colors::kTrackColorWarningBand));
}

void
LineTrackItem::BoxPlotRender(float graph_width)
{
    ImGui::BeginChild("LV", ImVec2(graph_width, m_track_content_height), false,
                      ImGuiWindowFlags_NoMouseInputs);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImVec2 cursor_position = ImGui::GetCursorScreenPos();
    ImVec2 content_size    = ImGui::GetContentRegionAvail();

    cursor_position.y += m_vertical_padding;
    content_size.y -= (m_vertical_padding * 2.0f);

    double      scale_y         = content_size.y / (m_max_y.Value() - m_min_y.Value());
    const float bottom_of_chart = cursor_position.y + content_size.y;

    ImU32 base_fill_color   = m_settings.GetColor(Colors::kLineChartColor);
    ImU32 alt_fill_color    = m_settings.GetColor(Colors::kLineChartColorAlt);
    ImU32 transparent_color = m_settings.GetColor(Colors::kTransparent);
    ImU32 outline_color     = alt_fill_color;
    ImU32 accent            = m_settings.GetColor(Colors::kAccent);

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

        ImU32 fill_color = (!m_counter_options->m_boxplot.enabled) ? transparent_color
                           : (m_counter_options->m_boxplot.stripes && (i % 2 == 0))
                               ? alt_fill_color
                               : base_fill_color;

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

    if(m_counter_options->m_highlight.enabled)
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
        for(size_t i = 0; i < m_pills_analysis.size(); i++)
        {
            if(m_pills_analysis[i])
            {
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
    if(m_options->Updated())
    {
        if(m_counter_options && m_track_statistics)
        {
            for(size_t i = 0; i < AnalysisTrackStatistics::Counter::kCounterCount; i++)
            {
                m_pills_analysis[i]->SetVisible(m_counter_options->m_show_analysis[i]);
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

bool
LineTrackItem::ExtractPointsFromData()
{
    const RawTrackData* rtd = m_data_provider.DataModel().GetTimeline().GetTrackData(m_track_id);

    // If no raw track data is found, this means the track was unloaded before the
    // response was processed
    if(!rtd)
    {
        spdlog::error("No raw track data found for track {}", m_track_id);
        // Reset the request state to idle
        m_request_state = TrackDataRequestState::kIdle;
        return false;
    }

    const RawTrackSampleData* sample_track = dynamic_cast<const RawTrackSampleData*>(rtd);
    if(!sample_track)
    {
        spdlog::debug("Invalid track data type for track {}", m_track_id);
        m_request_state = TrackDataRequestState::kError;
        return false;
    }

    if(sample_track->AllDataReady())
    {
        m_request_state = TrackDataRequestState::kIdle;
    }

    if(sample_track->GetData().empty())
    {
        spdlog::debug("No data for track {}", m_track_id);
        return false;
    }
    const std::vector<TraceCounter>& track_data = sample_track->GetData();

    m_data = track_data;

    return true;
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

void
LineTrackItem::RenderMetaAreaScale()
{
    ImVec2 content_region = ImGui::GetContentRegionMax();

    ImGui::SetCursorPos(ImVec2(content_region.x - m_meta_area_scale_width +
                                   m_metadata_padding.x,
                               m_metadata_padding.y));
    m_max_y.Render();

    ImVec2 min_size = ImGui::CalcTextSize(m_min_y.CompactValue().c_str());
    ImGui::SetCursorPos(ImVec2(content_region.x - m_meta_area_scale_width +
                                   m_metadata_padding.x,
                               content_region.y - min_size.y - m_metadata_padding.y));
    m_min_y.Render();
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
