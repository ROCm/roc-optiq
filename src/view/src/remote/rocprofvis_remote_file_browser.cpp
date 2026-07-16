// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_remote_file_browser.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <ctime>

#include "imgui.h"

#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_gui_helpers.h"

namespace RocProfVis
{
namespace View
{

namespace
{
    constexpr const char* POPUP_ID          = "Remote File Browser";
    constexpr float       DEFAULT_WIDTH     = 900.0f;
    constexpr float       DEFAULT_HEIGHT    = 620.0f;
    constexpr float       MIN_WIDTH         = 560.0f;
    constexpr float       MIN_HEIGHT        = 380.0f;
    constexpr float       SIZE_COL_WIDTH    = 96.0f;
    constexpr float       TYPE_COL_WIDTH    = 96.0f;
    constexpr float       TIME_COL_WIDTH    = 150.0f;
    constexpr float       ACTION_BTN_WIDTH  = 110.0f;

    // Stable per-column ids so header-click sorting maps back to a SortColumn.
    constexpr ImGuiID COL_ID_NAME = 0;
    constexpr ImGuiID COL_ID_SIZE = 1;
    constexpr ImGuiID COL_ID_TYPE = 2;
    constexpr ImGuiID COL_ID_TIME = 3;

    // Formats a byte count as a compact human-readable size (e.g. "4.0 KiB").
    std::string format_file_size(uint64_t bytes)
    {
        constexpr const char* UNITS[] = { "B", "KiB", "MiB", "GiB", "TiB" };
        double size = static_cast<double>(bytes);
        int    unit = 0;
        while(size >= 1024.0 && unit < 4)
        {
            size /= 1024.0;
            unit++;
        }

        char buf[32];
        if(unit == 0)
        {
            std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
        }
        else
        {
            std::snprintf(buf, sizeof(buf), "%.1f %s", size, UNITS[unit]);
        }
        return std::string(buf);
    }

    // Formats a Unix epoch (seconds) as local "YYYY-MM-DD HH:MM"; "-" if zero.
    std::string format_file_time(uint64_t epoch_seconds)
    {
        if(epoch_seconds == 0)
        {
            return "-";
        }

        std::time_t t = static_cast<std::time_t>(epoch_seconds);
        std::tm     tm_buf{};
#ifdef _WIN32
        if(localtime_s(&tm_buf, &t) != 0)
        {
            return "-";
        }
#else
        if(localtime_r(&t, &tm_buf) == nullptr)
        {
            return "-";
        }
#endif
        char buf[32];
        if(std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm_buf) == 0)
        {
            return "-";
        }
        return std::string(buf);
    }

    // The trailing path component (basename) of a POSIX path.
    std::string base_name(const std::string& path)
    {
        std::string::size_type pos = path.find_last_of('/');
        return pos == std::string::npos ? path : path.substr(pos + 1);
    }

    // The extension (without dot) used for the "Type" column / sort key.
    std::string file_extension(const std::string& path)
    {
        std::string name = base_name(path);
        std::string::size_type pos = name.find_last_of('.');
        if(pos == std::string::npos || pos == 0)
        {
            return std::string();
        }
        return name.substr(pos + 1);
    }

    std::string to_lower(const std::string& s)
    {
        std::string out = s;
        std::transform(out.begin(), out.end(), out.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return out;
    }

    // Case-insensitive lexical compare returning <0, 0, or >0.
    int casecmp(const std::string& a, const std::string& b)
    {
        return to_lower(a).compare(to_lower(b));
    }

    std::string type_label(const RemoteDir::FileEntry& entry)
    {
        if(entry.is_dir)
        {
            return "Folder";
        }
        std::string ext = file_extension(entry.name);
        if(ext.empty())
        {
            return "File";
        }
        return to_lower(ext) + " file";
    }

    // Draws a small folder glyph (filled) into dl anchored at top-left tl.
    void draw_folder_icon(ImDrawList* dl, ImVec2 tl, float size, ImU32 color)
    {
        float tab_h = size * 0.22f;
        float body_top = tl.y + tab_h * 0.6f;
        float rounding = size * 0.10f;
        // Folder tab.
        dl->AddRectFilled(ImVec2(tl.x, tl.y + size * 0.10f),
            ImVec2(tl.x + size * 0.5f, body_top + size * 0.12f), color, rounding);
        // Folder body.
        dl->AddRectFilled(ImVec2(tl.x, body_top),
            ImVec2(tl.x + size, tl.y + size * 0.9f), color, rounding);
    }

    // Draws a small document glyph (outline + text lines) into dl.
    void draw_file_icon(ImDrawList* dl, ImVec2 tl, float size, ImU32 color)
    {
        float x0 = tl.x + size * 0.16f;
        float x1 = tl.x + size * 0.84f;
        float y0 = tl.y + size * 0.08f;
        float y1 = tl.y + size * 0.92f;
        float rounding = size * 0.08f;
        dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), color, rounding, 0, 1.5f);
        float lx0 = x0 + size * 0.14f;
        float lx1 = x1 - size * 0.14f;
        dl->AddLine(ImVec2(lx0, y0 + size * 0.28f), ImVec2(lx1, y0 + size * 0.28f), color, 1.0f);
        dl->AddLine(ImVec2(lx0, y0 + size * 0.46f), ImVec2(lx1, y0 + size * 0.46f), color, 1.0f);
        dl->AddLine(ImVec2(lx0, y0 + size * 0.64f), ImVec2(lx1, y0 + size * 0.64f), color, 1.0f);
    }

