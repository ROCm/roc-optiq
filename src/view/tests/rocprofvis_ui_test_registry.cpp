// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ui_test_registry.h"

#include "rocprofvis_ui_fake_data_provider.h"

#include "imgui.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_events.h"
#include "rocprofvis_events_view.h"
#include "rocprofvis_project.h"
#include "rocprofvis_root_view.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "widgets/rocprofvis_tab_container.h"
#include "widgets/rocprofvis_widget.h"

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
struct EventsViewFixture
{
    explicit EventsViewFixture(bool seed_event)
    : timeline_selection(std::make_shared<TimelineSelection>(provider.Get()))
    , view(provider.Get(), timeline_selection)
    {
        if(seed_event)
        {
            selected_event_id = provider.SeedEventDetails();
            timeline_selection->SelectTrackEvent(FakeDataProvider::kTrackId,
                                                 selected_event_id);
            view.HandleEventSelectionChanged(selected_event_id, true);
        }
    }

    void Render(const char* window_name)
    {
        ImGui::SetNextWindowSize(ImVec2(900.0f, 600.0f), ImGuiCond_Always);
        ImGui::Begin(window_name, nullptr, ImGuiWindowFlags_NoSavedSettings);
        view.Render();
        ImGui::End();
    }

    FakeDataProvider                   provider;
    std::shared_ptr<TimelineSelection> timeline_selection;
    EventsView                         view;
    uint64_t                           selected_event_id = 0;
};

struct TabFixture
{
    TabFixture()
    {
        tabs.AddTab({ "Alpha", "alpha",
                      std::make_shared<RocCustomWidget>(
                          [this]() {
                              alpha_rendered = true;
                              ImGui::TextUnformatted("Alpha Body");
                          }),
                      false });
        tabs.AddTab({ "Beta", "beta",
                      std::make_shared<RocCustomWidget>(
                          [this]() {
                              beta_rendered = true;
                              ImGui::TextUnformatted("Beta Body");
                          }),
                      false });
        tabs.SetActiveTab("alpha");
    }

    void Render()
    {
        ImGui::SetNextWindowSize(ImVec2(700.0f, 360.0f), ImGuiCond_Always);
        ImGui::Begin("TabContainer Host", nullptr, ImGuiWindowFlags_NoSavedSettings);
        tabs.Render();
        ImGui::End();
    }

    TabContainer tabs;
    bool         alpha_rendered = false;
    bool         beta_rendered  = false;
};
}  // namespace

