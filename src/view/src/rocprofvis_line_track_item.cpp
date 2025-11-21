// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

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

LineTrackItem::LineTrackItem(DataProvider& dp, uint64_t id, std::string name, float zoom,
                             double time_offset_ns, double& min_x, double& max_x,
                             double scale_x, float max_meta_area_width)
: TrackItem(dp, id, name, zoom, time_offset_ns, min_x, max_x, scale_x)
, m_data({})
, m_highlight_y_limits()
, m_highlight_y_range(false)
, m_dp(dp)
, m_show_boxplot(true)
, m_show_boxplot_stripes(false)
, m_linetrack_project_settings(dp.GetTraceFilePath(), *this)
, m_min_y(0, "edit_min", "Min: ")
, m_max_y(0, "edit_max", "Max: ")
, m_vertical_padding(DEFAULT_VERTICAL_PADDING)
, m_is_tooltip_active(false)
{
    m_meta_area_scale_width = max_meta_area_width;
    UpdateYScaleExtents();

    if(m_linetrack_project_settings.Valid())
    {
        m_show_boxplot       = m_linetrack_project_settings.BoxPlot();
        m_show_boxplot_stripes = m_linetrack_project_settings.BoxPlotStripes();
        m_highlight_y_range  = m_linetrack_project_settings.Highlight();
        m_highlight_y_limits = m_linetrack_project_settings.HighlightRange();
    }
}

LineTrackItem::~LineTrackItem() {}

void
LineTrackItem::UpdateYScaleExtents()
{
    const track_info_t* track_info = m_data_provider.GetTrackInfo(m_id);
    if(track_info)
    {
        m_min_y.SetValue(0.0);  // Want to start at 0 by default.
        m_max_y.SetValue(track_info->max_value);
    }
    else
    {
        spdlog::warn("Track info not found for track ID: {}", m_id);
    }
    // Ensure that min and max are not equal to allow rendering
    if(m_min_y.Value() == m_max_y.Value())
    {
        m_max_y.SetValue(m_min_y.Value() + 1.0);
    }
    m_meta_area_scale_width = CalculateNewMetaAreaSize();
}

void
LineTrackItem::LineTrackRender(float graph_width)
{
    ImGui::BeginChild("LV", ImVec2(graph_width, m_track_content_height), false);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImVec2 cursor_position = ImGui::GetCursorScreenPos();
    ImVec2 content_size    = ImGui::GetContentRegionAvail();
    ImVec2 container_pos   = ImGui::GetWindowPos();

    // Apply vertical padding to prevent data lines from touching track boundaries
    cursor_position.y += m_vertical_padding;
    content_size.y -= (m_vertical_padding * 2.0f);

    double scale_y = content_size.y / (m_max_y.Value() - m_min_y.Value());

    double tooltip_x     = 0;
    double tooltip_y     = 0;
    bool   show_tooltip  = false;
    ImU32  generic_black = m_settings.GetColor(Colors::kLineChartColor);

    const float line_thickness = 2.0f;  // FIXME: hardcoded value

    for(int i = 1; i < m_data.size(); i++)
    {
        ImVec2 point_1 =
            MapToUI(m_data[i - 1], cursor_position, content_size, m_scale_x, scale_y);
        if(ImGui::IsMouseHoveringRect(ImVec2(point_1.x - 10, point_1.y - 10),
                                      ImVec2(point_1.x + 10, point_1.y + 10)) &&
           TimelineFocusManager::GetInstance().GetFocusedLayer() == Layer::kNone)
        {
            tooltip_x    = m_data[i - 1].x_value - m_min_x;
            tooltip_y    = m_data[i - 1].y_value;
            show_tooltip = true;
        }

        ImVec2 point_2 =
            MapToUI(m_data[i], cursor_position, content_size, m_scale_x, scale_y);
        ImU32 line_color = generic_black;

        if(point_2.x < container_pos.x || point_1.x > container_pos.x + content_size.x)
        {
            // Skip rendering if the points are outside the visible area.
            continue;
        }

        draw_list->AddLine(point_1, point_2, line_color, line_thickness);
    }
    if(m_highlight_y_range)
    {
        RenderHighlightBand(draw_list, cursor_position, content_size, scale_y);
    }
    if(show_tooltip)
    {
        RenderTooltip(tooltip_x, tooltip_y);
    }
    ImGui::EndChild();
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
    ImGui::BeginChild("LV", ImVec2(graph_width, m_track_content_height), false);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImVec2 cursor_position = ImGui::GetCursorScreenPos();
    ImVec2 content_size    = ImGui::GetContentRegionAvail();
    ImVec2 container_pos   = ImGui::GetWindowPos();

    // Apply vertical padding to prevent data lines from touching track boundaries
    cursor_position.y += m_vertical_padding;
    content_size.y -= (m_vertical_padding * 2.0f);

    double scale_y = content_size.y / (m_max_y.Value() - m_min_y.Value());

    double tooltip_start    = 0;
    double tooltip_duration = 0;
    double tooltip_value    = 0;
    bool   show_tooltip     = false;
    ImU32  fill_color       = m_settings.GetColor(Colors::kLineChartColor);
    ImU32  outline_color    = m_settings.GetColor(Colors::kLineChartColorAlt);

    for(size_t i = 0; i < m_data.size(); ++i)
    {
        ImVec2 point_start =
            MapToUI(m_data[i], cursor_position, content_size, m_scale_x, scale_y);
        rocprofvis_data_point_t point_end_data{ m_data[i].x_value + m_data[i].x2_value,
                                                m_data[i].y_value,
                                                m_data[i].x_value + m_data[i].x2_value };
        ImVec2                  point_end =
            MapToUI(point_end_data, cursor_position, content_size, m_scale_x, scale_y);

        float bottom_of_chart = cursor_position.y + content_size.y;


        // Controls Striping of Boxplots
        if(m_show_boxplot_stripes && (i % 2 == 0))
        {
            fill_color = m_settings.GetColor(Colors::kLineChartColorAlt);
        }
        else
        {
            fill_color = m_settings.GetColor(Colors::kLineChartColor);
        }   

        // Controls if the user can see the counter boxes
        if(!m_show_boxplot)
        {
            fill_color = m_settings.GetColor(Colors::kTransparent);
        }
       

        // Draw filled rectangle under the segment
        draw_list->AddRectFilled(ImVec2(point_start.x, point_start.y),
                                 ImVec2(point_end.x, bottom_of_chart), fill_color);

        draw_list->AddLine(point_start, point_end, outline_color, 1);

        // Draw vertical connector to next sample if value changes
        if(i + 1 < m_data.size())
        {
            ImVec2 next_point_start =
                MapToUI(m_data[i + 1], cursor_position, content_size, m_scale_x, scale_y);
            draw_list->AddLine(point_end, next_point_start, outline_color, 1);
        }

        if(ImGui::IsMouseHoveringRect(ImVec2(point_start.x - 10, point_start.y - 10),
                                      ImVec2(point_end.x + 10, bottom_of_chart)) &&
           TimelineFocusManager::GetInstance().GetFocusedLayer() == Layer::kNone &&
           !m_is_tooltip_active)
        {
            tooltip_start    = m_data[i].x_value - m_min_x;
            tooltip_duration = m_data[i].x2_value - m_data[i].x_value;
            tooltip_value    = m_data[i].y_value;
            show_tooltip     = true;
            draw_list->AddCircle(point_start, 4.0f,
                                 m_settings.GetColor(Colors::kAccentRed), 12, 3);
            draw_list->AddLine(point_start, point_end,
                               m_settings.GetColor(Colors::kAccentRed), 3);
            m_is_tooltip_active = true;
        }
    }

    if(m_highlight_y_range)
    {
        RenderHighlightBand(draw_list, cursor_position, content_size, scale_y);
    }
    if(show_tooltip)
    {
        ImGui::BeginTooltip();
        ImGui::Text("Start: %.2f ns", tooltip_start);
        ImGui::Text("End:   %.2f ns", tooltip_duration);
        ImGui::Text("Value: %.2f", tooltip_value);
        ImGui::EndTooltip();
    }
    ImGui::EndChild();

    m_is_tooltip_active = false;
}