    // Extension presets for the "type filter" dropdown. Directories are always
    // shown regardless of the active preset.
    struct TypeFilterPreset
    {
        const char*              label;
        std::vector<std::string> extensions;  // empty => match every file
    };

    const std::vector<TypeFilterPreset>& type_filter_presets()
    {
        static const std::vector<TypeFilterPreset> presets = {
            { "All files", {} },
            { "Trace databases (*.db, *.rpd)", { "db", "rpd" } },
            { "Project files (*.rpv)", { "rpv" } },
            { "Traces & projects (*.db, *.rpd, *.rpv)", { "db", "rpd", "rpv" } },
        };
        return presets;
    }
}  // namespace

RemoteFileBrowser::RemoteFileBrowser()
: m_callbacks()
, m_open(false)
, m_should_open_popup(false)
, m_busy(false)
, m_error()
, m_current_dir()
, m_entries()
, m_search_entries()
, m_in_search_mode(false)
, m_search_root()
, m_visible()
, m_selected(SELECTION_NONE)
, m_back_stack()
, m_forward_stack()
, m_filter()
, m_path_edit()
, m_show_hidden(false)
, m_searching(false)
, m_type_filter(0)
, m_selected_name()
, m_sort_column(SortColumn::Name)
, m_sort_ascending(true)
, m_needs_rebuild(false)
, m_scroll_to_selected(false)
{
}

void
RemoteFileBrowser::SetCallbacks(Callbacks callbacks)
{
    m_callbacks = std::move(callbacks);
}

void
RemoteFileBrowser::Open(const std::string& initial_dir)
{
    m_open              = true;
    m_should_open_popup = true;
    m_error.clear();
    m_in_search_mode = false;
    m_searching      = false;
    m_search_entries.clear();
    m_entries.clear();
    m_visible.clear();
    m_filter.clear();
    m_back_stack.clear();
    m_forward_stack.clear();
    m_selected = SELECTION_NONE;
    m_selected_name.clear();

    NavigateTo(initial_dir.empty() ? std::string(".") : initial_dir, false);
}

void
RemoteFileBrowser::Close()
{
    m_open              = false;
    m_should_open_popup = false;
    m_busy              = false;
}

void
RemoteFileBrowser::SetListing(const std::string& dir, const RemoteDir::Snapshot& snapshot)
{
    m_entries        = snapshot.list_dir;
    m_current_dir    = NormalizePath(dir);
    m_path_edit      = m_current_dir;
    m_in_search_mode = false;
    m_busy           = false;
    m_searching      = false;
    m_error.clear();
    // Selection is re-resolved from m_selected_name during rebuild so a Refresh
    // keeps the highlighted row; a fresh navigation cleared m_selected_name.
    m_selected      = SELECTION_NONE;
    m_needs_rebuild = true;
}

void
RemoteFileBrowser::SetSearchResults(std::vector<RemoteDir::FileEntry> results)
{
    m_search_entries = std::move(results);
    m_in_search_mode = true;
    m_busy           = false;
    m_searching      = false;
    m_error.clear();
    m_selected       = SELECTION_NONE;
    m_selected_name.clear();
    m_needs_rebuild  = true;
}

const std::vector<RemoteDir::FileEntry>&
RemoteFileBrowser::ActiveSource() const
{
    return m_in_search_mode ? m_search_entries : m_entries;
}

void
RemoteFileBrowser::NavigateTo(const std::string& path, bool record_history)
{
    std::string target = NormalizePath(path);

    if(record_history && !m_current_dir.empty() && target != m_current_dir)
    {
        m_back_stack.push_back(m_current_dir);
        m_forward_stack.clear();
    }

    m_current_dir    = target;
    m_path_edit      = target;
    m_in_search_mode = false;
    m_searching      = false;
    m_selected       = SELECTION_NONE;
    m_selected_name.clear();
    m_error.clear();
    m_busy           = true;
    m_needs_rebuild  = true;

    if(m_callbacks.request_listing)
    {
        m_callbacks.request_listing(target);
    }
}

