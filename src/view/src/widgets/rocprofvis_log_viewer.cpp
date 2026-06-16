// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_log_viewer.h"

#include "icons/rocprovfis_icon_defines.h"
#include "imgui.h"
#include "rocprofvis_core.h"
#include "rocprofvis_gui_helpers.h"
#include "rocprofvis_notification_manager.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <functional>
#include <regex>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

constexpr float LOG_VIEWER_DEFAULT_WIDTH      = 720.0f;
constexpr float LOG_VIEWER_DEFAULT_HEIGHT     = 420.0f;
constexpr float LOG_VIEWER_MIN_WIDTH          = 360.0f;
constexpr float LOG_VIEWER_MIN_HEIGHT         = 200.0f;
constexpr float LOG_VIEWER_TIME_COLUMN_WIDTH  = 130.0f;
constexpr float LOG_VIEWER_LEVEL_COLUMN_WIDTH = 80.0f;
constexpr float LOG_VIEWER_SEARCH_WIDTH       = 240.0f;
constexpr float LOG_VIEWER_LEVEL_COMBO_WIDTH  = 160.0f;
constexpr int   LOG_VIEWER_TABLE_COLUMN_COUNT = 3;

constexpr float LOG_VIEWER_DIM_ALPHA              = 0.4f;
constexpr float LOG_VIEWER_SWATCH_SCALE           = 0.6f;
constexpr float LOG_VIEWER_SWATCH_ROUNDING        = 2.0f;
constexpr float LOG_VIEWER_SWATCH_VERTICAL_CENTER = 0.5f;

constexpr float LOG_VIEWER_SCROLL_BOTTOM_EPSILON = 1.0f;
constexpr float LOG_VIEWER_JUMP_BUTTON_INSET     = 12.0f;
constexpr float LOG_VIEWER_SCROLL_TO_BOTTOM_Y    = 1.0f;

constexpr size_t LOG_VIEWER_CLIPBOARD_RESERVE_PER_LINE = 64;
constexpr int    LOG_VIEWER_RELATIVE_TIME_BUFFER       = 32;
constexpr int    LOG_VIEWER_RELATIVE_TIME_DECIMALS     = 3;

// Severity used when a reported level falls outside the known range.
constexpr int LOG_VIEWER_LEVEL_INFO_INDEX = 2;

#ifdef NDEBUG
constexpr int LOG_VIEWER_MIN_VISIBLE_LEVEL = LOG_VIEWER_LEVEL_INFO_INDEX;
#else
constexpr int LOG_VIEWER_MIN_VISIBLE_LEVEL = 0;
#endif

constexpr const char* LEVEL_LABELS[] = { "Trace", "Debug", "Info", "Warning", "Error",
                                         "Critical" };
constexpr const char* MIN_LEVEL_LABELS[] = { "All levels", "Debug & up", "Info & up",
                                             "Warning & up", "Error & up", "Critical only" };
constexpr Colors LEVEL_COLORS[] = { Colors::kLogTrace, Colors::kLogDebug, Colors::kLogInfo,
                                    Colors::kLogWarning, Colors::kLogError,
                                    Colors::kLogCritical };

static int
clamped_max_entries_from_settings()
{
    return std::clamp(
        SettingsManager::GetInstance().GetUserSettings().log_viewer_max_entries,
        LOG_VIEWER_MAX_ENTRIES_MIN, LOG_VIEWER_MAX_ENTRIES_MAX);
}

