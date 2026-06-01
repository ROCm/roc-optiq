// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ui_test_registry.h"

#include "imgui.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_events.h"
#include "rocprofvis_project.h"
#include "rocprofvis_root_view.h"
#include "rocprofvis_settings_manager.h"

#include <cstring>
#include <list>
#include <memory>
#include <string>

namespace RocProfVis
{
namespace View
{
namespace Test
{

namespace
{

// ---------------------------------------------------------------------------
// SideBar leaf-row geometry
//
// A leaf row lays out its controls left -> right on one line:
//   [eye icon] [scroll-to icon] [track-name button]
// Offsets are measured back from the row's left edge and kept conservative so
// a click still lands in the button hitbox at non-default font scaling.
// ---------------------------------------------------------------------------
constexpr float SIDEBAR_ICON_BUTTON_W = 24.0f;
constexpr float SIDEBAR_ICON_SPACING  = 2.0f;
constexpr float SIDEBAR_SCROLL_OFFSET =
    SIDEBAR_ICON_BUTTON_W * 0.5f + SIDEBAR_ICON_SPACING;
constexpr float SIDEBAR_EYE_OFFSET =
    SIDEBAR_ICON_BUTTON_W * 1.5f + SIDEBAR_ICON_SPACING * 2.0f;
constexpr float SIDEBAR_GROUP_EYE_OFFSET =
    SIDEBAR_ICON_BUTTON_W * 0.5f + SIDEBAR_ICON_SPACING;

// GPU counter leaves present in the bundled rocpd-transpose sample. Used by the
// SideBar / track scenes; missing rows are simply skipped (NoError).
const char* const SAMPLE_COUNTER_TRACKS[] = {
    "GFX Busy", "Pow", "Temp", "MemUsg", "MM Busy", "UMC Busy",
};

// ---------------------------------------------------------------------------
// AppUiDriver
//
// Thin wrapper over ImGuiTestContext that drives the *live* application UI.
// Every interaction is NoError / existence-guarded so a single missing widget
// (transient layout reflow, a non-developer build, a trace lacking a given
// track, ...) never flips the engine into IsError() and short-circuits the
// rest of a scene. These are crash smoke tests: touch everything, assert
// almost nothing beyond "the app is still standing".
//
// The scenes are registered as separate tests but chained like one user
// session: the first opens the trace, each subsequent scene reuses the already
// open project via EnsureTrace() (and re-opens it if run in isolation).
// ---------------------------------------------------------------------------
struct AppUiDriver
{
    explicit AppUiDriver(ImGuiTestContext* test_ctx)
    : ctx(test_ctx)
    , app(AppWindow::GetInstance())
    {
    }

    // Ensures a project is open, opening the most-recent trace if needed.
    // Returns false (and logs) when there is nothing to open or the load times
    // out, so a scene can skip cleanly instead of failing on environment state.
    bool EnsureTrace()
    {
        if(app->GetCurrentProject() != nullptr)
        {
            ctx->SetRef("//Main Window");
            return true;
        }
        const std::list<std::string>& recent_files =
            SettingsManager::GetInstance().GetInternalSettings().recent_files;
        if(recent_files.empty())
        {
            ctx->LogWarning("No recent files available; skipping scene.");
            return false;
        }
        const std::string trace = recent_files.front();
        ctx->LogInfo("Opening recent trace: %s", trace.c_str());
        app->OpenFile(trace);
        for(int i = 0; i < 900; ++i)
        {
            ctx->Yield();
            if(app->GetCurrentProject() != nullptr)
            {
                break;
            }
        }
        if(app->GetCurrentProject() == nullptr)
        {
            ctx->LogWarning("Recent trace did not open; skipping scene.");
            return false;
        }
        ctx->Yield(40);
        ctx->SetRef("//Main Window");
        return true;
    }

    void EnsureSidebarVisible()
    {
        if(!SettingsManager::GetInstance().GetAppWindowSettings().show_sidebar)
        {
            Menu("//Main Window/View/Show System Topology Panel");
            ctx->Yield(12);
        }
    }

    // Viewport-fractional point.
    ImVec2 Pt(float frac_x, float frac_y) const
    {
        const ImVec2 size = ImGui::GetIO().DisplaySize;
        return ImVec2(size.x * frac_x, size.y * frac_y);
    }

    void Click(const char* ref, int settle_frames = 10)
    {
        ctx->ItemClick(ref, 0, ImGuiTestOpFlags_NoError);
        ctx->Yield(settle_frames);
    }

    void ClickScreen(ImVec2 pos, int settle_frames = 6)
    {
        ctx->MouseTeleportToPos(pos);
        ctx->MouseClick(0);
        ctx->Yield(settle_frames);
    }

    void RightClickScreen(ImVec2 pos, int settle_frames = 10)
    {
        ctx->MouseTeleportToPos(pos);
        ctx->MouseClick(ImGuiMouseButton_Right);
        ctx->Yield(settle_frames);
    }

    void DragScreen(ImVec2 from, ImVec2 delta, int settle_frames = 6)
    {
        ctx->MouseTeleportToPos(from);
        ctx->MouseDragWithDelta(delta);
        ctx->Yield(settle_frames);
    }

    // Walks a "//Main Window/Menu/Sub/.../Item" path one segment at a time so
    // every click carries NoError (MenuClick has no NoError variant).
    void Menu(const char* path)
    {
        const char* start = path;
        if(strncmp(start, "//", 2) == 0)
        {
            start += 2;
        }
        const char* first_slash = strchr(start, '/');
        if(first_slash == nullptr)
        {
            return;
        }
        const char* second_slash = strchr(first_slash + 1, '/');

        std::string head = "//";
        if(second_slash != nullptr)
        {
            head.append(start, second_slash - start);
        }
        else
        {
            head.append(start);
        }
        ctx->ItemClick(head.c_str(), 0, ImGuiTestOpFlags_NoError);
        ctx->Yield(8);
        if(second_slash == nullptr)
        {
            return;
        }

        const char* p = second_slash + 1;
        while(*p != '\0')
        {
            const char* slash = strchr(p, '/');
            std::string sub("//$FOCUSED/");
            if(slash != nullptr)
            {
                sub.append(p, slash - p);
            }
            else
            {
                sub.append(p);
            }
            ctx->ItemClick(sub.c_str(), 0, ImGuiTestOpFlags_NoError);
            ctx->Yield(8);
            if(slash == nullptr)
            {
                break;
            }
            p = slash + 1;
        }
    }