int
RegisterRocOptiqUiTests(ImGuiTestEngine* engine, bool include_app_havoc)
{
    int test_count = 0;

    {
        std::shared_ptr<EventsViewFixture> fixture =
            std::make_shared<EventsViewFixture>(false);
        ImGuiTest* test =
            IM_REGISTER_TEST(engine, "roc-optiq/ui", "events_view_empty_state");
        test->GuiFunc = [fixture](ImGuiTestContext*) {
            fixture->Render("EventsView Empty Host");
        };
        test->TestFunc = [fixture](ImGuiTestContext* ctx) {
            ctx->SetRef("EventsView Empty Host");
            ctx->Yield(2);
            IM_CHECK_EQ(fixture->view.GetEventItemCountForTest(), static_cast<size_t>(0));
        };
        ++test_count;
    }

    {
        std::shared_ptr<EventsViewFixture> fixture =
            std::make_shared<EventsViewFixture>(true);
        ImGuiTest* test =
            IM_REGISTER_TEST(engine, "roc-optiq/ui", "events_view_seeded_event");
        test->GuiFunc = [fixture](ImGuiTestContext*) {
            fixture->Render("EventsView Seeded Host");
        };
        test->TestFunc = [fixture](ImGuiTestContext* ctx) {
            ctx->SetRef("EventsView Seeded Host");
            ctx->Yield(3);
            IM_CHECK_EQ(fixture->provider.Get().DataModel().GetEvents().EventCount(),
                        static_cast<size_t>(1));
            IM_CHECK_EQ(fixture->view.GetEventItemCountForTest(), static_cast<size_t>(1));
            IM_CHECK(fixture->view.HasEventItemForTest(fixture->selected_event_id));
        };
        ++test_count;
    }

    {
        std::shared_ptr<TabFixture> fixture = std::make_shared<TabFixture>();
        ImGuiTest* test =
            IM_REGISTER_TEST(engine, "roc-optiq/ui", "tab_container_switches_tabs");
        test->GuiFunc = [fixture](ImGuiTestContext*) { fixture->Render(); };
        test->TestFunc = [fixture](ImGuiTestContext* ctx) {
            ctx->SetRef("TabContainer Host");
            ctx->Yield(2);
            IM_CHECK(fixture->alpha_rendered);
            fixture->tabs.SetActiveTab("beta");
            ctx->Yield(2);
            const TabItem* active_tab = fixture->tabs.GetActiveTab();
            IM_CHECK(active_tab != nullptr);
            IM_CHECK_EQ(std::string(active_tab->m_id), std::string("beta"));
            IM_CHECK(fixture->beta_rendered);
        };
        ++test_count;
    }

    if(include_app_havoc)
    {
        ImGuiTest* test =
            IM_REGISTER_TEST(engine, "roc-optiq/ui-real", "recent_file_havoc");
        test->TestFunc = [](ImGuiTestContext* ctx) {
            const std::list<std::string>& recent_files =
                SettingsManager::GetInstance().GetInternalSettings().recent_files;
            if(recent_files.empty())
            {
                ctx->LogInfo("No recent files; skipping havoc test.");
                return;
            }

            const std::string recent_file = recent_files.front();
            ctx->LogInfo("Opening first recent file: %s", recent_file.c_str());

            // -----------------------------------------------------------------
            // Phase 1: Open the trace via the AppWindow API.
            // Bypassing the File/Recent Files menu avoids backslash-vs-slash
            // ambiguity in the test engine's path parser.
            // -----------------------------------------------------------------
            AppWindow* app = AppWindow::GetInstance();
            app->OpenFile(recent_file);

            for(int i = 0; i < 900; ++i)
            {
                ctx->Yield();
                if(app->GetCurrentProject() != nullptr)
                {
                    break;
                }
            }
            ctx->Yield(40);
            IM_CHECK(app->GetCurrentProject() != nullptr);

            ctx->SetRef("//Main Window");

            // All interactions use ImGuiTestOpFlags_NoError so a single missing
            // widget (e.g. caused by transient layout reflow) does not flip the
            // engine into IsError() and short-circuit every following lookup.
            auto try_click = [ctx](const char* ref, int settle_frames = 12) {
                ctx->ItemClick(ref, /*button=*/0, ImGuiTestOpFlags_NoError);
                ctx->Yield(settle_frames);
            };

            // ComboClick has no NoError flag; gate it on ItemExists for the
            // combo widget so a missing combo silently no-ops.
            auto try_combo = [ctx](const char* combo_ref,
                                   const char* item_label) {
                if(ctx->ItemExists(combo_ref))
                {
                    std::string ref = std::string(combo_ref) + "/" + item_label;
                    ctx->ComboClick(ref.c_str());
                    ctx->Yield(20);
                }
            };

            auto try_menu = [ctx](const char* path) {
                // Mirror MenuClick manually so each segment can carry NoError.
                // Path layout: //<window>/<menu>/<sub>/.../<item>
                const char* start = path;
                if(strncmp(start, "//", 2) == 0)
                    start += 2;
                const char* first_slash = strchr(start, '/');
                if(!first_slash)
                    return;
                const char* second_slash = strchr(first_slash + 1, '/');

                // Step 1: open the top-level menu (e.g. "//Main Window/View").
                std::string menu_open = "//";
                if(second_slash)
                    menu_open.append(start, second_slash - start);
                else
                    menu_open.append(start);
                ctx->ItemClick(menu_open.c_str(), /*button=*/0,
                               ImGuiTestOpFlags_NoError);
                ctx->Yield(8);
                if(!second_slash)
                    return;

                // Step 2..N: walk submenu segments inside the focused popup.
                const char* p = second_slash + 1;
                while(*p)
                {
                    const char* slash = strchr(p, '/');
                    std::string sub("//$FOCUSED/");
                    if(slash)
                        sub.append(p, slash - p);
                    else
                        sub.append(p);
                    ctx->ItemClick(sub.c_str(), /*button=*/0,
                                   ImGuiTestOpFlags_NoError);
                    ctx->Yield(8);
                    if(!slash)
                        break;
                    p = slash + 1;
                }
            };

            // -----------------------------------------------------------------
            // Phase 2: Cycle every analysis tab. Tabs live inside a
            // dynamically-named TabContainer child window, so use the wildcard
            // search to locate them anywhere under Main Window.
            // -----------------------------------------------------------------
            const char* tab_labels[] = {
                "Sample Table", "Event Details", "Track Details",
                "Annotations",  "Event Table",
            };
            for(const char* tab : tab_labels)
            {
                std::string ref = std::string("//Main Window/**/") + tab;
                // Tabs trigger heavy data fetches that resize the tab
                // container, so wait long enough for the layout to settle
                // before targeting the next tab.
                try_click(ref.c_str(), /*settle_frames=*/30);
            }
            // Right-click the active tab to flush any context menu and Esc
            // out so we exercise that codepath too.
            ctx->ItemClick("//Main Window/**/Event Table",
                           ImGuiMouseButton_Right, ImGuiTestOpFlags_NoError);
            ctx->Yield(10);
            ctx->KeyPress(ImGuiKey_Escape);
            ctx->Yield(8);

            // -----------------------------------------------------------------
            // Phase 3: Beat up the trace toolbar (Toolbar is a literal
            // BeginChild("Toolbar") inside Main Window).
            // -----------------------------------------------------------------
            try_click("//Main Window/Toolbar/Reset View", 8);

            // Drive measure mode end-to-end: enter the mode, drop two rulers
            // on the timeline, open the Options popup, hit Clear, then Exit.
            try_click("//Main Window/Toolbar/Measure", 15);
            const ImVec2 viewport_size_for_measure = ImGui::GetIO().DisplaySize;
            const float  measure_y =
                viewport_size_for_measure.y * 0.30f;  // hits a track lane
            ctx->MouseTeleportToPos(
                ImVec2(viewport_size_for_measure.x * 0.55f, measure_y));
            ctx->MouseClick(0);
            ctx->Yield(8);
            ctx->MouseTeleportToPos(
                ImVec2(viewport_size_for_measure.x * 0.80f, measure_y));
            ctx->MouseClick(0);
            ctx->Yield(15);
            // Toggle the measurement mode (Events <-> Anywhere).
            try_click("//Main Window/Toolbar/Anywhere", 8);
            try_click("//Main Window/Toolbar/Events", 8);
            // Options popup (only visible in Events mode).
            try_click("//Main Window/Toolbar/Options", 12);
            ctx->KeyPress(ImGuiKey_Escape);
            ctx->Yield(6);
            try_click("//Main Window/Toolbar/Clear", 8);
            try_click("//Main Window/Toolbar/Exit", 12);
            // Defensive Esc in case anything is still in flight.
            ctx->KeyPress(ImGuiKey_Escape);
            ctx->Yield(6);

            // Click the minimap (compass) icon. The button's label is the
            // icon-font glyph U+F273 (ICON_COMPASS); ImGui hashes the literal
            // bytes so we target it directly via the UTF-8 escape.
            const std::string compass_path =
                std::string("//Main Window/Toolbar/") + u8"\uF273";
            try_click(compass_path.c_str(), 25);
            // Poke around the popup body with a couple of clicks then close.
            const ImVec2 minimap_origin(
                viewport_size_for_measure.x * 0.55f,
                viewport_size_for_measure.y * 0.35f);
            ctx->MouseTeleportToPos(minimap_origin);
            ctx->MouseClick(0);
            ctx->Yield(6);
            ctx->MouseTeleportToPos(
                ImVec2(minimap_origin.x + 80.0f, minimap_origin.y + 30.0f));
            ctx->MouseClick(0);
            ctx->Yield(6);
            // Click the compass again to dismiss.
            try_click(compass_path.c_str(), 8);

            // -----------------------------------------------------------------
            // Phase 4: Hammer the search bar - type a single letter, then
            // longer terms, navigate the results popup with the keyboard, and
            // clear it out.
            // RenderEventSearch passes the literal id "search_bar" to
            // InputTextWithClear.
            // -----------------------------------------------------------------
            const char* search_path = "//Main Window/Toolbar/search_bar";
            const char* search_terms[] = { "h", "kernel", "Memcpy" };
            for(const char* term : search_terms)
            {
                ctx->ItemClick(search_path, 0, ImGuiTestOpFlags_NoError);
                ctx->Yield(4);
                ctx->KeyCharsReplaceEnter(term);
                ctx->Yield(35);  // wait for the search request to populate
                // Use the keyboard to navigate the results popup and pick the
                // first row (popup gets focus, ImGuiKey_DownArrow walks rows).
                ctx->KeyPress(ImGuiKey_DownArrow);
                ctx->Yield(6);
                ctx->KeyPress(ImGuiKey_Enter);
                ctx->Yield(15);
                ctx->KeyPress(ImGuiKey_Escape);
                ctx->Yield(4);
            }
            // Clear the search box so the popup closes cleanly.
            ctx->ItemClick(search_path, 0, ImGuiTestOpFlags_NoError);
            ctx->KeyCharsReplaceEnter("");
            ctx->KeyPress(ImGuiKey_Escape);
            ctx->Yield(6);

            // -----------------------------------------------------------------
            // Phase 5: Drive the timeline with hotkeys (HotkeyManager defaults:
            // W/S zoom, A/D pan, Up/Down scroll, M toggle mark, Ctrl+digit
            // saves a bookmark, digit restores it).
            // -----------------------------------------------------------------
            ctx->WindowFocus("//Main Window");
            ctx->Yield(4);
            ctx->KeyPress(ImGuiKey_S, 8);   // Start wide
            ctx->KeyPress(ImGuiKey_W, 18);  // Then zoom deeply into events
            ctx->KeyPress(ImGuiKey_D, 8);   // Pan right
            ctx->KeyPress(ImGuiKey_A, 8);   // Pan left
            ctx->KeyPress(ImGuiKey_DownArrow, 6);  // Scroll track list down
            ctx->KeyPress(ImGuiKey_UpArrow, 6);    // and back up
            ctx->KeyPress(ImGuiKey_M);             // Toggle mark
            ctx->Yield(6);
            ctx->KeyPress(ImGuiKey_M);
            ctx->Yield(6);

            // Force a very tight visible window through the same event path
            // TimelineSelection uses for NavigateToEvent / set-view-range.
            // This makes the visual zoom unmistakable even if hotkeys are not
            // focused on the timeline child.
            if(Project* project = app->GetCurrentProject())
            {
                std::shared_ptr<RocWidget> view = project->GetView();
                std::shared_ptr<RootView>  root_view =
                    std::dynamic_pointer_cast<RootView>(view);
                DataProvider* provider =
                    root_view ? root_view->GetDataProvider() : nullptr;
                if(provider)
                {
                    const TimelineModel& timeline = provider->DataModel().GetTimeline();
                    const double start = timeline.GetStartTime();
                    const double end   = timeline.GetEndTime();
                    if(end > start)
                    {
                        const double center = 0.5 * (start + end);
                        // Show a 1/200th slice around the trace center.
                        const double half_width = (end - start) / 400.0;
                        EventManager::GetInstance()->AddEvent(
                            std::make_shared<RangeEvent>(
                                static_cast<int>(RocEvents::kSetViewRange),
                                center - half_width, center + half_width,
                                provider->GetTraceFilePath()));
                        ctx->Yield(35);
                    }
                }
            }

            const ImGuiKey bookmark_digits[] = { ImGuiKey_1, ImGuiKey_3,
                                                 ImGuiKey_5, ImGuiKey_7 };
            for(ImGuiKey key : bookmark_digits)
            {
                ctx->KeyPress(key | ImGuiMod_Ctrl);  // Save bookmark
                ctx->Yield(4);
            }
            for(ImGuiKey key : bookmark_digits)
            {
                ctx->KeyPress(key);                  // Restore bookmark
                ctx->Yield(6);
            }

            // -----------------------------------------------------------------
            // Phase 6: Mouse-wheel zoom + drag-pan over the timeline graph.
            // The trace view's timeline lives inside the "Splitter View" /
            // "Splitter View Horizontal" / "Main Trace" child stack; targeting
            // it by name across multiple ImGui windows is brittle, so we
            // teleport the mouse to a known coordinate on the main viewport.
            // -----------------------------------------------------------------
            const ImVec2 viewport_size = ImGui::GetIO().DisplaySize;
            const ImVec2 timeline_pt(viewport_size.x * 0.6f,
                                     viewport_size.y * 0.45f);
            ctx->MouseTeleportToPos(timeline_pt);
            // Wheel up zooms in over the graph area. Do this hard.
            ctx->MouseWheelY(12.0f);
            ctx->Yield(6);
            ctx->MouseWheelY(8.0f);
            ctx->Yield(6);
            ctx->MouseWheelY(-4.0f);  // back out a little, but stay zoomed
            ctx->Yield(6);
            ctx->MouseDragWithDelta(ImVec2(220.0f, 0.0f));
            ctx->Yield(6);
            ctx->MouseDragWithDelta(ImVec2(-220.0f, 0.0f));
            ctx->Yield(6);
            // Vertical scroll on the description column scrolls the track list.
            const ImVec2 description_pt(viewport_size.x * 0.20f,
                                        viewport_size.y * 0.45f);
            ctx->MouseTeleportToPos(description_pt);
            ctx->MouseWheelY(-5.0f);
            ctx->Yield(6);
            ctx->MouseWheelY(5.0f);
            ctx->Yield(6);
            // Ctrl+drag over the graph = time range select.
            ctx->MouseTeleportToPos(timeline_pt);
            ctx->KeyDown(ImGuiMod_Ctrl);
            ctx->MouseDragWithDelta(ImVec2(260.0f, 0.0f));
            ctx->KeyUp(ImGuiMod_Ctrl);
            ctx->Yield(12);
            ctx->KeyPress(ImGuiKey_Escape);
            ctx->Yield(6);

            // -----------------------------------------------------------------
            // Phase 6b: Drag the splitters back and forth.
            // The trace view sits in an HSplitContainer (sidebar | rightside)
            // and the rightside is a VSplitContainer (timeline / analysis).
            // Splitter handles use GenUniqueName IDs so we can't path-target
            // them. Instead, we query the bounding rects of items we *can*
            // find (the sidebar's "Project" framed tree node, and an analysis
            // tab) to derive the exact splitter coordinates.
            // -----------------------------------------------------------------
            const float menu_h    = 28.0f;
            const float status_h  = 24.0f;
            const float content_h = viewport_size.y - menu_h - status_h;
            const ImGuiTestItemInfo project_info =
                ctx->ItemInfo("//Main Window/**/Project");
            float sidebar_right = viewport_size.x * 0.30f;
            float sidebar_mid_y = menu_h + content_h * 0.5f;
            if(project_info.ID != 0)
            {
                // The "Project" framed tree node spans the sidebar's available
                // width, so its right edge is the splitter's left edge.
                sidebar_right = project_info.RectFull.Max.x;
                sidebar_mid_y = (project_info.RectFull.Min.y +
                                 project_info.RectFull.Max.y) *
                                    0.5f +
                                100.0f;
            }
            const ImVec2 sidebar_splitter(sidebar_right + 3.0f, sidebar_mid_y);
            const float  splitter_drags_x[] = { 140.0f, -260.0f, 120.0f };
            for(float dx : splitter_drags_x)
            {
                ctx->MouseTeleportToPos(sidebar_splitter);
                ctx->MouseDragWithDelta(ImVec2(dx, 0.0f));
                ctx->Yield(6);
            }

            // Vertical splitter between timeline and analysis. Use a tab's
            // bounding box (Event Table is always visible) to find the top
            // edge of the analysis pane.
            const ImGuiTestItemInfo analysis_tab_info =
                ctx->ItemInfo("//Main Window/**/Event Table");
            float panels_x = viewport_size.x * 0.65f;
            float panels_y = menu_h + content_h * 0.62f;
            if(analysis_tab_info.ID != 0)
            {
                panels_x = (analysis_tab_info.RectFull.Min.x +
                            analysis_tab_info.RectFull.Max.x) *
                               0.5f +
                           220.0f;
                // Splitter sits a few pixels above the tab strip.
                panels_y = analysis_tab_info.RectFull.Min.y - 4.0f;
            }
            const ImVec2 panels_splitter(panels_x, panels_y);
            const float  splitter_drags_y[] = { -120.0f, 200.0f, -80.0f };
            for(float dy : splitter_drags_y)
            {
                ctx->MouseTeleportToPos(panels_splitter);
                ctx->MouseDragWithDelta(ImVec2(0.0f, dy));
                ctx->Yield(6);
            }

            // -----------------------------------------------------------------
            // Phase 6c: Sidebar tree - close-all then open-all the topology
            // hierarchy. ItemOpenAll/ItemCloseAll walk every TreeNode found
            // under the prefix.
            // -----------------------------------------------------------------
            if(ctx->ItemExists("//Main Window/**/Project"))
            {
                ctx->ItemCloseAll("//Main Window/**/Project");
                ctx->Yield(8);
                ctx->ItemOpenAll("//Main Window/**/Project");
                ctx->Yield(8);
            }

            // -----------------------------------------------------------------
            // Phase 6d: Toolbar icon-button bombing run - the Flow/Annotation
            // eye buttons and the minimap compass use icon-font glyphs as
            // labels, so the test engine can't find them by name. Click them
            // by approximate viewport coordinates instead. With NoError the
            // test won't abort if the position falls outside the toolbar.
            // -----------------------------------------------------------------
            const float toolbar_y = menu_h + 18.0f;
            const float toolbar_buttons_x[] = {
                65.0f,   // Flow show-all
                95.0f,   // Flow hide-all
                125.0f,  // Flow add
                225.0f,  // Annotations show-all
                255.0f,  // Annotations hide-all
                285.0f,  // Annotations add
            };
            for(float x : toolbar_buttons_x)
            {
                ctx->MouseTeleportToPos(ImVec2(x, toolbar_y));
                ctx->MouseClick(0);
                ctx->Yield(4);
            }
            // Compass / minimap icon sits between Measure and Reset View.
            const ImVec2 minimap_btn(viewport_size.x - 130.0f, toolbar_y);
            ctx->MouseTeleportToPos(minimap_btn);
            ctx->MouseClick(0);
            ctx->Yield(20);  // wait for popup
            // Click around the minimap popup body to interact with it.
            ctx->MouseTeleportToPos(ImVec2(minimap_btn.x - 30.0f,
                                           minimap_btn.y + 90.0f));
            ctx->MouseClick(0);
            ctx->Yield(6);
            ctx->MouseTeleportToPos(ImVec2(minimap_btn.x - 100.0f,
                                           minimap_btn.y + 90.0f));
            ctx->MouseClick(0);
            ctx->Yield(6);
            ctx->KeyPress(ImGuiKey_Escape);
            ctx->Yield(6);

            // Bookmarks combo: click at the combo position to drop it down,
            // then close.
            const ImVec2 bookmarks_btn(viewport_size.x - 230.0f, toolbar_y);
            ctx->MouseTeleportToPos(bookmarks_btn);
            ctx->MouseClick(0);
            ctx->Yield(15);
            ctx->KeyPress(ImGuiKey_Escape);
            ctx->Yield(6);
            

            // -----------------------------------------------------------------
            // Phase 6e: Click around the timeline graph to land on real events
            // (each click selects/deselects whatever event is at that pixel,
            // populating the Event Details tab). Sweep across x and stride
            // down y so we hit multiple track lanes.
            // -----------------------------------------------------------------
            const float graph_left   = viewport_size.x * 0.42f;
            const float graph_right  = viewport_size.x * 0.95f;
            const float graph_top    = menu_h + 80.0f;
            const float graph_bottom = menu_h + content_h * 0.55f;
            constexpr int click_x = 10;
            constexpr int click_y = 4;
            for(int yi = 0; yi < click_y; ++yi)
            {
                const float yt = static_cast<float>(yi) /
                                 static_cast<float>(click_y - 1);
                const float py = graph_top + (graph_bottom - graph_top) * yt;
                for(int xi = 0; xi < click_x; ++xi)
                {
                    const float xt = static_cast<float>(xi) /
                                     static_cast<float>(click_x - 1);
                    const float px =
                        graph_left + (graph_right - graph_left) * xt;
                    ctx->MouseTeleportToPos(ImVec2(px, py));
                    ctx->MouseClick(0);
                    ctx->Yield(2);
                }
            }
            // Ctrl+click somewhere to add to the multi-selection.
            ctx->MouseTeleportToPos(ImVec2(viewport_size.x * 0.55f,
                                           graph_top + 40.0f));
            ctx->KeyDown(ImGuiMod_Ctrl);
            ctx->MouseClick(0);
            ctx->KeyUp(ImGuiMod_Ctrl);
            ctx->Yield(6);
            // Esc clears the selection.
            ctx->KeyPress(ImGuiKey_Escape);
            ctx->Yield(6);

            // -----------------------------------------------------------------
            // Phase 7: Programmatically jump the viewable range, send a
            // RangeEvent, and use TimelineSelection. These produce visible
            // movement in the timeline regardless of which child window has
            // focus.
            // -----------------------------------------------------------------
            if(Project* project = app->GetCurrentProject())
            {
                std::shared_ptr<RocWidget> view = project->GetView();
                std::shared_ptr<RootView>  root_view =
                    std::dynamic_pointer_cast<RootView>(view);
                DataProvider* provider =
                    root_view ? root_view->GetDataProvider() : nullptr;
                if(provider)
                {
                    const TimelineModel& timeline = provider->DataModel().GetTimeline();
                    const double start = timeline.GetStartTime();
                    const double end   = timeline.GetEndTime();
                    if(end > start)
                    {
                        const double mid     = 0.5 * (start + end);
                        const double quarter = 0.25 * (end - start);
                        EventManager::GetInstance()->AddEvent(
                            std::make_shared<RangeEvent>(
                                static_cast<int>(RocEvents::kSetViewRange),
                                mid - quarter, mid + quarter,
                                provider->GetTraceFilePath()));
                        ctx->Yield(40);
                        EventManager::GetInstance()->AddEvent(
                            std::make_shared<RangeEvent>(
                                static_cast<int>(RocEvents::kSetViewRange),
                                start, end, provider->GetTraceFilePath()));
                        ctx->Yield(40);
                    }
                }
            }

            // -----------------------------------------------------------------
            // Phase 8: Toggle every shell panel twice via the View menu so the
            // UI ends up roughly where it started.
            // -----------------------------------------------------------------
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
                    try_menu(path);
                    ctx->Yield(6);
                }
            }

            // -----------------------------------------------------------------
            // Phase 9: Programmatic theme flip (very visible) and back.
            // -----------------------------------------------------------------
            UserSettings& user_settings =
                SettingsManager::GetInstance().GetUserSettings();
            const UserSettings before_theme = user_settings;
            user_settings.display_settings.use_dark_mode =
                !user_settings.display_settings.use_dark_mode;
            SettingsManager::GetInstance().ApplyUserSettings(before_theme);
            ctx->Yield(45);
            const UserSettings after_dark = user_settings;
            user_settings.display_settings.use_dark_mode =
                !user_settings.display_settings.use_dark_mode;
            SettingsManager::GetInstance().ApplyUserSettings(after_dark);
            ctx->Yield(45);

            // -----------------------------------------------------------------
            // Phase 10: About dialog.
            // -----------------------------------------------------------------
            try_menu("//Main Window/Help/About");
            ctx->Yield(25);
            try_click("//$FOCUSED/Close", 10);
            ctx->KeyPress(ImGuiKey_Escape);
            ctx->Yield(8);

            // -----------------------------------------------------------------
            // Phase 11: Preferences modal - exercise every category and a
            // handful of Display sub-controls, then Cancel so nothing
            // persists.
            // -----------------------------------------------------------------
            try_menu("//Main Window/Edit/Preferences");
            ctx->Yield(20);
            const char* categories[] = { "Units", "Other", "Hotkeys",
                                         "Display" };
            for(const char* category : categories)
            {
                std::string p = std::string("//$FOCUSED/") + category;
                try_click(p.c_str());
            }
            try_combo("//$FOCUSED/##theme", "Dark");
            try_combo("//$FOCUSED/##theme", "Light");
            try_click("//$FOCUSED/DPI-based Font Scaling");
            try_click("//$FOCUSED/DPI-based Font Scaling");
            for(int i = 0; i < 3; ++i)
            {
                try_click("//$FOCUSED/+", 8);
            }
            for(int i = 0; i < 3; ++i)
            {
                try_click("//$FOCUSED/-", 8);
            }
            try_combo("//$FOCUSED/##time_format", "Seconds");
            try_combo("//$FOCUSED/##time_format", "Timecode");
            try_click("//$FOCUSED/Other");
            try_click("//$FOCUSED/Don't ask before closing tabs");
            try_click("//$FOCUSED/Don't ask before closing tabs");
            try_click("//$FOCUSED/Cancel", 20);
            ctx->KeyPress(ImGuiKey_Escape);
            ctx->Yield(20);

            // -----------------------------------------------------------------
            // Phase 12: Developer Options - flip diagnostic windows on/off
            // (only registered in dev builds).
            // -----------------------------------------------------------------
            const char* dev_toggles[] = {
                "//Main Window/Developer Options/Show Metrics",
                "//Main Window/Developer Options/Show Debug Output Window",
            };
            for(const char* p : dev_toggles)
            {
                try_menu(p);
                ctx->Yield(15);
                try_menu(p);
                ctx->Yield(15);
            }

            // -----------------------------------------------------------------
            // Phase 13: Final tab re-cycle to verify the UI still responds.
            // -----------------------------------------------------------------
            for(const char* tab : tab_labels)
            {
                std::string ref = std::string("//Main Window/**/") + tab;
                try_click(ref.c_str(), 20);
            }
        };
        ++test_count;
    }

    if(include_app_havoc)
    {
        ImGuiTest* test =
            IM_REGISTER_TEST(engine, "roc-optiq/ui-real", "intro_demo_video_workflow");
        test->TestFunc = [](ImGuiTestContext* ctx) {
            const std::list<std::string>& recent_files =
                SettingsManager::GetInstance().GetInternalSettings().recent_files;
            if(recent_files.empty())
            {
                ctx->LogInfo("No recent files; skipping intro demo workflow.");
                return;
            }

            AppWindow* app = AppWindow::GetInstance();
            app->OpenFile(recent_files.front());
            for(int i = 0; i < 900; ++i)
            {
                ctx->Yield();
                if(app->GetCurrentProject())
                {
                    break;
                }
            }
            IM_CHECK(app->GetCurrentProject() != nullptr);
            ctx->Yield(30);

            auto no_error_click = [ctx](const char* ref, int frames = 8) {
                ctx->ItemClick(ref, 0, ImGuiTestOpFlags_NoError);
                ctx->Yield(frames);
            };

            auto focus_trace = [ctx]() {
                ctx->WindowFocus("//Main Window");
                ctx->Yield(4);
            };

            const ImVec2 display_size = ImGui::GetIO().DisplaySize;
            const ImVec2 graph_center(display_size.x * 0.62f, display_size.y * 0.42f);
            const ImVec2 graph_left(display_size.x * 0.48f, display_size.y * 0.42f);
            const ImVec2 graph_right(display_size.x * 0.83f, display_size.y * 0.42f);

            // Demo section: show overall trace, then zoom/pan into the busy
            // region shown throughout the recording.
            no_error_click("//Main Window/Toolbar/Reset View", 20);
            focus_trace();
            ctx->KeyPress(ImGuiKey_W, 10);
            ctx->KeyPress(ImGuiKey_D, 8);
            ctx->KeyPress(ImGuiKey_A, 4);
            ctx->MouseTeleportToPos(graph_center);
            ctx->MouseWheelY(10.0f);
            ctx->Yield(10);

            // Use the app event bus to show a very focused mid-trace slice,
            // matching the video’s repeated zoomed-in timeline demonstration.
            if(Project* project = app->GetCurrentProject())
            {
                std::shared_ptr<RootView> root_view =
                    std::dynamic_pointer_cast<RootView>(project->GetView());
                DataProvider* provider =
                    root_view ? root_view->GetDataProvider() : nullptr;
                if(provider)
                {
                    const TimelineModel& timeline = provider->DataModel().GetTimeline();
                    const double start = timeline.GetStartTime();
                    const double end   = timeline.GetEndTime();
                    if(end > start)
                    {
                        const double center = start + (end - start) * 0.35;
                        const double half_width = (end - start) / 250.0;
                        EventManager::GetInstance()->AddEvent(
                            std::make_shared<RangeEvent>(
                                static_cast<int>(RocEvents::kSetViewRange),
                                center - half_width, center + half_width,
                                provider->GetTraceFilePath()));
                        ctx->Yield(45);
                    }
                }
            }

            // Select events on multiple tracks and then inspect analysis tabs,
            // reproducing the demo’s event-selection/detail-table workflow.
            for(int row = 0; row < 4; ++row)
            {
                const float y = display_size.y * (0.24f + 0.08f * row);
                for(int col = 0; col < 5; ++col)
                {
                    const float x = display_size.x * (0.50f + 0.08f * col);
                    ctx->MouseTeleportToPos(ImVec2(x, y));
                    ctx->MouseClick(0);
                    ctx->Yield(3);
                }
            }
            ctx->KeyDown(ImGuiMod_Ctrl);
            ctx->MouseTeleportToPos(graph_left);
            ctx->MouseClick(0);
            ctx->MouseTeleportToPos(graph_right);
            ctx->MouseClick(0);
            ctx->KeyUp(ImGuiMod_Ctrl);
            ctx->Yield(12);

            const char* analysis_tabs[] = {
                "Event Details", "Track Details", "Event Table",
                "Sample Table",  "Annotations",
            };
            for(const char* tab : analysis_tabs)
            {
                std::string path = std::string("//Main Window/**/") + tab;
                no_error_click(path.c_str(), 25);
            }

            // Search workflow from the docs/video: type a short query, pick a
            // result, then clear it.
            const char* search_path = "//Main Window/Toolbar/search_bar";
            no_error_click(search_path, 5);
            ctx->KeyCharsReplaceEnter("h");
            ctx->Yield(40);
            ctx->KeyPress(ImGuiKey_DownArrow);
            ctx->KeyPress(ImGuiKey_Enter);
            ctx->Yield(20);
            no_error_click(search_path, 5);
            ctx->KeyCharsReplaceEnter("kernel");
            ctx->Yield(40);
            ctx->KeyPress(ImGuiKey_Escape);
            no_error_click(search_path, 5);
            ctx->KeyCharsReplaceEnter("");
            ctx->Yield(10);

            // Time range selection and clear.
            focus_trace();
            ctx->MouseTeleportToPos(graph_left);
            ctx->KeyDown(ImGuiMod_Ctrl);
            ctx->MouseDragWithDelta(ImVec2(360.0f, 0.0f));
            ctx->KeyUp(ImGuiMod_Ctrl);
            ctx->Yield(25);
            ctx->KeyPress(ImGuiKey_Escape);
            ctx->Yield(10);

            // Measurement demo: enter mode, drop two points, clear/exit.
            no_error_click("//Main Window/Toolbar/Measure", 15);
            ctx->MouseTeleportToPos(graph_left);
            ctx->MouseClick(0);
            ctx->Yield(8);
            ctx->MouseTeleportToPos(graph_right);
            ctx->MouseClick(0);
            ctx->Yield(15);
            no_error_click("//Main Window/Toolbar/Clear", 8);
            no_error_click("//Main Window/Toolbar/Exit", 10);

            // Splitter + panel layout demonstration.
            const ImGuiTestItemInfo project_info =
                ctx->ItemInfo("//Main Window/**/Project", ImGuiTestOpFlags_NoError);
            if(project_info.ID != 0)
            {
                const ImVec2 splitter(project_info.RectFull.Max.x + 3.0f,
                                      project_info.RectFull.Max.y + 140.0f);
                ctx->MouseTeleportToPos(splitter);
                ctx->MouseDragWithDelta(ImVec2(160.0f, 0.0f));
                ctx->Yield(8);
                ctx->MouseTeleportToPos(splitter);
                ctx->MouseDragWithDelta(ImVec2(-160.0f, 0.0f));
                ctx->Yield(8);
            }

            // Minimap + bookmarks toolbar controls.
            const std::string compass_path =
                std::string("//Main Window/Toolbar/") + u8"\uF273";
            no_error_click(compass_path.c_str(), 25);
            ctx->MouseTeleportToPos(ImVec2(display_size.x * 0.70f,
                                           display_size.y * 0.25f));
            ctx->MouseClick(0);
            ctx->Yield(8);
            no_error_click(compass_path.c_str(), 8);
            ctx->KeyPress(ImGuiKey_1 | ImGuiMod_Ctrl);
            ctx->Yield(6);
            ctx->KeyPress(ImGuiKey_1);
            ctx->Yield(10);

            // End where the video spends most of its time: timeline visible,
            // event table active, zoomed in.
            no_error_click("//Main Window/**/Event Table", 20);
        };
        ++test_count;
    }

    if(include_app_havoc)
    {
        ImGuiTest* test = IM_REGISTER_TEST(engine, "roc-optiq/ui-real",
                                           "comprehensive_demo_workflow");
        test->TestFunc = [](ImGuiTestContext* ctx) {
            const std::list<std::string>& recent_files =
                SettingsManager::GetInstance().GetInternalSettings().recent_files;
            if(recent_files.empty())
            {
                ctx->LogInfo("No recent files; skipping comprehensive workflow.");
                return;
            }

            AppWindow* app = AppWindow::GetInstance();
            app->OpenFile(recent_files.front());
            for(int i = 0; i < 900; ++i)
            {
                ctx->Yield();
                if(app->GetCurrentProject())
                {
                    break;
                }
            }
            IM_CHECK(app->GetCurrentProject() != nullptr);
            ctx->Yield(30);
            ctx->SetRef("//Main Window");

            auto noerr_click = [ctx](const char* ref, int frames = 8) {
                ctx->ItemClick(ref, 0, ImGuiTestOpFlags_NoError);
                ctx->Yield(frames);
            };
            auto noerr_dblclick = [ctx](const char* ref, int frames = 10) {
                ctx->ItemDoubleClick(ref, ImGuiTestOpFlags_NoError);
                ctx->Yield(frames);
            };
            auto try_menu_path = [ctx](const char* path) {
                const char* start = path;
                if(strncmp(start, "//", 2) == 0)
                    start += 2;
                const char* first_slash = strchr(start, '/');
                if(!first_slash)
                    return;
                const char* second_slash = strchr(first_slash + 1, '/');
                std::string head = "//";
                if(second_slash)
                    head.append(start, second_slash - start);
                else
                    head.append(start);
                ctx->ItemClick(head.c_str(), 0, ImGuiTestOpFlags_NoError);
                ctx->Yield(8);
                if(!second_slash)
                    return;
                const char* p = second_slash + 1;
                while(*p)
                {
                    const char* slash = strchr(p, '/');
                    std::string sub("//$FOCUSED/");
                    if(slash)
                        sub.append(p, slash - p);
                    else
                        sub.append(p);
                    ctx->ItemClick(sub.c_str(), 0, ImGuiTestOpFlags_NoError);
                    ctx->Yield(8);
                    if(!slash)
                        break;
                    p = slash + 1;
                }
            };

            const ImVec2 display = ImGui::GetIO().DisplaySize;
            const float menu_h    = 28.0f;
            const float status_h  = 24.0f;
            const float content_h = display.y - menu_h - status_h;

            // -----------------------------------------------------------------
            // Step 1: Land on the trace, reset view and zoom to a busy slice.
            // -----------------------------------------------------------------
            noerr_click("//Main Window/Toolbar/Reset View", 18);
            ctx->WindowFocus("//Main Window");
            ctx->KeyPress(ImGuiKey_W, 8);
            if(Project* project = app->GetCurrentProject())
            {
                std::shared_ptr<RootView> root_view =
                    std::dynamic_pointer_cast<RootView>(project->GetView());
                DataProvider* provider =
                    root_view ? root_view->GetDataProvider() : nullptr;
                if(provider)
                {
                    const TimelineModel& timeline = provider->DataModel().GetTimeline();
                    const double start = timeline.GetStartTime();
                    const double end   = timeline.GetEndTime();
                    if(end > start)
                    {
                        const double mid     = start + (end - start) * 0.40;
                        const double quarter = (end - start) / 80.0;
                        EventManager::GetInstance()->AddEvent(
                            std::make_shared<RangeEvent>(
                                static_cast<int>(RocEvents::kSetViewRange),
                                mid - quarter, mid + quarter,
                                provider->GetTraceFilePath()));
                        ctx->Yield(40);
                    }
                }
            }

            // -----------------------------------------------------------------
            // Step 2: Click an event on the timeline graph (selects it and
            // populates the Event Details tab).
            // -----------------------------------------------------------------
            const ImVec2 graph_left(display.x * 0.50f, display.y * 0.30f);
            const ImVec2 graph_mid(display.x * 0.62f, display.y * 0.32f);
            const ImVec2 graph_right(display.x * 0.82f, display.y * 0.34f);
            ctx->MouseTeleportToPos(graph_mid);
            ctx->MouseClick(0);
            ctx->Yield(15);

            // -----------------------------------------------------------------
            // Step 3: Click a track in the Description column to select it
            // (Description column is roughly the leftmost 28% of the
            // viewport, and well above the analysis tab strip).
            // -----------------------------------------------------------------
            const ImVec2 description_pt(display.x * 0.20f,
                                        menu_h + content_h * 0.30f);
            ctx->MouseTeleportToPos(description_pt);
            ctx->MouseClick(0);
            ctx->Yield(20);

            // -----------------------------------------------------------------
            // Step 4: See Event Table, then Sample Table.
            // -----------------------------------------------------------------
            noerr_click("//Main Window/**/Event Table", 30);
            noerr_click("//Main Window/**/Sample Table", 30);

            // -----------------------------------------------------------------
            // Step 5: Open Event Details and let the panel populate.
            // -----------------------------------------------------------------
            noerr_click("//Main Window/**/Event Details", 35);

            // -----------------------------------------------------------------
            // Step 6: Search for "hip", navigate the results popup, double
            // click the first one to navigate to that event.
            // -----------------------------------------------------------------
            const char* search_path = "//Main Window/Toolbar/search_bar";
            noerr_click(search_path, 5);
            ctx->KeyCharsReplaceEnter("hip");
            ctx->Yield(50);  // give the controller time to search
            // The results popup is a child window; navigate via keyboard then
            // double-click the focused entry to jump.
            ctx->KeyPress(ImGuiKey_DownArrow);
            ctx->Yield(8);
            // Try to double-click the highlighted row directly. The infinite-
            // scroll table rows do not have stable IDs, so we approximate by
            // double-clicking the popup right under the search bar.
            ImGuiTestItemInfo search_info =
                ctx->ItemInfo(search_path, ImGuiTestOpFlags_NoError);
            if(search_info.ID != 0)
            {
                const ImVec2 result_row(
                    (search_info.RectFull.Min.x + search_info.RectFull.Max.x) * 0.5f,
                    search_info.RectFull.Max.y + 28.0f);
                ctx->MouseTeleportToPos(result_row);
                ctx->MouseDoubleClick(0);
                ctx->Yield(25);
            }
            ctx->KeyPress(ImGuiKey_Escape);
            ctx->Yield(8);
            noerr_click(search_path, 5);
            ctx->KeyCharsReplaceEnter("");
            ctx->Yield(8);

            // -----------------------------------------------------------------
            // Step 7: Right-click the timeline -> Add Annotation -> fill the
            // modal -> Save.
            // -----------------------------------------------------------------
            ctx->MouseTeleportToPos(graph_mid);
            ctx->MouseClick(ImGuiMouseButton_Right);
            ctx->Yield(15);
            ctx->ItemClick("//$FOCUSED/Add Annotation", 0,
                           ImGuiTestOpFlags_NoError);
            ctx->Yield(30);
            ctx->ItemClick("//$FOCUSED/##StickyTitle", 0,
                           ImGuiTestOpFlags_NoError);
            ctx->KeyCharsReplace("UI Test Annotation");
            ctx->ItemClick("//$FOCUSED/##StickyText", 0,
                           ImGuiTestOpFlags_NoError);
            ctx->KeyCharsReplace(
                "Sticky note created by comprehensive_demo_workflow.");
            ctx->Yield(15);
            ctx->ItemClick("//$FOCUSED/Save", 0, ImGuiTestOpFlags_NoError);
            ctx->Yield(20);

            // -----------------------------------------------------------------
            // Step 8: Sidebar deep-dive. Hide a chunk of tracks by clicking
            // eye icons next to TreeNodes; then expand/collapse the topology
            // tree using ItemOpenAll/CloseAll.
            // -----------------------------------------------------------------
            ImGuiTestItemInfo project_info =
                ctx->ItemInfo("//Main Window/**/Project",
                              ImGuiTestOpFlags_NoError);
            if(project_info.ID != 0)
            {
                // Eye buttons sit at the right edge of each row. Approximate
                // their x by the sidebar's right edge minus half a button.
                const float sidebar_right = project_info.RectFull.Max.x;
                const float eye_x         = sidebar_right - 40.0f;
                const float row_h         = 24.0f;
                const float top_y         = project_info.RectFull.Max.y + row_h;
                for(int i = 0; i < 6; ++i)
                {
                    ctx->MouseTeleportToPos(
                        ImVec2(eye_x, top_y + row_h * static_cast<float>(i)));
                    ctx->MouseClick(0);
                    ctx->Yield(4);
                }
                ctx->ItemCloseAll("//Main Window/**/Project");
                ctx->Yield(8);
                ctx->ItemOpenAll("//Main Window/**/Project");
                ctx->Yield(8);

                // -------------------------------------------------------------
                // Step 9: Resize the sidebar (drag the H splitter) - shrink
                // it, then grow it past the original width.
                // -------------------------------------------------------------
                const ImVec2 splitter(sidebar_right + 3.0f,
                                      project_info.RectFull.Max.y + content_h * 0.25f);
                ctx->MouseTeleportToPos(splitter);
                ctx->MouseDragWithDelta(ImVec2(-150.0f, 0.0f));
                ctx->Yield(8);
                ctx->MouseTeleportToPos(splitter);
                ctx->MouseDragWithDelta(ImVec2(220.0f, 0.0f));
                ctx->Yield(8);
                ctx->MouseTeleportToPos(splitter);
                ctx->MouseDragWithDelta(ImVec2(-70.0f, 0.0f));
                ctx->Yield(8);
            }

            // -----------------------------------------------------------------
            // Step 10: Settings deep-dive - open Preferences and exercise
            // Display / Units / Other / Hotkeys before cancelling.
            // -----------------------------------------------------------------
            try_menu_path("//Main Window/Edit/Preferences");
            ctx->Yield(25);
            ctx->ItemClick("//$FOCUSED/Display", 0, ImGuiTestOpFlags_NoError);
            ctx->Yield(8);
            if(ctx->ItemExists("//$FOCUSED/##theme"))
            {
                ctx->ComboClick("//$FOCUSED/##theme/Dark");
                ctx->Yield(20);
                ctx->ComboClick("//$FOCUSED/##theme/Light");
                ctx->Yield(20);
            }
            noerr_click("//$FOCUSED/DPI-based Font Scaling", 6);
            noerr_click("//$FOCUSED/DPI-based Font Scaling", 6);
            for(int i = 0; i < 4; ++i)
            {
                noerr_click("//$FOCUSED/+", 5);
            }
            for(int i = 0; i < 4; ++i)
            {
                noerr_click("//$FOCUSED/-", 5);
            }
            if(ctx->ItemExists("//$FOCUSED/##time_format"))
            {
                ctx->ComboClick("//$FOCUSED/##time_format/Seconds");
                ctx->Yield(20);
                ctx->ComboClick("//$FOCUSED/##time_format/Condensed Timecode");
                ctx->Yield(20);
                ctx->ComboClick("//$FOCUSED/##time_format/Timecode");
                ctx->Yield(20);
            }
            noerr_click("//$FOCUSED/Units", 8);
            noerr_click("//$FOCUSED/Other", 8);
            noerr_click("//$FOCUSED/Don't ask before exiting application", 6);
            noerr_click("//$FOCUSED/Don't ask before exiting application", 6);
            noerr_click("//$FOCUSED/Don't ask before closing tabs", 6);
            noerr_click("//$FOCUSED/Don't ask before closing tabs", 6);
            noerr_click("//$FOCUSED/Hotkeys", 12);
            noerr_click("//$FOCUSED/Display", 8);
            noerr_click("//$FOCUSED/Cancel", 15);
            ctx->KeyPress(ImGuiKey_Escape);
            ctx->Yield(15);

            // Final flourish: re-open Event Table on a zoomed timeline so the
            // demo ends in a clean state.
            noerr_click("//Main Window/**/Event Table", 25);
        };
        ++test_count;
    }

    if(include_app_havoc)
    {
        ImGuiTest* test = IM_REGISTER_TEST(engine, "roc-optiq/ui-real",
                                           "sidebar_navigation_and_visibility");
        test->TestFunc = [](ImGuiTestContext* ctx) {
            // A scripted walkthrough that mirrors the legacy ROCm Optiq
            // intro demo video, with the SideBar visibility flow as the
            // centrepiece.
            //
            // Act 1: Land on the trace, give the audience an overview
            //        pan/zoom.
            // Act 2: Time-range selection demo (Ctrl+drag on the
            //        timeline graph), then Esc to clear.
            // Act 3: Drop a yellow sticky note via the right-click
            //        context menu so it stays anchored as we navigate.
            // Act 4: SideBar centrepiece:
            //          - Scroll-to-track on "GFX Busy" (per-leaf nav).
            //          - Hide "GFX Busy" via its eye icon.
            //          - Hide the remaining GPU counters
            //            ("Pow", "Temp", "MemUsg", "MM Busy",
            //             "UMC Busy") one by one.
            //          - Restore every counter in a single click via
            //            the parent "Counters (N)" group's eye icon
            //            (SideBar::ApplyVisibility cascades the new
            //            state through every leaf in the subtree).
            // Act 5: Cycle the View menu panel toggles (Histogram,
            //        Summary) so the audience sees the shell shrink
            //        and grow.
            // Act 6: Open Edit > Preferences, walk every category
            //        (Display swatches / font +- / time-format combo,
            //        Units, "don't ask" checkboxes, Hotkeys), then
            //        Cancel out.
            // Act 7: Click into a few events on the timeline and cycle
            //        the analysis tabs.
            // Act 8: Reset View and zoom back into a busy slice so the
            //        recording ends on a clean, on-brand frame.

            const std::list<std::string>& recent_files =
                SettingsManager::GetInstance().GetInternalSettings().recent_files;
            if(recent_files.empty())
            {
                ctx->LogInfo(
                    "No recent files; skipping sidebar workflow.");
                return;
            }

            AppWindow* app = AppWindow::GetInstance();
            app->OpenFile(recent_files.front());
            for(int i = 0; i < 900; ++i)
            {
                ctx->Yield();
                if(app->GetCurrentProject() != nullptr)
                {
                    break;
                }
            }
            IM_CHECK(app->GetCurrentProject() != nullptr);
            ctx->Yield(40);
            ctx->SetRef("//Main Window");

            // ----- Shared lambdas ---------------------------------------
            auto noerr_click = [ctx](const char* ref, int settle_frames = 10) {
                ctx->ItemClick(ref, 0, ImGuiTestOpFlags_NoError);
                ctx->Yield(settle_frames);
            };

            // Mirror MenuClick manually so each segment can carry the
            // NoError flag (matches the helper in recent_file_havoc /
            // comprehensive_demo_workflow).
            auto try_menu = [ctx](const char* path) {
                const char* start = path;
                if(strncmp(start, "//", 2) == 0)
                {
                    start += 2;
                }
                const char* first_slash  = strchr(start, '/');
                if(!first_slash)
                {
                    return;
                }
                const char* second_slash = strchr(first_slash + 1, '/');

                std::string head = "//";
                if(second_slash)
                {
                    head.append(start, second_slash - start);
                }
                else
                {
                    head.append(start);
                }
                ctx->ItemClick(head.c_str(), 0, ImGuiTestOpFlags_NoError);
                ctx->Yield(8);
                if(!second_slash)
                {
                    return;
                }

                const char* p = second_slash + 1;
                while(*p)
                {
                    const char* slash = strchr(p, '/');
                    std::string sub("//$FOCUSED/");
                    if(slash)
                    {
                        sub.append(p, slash - p);
                    }
                    else
                    {
                        sub.append(p);
                    }
                    ctx->ItemClick(sub.c_str(), 0, ImGuiTestOpFlags_NoError);
                    ctx->Yield(8);
                    if(!slash)
                    {
                        break;
                    }
                    p = slash + 1;
                }
            };

            auto try_combo = [ctx](const char* combo_ref,
                                   const char* item_label) {
                if(ctx->ItemExists(combo_ref))
                {
                    std::string ref = std::string(combo_ref) + "/" + item_label;
                    ctx->ComboClick(ref.c_str());
                    ctx->Yield(18);
                }
            };

            // Helpers for the SideBar centrepiece -------------------------
            //
            // Leaf row layout (left -> right, same line):
            //   [eye icon]  [scroll-to icon]  [track name button]
            // Each icon button is roughly icon_font_width + 2 *
            // FramePadding.x wide; the row uses ItemSpacing(2, 0).
            // Offsets are conservative so a click still lands in the
            // button hitbox at non-default font scaling.
            constexpr float ICON_BUTTON_W    = 24.0f;
            constexpr float ICON_SPACING     = 2.0f;
            constexpr float SCROLL_OFFSET    = ICON_BUTTON_W * 0.5f +
                                               ICON_SPACING;
            constexpr float EYE_OFFSET       = ICON_BUTTON_W * 1.5f +
                                               ICON_SPACING * 2.0f;
            constexpr float GROUP_EYE_OFFSET = ICON_BUTTON_W * 0.5f +
                                               ICON_SPACING;

            auto row_for = [ctx](const char* label) -> ImGuiTestItemInfo {
                std::string path =
                    std::string("//Main Window/**/") + label;
                return ctx->ItemInfo(path.c_str(),
                                     ImGuiTestOpFlags_NoError);
            };

            auto click_screen = [ctx](ImVec2 pos, int settle_frames) {
                ctx->MouseTeleportToPos(pos);
                ctx->MouseClick(0);
                ctx->Yield(settle_frames);
            };

            auto scroll_to_track = [&](const char* track_name) -> bool {
                ImGuiTestItemInfo info = row_for(track_name);
                if(info.ID == 0)
                {
                    ctx->LogWarning(
                        "Sidebar track row '%s' not found.", track_name);
                    return false;
                }
                const float mid_y =
                    (info.RectFull.Min.y + info.RectFull.Max.y) * 0.5f;
                const float scroll_x = info.RectFull.Min.x - SCROLL_OFFSET;
                click_screen(ImVec2(scroll_x, mid_y), 25);
                return true;
            };

            auto hide_track = [&](const char* track_name) -> bool {
                ImGuiTestItemInfo info = row_for(track_name);
                if(info.ID == 0)
                {
                    ctx->LogWarning(
                        "Sidebar track row '%s' not found.", track_name);
                    return false;
                }
                const float mid_y =
                    (info.RectFull.Min.y + info.RectFull.Max.y) * 0.5f;
                const float eye_x = info.RectFull.Min.x - EYE_OFFSET;
                click_screen(ImVec2(eye_x, mid_y), 10);
                return true;
            };

            // The counter group header label includes the count
            // ("Counters (6)" / "Counters (12)"). Probe the common
            // range so the workflow stays trace-agnostic.
            auto find_counter_group = [&]() -> ImGuiTestItemInfo {
                for(int n = 1; n <= 32; ++n)
                {
                    std::string label =
                        "Counters (" + std::to_string(n) + ")";
                    ImGuiTestItemInfo info = row_for(label.c_str());
                    if(info.ID != 0)
                    {
                        return info;
                    }
                }
                return ImGuiTestItemInfo{};
            };

            // Re-issue a focused SetViewRange via the EventManager.
            // This is the same channel TimelineView listens to and is
            // visually identical to the user's W-key zooming, but it
            // doesn't depend on which child window owns keyboard focus.
            auto zoom_to_fraction = [ctx, app](double left_frac,
                                               double right_frac) {
                Project* project = app->GetCurrentProject();
                if(!project)
                {
                    return;
                }
                std::shared_ptr<RootView> root_view =
                    std::dynamic_pointer_cast<RootView>(project->GetView());
                DataProvider* provider =
                    root_view ? root_view->GetDataProvider() : nullptr;
                if(!provider)
                {
                    return;
                }
                const TimelineModel& timeline =
                    provider->DataModel().GetTimeline();
                const double start = timeline.GetStartTime();
                const double end   = timeline.GetEndTime();
                if(end <= start)
                {
                    return;
                }
                const double span = end - start;
                EventManager::GetInstance()->AddEvent(
                    std::make_shared<RangeEvent>(
                        static_cast<int>(RocEvents::kSetViewRange),
                        start + span * left_frac,
                        start + span * right_frac,
                        provider->GetTraceFilePath()));
                ctx->Yield(35);
            };

            // ----- Act 1: Initial overview ------------------------------
            // Make sure the System Topology panel is showing - some
            // earlier tests in the same session may have hidden it.
            if(!SettingsManager::GetInstance()
                    .GetAppWindowSettings()
                    .show_sidebar)
            {
                try_menu("//Main Window/View/Show System Topology Panel");
                ctx->Yield(15);
            }

            noerr_click("//Main Window/Toolbar/Reset View", 20);
            ctx->WindowFocus("//Main Window");
            ctx->Yield(4);
            // Quick "look around" - the legacy demo spends a few seconds
            // panning and zooming before doing anything else.
            ctx->KeyPress(ImGuiKey_W, 12);
            ctx->KeyPress(ImGuiKey_D, 6);
            ctx->KeyPress(ImGuiKey_A, 6);
            const ImVec2 viewport_size = ImGui::GetIO().DisplaySize;
            const ImVec2 graph_center(viewport_size.x * 0.62f,
                                      viewport_size.y * 0.40f);
            const ImVec2 graph_left(viewport_size.x * 0.48f,
                                    viewport_size.y * 0.40f);
            const ImVec2 graph_right(viewport_size.x * 0.85f,
                                     viewport_size.y * 0.40f);
            ctx->MouseTeleportToPos(graph_center);
            ctx->MouseWheelY(8.0f);
            ctx->Yield(10);
            // Settle on a busy slice ~35% into the trace.
            zoom_to_fraction(0.32, 0.42);

            // ----- Act 2: Time-range selection (Ctrl+drag) --------------
            ctx->MouseTeleportToPos(graph_left);
            ctx->KeyDown(ImGuiMod_Ctrl);
            ctx->MouseDragWithDelta(ImVec2(320.0f, 0.0f));
            ctx->KeyUp(ImGuiMod_Ctrl);
            ctx->Yield(25);
            ctx->KeyPress(ImGuiKey_Escape);
            ctx->Yield(8);

            // ----- Act 3: Sticky note creation --------------------------
            ctx->MouseTeleportToPos(graph_center);
            ctx->MouseClick(ImGuiMouseButton_Right);
            ctx->Yield(15);
            ctx->ItemClick("//$FOCUSED/Add Annotation", 0,
                           ImGuiTestOpFlags_NoError);
            ctx->Yield(28);
            ctx->ItemClick("//$FOCUSED/##StickyTitle", 0,
                           ImGuiTestOpFlags_NoError);
            ctx->KeyCharsReplace("Sidebar Demo");
            ctx->ItemClick("//$FOCUSED/##StickyText", 0,
                           ImGuiTestOpFlags_NoError);
            ctx->KeyCharsReplace(
                "Anchored before the SideBar visibility walkthrough.");
            ctx->Yield(15);
            ctx->ItemClick("//$FOCUSED/Save", 0, ImGuiTestOpFlags_NoError);
            ctx->Yield(20);

            // ----- Act 4: SideBar centrepiece ---------------------------
            // The framed "Project" header is DefaultOpen, but
            // ItemOpenAll guards against any user-collapsed sub-trees.
            if(ctx->ItemExists("//Main Window/**/Project"))
            {
                ctx->ItemOpenAll("//Main Window/**/Project");
                ctx->Yield(15);
            }

            // Phase 4a: navigate the timeline to GFX Busy via the
            // sidebar's scroll-to icon (per-leaf navigation).
            scroll_to_track("GFX Busy");

            // Phase 4b: hide GFX Busy via its own eye icon.
            hide_track("GFX Busy");

            // Phase 4c: hide the remaining GPU counter tracks one by one
            // to demonstrate the per-leaf eye toggle.
            const char* counters_to_hide[] = {
                "Pow", "Temp", "MemUsg", "MM Busy", "UMC Busy",
            };
            for(const char* counter : counters_to_hide)
            {
                hide_track(counter);
            }

            // Phase 4d: a single click on the parent group restores every
            // counter we just hid. After hiding all leaves the group's
            // cached state is kAllHidden, so DrawEyeButton flips it to
            // kAllVisible and ApplyVisibility cascades through the
            // subtree.
            ImGuiTestItemInfo group_info = find_counter_group();
            if(group_info.ID != 0)
            {
                const float mid_y = (group_info.RectFull.Min.y +
                                     group_info.RectFull.Max.y) *
                                    0.5f;
                const float eye_x =
                    group_info.RectFull.Min.x - GROUP_EYE_OFFSET;
                click_screen(ImVec2(eye_x, mid_y), 30);
            }
            else
            {
                // Fallback: if the group label couldn't be located,
                // walk the leaves and re-toggle each eye individually.
                hide_track("GFX Busy");
                for(const char* counter : counters_to_hide)
                {
                    hide_track(counter);
                }
            }

            // ----- Act 5: View menu panel toggles -----------------------
            // Recreate the legacy demo's "big layout change" beat by
            // hiding then restoring each ancillary panel.
            const char* view_toggles[] = {
                "//Main Window/View/Show Histogram",
                "//Main Window/View/Show Summary",
            };
            for(int pass = 0; pass < 2; ++pass)
            {
                for(const char* path : view_toggles)
                {
                    try_menu(path);
                    ctx->Yield(15);
                }
            }

            // ----- Act 6: Settings modal -------------------------------
            try_menu("//Main Window/Edit/Preferences");
            ctx->Yield(25);
            noerr_click("//$FOCUSED/Display", 10);
            // Theme combo: cycle Dark <-> Light a couple of times so the
            // colour swatches flip visibly.
            try_combo("//$FOCUSED/##theme", "Dark");
            try_combo("//$FOCUSED/##theme", "Light");
            // DPI scaling toggle and font +/- steps.
            noerr_click("//$FOCUSED/DPI-based Font Scaling", 6);
            noerr_click("//$FOCUSED/DPI-based Font Scaling", 6);
            for(int i = 0; i < 3; ++i)
            {
                noerr_click("//$FOCUSED/+", 6);
            }
            for(int i = 0; i < 3; ++i)
            {
                noerr_click("//$FOCUSED/-", 6);
            }
            // Time-format combo: matches the legacy demo's "combo
            // dropdown (time format)" beat.
            try_combo("//$FOCUSED/##time_format", "Seconds");
            try_combo("//$FOCUSED/##time_format", "Condensed Timecode");
            try_combo("//$FOCUSED/##time_format", "Timecode");

            // Walk the remaining categories.
            noerr_click("//$FOCUSED/Units", 12);
            noerr_click("//$FOCUSED/Other", 12);
            noerr_click("//$FOCUSED/Don't ask before exiting application", 6);
            noerr_click("//$FOCUSED/Don't ask before exiting application", 6);
            noerr_click("//$FOCUSED/Don't ask before closing tabs", 6);
            noerr_click("//$FOCUSED/Don't ask before closing tabs", 6);
            noerr_click("//$FOCUSED/Hotkeys", 14);
            // Briefly scroll the Hotkeys list so the audience sees it
            // is a real, scrollable table.
            ctx->MouseTeleportToPos(
                ImVec2(viewport_size.x * 0.5f, viewport_size.y * 0.55f));
            ctx->MouseWheelY(-4.0f);
            ctx->Yield(8);
            ctx->MouseWheelY(4.0f);
            ctx->Yield(8);
            noerr_click("//$FOCUSED/Display", 10);
            // Cancel so none of the changes persist into the next test.
            noerr_click("//$FOCUSED/Cancel", 15);
            ctx->KeyPress(ImGuiKey_Escape);
            ctx->Yield(15);

            // ----- Act 7: Event clicking + analysis tab cycle ----------
            // Click around the graph to land on real events; each click
            // selects/deselects whatever event is at that pixel and
            // populates Event Details.
            const float graph_top    = viewport_size.y * 0.20f;
            const float graph_bottom = viewport_size.y * 0.55f;
            const float graph_xmin   = viewport_size.x * 0.42f;
            const float graph_xmax   = viewport_size.x * 0.92f;
            for(int row = 0; row < 4; ++row)
            {
                const float yt = static_cast<float>(row) / 3.0f;
                const float py = graph_top + (graph_bottom - graph_top) * yt;
                for(int col = 0; col < 6; ++col)
                {
                    const float xt = static_cast<float>(col) / 5.0f;
                    const float px =
                        graph_xmin + (graph_xmax - graph_xmin) * xt;
                    ctx->MouseTeleportToPos(ImVec2(px, py));
                    ctx->MouseClick(0);
                    ctx->Yield(2);
                }
            }
            const char* analysis_tabs[] = {
                "Event Details", "Track Details", "Event Table",
                "Sample Table",  "Annotations",
            };
            for(const char* tab : analysis_tabs)
            {
                std::string ref = std::string("//Main Window/**/") + tab;
                noerr_click(ref.c_str(), 25);
            }

            // ----- Act 8: Final flourish -------------------------------
            // End where the legacy demo spent most of its time: timeline
            // visible, Event Table active, zoomed into a busy slice with
            // our sticky note still anchored.
            noerr_click("//Main Window/Toolbar/Reset View", 18);
            zoom_to_fraction(0.30, 0.45);
            noerr_click("//Main Window/**/Event Table", 25);
        };
        ++test_count;
    }

    return test_count;
}

}  // namespace Test
}  // namespace View
}  // namespace RocProfVis