static std::string
to_lower(const std::string& input)
{
    std::string out = input;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

static bool
icontains(const std::string& haystack, const std::string& needle_lower)
{
    if(needle_lower.empty())
    {
        return true;
    }
    return std::search(haystack.begin(), haystack.end(), needle_lower.begin(),
                       needle_lower.end(),
                       [](char hay, char needle) {
                           return static_cast<char>(std::tolower(
                                      static_cast<unsigned char>(hay))) == needle;
                       }) != haystack.end();
}

// Returns true when haystack matches the active search mode; optionally fills
// the first match span for single-line highlight rendering.
static bool
text_matches_search(const std::string& haystack, bool has_search, bool use_regex,
                    const std::string& search_lower, const std::regex& regex, bool regex_ok,
                    size_t* match_start, size_t* match_len)
{
    if(!has_search)
    {
        return true;
    }
    if(use_regex)
    {
        if(!regex_ok)
        {
            return true;
        }
        std::smatch match;
        if(!std::regex_search(haystack, match, regex) || match.empty())
        {
            return false;
        }
        if(match_start && match_len && haystack.find('\n') == std::string::npos)
        {
            *match_start = static_cast<size_t>(match.position(0));
            *match_len   = static_cast<size_t>(match.length(0));
        }
        return true;
    }

    if(!icontains(haystack, search_lower))
    {
        return false;
    }

    if(match_start && match_len && haystack.find('\n') == std::string::npos)
    {
        std::string lowered = to_lower(haystack);
        size_t      pos     = lowered.find(search_lower);
        if(pos != std::string::npos)
        {
            *match_start = pos;
            *match_len   = search_lower.size();
        }
    }
    return true;
}

LogViewer* LogViewer::s_instance = nullptr;

LogViewer*
LogViewer::GetInstance()
{
    if(!s_instance)
    {
        s_instance = new LogViewer();
    }
    return s_instance;
}

void
LogViewer::DestroyInstance()
{
    delete s_instance;
    s_instance = nullptr;
}

LogViewer::LogViewer()
: m_entries(static_cast<size_t>(clamped_max_entries_from_settings()))
, m_cache_capacity(clamped_max_entries_from_settings())
, m_visible(false)
, m_auto_scroll(true)
, m_use_regex(false)
, m_relative_time(false)
, m_paused(false)
, m_regex_error(false)
, m_scrolled_up(false)
, m_request_scroll_bottom(false)
, m_counts_dirty(true)
, m_filter_dirty(true)
, m_applied_level_mask(0)
, m_applied_use_regex(false)
, m_regex_ok(true)
, m_start_time(std::chrono::steady_clock::now())
{
    m_widget_name = GenUniqueName("LogViewer");
    m_level_enabled.fill(true);
    m_level_counts.fill(0);
    m_search_buffer.fill('\0');
    LoadUiState();
}

const char*
LogViewer::LevelLabel(int level)
{
    if(level >= 0 && level < LEVEL_COUNT)
    {
        return LEVEL_LABELS[level];
    }
    return LEVEL_LABELS[LOG_VIEWER_LEVEL_INFO_INDEX];
}

Colors
LogViewer::LevelColor(int level)
{
    if(level >= 0 && level < LEVEL_COUNT)
    {
        return LEVEL_COLORS[level];
    }
    return LEVEL_COLORS[LOG_VIEWER_LEVEL_INFO_INDEX];
}

void
LogViewer::LoadUiState()
{
    const LogViewerSettings& lv = SettingsManager::GetInstance().GetUserSettings().log_viewer;
    for(int i = 0; i < LEVEL_COUNT; ++i)
    {
        m_level_enabled[i] = ((lv.level_mask >> i) & 1) != 0;
    }
    for(int i = 0; i < LOG_VIEWER_MIN_VISIBLE_LEVEL && i < LEVEL_COUNT; ++i)
    {
        m_level_enabled[i] = false;
    }
    m_auto_scroll   = lv.auto_scroll;
    m_use_regex     = lv.use_regex;
    m_relative_time = lv.relative_time;
    m_visible       = lv.visible;
}

void
LogViewer::SaveUiState() const
{
    LogViewerSettings& lv   = SettingsManager::GetInstance().GetUserSettings().log_viewer;
    const int          mask = LevelMask();
    if(lv.level_mask == mask && lv.auto_scroll == m_auto_scroll &&
       lv.use_regex == m_use_regex && lv.relative_time == m_relative_time &&
       lv.visible == m_visible)
    {
        return;
    }
    lv.level_mask    = mask;
    lv.auto_scroll   = m_auto_scroll;
    lv.use_regex     = m_use_regex;
    lv.relative_time = m_relative_time;
    lv.visible       = m_visible;
}

void
LogViewer::SyncCacheCapacity()
{
    int desired = clamped_max_entries_from_settings();
    if(desired != m_cache_capacity)
    {
        // Resize reallocates the ring buffer, invalidating the pointers held
        // in m_filtered, so the filtered view must be rebuilt.
        m_entries.Resize(static_cast<size_t>(desired));
        m_cache_capacity = desired;
        MarkDataDirty();
    }
}

void
LogViewer::MarkDataDirty()
{
    m_counts_dirty = true;
    m_filter_dirty = true;
}

int
LogViewer::LevelMask() const
{
    int mask = 0;
    for(int i = 0; i < LEVEL_COUNT; ++i)
    {
        if(m_level_enabled[i])
        {
            mask |= (1 << i);
        }
    }
    return mask;
}

void
LogViewer::Poll()
{
    SyncCacheCapacity();
    if(!m_paused)
    {
        rocprofvis_core_get_log_entries_ex(this, &LogViewer::ProcessEntry);
    }
    SaveUiState();
}

void
LogViewer::ProcessEntry(void* user_ptr, int level, char const* timestamp,
                        char const* message)
{
    LogViewer* self = static_cast<LogViewer*>(user_ptr);
    if(self && message)
    {
        self->AddEntry(level, timestamp ? timestamp : "", message);
    }
}

void
LogViewer::AddEntry(int level, const char* timestamp, const char* message)
{
    int clamped_level = std::clamp(level, 0, LEVEL_COUNT - 1);

    std::string text = message;
    while(!text.empty() && (text.back() == '\n' || text.back() == '\r'))
    {
        text.pop_back();
    }

    double relative_s =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - m_start_time)
            .count();

    m_entries.Push(LogEntry{ clamped_level, timestamp, std::move(text), relative_s });
    MarkDataDirty();
}

