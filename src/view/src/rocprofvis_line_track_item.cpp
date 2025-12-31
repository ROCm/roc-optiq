// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_line_track_item.h"
#include "rocprofvis_click_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <charconv>
#include <iomanip>
#include <sstream>

namespace RocProfVis
{
namespace View
{

constexpr float DEFAULT_VERTICAL_PADDING = 2.0f;
constexpr float DEFAULT_LINE_THICKNESS   = 1.0f;
constexpr float SCALE_SEPERATOR_WIDTH    = 2.0f;

LineTrackItem::LineTrackItem(DataProvider& dp, uint64_t id, std::string name,
                             float                               max_meta_area_width,
                             std::shared_ptr<TimePixelTransform> tpt)
: TrackItem(dp, id, name, tpt)
, m_data({})
, m_highlight_y_limits()
, m_highlight_y_range(false)
, m_dp(dp)
, m_show_boxplot(true)
, m_show_boxplot_stripes(false)
, m_linetrack_project_settings(dp.GetTraceFilePath(), *this)
, m_min_y("edit_min")
, m_max_y("edit_max")
, m_vertical_padding(DEFAULT_VERTICAL_PADDING)
{
    if(!m_tpt)
    {
        spdlog::error("LineTrackItem: m_tpt shared_ptr is null, cannot construct");
        return;
    }
    m_meta_area_scale_width = max_meta_area_width;
    UpdateMetadata();

    if(m_linetrack_project_settings.Valid())
    {
        m_show_boxplot         = m_linetrack_project_settings.BoxPlot();
        m_show_boxplot_stripes = m_linetrack_project_settings.BoxPlotStripes();
        m_highlight_y_range    = m_linetrack_project_settings.Highlight();
        m_highlight_y_limits   = m_linetrack_project_settings.HighlightRange();
    }
}

LineTrackItem::~LineTrackItem() {}

void
LineTrackItem::UpdateMetadata()
{
    const TrackInfo* track_info = m_data_provider.DataModel().GetTimeline().GetTrack(m_id);
    if(track_info)
    {
        const CounterInfo* counter =
            m_data_provider.DataModel().GetTopology().GetCounter(track_info->topology.id);
        if(counter)
        {
            m_units = counter->units;
        }
        m_min_y.Init(0.0, m_units);  // Want to start at 0 by default.
        m_max_y.Init(m_units == "%" ? 100.0 : track_info->max_value, m_units);
    }
    else
    {
        spdlog::warn("Track info not found for track ID: {}", m_id);
    }
    // Ensure that min and max are not equal to allow rendering
    if(m_min_y.Value() == m_max_y.Value())
    {
        m_max_y.Init(m_min_y.Value() + 1.0, m_units);
    }
    m_meta_area_scale_width = CalculateNewMetaAreaSize();
}

void
LineTrackItem::RenderHighlightBand(ImDrawList* draw_list, const ImVec2& cursor_position,
                                   const ImVec2& content_size, double scale_y)
{
    float highlight_y_max = static_cast<float>(
        cursor_position.y + content_size.y -
        (m_highlight_y_limits.max_limit - static_cast<float>(m_min_y.Value())) * scale_y);
    float highlight_y_min = static_cast<float>(
        cursor_position.y + content_size.y -
        (m_highlight_y_limits.min_limit - static_cast<float>(m_min_y.Value())) * scale_y);

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
    ImU32 accent_red        = m_settings.GetColor(Colors::kAccentRed);

    int hovered_idx = -1;
    for(size_t i = 0; i < m_data.size(); ++i)
    {
        ImVec2 point_start = MapToUI(m_data[i].m_start_ts, m_data[i].m_value,
                                     cursor_position, content_size, scale_y);
        ImVec2 point_end = MapToUI(m_data[i].m_end_ts, m_data[i].m_value, cursor_position,
                                   content_size, scale_y);

        ImU32 fill_color = (!m_show_boxplot)                          ? transparent_color
                           : (m_show_boxplot_stripes && (i % 2 == 0)) ? alt_fill_color
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

    if(m_highlight_y_range)
        RenderHighlightBand(draw_list, cursor_position, content_size, scale_y);

    if(hovered_idx != -1)
    {
        auto&       hovered_item = m_data[hovered_idx];
        const auto& time_format  = m_settings.GetUserSettings().unit_settings.time_format;
        std::string start_str    = nanosecond_to_formatted_str(
            hovered_item.m_start_ts - m_tpt->GetMinX(), time_format, true);
        std::string dur_str = nanosecond_to_formatted_str(
            hovered_item.m_end_ts - hovered_item.m_start_ts, time_format, true);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                            m_settings.GetDefaultStyle().WindowPadding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,
                            m_settings.GetDefaultStyle().FrameRounding);
        ImGui::BeginTooltip();
        ImGui::Text("Start: %s", start_str.c_str());
        ImGui::Text("Duration: %s", dur_str.c_str());
        ImGui::Text("Value: %.2f %s", hovered_item.m_value, m_units.c_str());
        ImGui::EndTooltip();
        ImGui::PopStyleVar(2);

        // Map start and end points
        ImVec2 start_point = MapToUI(hovered_item.m_start_ts, hovered_item.m_value,
                                     cursor_position, content_size, scale_y);
        ImVec2 end_point   = MapToUI(hovered_item.m_end_ts, hovered_item.m_value,
                                     cursor_position, content_size, scale_y);

        // Draw a circle at the start
        draw_list->AddCircle(start_point, 4.0f, accent_red, 12, 3);

        // Draw a line from start to end
        draw_list->AddLine(start_point, end_point, accent_red,
                           DEFAULT_LINE_THICKNESS * 3);
    }

    ImGui::EndChild();
}

float
LineTrackItem::CalculateNewMetaAreaSize()
{
    ImVec2 max_size = ImGui::CalcTextSize(m_max_y.CompactValue().c_str());

    ImVec2 min_size = ImGui::CalcTextSize(m_min_y.CompactValue().c_str());

    return std::max(
               { max_size.x + m_max_y.ButtonSize(), min_size.x + m_min_y.ButtonSize() }) +
           6 * m_metadata_padding
                   .x;  // TODO: Hardcoded padding for posible label size
                        // Think later how it can be calculated or store as default values
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
    const RawTrackData* rtd = m_data_provider.DataModel().GetTimeline().GetTrackData(m_id);

    // If no raw track data is found, this means the track was unloaded before the
    // response was processed
    if(!rtd)
    {
        spdlog::error("No raw track data found for track {}", m_id);
        // Reset the request state to idle
        m_request_state = TrackDataRequestState::kIdle;
        return false;
    }

    const RawTrackSampleData* sample_track = dynamic_cast<const RawTrackSampleData*>(rtd);
    if(!sample_track)
    {
        spdlog::debug("Invalid track data type for track {}", m_id);
        m_request_state = TrackDataRequestState::kError;
        return false;
    }

    if(sample_track->AllDataReady())
    {
        m_request_state = TrackDataRequestState::kIdle;
    }

    if(sample_track->GetData().empty())
    {
        spdlog::debug("No data for track {}", m_id);
        return false;
    }
    const std::vector<rocprofvis_trace_counter_t> track_data = sample_track->GetData();

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
    ImVec2 window_pos     = ImGui::GetWindowPos();

    ImGui::SetCursorPos(ImVec2(content_region.x - m_meta_area_scale_width +
                                   m_metadata_padding.x + SCALE_SEPERATOR_WIDTH,
                               m_metadata_padding.y));
    m_max_y.Render();

    ImVec2 min_size = ImGui::CalcTextSize(m_min_y.CompactValue().c_str());
    ImGui::SetCursorPos(ImVec2(content_region.x - m_meta_area_scale_width +
                                   m_metadata_padding.x + SCALE_SEPERATOR_WIDTH,
                               content_region.y - min_size.y - m_metadata_padding.y));
    m_min_y.Render();

    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(window_pos.x + content_region.x - m_meta_area_scale_width, window_pos.y),
        ImVec2(window_pos.x + content_region.x - m_meta_area_scale_width,
               window_pos.y + content_region.y),
        m_settings.GetColor(Colors::kMetaDataSeparator), SCALE_SEPERATOR_WIDTH);
}

void
LineTrackItem::RenderChart(float graph_width)
{
    BoxPlotRender(graph_width);
}
void
LineTrackItem::RenderMetaAreaOptions()
{
    ImGui::Checkbox("Show Counter Boxes", &m_show_boxplot);
    ImGui::Checkbox("Alternate Counter Coloring", &m_show_boxplot_stripes);
    if(ImGui::Checkbox("Highlight Y Range", &m_highlight_y_range))
    {
        float min_limit                = static_cast<float>(m_min_y.Value());
        float max_limit                = static_cast<float>(m_max_y.Value());
        m_highlight_y_limits.min_limit = min_limit;
        m_highlight_y_limits.max_limit = max_limit;
    }

    if(m_highlight_y_range)
    {
        float min_limit = static_cast<float>(m_min_y.Value());
        float max_limit = static_cast<float>(m_max_y.Value());

        float min_percent =
            (m_highlight_y_limits.min_limit - min_limit) / (max_limit - min_limit);
        float max_percent =
            (m_highlight_y_limits.max_limit - min_limit) / (max_limit - min_limit);

        ImGui::BeginGroup();
        ImGui::TextUnformatted("Min Value");
        ImGui::SetNextItemWidth(120);
        if(ImGui::SliderFloat("##min_drag", &min_percent, 0.0f, 1.0f, "",
                              ImGuiSliderFlags_None))
        {
            m_highlight_y_limits.min_limit =
                min_limit + (max_limit - min_limit) * min_percent;
        }
        ImGui::SetNextItemWidth(120);
        if(ImGui::InputFloat("##min_input", &m_highlight_y_limits.min_limit))
        {
            m_highlight_y_limits.min_limit =
                std::clamp(m_highlight_y_limits.min_limit, min_limit, max_limit);
        }
        ImGui::EndGroup();
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(3.0f, 0.0f));
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                              m_settings.GetColor(Colors::kSplitterColor));