void
RemoteFileBrowser::NavigateUp()
{
    if(IsRoot(m_current_dir))
    {
        return;
    }
    NavigateTo(ParentPath(m_current_dir), true);
}

void
RemoteFileBrowser::NavigateBack()
{
    if(m_back_stack.empty())
    {
        return;
    }
    m_forward_stack.push_back(m_current_dir);
    std::string target = m_back_stack.back();
    m_back_stack.pop_back();
    NavigateTo(target, false);
}

void
RemoteFileBrowser::NavigateForward()
{
    if(m_forward_stack.empty())
    {
        return;
    }
    m_back_stack.push_back(m_current_dir);
    std::string target = m_forward_stack.back();
    m_forward_stack.pop_back();
    NavigateTo(target, false);
}

void
RemoteFileBrowser::NavigateHome()
{
    NavigateTo(".", true);
}

void
RemoteFileBrowser::Refresh()
{
    if(m_in_search_mode)
    {
        RunSearch();
        return;
    }
    m_busy = true;
    if(m_callbacks.request_listing)
    {
        m_callbacks.request_listing(m_current_dir);
    }
}

std::string
RemoteFileBrowser::EntryFullPath(const RemoteDir::FileEntry& entry) const
{
    // In search mode the entry name is a path (relative to the search root or
    // absolute); in normal mode it is a plain name under the current directory.
    if(m_in_search_mode)
    {
        return (!entry.name.empty() && entry.name[0] == '/')
                   ? NormalizePath(entry.name)
                   : JoinPath(m_search_root, entry.name);
    }
    return JoinPath(m_current_dir, entry.name);
}

void
RemoteFileBrowser::SetSelected(int selection)
{
    m_selected = selection;
    if(selection >= 0 && selection < static_cast<int>(m_visible.size()))
    {
        m_selected_name = ActiveSource()[m_visible[selection]].name;
    }
    else
    {
        m_selected_name.clear();
    }
}

void
RemoteFileBrowser::ActivateVisible(int visible_index)
{
    if(visible_index < 0 || visible_index >= static_cast<int>(m_visible.size()))
    {
        return;
    }

    const RemoteDir::FileEntry& entry = ActiveSource()[m_visible[visible_index]];
    std::string                 full_path = EntryFullPath(entry);

    if(entry.is_dir)
    {
        NavigateTo(full_path, true);
    }
    else if(m_callbacks.on_file_chosen)
    {
        m_callbacks.on_file_chosen(full_path);
        Close();
    }
}

void
RemoteFileBrowser::CommitSelection()
{
    if(m_selected == SELECTION_PARENT)
    {
        NavigateUp();
    }
    else if(m_selected >= 0)
    {
        ActivateVisible(m_selected);
    }
}

void
RemoteFileBrowser::ClearSearch()
{
    m_in_search_mode = false;
    m_search_entries.clear();
    m_filter.clear();
    m_selected      = SELECTION_NONE;
    m_needs_rebuild = true;
}

void
RemoteFileBrowser::RunSearch()
{
    if(m_filter.empty() || !m_callbacks.request_search)
    {
        return;
    }
    m_search_root = m_current_dir;
    m_busy        = true;
    m_searching   = true;
    m_selected    = SELECTION_NONE;
    m_selected_name.clear();
    m_callbacks.request_search(m_filter, m_current_dir);
}

bool
RemoteFileBrowser::PassesTypeFilter(const RemoteDir::FileEntry& entry) const
{
    // Directories are always shown so the user can navigate anywhere.
    if(entry.is_dir)
    {
        return true;
    }

    const std::vector<TypeFilterPreset>& presets = type_filter_presets();
    if(m_type_filter <= 0 || m_type_filter >= static_cast<int>(presets.size()))
    {
        return true;
    }

    const std::vector<std::string>& exts = presets[m_type_filter].extensions;
    if(exts.empty())
    {
        return true;
    }

    std::string ext = to_lower(file_extension(entry.name));
    for(const std::string& allowed : exts)
    {
        if(ext == allowed)
        {
            return true;
        }
    }
    return false;
}