bool*
LogViewer::VisiblePtr()
{
    return &m_visible;
}

void
LogViewer::OpenLogFile()
{
    const char* path = rocprofvis_core_get_log_path();
    if(path && path[0] != '\0')
    {
        if(!open_url(path))
        {
            spdlog::warn("Failed to open log file: {}", path);
        }
    }
}

void
LogViewer::RecomputeCounts()
{
    m_level_counts.fill(0);
    for(const LogEntry& entry : m_entries)
    {
        ++m_level_counts[entry.level];
    }
    m_counts_dirty = false;
}

int
LogViewer::CurrentMinLevelPreset() const
{
    int floor_level = -1;
    for(int i = LOG_VIEWER_MIN_VISIBLE_LEVEL; i < LEVEL_COUNT; ++i)
    {
        if(m_level_enabled[i])
        {
            floor_level = i;
            break;
        }
    }
    if(floor_level < 0)
    {
        return -1;
    }
    for(int i = LOG_VIEWER_MIN_VISIBLE_LEVEL; i < LEVEL_COUNT; ++i)
    {
        if(m_level_enabled[i] != (i >= floor_level))
        {
            return -1;
        }
    }
    return floor_level;
}

void
LogViewer::ApplyMinLevelPreset(int floor_level)
{
    for(int i = 0; i < LEVEL_COUNT; ++i)
    {
        m_level_enabled[i] = (i >= floor_level && i >= LOG_VIEWER_MIN_VISIBLE_LEVEL);
    }
}

std::string
LogViewer::FormatLine(const LogEntry& entry) const
{
    std::string time_str =
        m_relative_time ? RelativeTimeString(entry.relative_s) : entry.timestamp;
    return time_str + "\t[" + LevelLabel(entry.level) + "]\t" + entry.text;
}

std::string
LogViewer::RelativeTimeString(double relative_s) const
{
    char buffer[LOG_VIEWER_RELATIVE_TIME_BUFFER];
    std::snprintf(buffer, sizeof(buffer), "+%.*fs", LOG_VIEWER_RELATIVE_TIME_DECIMALS,
                  relative_s);
    return buffer;
}

void
LogViewer::CopyVisibleToClipboard() const
{
    std::string out;
    out.reserve(m_filtered.size() * LOG_VIEWER_CLIPBOARD_RESERVE_PER_LINE);
    for(const LogEntry* entry : m_filtered)
    {
        out += FormatLine(*entry);
        out += '\n';
    }
    ImGui::SetClipboardText(out.c_str());
    NotificationManager::GetInstance().Show("Copied " + std::to_string(m_filtered.size()) +
                                                " visible log lines",
                                            NotificationLevel::Info);
}