        ImGui::BeginChild("Splitter For Max/Min", ImVec2(1, 75), ImGuiChildFlags_None);

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(3.0f, 0.0f));
        ImGui::SameLine();

        ImGui::BeginGroup();
        ImGui::TextUnformatted("Max Value");
        ImGui::SetNextItemWidth(120);
        if(ImGui::SliderFloat("##max_drag", &max_percent, 0.0f, 1.0f, "",
                              ImGuiSliderFlags_None))
        {
            m_highlight_y_limits.max_limit =
                min_limit + (max_limit - min_limit) * max_percent;
        }
        ImGui::SetNextItemWidth(120);
        if(ImGui::InputFloat("##max_input", &m_highlight_y_limits.max_limit))
        {
            m_highlight_y_limits.max_limit =
                std::clamp(m_highlight_y_limits.max_limit, min_limit, max_limit);
        }
        ImGui::EndGroup();

        // Clamp and sync values only after user interaction
        m_highlight_y_limits.min_limit =
            std::clamp(m_highlight_y_limits.min_limit, min_limit, max_limit);
        m_highlight_y_limits.max_limit =
            std::clamp(m_highlight_y_limits.max_limit, min_limit, max_limit);

        if(m_highlight_y_limits.min_limit > m_highlight_y_limits.max_limit)
            m_highlight_y_limits.max_limit = m_highlight_y_limits.min_limit;
        if(m_highlight_y_limits.max_limit < m_highlight_y_limits.min_limit)
            m_highlight_y_limits.min_limit = m_highlight_y_limits.max_limit;
    }
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