    // ComboClick has no NoError variant, so gate on the combo existing.
    void Combo(const char* combo_ref, const char* item_label)
    {
        if(ctx->ItemExists(combo_ref))
        {
            std::string ref = std::string(combo_ref) + "/" + item_label;
            ctx->ComboClick(ref.c_str());
            ctx->Yield(18);
        }
    }

    // Finds a widget anywhere under Main Window by label (NoError).
    ImGuiTestItemInfo Row(const char* label)
    {
        std::string path = std::string("//Main Window/**/") + label;
        return ctx->ItemInfo(path.c_str(), ImGuiTestOpFlags_NoError);
    }

    DataProvider* Provider()
    {
        Project* project = app->GetCurrentProject();
        if(project == nullptr)
        {
            return nullptr;
        }
        std::shared_ptr<RootView> root_view =
            std::dynamic_pointer_cast<RootView>(project->GetView());
        return root_view ? root_view->GetDataProvider() : nullptr;
    }

    // Re-issues a focused SetViewRange via the EventManager - the same channel
    // TimelineView listens to for the W-key zoom, but independent of which
    // child window owns keyboard focus.
    void ZoomToFraction(double left_frac, double right_frac)
    {
        DataProvider* provider = Provider();
        if(provider == nullptr)
        {
            return;
        }
        const TimelineModel& timeline = provider->DataModel().GetTimeline();
        const double         start    = timeline.GetStartTime();
        const double         end      = timeline.GetEndTime();
        if(end <= start)
        {
            return;
        }
        const double span = end - start;
        EventManager::GetInstance()->AddEvent(std::make_shared<RangeEvent>(
            static_cast<int>(RocEvents::kSetViewRange), start + span * left_frac,
            start + span * right_frac, provider->GetTraceFilePath()));
        ctx->Yield(35);
    }

    // Clicks the per-leaf scroll-to icon for a sidebar track (NoError).
    bool ScrollToTrack(const char* track_name)
    {
        ImGuiTestItemInfo info = Row(track_name);
        if(info.ID == 0)
        {
            return false;
        }
        const float mid_y    = (info.RectFull.Min.y + info.RectFull.Max.y) * 0.5f;
        const float scroll_x = info.RectFull.Min.x - SIDEBAR_SCROLL_OFFSET;
        ClickScreen(ImVec2(scroll_x, mid_y), 25);
        return true;
    }

    // Clicks the per-leaf eye icon for a sidebar track (toggles visibility).
    bool ToggleTrackEye(const char* track_name)
    {
        ImGuiTestItemInfo info = Row(track_name);
        if(info.ID == 0)
        {
            return false;
        }
        const float mid_y = (info.RectFull.Min.y + info.RectFull.Max.y) * 0.5f;
        const float eye_x = info.RectFull.Min.x - SIDEBAR_EYE_OFFSET;
        ClickScreen(ImVec2(eye_x, mid_y), 10);
        return true;
    }

    // The counter group header label embeds its child count ("Counters (6)").
    // Probe a sensible range so the workflow stays trace-agnostic.
    ImGuiTestItemInfo FindCounterGroup()
    {
        for(int n = 1; n <= 32; ++n)
        {
            std::string       label = "Counters (" + std::to_string(n) + ")";
            ImGuiTestItemInfo info  = Row(label.c_str());
            if(info.ID != 0)
            {
                return info;
            }
        }
        return ImGuiTestItemInfo{};
    }

    // Sweeps clicks across the timeline graph to land on events in several
    // lanes (each click selects/deselects whatever event sits at that pixel).
    void SweepGraphClicks(int rows, int cols, float y_top_frac, float y_bot_frac)
    {
        const ImVec2 size      = ImGui::GetIO().DisplaySize;
        const float  graph_x0  = size.x * 0.42f;
        const float  graph_x1  = size.x * 0.92f;
        const float  graph_y0  = size.y * y_top_frac;
        const float  graph_y1  = size.y * y_bot_frac;
        for(int r = 0; r < rows; ++r)
        {
            const float yt = (rows > 1) ? static_cast<float>(r) /
                                              static_cast<float>(rows - 1)
                                        : 0.0f;
            const float py = graph_y0 + (graph_y1 - graph_y0) * yt;
            for(int c = 0; c < cols; ++c)
            {
                const float xt = (cols > 1) ? static_cast<float>(c) /
                                                  static_cast<float>(cols - 1)
                                            : 0.0f;
                const float px = graph_x0 + (graph_x1 - graph_x0) * xt;
                ctx->MouseTeleportToPos(ImVec2(px, py));
                ctx->MouseClick(0);
                ctx->Yield(2);
            }
        }
    }