void
LogViewer::RenderLevelToggle(int level)
{
    SettingsManager& settings = SettingsManager::GetInstance();
    const bool       has_any  = m_level_counts[level] > 0;
    const float      alpha    = has_any ? 1.0f : LOG_VIEWER_DIM_ALPHA;

    const ImVec2 origin     = ImGui::GetCursorScreenPos();
    const float  swatch     = ImGui::GetFontSize() * LOG_VIEWER_SWATCH_SCALE;
    const float  row_height = ImGui::GetFrameHeight();
    const float  y_offset   = (row_height - swatch) * LOG_VIEWER_SWATCH_VERTICAL_CENTER;
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(origin.x, origin.y + y_offset),
        ImVec2(origin.x + swatch, origin.y + y_offset + swatch),
        ApplyAlpha(settings.GetColor(LevelColor(level)), alpha), LOG_VIEWER_SWATCH_ROUNDING);
    ImGui::Dummy(ImVec2(swatch, row_height));
    ImGui::SameLine();

    std::string label = std::string(LevelLabel(level)) + " (" +
                        std::to_string(m_level_counts[level]) + ")###log_lvl" +
                        std::to_string(level);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(ApplyAlpha(
                                             settings.GetColor(Colors::kTextMain), alpha)));
    ImGui::Checkbox(label.c_str(), &m_level_enabled[level]);
    ImGui::PopStyleColor();
}

void
LogViewer::RenderLevelsPopup()
{
    if(ImGui::Button("Levels"))
    {
        ImGui::OpenPopup("##log_levels_popup");
    }
    if(ImGui::BeginPopup("##log_levels_popup"))
    {
        int         preset  = CurrentMinLevelPreset();
        const char* preview = preset >= 0 ? MIN_LEVEL_LABELS[preset] : "Custom";
        ImGui::SetNextItemWidth(LOG_VIEWER_LEVEL_COMBO_WIDTH);
        PushComboStyles();
        if(ImGui::BeginCombo("##log_min_level", preview))
        {
            for(int i = LOG_VIEWER_MIN_VISIBLE_LEVEL; i < LEVEL_COUNT; ++i)
            {
                if(ImGui::Selectable(MIN_LEVEL_LABELS[i], preset == i))
                {
                    ApplyMinLevelPreset(i);
                }
            }
            ImGui::EndCombo();
        }
        PopComboStyles();

        ImGui::Separator();
        for(int level = LOG_VIEWER_MIN_VISIBLE_LEVEL; level < LEVEL_COUNT; ++level)
        {
            RenderLevelToggle(level);
        }
        ImGui::EndPopup();
    }
}

void
LogViewer::RenderOptionsPopup()
{
    SettingsManager& settings = SettingsManager::GetInstance();
    FontManager&     fonts    = settings.GetFontManager();

    ImGui::PushFont(fonts.GetFont(FontType::kIcon), ImGui::GetFontSize());
    bool open_options = ImGui::Button(ICON_GEAR);
    ImGui::PopFont();
    if(ImGui::IsItemHovered())
    {
        SetTooltipStyled("Options");
    }
    if(open_options)
    {
        ImGui::OpenPopup("##log_options_popup");
    }
    if(ImGui::BeginPopup("##log_options_popup"))
    {
        ImGui::MenuItem("Auto-scroll", nullptr, &m_auto_scroll);
        ImGui::MenuItem("Relative time", nullptr, &m_relative_time);
        ImGui::MenuItem("Use regex filter", nullptr, &m_use_regex);
        ImGui::Separator();
        if(IconMenuItem(ICON_TRASH_CAN, "Clear"))
        {
            m_entries.Clear();
            MarkDataDirty();
        }
        if(IconMenuItem(ICON_OPEN, "Open log file"))
        {
            OpenLogFile();
        }
        ImGui::EndPopup();
    }
}