void
RemoteFileBrowser::RebuildVisible()
{
    m_visible.clear();

    const std::vector<RemoteDir::FileEntry>& src = ActiveSource();
    std::string needle = to_lower(m_filter);

    for(size_t i = 0; i < src.size(); i++)
    {
        const RemoteDir::FileEntry& entry = src[i];
        std::string name = base_name(entry.name);

        if(!m_show_hidden && !name.empty() && name[0] == '.')
        {
            continue;
        }

        if(!PassesTypeFilter(entry))
        {
            continue;
        }

        // The filter narrows the visible rows live in both directory and
        // search-result modes. Search results match on the whole path so the
        // user can also narrow by folder.
        if(!needle.empty())
        {
            const std::string& haystack = m_in_search_mode ? entry.name : name;
            if(to_lower(haystack).find(needle) == std::string::npos)
            {
                continue;
            }
        }

        m_visible.push_back(i);
    }

    ApplySort();

    // Re-resolve the remembered selection by name so it survives a rebuild
    // (sort change, filter change, or refresh of the same directory).
    m_selected = SELECTION_NONE;
    if(!m_selected_name.empty())
    {
        for(size_t vi = 0; vi < m_visible.size(); vi++)
        {
            if(src[m_visible[vi]].name == m_selected_name)
            {
                m_selected = static_cast<int>(vi);
                break;
            }
        }
    }
}

void
RemoteFileBrowser::ApplySort()
{
    const std::vector<RemoteDir::FileEntry>& src = ActiveSource();
    SortColumn column     = m_sort_column;
    bool       ascending  = m_sort_ascending;

    std::sort(m_visible.begin(), m_visible.end(),
        [&src, column, ascending](size_t ia, size_t ib)
        {
            const RemoteDir::FileEntry& a = src[ia];
            const RemoteDir::FileEntry& b = src[ib];

            // Folders always precede files, independent of sort direction.
            if(a.is_dir != b.is_dir)
            {
                return a.is_dir;
            }

            int cmp = 0;
            switch(column)
            {
                case SortColumn::Size:
                    cmp = (a.size < b.size) ? -1 : (a.size > b.size ? 1 : 0);
                    break;
                case SortColumn::Type:
                    cmp = casecmp(file_extension(a.name), file_extension(b.name));
                    break;
                case SortColumn::Modified:
                    cmp = (a.time < b.time) ? -1 : (a.time > b.time ? 1 : 0);
                    break;
                case SortColumn::Name:
                default:
                    break;
            }
            if(cmp == 0)
            {
                cmp = casecmp(base_name(a.name), base_name(b.name));
            }
            return ascending ? (cmp < 0) : (cmp > 0);
        });
}

void
RemoteFileBrowser::HandleKeyboard()
{
    // Suppress list navigation while the user types in the address/filter fields.
    if(ImGui::IsAnyItemActive())
    {
        return;
    }
    if(!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    {
        return;
    }

    const int visible_count = static_cast<int>(m_visible.size());

    if(ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))
    {
        CommitSelection();
        return;
    }
    if(ImGui::IsKeyPressed(ImGuiKey_Backspace))
    {
        NavigateUp();
        return;
    }

    if(visible_count <= 0)
    {
        return;
    }

    constexpr int PAGE_STEP = 10;
    int           next      = m_selected;

    if(ImGui::IsKeyPressed(ImGuiKey_DownArrow))
    {
        next = (m_selected < 0) ? 0 : std::min(m_selected + 1, visible_count - 1);
    }
    else if(ImGui::IsKeyPressed(ImGuiKey_UpArrow))
    {
        next = (m_selected <= 0) ? 0 : (m_selected - 1);
    }
    else if(ImGui::IsKeyPressed(ImGuiKey_PageDown))
    {
        next = (m_selected < 0) ? 0 : std::min(m_selected + PAGE_STEP, visible_count - 1);
    }
    else if(ImGui::IsKeyPressed(ImGuiKey_PageUp))
    {
        next = (m_selected <= 0) ? 0 : std::max(m_selected - PAGE_STEP, 0);
    }
    else if(ImGui::IsKeyPressed(ImGuiKey_Home))
    {
        next = 0;
    }
    else if(ImGui::IsKeyPressed(ImGuiKey_End))
    {
        next = visible_count - 1;
    }
    else
    {
        return;
    }

    if(next != m_selected)
    {
        SetSelected(next);
    }
    m_scroll_to_selected = true;
}