void
LineTrackItem::RenderTooltip(double tooltip_x, double tooltip_y)
{
    std::string x_label = nanosecond_to_formatted_str(
        tooltip_x, m_settings.GetUserSettings().unit_settings.time_format, true);
    ImGui::BeginTooltip();
    ImGui::Text("X: %s", x_label.c_str());
    ImGui::Text("Y: %.2f", tooltip_y);
    ImGui::EndTooltip();
}

float
LineTrackItem::CalculateNewMetaAreaSize()
{
    ImVec2 max_size =
        ImGui::CalcTextSize((m_max_y.CompactValue() + m_max_y.Prefix()).c_str());

    ImVec2 min_size =
        ImGui::CalcTextSize((m_min_y.CompactValue() + m_min_y.Prefix()).c_str());

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
    const RawTrackData* rtd = m_data_provider.GetRawTrackData(m_id);

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
    uint64_t                                      count      = track_data.size();

    m_data.clear();
    m_data.reserve(count);
    for(uint64_t i = 0; i < count; i++)
    {
        m_data.emplace_back(rocprofvis_data_point_t{
            track_data[i].m_start_ts, track_data[i].m_value, track_data[i].m_duration });
    }
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

    ImGui::SetCursorPos(
        ImVec2(content_region.x + m_metadata_padding.x - m_meta_area_scale_width,
               m_metadata_padding.y));
    ImGui::TextUnformatted(m_max_y.Prefix().c_str());
    ImGui::SameLine();
    m_max_y.Render();

    ImVec2 min_size = ImGui::CalcTextSize(m_min_y.CompactValue().c_str());
    ImGui::SetCursorPos(
        ImVec2(content_region.x + m_metadata_padding.x - m_meta_area_scale_width,
               content_region.y - min_size.y - m_metadata_padding.y));
    ImGui::TextUnformatted(m_min_y.Prefix().c_str());
    ImGui::SameLine();
    m_min_y.Render();

    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(window_pos.x + content_region.x - m_meta_area_scale_width, window_pos.y),
        ImVec2(window_pos.x + content_region.x - m_meta_area_scale_width,
               window_pos.y + content_region.y),
        m_settings.GetColor(Colors::kMetaDataSeparator), 2.0f);
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
LineTrackItem::MapToUI(rocprofvis_data_point_t& point, ImVec2& cursor_position,
                       ImVec2& content_size, double scaleX, double scaleY)
{
    ImVec2 container_pos = ImGui::GetWindowPos();

    double x = container_pos.x + (point.x_value - (m_min_x + m_time_offset_ns)) * scaleX;
    double y =
        cursor_position.y + content_size.y - (point.y_value - m_min_y.Value()) * scaleY;

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

LineTrackItem::VerticalLimits::VerticalLimits(double value, std::string field_id,
                                              std::string prefix)
: m_text_field(std::move(field_id))
, m_prefix(std::move(prefix))
{
    SetValue(value);
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

const std::string&
LineTrackItem::VerticalLimits::Prefix()
{
    return m_prefix;
}

void
LineTrackItem::VerticalLimits::SetValue(double value)
{
    m_default_value     = value;
    m_formatted_default = FormatValue(value);
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