LineTrackProjectSettings::LineTrackProjectSettings(const std::string& project_id,
                                                   LineTrackItem&     track_item)
: ProjectSetting(project_id)
, m_track_item(track_item)
{}

LineTrackProjectSettings::~LineTrackProjectSettings() {}

void
LineTrackProjectSettings::ToJson()
{
    jt::Json& track = m_settings_json[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_TRACK]
                                     [m_track_item.GetID()];
    track[JSON_KEY_TIMELINE_TRACK_BOX_PLOT] = m_track_item.m_show_boxplot;
    track[JSON_KEY_TIMELINE_TRACK_COLOR]    = m_track_item.m_highlight_y_range;
    track[JSON_KEY_TIMELINE_TRACK_COLOR_RANGE_MIN] =
        m_track_item.m_highlight_y_limits.min_limit;
    track[JSON_KEY_TIMELINE_TRACK_COLOR_RANGE_MAX] =
        m_track_item.m_highlight_y_limits.max_limit;
    track[JSON_KEY_TIMELINE_TRACK_STRIPES] = m_track_item.m_show_boxplot_stripes;
}

bool
LineTrackProjectSettings::Valid() const
{
    jt::Json& track = m_settings_json[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_TRACK]
                                     [m_track_item.GetID()];
    return track[JSON_KEY_TIMELINE_TRACK_BOX_PLOT].isBool() &&
           track[JSON_KEY_TIMELINE_TRACK_STRIPES].isBool() &&
           track[JSON_KEY_TIMELINE_TRACK_COLOR].isBool() &&
           track[JSON_KEY_TIMELINE_TRACK_COLOR_RANGE_MIN].isNumber() &&
           track[JSON_KEY_TIMELINE_TRACK_COLOR_RANGE_MAX].isNumber();
}