void
RemoteFileBrowser::Render()
{
    if(!m_open)
    {
        return;
    }

    SettingsManager& settings = SettingsManager::GetInstance();

    if(m_should_open_popup)
    {
        ImGui::OpenPopup(POPUP_ID);
        m_should_open_popup = false;
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(
        GetResponsiveWindowSize(ImVec2(DEFAULT_WIDTH, DEFAULT_HEIGHT), ImVec2(MIN_WIDTH, MIN_HEIGHT)),
        ImGuiCond_Appearing);

    bool keep_open = true;
    if(ImGui::BeginPopupModal(POPUP_ID, &keep_open, ImGuiWindowFlags_NoCollapse))
    {
        HandleKeyboard();

        RenderToolbar(settings);
        RenderAddressBar(settings);
        RenderSearchRow(settings);

        if(!m_error.empty())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", m_error.c_str());
        }

        // Reserve space for the footer (path/count line + a row of buttons).
        float footer_height = ImGui::GetFrameHeightWithSpacing() +
                              ImGui::GetTextLineHeightWithSpacing() +
                              ImGui::GetStyle().ItemSpacing.y * 2.0f +
                              ImGui::GetStyle().FramePadding.y * 2.0f;

        RenderTable(settings, -footer_height);
        RenderFooter(settings);

        ImGui::EndPopup();
    }

    if(!keep_open && m_open)
    {
        Close();
        if(m_callbacks.on_cancel)
        {
            m_callbacks.on_cancel();
        }
    }
}

void
RemoteFileBrowser::RenderToolbar(SettingsManager& settings)
{
    ImFont* icon_font = settings.GetFontManager().GetFont(FontType::kIcon);
    ImVec2  btn_size  = ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight());

    bool can_back    = !m_back_stack.empty();
    bool can_forward = !m_forward_stack.empty();
    bool can_up      = !m_in_search_mode && !IsRoot(m_current_dir);

    if(!can_back) { ImGui::BeginDisabled(); }
    if(IconButton(ICON_CHEVRON_LEFT, icon_font, btn_size, "Back", false))
    {
        NavigateBack();
    }
    if(!can_back) { ImGui::EndDisabled(); }
    ImGui::SameLine();

    if(!can_forward) { ImGui::BeginDisabled(); }
    if(IconButton(ICON_CHEVRON_RIGHT, icon_font, btn_size, "Forward", false))
    {
        NavigateForward();
    }
    if(!can_forward) { ImGui::EndDisabled(); }
    ImGui::SameLine();

    if(!can_up) { ImGui::BeginDisabled(); }
    if(IconButton(ICON_ARROW_UP, icon_font, btn_size, "Up one level", false))
    {
        NavigateUp();
    }
    if(!can_up) { ImGui::EndDisabled(); }
    ImGui::SameLine();

    if(IconButton(ICON_ARROWS_CYCLE, icon_font, btn_size, "Refresh", false))
    {
        Refresh();
    }
    ImGui::SameLine();

    if(IconButton(ICON_COMPASS, icon_font, btn_size, "Home", false))
    {
        NavigateHome();
    }
    ImGui::SameLine();

    if(m_busy)
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Loading...");
    }
}

void
RemoteFileBrowser::RenderAddressBar(SettingsManager& settings)
{
    ImFont* icon_font = settings.GetFontManager().GetFont(FontType::kIcon);

    // Editable address field: Enter navigates to the typed path.
    ImGui::SetNextItemWidth(-(ACTION_BTN_WIDTH * 0.6f));
    if(InputTextStringWithHint("##rfb_path", "/path/to/directory", m_path_edit,
           ImGuiInputTextFlags_EnterReturnsTrue))
    {
        NavigateTo(m_path_edit, true);
    }
    ImGui::SameLine();
    if(IconButton(ICON_ARROW_FORWARD, icon_font,
           ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()), "Go", false))
    {
        NavigateTo(m_path_edit, true);
    }

    // Clickable breadcrumb segments below the address field.
    ImU32 accent = settings.GetColor(Colors::kAccent);
    std::string dir     = NormalizePath(m_current_dir);
    bool        is_abs  = !dir.empty() && dir[0] == '/';

    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, accent);

    if(is_abs)
    {
        if(ImGui::SmallButton("/##crumb_root"))
        {
            NavigateTo("/", true);
        }
    }

    // Split the remaining path into segments and render a button per segment.
    std::string accumulated = is_abs ? "/" : "";
    int         crumb_index = 0;
    std::string working = dir;
    if(is_abs && !working.empty())
    {
        working.erase(0, 1);
    }

    std::string::size_type start = 0;
    while(start <= working.size())
    {
        std::string::size_type slash = working.find('/', start);
        std::string piece = (slash == std::string::npos)
                                ? working.substr(start)
                                : working.substr(start, slash - start);
        if(!piece.empty() && piece != ".")
        {
            if(accumulated.empty())
            {
                accumulated = piece;
            }
            else if(accumulated.back() == '/')
            {
                accumulated += piece;
            }
            else
            {
                accumulated += "/" + piece;
            }

            ImGui::SameLine(0.0f, 2.0f);
            ImGui::TextDisabled(">");
            ImGui::SameLine(0.0f, 2.0f);

            std::string label = piece + "##crumb" + std::to_string(crumb_index++);
            std::string crumb_target = accumulated;
            if(ImGui::SmallButton(label.c_str()))
            {
                NavigateTo(crumb_target, true);
            }
        }
        if(slash == std::string::npos)
        {
            break;
        }
        start = slash + 1;
    }

    ImGui::PopStyleColor(2);
}