void
LogViewer::RenderNav()
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = ImGui::GetStyle();

    struct NavItem
    {
        float                 width;
        std::function<void()> render;
    };
    std::vector<NavItem> items;
    items.reserve(4);

    const auto button_width = [&](const char* label) {
        return ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f;
    };

    items.push_back({ button_width("Levels"), [this]() { RenderLevelsPopup(); } });

    items.push_back({ LOG_VIEWER_SEARCH_WIDTH, [this, &settings]() {
                         if(m_regex_error)
                         {
                             ImGui::PushStyleColor(ImGuiCol_FrameBg,
                                                   settings.GetColor(Colors::kBgError));
                         }
                         ImGui::SetNextItemWidth(LOG_VIEWER_SEARCH_WIDTH);
                         ImGui::InputTextWithHint("##log_search",
                                                  m_use_regex ? "Regex filter..."
                                                              : "Filter messages...",
                                                  m_search_buffer.data(),
                                                  m_search_buffer.size());
                         if(m_regex_error)
                         {
                             ImGui::PopStyleColor();
                         }
                     } });

    const float pause_width =
        std::max(button_width("Pause"), button_width("Resume"));
    const char* pause_label = m_paused ? "Resume" : "Pause";
    items.push_back({ pause_width, [this, pause_label]() {
                         if(ImGui::Button(pause_label))
                         {
                             m_paused = !m_paused;
                         }
                     } });

    items.push_back({ ImGui::GetFrameHeight(), [this]() { RenderOptionsPopup(); } });

    const float right_edge =
        ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    for(size_t i = 0; i < items.size(); ++i)
    {
        if(i > 0)
        {
            float next_end =
                ImGui::GetItemRectMax().x + style.ItemSpacing.x + items[i].width;
            if(next_end < right_edge)
            {
                ImGui::SameLine();
            }
        }
        items[i].render();
    }
}

void
LogViewer::RenderJumpToLatest()
{
    if(!(m_auto_scroll && m_scrolled_up))
    {
        return;
    }

    SettingsManager&  settings  = SettingsManager::GetInstance();
    const ImGuiStyle& style     = ImGui::GetStyle();
    const ImVec2      table_max = ImGui::GetItemRectMax();
    const char*       label     = "Jump to latest";
    const ImVec2      text_sz   = ImGui::CalcTextSize(label);
    const ImVec2      button_sz(text_sz.x + style.FramePadding.x * 2.0f,
                                text_sz.y + style.FramePadding.y * 2.0f);
    ImGui::SetCursorScreenPos(
        ImVec2(table_max.x - button_sz.x - LOG_VIEWER_JUMP_BUTTON_INSET,
               table_max.y - button_sz.y - LOG_VIEWER_JUMP_BUTTON_INSET));
    ImGui::PushStyleColor(ImGuiCol_Button, settings.GetColor(Colors::kAccent));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, settings.GetColor(Colors::kAccentHover));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, settings.GetColor(Colors::kAccentActive));
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextOnAccent));
    if(ImGui::Button(label))
    {
        m_request_scroll_bottom = true;
    }
    ImGui::PopStyleColor(4);
}

void
LogViewer::RebuildFilter()
{
    const std::string search_raw = m_search_buffer.data();
    const bool        has_search = !search_raw.empty();
    m_search_lower               = to_lower(search_raw);

    m_regex_ok = true;
    if(has_search && m_use_regex)
    {
        try
        {
            m_regex = std::regex(search_raw, std::regex::icase | std::regex::optimize);
        }
        catch(const std::regex_error&)
        {
            m_regex_ok = false;
        }
    }
    m_regex_error = has_search && m_use_regex && !m_regex_ok;

    m_filtered.clear();
    for(const LogEntry& entry : m_entries)
    {
        if(!m_level_enabled[entry.level])
        {
            continue;
        }
        if(!text_matches_search(entry.text, has_search, m_use_regex, m_search_lower, m_regex,
                                m_regex_ok, nullptr, nullptr))
        {
            continue;
        }
        m_filtered.push_back(&entry);
    }
}