bool
LineTrackProjectSettings::BoxPlot() const
{
    return m_settings_json[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_TRACK]
                          [m_track_item.GetID()][JSON_KEY_TIMELINE_TRACK_BOX_PLOT]
                              .getBool();
}

bool
LineTrackProjectSettings::BoxPlotStripes() const
{
    return m_settings_json[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_TRACK]
                          [m_track_item.GetID()][JSON_KEY_TIMELINE_TRACK_STRIPES]
                              .getBool();
}

bool
LineTrackProjectSettings::Highlight() const
{
    return m_settings_json[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_TRACK]
                          [m_track_item.GetID()][JSON_KEY_TIMELINE_TRACK_COLOR]
                              .getBool();
}

HighlightYRange
LineTrackProjectSettings::HighlightRange() const
{
    jt::Json& track = m_settings_json[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_TRACK]
                                     [m_track_item.GetID()];
    return HighlightYRange{
        static_cast<float>(track[JSON_KEY_TIMELINE_TRACK_COLOR_RANGE_MAX].getNumber()),
        static_cast<float>(track[JSON_KEY_TIMELINE_TRACK_COLOR_RANGE_MIN].getNumber())
    };
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
        else if(committed_text != m_compact_str)
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
    if(!m_units.empty())
    {
        m_formatted_str += " " + m_units;
        m_compact_str += " " + m_units;
    }
    m_text_field.SetText(m_compact_str, m_formatted_str, m_formatted_default);
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
    double      result = 0.0;
    const char* first  = input.data();
    const char* last   = input.data() + input.size();
    auto [ptr, error_code] =
        std::from_chars(first, last, result, std::chars_format::general);
    if(error_code == std::errc{} && std::isfinite(result) && ptr == last)
    {
        return result;
    }
    else
    {
        m_text_field.RevertToDefault();
        return m_default_value;
    }
}

}  // namespace View
}  // namespace RocProfVis
