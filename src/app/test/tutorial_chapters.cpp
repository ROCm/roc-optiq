// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
#include "tutorial_chapters.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "ImGuiFileDialog.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "imgui_te_internal.h"

#include "compute/rocprofvis_compute_comparison.h"
#include "compute/rocprofvis_compute_kernel_details.h"
#include "compute/rocprofvis_compute_summary.h"
#include "compute/rocprofvis_compute_table_view.h"
#include "compute/rocprofvis_compute_view.h"
#include "compute/rocprofvis_compute_workload_view.h"
#include "icons/rocprovfis_icon_defines.h"
#include "model/rocprofvis_timeline_model.h"
#include "rocprofvis_analysis_view.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_event_search.h"
#include "rocprofvis_line_track_item.h"
#include "rocprofvis_measurement_controller.h"
#include "rocprofvis_project.h"
#include "rocprofvis_raw_track_data.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_time_to_pixel.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_timeline_view.h"
#include "rocprofvis_trace_view.h"
#include "rocprofvis_track_item.h"
#include "rocprofvis_view_module.h"
#include "rocprofvis_view_test_access.h"
#include "tutorial_recorder.h"
#include "widgets/rocprofvis_tab_container.h"

namespace RocProfVis
{
namespace Tutorial
{

using View::AnalysisView;
using View::AppWindow;
using View::AppWindowSettings;
using View::AppWindowTestPeer;
using View::ComputeView;
using View::DataProvider;
using View::FlameTrackItem;
using View::FlameTrackItemTestPeer;
using View::LineTrackItem;
using View::MeasurementController;
using View::Project;
using View::ProviderState;
using View::RawTrackSampleData;
using View::SettingsManager;
using View::TabContainer;
using View::TabItem;
using View::TimelineSelection;
using View::TimelineView;
using View::TimelineViewTestPeer;
using View::TimePixelTransform;
using View::TraceCounter;
using View::TraceEvent;
using View::TraceView;
using View::TraceViewTestPeer;
using View::TrackInfo;
using View::TrackItem;

using View::ComputeComparisonView;
using View::ComputeKernelDetailsView;
using View::ComputeSummaryView;
using View::ComputeTableView;
using View::ComputeViewTestPeer;
using View::ComputeWorkloadView;

using View::ICON_ADD_NOTE;
using View::ICON_ARROWS_CYCLE;
using View::ICON_ARROWS_SHRINK;
using View::ICON_CHART_BAR;
using View::ICON_CHART_PIE;
using View::ICON_COMPASS;
using View::ICON_ELLIPSIS;
using View::ICON_EYE;
using View::ICON_EYE_SLASH;
using View::ICON_EYE_THIN;
using View::ICON_FUNNEL;
using View::ICON_LIST;
using View::ICON_STICKY_NOTE;
using View::ICON_X_CIRCLED;

constexpr const char* SYSTEM_TRACE_FILE  = "rocpd-transpose.db";
constexpr const char* COMPUTE_TRACE_FILE = "rocprof_compute_23ed6f36.db";
constexpr const char* PROJECT_FILE_STEM  = "transpose_walkthrough";
constexpr const char* FILE_DIALOG_KEY    = "ChooseFileDlgKey";
constexpr const char* MAIN_WINDOW        = "//Main Window";
// Temp folder holding the samples as they were before any chapter opened them.
constexpr const char* PRISTINE_SAMPLES   = "optiq-tutorial-samples";

constexpr float LOAD_TIMEOUT_S      = 180.0f;
constexpr float SETTLE_TIMEOUT_S    = 60.0f;
constexpr float VISIBLE_LOAD_S      = 2.0f;
constexpr float VISIBLE_SETTLE_S    = 0.25f;
constexpr int   SETTLE_FRAMES       = 3;
// Frames a newly opened compute analysis takes to lay itself out.
constexpr int   LAYOUT_SETTLE_FRAMES = 20;
// The longest a table takes to send the request a click asked for.
constexpr float REQUEST_START_S     = 0.5f;
constexpr float HOVER_S             = 1.2f;
constexpr float TOOLTIP_READ_S      = 1.8f;
constexpr float READ_S              = 2.6f;
// How long a menu that stays open after a checkbox click shows the new check.
constexpr float OPTION_SHOWN_S      = 0.3f;
constexpr float MIN_CLICKABLE_WIDTH = 14.0f;
constexpr float MENU_LABEL_OFFSET   = 110.0f;
constexpr float ROW_TOLERANCE       = 4.0f;
// How deep under a window items are gathered, as deep as GatherItems() goes.
constexpr int   GATHER_DEPTH        = 99;
constexpr int   FRAME_POLL_LIMIT    = 20000;
// A trace still without tracks this many frames after it finished loading
// came from a damaged database; it is reopened from a fresh copy.
constexpr int   EMPTY_TRACE_FRAMES  = 600;
constexpr int   OPEN_ATTEMPTS       = 2;
constexpr int   ROW_SUFFIX_SIZE     = 16;
// Share of the work area the file dialog is given on camera.
constexpr float FILE_DIALOG_WIDTH   = 0.6f;
constexpr float FILE_DIALOG_HEIGHT  = 0.66f;
constexpr float FILE_DIALOG_TIMEOUT_S = 10.0f;
// Right of the longest file name, where the pointer's tail misses the name in
// the row below; file rows are selectable across their whole width.
constexpr float FILE_NAME_GAP       = 40.0f;
// How far into the details panel the pointer rests after a splitter drag.
constexpr float SPLITTER_REST_Y     = 90.0f;
// How much taller chapter 06 makes the details panel; more would push the
// second queue off the timeline.
constexpr float DETAILS_GROWTH_Y    = 130.0f;
// Where the pointer waits before a chapter starts: over the empty details panel
// of a trace, or the status bar under a compute analysis, where nothing pops up
// under it.
constexpr float PARK_X              = 0.62f;
constexpr float PARK_Y              = 0.9f;
constexpr float PARK_STATUS_Y       = 0.985f;
// The share of a track row that must be on screen for the pointer to rest on it.
constexpr float REST_ROW_SHOWN      = 0.6f;
// Across a counter's description area, its average pill, after the type pill.
constexpr float AVERAGE_PILL_X      = 0.43f;
// The sample's GPU power and memory usage counters report sentinel values
// (65535 W, petabytes of memory), so the counters on camera are CPU ones, which
// come twelve rows before them. Chapter 04 scrolls to the CPU time counter;
// chapter 05 plots context switches, whose values vary across the whole trace
// while CPU time stays near zero.
constexpr const char* GO_TO_COUNTER = "thread_cpu_time";
constexpr const char* PLOT_COUNTER  = "thread_context_switch";
// How wide a counter sample must be drawn for the pointer to land on it alone.
constexpr float MIN_SAMPLE_WIDTH    = 2.0f;
// Across the topology sidebar, labels start right of the eye and go-to icons.
constexpr float SIDEBAR_LABEL_X     = 0.6f;
constexpr float TOOLBAR_HOVER_S     = 0.7f;
// Between the buttons of a group the narration names with one word.
constexpr float TOOLBAR_STEP_S      = 0.35f;
constexpr float ITEM_TIMEOUT_S      = 10.0f;
// Across a details table, the narrow id column on its left edge.
constexpr float ID_COLUMN_X         = 0.03f;
// Across a queue row, a spot before the first kernel with the whole trace in view.
constexpr float QUEUE_IDLE_X        = 0.03f;
// Across a track's description area, a spot right of its name and pills and left
// of its values and arrow, where nothing raises a tooltip.
constexpr float META_EMPTY_X        = 0.72f;
// Down the summary window, the dispatch table under the chart.
constexpr float SUMMARY_REST_Y      = 0.85f;
// Right of the summary's chart buttons, empty space in the row under the chart,
// far enough right that the pointer's tail misses the column headers below.
constexpr float SUMMARY_ASIDE_X     = 230.0f;
// The track summary's tooltip belongs to both of its lines, and its breakdown
// line spans the summary's width; this far in from that line's right end, past
// its words, the pointer raises the tooltip without covering any text.
constexpr float TRACK_SUMMARY_INSET = 12.0f;
// A page scrolls, a burst of wheel notches at a time, until its target's top
// lands this far down the view, and counts as there within the band.
constexpr int   SCROLL_BURST_LIMIT  = 4;
constexpr float SCROLL_TARGET_Y     = 0.06f;
constexpr float SCROLL_TARGET_BAND  = 0.3f;
constexpr float WHEEL_BURST_GAP_S   = 0.3f;
// Page scrolls turn the wheel a notch at a time this far apart, under a
// caption that stays up this much longer than the turning.
constexpr float WHEEL_NOTCH_GAP_S      = 0.18f;
constexpr float WHEEL_CAPTION_LINGER_S = 1.0f;
constexpr const char* DETAILS_TABLE = "infinite_scroll_table";
constexpr const char* KERNEL_TABLE  = "kernel_selection_table";
// The card around a System Speed-of-Light table, named after the table's title.
constexpr const char* SOL_PANEL     = "Speed-of-Light_panel_";
// The comparison threshold rises 0.1% per pixel dragged. The differences on
// screen run from about 32% to 100%, so a drag to 55% clears several rows and
// leaves several highlighted. The slider sits too near the window's right edge
// for that in one stroke, so the drag takes as many strokes as it needs.
constexpr float THRESHOLD_DRAG_X    = 550.0f;
constexpr float THRESHOLD_EDGE_GAP  = 30.0f;
// Across a row of the Bookmarks list, right of its number and left of its button.
constexpr float BOOKMARK_ROW_X      = 0.35f;
// In the Presets popup, an empty spot of the list, clear of its placeholder.
constexpr float PRESET_REST_X       = 0.12f;
constexpr float PRESET_REST_Y       = 0.2f;

// ---------------------------------------------------------------------------
// Window and item lookup

static bool
window_name_contains(const ImGuiWindow* window, const char* fragment)
{
    return window != nullptr && window->WasActive && !window->Hidden &&
           std::strstr(window->Name, fragment) != nullptr;
}

// Most recently submitted active window whose name contains `fragment`. Child
// windows repeat their parent's name as a prefix, so the outermost match wins.
static ImGuiWindow*
find_window(const char* fragment)
{
    ImGuiWindow* found = nullptr;
    for(ImGuiWindow* window : ImGui::GetCurrentContext()->Windows)
    {
        if(window_name_contains(window, fragment) &&
           (window->ParentWindow == nullptr ||
            std::strstr(window->ParentWindow->Name, fragment) == nullptr))
        {
            found = window;
        }
    }
    return found;
}

static ImGuiWindow*
find_child_window(const ImGuiWindow* parent, const char* fragment)
{
    for(ImGuiWindow* window : ImGui::GetCurrentContext()->Windows)
    {
        if(parent != nullptr && window->ParentWindow == parent &&
           window_name_contains(window, fragment))
        {
            return window;
        }
    }
    return nullptr;
}

static ImRect
window_rect(const ImGuiWindow* window)
{
    return window != nullptr ? window->Rect() : ImRect();
}

static bool
has_area(const ImRect& rect)
{
    return rect.GetWidth() > 0.0f && rect.GetHeight() > 0.0f;
}

static ImVec2
rect_point(const ImRect& rect, float fx, float fy)
{
    return ImVec2(rect.Min.x + rect.GetWidth() * fx, rect.Min.y + rect.GetHeight() * fy);
}

// Pointer target on a full-width row (menu entry, table row): over its label.
static ImVec2
label_point(const ImRect& rect)
{
    return ImVec2(rect.Min.x + ImMin(rect.GetWidth() * 0.35f, MENU_LABEL_OFFSET),
                  rect.GetCenter().y);
}

// The topology sidebar is the LeftColumn child that hosts the "Project" tree.
static ImGuiWindow*
find_sidebar_window(ImGuiTestContext* ctx)
{
    for(ImGuiWindow* window : ImGui::GetCurrentContext()->Windows)
    {
        if(window_name_contains(window, "LeftColumn") &&
           ctx->ItemExists(ImHashStr("Project", 0, window->ID)))
        {
            return window;
        }
    }
    return nullptr;
}

static ImGuiWindow*
find_toolbar_window()
{
    return find_window("/Toolbar_");
}

static ImGuiWindow*
find_details_window()
{
    return find_window("BottomRow");
}

static ImGuiWindow*
find_graph_window()
{
    return find_window("/Graph View Main_");
}

// Child windows carry their parent's name as a prefix, so a plain name match can
// land on a panel inside the dialog; the dialog's buttons live in its root.
static ImGuiWindow*
find_file_dialog_window()
{
    ImGuiWindow* child = find_window(FILE_DIALOG_KEY);
    return child != nullptr ? child->RootWindow : nullptr;
}

// Union of the timeline overview (histogram) row, label column included.
static ImRect
overview_rect()
{
    ImRect rect(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);
    for(const char* fragment : { "/HistogramSidebar_", "/Histogram_" })
    {
        ImGuiWindow* window = find_window(fragment);
        if(window != nullptr)
        {
            rect.Add(window->Rect());
        }
    }
    return rect.IsInverted() ? ImRect() : rect;
}

// The status bar is the thin, full-width child window along the bottom edge.
static ImRect
status_bar_rect()
{
    constexpr float MAX_HEIGHT_FRACTION = 0.06f;
    constexpr float MIN_WIDTH_FRACTION  = 0.9f;
    constexpr float EDGE_TOLERANCE      = 2.0f;
    const ImVec2    display             = ImGui::GetIO().DisplaySize;
    ImRect          best;
    for(ImGuiWindow* window : ImGui::GetCurrentContext()->Windows)
    {
        if(!window->WasActive || window->Hidden || window->ParentWindow == nullptr)
        {
            continue;
        }
        const ImRect rect = window->Rect();
        if(rect.Max.y >= display.y - EDGE_TOLERANCE && rect.GetHeight() > 0.0f &&
           rect.GetHeight() < display.y * MAX_HEIGHT_FRACTION &&
           rect.GetWidth() > display.x * MIN_WIDTH_FRACTION)
        {
            best = rect;
        }
    }
    return best;
}

static ImRect
item_rect(ImGuiTestContext* ctx, ImGuiTestRef ref)
{
    const ImGuiTestItemInfo info = ctx->ItemInfo(ref, ImGuiTestOpFlags_NoError);
    return info.ID != 0 ? info.RectFull : ImRect();
}

// Items of `window` and its child windows, in reading order. GatherItems() only
// sees the main navigation layer, and a table scrolled down moves its frozen
// rows (column headers, filter boxes) to the menu layer, so `with_frozen_rows`
// gathers both layers.
static std::vector<ImGuiTestItemInfo>
gather_items(ImGuiTestContext* ctx, ImGuiWindow* window, bool with_frozen_rows = false)
{
    std::vector<ImGuiTestItemInfo> items;
    if(window == nullptr || ctx->IsError())
    {
        return items;
    }
    ImGuiTestItemList list;
    if(with_frozen_rows)
    {
        // The same loop as GatherItems(), with the menu layer let in.
        ImGuiTestGatherTask& task = ctx->Engine->GatherTask;
        task.InParentID           = window->ID;
        task.InMaxDepth           = GATHER_DEPTH;
        task.InLayerMask          = (1 << ImGuiNavLayer_Main) | (1 << ImGuiNavLayer_Menu);
        task.OutList              = &list;
        for(int gathered = -1; gathered != list.GetSize();)
        {
            gathered = list.GetSize();
            ctx->Yield();
        }
        task.Clear();
    }
    else
    {
        ctx->GatherItems(&list, ImGuiTestRef(window->ID));
    }
    for(const ImGuiTestItemInfo& info : list)
    {
        items.push_back(info);
    }
    std::sort(items.begin(), items.end(),
              [](const ImGuiTestItemInfo& a, const ImGuiTestItemInfo& b) {
                  if(std::abs(a.RectFull.Min.y - b.RectFull.Min.y) > ROW_TOLERANCE)
                  {
                      return a.RectFull.Min.y < b.RectFull.Min.y;
                  }
                  return a.RectFull.Min.x < b.RectFull.Min.x;
              });
    return items;
}

// Whether an item's debug label shows `wanted`. The engine keeps only the first
// characters of a label, and icon menu entries spend several on padding, so a
// label that filled the buffer only has to be a prefix of `wanted`.
static bool
label_matches(const char* debug_label, const char* wanted)
{
    if(std::strstr(debug_label, wanted) != nullptr)
    {
        return true;
    }
    constexpr size_t LABEL_CAPACITY = sizeof(ImGuiTestItemInfo::DebugLabel);
    const char*      text           = debug_label;
    while(*text == ' ')
    {
        text++;
    }
    const size_t length = std::strlen(text);
    return std::strlen(debug_label) + 1 >= LABEL_CAPACITY && length > 0 &&
           std::strncmp(wanted, text, length) == 0;
}

// The `occurrence`-th item (in reading order) whose debug label shows `label`.
// ID 0 when none matches.
static ImGuiTestItemInfo
find_item(ImGuiTestContext* ctx, ImGuiWindow* window, const char* label, int occurrence = 0)
{
    for(const ImGuiTestItemInfo& info : gather_items(ctx, window))
    {
        if(label_matches(info.DebugLabel, label) && occurrence-- == 0)
        {
            return info;
        }
    }
    return ImGuiTestItemInfo();
}

// The item of `window` whose ID is `label` hashed in its own parent scope. This
// finds widgets that never report a label to the engine (combos, icon buttons)
// whatever IDs their callers pushed around them.
static ImGuiTestItemInfo
find_hashed_item(ImGuiTestContext* ctx, ImGuiWindow* window, const char* label)
{
    for(const ImGuiTestItemInfo& info : gather_items(ctx, window))
    {
        if(info.ID == ImHashStr(label, 0, info.ParentID))
        {
            return info;
        }
    }
    return ImGuiTestItemInfo();
}

static ImGuiTestItemInfo
find_item_exact(ImGuiTestContext* ctx, ImGuiWindow* window, const char* label,
                bool with_frozen_rows = false)
{
    for(const ImGuiTestItemInfo& info : gather_items(ctx, window, with_frozen_rows))
    {
        if(std::strcmp(info.DebugLabel, label) == 0)
        {
            return info;
        }
    }
    return ImGuiTestItemInfo();
}

// A point in `column` on the first data row of the table in `window`, below the
// header and, when shown, the filter row.
static bool
first_table_row(ImGuiTestContext* ctx, ImGuiWindow* window, const char* column, ImVec2& out)
{
    const ImGuiTestItemInfo header = find_item_exact(ctx, window, column, true);
    if(header.ID == 0)
    {
        return false;
    }
    const float row_height = ImGui::GetFrameHeight();
    float       top        = header.RectFull.Max.y;
    for(const ImGuiTestItemInfo& item : gather_items(ctx, window, true))
    {
        if(std::strcmp(item.DebugLabel, "##input_text_with_clear") == 0 &&
           item.RectFull.Min.y >= header.RectFull.Min.y &&
           item.RectFull.Min.y < header.RectFull.Max.y + row_height * 1.5f)
        {
            top = ImMax(top, item.RectFull.Max.y);
        }
    }
    out = ImVec2(header.RectFull.GetCenter().x, top + row_height * 0.6f);
    return true;
}

// The entry of any open popup (menus included, newest first) whose label
// contains `label`.
static ImGuiTestItemInfo
find_popup_item(ImGuiTestContext* ctx, const char* label)
{
    ImGuiContext& g = *ImGui::GetCurrentContext();
    std::string   seen;
    for(int i = g.OpenPopupStack.Size - 1; i >= 0; i--)
    {
        ImGuiWindow* popup = g.OpenPopupStack[i].Window;
        if(popup == nullptr)
        {
            continue;
        }
        for(const ImGuiTestItemInfo& info : gather_items(ctx, popup))
        {
            if(label_matches(info.DebugLabel, label))
            {
                return info;
            }
            seen += std::string(" '") + info.DebugLabel + "'";
        }
        seen += std::string(" in ") + popup->Name + ";";
    }
    ctx->LogInfo("No popup entry '%s' among:%s", label, seen.c_str());
    return ImGuiTestItemInfo();
}

static bool
hover_popup_item(Director& director, const char* label, float seconds = 0.9f)
{
    const ImGuiTestItemInfo item = find_popup_item(director.Context(), label);
    if(item.ID == 0)
    {
        director.Context()->LogWarning("Menu entry '%s' not found", label);
        return false;
    }
    director.Hover(label_point(item.RectFull), seconds);
    return true;
}

// Disabled entries leave their menu open when clicked, so the menu is closed
// instead to keep it from swallowing the next interaction.
static bool
click_popup_item(Director& director, const char* label, float hover_s = 0.6f)
{
    const ImGuiTestItemInfo item = find_popup_item(director.Context(), label);
    if(!hover_popup_item(director, label, hover_s))
    {
        return false;
    }
    if(item.ItemFlags & ImGuiItemFlags_Disabled)
    {
        director.Context()->PopupCloseAll();
        return false;
    }
    director.Context()->MouseClick(ImGuiMouseButton_Left);
    return true;
}

static void
open_menu(Director& director, const char* menu)
{
    director.Context()->SetRef(MAIN_WINDOW);
    director.ClickItem((std::string("##MenuBar/") + menu).c_str());
    director.Hold(0.4f);
}

// Opens the menu bar's `menu` as the narration says `word`.
static void
open_menu_on(Director& director, const char* menu, const char* word)
{
    director.Context()->SetRef(MAIN_WINDOW);
    director.PointAtItem((std::string("##MenuBar/") + menu).c_str(), word);
    director.ClickHere();
}

// Points at the open popup's entry `label` as the narration says `word`.
static bool
point_at_popup_item(Director& director, const char* label, const char* word)
{
    const ImGuiTestItemInfo item = find_popup_item(director.Context(), label);
    if(item.ID == 0)
    {
        director.Context()->LogWarning("Menu entry '%s' not found", label);
        return false;
    }
    director.PointAt(label_point(item.RectFull), word);
    return true;
}

// Clicks the open popup's entry `label` as the narration says `word`; see
// click_popup_item() about disabled entries.
static bool
click_popup_item_on(Director& director, const char* label, const char* word)
{
    const ImGuiTestItemInfo item = find_popup_item(director.Context(), label);
    if(!point_at_popup_item(director, label, word))
    {
        return false;
    }
    if(item.ItemFlags & ImGuiItemFlags_Disabled)
    {
        director.Context()->PopupCloseAll();
        return false;
    }
    director.ClickHere();
    return true;
}

// Clicks `ref` once the pointer reaches it as the narration says `word`.
static void
click_item_on(Director& director, ImGuiTestRef ref, const char* word)
{
    director.PointAtItem(ref, word);
    director.ClickHere();
}

static void
close_popups(Director& director)
{
    director.Context()->PopupCloseAll();
    director.Hold(0.3f);
}

static void rest_pointer(Director& director, bool promptly = false);

// Moves the pointer to rest before closing the open menus, so a closing menu
// doesn't leave it over whatever the menu covered, raising that tooltip.
static void
leave_popups(Director& director)
{
    rest_pointer(director);
    close_popups(director);
}

// ---------------------------------------------------------------------------
// Project state

static std::string
sample_path(const char* file_name)
{
    return (std::filesystem::path(GetConfig().samples_dir) / file_name).string();
}

static std::filesystem::path
pristine_samples_dir()
{
    std::error_code ec;
    return std::filesystem::temp_directory_path(ec) / PRISTINE_SAMPLES;
}

// Optiq writes caches into a trace's database, and a copy that several chapters
// had opened was once found corrupted, so each chapter opens a fresh copy of
// the samples as they were before the first one ran. Call before any chapter.
static void
snapshot_samples()
{
    std::error_code             ec;
    const std::filesystem::path dir = pristine_samples_dir();
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    for(const char* file_name : { SYSTEM_TRACE_FILE, COMPUTE_TRACE_FILE })
    {
        std::filesystem::copy_file(sample_path(file_name), dir / file_name,
                                   std::filesystem::copy_options::overwrite_existing, ec);
    }
}

// Puts back the sample `file_name` as it was when the run started, with no
// write-ahead log left beside it. No project may have it open: overwriting a
// database under an open connection corrupts it, so a sample whose log is
// still held is left alone.
static void
restore_sample(ImGuiTestContext* ctx, const char* file_name)
{
    const std::string target = sample_path(file_name);
    std::error_code   ec;
    for(const char* suffix : { "-wal", "-shm" })
    {
        std::filesystem::remove(target + suffix, ec);
        if(ec)
        {
            ctx->LogError("Could not delete %s%s, so it stays as it is: %s", file_name,
                          suffix, ec.message().c_str());
            return;
        }
    }
    std::filesystem::copy_file(pristine_samples_dir() / file_name, target,
                               std::filesystem::copy_options::overwrite_existing, ec);
    if(ec)
    {
        ctx->LogError("Could not restore %s: %s", file_name, ec.message().c_str());
    }
}

static TraceView*
current_trace_view()
{
    Project* project = AppWindow::GetInstance()->GetCurrentProject();
    return project != nullptr ? dynamic_cast<TraceView*>(project->GetView().get()) : nullptr;
}

static ComputeView*
current_compute_view()
{
    Project* project = AppWindow::GetInstance()->GetCurrentProject();
    return project != nullptr ? dynamic_cast<ComputeView*>(project->GetView().get())
                              : nullptr;
}

static TimelineView*
timeline_of(TraceView* trace_view)
{
    return trace_view != nullptr ? TraceViewTestPeer{ *trace_view }.TimelineViewPtr()
                                 : nullptr;
}

static bool
trace_view_ready(TraceView* trace_view)
{
    if(trace_view == nullptr)
    {
        return false;
    }
    DataProvider* provider = trace_view->GetDataProvider();
    TimelineView* timeline = timeline_of(trace_view);
    return provider != nullptr && provider->GetState() == ProviderState::kReady &&
           timeline != nullptr && TimelineViewTestPeer{ *timeline }.TrackCount() > 0 &&
           TimelineViewTestPeer{ *timeline }.FirstFlameWindowId() != 0;
}

static bool
data_settled()
{
    Project* project = AppWindow::GetInstance()->GetCurrentProject();
    if(project == nullptr)
    {
        return true;
    }
    std::shared_ptr<View::RootView> root =
        std::dynamic_pointer_cast<View::RootView>(project->GetView());
    DataProvider* provider = root ? root->GetDataProvider() : nullptr;
    return provider == nullptr || provider->GetPendingRequestCount() == 0;
}

// Yields until outstanding data requests drain for a few consecutive frames.
static void
settle(Director& director, float visible_s = VISIBLE_SETTLE_S)
{
    int calm_frames = 0;
    director.WaitFor(
        [&calm_frames]() {
            calm_frames = data_settled() ? calm_frames + 1 : 0;
            return calm_frames >= SETTLE_FRAMES;
        },
        visible_s, SETTLE_TIMEOUT_S);
}

// Settles after a click whose data request only goes out a few frames later,
// such as a table's Submit button.
static void
settle_request(Director& director)
{
    director.WaitFor([]() { return !data_settled(); }, REQUEST_START_S, REQUEST_START_S);
    settle(director);
}

static void
settle_off_camera(ImGuiTestContext* ctx)
{
    int calm_frames = 0;
    for(int i = 0; i < FRAME_POLL_LIMIT && calm_frames < SETTLE_FRAMES; i++)
    {
        calm_frames = data_settled() ? calm_frames + 1 : 0;
        ctx->Yield();
    }
}

// Closes whatever an earlier chapter may have left open on top of the view.
static void
dismiss_dialogs(ImGuiTestContext* ctx)
{
    if(ImGuiFileDialog::Instance()->IsOpened())
    {
        ImGuiFileDialog::Instance()->Close();
    }
    ctx->PopupCloseAll();
    ctx->Yield(2);
}

// Closes the projects whose tabs are `ids`. A tab disappears at once, but its
// project only lets go of the trace a frame or more later, when the app handles
// the close and frees the controller on a background job; this waits for both,
// so no connection is left open on the sample.
static void
remove_projects(ImGuiTestContext* ctx, const std::vector<std::string>& ids)
{
    AppWindow* app = AppWindow::GetInstance();
    for(const std::string& id : ids)
    {
        app->GetTabContainer()->RemoveTab(id);
    }
    auto any_open = [app, &ids]() {
        return std::any_of(ids.begin(), ids.end(),
                           [app](const std::string& id) { return app->GetProject(id) != nullptr; });
    };
    for(int i = 0; i < FRAME_POLL_LIMIT && any_open(); i++)
    {
        ctx->Yield();
    }
    AppWindowTestPeer peer{ *app };
    for(int i = 0; i < FRAME_POLL_LIMIT && peer.ProviderCleanupJobCount() > 0; i++)
    {
        ctx->Yield();
    }
}

static void
close_all_projects(ImGuiTestContext* ctx)
{
    dismiss_dialogs(ctx);
    std::shared_ptr<TabContainer> tabs = AppWindow::GetInstance()->GetTabContainer();
    if(tabs == nullptr)
    {
        return;
    }
    std::vector<std::string> ids;
    for(const TabItem* tab : tabs->GetTabs())
    {
        ids.push_back(tab->m_id);
    }
    remove_projects(ctx, ids);
    ctx->Yield(3);
}

// Brings `file_name` to the front, opening it off camera when needed.
static void
activate_project(ImGuiTestContext* ctx, const char* file_name)
{
    AppWindow*                    app  = AppWindow::GetInstance();
    std::shared_ptr<TabContainer> tabs = app->GetTabContainer();
    std::string                   id;
    for(const TabItem* tab : tabs->GetTabs())
    {
        Project* project = app->GetProject(tab->m_id);
        if(project != nullptr &&
           std::filesystem::path(project->GetID()).filename() == file_name)
        {
            id = tab->m_id;
        }
    }
    if(id.empty())
    {
        app->OpenFile(sample_path(file_name));
    }
    else
    {
        tabs->SetActiveTab(id);
    }
    ctx->Yield(3);
}

static std::vector<std::string>
project_tabs(const char* file_name)
{
    AppWindow*               app = AppWindow::GetInstance();
    std::vector<std::string> ids;
    for(const TabItem* tab : app->GetTabContainer()->GetTabs())
    {
        Project* project = app->GetProject(tab->m_id);
        if(project != nullptr &&
           std::filesystem::path(project->GetID()).filename() == file_name)
        {
            ids.push_back(tab->m_id);
        }
    }
    return ids;
}

static void
close_project(ImGuiTestContext* ctx, const char* file_name)
{
    remove_projects(ctx, project_tabs(file_name));
    ctx->Yield(10);
}

static void
show_all_panels()
{
    AppWindowSettings& panels = SettingsManager::GetInstance().GetAppWindowSettings();
    panels.show_toolbar       = true;
    panels.show_details_panel = true;
    panels.show_sidebar       = true;
    panels.show_histogram     = true;
    panels.show_summary       = false;
    AppWindow::GetInstance()->ApplyPanelVisibilitySettings();
}

static void
show_all_tracks(TraceView* trace_view)
{
    for(TrackItem* track : *timeline_of(trace_view)->GetTracks())
    {
        if(track != nullptr && !track->IsDisplayed())
        {
            track->SetDisplay(true);
        }
    }
}

// Whether `trace_view` finished loading without a single track, as a trace
// read from a damaged database does.
static bool
trace_view_empty(TraceView* trace_view)
{
    DataProvider* provider = trace_view != nullptr ? trace_view->GetDataProvider() : nullptr;
    TimelineView* timeline = timeline_of(trace_view);
    return provider != nullptr && provider->GetState() == ProviderState::kReady &&
           timeline != nullptr && TimelineViewTestPeer{ *timeline }.TrackCount() == 0;
}

// Waits for the system trace to load. Null when it doesn't, which is known
// early for a trace that loads without tracks.
static TraceView*
wait_for_system_trace(ImGuiTestContext* ctx)
{
    int empty_frames = 0;
    for(int i = 0; i < FRAME_POLL_LIMIT && empty_frames < EMPTY_TRACE_FRAMES; i++)
    {
        TraceView* trace_view = current_trace_view();
        if(trace_view_ready(trace_view))
        {
            return trace_view;
        }
        empty_frames = trace_view_empty(trace_view) ? empty_frames + 1 : 0;
        ctx->Yield();
    }
    return nullptr;
}

// Reloads the system trace off camera, so no chapter inherits another's track
// heights, order, notes or bookmarks, and shows its default state: full time
// range, nothing selected, every panel and track visible.
static TraceView*
prepare_system_trace(ImGuiTestContext* ctx)
{
    dismiss_dialogs(ctx);
    TraceView* trace_view = nullptr;
    for(int attempt = 0; attempt < OPEN_ATTEMPTS && trace_view == nullptr; attempt++)
    {
        if(attempt > 0)
        {
            ctx->LogWarning("%s loaded without tracks; reopening a fresh copy",
                            SYSTEM_TRACE_FILE);
        }
        close_project(ctx, SYSTEM_TRACE_FILE);
        restore_sample(ctx, SYSTEM_TRACE_FILE);
        activate_project(ctx, SYSTEM_TRACE_FILE);
        trace_view = wait_for_system_trace(ctx);
    }
    if(trace_view == nullptr)
    {
        ctx->LogError("System trace %s did not load", SYSTEM_TRACE_FILE);
        return nullptr;
    }

    show_all_panels();
    show_all_tracks(trace_view);
    TraceViewTestPeer peer{ *trace_view };
    peer.ResetLayout();
    if(MeasurementController* measurement = peer.MeasurementControllerPtr())
    {
        measurement->ExitMeasurementMode();
        measurement->ClearMeasurement();
    }
    if(View::EventSearch* search = peer.EventSearchPtr())
    {
        search->Clear();
    }
    std::shared_ptr<TimelineSelection> selection = trace_view->GetTimelineSelection();
    selection->UnselectAllEvents();
    selection->UnselectAllTracks();
    selection->ClearTimeRange();
    if(AnalysisView* analysis = peer.AnalysisViewPtr())
    {
        analysis->SelectTab("Event Table");
    }
    ImGuiWindow* minimap = ImGui::FindWindowByName("Minimap");
    if(minimap != nullptr && minimap->WasActive)
    {
        ctx->WindowClose("//Minimap");
    }
    const View::TimelineModel& timeline_model =
        trace_view->GetDataProvider()->DataModel().GetTimeline();
    peer.TimelineViewPtr()->MoveToPosition(timeline_model.GetStartTime(),
                                           timeline_model.GetEndTime(), 0.0, false);
    ctx->Yield(10);
    settle_off_camera(ctx);
    ctx->Yield(10);
    return trace_view;
}

static bool
compute_view_ready(ComputeView* compute_view)
{
    if(compute_view == nullptr)
    {
        return false;
    }
    ComputeViewTestPeer     peer{ *compute_view };
    View::ComputeSelection* selection = peer.ComputeSelectionPtr();
    return peer.TabContainerPtr() != nullptr && selection != nullptr &&
           selection->GetSelectedKernel() != View::ComputeSelection::INVALID_SELECTION_ID;
}

// Reopens the compute database off camera, so no chapter inherits another's
// added columns or pinned metrics.
static ComputeView*
prepare_compute_trace(ImGuiTestContext* ctx)
{
    dismiss_dialogs(ctx);
    close_project(ctx, COMPUTE_TRACE_FILE);
    restore_sample(ctx, COMPUTE_TRACE_FILE);
    activate_project(ctx, COMPUTE_TRACE_FILE);
    for(int i = 0; i < FRAME_POLL_LIMIT && !compute_view_ready(current_compute_view()); i++)
    {
        ctx->Yield();
    }
    ComputeView* compute_view = current_compute_view();
    if(!compute_view_ready(compute_view))
    {
        ctx->LogError("Compute database %s did not load", COMPUTE_TRACE_FILE);
        return nullptr;
    }
    ComputeViewTestPeer{ *compute_view }.TabContainerPtr()->SetActiveTab(
        ComputeSummaryView::TAB_ID);
    ctx->Yield(10);
    settle_off_camera(ctx);
    ctx->Yield(10);
    return compute_view;
}

// Parks the pointer somewhere neutral before the first recorded frame.
static void
park_pointer(ImGuiTestContext* ctx, float fx, float fy)
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ctx->MouseTeleportToPos(ImVec2(display.x * fx, display.y * fy));
    ctx->Yield(2);
}

// Whether an opening file dialog is ready to be shown. Until then it keeps
// giving the dialog a comfortable size in the middle of the window: the app
// opens it small in a corner, where the footage's UI scale crops its fields.
static std::function<bool()>
file_dialog_presented()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2         size(viewport->WorkSize.x * FILE_DIALOG_WIDTH,
                      viewport->WorkSize.y * FILE_DIALOG_HEIGHT);
    const ImVec2         pos(viewport->WorkPos.x + (viewport->WorkSize.x - size.x) * 0.5f,
                     viewport->WorkPos.y + (viewport->WorkSize.y - size.y) * 0.5f);
    // The dialog keeps its own size and place for its first frames, so it only
    // counts as presented once it has been drawn where it was put for a few
    // frames running.
    return [size, pos, placed_frames = 0]() mutable {
        ImGuiWindow* dialog = find_file_dialog_window();
        if(dialog == nullptr)
        {
            return false;
        }
        const bool placed =
            ImLengthSqr(ImVec2(dialog->Pos.x - pos.x, dialog->Pos.y - pos.y)) < 1.0f &&
            ImLengthSqr(ImVec2(dialog->Size.x - size.x, dialog->Size.y - size.y)) < 1.0f;
        placed_frames = placed ? placed_frames + 1 : 0;
        ImGui::SetWindowSize(dialog, size, ImGuiCond_Always);
        ImGui::SetWindowPos(dialog, pos, ImGuiCond_Always);
        return placed_frames >= SETTLE_FRAMES;
    };
}