void
LogViewer::RenderTable()
{
    SettingsManager& settings = SettingsManager::GetInstance();

    const std::string search_raw = m_search_buffer.data();
    const int         level_mask = LevelMask();
    if(m_filter_dirty || level_mask != m_applied_level_mask ||
       m_use_regex != m_applied_use_regex || search_raw != m_applied_search)
    {
        RebuildFilter();
        m_filter_dirty       = false;
        m_applied_level_mask = level_mask;
        m_applied_use_regex  = m_use_regex;
        m_applied_search     = search_raw;
    }

    const bool has_search = !search_raw.empty();

    constexpr ImGuiTableFlags table_flags =
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;

    const ImVec2 table_size = ImVec2(0.0f, ImGui::GetContentRegionAvail().y);

    if(ImGui::BeginTable("##log_viewer_table", LOG_VIEWER_TABLE_COLUMN_COUNT, table_flags,
                         table_size))
    {
        ImGui::TableSetupColumn(m_relative_time ? "Elapsed" : "Time",
                                ImGuiTableColumnFlags_WidthFixed,
                                LOG_VIEWER_TIME_COLUMN_WIDTH);
        ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed,
                                LOG_VIEWER_LEVEL_COLUMN_WIDTH);
        ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        const ImU32 text_main       = settings.GetColor(Colors::kTextMain);
        const ImU32 highlight_color = settings.GetColor(Colors::kEventSearchHighlight);
        const ImVec4 text_main_vec  = ImGui::ColorConvertU32ToFloat4(text_main);

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(m_filtered.size()));
        while(clipper.Step())
        {
            for(int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
            {
                const LogEntry* entry = m_filtered[row];
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::PushStyleColor(ImGuiCol_Text, text_main_vec);
                const std::string& time_str = m_relative_time
                                                  ? RelativeTimeString(entry->relative_s)
                                                  : entry->timestamp;
                ImGui::TextUnformatted(time_str.c_str());
                ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(1);
                ImGui::PushStyleColor(
                    ImGuiCol_Text,
                    ImGui::ColorConvertU32ToFloat4(settings.GetColor(LevelColor(entry->level))));
                ImGui::TextUnformatted(LevelLabel(entry->level));
                ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(2);
                ImGui::PushStyleColor(ImGuiCol_Text, text_main_vec);

                size_t match_start = std::string::npos;
                size_t match_len   = 0;
                if(has_search)
                {
                    text_matches_search(entry->text, has_search, m_use_regex, m_search_lower,
                                        m_regex, m_regex_ok, &match_start, &match_len);
                }

                if(match_start != std::string::npos && match_len > 0)
                {
                    ImVec2 pos = ImGui::GetCursorScreenPos();
                    float  x0  = pos.x +
                                ImGui::CalcTextSize(
                                    entry->text.substr(0, match_start).c_str())
                                    .x;
                    float x1 = pos.x +
                               ImGui::CalcTextSize(
                                   entry->text.substr(0, match_start + match_len).c_str())
                                   .x;
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        ImVec2(x0, pos.y),
                        ImVec2(x1, pos.y + ImGui::GetTextLineHeight()), highlight_color);
                }

                CopyableTextUnformatted(
                    entry->text.c_str(), std::to_string(row), COPY_DATA_NOTIFICATION,
                    false, true, [this, entry](const char* value) {
                        if(ImGui::BeginPopupContextItem())
                        {
                            if(IconMenuItem(ICON_COPY, "Copy message"))
                            {
                                ImGui::SetClipboardText(value);
                                NotificationManager::GetInstance().Show(
                                    "Message copied", NotificationLevel::Info);
                            }
                            if(IconMenuItem(ICON_COPY, "Copy line"))
                            {
                                ImGui::SetClipboardText(FormatLine(*entry).c_str());
                                NotificationManager::GetInstance().Show(
                                    "Log line copied", NotificationLevel::Info);
                            }
                            if(IconMenuItem(ICON_LIST, "Copy all visible"))
                            {
                                CopyVisibleToClipboard();
                            }
                            ImGui::EndPopup();
                        }
                    });
                ImGui::PopStyleColor();
            }
        }

        m_scrolled_up = (ImGui::GetScrollMaxY() - ImGui::GetScrollY()) >
                        LOG_VIEWER_SCROLL_BOTTOM_EPSILON;
        if(m_request_scroll_bottom || (m_auto_scroll && !m_scrolled_up))
        {
            ImGui::SetScrollHereY(LOG_VIEWER_SCROLL_TO_BOTTOM_Y);
            m_scrolled_up = false;
        }
        m_request_scroll_bottom = false;

        ImGui::EndTable();
    }

    RenderJumpToLatest();
}

void
LogViewer::Render()
{
    if(!m_visible)
    {
        return;
    }

    if(m_counts_dirty)
    {
        RecomputeCounts();
    }

    ImGui::SetNextWindowSize(
        ImVec2(LOG_VIEWER_DEFAULT_WIDTH, LOG_VIEWER_DEFAULT_HEIGHT),
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(LOG_VIEWER_MIN_WIDTH, LOG_VIEWER_MIN_HEIGHT), ImVec2(FLT_MAX, FLT_MAX));
    if(ImGui::Begin("Log Viewer", &m_visible))
    {
        RenderNav();
        ImGui::Separator();
        RenderTable();
    }
    ImGui::End();
}

}  // namespace View
}  // namespace RocProfVis