void
RemoteFileBrowser::RenderSearchRow(SettingsManager& settings)
{
    // Live filter box: narrows the current view instantly in every mode.
    ImGui::SetNextItemWidth(300.0f);
    std::string previous_filter = m_filter;
    bool enter_pressed = InputTextStringWithHint("##rfb_filter", "Filter by name", m_filter,
        ImGuiInputTextFlags_EnterReturnsTrue);
    if(m_filter != previous_filter)
    {
        m_needs_rebuild = true;
    }

    // Clear-filter button (kept in the layout even when empty so nothing jumps).
    ImGui::SameLine();
    if(!m_filter.empty())
    {
        if(XButton("##rfb_filter_clear", "Clear filter", &settings))
        {
            m_filter.clear();
            m_needs_rebuild = true;
        }
    }
    else
    {
        ImGui::Dummy(ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
    }

    // Recursive "search subfolders" action (Enter in the filter box also runs it).
    bool can_search = !m_filter.empty();
    if(m_callbacks.request_search)
    {
        ImGui::SameLine();
        if(!can_search) { ImGui::BeginDisabled(); }
        bool search_clicked = ImGui::Button("Search subfolders");
        if(!can_search) { ImGui::EndDisabled(); }
        if((search_clicked || enter_pressed) && can_search)
        {
            RunSearch();
        }
    }

    // File-type filter dropdown.
    const std::vector<TypeFilterPreset>& presets = type_filter_presets();
    if(m_type_filter < 0 || m_type_filter >= static_cast<int>(presets.size()))
    {
        m_type_filter = 0;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(240.0f);
    PushComboStyles();
    if(ImGui::BeginCombo("##rfb_type", presets[m_type_filter].label))
    {
        for(int i = 0; i < static_cast<int>(presets.size()); i++)
        {
            bool is_selected = (m_type_filter == i);
            if(ImGui::Selectable(presets[i].label, is_selected))
            {
                m_type_filter   = i;
                m_needs_rebuild = true;
            }
            if(is_selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    PopComboStyles();

    ImGui::SameLine();
    if(ImGui::Checkbox("Show hidden", &m_show_hidden))
    {
        m_needs_rebuild = true;
    }

    // Search status banner.
    if(m_searching)
    {
        ImGui::TextDisabled("Searching \"%s\" in %s ...", m_filter.c_str(),
            m_search_root.c_str());
    }
    else if(m_in_search_mode)
    {
        ImGui::TextDisabled("%zu match%s in %s", m_search_entries.size(),
            m_search_entries.size() == 1 ? "" : "es", m_search_root.c_str());
        ImGui::SameLine();
        if(ImGui::SmallButton("Clear search"))
        {
            ClearSearch();
        }
    }
}

void
RemoteFileBrowser::RenderTable(SettingsManager& settings, float table_height)
{
    ImU32 folder_color = settings.GetColor(Colors::kAccent);

    const ImGuiTableFlags table_flags =
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable |
        ImGuiTableFlags_Sortable | ImGuiTableFlags_SortTristate;

    if(ImGui::BeginTable("##rfb_files", 4, table_flags, ImVec2(0.0f, table_height)))
    {
        ImGui::TableSetupColumn("Name",
            ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort, 0.0f,
            COL_ID_NAME);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, SIZE_COL_WIDTH,
            COL_ID_SIZE);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, TYPE_COL_WIDTH,
            COL_ID_TYPE);
        ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, TIME_COL_WIDTH,
            COL_ID_TIME);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        if(ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs())
        {
            if(sort_specs->SpecsDirty)
            {
                if(sort_specs->SpecsCount > 0)
                {
                    const ImGuiTableColumnSortSpecs& spec = sort_specs->Specs[0];
                    switch(spec.ColumnUserID)
                    {
                        case COL_ID_SIZE: m_sort_column = SortColumn::Size; break;
                        case COL_ID_TYPE: m_sort_column = SortColumn::Type; break;
                        case COL_ID_TIME: m_sort_column = SortColumn::Modified; break;
                        case COL_ID_NAME:
                        default:          m_sort_column = SortColumn::Name; break;
                    }
                    m_sort_ascending = (spec.SortDirection != ImGuiSortDirection_Descending);
                }
                m_needs_rebuild       = true;
                sort_specs->SpecsDirty = false;
            }
        }

        if(m_needs_rebuild)
        {
            RebuildVisible();
            m_needs_rebuild = false;
        }

        // Shared icon geometry: reserve a run of spaces at the start of each
        // name so the vector-drawn folder/file glyph has room to its left.
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        float       line_h    = ImGui::GetTextLineHeight();
        float       icon_sz   = line_h * 0.95f;
        float       space_w   = ImGui::CalcTextSize(" ").x;
        if(space_w <= 0.0f)
        {
            space_w = 4.0f;
        }
        int pad_spaces = static_cast<int>(std::ceil((icon_sz + 4.0f) / space_w));
        if(pad_spaces < 2)
        {
            pad_spaces = 2;
        }
        std::string pad(static_cast<size_t>(pad_spaces), ' ');
        ImU32 file_icon_color = ApplyAlpha(ImGui::GetColorU32(ImGuiCol_Text), 0.75f);

        // Synthetic ".." row to step up a level (hidden in search mode / at root).
        if(!m_in_search_mode && !IsRoot(m_current_dir))
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            bool   parent_selected = (m_selected == SELECTION_PARENT);
            ImVec2 row_pos         = ImGui::GetCursorScreenPos();
            ImGui::PushStyleColor(ImGuiCol_Text, folder_color);
            if(ImGui::Selectable((pad + "..##rfb_parent").c_str(), parent_selected,
                   ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
            {
                m_selected = SELECTION_PARENT;
                m_selected_name.clear();
                if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    NavigateUp();
                }
            }
            ImGui::PopStyleColor();
            draw_folder_icon(draw_list,
                ImVec2(row_pos.x, row_pos.y + (line_h - icon_sz) * 0.5f), icon_sz, folder_color);

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted("-");
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted("Parent folder");
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted("-");
        }

        const std::vector<RemoteDir::FileEntry>& src = ActiveSource();

        if(m_visible.empty() && !m_busy)
        {
            RenderEmptyState(settings);
        }

        for(size_t vi = 0; vi < m_visible.size(); vi++)
        {
            const RemoteDir::FileEntry& entry = src[m_visible[vi]];

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            bool        selected = (m_selected == static_cast<int>(vi));
            std::string display  = entry.is_dir ? (entry.name + "/") : entry.name;
            std::string label    = pad + display + "##rfb_row" + std::to_string(vi);
            ImVec2      row_pos   = ImGui::GetCursorScreenPos();

            if(entry.is_dir)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, folder_color);
            }
            bool clicked = ImGui::Selectable(label.c_str(), selected,
                ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick);
            if(entry.is_dir)
            {
                ImGui::PopStyleColor();
            }

            ImVec2 icon_tl(row_pos.x, row_pos.y + (line_h - icon_sz) * 0.5f);
            if(entry.is_dir)
            {
                draw_folder_icon(draw_list, icon_tl, icon_sz, folder_color);
            }
            else
            {
                draw_file_icon(draw_list, icon_tl, icon_sz, file_icon_color);
            }

            if(clicked)
            {
                SetSelected(static_cast<int>(vi));
                if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    ActivateVisible(static_cast<int>(vi));
                }
            }

            // Per-row right-click menu (Open / Copy path / Refresh).
            std::string ctx_id = "##rfb_ctx" + std::to_string(vi);
            if(ImGui::BeginPopupContextItem(ctx_id.c_str()))
            {
                SetSelected(static_cast<int>(vi));
                std::string full_path = EntryFullPath(entry);
                if(IconMenuItem(ICON_OPEN, entry.is_dir ? "Open folder" : "Open"))
                {
                    ActivateVisible(static_cast<int>(vi));
                }
                if(IconMenuItem(ICON_COPY, "Copy path"))
                {
                    ImGui::SetClipboardText(full_path.c_str());
                }
                ImGui::Separator();
                if(IconMenuItem(ICON_ARROWS_CYCLE, "Refresh"))
                {
                    Refresh();
                }
                ImGui::EndPopup();
            }

            if(m_scroll_to_selected && selected)
            {
                ImGui::SetScrollHereY();
            }

            if(ImGui::IsItemHovered() && !entry.name.empty())
            {
                SetTooltipStyled("%s", EntryFullPath(entry).c_str());
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(entry.is_dir ? "-" : format_file_size(entry.size).c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(type_label(entry).c_str());

            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(format_file_time(entry.time).c_str());
        }

        ImGui::EndTable();
    }

    m_scroll_to_selected = false;
}

void
RemoteFileBrowser::RenderEmptyState(SettingsManager& settings)
{
    (void)settings;

    std::string message;
    if(m_in_search_mode)
    {
        message = m_search_entries.empty()
                      ? ("No matches for \"" + m_filter + "\" in " + m_search_root)
                      : "No results match the current filter.";
    }
    else if(!m_filter.empty() || m_type_filter > 0)
    {
        message = "No items match the current filter.";
    }
    else
    {
        message = "This folder is empty.";
    }

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeight() * 0.5f));
    ImGui::TextDisabled("%s", message.c_str());
}