// Clicks, where the pointer is, what opens a file dialog, and cuts from the
// button's release until the dialog is presented: it draws a few frames before
// the click returns.
static bool
click_to_open_file_dialog(Director& director)
{
    return director.ClickAndCutUntil(ImGui::GetIO().MousePos, file_dialog_presented(),
                                     FILE_DIALOG_TIMEOUT_S);
}

// Drags the splitter above the details panel; negative `delta_y` makes the panel
// taller. The pointer then leaves the splitter so its resize cursor doesn't
// linger on screen, and rests a beat unless the line goes straight on.
static void
drag_details_splitter(Director& director, float delta_y, bool beat = true)
{
    const ImRect details = window_rect(find_details_window());
    if(!has_area(details))
    {
        return;
    }
    const ImVec2 grip(details.Min.x + details.GetWidth() * 0.5f, details.Min.y - 1.5f);
    director.Hover(grip, 0.5f);
    director.Drag(grip, ImVec2(grip.x, grip.y + delta_y), 1.0f);
    director.MoveTo(ImVec2(grip.x, grip.y + delta_y + SPLITTER_REST_Y));
    if(beat)
    {
        director.Beat();
    }
}

// Clicks the details panel's `tab` as the narration says `word`.
static void
click_details_tab_on(Director& director, const char* tab, const char* word)
{
    director.Context()->SetRef(MAIN_WINDOW);
    click_item_on(director, (std::string("**/") + tab).c_str(), word);
    director.Hold(0.3f);
}

// ---------------------------------------------------------------------------
// Timeline tracks and events

// One on-screen timeline row: its track, description area and chart area.
struct TrackRow
{
    TrackItem*   track     = nullptr;
    ImGuiWindow* container = nullptr;
    ImRect       meta;
    ImRect       chart;
};

static bool
ends_with(const char* text, const char* suffix)
{
    const size_t text_length   = std::strlen(text);
    const size_t suffix_length = std::strlen(suffix);
    return text_length >= suffix_length &&
           std::strcmp(text + text_length - suffix_length, suffix) == 0;
}

// Rows the timeline rendered last frame, top to bottom. Each row is
// BeginChild("") under PushID(track_index); hashing an empty label returns its
// seed, and child windows are named "<parent>/<label>_%08X" after that ID, so a
// row window's name ends in the PushID hash of its track's index.
static std::vector<TrackRow>
track_rows(TraceView* trace_view)
{
    std::vector<TrackRow> rows;
    TimelineView*         timeline = timeline_of(trace_view);
    if(timeline == nullptr)
    {
        return rows;
    }
    std::vector<ImGuiWindow*> containers;
    for(ImGuiWindow* window : ImGui::GetCurrentContext()->Windows)
    {
        if(window_name_contains(window, "/MetaData Area_") && window->ParentWindow != nullptr)
        {
            containers.push_back(window->ParentWindow);
        }
    }
    std::shared_ptr<std::vector<TrackItem*>> tracks = timeline->GetTracks();
    for(int index = 0; index < static_cast<int>(tracks->size()); index++)
    {
        TrackItem* track = (*tracks)[index];
        if(track == nullptr || !track->IsDisplayed())
        {
            continue;
        }
        ImGuiWindow* container = nullptr;
        for(ImGuiWindow* candidate : containers)
        {
            const ImGuiID seed   = candidate->ParentWindow ? candidate->ParentWindow->ID : 0;
            const ImGuiID row_id = ImHashData(&index, sizeof(index), seed);
            char          suffix[ROW_SUFFIX_SIZE];
            ImFormatString(suffix, IM_COUNTOF(suffix), "/_%08X", row_id);
            if(ends_with(candidate->Name, suffix))
            {
                container = candidate;
            }
        }
        FlameTrackItem* flame = dynamic_cast<FlameTrackItem*>(track);
        if(container == nullptr && flame != nullptr)
        {
            ImGuiWindow* chart =
                ImGui::FindWindowByID(FlameTrackItemTestPeer{ *flame }.FlameWindowId());
            if(chart != nullptr && chart->WasActive)
            {
                container = chart->ParentWindow;
            }
        }
        if(container == nullptr)
        {
            continue;
        }
        TrackRow row;
        row.track     = track;
        row.container = container;
        for(ImGuiWindow* window : ImGui::GetCurrentContext()->Windows)
        {
            if(!window->WasActive || window->ParentWindow != container)
            {
                continue;
            }
            if(std::strstr(window->Name, "/MetaData Area_") != nullptr)
            {
                row.meta = window->Rect();
            }
            else if(std::strstr(window->Name, "/FV_") != nullptr ||
                    std::strstr(window->Name, "/LV_") != nullptr)
            {
                row.chart = window->Rect();
            }
        }
        if(has_area(row.meta))
        {
            rows.push_back(row);
        }
    }
    std::sort(rows.begin(), rows.end(), [](const TrackRow& a, const TrackRow& b) {
        return a.meta.Min.y < b.meta.Min.y;
    });
    return rows;
}

// Waits for the timeline to draw a row matching `match`, which it may skip for
// a few frames after the view changes.
static std::vector<TrackRow>
wait_for_rows(ImGuiTestContext* ctx, TraceView* trace_view,
              const std::function<bool(TrackItem*)>& match)
{
    constexpr int ROW_WAIT_FRAMES = 240;
    std::vector<TrackRow> rows = track_rows(trace_view);
    for(int i = 0; i < ROW_WAIT_FRAMES; i++)
    {
        bool found = false;
        for(const TrackRow& row : rows)
        {
            found = found || match(row.track);
        }
        if(found)
        {
            break;
        }
        ctx->Yield();
        rows = track_rows(trace_view);
    }
    return rows;
}

static const TrackRow*
find_row(const std::vector<TrackRow>& rows, const std::function<bool(TrackItem*)>& match)
{
    for(const TrackRow& row : rows)
    {
        if(match(row.track))
        {
            return &row;
        }
    }
    return nullptr;
}

static TrackItem*
find_track(TraceView* trace_view, const std::function<bool(TrackItem*)>& match)
{
    for(TrackItem* track : *timeline_of(trace_view)->GetTracks())
    {
        if(track != nullptr && match(track))
        {
            return track;
        }
    }
    return nullptr;
}

static bool
has_operation(const TrackItem* track, rocprofvis_dm_event_operation_t operation)
{
    const TrackInfo* info = track->GetTrackInfo();
    return info != nullptr && info->operation_types.count(operation) > 0;
}

static bool
is_api_thread(TrackItem* track)
{
    return dynamic_cast<FlameTrackItem*>(track) != nullptr &&
           has_operation(track, kRocProfVisDmOperationLaunch);
}

static bool
is_sampled_thread(TrackItem* track)
{
    return dynamic_cast<FlameTrackItem*>(track) != nullptr &&
           has_operation(track, kRocProfVisDmOperationLaunchSample);
}

static bool
is_kernel_queue(TrackItem* track)
{
    return dynamic_cast<FlameTrackItem*>(track) != nullptr &&
           has_operation(track, kRocProfVisDmOperationDispatch);
}

static std::function<bool(TrackItem*)>
named(const char* name)
{
    return [name](TrackItem* track) { return track->GetName() == name; };
}

static std::function<bool(TrackItem*)>
named_counter(const char* name)
{
    return [name](TrackItem* track) {
        return dynamic_cast<LineTrackItem*>(track) != nullptr && track->GetName() == name;
    };
}

// The chart area of the timeline (every row's graph column), clipped to what
// is visible.
static ImRect
graph_area(const std::vector<TrackRow>& rows)
{
    ImGuiWindow* graph = find_graph_window();
    if(graph == nullptr || rows.empty())
    {
        return ImRect();
    }
    ImRect area = graph->InnerClipRect;
    for(const TrackRow& row : rows)
    {
        if(has_area(row.chart))
        {
            area.Min.x = row.chart.Min.x;
            area.Max.x = ImMin(area.Max.x, row.chart.Max.x);
            break;
        }
    }
    return area;
}

static ImRect
visible_part(const ImRect& rect)
{
    ImGuiWindow* graph = find_graph_window();
    ImRect       clipped = rect;
    if(graph != nullptr)
    {
        clipped.ClipWith(graph->InnerClipRect);
    }
    return clipped;
}

// Where the track name sits in a row's description area.
static ImVec2
track_label_point(const TrackRow& row)
{
    const float scale = GetConfig().ui_scale;
    return ImVec2(row.meta.Min.x + 30.0f * scale, row.meta.Min.y + 12.0f * scale);
}

// An empty spot of the description area, clear of the label, pills and arrow,
// within the part of the row that is on screen.
static ImVec2
track_meta_point(const TrackRow& row)
{
    return rect_point(visible_part(row.meta), 0.62f, 0.32f);
}

// Moves the pointer to `pos`. The engine pauses before every move; `promptly`
// skips that pause, for leaving a spot at once, such as where a menu entry was
// clicked and the menu closed over controls that raise tooltips.
static void
move_pointer(Director& director, const ImVec2& pos, bool promptly)
{
    ImGuiTestEngineIO& io    = *director.Context()->EngineIO;
    const float        pause = io.ActionDelayStandard;
    if(promptly)
    {
        io.ActionDelayStandard = 0.0f;
    }
    director.MoveTo(pos);
    io.ActionDelayStandard = pause;
}

// Moves the pointer onto an empty spot of the description area of the first
// track on screen, where it raises no tooltip while the narration carries on.
// Compute views have no tracks, so there it rests on the status bar.
static void
rest_pointer(Director& director, bool promptly)
{
    for(const TrackRow& row : track_rows(current_trace_view()))
    {
        if(visible_part(row.meta).GetHeight() >= row.meta.GetHeight() * REST_ROW_SHOWN)
        {
            move_pointer(director, track_meta_point(row), promptly);
            return;
        }
    }
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    move_pointer(director, ImVec2(display.x * PARK_X, display.y * PARK_STATUS_Y), promptly);
}