    ImGuiTestContext* ctx;
    AppWindow*        app;
};

}  // namespace

int
RegisterRocOptiqUiTests(ImGuiTestEngine* engine, bool include_app_havoc)
{
    int test_count = 0;

    // These scenes drive the real application against an opened trace, so they
    // are only registered for the in-app runner (roc-optiq -u). The headless
    // path registers nothing.
    if(!include_app_havoc)
    {
        return test_count;
    }

    // -----------------------------------------------------------------------
    // Scene 01 - Open a trace and deep-dive the SideBar topology tree.
    // -----------------------------------------------------------------------
    {
        ImGuiTest* test =
            IM_REGISTER_TEST(engine, "roc-optiq/ui", "01_open_trace_and_sidebar");
        test->TestFunc = [](ImGuiTestContext* ctx) {
            AppUiDriver d(ctx);
            if(!d.EnsureTrace())
            {
                return;
            }
            d.EnsureSidebarVisible();

            // Expand / collapse / re-expand the whole topology tree.
            if(ctx->ItemExists("//Main Window/**/Project"))
            {
                ctx->ItemOpenAll("//Main Window/**/Project");
                ctx->Yield(12);
                ctx->ItemCloseAll("//Main Window/**/Project");
                ctx->Yield(10);
                ctx->ItemOpenAll("//Main Window/**/Project");
                ctx->Yield(12);
            }

            // Per-leaf scroll-to navigation for a few tracks.
            for(const char* track : SAMPLE_COUNTER_TRACKS)
            {
                d.ScrollToTrack(track);
            }

            // Per-leaf eye toggles: hide each counter one at a time.
            for(const char* track : SAMPLE_COUNTER_TRACKS)
            {
                d.ToggleTrackEye(track);
            }

            // Parent-group eye: a single click restores every hidden leaf via
            // SideBar::ApplyVisibility cascading through the subtree.
            ImGuiTestItemInfo group = d.FindCounterGroup();
            if(group.ID != 0)
            {
                const float mid_y =
                    (group.RectFull.Min.y + group.RectFull.Max.y) * 0.5f;
                const float eye_x = group.RectFull.Min.x - SIDEBAR_GROUP_EYE_OFFSET;
                d.ClickScreen(ImVec2(eye_x, mid_y), 25);
                // Toggle the group once more (all-visible -> all-hidden -> back).
                d.ClickScreen(ImVec2(eye_x, mid_y), 15);
                d.ClickScreen(ImVec2(eye_x, mid_y), 15);
            }

            // Drag the sidebar (horizontal) splitter, derived from the framed
            // Project node's right edge.
            ImGuiTestItemInfo project = d.Row("Project");
            if(project.ID != 0)
            {
                const ImVec2 size = ImGui::GetIO().DisplaySize;
                const ImVec2 splitter(project.RectFull.Max.x + 3.0f,
                                      project.RectFull.Max.y + size.y * 0.20f);
                const float drags[] = { -150.0f, 220.0f, -70.0f };
                for(float dx : drags)
                {
                    d.DragScreen(splitter, ImVec2(dx, 0.0f));
                }
            }
        };
        ++test_count;
    }

    // -----------------------------------------------------------------------
    // Scene 02 - TimelineView navigation (hotkeys, wheel, drag, programmatic
    // range) plus the histogram and its normalize context menu.
    // -----------------------------------------------------------------------
    {
        ImGuiTest* test =
            IM_REGISTER_TEST(engine, "roc-optiq/ui", "02_timeline_navigation");
        test->TestFunc = [](ImGuiTestContext* ctx) {
            AppUiDriver d(ctx);
            if(!d.EnsureTrace())
            {
                return;
            }

            d.Click("//Main Window/Toolbar/Reset View", 15);
            ctx->WindowFocus("//Main Window");
            ctx->Yield(4);

            // Keyboard: zoom (W/S), pan (A/D), scroll tracks (Up/Down), mark (M),
            // and a Shift-held speed-boost pan.
            ctx->KeyPress(ImGuiKey_S, 8);
            ctx->KeyPress(ImGuiKey_W, 14);
            ctx->KeyPress(ImGuiKey_D, 6);
            ctx->KeyPress(ImGuiKey_A, 6);
            ctx->KeyDown(ImGuiMod_Shift);
            ctx->KeyPress(ImGuiKey_D, 6);
            ctx->KeyPress(ImGuiKey_A, 6);
            ctx->KeyUp(ImGuiMod_Shift);
            ctx->KeyPress(ImGuiKey_DownArrow, 6);
            ctx->KeyPress(ImGuiKey_UpArrow, 6);
            ctx->KeyPress(ImGuiKey_M);
            ctx->Yield(4);
            ctx->KeyPress(ImGuiKey_M);
            ctx->Yield(4);

            // Mouse-wheel zoom (in then out) anchored on the graph, then drag-pan.
            const ImVec2 graph = d.Pt(0.62f, 0.40f);
            ctx->MouseTeleportToPos(graph);
            ctx->MouseWheelY(10.0f);
            ctx->Yield(6);
            ctx->MouseWheelY(-4.0f);
            ctx->Yield(6);
            ctx->MouseDragWithDelta(ImVec2(220.0f, 0.0f));
            ctx->Yield(6);
            ctx->MouseDragWithDelta(ImVec2(-220.0f, 0.0f));
            ctx->Yield(6);

            // Vertical wheel over the description column scrolls the track list.
            ctx->MouseTeleportToPos(d.Pt(0.18f, 0.42f));
            ctx->MouseWheelY(-5.0f);
            ctx->Yield(6);
            ctx->MouseWheelY(5.0f);
            ctx->Yield(6);

            // Programmatic view ranges (wide -> narrow -> wide).
            d.ZoomToFraction(0.30, 0.45);
            d.ZoomToFraction(0.38, 0.40);
            d.ZoomToFraction(0.0, 1.0);

            // Histogram: make sure it is visible, then right-click it for the
            // normalize context menu.
            if(!SettingsManager::GetInstance().GetAppWindowSettings().show_histogram)
            {
                d.Menu("//Main Window/View/Show Histogram");
                ctx->Yield(12);
            }
            const ImVec2 size = ImGui::GetIO().DisplaySize;
            d.RightClickScreen(ImVec2(size.x * 0.6f, size.y * 0.52f), 10);
            ctx->ItemClick("//$FOCUSED/Normalize: All Tracks", 0,
                           ImGuiTestOpFlags_NoError);
            ctx->Yield(8);
            d.RightClickScreen(ImVec2(size.x * 0.6f, size.y * 0.52f), 10);
            ctx->ItemClick("//$FOCUSED/Normalize: Visible Tracks", 0,
                           ImGuiTestOpFlags_NoError);
            ctx->Yield(8);
            ctx->KeyPress(ImGuiKey_Escape);

            d.Click("//Main Window/Toolbar/Reset View", 12);
        };
        ++test_count;
    }

    // -----------------------------------------------------------------------
    // Scene 03 - Minimap navigator (compass popup + drag-to-pan).
    // -----------------------------------------------------------------------
    {
        ImGuiTest* test = IM_REGISTER_TEST(engine, "roc-optiq/ui", "03_minimap");
        test->TestFunc = [](ImGuiTestContext* ctx) {
            AppUiDriver d(ctx);
            if(!d.EnsureTrace())
            {
                return;
            }
            d.ZoomToFraction(0.30, 0.50);

            const ImVec2 size = ImGui::GetIO().DisplaySize;
            const float  toolbar_y = 28.0f + 18.0f;
            // Compass / minimap toggle lives near the right edge of the toolbar.
            const ImVec2 compass(size.x - 130.0f, toolbar_y);
            d.ClickScreen(compass, 20);
            // Drag inside the minimap popup body to pan the main timeline.
            const ImVec2 minimap_body(size.x * 0.55f, size.y * 0.35f);
            d.DragScreen(minimap_body, ImVec2(120.0f, 0.0f), 8);
            d.DragScreen(minimap_body, ImVec2(-160.0f, 0.0f), 8);
            d.ClickScreen(ImVec2(minimap_body.x + 40.0f, minimap_body.y), 6);
            // Close it again.
            d.ClickScreen(compass, 8);
            ctx->KeyPress(ImGuiKey_Escape);
        };
        ++test_count;
    }

    // -----------------------------------------------------------------------
    // Scene 04 - Track items: selection, per-type gear options, height resize
    // and reorder. Gear/resize are canvas widgets without stable IDs, so they
    // are best-effort coordinate interactions; the gear popup's option labels
    // are clicked via $FOCUSED once (if) it opens.
    // -----------------------------------------------------------------------
    {
        ImGuiTest* test = IM_REGISTER_TEST(engine, "roc-optiq/ui", "04_track_options");
        test->TestFunc = [](ImGuiTestContext* ctx) {
            AppUiDriver d(ctx);
            if(!d.EnsureTrace())
            {
                return;
            }
            d.EnsureSidebarVisible();
            d.ZoomToFraction(0.30, 0.45);

            const ImVec2 size      = ImGui::GetIO().DisplaySize;
            const float  menu_h    = 28.0f;
            const float  content_h = size.y - menu_h - 24.0f;
            // Track lanes occupy the upper-to-middle band of the graph; sample
            // a few Y positions to cover both flame and line tracks.
            const float lane_y[] = {
                menu_h + content_h * 0.18f,
                menu_h + content_h * 0.30f,
                menu_h + content_h * 0.42f,
            };
            const float meta_desc_x = size.x * 0.16f;  // description column
            const float meta_gear_x = size.x * 0.30f;  // gear sits near its right

            for(float y : lane_y)
            {
                // Select the track by clicking its description.
                d.ClickScreen(ImVec2(meta_desc_x, y), 8);

                // Open the gear popup and toggle every known option. Flame and
                // line tracks expose different sets; NoError no-ops the rest.
                d.ClickScreen(ImVec2(meta_gear_x, y), 8);
                const char* gear_items[] = {
                    "Color by Name",    "Color by Time Level",
                    "No Color",         "Compact Mode",
                    "Show Counter Boxes", "Alternate Counter Coloring",
                    "Highlight Y Range",
                };
                for(const char* item : gear_items)
                {
                    std::string ref = std::string("//$FOCUSED/") + item;
                    ctx->ItemClick(ref.c_str(), 0, ImGuiTestOpFlags_NoError);
                    ctx->Yield(4);
                }
                ctx->KeyPress(ImGuiKey_Escape);
                ctx->Yield(4);
            }

            // Resize a track: drag its bottom resize bar.
            d.DragScreen(ImVec2(meta_desc_x, lane_y[1]), ImVec2(0.0f, 60.0f), 8);
            d.DragScreen(ImVec2(meta_desc_x, lane_y[1] + 60.0f),
                         ImVec2(0.0f, -60.0f), 8);

            // Reorder: drag one track's description over another.
            d.DragScreen(ImVec2(meta_desc_x, lane_y[0]), ImVec2(0.0f, 80.0f), 10);
            d.DragScreen(ImVec2(meta_desc_x, lane_y[0]), ImVec2(0.0f, -80.0f), 10);
        };
        ++test_count;
    }

    // -----------------------------------------------------------------------
    // Scene 05 - Event selection + the Event Details panel (basic / ext data /
    // flow / call stack / arguments are rendered for whatever is selected).
    // -----------------------------------------------------------------------
    {
        ImGuiTest* test =
            IM_REGISTER_TEST(engine, "roc-optiq/ui", "05_event_selection_details");
        test->TestFunc = [](ImGuiTestContext* ctx) {
            AppUiDriver d(ctx);
            if(!d.EnsureTrace())
            {
                return;
            }
            d.ZoomToFraction(0.34, 0.42);
            d.Click("//Main Window/**/Event Details", 20);

            // Land on events across several lanes.
            d.SweepGraphClicks(4, 8, 0.22f, 0.55f);

            // Multi-select with Ctrl, then a Ctrl+drag range selection.
            const ImVec2 a = d.Pt(0.50f, 0.30f);
            const ImVec2 b = d.Pt(0.70f, 0.34f);
            ctx->KeyDown(ImGuiMod_Ctrl);
            d.ClickScreen(a, 4);
            d.ClickScreen(b, 4);
            ctx->KeyUp(ImGuiMod_Ctrl);
            ctx->Yield(10);

            ctx->MouseTeleportToPos(a);
            ctx->KeyDown(ImGuiMod_Ctrl);
            ctx->MouseDragWithDelta(ImVec2(280.0f, 0.0f));
            ctx->KeyUp(ImGuiMod_Ctrl);
            ctx->Yield(12);

            // Scroll through the details panel and copy a value cell.
            const ImVec2 details = d.Pt(0.70f, 0.80f);
            ctx->MouseTeleportToPos(details);
            ctx->MouseWheelY(-4.0f);
            ctx->Yield(6);
            ctx->MouseWheelY(4.0f);
            ctx->Yield(6);
            d.ClickScreen(d.Pt(0.60f, 0.74f), 6);  // copy-on-click value cell

            ctx->KeyPress(ImGuiKey_Escape);  // clear selection
            ctx->Yield(6);
        };
        ++test_count;
    }

    // -----------------------------------------------------------------------
    // Scene 06 - Event Table (MultiTrackTable): populate via track selection,
    // group-by, filter, sort, row context menu, scroll.
    // -----------------------------------------------------------------------
    {
        ImGuiTest* test = IM_REGISTER_TEST(engine, "roc-optiq/ui", "06_event_table");
        test->TestFunc = [](ImGuiTestContext* ctx) {
            AppUiDriver d(ctx);
            if(!d.EnsureTrace())
            {
                return;
            }
            d.ZoomToFraction(0.30, 0.50);
            d.Click("//Main Window/**/Event Table", 25);

            // Select a couple of lanes so the table has rows.
            const ImVec2 size = ImGui::GetIO().DisplaySize;
            d.ClickScreen(ImVec2(size.x * 0.16f, size.y * 0.24f), 10);
            ctx->KeyDown(ImGuiMod_Ctrl);
            d.ClickScreen(ImVec2(size.x * 0.16f, size.y * 0.34f), 10);
            ctx->KeyUp(ImGuiMod_Ctrl);
            ctx->Yield(30);

            // Group-by combo: open, pick the next option by keyboard, Submit,
            // then reset back to none.
            if(ctx->ItemExists("//Main Window/**/##group_by"))
            {
                ctx->ItemClick("//Main Window/**/##group_by", 0,
                               ImGuiTestOpFlags_NoError);
                ctx->Yield(6);
                ctx->KeyPress(ImGuiKey_DownArrow);
                ctx->KeyPress(ImGuiKey_Enter);
                ctx->Yield(8);
                d.Click("//Main Window/**/Submit", 30);
            }

            // Filter: type a WHERE clause, Submit, then clear + Submit.
            if(ctx->ItemExists("//Main Window/**/filters"))
            {
                ctx->ItemClick("//Main Window/**/filters", 0,
                               ImGuiTestOpFlags_NoError);
                ctx->KeyCharsReplaceEnter("duration > 0");
                ctx->Yield(6);
                d.Click("//Main Window/**/Submit", 30);
                ctx->ItemClick("//Main Window/**/filters", 0,
                               ImGuiTestOpFlags_NoError);
                ctx->KeyCharsReplaceEnter("");
                d.Click("//Main Window/**/Submit", 30);
            }

            // Sort by clicking a couple of column headers (top of the table).
            const float header_y = size.y * 0.60f;
            d.ClickScreen(ImVec2(size.x * 0.55f, header_y), 12);
            d.ClickScreen(ImVec2(size.x * 0.70f, header_y), 12);

            // Row context menu: copy variants + go-to-event (skip Export, which
            // would open a file dialog).
            const ImVec2 row(size.x * 0.55f, size.y * 0.72f);
            d.RightClickScreen(row, 10);
            ctx->ItemClick("//$FOCUSED/Copy Row Data", 0, ImGuiTestOpFlags_NoError);
            ctx->Yield(6);
            d.RightClickScreen(row, 10);
            ctx->ItemClick("//$FOCUSED/Copy Cell Data", 0, ImGuiTestOpFlags_NoError);
            ctx->Yield(6);
            d.RightClickScreen(row, 10);
            ctx->ItemClick("//$FOCUSED/Go To Event", 0, ImGuiTestOpFlags_NoError);
            ctx->Yield(15);
            ctx->KeyPress(ImGuiKey_Escape);

            // Scroll the virtualized rows.
            ctx->MouseTeleportToPos(row);
            ctx->MouseWheelY(-8.0f);
            ctx->Yield(8);
            ctx->MouseWheelY(8.0f);
            ctx->Yield(8);
        };
        ++test_count;
    }

    // -----------------------------------------------------------------------
    // Scene 07 - Sample Table: counter-track samples, group/filter, go-to-
    // sample context action, scroll.
    // -----------------------------------------------------------------------
    {
        ImGuiTest* test = IM_REGISTER_TEST(engine, "roc-optiq/ui", "07_sample_table");
        test->TestFunc = [](ImGuiTestContext* ctx) {
            AppUiDriver d(ctx);
            if(!d.EnsureTrace())
            {
                return;
            }
            d.EnsureSidebarVisible();
            d.ZoomToFraction(0.30, 0.50);
            d.Click("//Main Window/**/Sample Table", 25);

            // Select counter tracks via the sidebar so the sample table fills.
            for(const char* track : SAMPLE_COUNTER_TRACKS)
            {
                d.ScrollToTrack(track);
            }
            const ImVec2 size = ImGui::GetIO().DisplaySize;
            d.ClickScreen(ImVec2(size.x * 0.16f, size.y * 0.30f), 10);
            ctx->KeyDown(ImGuiMod_Ctrl);
            d.ClickScreen(ImVec2(size.x * 0.16f, size.y * 0.40f), 10);
            ctx->KeyUp(ImGuiMod_Ctrl);
            ctx->Yield(30);

            if(ctx->ItemExists("//Main Window/**/filters"))
            {
                ctx->ItemClick("//Main Window/**/filters", 0,
                               ImGuiTestOpFlags_NoError);
                ctx->KeyCharsReplaceEnter("value >= 0");
                d.Click("//Main Window/**/Submit", 30);
                ctx->ItemClick("//Main Window/**/filters", 0,
                               ImGuiTestOpFlags_NoError);
                ctx->KeyCharsReplaceEnter("");
                d.Click("//Main Window/**/Submit", 30);
            }

            const ImVec2 row(size.x * 0.55f, size.y * 0.72f);
            d.RightClickScreen(row, 10);
            ctx->ItemClick("//$FOCUSED/Go To Sample", 0, ImGuiTestOpFlags_NoError);
            ctx->Yield(12);
            d.RightClickScreen(row, 10);
            ctx->ItemClick("//$FOCUSED/Copy Row Data", 0, ImGuiTestOpFlags_NoError);
            ctx->Yield(6);
            ctx->KeyPress(ImGuiKey_Escape);

            ctx->MouseTeleportToPos(row);
            ctx->MouseWheelY(-8.0f);
            ctx->Yield(8);
            ctx->MouseWheelY(8.0f);
            ctx->Yield(8);
        };
        ++test_count;
    }

    // -----------------------------------------------------------------------
    // Scene 08 - Event search: queries, result navigation, go-to-event, clear.
    // -----------------------------------------------------------------------
    {
        ImGuiTest* test = IM_REGISTER_TEST(engine, "roc-optiq/ui", "08_event_search");
        test->TestFunc = [](ImGuiTestContext* ctx) {
            AppUiDriver d(ctx);
            if(!d.EnsureTrace())
            {
                return;
            }
            const char* search_path = "//Main Window/Toolbar/search_bar";
            const char* terms[]     = { "h", "copy", "Memcpy", "transpose" };
            for(const char* term : terms)
            {
                ctx->ItemClick(search_path, 0, ImGuiTestOpFlags_NoError);
                ctx->Yield(4);
                ctx->KeyCharsReplaceEnter(term);
                ctx->Yield(35);  // let the controller populate the results popup
                ctx->KeyPress(ImGuiKey_DownArrow);
                ctx->Yield(6);
                ctx->KeyPress(ImGuiKey_DownArrow);
                ctx->Yield(6);
                ctx->KeyPress(ImGuiKey_Enter);  // jump to the highlighted result
                ctx->Yield(15);
                // Right-click a result row for the Go To Event context action.
                ImGuiTestItemInfo info =
                    ctx->ItemInfo(search_path, ImGuiTestOpFlags_NoError);
                if(info.ID != 0)
                {
                    const ImVec2 result_row(
                        (info.RectFull.Min.x + info.RectFull.Max.x) * 0.5f,
                        info.RectFull.Max.y + 28.0f);
                    d.RightClickScreen(result_row, 8);
                    ctx->ItemClick("//$FOCUSED/Go To Event", 0,
                                   ImGuiTestOpFlags_NoError);
                    ctx->Yield(12);
                }
                ctx->KeyPress(ImGuiKey_Escape);
                ctx->Yield(4);
            }
            // Clear the box so the popup closes cleanly.
            ctx->ItemClick(search_path, 0, ImGuiTestOpFlags_NoError);
            ctx->KeyCharsReplaceEnter("");
            ctx->KeyPress(ImGuiKey_Escape);
            ctx->Yield(6);
        };
        ++test_count;
    }

    // -----------------------------------------------------------------------
    // Scene 09 - Annotations: create via right-click, list + visibility in the
    // Annotations tab, edit the sticky note, then delete it (cleanup).
    // -----------------------------------------------------------------------
    {
        ImGuiTest* test = IM_REGISTER_TEST(engine, "roc-optiq/ui", "09_annotations");
        test->TestFunc = [](ImGuiTestContext* ctx) {
            AppUiDriver d(ctx);
            if(!d.EnsureTrace())
            {
                return;
            }
            d.ZoomToFraction(0.32, 0.44);
            const ImVec2 note_pos = d.Pt(0.62f, 0.40f);

            // Create.
            d.RightClickScreen(note_pos, 15);
            ctx->ItemClick("//$FOCUSED/Add Annotation", 0, ImGuiTestOpFlags_NoError);
            ctx->Yield(25);
            ctx->ItemClick("//$FOCUSED/##StickyTitle", 0, ImGuiTestOpFlags_NoError);
            ctx->KeyCharsReplace("Deep Dive Note");
            ctx->ItemClick("//$FOCUSED/##StickyText", 0, ImGuiTestOpFlags_NoError);
            ctx->KeyCharsReplace("Created by the annotations scene.");
            ctx->Yield(10);
            ctx->ItemClick("//$FOCUSED/Save", 0, ImGuiTestOpFlags_NoError);
            ctx->Yield(20);

            // Annotations tab: select the row + toggle its visibility checkbox.
            d.Click("//Main Window/**/Annotations", 20);
            const ImVec2 size = ImGui::GetIO().DisplaySize;
            const ImVec2 list_row(size.x * 0.62f, size.y * 0.72f);
            d.ClickScreen(list_row, 8);
            d.ClickScreen(ImVec2(size.x * 0.50f, size.y * 0.72f), 8);  // visibility
            d.ClickScreen(ImVec2(size.x * 0.50f, size.y * 0.72f), 8);

            // Edit + delete: double-click the sticky note to open Edit
            // Annotation, tweak the text, then Delete to clean up.
            ctx->MouseTeleportToPos(note_pos);
            ctx->MouseDoubleClick(0);
            ctx->Yield(20);
            ctx->ItemClick("//$FOCUSED/##StickyText", 0, ImGuiTestOpFlags_NoError);
            ctx->KeyCharsReplace("Edited by the annotations scene.");
            ctx->Yield(8);
            ctx->ItemClick("//$FOCUSED/Delete", 0, ImGuiTestOpFlags_NoError);
            ctx->Yield(15);
            // If the edit modal never opened, make sure nothing is left open.
            ctx->KeyPress(ImGuiKey_Escape);
            ctx->Yield(6);
        };
        ++test_count;
    }

    // -----------------------------------------------------------------------
    // Scene 10 - Summary panel: HW utilization bars and the Top Kernels chart
    // (pie / bar / table modes + node & GPU filter combos + kernel select).
    // -----------------------------------------------------------------------
    {
        ImGuiTest* test = IM_REGISTER_TEST(engine, "roc-optiq/ui", "10_summary_panel");
        test->TestFunc = [](ImGuiTestContext* ctx) {
            AppUiDriver d(ctx);
            if(!d.EnsureTrace())
            {
                return;
            }
            if(!SettingsManager::GetInstance().GetAppWindowSettings().show_summary)
            {
                d.Menu("//Main Window/View/Show Summary");
                ctx->Yield(20);
            }

            // Node / GPU filter combos (cycle a couple of entries each).
            if(ctx->ItemExists("//**/##node_combo"))
            {
                ctx->ItemClick("//**/##node_combo", 0, ImGuiTestOpFlags_NoError);
                ctx->Yield(6);
                ctx->KeyPress(ImGuiKey_DownArrow);
                ctx->KeyPress(ImGuiKey_Enter);
                ctx->Yield(12);
            }
            if(ctx->ItemExists("//**/##gpu_combo"))
            {
                ctx->ItemClick("//**/##gpu_combo", 0, ImGuiTestOpFlags_NoError);
                ctx->Yield(6);
                ctx->KeyPress(ImGuiKey_DownArrow);
                ctx->KeyPress(ImGuiKey_Enter);
                ctx->Yield(12);
            }

            // The Top Kernels Pie / Bar / Table mode buttons use icon-font
            // glyph labels; click across the summary toolbar band to hit them,
            // then click into the kernel list area.
            const ImVec2 size = ImGui::GetIO().DisplaySize;
            const float  band_y = size.y * 0.30f;
            for(float fx : { 0.55f, 0.60f, 0.65f })
            {
                d.ClickScreen(ImVec2(size.x * fx, band_y), 10);
            }
            d.ClickScreen(ImVec2(size.x * 0.60f, size.y * 0.55f), 10);  // kernel row
            d.ClickScreen(ImVec2(size.x * 0.60f, size.y * 0.62f), 10);

            // Put the panel back.
            d.Menu("//Main Window/View/Show Summary");
            ctx->Yield(15);
        };
        ++test_count;
    }

    // -----------------------------------------------------------------------
    // Scene 11 - Measurement mode + flow arrows + toolbar controls.
    // -----------------------------------------------------------------------
    {
        ImGuiTest* test =
            IM_REGISTER_TEST(engine, "roc-optiq/ui", "11_measure_and_flow");
        test->TestFunc = [](ImGuiTestContext* ctx) {
            AppUiDriver d(ctx);
            if(!d.EnsureTrace())
            {
                return;
            }
            d.ZoomToFraction(0.32, 0.44);
            const ImVec2 left  = d.Pt(0.48f, 0.40f);
            const ImVec2 right = d.Pt(0.82f, 0.40f);

            // Measurement: enter, drop two rulers, toggle mode, Options, Clear,
            // Exit.
            d.Click("//Main Window/Toolbar/Measure", 15);
            d.ClickScreen(left, 8);
            d.ClickScreen(right, 12);
            d.Click("//Main Window/Toolbar/Anywhere", 8);
            d.Click("//Main Window/Toolbar/Events", 8);
            d.Click("//Main Window/Toolbar/Options", 10);
            ctx->KeyPress(ImGuiKey_Escape);
            ctx->Yield(6);
            d.Click("//Main Window/Toolbar/Clear", 8);
            d.Click("//Main Window/Toolbar/Exit", 12);

            // Same flow via the timeline right-click context menu.
            d.RightClickScreen(d.Pt(0.6f, 0.40f), 10);
            ctx->ItemClick("//$FOCUSED/Enter Measurement Mode", 0,
                           ImGuiTestOpFlags_NoError);
            ctx->Yield(8);
            d.ClickScreen(left, 6);
            d.ClickScreen(right, 8);
            d.RightClickScreen(d.Pt(0.6f, 0.40f), 10);
            ctx->ItemClick("//$FOCUSED/Clear Measurement", 0,
                           ImGuiTestOpFlags_NoError);
            ctx->Yield(6);
            d.RightClickScreen(d.Pt(0.6f, 0.40f), 10);
            ctx->ItemClick("//$FOCUSED/Exit Measurement Mode", 0,
                           ImGuiTestOpFlags_NoError);
            ctx->Yield(8);

            // Flow arrows: the flow show-all / hide-all / add toolbar buttons
            // are icon-font glyphs on the left of the toolbar. Click across that
            // band, then select an event so its flow arrows render.
            const float toolbar_y = 28.0f + 18.0f;
            for(float x : { 65.0f, 95.0f, 125.0f })
            {
                d.ClickScreen(ImVec2(x, toolbar_y), 6);
            }
            d.SweepGraphClicks(2, 4, 0.24f, 0.40f);
        };
        ++test_count;
    }

    // -----------------------------------------------------------------------
    // Scene 12 - Bookmarks: save (Ctrl+digit) / restore (digit) and the
    // bookmarks dropdown.
    // -----------------------------------------------------------------------
    {
        ImGuiTest* test = IM_REGISTER_TEST(engine, "roc-optiq/ui", "12_bookmarks");
        test->TestFunc = [](ImGuiTestContext* ctx) {
            AppUiDriver d(ctx);
            if(!d.EnsureTrace())
            {
                return;
            }
            ctx->WindowFocus("//Main Window");
            ctx->Yield(4);

            d.ZoomToFraction(0.20, 0.30);
            ctx->KeyPress(ImGuiKey_1 | ImGuiMod_Ctrl);
            ctx->Yield(4);
            d.ZoomToFraction(0.55, 0.70);
            ctx->KeyPress(ImGuiKey_3 | ImGuiMod_Ctrl);
            ctx->Yield(4);
            d.ZoomToFraction(0.0, 1.0);
            ctx->KeyPress(ImGuiKey_1);  // restore first
            ctx->Yield(10);
            ctx->KeyPress(ImGuiKey_3);  // restore second
            ctx->Yield(10);

            // Bookmarks dropdown lives on the toolbar's right side.
            const ImVec2 size = ImGui::GetIO().DisplaySize;
            d.ClickScreen(ImVec2(size.x - 230.0f, 28.0f + 18.0f), 12);
            ctx->KeyPress(ImGuiKey_Escape);
            ctx->Yield(6);
        };
        ++test_count;
    }

    // -----------------------------------------------------------------------
    // Scene 13 - Preferences (the "Settings" modal): Display, Units, Other and
    // a hotkey rebind round-trip. Ends on Cancel so nothing persists.
    // -----------------------------------------------------------------------
    {
        ImGuiTest* test = IM_REGISTER_TEST(engine, "roc-optiq/ui", "13_preferences");
        test->TestFunc = [](ImGuiTestContext* ctx) {
            AppUiDriver d(ctx);
            if(!d.EnsureTrace())
            {
                return;
            }
            d.Menu("//Main Window/Edit/Preferences");
            ctx->Yield(25);

            // Display.
            d.Click("//$FOCUSED/Display", 10);
            d.Combo("//$FOCUSED/##theme", "Dark");
            d.Combo("//$FOCUSED/##theme", "Light");
            d.Click("//$FOCUSED/DPI-based Font Scaling", 6);
            d.Click("//$FOCUSED/DPI-based Font Scaling", 6);
            for(int i = 0; i < 3; ++i)
            {
                d.Click("//$FOCUSED/+", 5);
            }
            for(int i = 0; i < 3; ++i)
            {
                d.Click("//$FOCUSED/-", 5);
            }
            d.Combo("//$FOCUSED/##time_format", "Seconds");
            d.Combo("//$FOCUSED/##time_format", "Timecode");

            // Units + Other.
            d.Click("//$FOCUSED/Units", 10);
            d.Click("//$FOCUSED/Other", 10);
            d.Click("//$FOCUSED/Don't ask before exiting application", 6);
            d.Click("//$FOCUSED/Don't ask before exiting application", 6);
            d.Click("//$FOCUSED/Don't ask before closing tabs", 6);
            d.Click("//$FOCUSED/Don't ask before closing tabs", 6);

            // Hotkeys: scroll the list, start a rebind on a row then cancel it
            // with Esc (so the binding is unchanged). Rebind rows are labelled
            // with their current chord, so target them by position.
            d.Click("//$FOCUSED/Hotkeys", 14);
            const ImVec2 size = ImGui::GetIO().DisplaySize;
            ctx->MouseTeleportToPos(ImVec2(size.x * 0.5f, size.y * 0.55f));
            ctx->MouseWheelY(-5.0f);
            ctx->Yield(8);
            ctx->MouseWheelY(5.0f);
            ctx->Yield(8);
            // Click a rebind cell (right column of the first visible row) to arm
            // rebinding, then Esc to cancel.
            d.ClickScreen(ImVec2(size.x * 0.62f, size.y * 0.40f), 8);
            ctx->KeyPress(ImGuiKey_Escape);
            ctx->Yield(8);

            d.Click("//$FOCUSED/Display", 8);
            d.Click("//$FOCUSED/Cancel", 15);
            ctx->KeyPress(ImGuiKey_Escape);
            ctx->Yield(12);
        };
        ++test_count;
    }

    // -----------------------------------------------------------------------
    // Scene 14 - Menu bar, panel toggles, About dialog, and a theme flip.
    // -----------------------------------------------------------------------
    {
        ImGuiTest* test =
            IM_REGISTER_TEST(engine, "roc-optiq/ui", "14_menus_panels_theme");
        test->TestFunc = [](ImGuiTestContext* ctx) {
            AppUiDriver d(ctx);
            if(!d.EnsureTrace())
            {
                return;
            }

            // Open each top-level menu and dismiss it.
            for(const char* menu : { "//Main Window/File", "//Main Window/Edit",
                                     "//Main Window/Help" })
            {
                ctx->ItemClick(menu, 0, ImGuiTestOpFlags_NoError);
                ctx->Yield(8);
                ctx->KeyPress(ImGuiKey_Escape);
                ctx->Yield(4);
            }

            // Toggle every View panel twice so the shell ends where it started.
            const char* view_toggles[] = {
                "//Main Window/View/Show Tool Bar",
                "//Main Window/View/Show Advanced Details Panel",
                "//Main Window/View/Show System Topology Panel",
                "//Main Window/View/Show Histogram",
                "//Main Window/View/Show Summary",
            };
            for(int pass = 0; pass < 2; ++pass)
            {
                for(const char* path : view_toggles)
                {
                    d.Menu(path);
                    ctx->Yield(6);
                }
            }

            // About dialog.
            d.Menu("//Main Window/Help/About");
            ctx->Yield(20);
            ctx->ItemClick("//$FOCUSED/Close", 0, ImGuiTestOpFlags_NoError);
            ctx->KeyPress(ImGuiKey_Escape);
            ctx->Yield(8);

            // Developer windows (only present in developer builds; NoError
            // no-ops otherwise).
            for(const char* path : { "//Main Window/Developer Options/Show Metrics",
                                     "//Main Window/Developer Options/Show Debug "
                                     "Output Window" })
            {
                d.Menu(path);
                ctx->Yield(10);
                d.Menu(path);
                ctx->Yield(10);
            }

            // Theme flip via SettingsManager (no disk write) and back, so the
            // ApplyUserSettings / font-rebuild path runs with a trace loaded.
            UserSettings&      user_settings =
                SettingsManager::GetInstance().GetUserSettings();
            const UserSettings before_flip = user_settings;
            user_settings.display_settings.use_dark_mode =
                !user_settings.display_settings.use_dark_mode;
            SettingsManager::GetInstance().ApplyUserSettings(before_flip,
                                                             /*save_json=*/false);
            ctx->Yield(30);
            const UserSettings after_flip = user_settings;
            user_settings.display_settings.use_dark_mode =
                !user_settings.display_settings.use_dark_mode;
            SettingsManager::GetInstance().ApplyUserSettings(after_flip,
                                                             /*save_json=*/false);
            ctx->Yield(30);

            d.Click("//Main Window/Toolbar/Reset View", 12);
        };
        ++test_count;
    }

    return test_count;
}

}  // namespace Test
}  // namespace View
}  // namespace RocProfVis