void
RemoteFileBrowser::RenderFooter(SettingsManager& settings)
{
    (void)settings;

    // Selected-path preview + item count.
    std::string selected_path;
    if(m_selected == SELECTION_PARENT)
    {
        selected_path = "..";
    }
    else if(m_selected >= 0 && m_selected < static_cast<int>(m_visible.size()))
    {
        const RemoteDir::FileEntry& entry = ActiveSource()[m_visible[m_selected]];
        selected_path                     = EntryFullPath(entry);
        if(!entry.is_dir)
        {
            selected_path += "   (" + format_file_size(entry.size) + ", " +
                             format_file_time(entry.time) + ")";
        }
    }

    ImGui::Separator();

    ImGui::AlignTextToFramePadding();
    if(selected_path.empty())
    {
        ImGui::TextDisabled("%zu item%s", m_visible.size(),
            m_visible.size() == 1 ? "" : "s");
    }
    else
    {
        ImGui::TextUnformatted("Selected:");
        ImGui::SameLine();
        ElidedText(selected_path.c_str(), ImGui::GetContentRegionAvail().x, 0.0f,
            Alignment_Left, true);
    }

    // Right-align the Open / Cancel buttons on their own line.
    float buttons_width = ACTION_BTN_WIDTH * 2.0f + ImGui::GetStyle().ItemSpacing.x;
    float avail         = ImGui::GetContentRegionAvail().x;
    if(avail > buttons_width)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - buttons_width));
    }

    bool can_open = (m_selected == SELECTION_PARENT) || (m_selected >= 0);
    if(!can_open) { ImGui::BeginDisabled(); }
    if(ImGui::Button("Open", ImVec2(ACTION_BTN_WIDTH, 0.0f)))
    {
        CommitSelection();
    }
    if(!can_open) { ImGui::EndDisabled(); }

    ImGui::SameLine();
    if(ImGui::Button("Cancel", ImVec2(ACTION_BTN_WIDTH, 0.0f)))
    {
        Close();
        if(m_callbacks.on_cancel)
        {
            m_callbacks.on_cancel();
        }
        ImGui::CloseCurrentPopup();
    }
}