// Like rest_pointer(), on the first track `match` accepts, so the pointer stays
// by the track that just changed instead of over events that raise tooltips.
static void
rest_on_track(Director& director, TraceView* trace_view,
              const std::function<bool(TrackItem*)>& match, bool promptly = false)
{
    const std::vector<TrackRow> rows = track_rows(trace_view);
    const TrackRow*             row  = find_row(rows, match);
    if(row != nullptr &&
       visible_part(row->meta).GetHeight() >= row->meta.GetHeight() * REST_ROW_SHOWN)
    {
        move_pointer(director, track_meta_point(*row), promptly);
    }
    else
    {
        rest_pointer(director, promptly);
    }
}

// Whether the row of the first track `match` accepts is drawn and entirely on
// screen.
static bool
row_fully_shown(TraceView* trace_view, const std::function<bool(TrackItem*)>& match)
{
    const std::vector<TrackRow> rows = track_rows(trace_view);
    const TrackRow*             row  = find_row(rows, match);
    return row != nullptr && visible_part(row->meta).GetHeight() >= row->meta.GetHeight() - 1.0f;
}

// Waits, off camera, for `window` to draw an item showing `label`: tables drop
// their items while they refetch rows. Logs an error when it never appears, so
// a skipped step shows up in the recording log.
static ImGuiTestItemInfo
await_item(Director& director, const std::function<ImGuiWindow*()>& window, const char* label,
           bool exact)
{
    ImGuiTestContext* ctx = director.Context();
    ImGuiTestItemInfo found;
    director.WaitFor(
        [&]() {
            for(const ImGuiTestItemInfo& info : gather_items(ctx, window(), true))
            {
                if(exact ? std::strcmp(info.DebugLabel, label) == 0
                         : label_matches(info.DebugLabel, label))
                {
                    found = info;
                    return true;
                }
            }
            return false;
        },
        0.0f, ITEM_TIMEOUT_S);
    if(found.ID == 0)
    {
        ctx->LogError("'%s' never appeared", label);
    }
    return found;
}

static void
click_reset_view(Director& director)
{
    director.Context()->SetRef(MAIN_WINDOW);
    director.ClickItem("**/Reset View");
    rest_pointer(director, true);
    settle(director);
}

// Clears the selected events and time range and shows the whole trace, without
// input that would leave a caption or click ripple on screen. For Cut().
static void
reset_view_off_camera(ImGuiTestContext* ctx, TraceView* trace_view)
{
    std::shared_ptr<TimelineSelection> selection = trace_view->GetTimelineSelection();
    selection->UnselectAllEvents();
    selection->ClearTimeRange();
    const View::TimelineModel& model = trace_view->GetDataProvider()->DataModel().GetTimeline();
    timeline_of(trace_view)->MoveToPosition(model.GetStartTime(), model.GetEndTime(), 0.0, false);
    ctx->Yield(6);
    settle_off_camera(ctx);
    ctx->Yield(6);
}

struct EventBar
{
    std::string name;
    double      start_ns    = 0.0;
    double      duration_ns = 0.0;
    ImRect      rect;
};

// On-screen bars of `track` wide enough to click, left to right. Bars register
// with the test engine under GetID(uuid) in their chart window.
static std::vector<EventBar>
visible_bars(ImGuiTestContext* ctx, FlameTrackItem* track)
{
    std::vector<EventBar> bars;
    ImGuiWindow* chart = ImGui::FindWindowByID(FlameTrackItemTestPeer{ *track }.FlameWindowId());
    if(chart == nullptr || !chart->WasActive)
    {
        return bars;
    }
    ImGuiTestItemList items;
    ctx->GatherItems(&items, ImGuiTestRef(chart->ID));
    for(const TraceEvent& event : FlameTrackItemTestPeer{ *track }.Events())
    {
        const void* key =
            reinterpret_cast<const void*>(static_cast<uintptr_t>(event.m_id.uuid));
        const ImGuiTestItemInfo* info = items.GetByID(ImHashData(&key, sizeof(key), chart->ID));
        if(info == nullptr)
        {
            continue;
        }
        const ImRect rect = visible_part(info->RectFull);
        if(rect.GetWidth() >= MIN_CLICKABLE_WIDTH && rect.GetHeight() > 0.0f)
        {
            bars.push_back({ event.m_name, event.m_start_ts, event.m_duration, rect });
        }
    }
    std::sort(bars.begin(), bars.end(),
              [](const EventBar& a, const EventBar& b) { return a.rect.Min.x < b.rect.Min.x; });
    return bars;
}

// The visible bar closest to `fx` of the graph width whose name contains `name`
// (any bar when `name` is empty). Returns false when none is on screen.
static bool
pick_bar(ImGuiTestContext* ctx, FlameTrackItem* track, const char* name, float fx,
         const ImRect& graph, EventBar& out)
{
    const float target = graph.Min.x + graph.GetWidth() * fx;
    float       best   = FLT_MAX;
    for(const EventBar& bar : visible_bars(ctx, track))
    {
        if(name != nullptr && name[0] != '\0' && bar.name.find(name) == std::string::npos)
        {
            continue;
        }
        const float distance = std::abs(bar.rect.GetCenter().x - target);
        if(distance < best)
        {
            best = distance;
            out  = bar;
        }
    }
    return best < FLT_MAX;
}

// Zooms the timeline off camera to `span_ns` centered on `center_ns`.
static void
frame_time_window(ImGuiTestContext* ctx, TraceView* trace_view, double center_ns,
                  double span_ns)
{
    timeline_of(trace_view)->MoveToPosition(center_ns - span_ns * 0.5,
                                            center_ns + span_ns * 0.5, 0.0, false);
    ctx->Yield(6);
    settle_off_camera(ctx);
    ctx->Yield(6);
}

// The cached event of `track` that starts closest to `time_ns`.
static bool
event_near(FlameTrackItem* track, double time_ns, TraceEvent& out)
{
    bool   found = false;
    double best  = 0.0;
    for(const TraceEvent& event : FlameTrackItemTestPeer{ *track }.Events())
    {
        const double distance = std::abs(event.m_start_ts - time_ns);
        if(event.m_duration > 0.0 && (!found || distance < best))
        {
            out   = event;
            best  = distance;
            found = true;
        }
    }
    return found;
}

// The middle of the highest sample drawn on a counter row, where the graph's
// tooltip reads a value above zero. Returns false when no sample is on screen.
static bool
peak_sample_point(TraceView* trace_view, const TrackRow& row, ImVec2& out)
{
    const auto* samples = dynamic_cast<const RawTrackSampleData*>(
        trace_view->GetDataProvider()->DataModel().GetTimeline().GetTrackData(
            row.track->GetID()));
    TimelineView* timeline = timeline_of(trace_view);
    std::shared_ptr<TimePixelTransform> transform =
        timeline != nullptr ? timeline->GetTransform() : nullptr;
    if(samples == nullptr || transform == nullptr || !has_area(row.chart))
    {
        return false;
    }
    bool   found = false;
    double peak  = 0.0;
    for(const TraceCounter& sample : samples->GetData())
    {
        const float start = row.chart.Min.x + transform->RawTimeToPixel(sample.m_start_ts);
        const float end   = row.chart.Min.x + transform->RawTimeToPixel(sample.m_end_ts);
        const float mid   = (start + end) * 0.5f;
        if(end - start < MIN_SAMPLE_WIDTH || mid < row.chart.Min.x || mid > row.chart.Max.x)
        {
            continue;
        }
        if(!found || sample.m_value > peak)
        {
            peak  = sample.m_value;
            out   = ImVec2(mid, row.chart.GetCenter().y);
            found = true;
        }
    }
    return found;
}

// With the whole trace in view, a spot on a queue row before the first kernel
// reaches the GPU: no event there raises a tooltip or takes a click.
static ImVec2
queue_idle_point(const TrackRow& queue_row)
{
    return ImVec2(queue_row.chart.Min.x + queue_row.chart.GetWidth() * QUEUE_IDLE_X,
                  queue_row.chart.GetCenter().y);
}

// A mouse press inside the graph gives the timeline keyboard focus; pressed
// before the first kernel, it selects nothing.
static void
focus_timeline(Director& director, const TrackRow& queue_row)
{
    director.Click(queue_idle_point(queue_row));
    director.Hold(0.4f);
}

// Opens the right-click menu of a track's description area.
static void
open_track_menu(Director& director, const TrackRow& row)
{
    director.Click(track_meta_point(row), ImGuiMouseButton_Right);
    director.Hold(0.6f);
}

// Clicks a checkbox or radio button under Track Options of the first track
// `match` accepts, as the narration says `word`; the menu opens right away, so
// the word should come a couple of seconds into the line. Those options leave
// the menu open, so the menus are closed as soon as the choice shows, and the
// pointer heads straight back to the track: it crosses neither the menu's other
// entries nor rests on the events the menu covered.
static void
toggle_track_option_on(Director& director, TraceView* trace_view,
                       const std::function<bool(TrackItem*)>& match, const char* option,
                       const char* word)
{
    const std::vector<TrackRow> rows = track_rows(trace_view);
    const TrackRow*             row  = find_row(rows, match);
    if(row == nullptr)
    {
        director.Context()->LogError("No track on screen for '%s'", option);
        return;
    }
    open_track_menu(director, *row);
    hover_popup_item(director, "Track Options", 0.5f);
    click_popup_item_on(director, option, word);
    director.Hold(OPTION_SHOWN_S);
    director.Context()->PopupCloseAll();
    rest_on_track(director, trace_view, match, true);
}

// ---------------------------------------------------------------------------
// Topology sidebar

struct SidebarRow
{
    ImGuiTestItemInfo name;
    ImGuiID           eye_id   = 0;
    ImGuiID           go_to_id = 0;
};

// A track row of the sidebar by track name. Its buttons are keyed by the
// track id scope, so they resolve even while scrolled out of view.
static bool
find_sidebar_track(ImGuiTestContext* ctx, ImGuiWindow* sidebar, const std::string& name,
                   SidebarRow& out)
{
    for(const ImGuiTestItemInfo& item : gather_items(ctx, sidebar))
    {
        if(item.ID == ImHashStr(name.c_str(), 0, item.ParentID))
        {
            out.name = item;
            for(const char* icon : { ICON_EYE, ICON_EYE_SLASH })
            {
                const ImGuiID eye = ImHashStr(icon, 0, item.ParentID);
                if(ctx->ItemExists(eye))
                {
                    out.eye_id = eye;
                }
            }
            const ImGuiID go_to = ImHashStr(ICON_ARROWS_SHRINK, 0, item.ParentID);
            out.go_to_id        = ctx->ItemExists(go_to) ? go_to : 0;
            return true;
        }
    }
    return false;
}

// A branch row (tree node) of the sidebar whose label contains `label`.
static ImGuiTestItemInfo
find_sidebar_branch(ImGuiTestContext* ctx, ImGuiWindow* sidebar, const char* label,
                    int occurrence = 0)
{
    return find_item(ctx, sidebar, label, occurrence);
}