std::string
RemoteFileBrowser::NormalizePath(const std::string& path)
{
    if(path.empty())
    {
        return ".";
    }

    bool                     absolute = (path[0] == '/');
    std::vector<std::string> parts;

    std::string::size_type start = 0;
    while(start <= path.size())
    {
        std::string::size_type slash = path.find('/', start);
        std::string segment = (slash == std::string::npos)
                                  ? path.substr(start)
                                  : path.substr(start, slash - start);

        if(!segment.empty() && segment != ".")
        {
            if(segment == "..")
            {
                if(!parts.empty() && parts.back() != "..")
                {
                    parts.pop_back();
                }
                else if(!absolute)
                {
                    parts.push_back("..");
                }
            }
            else
            {
                parts.push_back(segment);
            }
        }

        if(slash == std::string::npos)
        {
            break;
        }
        start = slash + 1;
    }

    std::string result = absolute ? "/" : "";
    for(size_t i = 0; i < parts.size(); i++)
    {
        result += parts[i];
        if(i + 1 < parts.size())
        {
            result += "/";
        }
    }

    if(result.empty())
    {
        result = absolute ? "/" : ".";
    }
    return result;
}

std::string
RemoteFileBrowser::JoinPath(const std::string& dir, const std::string& name)
{
    if(name == "..")
    {
        return ParentPath(dir);
    }
    if(!name.empty() && name[0] == '/')
    {
        return NormalizePath(name);
    }

    std::string joined = dir;
    if(joined.empty())
    {
        joined = name;
    }
    else if(joined.back() == '/')
    {
        joined += name;
    }
    else
    {
        joined += "/" + name;
    }
    return NormalizePath(joined);
}

std::string
RemoteFileBrowser::ParentPath(const std::string& dir)
{
    std::string normalized = NormalizePath(dir);
    if(IsRoot(normalized))
    {
        return normalized;
    }

    std::string::size_type pos = normalized.find_last_of('/');
    if(pos == std::string::npos)
    {
        return ".";
    }
    if(pos == 0)
    {
        return "/";
    }
    return normalized.substr(0, pos);
}

bool
RemoteFileBrowser::IsRoot(const std::string& dir)
{
    std::string normalized = NormalizePath(dir);
    return normalized == "/" || normalized == ".";
}

}  // namespace View
}  // namespace RocProfVis