// The branch-level eye button drawn on the same line as `branch`.
static ImGuiTestItemInfo
branch_eye(ImGuiTestContext* ctx, ImGuiWindow* sidebar, const ImGuiTestItemInfo& branch)
{
    ImGuiTestItemInfo best;
    for(const ImGuiTestItemInfo& item : gather_items(ctx, sidebar))
    {
        const bool same_row =
            std::abs(item.RectFull.GetCenter().y - branch.RectFull.GetCenter().y) < ROW_TOLERANCE;
        const bool is_eye = std::strcmp(item.DebugLabel, ICON_EYE) == 0 ||
                            std::strcmp(item.DebugLabel, ICON_EYE_SLASH) == 0;
        if(same_row && is_eye && item.RectFull.Max.x <= branch.RectFull.Min.x + ROW_TOLERANCE)
        {
            best = item;
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// Chapter 01: the welcome page and opening a trace

static void
chapter_open_trace(ImGuiTestContext* ctx)
{
    close_all_projects(ctx);
    restore_sample(ctx, SYSTEM_TRACE_FILE);
    park_pointer(ctx, 0.62f, 0.72f);

    Director director(ctx, ctx->Test->Name);
    if(!director.Start())
    {
        return;
    }
    director.Say("Let's start at the very beginning, by opening a trace.");

    // Headings are plain text, so they are located through their cards. The
    // pointer waits past the end of the subtitle rather than on its words.
    const ImRect body = window_rect(find_window("/welcome_body_"));
    if(has_area(body))
    {
        director.Hover(ImVec2(body.Min.x + body.GetWidth() * 0.8f, body.Min.y + 30.0f), READ_S);
    }
    ctx->SetRef(MAIN_WINDOW);
    director.Say("Optiq opens on a welcome page. From here, Open Trace File loads traces from "
                 "the ROCm Systems Profiler, databases from the ROCm Compute Profiler, and saved "
                 "Optiq projects.");
    director.HoverItem("**/open_trace/action", TOOLTIP_READ_S + 1.0f);
    director.Beat();
    const ImRect left_card = window_rect(find_window("/welcome_card_left_"));
    director.Say("You can also drag a file straight onto the window. The files you've opened "
                 "show up under Recent, and Resources links to the documentation and the source "
                 "code.");
    // Beside the text of each card, not on it.
    if(has_area(left_card))
    {
        director.Hover(rect_point(left_card, 0.8f, 0.82f), 2.0f);
    }
    const ImRect right_card = window_rect(find_window("/welcome_card_right_"));
    if(has_area(right_card))
    {
        director.Hover(rect_point(right_card, 0.7f, 0.3f), 1.5f);
        director.Hover(rect_point(right_card, 0.7f, 0.6f), 1.5f);
    }
    director.Pause();

    // Open the trace through the file browser.
    ctx->SetRef(MAIN_WINDOW);
    director.Say("Let's open a sample trace. Click Open Trace File.");
    director.PointAtItem("**/open_trace/action", "Click");
    director.WaitForVoice();
    click_to_open_file_dialog(director);

    const std::string file_label    = std::string("[File] ") + SYSTEM_TRACE_FILE;
    const std::string compute_label = std::string("[File] ") + COMPUTE_TRACE_FILE;
    ImGuiTestItemInfo file_row;
    ImVec2            file_row_pos;
    // The dialog opens with the file names under the button's click spot, so
    // the pointer is moved past them to the end of the row off camera.
    director.Cut([&]() {
        ctx->Yield(SETTLE_FRAMES);
        file_row = find_item(ctx, find_file_dialog_window(), file_label.c_str());
        if(file_row.ID != 0)
        {
            const float names_width = ImMax(ImGui::CalcTextSize(file_label.c_str()).x,
                                            ImGui::CalcTextSize(compute_label.c_str()).x);
            file_row_pos = ImVec2(file_row.RectFull.Min.x + names_width + FILE_NAME_GAP,
                                  file_row.RectFull.GetCenter().y);
            ctx->MouseTeleportToPos(file_row_pos);
        }
    });
    if(file_row.ID == 0)
    {
        ctx->LogError("File dialog does not list %s", SYSTEM_TRACE_FILE);
        return;
    }
    director.Beat();
    director.Say("In the file browser, select the trace file, and then click OK.");
    director.PointAt(file_row_pos, "select");
    director.ClickHere();
    const ImGuiTestItemInfo ok = find_item(ctx, find_file_dialog_window(), "OK");
    if(ok.ID == 0)
    {
        ctx->LogError("File dialog has no OK button");
        return;
    }
    director.PointAt(ok.RectFull.GetCenter(), "OK");
    director.ClickHere();
    director.Say("Optiq reads the trace and opens it in a new tab.");

    director.WaitFor([]() { return trace_view_ready(current_trace_view()); }, VISIBLE_LOAD_S,
                     LOAD_TIMEOUT_S);
    settle(director);
    director.Say("The timeline shows what every CPU thread and GPU queue was doing, moment by "
                 "moment. We'll learn how to get around it later in this series.");
    rest_pointer(director);
    director.Pause();

    // The file stays one click away afterwards.
    director.Say("To open another file later, use the File menu. Recent Files keeps the "
                 "traces you've opened close at hand.");
    open_menu_on(director, "File", "File");
    point_at_popup_item(director, "Recent Files", "Recent");
    director.Hold(READ_S);
    leave_popups(director);

    ctx->SetRef(MAIN_WINDOW);
    const ImRect tab = item_rect(ctx, (std::string("**/") + SYSTEM_TRACE_FILE).c_str());
    director.Say("Each file opens in its own tab, so you can keep several traces open and "
                 "switch between them.");
    if(has_area(tab))
    {
        director.Hover(tab.GetCenter(), 2.0f);
    }
    rest_pointer(director);
    director.Pause();
    director.Say("And that's how you open a trace in ROCm Optiq.");
    director.Finish();
}

// ---------------------------------------------------------------------------
// Chapter 02: a tour of the workspace

static void
spotlight_and_hover(Director& director, const ImRect& rect, float fx, float fy)
{
    if(!has_area(rect))
    {
        return;
    }
    director.Spotlight(rect);
    director.Hover(rect_point(rect, fx, fy), READ_S);
}

static void
chapter_interface_tour(ImGuiTestContext* ctx)
{
    TraceView* trace_view = prepare_system_trace(ctx);
    if(trace_view == nullptr)
    {
        return;
    }
    park_pointer(ctx, PARK_X, PARK_Y);

    Director director(ctx, ctx->Test->Name);
    if(!director.Start())
    {
        return;
    }
    director.Say("Next, let's take a quick tour of the workspace, so you know where everything "
                 "lives.");
    director.Pause();

    // Menu bar: the pointer reaches each menu as the line names it.
    ImGuiWindow* main_window = ImGui::FindWindowByName("Main Window");
    director.Say("Along the top is the menu bar, with the File, Edit, View, and Help menus. The "
                 "View menu lets you show or hide each panel.");
    director.Spotlight(main_window->MenuBarRect());
    ctx->SetRef(MAIN_WINDOW);
    for(const char* menu : { "File", "Edit", "View", "Help" })
    {
        director.PointAtItem((std::string("##MenuBar/") + menu).c_str(), menu);
    }
    director.ClearSpotlight();
    open_menu_on(director, "View", "View");
    point_at_popup_item(director, "Show Advanced Details Panel", "panel");
    director.Hold(READ_S);
    close_popups(director);
    director.Beat();

    // Toolbar, control by control, each reached as the narration names it.
    ImGuiWindow* toolbar = find_toolbar_window();
    // The bookmarks menu has no label, and the ellipsis beside the search box
    // (its advanced options) isn't one of the controls the narration names.
    const ImGuiID bookmarks =
        toolbar != nullptr ? ImHashStr("bookmark_toggle_dropdown", 0, toolbar->ID) : 0;
    std::vector<ImGuiTestItemInfo> controls;
    for(const ImGuiTestItemInfo& item : gather_items(ctx, toolbar))
    {
        const bool named = item.DebugLabel[0] != '\0' &&
                           !label_matches(item.DebugLabel, ICON_ELLIPSIS);
        if(item.RectFull.GetWidth() > 0.0f && (named || item.ID == bookmarks))
        {
            controls.push_back(item);
        }
    }
    const auto control_index = [&controls](const char* label) {
        size_t index = 0;
        while(index < controls.size() && !label_matches(controls[index].DebugLabel, label))
        {
            index++;
        }
        return index;
    };
    const auto point_at = [&director, &controls](size_t index, const char* word) {
        if(index < controls.size())
        {
            director.PointAt(controls[index].RectFull.GetCenter(), word);
        }
    };
    const size_t search  = control_index("##input_text_with_clear");
    const size_t measure = control_index("Measure");
    // Before the search box sit the flow buttons, then as many annotation
    // buttons; the pointer reaches each group as the line names it.
    const size_t group = search / 2;
    director.Say("Just below it is the toolbar. The first buttons control flow arrows, which "
                 "link related events, and annotations, the notes you can pin to the timeline.");
    director.Spotlight(window_rect(toolbar));
    for(size_t i = 0; i < search && i < controls.size(); i++)
    {
        director.PointAt(controls[i].RectFull.GetCenter(),
                         i == 0 ? "flow" : i == group ? "annotations" : nullptr);
        director.Hold(TOOLBAR_STEP_S);
    }
    director.Say("Next to those are the event search box and your bookmarks.");
    point_at(search, "search");
    point_at(search + 1, "bookmarks");
    director.Hold(TOOLBAR_HOVER_S);
    director.Say("Then come Measure mode, the minimap, and Reset View, which brings back the "
                 "whole trace.");
    point_at(measure, "Measure");
    point_at(measure + 1, "minimap");
    point_at(measure + 2, "Reset");
    director.Hold(TOOLBAR_HOVER_S);
    director.Beat();

    // Topology, overview, timeline, details, status bar.
    ImGuiWindow* sidebar = find_sidebar_window(ctx);
    director.Say("On the left, the System Topology panel lists the hardware and software in "
                 "the trace, from nodes and processors down to queues, processes, and threads.");
    spotlight_and_hover(director, window_rect(sidebar), SIDEBAR_LABEL_X, 0.3f);
    director.Hover(rect_point(window_rect(sidebar), SIDEBAR_LABEL_X, 0.55f), 1.5f);

    director.Say("Across the top, the timeline overview charts activity across the whole "
                 "trace.");
    spotlight_and_hover(director, overview_rect(), 0.55f, 0.5f);

    ImRect timeline_rect = window_rect(find_window("/Main Trace_"));
    if(!has_area(timeline_rect))
    {
        timeline_rect = window_rect(find_graph_window());
    }
    std::vector<TrackRow> rows = track_rows(trace_view);
    director.Say("Below it is the timeline, the heart of Optiq, with one row for each "
                 "track.");
    if(has_area(timeline_rect))
    {
        const TrackRow* idle_queue = find_row(rows, is_kernel_queue);
        director.Spotlight(timeline_rect);
        director.Hover(idle_queue != nullptr ? queue_idle_point(*idle_queue)
                                             : rect_point(timeline_rect, 0.5f, 0.35f),
                       READ_S);
    }
    if(!rows.empty())
    {
        ImRect meta_column(rows.front().meta.Min, rows.back().meta.Max);
        meta_column.ClipWith(timeline_rect);
        director.Say("Each track has a description area on the left, with its name and "
                     "details.");
        spotlight_and_hover(director, meta_column, META_EMPTY_X, 0.3f);
        director.Say("To the right is its graph area, where events and counter values are drawn "
                     "over time.");
        spotlight_and_hover(director, graph_area(rows), 0.6f, 0.5f);
    }

    // A selected queue gives the details panel something to show.
    rows                       = track_rows(trace_view);
    const TrackRow* queue_row  = find_row(rows, is_kernel_queue);
    const bool      have_queue = queue_row != nullptr;
    const ImVec2    queue_point = have_queue ? track_meta_point(*queue_row) : ImVec2();
    director.Say("At the bottom, the Advanced Details panel shows more about whatever you "
                 "select. Its tabs hold the event table, the sample table, event details, track "
                 "details, top events, and annotations.");
    if(have_queue)
    {
        director.Click(queue_point);
        settle(director);
    }
    director.Spotlight(window_rect(find_details_window()));
    AnalysisView* analysis = TraceViewTestPeer{ *trace_view }.AnalysisViewPtr();
    if(analysis != nullptr)
    {
        // The line names the tabs in order, each by the first word of its title.
        ctx->SetRef(MAIN_WINDOW);
        for(const std::string& tab : analysis->ListTabs())
        {
            director.PointAtItem(("**/" + tab).c_str(), tab.substr(0, tab.find(' ')).c_str());
        }
        director.Hold(TOOLBAR_HOVER_S);
    }

    director.Say("And the status bar along the bottom edge tells you what Optiq is working "
                 "on.");
    spotlight_and_hover(director, status_bar_rect(), 0.05f, 0.5f);
    director.ClearSpotlight();
    if(have_queue)
    {
        director.Click(queue_point);
    }
    director.Pause();

    // Splitters resize the panes.
    director.Say("You can drag the borders between panels to resize them, and make room for "
                 "what you're working on.");
    // One border, out and back, while the line is spoken.
    const ImRect sidebar_rect = window_rect(sidebar);
    const ImVec2 v_split(sidebar_rect.Max.x + 1.5f,
                         sidebar_rect.Min.y + sidebar_rect.GetHeight() * 0.5f);
    director.PointAt(v_split, "Drag");
    director.Drag(v_split, ImVec2(v_split.x + 160.0f, v_split.y), 1.0f);
    director.Hold(0.4f);
    director.Drag(ImVec2(v_split.x + 160.0f, v_split.y), v_split, 1.0f);
    rest_pointer(director, true);
    director.Pause();
    director.Say("That's the workspace at a glance.");
    director.Finish();
}

// ---------------------------------------------------------------------------
// Chapter 03: navigating the timeline

static void
chapter_navigate_timeline(ImGuiTestContext* ctx)
{
    TraceView* trace_view = prepare_system_trace(ctx);
    if(trace_view == nullptr)
    {
        return;
    }
    std::vector<TrackRow> rows      = wait_for_rows(ctx, trace_view, is_kernel_queue);
    const TrackRow*       queue_row = find_row(rows, is_kernel_queue);
    const ImRect          graph     = graph_area(rows);
    if(queue_row == nullptr || !has_area(graph))
    {
        ctx->LogError("Timeline rows are not on screen (%d rows)", static_cast<int>(rows.size()));
        return;
    }
    const TrackRow queue      = *queue_row;
    const ImVec2   busy_point = ImVec2(graph.Min.x + graph.GetWidth() * 0.62f,
                                     queue.chart.GetCenter().y);
    // The timeline only takes its keys while the pointer is over it, so during
    // key presses the pointer waits on an empty part of a description area,
    // where panning brings no event under it.
    const ImVec2 keys_rest = rect_point(queue.meta, META_EMPTY_X, 0.5f);
    park_pointer(ctx, PARK_X, PARK_Y);

    Director director(ctx, ctx->Test->Name);
    if(!director.Start())
    {
        return;
    }
    director.Say("Now let's learn how to move around the timeline, using the keyboard and the "
                 "mouse.");
    director.Pause();

    // The header sums up what the trace contains.
    ImGuiWindow* const header    = find_window("/HistogramSidebar_");
    const ImRect       breakdown = window_rect(find_child_window(header, "/elided_"));
    director.Say("First, the summary in the top-left corner counts the tracks in the trace, "
                 "grouped by type.");
    if(has_area(breakdown))
    {
        director.Hover(ImVec2(breakdown.Max.x - TRACK_SUMMARY_INSET, breakdown.GetCenter().y),
                       READ_S);
    }

    // Keyboard: W/S zoom at the pointer, A/D pan, arrows scroll the tracks,
    // each pressed as the line names it.
    director.Say("Click in the graph area to give the timeline keyboard focus. Then press W "
                 "and S to zoom in and out around the pointer.");
    focus_timeline(director, queue);
    director.PointAt(busy_point, "Then");
    director.WaitUntilSaid("W");
    director.KeyHold(ImGuiKey_W, "W", 1.0f);
    director.KeyHold(ImGuiKey_S, "S", 0.6f);
    settle(director);
    director.PointAt(keys_rest);
    director.Say("The A and D keys pan left and right, and holding Shift moves twice as fast.");
    director.KeyHold(ImGuiKey_A, "A", 0.8f);
    director.KeyHold(ImGuiKey_D, "D", 0.8f);
    director.WaitUntilSaid("Shift");
    director.KeyHold(ImGuiMod_Shift | ImGuiKey_D, "Shift + D", 1.0f);
    settle(director);
    director.Say("The up and down arrow keys scroll through the tracks.");
    director.WaitUntilSaid("down");
    director.KeyHold(ImGuiKey_DownArrow, "Down Arrow", 1.2f);
    director.KeyHold(ImGuiKey_UpArrow, "Up Arrow", 1.2f);
    settle(director);
    director.Pause();

    // Mouse: the wheel zooms over the graph and scrolls over the track names.
    // Each is put back briskly as the line ends.
    director.Say("With the mouse, the scroll wheel zooms whenever the pointer is over the graph "
                 "area.");
    director.PointAt(busy_point, "scroll");
    director.Caption("Mouse wheel", 2.5f);
    director.Wheel(busy_point, 1.0f, 4, 0.3f);
    settle(director);
    director.Caption("Mouse wheel", 1.5f);
    director.Wheel(busy_point, -1.0f, 4, 0.2f);
    settle(director);
    const ImVec2 meta_point = rect_point(queue.meta, 0.55f, 0.5f);
    director.Say("Over the description area, the same wheel scrolls up and down through the "
                 "list of tracks.");
    director.PointAt(meta_point, "scrolls");
    director.Caption("Mouse wheel", 2.0f);
    director.Wheel(meta_point, -1.0f, 4, 0.3f);
    director.Caption("Mouse wheel", 1.5f);
    director.Wheel(meta_point, 1.0f, 4, 0.2f);
    director.PointAt(keys_rest);
    director.Pause();

    // Dragging the graph pans in time. The view stays where the drag leaves
    // it: the overview below shows that part of the trace.
    director.Say("Once you've zoomed in, you can drag the graph area to pan through time.");
    director.PointAt(busy_point, "zoomed");
    director.KeyHold(ImGuiKey_W, "W", 0.8f);
    settle(director);
    director.WaitUntilSaid("drag");
    director.Caption("Drag", 2.0f);
    director.Drag(busy_point, ImVec2(busy_point.x - graph.GetWidth() * 0.25f, busy_point.y),
                  1.3f);
    settle(director);
    director.PointAt(track_meta_point(queue));
    director.Pause();

    // The overview highlights the visible span; dragging it moves the view.
    const ImRect bars = window_rect(find_window("/Histogram Bars_"));
    TimelineView* timeline = timeline_of(trace_view);
    if(has_area(bars) && timeline != nullptr)
    {
        const View::TimelineModel& model =
            trace_view->GetDataProvider()->DataModel().GetTimeline();
        const View::ViewCoords coords   = timeline->GetViewCoords();
        const double           range_ns = model.GetEndTime() - model.GetStartTime();
        const float            center   = static_cast<float>(
            ((coords.v_min_x + coords.v_max_x) * 0.5 - model.GetStartTime()) / range_ns);
        const ImVec2 grab(bars.Min.x + bars.GetWidth() * center, bars.GetCenter().y);
        director.Say("The timeline overview highlights the part of the trace you're looking "
                     "at, and you can drag the highlight to move the view.");
        director.PointAt(grab, "highlights");
        director.WaitUntilSaid("drag");
        director.Drag(grab, ImVec2(grab.x - bars.GetWidth() * 0.2f, grab.y), 1.5f);
        settle(director);
        director.Pause();
    }

    // Track names show their full details on hover; a thread's tooltip starts
    // with its full name, where a queue's has none.
    rows = track_rows(trace_view);
    if(const TrackRow* row = find_row(rows, is_sampled_thread))
    {
        director.Say("Hover over a track's name to see its full name, its track ID, and how "
                     "many events it holds.");
        director.Hover(track_label_point(*row), READ_S);
    }
    director.Pause();

    director.Say("When you're done exploring, Reset View brings back the whole trace.");
    ctx->SetRef(MAIN_WINDOW);
    director.PointAtItem("**/Reset View", "Reset");
    director.WaitUntilSaid("whole");
    director.ClickHere();
    rest_pointer(director, true);
    settle(director);
    director.Pause();
    director.Say("Now you can get to any part of a trace in just a few moves.");
    director.Finish();
}

// ---------------------------------------------------------------------------
// Chapter 04: the System Topology panel

static void
chapter_topology_panel(ImGuiTestContext* ctx)
{
    TraceView* trace_view = prepare_system_trace(ctx);
    if(trace_view == nullptr)
    {
        return;
    }
    ImGuiWindow* sidebar = find_sidebar_window(ctx);
    if(sidebar == nullptr)
    {
        ctx->LogError("Topology sidebar not found");
        return;
    }
    park_pointer(ctx, PARK_X, PARK_Y);

    Director director(ctx, ctx->Test->Name);
    if(!director.Start())
    {
        return;
    }
    director.Say("Next, let's look at the System Topology panel, which organizes every track in "
                 "the trace.");
    director.Pause();

    // The tree mirrors the hardware and the software that ran on it.
    // The pointer reaches each part of the tree as the line names it.
    director.Say("The tree mirrors the system that was profiled: its nodes and processors, "
                 "each GPU's queues and counters, and then the CPUs.");
    const std::pair<const char*, const char*> tree[] = {
        { "Project", "tree" },  { "Nodes", "nodes" },       { "Processors", "processors" },
        { "GPU0", "GPU" },      { "Queues", "queues" },     { "Counters", "counters" },
        { "CPU0", "CPUs" }
    };
    for(const auto& [label, word] : tree)
    {
        const ImGuiTestItemInfo branch = find_sidebar_branch(ctx, sidebar, label);
        if(branch.ID != 0)
        {
            director.PointAt(label_point(branch.RectFull), word);
        }
    }
    director.Hold(HOVER_S);

    // Branches fold away.
    director.Say("Click a branch to collapse it, and click it again to expand it.");
    const ImGuiTestItemInfo gpu_counters = find_sidebar_branch(ctx, sidebar, "Counters", 0);
    if(gpu_counters.ID != 0)
    {
        director.PointAtItem(gpu_counters.ID, "collapse");
        director.ClickHere();
        director.WaitUntilSaid("expand");
        director.ClickHere();
    }
    director.Pause();

    // The eye hides a track, or every track under a branch.
    // The eye's icon, and with it the button's ID, flips with visibility, so it
    // is looked up again before every click.
    SidebarRow queue2;
    if(find_sidebar_track(ctx, sidebar, "Queue 2", queue2) && queue2.eye_id != 0)
    {
        director.Say("The eye button hides a track from the timeline, and clicking it again "
                     "shows the track.");
        director.PointAtItem(queue2.eye_id, "eye");
        director.WaitUntilSaid("hides");
        director.ClickHere();
        if(find_sidebar_track(ctx, sidebar, "Queue 2", queue2) && queue2.eye_id != 0)
        {
            director.PointAtItem(queue2.eye_id, "shows");
            director.ClickHere();
        }
        director.Pause();
    }
    const ImGuiTestItemInfo gpu_queues = find_sidebar_branch(ctx, sidebar, "Queues", 0);
    const ImGuiTestItemInfo queues_eye = branch_eye(ctx, sidebar, gpu_queues);
    if(queues_eye.ID != 0)
    {
        director.Say("On a branch, the eye hides or shows every track below it at once.");
        director.PointAtItem(queues_eye.ID, "eye");
        director.WaitUntilSaid("hides");
        director.ClickHere();
        const ImGuiTestItemInfo hidden_eye = branch_eye(ctx, sidebar, gpu_queues);
        if(hidden_eye.ID != 0)
        {
            director.PointAtItem(hidden_eye.ID, "shows");
            director.ClickHere();
        }
        director.Pause();
    }

    // Clicking a name selects the track everywhere.
    SidebarRow queue1;
    if(find_sidebar_track(ctx, sidebar, "Queue 1", queue1))
    {
        director.Say("Clicking a track's name selects it, both here and on the timeline.");
        director.ClickItem(queue1.name.ID);
        settle(director);
        director.Pause();

        // The right-click menu isolates or restores tracks, each chosen as the
        // line names it.
        director.Say("Right-click a track for more options. Hide All But This Track isolates "
                     "it, and Show All Tracks brings everything back.");
        director.PointAtItem(queue1.name.ID, "Right");
        director.ClickHere(ImGuiMouseButton_Right);
        click_popup_item_on(director, "Hide All But This Track", "Hide");
        settle(director);
        if(find_sidebar_track(ctx, sidebar, "Queue 1", queue1))
        {
            director.PointAtItem(queue1.name.ID, "isolates");
            director.ClickHere(ImGuiMouseButton_Right);
            click_popup_item_on(director, "Show All Tracks", "Show");
            settle(director);
        }
        // A second click on the name unselects the track again.
        if(find_sidebar_track(ctx, sidebar, "Queue 1", queue1))
        {
            director.ClickItem(queue1.name.ID);
        }
        director.Pause();
    }

    // The arrow scrolls the timeline to a track.
    SidebarRow counter;
    if(find_sidebar_track(ctx, sidebar, GO_TO_COUNTER, counter) && counter.go_to_id != 0)
    {
        director.Say("The arrow button scrolls the timeline straight to a track, like this CPU "
                     "time counter.");
        director.PointAtItem(counter.go_to_id, "arrow");
        director.WaitUntilSaid("straight");
        director.ClickHere();
        settle(director);
        director.Pause();
    }

    const ImGuiTestItemInfo processors = find_sidebar_branch(ctx, sidebar, "Processors");
    if(processors.ID != 0)
    {
        director.Say("On a branch, the same menu shows or hides all of the tracks below it.");
        director.PointAtItem(processors.ID, "branch");
        director.ClickHere(ImGuiMouseButton_Right);
        point_at_popup_item(director, "Show All Tracks Below", "shows");
        point_at_popup_item(director, "Hide All Tracks Below", "hides");
        director.Hold(HOVER_S);
        close_popups(director);
    }

    // Streams list the queues each stream dispatched to. The tree scrolls down
    // as the line begins, and each branch is reached as it is named.
    director.Say("Further down, the software side lists processes, their threads, and HIP "
                 "streams.");
    const ImVec2 sidebar_point = rect_point(window_rect(sidebar), 0.5f, 0.6f);
    director.PointAt(sidebar_point);
    director.Caption("Mouse wheel", 1.5f);
    director.Wheel(sidebar_point, -3.0f, 4, 0.2f);
    const std::pair<const char*, const char*> software[] = {
        { "Processes", "processes" }, { "Threads", "threads" }, { "Streams", "streams" }
    };
    for(const auto& [label, word] : software)
    {
        const ImGuiTestItemInfo branch = find_sidebar_branch(ctx, sidebar, label);
        if(branch.ID != 0)
        {
            director.PointAt(label_point(branch.RectFull), word);
        }
    }
    director.Hold(HOVER_S);
    // Under a stream, its work is listed again by the queue it ran on.
    director.Say("Streams group GPU work the way your application submitted it, while queues "
                 "show where the work actually ran.");
    const ImGuiTestItemInfo stream = find_sidebar_branch(ctx, sidebar, "Stream 1");
    ImGuiTestItemInfo       stream_queue;
    for(const ImGuiTestItemInfo& item : gather_items(ctx, sidebar))
    {
        if(stream.ID != 0 && stream_queue.ID == 0 && label_matches(item.DebugLabel, "Queue 1") &&
           item.RectFull.Min.y > stream.RectFull.Min.y)
        {
            stream_queue = item;
        }
    }
    if(stream.ID != 0)
    {
        director.PointAt(label_point(stream.RectFull), "Streams");
    }
    if(stream_queue.ID != 0)
    {
        director.PointAt(label_point(stream_queue.RectFull), "queues");
    }
    director.Hold(HOVER_S);
    director.Pause();
    director.Say("That's the System Topology panel.");
    director.Finish();
    show_all_tracks(trace_view);
}

// ---------------------------------------------------------------------------
// Chapter 05: working with tracks

static void
chapter_working_with_tracks(ImGuiTestContext* ctx)
{
    TraceView* trace_view = prepare_system_trace(ctx);
    if(trace_view == nullptr)
    {
        return;
    }
    std::shared_ptr<TimelineSelection> selection = trace_view->GetTimelineSelection();
    park_pointer(ctx, PARK_X, PARK_Y);

    Director director(ctx, ctx->Test->Name);
    if(!director.Start())
    {
        return;
    }
    director.Say("Now let's select, style, and arrange the tracks on the timeline.");
    director.Pause();

    // Selecting a track fills the details panel.
    std::vector<TrackRow> rows      = track_rows(trace_view);
    const TrackRow*       queue_row = find_row(rows, is_kernel_queue);
    if(queue_row != nullptr)
    {
        const TrackRow queue = *queue_row;
        director.Say("Click a track's description area to select it, and its events fill the "
                     "Event Table below.");
        director.PointAt(track_meta_point(queue), "Click");
        director.ClickHere();
        settle(director);
        director.Say("The Track Details tab describes the selected track itself.");
        click_details_tab_on(director, "Track Details", "Track");
        settle(director);
        director.Say("Switch back to the Event Table, and click the track again to clear the "
                     "selection.");
        click_details_tab_on(director, "Event Table", "Event");
        settle(director);
        director.PointAt(track_meta_point(queue), "click");
        director.ClickHere();
        director.Beat();

        // Utilization of the visible range, as a pill.
        director.Say("Queue tracks also show a utilization pill, which tells you how busy the "
                     "queue was across the visible time range.");
        director.PointAt(rect_point(queue.meta, 0.3f, 0.78f), "utilization");
        director.Hold(TOOLTIP_READ_S);

        // The right-click menu.
        director.Say("Right-click a track to copy its name or its ID, or to reveal it in the "
                     "System Topology panel.");
        director.PointAt(track_meta_point(queue), "Right");
        director.ClickHere(ImGuiMouseButton_Right);
        director.Hold(0.3f);
        point_at_popup_item(director, "Copy track name", "name");
        point_at_popup_item(director, "Copy track ID", "ID");
        click_popup_item_on(director, "Reveal in topology", "reveal");
        director.Pause();
    }

    // Event coloring and compact mode are per-track options.
    rows                  = track_rows(trace_view);
    const TrackRow* api   = find_row(rows, is_api_thread);
    FlameTrackItem* flame = api != nullptr ? dynamic_cast<FlameTrackItem*>(api->track) : nullptr;
    if(api != nullptr && flame != nullptr)
    {
        const View::EventTrackOptions::EventColorMode original =
            FlameTrackItemTestPeer{ *flame }.GetEventColorMode();
        auto color_by = [&director, trace_view](const char* mode, const char* word) {
            toggle_track_option_on(director, trace_view, is_api_thread, mode, word);
            director.Hold(1.0f);
        };
        director.Say("Track Options changes how a track's events are colored. No Color draws "
                     "them all alike.");
        color_by("No Color", "No");
        director.Say("Color by Time Level gives events with the same name different colors when "
                     "they start at different times.");
        color_by("Color by Time Level", "Level");
        director.Say("And Color by Name, the default, gives every event with the same name the "
                     "same color.");
        color_by("Color by Name", "Name");
        if(FlameTrackItemTestPeer{ *flame }.GetEventColorMode() != original)
        {
            FlameTrackItemTestPeer{ *flame }.SetEventColorMode(original);
        }
    }

    // Tall call stacks expand, or fold into compact rows.
    rows                   = track_rows(trace_view);
    const TrackRow* sample = find_row(rows, is_sampled_thread);
    if(sample != nullptr)
    {
        const TrackRow          sample_row = *sample;
        const ImGuiTestItemInfo expand     = find_item(ctx, sample_row.container, "##expand");
        TrackItem* const        tall_track = sample_row.track;
        auto                    is_tall    = [tall_track](TrackItem* track) {
            return track == tall_track;
        };
        if(expand.ID != 0)
        {
            director.Say("Tracks with deep call stacks can be expanded to show every level.");
            click_item_on(director, expand.ID, "expanded");
            settle(director);
            director.Hold(1.0f);
            rows = track_rows(trace_view);
            if(find_row(rows, is_tall) != nullptr)
            {
                director.Say("Compact Mode, under Track Options, squeezes those levels into less "
                             "height.");
                toggle_track_option_on(director, trace_view, is_tall, "Compact Mode", "squeezes");
                director.Hold(1.0f);
                director.Say("Turn Compact Mode off again to bring back the full height.");
                toggle_track_option_on(director, trace_view, is_tall, "Compact Mode", "off");
            }

            // The collapse arrow sits at the bottom of the expanded row, below
            // the fold, so the tracks scroll until it shows.
            director.Say("At the bottom of an expanded track, an arrow collapses it again.");
            // The wheel turns where it started, over the track list, so the
            // pointer doesn't chase the scrolling track between notches.
            int    scrolled = 0;
            ImVec2 wheel_at;
            for(int step = 0; step < 16; step++)
            {
                rows                   = track_rows(trace_view);
                const TrackRow* tall   = find_row(rows, is_tall);
                if(tall == nullptr)
                {
                    break;
                }
                const ImGuiTestItemInfo contract = find_item(ctx, tall->container, "##contract");
                if(contract.ID != 0)
                {
                    click_item_on(director, contract.ID, "arrow");
                    settle(director);
                    break;
                }
                if(scrolled == 0)
                {
                    wheel_at = rect_point(visible_part(tall->meta), 0.5f, 0.5f);
                    director.Caption("Mouse wheel", 3.0f);
                }
                director.Wheel(wheel_at, -1.0f, 1, 0.25f);
                scrolled++;
            }
            if(scrolled > 0)
            {
                director.PointAt(wheel_at);
                director.Wheel(wheel_at, 1.0f, scrolled + 2, 0.08f);
                settle(director);
            }
            director.Pause();
        }
    }

    // Drag a row's bottom edge to resize it.
    rows = track_rows(trace_view);
    if(const TrackRow* queue2 = find_row(rows, named("Queue 2")))
    {
        const ImVec2 edge(queue2->meta.GetCenter().x, queue2->meta.Max.y - 1.0f);
        director.Say("Drag the bottom edge of a track to make it taller, and drag it back when "
                     "you're done.");
        director.PointAt(edge, "bottom");
        director.Drag(edge, ImVec2(edge.x, edge.y + 70.0f), 1.0f);
        director.WaitUntilSaid("back");
        director.Drag(ImVec2(edge.x, edge.y + 70.0f), edge, 0.8f);
        director.Pause();
    }

    // Drag the grip on a row's left edge to reorder it.
    rows = track_rows(trace_view);
    const TrackRow* stream = find_row(rows, named("Stream 1"));
    const TrackRow* first  = rows.empty() ? nullptr : &rows.front();
    if(stream != nullptr && first != nullptr)
    {
        const ImVec2 grip(stream->meta.Min.x + 6.0f, stream->meta.GetCenter().y);
        const ImVec2 target(grip.x, first->meta.GetCenter().y);
        director.Say("To reorder tracks, drag the grip on a track's left edge. Here, Stream 1 "
                     "moves to the top.");
        director.PointAt(grip, "grip");
        director.Drag(grip, target, 1.2f);
        settle(director);
        director.Hold(1.0f);
        rows = track_rows(trace_view);
        if(!rows.empty())
        {
            director.Say("Sort Tracks puts them back in order, either by topology, or by track "
                         "type, which is the default.");
            open_track_menu(director, rows.front());
            point_at_popup_item(director, "Sort Tracks", "Sort");
            director.Hold(0.3f);
            point_at_popup_item(director, "Topology", "topology");
            click_popup_item_on(director, "Default", "default");
            rest_pointer(director, true);
            settle(director);
        }
        director.Pause();
    }

    // Counter tracks plot sampled values, with statistics pills.
    TrackItem* demo = find_track(trace_view, named_counter(PLOT_COUNTER));
    if(demo != nullptr)
    {
        director.Say("Counter tracks plot sampled values over time. Hover over the graph to "
                     "read a value.");
        director.Caption("Mouse wheel", 3.0f);
        rows = track_rows(trace_view);
        const ImRect graph = graph_area(rows);
        for(int i = 0; i < 12 && !row_fully_shown(trace_view, named_counter(PLOT_COUNTER)); i++)
        {
            director.Wheel(rect_point(rows.front().meta, 0.5f, 0.5f), -2.0f, 1, 0.15f);
        }
        settle(director);
        rows                     = track_rows(trace_view);
        const TrackRow* demo_row = find_row(rows, named_counter(PLOT_COUNTER));
        if(demo_row != nullptr && has_area(graph))
        {
            const TrackRow counter = *demo_row;
            ImVec2         peak    = rect_point(counter.chart, 0.55f, 0.5f);
            if(!peak_sample_point(trace_view, counter, peak))
            {
                ctx->LogWarning("No %s sample on screen to hover", PLOT_COUNTER);
            }
            director.PointAt(peak, "Hover");
            director.Hold(TOOLTIP_READ_S);
            director.Say("Their pills show the average and standard deviation across the "
                         "visible range.");
            director.PointAt(rect_point(counter.meta, AVERAGE_PILL_X, 0.78f), "average");
            director.Hold(TOOLTIP_READ_S);
            // The option leaves its menu open, so the same menu turns the boxes
            // back on and then shows the other counter options.
            director.Say("For counters, Track Options can turn off the filled boxes under the "
                         "line.");
            open_track_menu(director, counter);
            hover_popup_item(director, "Track Options", 0.5f);
            click_popup_item_on(director, "Show Counter Boxes", "off");
            // The boxes stay off long enough for the change to register.
            director.Pause();
            ctx->Sleep(0.6f);
            director.Say("Turn them back on the same way.");
            click_popup_item_on(director, "Show Counter Boxes", "back");
            director.Hold(0.6f);
            director.Say("You can also alternate the colors of the boxes, or highlight a range "
                         "of values.");
            point_at_popup_item(director, "Alternate Counter Coloring", "alternate");
            point_at_popup_item(director, "Highlight Y Range", "highlight");
            director.Hold(0.8f);
            leave_popups(director);
            director.Pause();
        }
        else
        {
            ctx->LogError("The %s counter row never came on screen", PLOT_COUNTER);
        }
    }

    director.Say("Those are the essentials of working with tracks.");
    director.Finish();
}

// ---------------------------------------------------------------------------
// Chapter 06: events, Event Details and flows

static void
chapter_events_and_flows(ImGuiTestContext* ctx)
{
    TraceView* trace_view = prepare_system_trace(ctx);
    if(trace_view == nullptr)
    {
        return;
    }
    std::shared_ptr<TimelineSelection> selection = trace_view->GetTimelineSelection();

    // Start on a handful of kernels: a few dispatches wide at this zoom.
    FlameTrackItem* queue = dynamic_cast<FlameTrackItem*>(find_track(trace_view, is_kernel_queue));
    const View::TimelineModel& model = trace_view->GetDataProvider()->DataModel().GetTimeline();
    const double               start = model.GetStartTime();
    const double               range = model.GetEndTime() - start;
    const double center = start + range * 0.6;
    frame_time_window(ctx, trace_view, center, range * 0.012);
    TraceEvent kernel;
    if(queue != nullptr && event_near(queue, center, kernel))
    {
        frame_time_window(ctx, trace_view, kernel.m_start_ts + kernel.m_duration * 1.5,
                          kernel.m_duration * 6.0);
    }
    park_pointer(ctx, PARK_X, PARK_Y);

    Director director(ctx, ctx->Test->Name);
    if(!director.Start())
    {
        return;
    }
    director.Say("Next, we'll inspect individual events, and follow the flows that connect "
                 "them.");
    director.Pause();

    // A taller details panel shows more of each event.
    director.Say("First, drag the splitter up to give the details panel more room.");
    drag_details_splitter(director, -DETAILS_GROWTH_Y);
    settle(director);

    std::vector<TrackRow> rows  = track_rows(trace_view);
    const ImRect          graph = graph_area(rows);
    const TrackRow*       q1    = find_row(rows, named("Queue 1"));
    const TrackRow*       q2    = find_row(rows, named("Queue 2"));
    const TrackRow*       api   = find_row(rows, is_api_thread);

    // Hovering shows an event's name, start and duration.
    EventBar k1;
    EventBar k2;
    EventBar call;
    const bool have_k1 =
        q1 != nullptr && pick_bar(ctx, dynamic_cast<FlameTrackItem*>(q1->track), "", 0.45f, graph, k1);
    const TrackRow* second_queue = q2 != nullptr ? q2 : q1;
    const bool      have_k2      = second_queue != nullptr &&
                         pick_bar(ctx, dynamic_cast<FlameTrackItem*>(second_queue->track), "",
                                  q2 != nullptr ? 0.55f : 0.8f, graph, k2) &&
                         (!have_k1 || k2.start_ns != k1.start_ns);
    const bool have_call =
        api != nullptr && pick_bar(ctx, dynamic_cast<FlameTrackItem*>(api->track), "", 0.5f, graph, call);
    if(have_k1)
    {
        director.Say("Hover over an event to see its name, start time, and duration, like this "
                     "kernel on a GPU queue.");
        director.Hover(k1.rect.GetCenter(), READ_S);
    }
    if(have_call)
    {
        director.Say("The same works for this HIP API call on a CPU thread.");
        director.PointAt(call.rect.GetCenter(), "HIP");
        director.Hold(READ_S);
    }
    director.Beat();

    // Click selects an event; its details and flows open below.
    if(have_k1)
    {
        director.Say("Click an event to select it, and then open the Event Details tab.");
        director.PointAt(k1.rect.GetCenter(), "Click");
        director.ClickHere();
        settle(director);
        click_details_tab_on(director, "Event Details", "Details");
        settle(director);
        director.Say("It shows the event's name, start, and duration. Under Flow Data, it lists "
                     "the other events this one is connected to.");
        director.Hold(READ_S);
        const ImGuiTestItemInfo extended =
            find_item(ctx, find_details_window(), "Event Extended Data");
        const ImRect details = window_rect(find_details_window());
        if(extended.ID != 0 && has_area(details))
        {
            director.Say("Expand Event Extended Data to see everything else the trace recorded, "
                         "such as the event's queue, stream, and dispatch IDs.");
            click_item_on(director, extended.ID, "Expand");
            settle(director);
            const ImVec2 inside = rect_point(details, 0.4f, 0.6f);
            director.Hover(inside, 0.6f);
            director.Caption("Mouse wheel", 2.0f);
            director.Wheel(inside, -3.0f, 2, 0.5f);
            director.Hold(READ_S);
        }
        director.Pause();
    }

    // Ctrl+click adds more events to the selection.
    if(have_k2)
    {
        director.Say("Hold Control and click another event to add it to the selection.");
        director.WaitUntilSaid("Control");
        director.HoldKeys(ImGuiMod_Ctrl, "Ctrl + Click");
        director.PointAt(k2.rect.GetCenter(), "click");
        director.ClickHere();
        director.ReleaseKeys();
        settle(director);
        director.Beat();
        rest_pointer(director);
        director.Pause();
    }

    // Flow arrows can be hidden or shown. Each of these kernels has a single flow,
    // so the fan and chain render styles would look the same here.
    ImGuiWindow*            toolbar    = find_toolbar_window();
    const ImGuiTestItemInfo hide_flows = find_item(ctx, toolbar, ICON_EYE_SLASH);
    const ImGuiTestItemInfo show_flows = find_item(ctx, toolbar, ICON_EYE);
    if(hide_flows.ID != 0 && show_flows.ID != 0)
    {
        director.Say("Flow arrows connect related events, like an API call and the kernel it "
                     "launched, so you can follow work from the CPU to the GPU.");
        director.Hold(READ_S);
        director.Say("The eye buttons under Flow in the toolbar hide the arrows, or show them "
                     "again.");
        click_item_on(director, hide_flows.ID, "hide");
        click_item_on(director, show_flows.ID, "show");
        director.Pause();
    }

    // A HIP API call carries its arguments.
    if(have_call)
    {
        // Its details fit the panel, so nothing needs scrolling.
        director.Say("Select a HIP API call, and Event Details also shows the arguments it "
                     "was called with.");
        director.PointAt(call.rect.GetCenter(), "Select");
        director.ClickHere();
        settle(director);
        rest_pointer(director);
        director.Hold(READ_S);
        director.Pause();
    }

    // Edit > Unselect All Events clears the selection.
    director.Say("To clear the selection, choose Edit, then Unselect All Events.");
    open_menu_on(director, "Edit", "Edit");
    click_popup_item_on(director, "Unselect All Events", "Unselect");
    rest_pointer(director, true);
    director.Pause();
    director.Say("Now you can inspect any event in a trace, and see how it connects to the "
                 "rest.");
    director.Finish();
    selection->UnselectAllEvents();
}

// ---------------------------------------------------------------------------
// Chapter 07: time ranges and measuring

static void
chapter_ranges_and_measure(ImGuiTestContext* ctx)
{
    TraceView* trace_view = prepare_system_trace(ctx);
    if(trace_view == nullptr)
    {
        return;
    }
    std::vector<TrackRow> rows      = wait_for_rows(ctx, trace_view, is_kernel_queue);
    const TrackRow*       queue_row = find_row(rows, is_kernel_queue);
    const ImRect          graph     = graph_area(rows);
    if(queue_row == nullptr || !has_area(graph))
    {
        ctx->LogError("Timeline rows are not on screen (%d rows)", static_cast<int>(rows.size()));
        return;
    }
    const TrackRow queue = *queue_row;
    const float    lane_y = queue.chart.GetCenter().y;
    auto           at     = [&graph, lane_y](float fx) {
        return ImVec2(graph.Min.x + graph.GetWidth() * fx, lane_y);
    };
    park_pointer(ctx, PARK_X, PARK_Y);

    Director director(ctx, ctx->Test->Name);
    if(!director.Start())
    {
        return;
    }
    director.Say("Next, let's select time ranges, and measure the time between events.");
    director.Pause();

    // Ctrl+drag marks a time range.
    director.Say("Hold Control and drag across the timeline to select a time range.");
    director.PointAt(at(0.40f), "Control");
    director.HoldKeys(ImGuiMod_Ctrl, "Ctrl + Drag");
    director.WaitUntilSaid("drag");
    director.Drag(at(0.40f), at(0.58f), 1.3f);
    director.ReleaseKeys();
    settle(director);
    director.Pause();

    // Its edges can be dragged.
    director.Say("Drag either edge to adjust the range.");
    director.PointAt(at(0.58f), "edge");
    director.Drag(at(0.58f), at(0.64f), 0.9f);
    settle(director);
    director.Beat();

    // The right-click menu zooms to it.
    director.Say("Right-click the range to remove it, or to zoom the timeline to fit it.");
    director.PointAt(at(0.5f), "Right");
    director.ClickHere(ImGuiMouseButton_Right);
    director.Hold(0.3f);
    point_at_popup_item(director, "Remove Time Range Selection", "remove");
    click_popup_item_on(director, "Zoom to Time Range Selection", "zoom");
    settle(director);
    director.Pause();

    director.Say("Press Escape to clear the range, and Reset View to see the whole trace "
                 "again.");
    director.MoveTo(at(0.5f));
    director.WaitUntilSaid("Escape");
    director.Key(ImGuiKey_Escape, "Esc");
    settle(director);
    ctx->SetRef(MAIN_WINDOW);
    click_item_on(director, "**/Reset View", "Reset");
    rest_pointer(director, true);
    settle(director);
    director.Pause();

    // M turns the selected events into a range, Z zooms to it.
    director.Say("You can also turn selected events into a range. First, zoom in and select "
                 "a kernel.");
    focus_timeline(director, queue);
    director.MoveTo(at(0.62f));
    director.WaitUntilSaid("zoom");
    director.KeyHold(ImGuiKey_W, "W", 1.4f);
    settle(director);
    rows = track_rows(trace_view);
    EventBar kernel;
    const TrackRow* q = find_row(rows, is_kernel_queue);
    if(q != nullptr &&
       pick_bar(ctx, dynamic_cast<FlameTrackItem*>(q->track), "", 0.5f, graph_area(rows), kernel))
    {
        director.PointAt(kernel.rect.GetCenter(), "kernel");
        director.ClickHere();
        settle(director);
        rest_pointer(director, true);
        director.Say("Then press the M key to make it the time range, and the Z key to zoom "
                     "to it.");
        director.WaitUntilSaid("M");
        director.Key(ImGuiKey_M, "M");
        settle(director);
        director.WaitUntilSaid("zoom");
        director.Key(ImGuiKey_Z, "Z");
        settle(director);
        director.Pause();
        // Back to the whole trace, off camera, for measuring.
        director.Cut([&]() { reset_view_off_camera(ctx, trace_view); });
    }
    else
    {
        click_reset_view(director);
    }

    // Measure the time between two events.
    ctx->SetRef(MAIN_WINDOW);
    director.Say("To measure the time between two events, turn on Measure mode in the "
                 "toolbar.");
    click_item_on(director, "**/measure_start/Measure", "mode");
    director.Beat();
    director.Say("In Events mode, the rulers snap to the events you click.");
    director.PointAtItem("**/measure_mode/Events", "Events");
    director.Hold(TOOLTIP_READ_S);
    director.Say("The Options menu chooses whether they snap to each event's start, or to its "
                 "end.");
    click_item_on(director, "**/Options", "Options");
    director.Hold(0.3f);
    point_at_popup_item(director, "Event start", "start");
    point_at_popup_item(director, "Event end", "end");
    director.Hold(0.4f);
    close_popups(director);

    rows = track_rows(trace_view);
    director.Say("Now zoom in, then click one event and then another, and Optiq shows the time "
                 "between them.");
    if(const TrackRow* current = find_row(rows, is_kernel_queue))
    {
        focus_timeline(director, *current);
    }
    director.MoveTo(at(0.62f));
    director.KeyHold(ImGuiKey_W, "W", 1.4f);
    settle(director);
    rows = track_rows(trace_view);
    q    = find_row(rows, is_kernel_queue);
    EventBar first;
    EventBar second;
    const ImRect zoomed = graph_area(rows);
    if(q != nullptr &&
       pick_bar(ctx, dynamic_cast<FlameTrackItem*>(q->track), "", 0.3f, zoomed, first) &&
       pick_bar(ctx, dynamic_cast<FlameTrackItem*>(q->track), "", 0.75f, zoomed, second) &&
       first.start_ns != second.start_ns)
    {
        director.PointAt(first.rect.GetCenter(), "click");
        director.ClickHere();
        director.PointAt(second.rect.GetCenter(), "another");
        director.ClickHere();
        settle(director);
        rest_pointer(director, true);
        director.Pause();

        MeasurementController* measurement =
            TraceViewTestPeer{ *trace_view }.MeasurementControllerPtr();
        const ImRect label =
            TimelineViewTestPeer{ *timeline_of(trace_view) }.MeasureDurationLabel();
        const ImVec2 label_point = label.GetCenter();
        if(measurement != nullptr && has_area(label) &&
           measurement->GetMeasurementState() == View::MeasurementState::kComplete)
        {
            director.Say("Right-click the measurement to copy its duration, clear it, or zoom "
                         "the timeline to fit it.");
            director.PointAt(label_point, "Right");
            director.ClickHere(ImGuiMouseButton_Right);
            director.Hold(0.3f);
            point_at_popup_item(director, "Copy Measurement Duration", "copy");
            point_at_popup_item(director, "Clear Measurement", "clear");
            if(!click_popup_item_on(director, "Zoom to Measurement", "zoom"))
            {
                close_popups(director);
            }
            settle(director);
            rest_pointer(director);
            director.Pause();
        }
        // Reset View slides under the pointer once Clear is gone.
        ctx->SetRef(MAIN_WINDOW);
        director.Say("Clear removes the measurement, and Reset View zooms back out.");
        click_item_on(director, "**/Clear", "Clear");
        rest_pointer(director, true);
        ctx->SetRef(MAIN_WINDOW);
        click_item_on(director, "**/Reset View", "Reset");
        rest_pointer(director, true);
        settle(director);
        director.Pause();
    }

    // Anywhere mode places the rulers freely.
    ctx->SetRef(MAIN_WINDOW);
    director.Say("Switch to Anywhere mode, and you can place the rulers at any two points on "
                 "the timeline.");
    click_item_on(director, "**/measure_mode/Events", "Anywhere");
    rows                   = track_rows(trace_view);
    const ImRect    full   = graph_area(rows);
    const TrackRow* lane   = find_row(rows, is_kernel_queue);
    if(has_area(full) && lane != nullptr)
    {
        const float y = lane->chart.GetCenter().y;
        director.PointAt(ImVec2(full.Min.x + full.GetWidth() * 0.3f, y), "place");
        director.ClickHere();
        director.PointAt(ImVec2(full.Min.x + full.GetWidth() * 0.7f, y), "points");
        director.ClickHere();
        settle(director);
        rest_pointer(director);
        director.Pause();
    }
    ctx->SetRef(MAIN_WINDOW);
    director.Say("When you're done, click Exit to leave Measure mode.");
    // The toolbar changes under the pointer, which would land on the file tab
    // and raise its tooltip, so it moves off straight away.
    click_item_on(director, "**/measure_exit/Exit", "Exit");
    rest_pointer(director, true);
    director.Pause();
    director.Say("And that's time ranges and measurements.");
    director.Finish();
}

// ---------------------------------------------------------------------------
// Chapter 08: the Advanced Details tables

// The table named `name` that `host` or one of its child windows drew last
// frame. Look it up again after yielding: the pool holding tables can move them.
static ImGuiTable*
find_table(ImGuiWindow* host, const char* name)
{
    ImGuiContext& g = *ImGui::GetCurrentContext();
    for(int i = 0; host != nullptr && i < g.Tables.GetMapSize(); i++)
    {
        ImGuiTable* table = g.Tables.TryGetMapData(i);
        if(table == nullptr || table->LastFrameActive < g.FrameCount - 1 ||
           table->InnerWindow == nullptr || std::strstr(table->InnerWindow->Name, name) == nullptr)
        {
            continue;
        }
        for(ImGuiWindow* window = table->OuterWindow; window != nullptr;
            window = window->ParentWindow)
        {
            if(window == host)
            {
                return table;
            }
        }
    }
    return nullptr;
}

// Index of the column of `table` named `column`, or -1.
static int
table_column_index(const ImGuiTable* table, const char* column)
{
    for(int n = 0; table != nullptr && n < table->ColumnsCount; n++)
    {
        if(std::strcmp(ImGui::TableGetColumnName(table, n), column) == 0)
        {
            return n;
        }
    }
    return -1;
}

// How far one notch of the mouse wheel scrolls `window` along `axis`, worked
// out the way ImGui does it.
static float
wheel_step(const ImGuiWindow* window, ImGuiAxis axis)
{
    if(axis == ImGuiAxis_X)
    {
        return ImTrunc(ImMin(2.0f * window->FontRefSize, window->InnerRect.GetWidth() * 0.67f));
    }
    return ImTrunc(ImMin(5.0f * window->FontRefSize, window->InnerRect.GetHeight() * 0.67f));
}

// Holds Shift and turns the wheel over the horizontal scrollbar of `table`, where
// no cell raises a tooltip. Negative notches scroll right.
static void
shift_wheel_table(Director& director, const ImGuiTable* table, int notches)
{
    const ImVec2 at = ImGui::GetWindowScrollbarRect(table->InnerWindow, ImGuiAxis_X).GetCenter();
    director.MoveTo(at);
    director.HoldKeys(ImGuiMod_Shift, "Shift + Mouse wheel");
    director.Wheel(at, static_cast<float>(notches), 1, WHEEL_BURST_GAP_S);
    director.ReleaseKeys();
    settle(director);
}

// Scrolls the table named `name` in `host` right until its column `index` is
// in view, when it is not already.
static void
reveal_table_column(Director& director, ImGuiWindow* host, const char* name, int index)
{
    const ImGuiTable* table = find_table(host, name);
    if(table == nullptr || index < 0 || index >= table->ColumnsCount)
    {
        return;
    }
    const ImGuiWindow* inner = table->InnerWindow;
    const float        step  = wheel_step(inner, ImGuiAxis_X);
    // A notch of margin keeps the column clear of the vertical scrollbar.
    const float overflow = table->Columns[index].MaxX + step - table->InnerClipRect.Max.x;
    const float room     = inner->ScrollMax.x - inner->Scroll.x;
    const int   notches  = static_cast<int>(ImCeil(ImMin(overflow, room) / step));
    if(notches > 0)
    {
        shift_wheel_table(director, table, -notches);
    }
}

// Scrolls the table named `name` in `host` back to its first column.
static void
scroll_table_home(Director& director, ImGuiWindow* host, const char* name)
{
    const ImGuiTable* table = find_table(host, name);
    if(table != nullptr && table->InnerWindow->Scroll.x > 0.0f)
    {
        const float step = wheel_step(table->InnerWindow, ImGuiAxis_X);
        shift_wheel_table(director, table,
                          static_cast<int>(ImCeil(table->InnerWindow->Scroll.x / step)));
    }
}

static void
chapter_details_tables(ImGuiTestContext* ctx)
{
    TraceView* trace_view = prepare_system_trace(ctx);
    if(trace_view == nullptr)
    {
        return;
    }
    std::shared_ptr<TimelineSelection> selection = trace_view->GetTimelineSelection();
    park_pointer(ctx, PARK_X, PARK_Y);

    Director director(ctx, ctx->Test->Name);
    if(!director.Start())
    {
        return;
    }
    director.Say("Now let's dig into the Advanced Details tables, where you can sort, filter, "
                 "and group events.");
    director.Pause();

    // Selected tracks feed the Event Table.
    std::vector<TrackRow> rows = track_rows(trace_view);
    director.Say("The tables show the tracks you select. Select a CPU thread and a GPU queue, "
                 "and the Event Table lists all of their events.");
    const std::pair<std::function<bool(TrackItem*)>, const char*> picks[] = {
        { is_api_thread, "thread" }, { named("Queue 1"), "queue" }
    };
    for(const auto& [match, word] : picks)
    {
        if(const TrackRow* row = find_row(rows, match))
        {
            director.PointAt(track_meta_point(*row), word);
            director.ClickHere();
            settle(director);
        }
    }
    director.Pause();

    // Give the tables room for more rows, scrolled from the short id column,
    // whose cells raise no truncation tooltips.
    director.Say("Drag the splitter up to see more rows at once, and scroll through them with "
                 "the mouse wheel.");
    drag_details_splitter(director, -260.0f, false);
    settle(director);
    ImGuiWindow* details = find_details_window();
    const ImRect panel   = window_rect(details);
    director.PointAt(rect_point(panel, ID_COLUMN_X, 0.62f), "scroll");
    director.Caption("Mouse wheel", 2.5f);
    director.Wheel(rect_point(panel, ID_COLUMN_X, 0.62f), -3.0f, 3, 0.35f);

    // Sort by a column.
    const ImGuiTestItemInfo duration = await_item(director, find_details_window, "duration", true);
    if(duration.ID != 0)
    {
        director.Say("Click a column header to sort by that column, and click it again to "
                     "reverse the order.");
        click_item_on(director, duration.ID, "Click");
        settle_request(director);
        director.WaitUntilSaid("again");
        director.ClickHere();
        settle_request(director);
        director.Pause();
    }

    // Basic filters: a box under every column header.
    const ImGuiTestItemInfo name_header = await_item(director, find_details_window, "name", true);
    ImGuiTestItemInfo       name_filter;
    for(const ImGuiTestItemInfo& item : gather_items(ctx, find_details_window(), true))
    {
        if(name_header.ID != 0 && std::strcmp(item.DebugLabel, "##input_text_with_clear") == 0 &&
           item.RectFull.Min.y > name_header.RectFull.Min.y &&
           item.RectFull.Min.x < name_header.RectFull.Max.x &&
           item.RectFull.Max.x > name_header.RectFull.Min.x)
        {
            name_filter = item;
            break;
        }
    }
    if(name_filter.ID == 0)
    {
        ctx->LogError("No filter box under the name column");
    }
    else
    {
        // The pointer waits on the id column, clear of the text being typed.
        const ImVec2 aside = rect_point(window_rect(find_details_window()), ID_COLUMN_X, 0.62f);
        director.Say("The box under each header filters its column. Type hipMemcpy under name, "
                     "and only the memory copy calls remain.");
        click_item_on(director, name_filter.ID, "Type");
        director.MoveTo(aside);
        director.Type("hipMemcpy");
        director.Key(ImGuiKey_Enter, "Enter");
        settle_request(director);
        director.Pause();
        // In Basic mode the reset button empties the boxes without querying the
        // table again; a filter only applies when Enter is pressed in its box.
        director.Say("To remove the filter, delete the text and press Enter.");
        click_item_on(director, name_filter.ID, "delete");
        director.MoveTo(aside);
        director.Key(ImGuiMod_Ctrl | ImGuiKey_A, "Ctrl + A", 1, 0.15f);
        director.Key(ImGuiKey_Backspace, "Backspace", 1, 0.15f);
        director.WaitUntilSaid("Enter");
        director.Key(ImGuiKey_Enter, "Enter", 1, 0.2f);
        settle_request(director);
        director.Beat();
    }

    // Advanced mode: aggregate rows by a column, or filter with expressions.
    const ImGuiTestItemInfo filter_mode = find_item(ctx, details, ICON_FUNNEL);
    if(filter_mode.ID != 0)
    {
        director.Say("Switch the filter mode to Advanced, and you can group rows by a column, or "
                     "filter them with expressions.");
        click_item_on(director, filter_mode.ID, "mode");
        director.Hold(0.4f);
        click_popup_item_on(director, "Advanced", "Advanced");
        settle(director);
        director.Beat();
    }
    ctx->SetRef(MAIN_WINDOW);
    const ImGuiTestItemInfo submit   = ctx->ItemInfo("**/Submit", ImGuiTestOpFlags_NoError);
    const ImGuiID           group_by = ImHashStr("##group_by", 0, submit.ParentID);
    if(submit.ID != 0 && ctx->ItemExists(group_by))
    {
        director.Say("Group by name and click Submit, and each event name becomes a single "
                     "row.");
        click_item_on(director, group_by, "Group");
        director.Hold(0.3f);
        click_popup_item_on(director, "name", "name");
        click_item_on(director, submit.ID, "Submit");
        settle_request(director);
        director.Hold(1.0f);
        director.Pause();
        director.Say("Now set the grouping back to None, and filter with an expression "
                     "instead.");
        click_item_on(director, group_by, "grouping");
        director.Hold(0.3f);
        click_popup_item_on(director, "-- None --", "None");
        director.ClickItem(submit.ID);
        settle_request(director);
        director.Beat();

        director.Say("Durations are in nanoseconds, so duration greater than one million keeps "
                     "only the events longer than a millisecond.");
        click_item_on(director, "**/filters/##input_text_with_clear", "duration");
        director.Type("duration > 1000000");
        director.Hold(0.3f);
        director.ClickItem(submit.ID);
        settle_request(director);
        // After the grouping is reset, duration sits far to the right, so the
        // table scrolls over to show the durations the filter kept.
        const int duration_column =
            table_column_index(find_table(find_details_window(), DETAILS_TABLE), "duration");
        if(duration_column < 0)
        {
            ctx->LogError("No duration column in the Event Table");
        }
        else
        {
            director.Say("Scroll across to the duration column to check the values that are "
                         "left.");
            reveal_table_column(director, find_details_window(), DETAILS_TABLE, duration_column);
            const ImGuiTestItemInfo filtered =
                await_item(director, find_details_window, "duration", true);
            if(filtered.ID != 0)
            {
                director.PointAtItem(filtered.ID, "duration");
                rest_pointer(director);
                director.Hold(1.2f);
            }
        }
        director.Pause();
    }

    // Right-click a row to jump to its event on the timeline.
    director.Say("Right-click a row to copy its data, or choose Go To Event to find it on the "
                 "timeline.");
    scroll_table_home(director, find_details_window(), DETAILS_TABLE);
    ImVec2 row_point;
    if(first_table_row(ctx, find_details_window(), "name", row_point))
    {
        director.PointAt(row_point, "Right");
        director.ClickHere(ImGuiMouseButton_Right);
        director.Hold(0.3f);
        point_at_popup_item(director, "Copy Row Data", "copy");
        if(!click_popup_item_on(director, "Go To Event", "Event"))
        {
            close_popups(director);
        }
        settle(director);
        rest_pointer(director);
    }
    director.Pause();
    const ImGuiTestItemInfo reset_advanced = find_item(ctx, details, ICON_ARROWS_CYCLE);
    if(reset_advanced.ID != 0)
    {
        director.Say("In Advanced mode, the reset button clears the grouping and the filter.");
        click_item_on(director, reset_advanced.ID, "reset");
        settle_request(director);
        rest_pointer(director);
        director.Pause();
    }

    // A time-range selection limits every table to that span.
    director.Cut([&]() { reset_view_off_camera(ctx, trace_view); });
    rows              = track_rows(trace_view);
    const ImRect full = graph_area(rows);
    if(!rows.empty() && has_area(full))
    {
        const float  y = visible_part(rows.front().chart).GetCenter().y;
        const ImVec2 range_start(full.Min.x + full.GetWidth() * 0.45f, y);
        director.Say("Back on the whole trace, a time range selection limits every table to "
                     "just that span.");
        director.PointAt(range_start, "range");
        director.HoldKeys(ImGuiMod_Ctrl, "Ctrl + Drag");
        director.Drag(range_start, ImVec2(full.Min.x + full.GetWidth() * 0.6f, y), 1.2f);
        director.ReleaseKeys();
        settle(director);
        rest_pointer(director);
        director.Pause();
    }

    // Top Events and Track Details summarize the selected tracks.
    director.Say("Top Events ranks the events in the selected tracks by name, with their "
                 "counts and durations.");
    click_details_tab_on(director, "Top Events", "Top");
    settle(director);
    director.Hover(rect_point(window_rect(find_details_window()), 0.4f, 0.5f), READ_S);
    director.Pause();

    // Track Details misreports the end of the thread track, so only the queue
    // stays selected for it.
    director.Say("With just the queue selected, Track Details shows how busy the queue was "
                 "during the range.");
    rows = track_rows(trace_view);
    if(const TrackRow* api = find_row(rows, is_api_thread))
    {
        director.PointAt(track_meta_point(*api), "just");
        director.ClickHere();
        settle(director);
    }
    click_details_tab_on(director, "Track Details", "Details");
    settle(director);
    director.Hover(rect_point(window_rect(find_details_window()), 0.4f, 0.5f), READ_S);
    director.Pause();

    // Counter tracks fill the Sample Table. Sidebar clicks add to the selection,
    // so the queue is unselected first.
    ImGuiWindow* sidebar = find_sidebar_window(ctx);
    director.Say("Counter tracks have their own table. To start fresh, choose Edit, then "
                 "Unselect All Tracks.");
    open_menu_on(director, "Edit", "Edit");
    click_popup_item_on(director, "Unselect All Tracks", "Unselect");
    rest_pointer(director, true);
    settle(director);
    SidebarRow gfx;
    if(sidebar != nullptr && find_sidebar_track(ctx, sidebar, "GFX Busy", gfx))
    {
        director.Say("Then select a counter, like GFX Busy, and the Sample Table lists its "
                     "values.");
        click_item_on(director, gfx.name.ID, "select");
        settle(director);
        click_details_tab_on(director, "Sample Table", "Sample");
        settle(director);
        rest_pointer(director, true);
        director.Hold(1.2f);
        director.Pause();
    }
    director.Say("And those are the Advanced Details tables.");
    director.Finish();
    selection->UnselectAllTracks();
}

// ---------------------------------------------------------------------------
// Chapter 09: search, the minimap and the summary

static ImGuiWindow*
newest_popup()
{
    ImGuiContext& g = *ImGui::GetCurrentContext();
    for(int i = g.OpenPopupStack.Size - 1; i >= 0; i--)
    {
        ImGuiWindow* popup = g.OpenPopupStack[i].Window;
        if(popup != nullptr && popup->WasActive)
        {
            return popup;
        }
    }
    return nullptr;
}

static ImRect
newest_popup_rect()
{
    return window_rect(newest_popup());
}

static void
chapter_search_minimap_summary(ImGuiTestContext* ctx)
{
    TraceView* trace_view = prepare_system_trace(ctx);
    if(trace_view == nullptr)
    {
        return;
    }
    park_pointer(ctx, PARK_X, PARK_Y);

    Director director(ctx, ctx->Test->Name);
    if(!director.Start())
    {
        return;
    }
    director.Say("Next, we'll search for events, get an overview with the minimap, and see the "
                 "top kernels in the summary.");
    director.Pause();

    // Search by event name; a result jumps to its event.
    ctx->SetRef(MAIN_WINDOW);
    director.Say("Type a name into the search box and press Enter. Search ignores case, and "
                 "matches any part of an event's name.");
    click_item_on(director, "**/search_bar/##input_text_with_clear", "Type");
    director.Type("transpose_a");
    director.WaitUntilSaid("Enter");
    director.Key(ImGuiKey_Enter, "Enter");
    settle(director);
    director.Hold(1.5f);
    const ImRect results = newest_popup_rect();
    if(has_area(results))
    {
        const float  row_height = ImGui::GetFrameHeight();
        const ImVec2 first_row(results.Min.x + results.GetWidth() * 0.3f,
                               results.Min.y + row_height * 3.2f);
        director.Say("Click a result to jump straight to that event on the timeline.");
        director.PointAt(first_row, "Click");
        director.ClickHere();
        settle(director);
        director.Pause();
    }
    director.Say("To search for several terms at once, put each one in quotes. By default, "
                 "results must match all of them.");
    // The new terms are typed over the old ones.
    ctx->SetRef(MAIN_WINDOW);
    director.ClickItem("**/search_bar/##input_text_with_clear");
    director.Key(ImGuiMod_Ctrl | ImGuiKey_A, "Ctrl + A", 1, 0.15f);
    director.Type("\"hip\"\"Memcpy\"");
    director.Key(ImGuiKey_Enter, "Enter", 1, 0.2f);
    settle(director);
    director.Hold(1.5f);
    director.Say("Clear the search when you're done, and Reset View takes you back to the whole "
                 "trace.");
    ctx->SetRef(MAIN_WINDOW);
    const std::string clear_ref =
        std::string("**/search_bar/") + ICON_X_CIRCLED + "/" + ICON_X_CIRCLED;
    click_item_on(director, clear_ref.c_str(), "Clear");
    ctx->SetRef(MAIN_WINDOW);
    click_item_on(director, "**/Reset View", "Reset");
    rest_pointer(director, true);
    settle(director);
    director.Pause();

    // The minimap shows where events and counter peaks are across the trace.
    // The timeline is zoomed in first: with the whole trace in view, a click on
    // the minimap would have no other time to move it to.
    ImGuiWindow*            toolbar = find_toolbar_window();
    const ImGuiTestItemInfo compass = find_item(ctx, toolbar, ICON_COMPASS);
    std::vector<TrackRow>   rows    = track_rows(trace_view);
    const TrackRow*         queue   = find_row(rows, is_kernel_queue);
    if(compass.ID != 0 && queue != nullptr)
    {
        const TrackRow queue_row = *queue;
        director.Say("Zoom in on part of the trace, then open the minimap with the compass "
                     "button.");
        focus_timeline(director, queue_row);
        director.MoveTo(ImVec2(queue_row.chart.Min.x + queue_row.chart.GetWidth() * 0.6f,
                               queue_row.chart.GetCenter().y));
        director.KeyHold(ImGuiKey_W, "W", 1.3f);
        settle(director);
        click_item_on(director, compass.ID, "compass");
        director.WaitFor(
            []() {
                ImGuiWindow* window = ImGui::FindWindowByName("Minimap");
                return window != nullptr && window->WasActive;
            },
            0.5f, ITEM_TIMEOUT_S);
        ImGuiWindow* minimap = ImGui::FindWindowByName("Minimap");
        if(minimap != nullptr && minimap->WasActive)
        {
            ctx->SetRef("//Minimap");
            const ImRect map = minimap->Rect();
            director.Say("The minimap is an overview of the whole trace. It shows where events "
                         "are dense, where counters peak, and which part you're looking at.");
            director.Hover(rect_point(map, 0.5f, 0.55f), READ_S);
            if(ctx->ItemExists("**/##events"))
            {
                director.Say("You can turn the event layer off, and back on.");
                click_item_on(director, "**/##events", "off");
                director.WaitUntilSaid("back");
                director.ClickHere();
            }
            if(ctx->ItemExists("**/##counters"))
            {
                director.Say("The counter layer works the same way.");
                click_item_on(director, "**/##counters", "counter");
                director.Hold(0.8f);
                director.ClickHere();
            }
            director.Beat();
            // The minimap moves the view on a click, centering it on the clicked
            // time and height at the same zoom. Clicking at the height of the
            // part in view moves it through time only.
            const ImGuiTestItemInfo hit      = find_item_exact(ctx, minimap, "Hit");
            TimelineView*           timeline = timeline_of(trace_view);
            if(hit.ID != 0 && timeline != nullptr)
            {
                const float total_h = ImMax(timeline->GetTotalTrackHeight(), 1.0f);
                const float view_y  = static_cast<float>(timeline->GetViewCoords().y) +
                                     timeline->GetGraphSize().y * 0.5f;
                const float click_y = hit.RectFull.Min.y +
                                      hit.RectFull.GetHeight() * ImClamp(view_y / total_h, 0.0f, 1.0f);
                const auto  spot    = [&hit, click_y](float fx) {
                    return ImVec2(hit.RectFull.Min.x + hit.RectFull.GetWidth() * fx, click_y);
                };
                director.Say("Click anywhere on the minimap to move the timeline there.");
                director.PointAt(spot(0.3f), "Click");
                director.ClickHere();
                settle(director);
                director.PointAt(spot(0.7f), "there");
                director.ClickHere();
                settle(director);
                director.Hold(0.8f);
            }
            else
            {
                ctx->LogError("The minimap has no navigation area");
            }
            director.Pause();
        }
        else
        {
            ctx->LogError("The minimap did not open");
        }
        director.Say("Click the compass again to close the minimap, and reset the view.");
        click_item_on(director, compass.ID, "compass");
        rest_pointer(director, true);
        ctx->SetRef(MAIN_WINDOW);
        click_item_on(director, "**/Reset View", "reset");
        rest_pointer(director, true);
        settle(director);
        director.Pause();
    }
    else
    {
        click_reset_view(director);
    }

    // View > Show Summary lists the top kernels.
    director.Say("Choose View, then Show Summary, to chart the top kernels by execution time.");
    open_menu_on(director, "View", "View");
    click_popup_item_on(director, "Show Summary", "Summary");
    settle(director, 1.0f);
    ImGuiWindow* summary = ImGui::FindWindowByName("Summary");
    if(summary != nullptr && summary->WasActive)
    {
        // Resting below the chart, the pointer is clear of the window's resize
        // border and doesn't turn the pie to its black hover color.
        director.Hover(rect_point(summary->Rect(), 0.4f, SUMMARY_REST_Y), READ_S);

        // Selecting a kernel lists its dispatches below the chart. A hovered
        // slice turns black, and this kernel is nearly the whole pie, so the
        // pointer leaves the chart as soon as it has clicked, for the empty
        // stretch right of the chart buttons, clear of the list as well.
        const ImVec2 wedge = rect_point(summary->Rect(), 0.46f, 0.25f);
        ctx->SetRef("//Summary");
        const std::string       list_ref = std::string("**/") + ICON_LIST + "/" + ICON_LIST;
        const ImGuiTestItemInfo list_button =
            ctx->ItemInfo(list_ref.c_str(), ImGuiTestOpFlags_NoError);
        const ImVec2 aside =
            list_button.ID != 0
                ? ImVec2(list_button.RectFull.Max.x + SUMMARY_ASIDE_X,
                         list_button.RectFull.GetCenter().y)
                : rect_point(summary->Rect(), 0.4f, SUMMARY_REST_Y);
        director.Say("Select a kernel to list its dispatches below the chart.");
        director.PointAt(wedge, "Select");
        director.ClickHere();
        director.MoveTo(aside);
        settle(director);
        director.Hold(READ_S);
        director.Pause();
        ctx->SetRef("//Summary");
        director.Say("The buttons at the bottom switch the chart between bars, a table, and the "
                     "pie chart.");
        const std::pair<const char*, const char*> views[] = {
            { ICON_CHART_BAR, "bars" }, { ICON_LIST, "table" }, { ICON_CHART_PIE, "pie" }
        };
        for(const auto& [icon, word] : views)
        {
            const std::string ref = std::string("**/") + icon + "/" + icon;
            if(ctx->ItemExists(ref.c_str()))
            {
                click_item_on(director, ref.c_str(), word);
                settle(director);
            }
        }
        director.Hold(1.0f);
        director.Pause();
    }
    director.Say("That's search, the minimap, and the summary.");
    director.Finish();
}

// ---------------------------------------------------------------------------
// Chapter 10: annotations, bookmarks and projects

static ImGuiWindow*
newest_sticky_note()
{
    return find_window("StickyNoteWindow##");
}

// Scrolls the tracks, off camera, so the first track `match` accepts sits
// halfway down the timeline, where the toolbar's plus button attaches a note.
// Call while the tracks are scrolled to the top, as prepare_system_trace()
// leaves them, so the first row starts the content.
static void
center_track_vertically(ImGuiTestContext* ctx, TraceView* trace_view,
                        const std::function<bool(TrackItem*)>& match)
{
    TimelineView*               timeline = timeline_of(trace_view);
    const std::vector<TrackRow> rows     = track_rows(trace_view);
    const TrackRow*             row      = find_row(rows, match);
    if(timeline == nullptr || row == nullptr)
    {
        ctx->LogError("The track to center is not on the timeline");
        return;
    }
    const float content_y = row->meta.GetCenter().y - rows.front().meta.Min.y;
    const View::TimelineModel& model = trace_view->GetDataProvider()->DataModel().GetTimeline();
    timeline->MoveToPosition(model.GetStartTime(), model.GetEndTime(),
                             content_y - timeline->GetGraphSize().y * 0.5f, false);
    ctx->Yield(6);
    settle_off_camera(ctx);
    ctx->Yield(6);
}

// Types the newest note's title and text, each started as the narration says
// its word.
static void
write_note_on(Director& director, const char* title, const char* text, const char* title_word,
              const char* text_word)
{
    ImGuiTestContext* ctx  = director.Context();
    ImGuiWindow*      note = newest_sticky_note();
    if(note == nullptr)
    {
        ctx->LogWarning("No annotation window opened");
        return;
    }
    const ImGuiTestItemInfo title_field = find_item(ctx, note, "##title_");
    if(title_field.ID != 0)
    {
        click_item_on(director, title_field.ID, title_word);
        director.Type(title);
        director.Key(ImGuiKey_Enter, "Enter");
    }
    director.PointAt(rect_point(note->Rect(), 0.5f, 0.7f), text_word);
    director.ClickHere();
    director.Type(text);
}

// Closes the newest note as the narration says `word`.
static void
close_note_on(Director& director, const char* word)
{
    ImGuiTestContext* ctx  = director.Context();
    ImGuiWindow*      note = newest_sticky_note();
    if(note != nullptr)
    {
        const ImGuiTestItemInfo close = find_item(ctx, note, ICON_X_CIRCLED);
        if(close.ID != 0)
        {
            click_item_on(director, close.ID, word);
        }
    }
    rest_pointer(director);
}

static void
chapter_annotations_bookmarks(ImGuiTestContext* ctx)
{
    TraceView* trace_view = prepare_system_trace(ctx);
    if(trace_view == nullptr)
    {
        return;
    }
    // Left at the top, the tracks put a sampled thread halfway down, and the
    // Annotations tab names sampled threads with their "(S)" twice, so the
    // plus button's note goes on Queue 1 instead.
    center_track_vertically(ctx, trace_view, named("Queue 1"));
    park_pointer(ctx, PARK_X, PARK_Y);

    Director director(ctx, ctx->Test->Name);
    if(!director.Start())
    {
        return;
    }
    director.Say("Next, let's annotate a trace, bookmark views, and save our work as a "
                 "project.");
    director.Pause();

    // Add an annotation from the toolbar and write the note.
    ctx->SetRef(MAIN_WINDOW);
    const std::string add_note = std::string("**/add_new_sticky/") + ICON_ADD_NOTE;
    director.Say("Click the plus button in the toolbar to add an annotation.");
    click_item_on(director, add_note.c_str(), "plus");
    director.Hold(0.8f);
    // The note's header shows about ten characters of its title.
    director.Say("Give it a short title, and then write your note. A note can record what you "
                 "found, or remind you to come back later.");
    write_note_on(director, "Overview", "Annotations stay attached to their track.", "title",
                  "note");
    director.Say("Close the note, and it's saved on the timeline.");
    close_note_on(director, "Close");
    director.Pause();

    // Annotations stay on their track; the marker can be dragged along it. Each
    // marker is a button in its own marker-sized child window of the overlay.
    std::vector<TrackRow> rows   = track_rows(trace_view);
    const ImRect          graph  = graph_area(rows);
    ImGuiWindow*          marker = find_window("StickyButtonArea##");
    if(marker != nullptr)
    {
        const ImVec2 from = marker->Rect().GetCenter();
        director.Say("Each annotation is attached to a track. Drag its marker to move it along "
                     "the track.");
        director.PointAt(from, "marker");
        director.Hold(0.3f);
        director.Drag(from, ImVec2(from.x + graph.GetWidth() * 0.12f, from.y), 1.1f);
        director.Pause();
    }
    else
    {
        ctx->LogError("No annotation marker on the timeline");
    }

    // Right-click the timeline to annotate a specific spot: here, before the
    // first kernel reaches the GPU queues.
    if(const TrackRow* q = find_row(rows, named("Queue 2")))
    {
        const ImVec2 spot(graph.Min.x + graph.GetWidth() * 0.08f, q->chart.GetCenter().y);
        director.Say("To mark a specific moment, right-click the timeline and choose Add "
                     "Annotation, like here, in the idle gap before the first kernel.");
        director.PointAt(spot, "right");
        director.ClickHere(ImGuiMouseButton_Right);
        director.Hold(0.3f);
        click_popup_item_on(director, "Add Annotation", "Add");
        director.Hold(0.8f);
        director.Say("Title this one Idle gap, and note that the GPU queues stay idle until the "
                     "first kernel launches.");
        write_note_on(director, "Idle gap",
                      "The GPU queues stay idle until the first kernel launch.", "Title", "note");
        director.Pause();
    }

    // The Annotations tab lists every note. A row brings back the view its note
    // was added in, the whole trace here, so the timeline first zooms in away
    // from both notes.
    director.Say("Close it, and zoom in. Notes can fall out of view as you zoom, but the "
                 "Annotations tab still lists every one.");
    close_note_on(director, "Close");
    rows = track_rows(trace_view);
    if(const TrackRow* q = find_row(rows, named("Queue 1")))
    {
        focus_timeline(director, *q);
        director.MoveTo(ImVec2(graph.Min.x + graph.GetWidth() * 0.7f, q->chart.GetCenter().y));
        director.WaitUntilSaid("zoom");
        director.KeyHold(ImGuiKey_W, "W", 1.3f);
        settle(director);
    }
    click_details_tab_on(director, "Annotations", "Annotations");
    const ImGuiTestItemInfo idle_gap =
        await_item(director, find_details_window, "Idle gap", false);
    if(idle_gap.ID != 0)
    {
        director.Say("Click one to bring it back into view.");
        click_item_on(director, idle_gap.ID, "Click");
        settle(director);
    }
    rest_pointer(director);
    director.Hold(1.2f);
    director.Pause();

    // The toolbar hides or shows the annotation layer.
    ctx->SetRef(MAIN_WINDOW);
    const std::string hide_notes = std::string("**/hide_all_stickies/") + ICON_EYE_THIN;
    const std::string show_notes = std::string("**/show_all_stickies/") + ICON_EYE;
    director.Say("The eye buttons next to the plus button hide all annotations, or show them "
                 "again.");
    click_item_on(director, hide_notes.c_str(), "hide");
    click_item_on(director, show_notes.c_str(), "show");
    director.Pause();

    // Bookmarks: Ctrl+number saves the view, the number restores it.
    rows = track_rows(trace_view);
    if(const TrackRow* q = find_row(rows, is_kernel_queue))
    {
        director.Say("Bookmarks save a view so you can come back to it. Zoom in, then press "
                     "Control and a number, like Control 1.");
        focus_timeline(director, *q);
        director.MoveTo(ImVec2(graph.Min.x + graph.GetWidth() * 0.7f, q->chart.GetCenter().y));
        director.WaitUntilSaid("Zoom");
        director.KeyHold(ImGuiKey_W, "W", 1.3f);
        settle(director);
        director.WaitUntilSaid("like");
        director.Key(ImGuiMod_Ctrl | ImGuiKey_1, "Ctrl + 1");
        director.Pause();
        director.Say("Reset the view, and then press the number on its own to jump straight back "
                     "to the bookmark.");
        ctx->SetRef(MAIN_WINDOW);
        click_item_on(director, "**/Reset View", "Reset");
        rest_pointer(director, true);
        settle(director);
        director.MoveTo(ImVec2(graph.Min.x + graph.GetWidth() * 0.5f, q->chart.GetCenter().y));
        director.WaitUntilSaid("number");
        director.Key(ImGuiKey_1, "1");
        settle(director);
        director.Pause();
    }
    ImGuiWindow* toolbar = find_toolbar_window();
    if(toolbar != nullptr)
    {
        const ImGuiID bookmarks = ImHashStr("bookmark_toggle_dropdown", 0, toolbar->ID);
        if(ctx->ItemExists(bookmarks))
        {
            director.Say("The bookmarks menu in the toolbar lists your bookmarks too.");
            click_item_on(director, bookmarks, "menu");
            director.Hold(0.4f);
            // The pointer points beside the number of the bookmark just saved,
            // clear of the other rows, and chooses it: the list closes, and the
            // view is already there.
            const ImGuiTestItemInfo saved = find_item_exact(ctx, newest_popup(), "1");
            if(saved.ID != 0)
            {
                const ImVec2 beside(saved.RectFull.Min.x +
                                        saved.RectFull.GetWidth() * BOOKMARK_ROW_X,
                                    saved.RectFull.GetCenter().y);
                director.PointAt(beside, "lists");
                director.Hold(0.8f);
                director.ClickHere();
                rest_pointer(director, true);
            }
            else
            {
                director.Hold(1.0f);
                close_popups(director);
            }
            director.Pause();
        }
    }

    // Save the session as a project file.
    director.Say("To keep your annotations, bookmarks, and track layout, save the session as "
                 "a project with File, Save As.");
    open_menu_on(director, "File", "File");
    point_at_popup_item(director, "Save As", "Save");
    click_to_open_file_dialog(director);
    director.Hold(0.6f);
    ImGuiWindow*            dialog     = find_file_dialog_window();
    const ImGuiTestItemInfo name_field = find_item(ctx, dialog, "##FileName");
    if(name_field.ID != 0)
    {
        director.Say("Give the project a name, and click OK.");
        click_item_on(director, name_field.ID, "name");
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_A);
        director.Type(PROJECT_FILE_STEM);
        const ImGuiTestItemInfo ok = find_item(ctx, find_file_dialog_window(), "OK");
        if(ok.ID != 0)
        {
            director.PointAt(ok.RectFull.GetCenter(), "OK");
            director.ClickHere();
        }
        settle(director);
        director.Pause();
    }
    dismiss_dialogs(ctx);
    director.Say("Open the project later to pick up right where you left off, and use File, "
                 "Save to update it.");
    open_menu_on(director, "File", "File");
    point_at_popup_item(director, "Save", "Save");
    director.Hold(0.8f);
    leave_popups(director);
    director.Pause();
    director.Say("That's annotations, bookmarks, and projects.");
    director.Finish();
}

// ---------------------------------------------------------------------------
// Chapter 11: customizing Optiq

static ImGuiWindow*
settings_content()
{
    return find_window("/SettingsContent_");
}

static void
open_preferences(Director& director)
{
    open_menu(director, "Edit");
    click_popup_item(director, "Preferences", 0.8f);
    director.Hold(1.0f);
}

static void
settings_category(Director& director, const char* category)
{
    ImGuiWindow* categories = find_window("/SettingsCategories_");
    if(categories == nullptr)
    {
        return;
    }
    director.ClickItem(ImHashStr(category, 0, categories->ID));
    director.Hold(0.8f);
}

// Picks `choice` from `combo`. The list closes over the settings below it, some
// with tooltips, so the pointer goes back to the combo, which has none.
static void
settings_combo(Director& director, const char* combo, const char* choice)
{
    ImGuiWindow* content = settings_content();
    if(content == nullptr)
    {
        return;
    }
    const ImGuiID id  = ImHashStr(combo, 0, content->ID);
    const ImRect  box = item_rect(director.Context(), id);
    director.ClickItem(id);
    director.Hold(0.8f);
    click_popup_item(director, choice, 0.8f);
    if(has_area(box))
    {
        move_pointer(director, box.GetCenter(), true);
    }
    director.Hold(1.0f);
}

static void
settings_button(Director& director, const char* label)
{
    ImGuiWindow* content = settings_content();
    if(content == nullptr || content->ParentWindow == nullptr)
    {
        return;
    }
    const ImGuiTestItemInfo button = find_item(director.Context(), content->ParentWindow, label);
    if(button.ID != 0)
    {
        director.Hover(button.RectFull.GetCenter(), 0.6f);
        director.Click(button.RectFull.GetCenter());
    }
    director.Beat();
}

// settings_combo(), opened as the narration says `open_word` and set as it says
// `choice_word`.
static void
settings_combo_on(Director& director, const char* combo, const char* choice, const char* open_word,
                  const char* choice_word)
{
    ImGuiWindow* content = settings_content();
    if(content == nullptr)
    {
        return;
    }
    const ImGuiID id  = ImHashStr(combo, 0, content->ID);
    const ImRect  box = item_rect(director.Context(), id);
    click_item_on(director, id, open_word);
    director.Hold(0.4f);
    click_popup_item_on(director, choice, choice_word);
    if(has_area(box))
    {
        move_pointer(director, box.GetCenter(), true);
    }
}

// settings_button(), clicked as the narration says `word`.
static void
settings_button_on(Director& director, const char* label, const char* word)
{
    ImGuiWindow* content = settings_content();
    if(content == nullptr || content->ParentWindow == nullptr)
    {
        return;
    }
    const ImGuiTestItemInfo button = find_item(director.Context(), content->ParentWindow, label);
    if(button.ID != 0)
    {
        director.PointAt(button.RectFull.GetCenter(), word);
        director.ClickHere();
    }
}

// Toggles the View menu's `entry` as the narration says `word`, then rests the
// pointer at once, or leaves it by the menu for the next toggle.
static void
toggle_view_menu_on(Director& director, const char* entry, const char* word, bool rest = true)
{
    director.Context()->SetRef(MAIN_WINDOW);
    director.PointAtItem("##MenuBar/View");
    director.ClickHere();
    click_popup_item_on(director, entry, word);
    director.Hold(0.2f);
    if(rest)
    {
        rest_pointer(director, true);
    }
}

static void
chapter_customizing(ImGuiTestContext* ctx)
{
    TraceView* trace_view = prepare_system_trace(ctx);
    if(trace_view == nullptr)
    {
        return;
    }
    park_pointer(ctx, PARK_X, PARK_Y);

    Director director(ctx, ctx->Test->Name);
    if(!director.Start())
    {
        return;
    }
    director.Say("Now let's customize Optiq: its theme, time units, hotkeys, and layout.");
    director.Pause();

    // Edit > Preferences: the dark theme.
    director.Say("Open Edit, then Preferences. The settings are grouped into Display, Units, "
                 "Other, and Hotkeys.");
    open_menu_on(director, "Edit", "Edit");
    click_popup_item_on(director, "Preferences", "Preferences");
    director.Hold(0.4f);
    for(const char* category : { "Display", "Units", "Other", "Hotkeys" })
    {
        ImGuiWindow* categories = find_window("/SettingsCategories_");
        if(categories != nullptr)
        {
            director.PointAtItem(ImHashStr(category, 0, categories->ID), category);
        }
    }
    if(ImGuiWindow* categories = find_window("/SettingsCategories_"))
    {
        director.Hold(0.4f);
        director.HoverItem(ImHashStr("Display", 0, categories->ID), 0.3f);
    }
    director.Say("Under Display, you can switch to the dark theme.");
    settings_combo_on(director, "##theme", "Dark", "switch", "dark");
    director.Hold(0.6f);
    ImGuiWindow* content = settings_content();
    if(content != nullptr)
    {
        director.Say("You can also color-code tracks by node in multi-node traces, drop the "
                     "sidebar's row icons in favor of its right-click menu, and change the font "
                     "size.");
        const std::pair<const char*, const char*> options[] = {
            { "Multi-node decorators", "color" }, { "Compact topology sidebar", "icons" }
        };
        for(const auto& [option, word] : options)
        {
            const ImGuiTestItemInfo box = find_item(ctx, content, option);
            if(box.ID != 0)
            {
                director.PointAtItem(box.ID, word);
                director.Hold(1.0f);
            }
        }
        director.PointAtItem(ImHashStr("##font_size", 0, content->ID), "font");
        director.Hold(1.0f);
    }
    director.Say("Click OK to apply your changes. Here's Optiq in the dark theme.");
    settings_button_on(director, "OK", "OK");
    const std::vector<TrackRow> dark_rows  = track_rows(trace_view);
    const TrackRow*             dark_queue = find_row(dark_rows, is_kernel_queue);
    director.Hover(dark_queue != nullptr
                       ? queue_idle_point(*dark_queue)
                       : rect_point(window_rect(find_graph_window()), 0.5f, 0.4f),
                   READ_S);
    director.Pause();
    // The light theme comes back off camera: the same steps again add nothing.
    director.Say("We'll switch back to the light theme for the rest of this series.");
    director.Hold(1.0f);
    director.Cut([&]() {
        open_preferences(director);
        settings_combo(director, "##theme", "Light");
        settings_button(director, "OK");
        rest_pointer(director, true);
        ctx->Sleep(0.7f);
    });
    director.Pause();

    // Units: the time format used everywhere.
    director.Say("Back in Preferences, the Units page sets the time format used throughout "
                 "Optiq.");
    open_menu_on(director, "Edit", "Back");
    click_popup_item_on(director, "Preferences", "Preferences");
    director.Hold(0.4f);
    if(ImGuiWindow* categories = find_window("/SettingsCategories_"))
    {
        click_item_on(director, ImHashStr("Units", 0, categories->ID), "Units");
        director.Hold(0.4f);
    }
    director.Say("Let's pick milliseconds, and click OK.");
    settings_combo_on(director, "##time_format", "Milliseconds", "pick", "milliseconds");
    settings_button_on(director, "OK", "OK");
    rest_pointer(director, true);
    director.Say("Now times across Optiq are shown in milliseconds.");
    director.Hover(rect_point(overview_rect(), 0.5f, 0.3f), READ_S);
    director.Pause();
    director.Say("We'll set it back to timecode, the default.");
    director.Hold(0.8f);
    director.Cut([&]() {
        open_preferences(director);
        settings_category(director, "Units");
        settings_combo(director, "##time_format", "Timecode");
        settings_button(director, "OK");
        rest_pointer(director, true);
        ctx->Sleep(0.7f);
    });
    director.Pause();

    // Hotkeys can be rebound.
    director.Say("Preferences also has a Hotkeys page, which lets you rebind keyboard shortcuts "
                 "to the keys you prefer.");
    open_menu(director, "Edit");
    click_popup_item_on(director, "Preferences", "Preferences");
    director.Hold(0.4f);
    if(ImGuiWindow* categories = find_window("/SettingsCategories_"))
    {
        click_item_on(director, ImHashStr("Hotkeys", 0, categories->ID), "Hotkeys");
        director.Hold(0.4f);
    }
    // The list scrolls from its scrollbar, where the pointer hides no binding.
    content                   = settings_content();
    const ImGuiTable* hotkeys = find_table(content, "hotkeys_table");
    if(hotkeys != nullptr)
    {
        const ImVec2 scrollbar =
            ImGui::GetWindowScrollbarRect(hotkeys->InnerWindow, ImGuiAxis_Y).GetCenter();
        director.PointAt(scrollbar);
        director.Say("Scroll through the list to find a shortcut, and click Cancel to close "
                     "without making changes.");
        director.WaitUntilSaid("Scroll");
        director.Caption("Mouse wheel", 2.0f);
        director.Wheel(scrollbar, -2.0f, 2, 0.5f);
        director.Hold(0.4f);
    }
    else
    {
        ctx->LogError("The hotkeys table did not show");
        director.Say("Click Cancel to close without making changes.");
    }
    settings_button_on(director, "Cancel", "Cancel");
    rest_pointer(director, true);
    director.Pause();

    // The View menu shows and hides panels, each as the line names it.
    director.Say("The View menu shows and hides panels. Hide the Advanced Details panel, the "
                 "System Topology panel, and the timeline overview, and the timeline gets the "
                 "whole window.");
    toggle_view_menu_on(director, "Show Advanced Details Panel", "Advanced", false);
    toggle_view_menu_on(director, "Show System Topology Panel", "System", false);
    toggle_view_menu_on(director, "Show Timeline Overview", "overview");
    director.Pause();
    director.Say("Turn them back on from the same menu, and the familiar layout returns.");
    toggle_view_menu_on(director, "Show Timeline Overview", "Turn", false);
    toggle_view_menu_on(director, "Show System Topology Panel", "menu", false);
    toggle_view_menu_on(director, "Show Advanced Details Panel", "layout");
    director.Pause();
    director.Say("That's how to make Optiq your own.");
    director.Finish();
    show_all_panels();
}

// ---------------------------------------------------------------------------
// Chapters 12-14: ROCm Compute Profiler analysis data

// The compute view's page, between its tab strip and the status bar.
static ImRect
compute_page_rect(ImGuiTestContext* ctx)
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ctx->SetRef(MAIN_WINDOW);
    const ImRect tab    = item_rect(ctx, "**/Summary View");
    const ImRect status = status_bar_rect();
    const float  top    = has_area(tab) ? tab.Max.y : display.y * 0.15f;
    const float  bottom = has_area(status) ? status.Min.y : display.y;
    return ImRect(ImVec2(0.0f, top + 4.0f), ImVec2(display.x, bottom - 4.0f));
}

// A newly shown compute page draws half laid out for its first frames: ready
// once data requests have stayed drained as long as a new page takes to settle.
static std::function<bool()>
page_laid_out()
{
    auto calm_frames = std::make_shared<int>(0);
    return [calm_frames]() {
        *calm_frames = data_settled() ? *calm_frames + 1 : 0;
        return *calm_frames >= LAYOUT_SETTLE_FRAMES;
    };
}

// Clicks the compute view's tab `label` as the narration says `word`. The
// footage cuts from the click until its page is laid out.
static void
click_compute_tab_on(Director& director, const char* label, const char* word)
{
    ImGuiTestContext* ctx = director.Context();
    ctx->SetRef(MAIN_WINDOW);
    const ImRect tab = item_rect(ctx, (std::string("**/") + label).c_str());
    if(!has_area(tab))
    {
        ctx->LogError("No %s tab on screen", label);
        return;
    }
    director.PointAt(tab.GetCenter(), word);
    director.ClickAndCutUntil(tab.GetCenter(), page_laid_out(), SETTLE_TIMEOUT_S);
}

// Opens `combo` as the narration says `word` and lingers on the list, then
// picks the entry at `index` as it says `pick_word` (or just closes the list
// when `index` is negative).
static void
browse_combo_on(Director& director, ImGuiTestRef combo, int index, const char* word,
                const char* pick_word = nullptr)
{
    ImGuiTestContext* ctx = director.Context();
    click_item_on(director, combo, word);
    director.Hold(0.8f);
    ImGuiTestItemList entries;
    ctx->GatherItems(&entries, "//$FOCUSED");
    if(index >= 0 && index < entries.GetSize())
    {
        director.PointAt(label_point(entries[index]->RectFull), pick_word);
        director.Hold(0.3f);
        ctx->MouseClick(ImGuiMouseButton_Left);
        settle(director);
    }
    else
    {
        close_popups(director);
    }
}

// Where the wheel scrolls the page `scroller` rather than zooming one of its
// plots: the strip of padding down its left edge, which no card covers.
static ImVec2
page_wheel_point(const ImGuiWindow* scroller, const ImRect& page)
{
    if(scroller == nullptr)
    {
        return rect_point(page, 0.99f, 0.5f);
    }
    const ImRect box = scroller->Rect();
    return ImVec2(box.Min.x + scroller->WindowPadding.x * 0.5f, box.GetCenter().y);
}

// Turns the wheel `notches` notches over `at` (negative scrolls down) under a
// caption, one notch at a time, so the page visibly scrolls rather than jumps.
static void
wheel_page(Director& director, const ImVec2& at, int notches)
{
    const int steps = std::abs(notches);
    if(steps == 0)
    {
        return;
    }
    director.MoveTo(at);
    director.Caption("Mouse wheel",
                     static_cast<float>(steps) * WHEEL_NOTCH_GAP_S + WHEEL_CAPTION_LINGER_S);
    director.Wheel(at, notches < 0 ? -1.0f : 1.0f, steps, WHEEL_NOTCH_GAP_S);
    settle(director);
}

// Wheels the page `scroller` from `at`, a burst of notches at a time, until the
// top of the window named `fragment` lands `target_y` of the way down `page`,
// or is on screen with the page scrolled to its end. Logs an error when that
// window is missing or never gets there.
static bool
scroll_into_view(Director& director, const ImGuiWindow* scroller, const ImVec2& at,
                 const char* fragment, const ImRect& page, float target_y = SCROLL_TARGET_Y)
{
    const float goal    = page.Min.y + page.GetHeight() * target_y;
    bool        in_view = false;
    for(int burst = 0; scroller != nullptr && burst <= SCROLL_BURST_LIMIT; burst++)
    {
        const ImRect target   = window_rect(find_window(fragment));
        const float  step     = wheel_step(scroller, ImGuiAxis_Y);
        const float  room     = scroller->ScrollMax.y - scroller->Scroll.y;
        const bool   at_end   = room < 1.0f;
        float        distance = 0.0f;
        if(has_area(target))
        {
            in_view = target.Min.y >= page.Min.y &&
                      (target.Min.y <= page.Min.y + page.GetHeight() * SCROLL_TARGET_BAND ||
                       (at_end && target.Min.y < page.Max.y));
            distance = ImClamp(target.Min.y - goal, -scroller->Scroll.y, room);
        }
        else
        {
            // ImGui hides child windows that are entirely out of sight, so a
            // target further down only turns up once the page nears it.
            distance = ImMin(page.GetHeight() * 0.5f, room);
        }
        const int notches = static_cast<int>(std::lround(distance / step));
        if(in_view || notches == 0 || burst == SCROLL_BURST_LIMIT)
        {
            break;
        }
        wheel_page(director, at, -notches);
    }
    if(!in_view)
    {
        director.Context()->LogError("Could not scroll '%s' into view", fragment);
    }
    return in_view;
}

// Wheels the page `scroller` from `at` until the bottom of the window named
// `fragment` is on `page`, whatever that leaves above it.
static void
scroll_bottom_into_view(Director& director, const ImGuiWindow* scroller, const ImVec2& at,
                        const char* fragment, const ImRect& page)
{
    const ImRect target = window_rect(find_window(fragment));
    if(scroller == nullptr || !has_area(target))
    {
        director.Context()->LogError("Could not scroll '%s' into view", fragment);
        return;
    }
    const float overflow = ImMin(target.Max.y - page.Max.y, scroller->ScrollMax.y - scroller->Scroll.y);
    wheel_page(director, at, -static_cast<int>(ImCeil(overflow / wheel_step(scroller, ImGuiAxis_Y))));
}

// Hovers the cells of the first `count` rows on `page` in the column headed
// `column` of the metric table in `panel`.
static void
hover_column_cells(Director& director, ImGuiWindow* panel, const ImRect& page, const char* column,
                   int count)
{
    ImGuiTestContext*                    ctx   = director.Context();
    const std::vector<ImGuiTestItemInfo> items = gather_items(ctx, panel, true);
    ImGuiTestItemInfo                    header;
    for(const ImGuiTestItemInfo& item : items)
    {
        if(header.ID == 0 && std::strcmp(item.DebugLabel, column) == 0)
        {
            header = item;
        }
    }
    if(header.ID == 0)
    {
        ctx->LogError("No '%s' column in the metric table", column);
        return;
    }
    for(const ImGuiTestItemInfo& item : items)
    {
        if(count > 0 && item.RectFull.Min.y > header.RectFull.Max.y &&
           item.RectFull.Min.x >= header.RectFull.Min.x - ROW_TOLERANCE &&
           item.RectFull.Min.x < header.RectFull.Max.x && page.Contains(item.RectFull.GetCenter()))
        {
            director.Hover(label_point(item.RectFull), TOOLTIP_READ_S);
            count--;
        }
    }
}

// Hovers `count` evenly spaced points around a pie chart centred in `area`.
static void
hover_pie(Director& director, const ImRect& area, int count)
{
    constexpr float TWO_PI = 6.2831853f;
    const ImVec2    center = area.GetCenter();
    const float     radius = ImMin(area.GetWidth(), area.GetHeight()) * 0.25f;
    for(int i = 0; i < count; i++)
    {
        const float angle = TWO_PI * (static_cast<float>(i) + 0.3f) / static_cast<float>(count);
        director.Hover(ImVec2(center.x + std::cos(angle) * radius,
                              center.y + std::sin(angle) * radius),
                       1.4f);
    }
}

static void
chapter_compute_summary(ImGuiTestContext* ctx)
{
    if(prepare_system_trace(ctx) == nullptr)
    {
        return;
    }
    close_project(ctx, COMPUTE_TRACE_FILE);
    restore_sample(ctx, COMPUTE_TRACE_FILE);
    park_pointer(ctx, PARK_X, PARK_Y);

    Director director(ctx, ctx->Test->Name);
    if(!director.Start())
    {
        return;
    }
    director.Say("Now let's move on to ROCm Compute Profiler data, and explore its Summary "
                 "View.");
    director.Pause();

    // Compute Profiler analysis databases open the same way as traces. The
    // dialog remembers the last file type used, so every supported type is
    // picked explicitly.
    director.Say("Compute Profiler databases open just like traces. Choose File, then Open.");
    open_menu_on(director, "File", "File");
    point_at_popup_item(director, "Open", "Open");
    click_to_open_file_dialog(director);
    director.Hold(0.4f);
    const ImGuiTestItemInfo file_types =
        find_hashed_item(ctx, find_file_dialog_window(), "##Filters");
    director.Say("In the file browser, set the file type to All Supported, then select the "
                 "database, and click OK.");
    if(file_types.ID != 0)
    {
        click_item_on(director, file_types.ID, "type");
        director.Hold(0.3f);
        if(!click_popup_item_on(director, "All Supported", "Supported"))
        {
            close_popups(director);
        }
    }
    const std::string       file_label = std::string("[File] ") + COMPUTE_TRACE_FILE;
    const ImGuiTestItemInfo file_row =
        find_item(ctx, find_file_dialog_window(), file_label.c_str());
    if(file_row.ID == 0)
    {
        ctx->LogError("File dialog does not list %s", COMPUTE_TRACE_FILE);
        return;
    }
    const ImVec2 file_name_pos(
        file_row.RectFull.Min.x + ImGui::CalcTextSize(file_label.c_str()).x * 0.6f,
        file_row.RectFull.GetCenter().y);
    director.PointAt(file_name_pos, "select");
    director.ClickHere();
    const ImGuiTestItemInfo ok = find_item(ctx, find_file_dialog_window(), "OK");
    if(ok.ID == 0)
    {
        ctx->LogError("No OK button in the file dialog");
        return;
    }
    // The new tab draws half laid out for a few frames after it's ready, so the
    // footage cuts from the click until it has stayed ready for a while.
    int        ready_frames = 0;
    const auto laid_out     = [&ready_frames]() {
        ready_frames = compute_view_ready(current_compute_view()) ? ready_frames + 1 : 0;
        return ready_frames >= LAYOUT_SETTLE_FRAMES;
    };
    director.PointAt(ok.RectFull.GetCenter(), "OK");
    if(!director.ClickAndCutUntil(ok.RectFull.GetCenter(), laid_out, LOAD_TIMEOUT_S))
    {
        ctx->LogError("Compute database %s did not open", COMPUTE_TRACE_FILE);
        return;
    }
    director.Say("Optiq opens the analysis in a new tab.");
    // The roofline under the OK button hides the pointer, which leaves at once.
    rest_pointer(director, true);
    settle(director, 1.0f);
    director.Pause();

    // Each analysis view has its own tab; workload and kernel pickers sit above.
    ctx->SetRef(MAIN_WINDOW);
    director.Say("The analysis has five views: Summary, Kernel Details, Table View, Workload "
                 "Details, and Baseline Comparison.");
    for(const std::string tab : { "Summary View", "Kernel Details", "Table View",
                                  "Workload Details", "Baseline Comparison" })
    {
        if(ctx->ItemExists(("**/" + tab).c_str()))
        {
            director.PointAtItem(("**/" + tab).c_str(), tab.substr(0, tab.find(' ')).c_str());
        }
    }
    director.Hold(TOOLBAR_HOVER_S);
    ImGuiWindow* toolbar = find_toolbar_window();
    director.Say("The pickers in the toolbar choose the workload, and the kernel you're "
                 "analyzing.");
    const std::pair<const char*, const char*> pickers[] = { { "##Workloads", "workload" },
                                                            { "##Kernels", "kernel" } };
    for(const auto& [picker, word] : pickers)
    {
        const ImGuiTestItemInfo combo = find_hashed_item(ctx, toolbar, picker);
        if(combo.ID != 0)
        {
            browse_combo_on(director, combo.ID, -1, word);
        }
    }
    director.Pause();

    // Top kernels by execution time: table, pie and bar charts.
    ImGuiWindow* card = find_window("/top_kernels_card_");
    if(card != nullptr)
    {
        director.Say("The Summary View starts with the top kernels by execution time, shown as "
                     "a table and a pie chart.");
        director.PointAt(rect_point(card->Rect(), 0.3f, 0.35f), "table");
        director.Hold(0.8f);
        ImGuiWindow* chart = find_window("/chart_area_");
        if(chart != nullptr)
        {
            director.Say("Hover over a slice of the pie to see that kernel's share of the total "
                         "time.");
            hover_pie(director, chart->Rect(), 2);
        }
        const ImGuiTestItemInfo bar = find_hashed_item(ctx, card, ICON_CHART_BAR);
        if(bar.ID != 0)
        {
            director.Say("The bar chart compares kernels by any statistic you choose, such as "
                         "how many times each one was invoked.");
            click_item_on(director, bar.ID, "bar");
            director.Hold(0.5f);
            const ImGuiTestItemInfo plot = find_hashed_item(ctx, card, "##plot_combo");
            if(plot.ID != 0)
            {
                browse_combo_on(director, plot.ID, 0, "statistic", "invoked");
                director.Hold(1.0f);
                director.Say("Let's switch back to total duration, and return to the pie "
                             "chart.");
                browse_combo_on(director, plot.ID, 1, "switch", "total");
            }
            const ImGuiTestItemInfo pie = find_hashed_item(ctx, card, ICON_CHART_PIE);
            if(pie.ID != 0)
            {
                click_item_on(director, pie.ID, "pie");
            }
            director.Beat();
        }
        director.Pause();
    }

    // The roofline places every kernel against the hardware ceilings; its filters
    // and legend sit above the plot.
    ImGuiWindow* roofline = find_window("/roofline_container_");
    if(roofline != nullptr)
    {
        director.Say("Next is the roofline chart. The menus above it filter the chart to a "
                     "compute peak, or to a single kernel.");
        const std::pair<const char*, const char*> filters[] = { { "##compute_peak", "peak" },
                                                                { "##kernel", "kernel" } };
        for(const auto& [filter, word] : filters)
        {
            const ImGuiTestItemInfo combo = find_hashed_item(ctx, roofline, filter);
            if(combo.ID != 0)
            {
                browse_combo_on(director, combo.ID, -1, word);
            }
        }
        const ImGuiTestItemInfo legend = find_hashed_item(ctx, roofline, "toggle_menus");
        if(legend.ID != 0)
        {
            director.Say("The arrow button collapses the legend, to give the plot more room.");
            director.PointAt(legend.RectFull.GetCenter(), "arrow");
            director.ClickHere();
            director.Hold(1.0f);
        }
        else
        {
            ctx->LogError("No legend button on the roofline");
        }
    }
    const ImRect       page         = compute_page_rect(ctx);
    const ImGuiWindow* summary_page = find_window("/summary_");
    const ImVec2       scroll_at    = page_wheel_point(summary_page, page);
    director.Say("The roofline plots every kernel against the hardware's compute and memory "
                 "limits, showing whether it's compute bound or memory bound.");
    scroll_bottom_into_view(director, summary_page, scroll_at, "/roofline_container_", page);
    roofline = find_window("/roofline_container_");
    if(roofline != nullptr)
    {
        ImRect plot = roofline->Rect();
        plot.ClipWith(page);
        for(float fx : { 0.25f, 0.45f })
        {
            director.Hover(rect_point(plot, fx, 0.7f), 1.0f);
        }
    }
    // Off the plot, so its hover label is gone before the next topic.
    rest_pointer(director, true);
    director.Pause();

    // System Speed-of-Light: every metric against its peak. A metric's name
    // pops up its description, which the app wraps past the tooltip's edge, so
    // the pointer reads each metric's share of the peak instead.
    director.Say("Last, System Speed-of-Light compares key metrics with the hardware's peak, "
                 "so you can see which parts of the GPU the workload pushes hardest.");
    if(scroll_into_view(director, summary_page, scroll_at, SOL_PANEL, page))
    {
        hover_column_cells(director, find_window(SOL_PANEL), page, "Pct of Peak", 2);
    }
    director.Pause();
    director.Say("And that's the Summary View.");
    director.Finish();
}

static void
chapter_compute_kernel_details(ImGuiTestContext* ctx)
{
    ComputeView* compute_view = prepare_compute_trace(ctx);
    if(compute_view == nullptr)
    {
        return;
    }
    park_pointer(ctx, PARK_X, PARK_STATUS_Y);

    Director director(ctx, ctx->Test->Name);
    if(!director.Start())
    {
        return;
    }
    director.Say("Next, we'll analyze individual kernels in the Kernel Details view.");
    director.Pause();
    director.Say("Open the Kernel Details tab.");
    click_compute_tab_on(director, "Kernel Details", "Kernel");
    director.Beat();

    // The kernel selection table drives every chart on the page.
    ImGuiWindow* table = find_window(KERNEL_TABLE);
    director.WaitFor([&table]() {
        table = find_window(KERNEL_TABLE);
        return table != nullptr;
    }, 2.0f, 30.0f);
    if(table != nullptr)
    {
        std::vector<ImGuiTestItemInfo> cells = gather_items(ctx, table, true);
        director.Say("The kernel selection table lists every kernel, along with its key "
                     "metrics.");
        director.Hover(rect_point(table->Rect(), 0.3f, 0.2f), READ_S);
        // A row is one selectable across every column, but its name cell pops
        // the kernel's full name up over the page, so the row is picked from
        // its duration cell instead.
        ImGuiTestItemInfo duration;
        for(const ImGuiTestItemInfo& cell : cells)
        {
            if(duration.ID == 0 && label_matches(cell.DebugLabel, "Duration"))
            {
                duration = cell;
            }
        }
        int picked = 0;
        for(const ImGuiTestItemInfo& cell : cells)
        {
            if(cell.DebugLabel[0] != '\0' && cell.RectFull.Min.y > table->Rect().Min.y + 60.0f &&
               picked < 2)
            {
                if(picked == 1)
                {
                    const float  x = duration.ID != 0 ? duration.RectFull.GetCenter().x
                                                      : rect_point(cell.RectFull, 0.45f, 0.5f).x;
                    const ImVec2 row_point(x, cell.RectFull.GetCenter().y);
                    director.Say("Select a kernel, and everything on this page updates to match "
                                 "it.");
                    director.PointAt(row_point, "Select");
                    director.ClickHere();
                    settle(director);
                    rest_pointer(director);
                    director.Pause();
                }
                picked++;
            }
        }
    }

    ctx->SetRef(MAIN_WINDOW);
    if(ctx->ItemExists("**/Show Bar Charts"))
    {
        director.Say("Show Bar Charts draws each value as a bar, so outliers stand out.");
        click_item_on(director, "**/Show Bar Charts", "Show");
        director.Hold(1.5f);
        director.Say("Click it again to hide the bars.");
        ctx->SetRef(MAIN_WINDOW);
        click_item_on(director, "**/Hide Bar Charts", "Click");
        director.Beat();
    }

    // Filter kernels by name or by metric value.
    table = find_window(KERNEL_TABLE);
    const ImGuiTestItemInfo name_filter = find_item(ctx, table, "##filter");
    if(name_filter.ID != 0)
    {
        director.Say("The box under each column header filters the kernels. For names, use "
                     "LIKE with percent signs as wildcards, then click Apply Filters.");
        click_item_on(director, name_filter.ID, "names");
        director.Type("LIKE %rocprim%");
        ctx->SetRef(MAIN_WINDOW);
        click_item_on(director, "**/Apply Filters", "Apply");
        settle(director);
        director.Pause();
        director.Say("Clear All Filters brings every kernel back.");
        ctx->SetRef(MAIN_WINDOW);
        click_item_on(director, "**/Clear All Filters", "Clear");
        settle(director);
        director.Beat();
    }

    // Add Metric picks any metric as a new column.
    ctx->SetRef(MAIN_WINDOW);
    if(ctx->ItemExists("**/Add Metric"))
    {
        director.Say("Add Metric adds any metric as a new column in the table.");
        click_item_on(director, "**/Add Metric", "Add");
        director.Hold(0.8f);
        director.Say("Browse through the categories to find the metric you want, then click Add, "
                     "and it appears as a new column.");
        for(int level = 0; level < 4; level++)
        {
            ImGuiWindow* list = find_window("/##list_");
            const std::vector<ImGuiTestItemInfo> options = gather_items(ctx, list);
            if(options.empty())
            {
                break;
            }
            const ImGuiTestItemInfo& option = options[ImMin<size_t>(1, options.size() - 1)];
            director.PointAt(label_point(option.RectFull));
            director.Hold(0.3f);
            ctx->MouseClick(ImGuiMouseButton_Left);
            director.Hold(0.25f);
        }
        const ImGuiTestItemInfo add = find_popup_item(ctx, "Add");
        if(add.ID != 0)
        {
            director.PointAt(add.RectFull.GetCenter(), "Add");
            ctx->MouseClick(ImGuiMouseButton_Left);
            settle(director);
        }
        else
        {
            close_popups(director);
        }
        director.Pause();
    }

    // Below the table: memory chart, speed-of-light and the kernel's roofline.
    const ImRect       page_rect   = compute_page_rect(ctx);
    const ImGuiWindow* page_window = find_window("/kernel_details_");
    const ImVec2       scroll_at   = page_wheel_point(page_window, page_rect);
    director.Say("Below the table, the memory chart shows the kernel's traffic through each "
                 "level of the memory hierarchy.");
    if(scroll_into_view(director, page_window, scroll_at, "/memory_chart_card_", page_rect))
    {
        ImRect chart = window_rect(find_window("/memory_chart_card_"));
        chart.ClipWith(page_rect);
        director.Hover(rect_point(chart, 0.3f, 0.4f), 1.2f);
        director.Hover(rect_point(chart, 0.6f, 0.4f), 1.2f);
    }
    director.Say("Further down are System Speed-of-Light, and the kernel's own roofline chart.");
    if(scroll_into_view(director, page_window, scroll_at, "/roofline_card_", page_rect))
    {
        ImRect plot = window_rect(find_window("/roofline_card_"));
        plot.ClipWith(page_rect);
        director.Hover(rect_point(plot, 0.4f, 0.5f), 1.2f);
    }
    rest_pointer(director, true);
    director.Pause();
    director.Say("And that's Kernel Details.");
    director.Finish();
}

static void
chapter_compute_tables_and_comparison(ImGuiTestContext* ctx)
{
    ComputeView* compute_view = prepare_compute_trace(ctx);
    if(compute_view == nullptr)
    {
        return;
    }
    park_pointer(ctx, PARK_X, PARK_STATUS_Y);

    Director director(ctx, ctx->Test->Name);
    if(!director.Start())
    {
        return;
    }
    director.Say("Finally, let's explore the Table View, Workload Details, and Baseline "
                 "Comparison.");
    director.Pause();

    // Table View: every metric of the kernel, grouped by hardware block.
    director.Say("The Table View lists every metric collected for the selected kernel.");
    click_compute_tab_on(director, "Table View", "Table");
    director.Beat();
    const ImRect page_rect = compute_page_rect(ctx);
    director.Say("Its metrics are grouped into tabs by hardware block, from System "
                 "Speed-of-Light and the roofline, to wavefronts and the L2 cache.");
    const std::pair<const char*, const char*> categories[] = {
        { "System Speed-of-Light", "System" }, { "Roofline", "roofline" },
        { "Wavefront", "wavefronts" }, { "L2 Cache", "cache" }
    };
    for(const auto& [category, word] : categories)
    {
        ctx->SetRef(MAIN_WINDOW);
        const ImGuiTestItemInfo tab =
            ctx->ItemInfo((std::string("**/") + category).c_str(), ImGuiTestOpFlags_NoError);
        if(tab.ID != 0)
        {
            click_item_on(director, tab.ID, word);
            settle(director);
        }
    }
    director.Hold(1.0f);

    // Pinned metrics collect at the top and can be saved as a preset.
    ImGuiWindow* metrics = nullptr;
    for(ImGuiWindow* window : ImGui::GetCurrentContext()->Windows)
    {
        if(window_name_contains(window, "_table") && window_name_contains(window, "TabContainer"))
        {
            metrics = window;
        }
    }
    int pinned = 0;
    director.Say("Pin the metrics you care about with the checkbox beside each one.");
    for(const ImGuiTestItemInfo& item : gather_items(ctx, metrics))
    {
        if(pinned < 2 && item.DebugLabel[0] == '\0' && item.RectFull.GetWidth() > 0.0f &&
           item.RectFull.GetWidth() < 40.0f)
        {
            click_item_on(director, item.ID, pinned == 0 ? "Pin" : "checkbox");
            director.Hold(0.4f);
            pinned++;
        }
    }
    if(pinned > 0)
    {
        director.Pause();
    }
    if(metrics != nullptr)
    {
        const ImVec2 metric_name = rect_point(metrics->Rect(), 0.25f, 0.35f);
        director.Say("Right-click a metric to copy it, or to send it to Kernel Details, where "
                     "it becomes a new column in the kernel selection table.");
        director.PointAt(metric_name, "Right");
        director.ClickHere(ImGuiMouseButton_Right);
        director.Hold(0.3f);
        point_at_popup_item(director, "Copy", "copy");
        if(!click_popup_item_on(director, "Send metric to kernel details", "send"))
        {
            close_popups(director);
            ctx->LogError("Send metric to kernel details was unavailable");
        }
        click_compute_tab_on(director, "Kernel Details", "Kernel");

        // The sent metric becomes the table's last column, scrolled to when it
        // lies past the right edge.
        ImGuiWindow* kernels = nullptr;
        director.WaitFor(
            [&kernels]() {
                kernels = find_window(KERNEL_TABLE);
                return kernels != nullptr;
            },
            0.5f, ITEM_TIMEOUT_S);
        ImGuiWindow*      host  = kernels != nullptr ? kernels->ParentWindow : nullptr;
        const ImGuiTable* table = find_table(host, KERNEL_TABLE);
        if(table == nullptr)
        {
            ctx->LogError("The kernel selection table did not show");
        }
        else
        {
            const int sent = table->ColumnsCount - 1;
            reveal_table_column(director, host, KERNEL_TABLE, sent);
            table = find_table(host, KERNEL_TABLE);
            if(table != nullptr)
            {
                // Over the new column's first value rather than its header,
                // whose description the app wraps past the window's edge. The
                // first labeled item below the header and filter rows is the
                // first kernel's row.
                const ImGuiTableColumn& column = table->Columns[sent];
                float row_y = table->InnerClipRect.Min.y + ImGui::GetFrameHeight() * 2.5f;
                for(const ImGuiTestItemInfo& item : gather_items(ctx, table->InnerWindow))
                {
                    if(item.DebugLabel[0] != '\0' &&
                       item.RectFull.Min.y > table->InnerClipRect.Min.y + 60.0f)
                    {
                        row_y = item.RectFull.GetCenter().y;
                        break;
                    }
                }
                director.PointAt(ImVec2((column.MinX + column.MaxX) * 0.5f, row_y), "column");
                director.Hold(1.5f);
            }
        }
        director.Say("Back in the Table View, you can save your pinned metrics as a preset for "
                     "next time.");
        click_compute_tab_on(director, "Table View", "Back");
    }
    else
    {
        director.Say("You can save your pinned metrics as a preset for next time.");
    }

    const ImGuiTestItemInfo presets =
        find_hashed_item(ctx, find_toolbar_window(), View::ICON_CHEVRON_DOWN);
    if(presets.ID != 0)
    {
        click_item_on(director, presets.ID, "preset");
        director.Hold(0.6f);
        const ImGuiTestItemInfo name = find_popup_item(ctx, "##input_text_with_clear");
        if(name.ID != 0)
        {
            director.Say("Enter a name, and click the plus button to save it.");
            // The text cursor would sit in the middle of the name as it's typed.
            click_item_on(director, name.ID, "name");
            move_pointer(director, rect_point(newest_popup_rect(), PRESET_REST_X, PRESET_REST_Y),
                         true);
            director.Type("Memory metrics");
            const ImGuiTestItemInfo save = find_popup_item(ctx, ICON_ADD_NOTE);
            if(save.ID != 0)
            {
                click_item_on(director, save.ID, "plus");
                move_pointer(director, name.RectFull.GetCenter(), true);
                director.Hold(1.2f);
            }
            else
            {
                ctx->LogError("No save button in the Presets popup");
            }
        }
        else
        {
            ctx->LogError("The Presets popup did not open");
        }
        director.Pause();
    }

    // Workload Details: the system and the profiler configuration.
    director.Say("Workload Details shows the system the data was collected on, and how the "
                 "profiler was configured.");
    if(presets.ID != 0)
    {
        // A click away from the popup closes it, on the status bar, where the
        // pointer then raises no tooltip.
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        director.Click(ImVec2(display.x * PARK_X, display.y * PARK_STATUS_Y));
        director.Context()->PopupCloseAll();
    }
    click_compute_tab_on(director, "Workload Details", "Workload");
    director.PointAt(rect_point(page_rect, 0.25f, 0.35f), "system");
    director.Hold(1.0f);
    director.PointAt(rect_point(page_rect, 0.75f, 0.35f), "configured");
    director.Hold(1.2f);
    director.Pause();

    // Baseline Comparison: two kernels or workloads side by side.
    director.Say("Baseline Comparison puts two measurements side by side: two workloads, or "
                 "two kernels from the same workload.");
    click_compute_tab_on(director, "Baseline Comparison", "Baseline");
    director.Pause();
    ImGuiWindow* toolbar = nullptr;
    for(ImGuiWindow* window : ImGui::GetCurrentContext()->Windows)
    {
        if(window_name_contains(window, "TabContainer") && window_name_contains(window, "/toolbar_"))
        {
            toolbar = window;
        }
    }
    if(toolbar != nullptr)
    {
        director.Say("Choose the target workload and kernel to compare with the baseline.");
        ctx->SetRef(toolbar->ID);
        if(ctx->ItemExists("##TargetWorkloads"))
        {
            browse_combo_on(director, ImGuiTestRef(ctx->GetID("##TargetWorkloads")), 0,
                            "workload");
        }
        ctx->SetRef(toolbar->ID);
        if(ctx->ItemExists("##target_kernels"))
        {
            browse_combo_on(director, ImGuiTestRef(ctx->GetID("##target_kernels")), 1, "kernel");
        }
        settle(director, 1.0f);
        director.Say("The table shows each metric for both, with the difference and the "
                     "percentage change color coded.");
        director.Hold(1.5f);
        ctx->SetRef(toolbar->ID);
        if(ctx->ItemExists("##threshold_percentage"))
        {
            const ImRect threshold = item_rect(ctx, "##threshold_percentage");
            director.Say("The threshold sets how large a percentage difference must be before "
                         "it's highlighted. Drag it up, and the smaller differences lose their "
                         "color.");
            director.PointAtItem("##threshold_percentage", "threshold");
            director.Hold(0.8f);
            const ImVec2 from = threshold.GetCenter();
            const float  room =
                ImMax(ImGui::GetIO().DisplaySize.x - from.x - THRESHOLD_EDGE_GAP, 1.0f);
            const int   strokes = static_cast<int>(ImCeil(THRESHOLD_DRAG_X / room));
            const float reach   = THRESHOLD_DRAG_X / static_cast<float>(strokes);
            director.WaitUntilSaid("Drag");
            for(int stroke = 0; stroke < strokes; stroke++)
            {
                director.PointAt(from);
                director.Drag(from, ImVec2(from.x + reach, from.y), 0.6f);
            }
            rest_pointer(director, true);
            director.Hold(1.2f);
        }
        // The button's label names what the table lists now, not what a click does.
        director.Say("The Show All Metrics button switches between all metrics and only the ones "
                     "both measurements share. These two ran on the same GPU, so nothing changes "
                     "here, but it matters across GPU architectures.");
        const std::pair<const char*, const char*> toggles[] = {
            { "Show All Metrics", "switches" }, { "Show Common Metrics", "share" }
        };
        for(const auto& [label, word] : toggles)
        {
            ctx->SetRef(toolbar->ID);
            if(ctx->ItemExists(label))
            {
                click_item_on(director, label, word);
                settle(director);
            }
        }
    }
    rest_pointer(director);
    director.Pause();
    director.Say("And that's Baseline Comparison.");
    director.Finish();
}

// ---------------------------------------------------------------------------

static void
register_chapter(ImGuiTestEngine* engine, const char* name, ImGuiTestTestFunc* func)
{
    ImGuiTest* test = IM_REGISTER_TEST(engine, "tutorial", name);
    test->TestFunc  = func;
}

void
RegisterChapters(ImGuiTestEngine* engine)
{
    snapshot_samples();
    register_chapter(engine, "01_opening_a_trace", chapter_open_trace);
    register_chapter(engine, "02_interface_tour", chapter_interface_tour);
    register_chapter(engine, "03_navigating_the_timeline", chapter_navigate_timeline);
    register_chapter(engine, "04_system_topology_panel", chapter_topology_panel);
    register_chapter(engine, "05_working_with_tracks", chapter_working_with_tracks);
    register_chapter(engine, "06_events_and_flows", chapter_events_and_flows);
    register_chapter(engine, "07_time_ranges_and_measuring", chapter_ranges_and_measure);
    register_chapter(engine, "08_advanced_details_tables", chapter_details_tables);
    register_chapter(engine, "09_search_minimap_and_summary", chapter_search_minimap_summary);
    register_chapter(engine, "10_annotations_bookmarks_and_projects",
                     chapter_annotations_bookmarks);
    register_chapter(engine, "11_customizing_optiq", chapter_customizing);
    register_chapter(engine, "12_compute_opening_and_summary", chapter_compute_summary);
    register_chapter(engine, "13_compute_kernel_details", chapter_compute_kernel_details);
    register_chapter(engine, "14_compute_tables_workload_and_comparison",
                     chapter_compute_tables_and_comparison);
}

}  // namespace Tutorial
}  // namespace RocProfVis
