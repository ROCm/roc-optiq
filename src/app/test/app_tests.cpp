// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
#include "app_tests.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"
#include "imgui.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_project.h"
#include "rocprofvis_trace_view.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_analysis_view.h"
#include "rocprofvis_events_view.h"
#include "rocprofvis_timeline_view.h"
#include "rocprofvis_flame_track_item.h"
#include "rocprofvis_measurement_controller.h"
#include "rocprofvis_minimap.h"
#include "rocprofvis_event_search.h"
#include "rocprofvis_summary_view.h"
#include "icons/rocprovfis_icon_defines.h"
#include <string>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cmath>
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "rocprofvis_data_provider.h"
#include "model/rocprofvis_trace_data_model.h"
#include "model/rocprofvis_timeline_model.h"
#include "compute/rocprofvis_compute_view.h"
#include "compute/rocprofvis_compute_selection.h"
#include "model/compute/rocprofvis_compute_data_model.h"
#include "widgets/rocprofvis_tab_container.h"
#include "rocprofvis_view_test_access.h"
#include "sqlite3.h"
using namespace RocProfVis::View;

namespace
{
    TraceView* GetTraceViewOrSkip(ImGuiTestContext* ctx)
    {
        AppWindow* app = AppWindow::GetInstance();
        Project* project = app->GetCurrentProject();
        // A null project means the db never opened (a real regression); fail hard.
        // A non-null project of the wrong view type is an expected wrong-db skip.
        IM_CHECK_RETV(project != nullptr, nullptr);
        TraceView* tv = dynamic_cast<TraceView*>(project->GetView().get());
        if (tv == nullptr)
        {
            ctx->LogWarning("SKIP: no trace view loaded (open a system/trace profile to exercise this)");
            return nullptr;
        }
        return tv;
    }

    ComputeView* GetComputeViewOrSkip(ImGuiTestContext* ctx)
    {
        AppWindow* app = AppWindow::GetInstance();
        Project* project = app->GetCurrentProject();
        IM_CHECK_RETV(project != nullptr, nullptr);
        ComputeView* cv = dynamic_cast<ComputeView*>(project->GetView().get());
        if (cv == nullptr)
        {
            ctx->LogWarning("SKIP: no compute view loaded (open a compute profile to exercise this)");
            return nullptr;
        }
        return cv;
    }

// Flame-graph event bars are raw draw_list rects registered with the Test
// Engine via IMGUI_TEST_ENGINE_ITEM_ADD under the track's "FV" child window.
// These helpers gather that window's bars and pick reliably clickable targets
// by width: the widest two avoid 1px slivers a click would miss, and give
// multi-select tests two distinct targets on the same lane.

// Finds the two widest bars under `flame_window_id`. out_second may stay empty
// if fewer than two bars exist. Returns the number found (0, 1, or 2).
int WidestFlameBars(ImGuiTestContext* ctx, unsigned int flame_window_id,
                    ImGuiTestItemInfo& out_first, ImGuiTestItemInfo& out_second)
{
    if(flame_window_id == 0) return 0;
    ImGuiTestItemList bars;
    ctx->GatherItems(&bars, ImGuiTestRef(flame_window_id));

    int   found        = 0;
    float first_width  = -1.0f;
    float second_width = -1.0f;
    for(const ImGuiTestItemInfo& bar : bars)
    {
        const float width = bar.RectFull.GetWidth();
        if(width > first_width)
        {
            out_second   = out_first;
            second_width = first_width;
            out_first    = bar;
            first_width  = width;
        }
        else if(width > second_width)
        {
            out_second   = bar;
            second_width = width;
        }
        found = found < 2 ? found + 1 : 2;
    }
    return found;
}

bool FirstEventScreenCenter(ImGuiTestContext* ctx, unsigned int flame_window_id,
                            ImVec2& out_center)
{
    ImGuiTestItemInfo first, second;
    if(WidestFlameBars(ctx, flame_window_id, first, second) < 1) return false;
    out_center = first.RectFull.GetCenter();
    return true;
}

bool TwoEventScreenCenters(ImGuiTestContext* ctx, unsigned int flame_window_id,
                           ImVec2& out_first, ImVec2& out_second)
{
    ImGuiTestItemInfo first, second;
    if(WidestFlameBars(ctx, flame_window_id, first, second) < 2) return false;
    out_first  = first.RectFull.GetCenter();
    out_second = second.RectFull.GetCenter();
    return true;
}

// Finds the first pair of bars that overlap in time but sit on different
// rows, returning their centers. Only overlapping bars meaningfully test
// selection -- well-separated bars have disjoint hit-regions and resolve
// correctly regardless, so clicking them would prove nothing.
bool TwoStackedEventScreenCenters(ImGuiTestContext* ctx, unsigned int flame_window_id,
                                  ImVec2& out_first, ImVec2& out_second)
{
    if(flame_window_id == 0) return false;
    ImGuiTestItemList bars;
    ctx->GatherItems(&bars, ImGuiTestRef(flame_window_id));

    const int count = bars.GetSize();
    // Skip bars too thin to reliably click.
    const float kMinClickWidth = 4.0f;
    for(int i = 0; i < count; i++)
    {
        const ImGuiTestItemInfo* a = bars.GetByIndex(i);
        if(a->RectFull.GetWidth() < kMinClickWidth) continue;
        // Two bars are on different rows if their centers are more than half a
        // bar-height apart.
        const float row_threshold = a->RectFull.GetHeight() * 0.5f;
        for(int j = i + 1; j < count; j++)
        {
            const ImGuiTestItemInfo* b = bars.GetByIndex(j);
            if(b->RectFull.GetWidth() < kMinClickWidth) continue;
            const bool overlap_x = a->RectFull.Min.x < b->RectFull.Max.x &&
                                   b->RectFull.Min.x < a->RectFull.Max.x;
            const bool diff_row =
                fabsf(a->RectFull.GetCenter().y - b->RectFull.GetCenter().y) > row_threshold;
            if(overlap_x && diff_row)
            {
                out_first  = a->RectFull.GetCenter();
                out_second = b->RectFull.GetCenter();
                return true;
            }
        }
    }
    return false;
}

// Restores show_summary when it goes out of scope. The Summary tests set it
// true to drive their load path; without this, that state would leak into
// later tests and cover the timeline.
struct ShowSummaryGuard
{
    bool prev;
    ShowSummaryGuard()
        : prev(SettingsManager::GetInstance().GetAppWindowSettings().show_summary)
    {
    }
    ~ShowSummaryGuard()
    {
        SettingsManager::GetInstance().GetAppWindowSettings().show_summary = prev;
    }
};

// Run one SUM or COUNT on the sample db through a private read-only connection,
// handing the single result back via `out`. Optiq builds its own totals by
// summing raw rows in C++, so this SQL is an independent second opinion: if
// Optiq's math is off, the two numbers diverge. Returns false on any sqlite error
// so the caller fails loudly instead of trusting a garbage value.
bool DbScalarQuery(const std::string& db_path, const char* sql, double& out)
{
    sqlite3* db = nullptr;
    if(sqlite3_open_v2(db_path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK)
    {
        if(db) sqlite3_close(db);
        return false;
    }
    sqlite3_stmt* stmt = nullptr;
    bool          ok   = false;
    if(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK &&
       sqlite3_step(stmt) == SQLITE_ROW)
    {
        out = sqlite3_column_double(stmt, 0);
        ok  = true;
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok;
}

// True for the v4 schema, where kernel timestamps live in their own
// rocpd_timestamp table joined by start_id/end_id, false for the older layout
// that keeps start/end inline on rocpd_kernel_dispatch. The duration query picks
// its form from this, matching the query factory in GetRocprofKernelDispatchSliceQuery.
bool DbHasTimestampTable(const std::string& db_path)
{
    double n = 0.0;
    return DbScalarQuery(
               db_path,
               "SELECT COUNT(*) FROM sqlite_master WHERE type IN ('table','view') "
               "AND name = 'rocpd_timestamp'",
               n) &&
           n > 0.0;
}
}  // namespace

void RegisterAppTests(ImGuiTestEngine* e)
{
    ImGuiTest* t = nullptr;

    t = IM_REGISTER_TEST(e, "app", "common_file_menu_exists");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        ctx->SetRef("Main Window");
        IM_CHECK(ctx->ItemExists("##MenuBar/File"));
    
    };

    t = IM_REGISTER_TEST(e, "app", "common_file_menu_opens");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        ctx->SetRef("Main Window");
        ctx->ItemClick("##MenuBar/File");
        IM_CHECK(ctx->ItemExists("//Menu_00/Open"));
        // Close the menu so its popup doesn't intercept clicks in later tests.
        ctx->PopupCloseAll();
    };
    t = IM_REGISTER_TEST(e, "app", "sys_events_view_populates");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        AnalysisView* av = TraceViewTestPeer{*tv}.AnalysisViewPtr();
        IM_CHECK(av != nullptr);
        if (av == nullptr) return;
        EventsView* ev = AnalysisViewTestPeer{*av}.EventsViewPtr();
        IM_CHECK(ev != nullptr);
        if (ev == nullptr) return;

        // A prior run may have left an event selected; the clear is dispatched
        // through EventManager, so yield before asserting the empty baseline.
        TraceViewTestPeer{*tv}.ClearEventSelection();
        ctx->Yield(3);
        IM_CHECK(EventsViewTestPeer{*ev}.EventItemCount() == 0);

        // Timeline events are canvas-drawn (no widget IDs). Click the screen
        // center of the first rendered event box, captured by the renderer with
        // the geometry it draws/hit-tests with, so the click is window- and
        // trace-independent (no hard-coded sidebar/track offsets).
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;

        ctx->Yield(3);

        ImVec2 event_center(0.0f, 0.0f);
        bool   have_center = FirstEventScreenCenter(
            ctx, TimelineViewTestPeer{*tlv}.FirstFlameWindowId(), event_center);
        IM_CHECK(have_center);
        if (!have_center) return;

        // Selection is deferred a frame, so move/release with the mouse parked.
        ctx->MouseMoveToPos(event_center);
        ctx->Yield(2);
        ctx->MouseDown(0);
        ctx->Yield(1);
        ctx->MouseUp(0);
        ctx->Yield(3);

        IM_CHECK(EventsViewTestPeer{*ev}.EventItemCount() > 0);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_timeline_zoom_hotkey");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;

        // The W/S zoom hotkeys only fire while the timeline has pseudo-focus,
        // which a mouse-down inside the graph sets. Park the cursor on the first
        // rendered event (its captured center is a point known to be in-graph)
        // and press the mouse to acquire focus.
        ctx->Yield(3);
        ImVec2 event_center(0.0f, 0.0f);
        bool   have_center = FirstEventScreenCenter(
            ctx, TimelineViewTestPeer{*tlv}.FirstFlameWindowId(), event_center);
        IM_CHECK(have_center);
        if (!have_center) return;

        ctx->MouseMoveToPos(event_center);
        ctx->MouseDown(0);
        ctx->Yield(1);
        ctx->MouseUp(0);
        ctx->Yield(2);

        const float zoom_before = TraceViewTestPeer{*tv}.TimelineViewPtr()->GetViewCoords().z;

        // Zoom in: hotkey is consumed in the timeline's per-frame input handler,
        // so keep the cursor parked in-graph while pressing.
        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_W);
        ctx->Yield(3);
        const float zoom_in = TraceViewTestPeer{*tv}.TimelineViewPtr()->GetViewCoords().z;
        IM_CHECK(zoom_in > zoom_before);

        // Zoom out returns toward the starting zoom.
        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_S);
        ctx->Yield(3);
        const float zoom_out = TraceViewTestPeer{*tv}.TimelineViewPtr()->GetViewCoords().z;
        IM_CHECK(zoom_out < zoom_in);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_bookmark_save_restore_hotkey");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;

        // Ctrl+1 always writes bookmark slot 1, and m_bookmarks is a slot-keyed
        // map, so a re-save overwrites rather than grows it. Clear first so the
        // 0->1 assertion holds on every invocation in a persistent process (the
        // interactive harness reuses one process; headless is fresh each run).
        TraceViewTestPeer{*tv}.ClearBookmarks();
        ctx->Yield(1);
        IM_CHECK(TraceViewTestPeer{*tv}.BookmarkCount() == 0);

        // HandleHotKeys is gated on IsWindowFocused(RootAndChildWindows) for
        // "Main Window". Headless that focus is implicit, but interactively the
        // Test Engine window holds it and the chord is dropped. Focus explicitly,
        // then click an event to land the cursor in-graph.
        ctx->Yield(3);
        ImVec2 event_center(0.0f, 0.0f);
        bool   have_center = FirstEventScreenCenter(
            ctx, TimelineViewTestPeer{*tlv}.FirstFlameWindowId(), event_center);
        IM_CHECK(have_center);
        if (!have_center) return;

        ctx->WindowFocus("Main Window");
        ctx->MouseMoveToPos(event_center);
        ctx->MouseClick(0);
        ctx->Yield(2);

        // Save the current view, then record the coords the bookmark captured.
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_1);
        ctx->Yield(3);
        IM_CHECK(TraceViewTestPeer{*tv}.BookmarkCount() == 1);
        const ViewCoords saved = TraceViewTestPeer{*tv}.TimelineViewPtr()->GetViewCoords();
        const double saved_span = saved.v_max_x - saved.v_min_x;
        IM_CHECK(saved_span > 0.0);
        if (saved_span <= 0.0) return;

        // Zoom in so the view range changes away from the saved one.
        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_W);
        ctx->Yield(3);
        const ViewCoords moved = TraceViewTestPeer{*tv}.TimelineViewPtr()->GetViewCoords();
        IM_CHECK((moved.v_max_x - moved.v_min_x) < saved_span);

        // Restore: bare "1" moves the view back to the saved range. Restore
        // reconstructs zoom/offset from the saved span through a float, so assert
        // within a small relative tolerance rather than exact equality.
        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_1);
        ctx->Yield(3);
        const ViewCoords restored = TraceViewTestPeer{*tv}.TimelineViewPtr()->GetViewCoords();
        const double tol = saved_span * 0.01;
        IM_CHECK(fabs(restored.v_min_x - saved.v_min_x) < tol);
        IM_CHECK(fabs(restored.v_max_x - saved.v_max_x) < tol);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_event_multi_select");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;
        std::shared_ptr<TimelineSelection> sel = tv->GetTimelineSelection();
        IM_CHECK(sel != nullptr);
        if (sel == nullptr) return;

        // Start from a clean selection.
        TraceViewTestPeer{*tv}.ClearEventSelection();
        ctx->Yield(3);
        std::vector<uint64_t> ids;
        sel->GetSelectedEvents(ids);
        IM_CHECK(ids.empty());

        // Need two distinct, clickable events in one flame track.
        ctx->Yield(3);
        ImVec2 first(0.0f, 0.0f), second(0.0f, 0.0f);
        bool have_two = TwoEventScreenCenters(
            ctx, TimelineViewTestPeer{*tlv}.FirstFlameWindowId(), first, second);
        if (!have_two)
        {
            ctx->LogWarning("SKIP: track lacks two distinct events to multi-select");
            return;
        }

        // Plain click selects the first event.
        ctx->MouseMoveToPos(first);
        ctx->MouseClick(0);
        ctx->Yield(3);
        ids.clear();
        sel->GetSelectedEvents(ids);
        IM_CHECK(ids.size() == 1);

        // Ctrl+click the second event adds to the selection (multi-select)
        // rather than replacing it. KeyDown holds the modifier the flame track
        // reads via HotkeyManager during click handling.
        ctx->KeyDown(ImGuiMod_Ctrl);
        ctx->MouseMoveToPos(second);
        ctx->MouseClick(0);
        ctx->Yield(3);
        ctx->KeyUp(ImGuiMod_Ctrl);
        ctx->Yield(2);

        ids.clear();
        sel->GetSelectedEvents(ids);
        IM_CHECK(ids.size() == 2);

        // Leave a clean selection for following tests.
        TraceViewTestPeer{*tv}.ClearEventSelection();
        ctx->Yield(2);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_minimap_toggle_drives_click");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        Minimap* mm = TraceViewTestPeer{*tv}.MinimapPtr();
        IM_CHECK(mm != nullptr);
        if (mm == nullptr) return;

        IM_CHECK(MinimapTestPeer{*mm}.ShowEvents() == true);
        // Counter overlay state before touching events, to confirm the two
        // layers toggle independently (checklist: "Layers independent").
        const bool counters_before = MinimapTestPeer{*mm}.ShowCounters();

        // The ICON_COMPASS toolbar button and the ##events checkbox are both real
        // ImGui widgets nested under unknown layout child-windows. The "**/"
        // wildcard ref locates them past those intermediates by trailing label.
        // The Minimap is a plain Begin("Minimap") window, not a popup, so ref it
        // by title rather than $FOCUSED (which the window does not reliably own).
        ctx->Yield(3);
        ctx->ItemClick("//Main Window/**/\uF273");  // \uF273 = ICON_COMPASS
        ctx->Yield(3);

        ctx->SetRef("//Minimap");
        IM_CHECK(ctx->ItemExists("**/##events"));
        ctx->ItemClick("**/##events");
        ctx->Yield(2);
        IM_CHECK(MinimapTestPeer{*mm}.ShowEvents() == false);
        // Toggling events must not disturb the counter overlay.
        IM_CHECK(MinimapTestPeer{*mm}.ShowCounters() == counters_before);

        // Toggle back to confirm the click is bidirectional.
        ctx->ItemClick("**/##events");
        ctx->Yield(2);
        IM_CHECK(MinimapTestPeer{*mm}.ShowEvents() == true);

        // Close the Minimap popup so it doesn't cover later tests' widgets.
        ctx->ItemClick("//Main Window/**/\uF273");  // \uF273 = ICON_COMPASS
        ctx->Yield(2);
    };

    t = IM_REGISTER_TEST(e, "app", "compute_view_tab_switch");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        ComputeView* cv = GetComputeViewOrSkip(ctx);
        if (!cv) return;
        TabContainer* tc = ComputeViewTestPeer{*cv}.TabContainerPtr();
        if (tc == nullptr)
        {
            ctx->LogWarning("SKIP: compute view has no tab container");
            return;
        }

        IM_CHECK(TabContainerTestPeer{*tc}.TabCount() >= 2);
        if (TabContainerTestPeer{*tc}.TabCount() < 2) return;

        ctx->Yield(3);
        const int start_idx = TabContainerTestPeer{*tc}.ActiveTabIndex();
        IM_CHECK(start_idx >= 0);

        // Each tab is submitted as BeginTabItem(label) under PushID(id), nested in
        // unknown layout child-windows. The "**/" wildcard locates the tab header
        // by its trailing label past those intermediates and the id seed.
        const int target_idx = (start_idx == 0) ? 1 : 0;
        const std::vector<const TabItem*> tabs = tc->GetTabs();
        IM_CHECK(target_idx < static_cast<int>(tabs.size()));
        if (target_idx >= static_cast<int>(tabs.size())) return;
        const std::string target_label = tabs[target_idx]->m_label;

        ctx->ItemClick(("//Main Window/**/" + target_label).c_str());
        ctx->Yield(3);

        IM_CHECK(TabContainerTestPeer{*tc}.ActiveTabIndex() == target_idx);
    };

    t = IM_REGISTER_TEST(e, "app", "compute_workload_auto_selected");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        ComputeView* cv = GetComputeViewOrSkip(ctx);
        if (!cv) return;
        ComputeSelection* sel = ComputeViewTestPeer{*cv}.ComputeSelectionPtr();
        IM_CHECK(sel != nullptr);
        if (sel == nullptr) return;

        // Loading a compute profile auto-selects the first workload, which
        // cascades to selecting that workload's first kernel. Both selection
        // ids must therefore be valid (not the INVALID sentinel) once loaded.
        ctx->Yield(3);
        IM_CHECK(sel->GetSelectedWorkload() != ComputeSelection::INVALID_SELECTION_ID);
        IM_CHECK(sel->GetSelectedKernel() != ComputeSelection::INVALID_SELECTION_ID);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_timeline_pan_hotkey");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;

        // Pan (A/D) shares the timeline's m_pseudo_focus gate, set by a mouse-down
        // in the graph; park on the first event to acquire it.
        ctx->Yield(3);
        ImVec2 event_center(0.0f, 0.0f);
        bool   have_center = FirstEventScreenCenter(
            ctx, TimelineViewTestPeer{*tlv}.FirstFlameWindowId(), event_center);
        IM_CHECK(have_center);
        if (!have_center) return;

        ctx->MouseMoveToPos(event_center);
        ctx->MouseDown(0);
        ctx->Yield(1);
        ctx->MouseUp(0);
        ctx->Yield(2);

        // At zoom 1 the view spans the full range, so pan is clamped with no
        // headroom; zoom in first to make room to move.
        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_W);
        ctx->KeyPress(ImGuiKey_W);
        ctx->Yield(3);

        // Pan left first so the subsequent D pan always has right-headroom
        // regardless of where the zoom centered the view.
        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_A);
        ctx->KeyPress(ImGuiKey_A);
        ctx->Yield(3);

        const double v_min_before = tlv->GetViewCoords().v_min_x;

        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_D);
        ctx->Yield(3);
        const double v_min_right = tlv->GetViewCoords().v_min_x;
        IM_CHECK(v_min_right > v_min_before);

        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_A);
        ctx->Yield(3);
        const double v_min_left = tlv->GetViewCoords().v_min_x;
        IM_CHECK(v_min_left < v_min_right);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_timeline_vscroll");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;

        ctx->Yield(3);
        ImVec2 event_center(0.0f, 0.0f);
        bool   have_center = FirstEventScreenCenter(
            ctx, TimelineViewTestPeer{*tlv}.FirstFlameWindowId(), event_center);
        IM_CHECK(have_center);
        if (!have_center) return;

        ctx->MouseMoveToPos(event_center);
        ctx->MouseDown(0);
        ctx->Yield(1);
        ctx->MouseUp(0);
        ctx->Yield(2);

        // Scroll step is a fraction of the max scroll offset, which is 0 when all
        // tracks fit the viewport; Up/Down then no-op and the assertion would
        // false-fail. Skip rather than assert on a trace with no headroom.
        if (TimelineViewTestPeer{*tlv}.MaxYScroll() <= 0.0f)
        {
            ctx->LogWarning("SKIP: all tracks fit the viewport, no vertical scroll headroom");
            return;
        }

        const double y_before = tlv->GetViewCoords().y;

        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_DownArrow);
        ctx->Yield(3);
        const double y_down = tlv->GetViewCoords().y;

        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_UpArrow);
        ctx->Yield(3);
        const double y_up = tlv->GetViewCoords().y;

        IM_CHECK(y_down > y_before);
        IM_CHECK(y_up < y_down);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_reset_view_button");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;

        ctx->Yield(3);
        ImVec2 event_center(0.0f, 0.0f);
        bool   have_center = FirstEventScreenCenter(
            ctx, TimelineViewTestPeer{*tlv}.FirstFlameWindowId(), event_center);
        IM_CHECK(have_center);
        if (!have_center) return;

        // Dirty the view via zoom alone; vertical scroll can no-op when all
        // tracks fit the viewport, so it is not a reliable way off default.
        ctx->MouseMoveToPos(event_center);
        ctx->MouseDown(0);
        ctx->Yield(1);
        ctx->MouseUp(0);
        ctx->Yield(2);
        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_W);
        ctx->KeyPress(ImGuiKey_W);
        ctx->Yield(3);

        const float zoom_dirty = tlv->GetViewCoords().z;
        IM_CHECK(zoom_dirty > 1.0f);

        // "Reset View" lives inside BeginChild("Toolbar") (no stable top-level
        // path), so reach it with a recursive wildcard search.
        ctx->SetRef("Main Window");
        ctx->ItemClick("**/Reset View");
        ctx->Yield(3);

        // Reset restores the full range (zoom ~1.0), not merely a smaller zoom;
        // pin to the default so a partial reset (e.g. still 2x) fails.
        const float  zoom_after = tlv->GetViewCoords().z;
        const double y_after    = tlv->GetViewCoords().y;
        IM_CHECK(zoom_after <= 1.0f + 0.01f);
        IM_CHECK(y_after == 0.0);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_timeline_compact_mode_toggle");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;

        ctx->Yield(3);
        FlameTrackItem* flame = TimelineViewTestPeer{*tlv}.FirstFlameTrack();
        IM_CHECK(flame != nullptr);
        if (flame == nullptr) return;

        // Compact Mode is a per-track gear option whose checkbox lives in a popup
        // with no stable widget id, so drive it through the same side-effecting
        // path the checkbox uses. Turning it on shrinks the per-event level
        // height; assert both the flag and the height follow, then restore.
        const bool  orig_compact = flame->IsCompactMode();
        const float orig_height  = FlameTrackItemTestPeer{*flame}.LevelHeight();

        // Capture observations, restore, THEN assert: IM_CHECK early-returns on
        // failure, so asserting before the restore would leak the flipped state
        // (per-track flag, shared across the process) into later tests.
        FlameTrackItemTestPeer{*flame}.SetCompactMode(!orig_compact);
        ctx->Yield(2);
        const bool  on_compact = flame->IsCompactMode();
        const float on_height  = FlameTrackItemTestPeer{*flame}.LevelHeight();

        FlameTrackItemTestPeer{*flame}.SetCompactMode(orig_compact);
        ctx->Yield(2);
        const bool  back_compact = flame->IsCompactMode();
        const float back_height  = FlameTrackItemTestPeer{*flame}.LevelHeight();

        IM_CHECK(on_compact != orig_compact);
        IM_CHECK(on_height != orig_height);
        IM_CHECK(back_compact == orig_compact);
        IM_CHECK(back_height == orig_height);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_timeline_mark_time_range");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;
        std::shared_ptr<TimelineSelection> sel = tv->GetTimelineSelection();
        IM_CHECK(sel != nullptr);
        if (sel == nullptr) return;

        // Start from no event selection and no marked range.
        TraceViewTestPeer{*tv}.ClearEventSelection();
        sel->ClearTimeRange();
        ctx->Yield(3);
        IM_CHECK(!sel->HasValidTimeRangeSelection());

        // The M (Toggle Mark) hotkey builds a time-range from the selected
        // events, so select one first. The hotkey shares the timeline's
        // pseudo-focus gate (set by a mouse-down in the graph), same as W/S/A/D.
        ImVec2 event_center(0.0f, 0.0f);
        bool   have_center = FirstEventScreenCenter(
            ctx, TimelineViewTestPeer{*tlv}.FirstFlameWindowId(), event_center);
        IM_CHECK(have_center);
        if (!have_center) return;

        ctx->MouseMoveToPos(event_center);
        ctx->MouseClick(0);
        ctx->Yield(3);
        std::vector<uint64_t> ids;
        sel->GetSelectedEvents(ids);
        IM_CHECK(ids.size() >= 1);

        // M marks the selected events' time range.
        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_M);
        ctx->Yield(3);
        IM_CHECK(sel->HasValidTimeRangeSelection());

        // M again clears the marked range (toggle).
        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_M);
        ctx->Yield(3);
        IM_CHECK(!sel->HasValidTimeRangeSelection());

        // Leave a clean selection state for following tests.
        TraceViewTestPeer{*tv}.ClearEventSelection();
        ctx->Yield(2);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_timeline_event_color_mode");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;

        ctx->Yield(3);
        FlameTrackItem* flame = TimelineViewTestPeer{*tlv}.FirstFlameTrack();
        IM_CHECK(flame != nullptr);
        if (flame == nullptr) return;

        // "Color by Name / Time Level / No Color" are gear-menu radio buttons in
        // a popup with no stable path; each sets the track's event color mode.
        // Drive that field directly and assert it changes, then restore.
        const EventTrackOptions::EventColorMode orig =
            FlameTrackItemTestPeer{*flame}.GetEventColorMode();
        const EventTrackOptions::EventColorMode other =
            (orig == EventTrackOptions::EventColorMode::kByTimeLevel)
                ? EventTrackOptions::EventColorMode::kByEventName
                : EventTrackOptions::EventColorMode::kByTimeLevel;

        // Capture, restore, THEN assert: IM_CHECK early-returns on failure, so
        // asserting before the restore would leak the changed color mode (shared
        // per-track state) into later tests in the same process.
        FlameTrackItemTestPeer{*flame}.SetEventColorMode(other);
        ctx->Yield(2);
        const EventTrackOptions::EventColorMode changed =
            FlameTrackItemTestPeer{*flame}.GetEventColorMode();

        FlameTrackItemTestPeer{*flame}.SetEventColorMode(orig);
        ctx->Yield(2);
        const EventTrackOptions::EventColorMode restored =
            FlameTrackItemTestPeer{*flame}.GetEventColorMode();

        IM_CHECK(changed == other);
        IM_CHECK(changed != orig);
        IM_CHECK(restored == orig);
    };

    t = IM_REGISTER_TEST(e, "app", "common_settings_theme_toggle");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        SettingsManager& sm = SettingsManager::GetInstance();

        // Flip the theme the same way the Preferences combo does: mutate the live
        // user settings then ApplyUserSettings(old, save_json=false). The false
        // keeps it off the on-disk settings json. The applied palette (GetColor)
        // must follow, not just the bool, so assert a theme color changes too.
        // Restore at the end so later tests / a 2nd run see the original theme.
        const bool   orig_dark = sm.GetUserSettings().display_settings.use_dark_mode;
        const ImU32  orig_bg   = sm.GetColor(Colors::kBgMain);

        // Capture, restore, THEN assert: IM_CHECK early-returns on failure, and
        // SettingsManager is a process-global singleton, so asserting before the
        // restore would leak a flipped theme into every later test and the next run.
        UserSettings before = sm.GetUserSettings();
        sm.GetUserSettings().display_settings.use_dark_mode = !orig_dark;
        sm.ApplyUserSettings(before, false);
        ctx->Yield(2);
        const bool  flipped_dark = sm.GetUserSettings().display_settings.use_dark_mode;
        const ImU32 flipped_bg   = sm.GetColor(Colors::kBgMain);

        UserSettings flipped = sm.GetUserSettings();
        sm.GetUserSettings().display_settings.use_dark_mode = orig_dark;
        sm.ApplyUserSettings(flipped, false);
        ctx->Yield(2);
        const bool  back_dark = sm.GetUserSettings().display_settings.use_dark_mode;
        const ImU32 back_bg   = sm.GetColor(Colors::kBgMain);

        IM_CHECK(flipped_dark != orig_dark);
        IM_CHECK(flipped_bg != orig_bg);
        IM_CHECK(back_dark == orig_dark);
        IM_CHECK(back_bg == orig_bg);
    };

    t = IM_REGISTER_TEST(e, "app", "common_settings_time_unit_change");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        SettingsManager& sm = SettingsManager::GetInstance();

        // Change the timeline time unit via the same path the Units combo uses,
        // then restore the original so later tests / a 2nd run see the default.
        const TimeFormat orig = sm.GetUserSettings().unit_settings.time_format;
        const TimeFormat other =
            (orig == TimeFormat::kNanoseconds) ? TimeFormat::kSeconds : TimeFormat::kNanoseconds;

        // Capture, restore, THEN assert: IM_CHECK early-returns on failure, and
        // SettingsManager is a process-global singleton, so asserting before the
        // restore would leak the changed time unit into later tests / the next run.
        UserSettings before = sm.GetUserSettings();
        sm.GetUserSettings().unit_settings.time_format = other;
        sm.ApplyUserSettings(before, false);
        ctx->Yield(2);
        const TimeFormat changed_fmt = sm.GetUserSettings().unit_settings.time_format;

        UserSettings changed = sm.GetUserSettings();
        sm.GetUserSettings().unit_settings.time_format = orig;
        sm.ApplyUserSettings(changed, false);
        ctx->Yield(2);
        const TimeFormat back_fmt = sm.GetUserSettings().unit_settings.time_format;

        IM_CHECK(changed_fmt == other);
        IM_CHECK(back_fmt == orig);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_histogram_normalization_toggle");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineModel& tl = tv->GetDataProvider()->DataModel().GetTimeline();

        // The All Tracks / Visible Tracks normalization toggle lives in the
        // histogram right-click popup (no stable path), so drive it through the
        // same model calls the menu items make. Assert the flag flips, then
        // restore the original so later tests / a 2nd run see the default.
        const bool orig_global = tl.IsNormalizeGlobal();

        // Capture, restore, THEN assert: IM_CHECK early-returns on failure, and the
        // TimelineModel is shared across every timeline test in the process, so
        // asserting before the restore would leak the flipped mode into later tests.
        tl.ToggleNormalization();
        tl.UpdateHistogram({});
        ctx->Yield(2);
        const bool toggled_global = tl.IsNormalizeGlobal();

        tl.ToggleNormalization();
        tl.UpdateHistogram({});
        ctx->Yield(2);
        const bool back_global = tl.IsNormalizeGlobal();

        IM_CHECK(toggled_global != orig_global);
        IM_CHECK(back_global == orig_global);
    };

    t = IM_REGISTER_TEST(e, "app", "compute_kernel_select_changes");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        ComputeView* cv = GetComputeViewOrSkip(ctx);
        if (!cv) return;
        ComputeSelection* sel = ComputeViewTestPeer{*cv}.ComputeSelectionPtr();
        IM_CHECK(sel != nullptr);
        if (sel == nullptr) return;

        ctx->Yield(3);
        const uint32_t workload = sel->GetSelectedWorkload();
        IM_CHECK(workload != ComputeSelection::INVALID_SELECTION_ID);
        const uint32_t auto_kernel = sel->GetSelectedKernel();
        IM_CHECK(auto_kernel != ComputeSelection::INVALID_SELECTION_ID);

        std::vector<const KernelInfo*> kernels =
            cv->GetDataProvider()->ComputeModel().GetKernelInfoList(workload);
        if (kernels.size() < 2)
        {
            ctx->LogWarning("SKIP: workload has fewer than two kernels to switch between");
            return;
        }

        uint32_t other = ComputeSelection::INVALID_SELECTION_ID;
        for (const KernelInfo* k : kernels)
        {
            if (k != nullptr && k->id != auto_kernel) { other = k->id; break; }
        }
        IM_CHECK(other != ComputeSelection::INVALID_SELECTION_ID);

        // Capture, restore, THEN assert: IM_CHECK early-returns on failure, and
        // ComputeSelection is shared across compute tests, so asserting before the
        // restore would leak the non-auto kernel into later tests / the next run.
        sel->SelectKernel(other);
        ctx->Yield(3);
        const uint32_t selected = sel->GetSelectedKernel();

        sel->SelectKernel(auto_kernel);
        ctx->Yield(2);

        IM_CHECK(selected == other);
        IM_CHECK(selected != auto_kernel);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_event_search_finds_results");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        EventSearch* es = TraceViewTestPeer{*tv}.EventSearchPtr();
        IM_CHECK(es != nullptr);
        if (es == nullptr) return;

        // Clear so the searched flag starts from a known baseline (the harness
        // reuses one process interactively).
        es->Clear();
        ctx->Yield(2);
        IM_CHECK(es->Searched() == false);

        // hipLaunchKernel is a launch region present in the trace; write it into
        // the production search buffer and run the search the same way the input
        // field's submit does.
        char* buf = es->TextInput();
        IM_CHECK(buf != nullptr);
        if (buf == nullptr) return;
        snprintf(buf, es->TextInputLimit(), "%s", "hipLaunchKernel");
        es->Search();
        ctx->Yield(2);
        IM_CHECK(es->Searched() == true);

        // The fetch is deferred; let it drain (Update re-runs Search when the
        // request completes) before reading the result count.
        for (int i = 0; i < 60 && EventSearchTestPeer{*es}.RequestPending(); i++) ctx->Yield(2);
        ctx->Yield(5);
        IM_CHECK(EventSearchTestPeer{*es}.ResultCount() > 0);

        es->Clear();
        ctx->Yield(2);
    };

    // Type a filter predicate into the Event Table's real text box, click its real
    // Submit, and check that the row count both drops and matches an independent SQL
    // count of the same predicate. Unlike the other sys_ tests, which poke the model
    // directly, this one drives the actual ImGui widgets and checks the numbers shown.
    t = IM_REGISTER_TEST(e, "app", "sys_event_table_sql_filter_restricts_rows");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        std::shared_ptr<TimelineSelection> sel = tv->GetTimelineSelection();
        IM_CHECK(sel != nullptr);
        if (sel == nullptr) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;
        DataProvider* dp = tv->GetDataProvider();
        IM_CHECK(dp != nullptr);
        if (dp == nullptr) return;

        // The Event Table lives in the Advanced Details panel, and its widgets only
        // register with the Test Engine while that panel is visible. Force it open (it
        // is on by default, but a saved layout could have hidden it) and restore the
        // previous state at the end.
        const bool details_prev =
            SettingsManager::GetInstance().GetAppWindowSettings().show_details_panel;
        tv->SetAnalysisViewVisibility(true);
        ctx->Yield(3);

        // Clear the time-range selection first. The set of rows fetched follows the
        // selection, so clearing it brings every row into scope. The Event Table's
        // duration is the full (end - start), which is what the SQL predicate below
        // compares against.
        sel->ClearTimeRange();
        sel->UnselectAllTracks();
        ctx->Yield(3);

        // Wait for the timeline's flame (event) tracks to load.
        std::vector<FlameTrackItem*> flames;
        for (int i = 0; i < 120 && flames.empty(); i++)
        {
            flames = TimelineViewTestPeer{*tlv}.DisplayedFlameTracks();
            if (flames.empty()) ctx->Yield(2);
        }
        if (flames.empty())
        {
            ctx->LogWarning("SKIP: no flame tracks loaded to populate the Event Table");
            tv->SetAnalysisViewVisibility(details_prev);
            return;
        }

        // Select only the HIP-region tracks (their one operation is Launch). Rows for
        // such a track come from a single source, the non-sample rocpd_region rows
        // (SAMPLE.id IS NULL), which SQL can reproduce exactly. Skip GPU dispatch and
        // stream tracks: their events are all over 1ms, so `duration > 2000` would
        // match every row and the filter would not visibly shrink the set.
        std::vector<std::pair<uint64_t, uint64_t>> region_tracks;  // (pid, tid)
        const TimelineModel& tlm = dp->DataModel().GetTimeline();
        for (FlameTrackItem* flame : flames)
        {
            if (flame == nullptr) continue;
            const TrackInfo* ti = tlm.GetTrack(flame->GetID());
            if (ti == nullptr) continue;
            if (ti->operation_types.size() == 1 &&
                ti->operation_types.count(kRocProfVisDmOperationLaunch) == 1)
            {
                sel->SelectTrack(*flame);
                region_tracks.emplace_back(ti->agent_or_pid, ti->queue_id_or_tid);
            }
        }
        if (region_tracks.empty())
        {
            ctx->LogWarning("SKIP: no HIP region track to exercise the SQL filter");
            tv->SetAnalysisViewVisibility(details_prev);
            return;
        }

        // Drain the async fetch triggered by the track selection before reading the
        // row count.
        ctx->Yield(3);
        for (int i = 0; i < 120 &&
                        dp->IsRequestPending(DataProvider::EVENT_TABLE_REQUEST_ID);
             i++)
            ctx->Yield(2);
        ctx->Yield(5);

        const uint64_t unfiltered =
            dp->DataModel().GetTables().GetTableTotalRowCount(TableType::kEventTable);
        IM_CHECK(unfiltered > 0);
        if (unfiltered == 0)
        {
            sel->UnselectAllTracks();
            tv->SetAnalysisViewVisibility(details_prev);
            return;
        }

        // Independent oracle: a separate read-only sqlite handle counts the same rows
        // the Event Table shows, the non-sample rocpd_region rows. Duration there is
        // (end - start) ns, so the UI's `duration > 2000` is `(R.end - R.start) > 2000`
        // in SQL, summed over the selected (pid, tid) tracks.
        auto sql_count = [&](bool apply_predicate) -> long long {
            sqlite3* db = nullptr;
            if (sqlite3_open_v2(dp->GetTraceFilePath().c_str(), &db,
                                SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK)
            {
                if (db) sqlite3_close(db);
                return -1;
            }
            long long total = 0;
            bool      ok    = true;
            for (const std::pair<uint64_t, uint64_t>& pt : region_tracks)
            {
                std::string q =
                    "SELECT COUNT(*) FROM rocpd_region R "
                    "LEFT JOIN rocpd_sample SAMPLE ON SAMPLE.event_id = R.event_id "
                    "WHERE SAMPLE.id IS NULL AND R.pid=" + std::to_string(pt.first) +
                    " AND R.tid=" + std::to_string(pt.second);
                if (apply_predicate) q += " AND (R.end - R.start) > 2000";
                sqlite3_stmt* stmt = nullptr;
                if (sqlite3_prepare_v2(db, q.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
                {
                    ok = false;
                    break;
                }
                if (sqlite3_step(stmt) == SQLITE_ROW)
                    total += sqlite3_column_int64(stmt, 0);
                else
                    ok = false;
                sqlite3_finalize(stmt);
                if (!ok) break;
            }
            sqlite3_close(db);
            return ok ? total : -1;
        };

        const long long sql_total = sql_count(false);
        IM_CHECK(sql_total >= 0);
        // The selected rows must match the SQL source before filtering, or the
        // filtered-count check below would be comparing two unrelated sets of rows.
        IM_CHECK(static_cast<uint64_t>(sql_total) == unfiltered);

        // The Event Table is the analysis view's default tab. Click it to be sure it
        // is the active one, then check the filter widgets resolve before driving them
        // rather than relying on a wildcard match.
        const char* kTabRef    = "//Main Window/**/Event Table";
        const char* kFilterRef = "//Main Window/**/filters/##input_text_with_clear";
        const char* kSubmitRef = "//Main Window/**/Submit";
        if (ctx->ItemExists(kTabRef)) ctx->ItemClick(kTabRef);
        ctx->Yield(2);
        IM_CHECK(ctx->ItemExists(kFilterRef));
        IM_CHECK(ctx->ItemExists(kSubmitRef));

        // Type the predicate into the text box and click Submit, driving the real
        // widgets rather than poking the model directly.
        ctx->ItemInput(kFilterRef);
        ctx->KeyCharsReplace("duration > 2000");
        ctx->ItemClick(kSubmitRef);

        // Submit queues an async refetch that applies the predicate in memory over the
        // merged rows. Drain it before reading the filtered count.
        ctx->Yield(3);
        for (int i = 0; i < 120 &&
                        dp->IsRequestPending(DataProvider::EVENT_TABLE_REQUEST_ID);
             i++)
            ctx->Yield(2);
        ctx->Yield(5);

        const uint64_t filtered =
            dp->DataModel().GetTables().GetTableTotalRowCount(TableType::kEventTable);
        const long long sql_filtered = sql_count(true);
        IM_CHECK(sql_filtered >= 0);

        ctx->LogInfo("event table filter: unfiltered=%llu filtered=%llu sql_filtered=%lld",
                     (unsigned long long) unfiltered, (unsigned long long) filtered,
                     sql_filtered);

        // Restore before the asserts. Clear the filter and drop the selection now, so
        // that if an IM_CHECK below fails and returns, it cannot leave a filtered
        // Event Table behind for later tests in this reused process.
        ctx->ItemInput(kFilterRef);
        ctx->KeyCharsReplace("");
        ctx->ItemClick(kSubmitRef);
        ctx->Yield(3);
        for (int i = 0; i < 120 &&
                        dp->IsRequestPending(DataProvider::EVENT_TABLE_REQUEST_ID);
             i++)
            ctx->Yield(2);
        sel->UnselectAllTracks();
        ctx->Yield(2);
        tv->SetAnalysisViewVisibility(details_prev);

        // Two things must hold: the filter shrank the displayed rows, and the result
        // matches the independent SQL count of the same predicate on the same source.
        IM_CHECK(filtered < unfiltered);
        IM_CHECK(static_cast<long long>(filtered) == sql_filtered);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_summary_pie_kernel_select");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        SummaryView* sv = TraceViewTestPeer{*tv}.SummaryViewPtr();
        IM_CHECK(sv != nullptr);
        if (sv == nullptr) return;
        TopKernels* tk = SummaryViewTestPeer{*sv}.TopKernelsPtr();
        IM_CHECK(tk != nullptr);
        if (tk == nullptr) return;

        // FetchSummary + TopKernels::Update only run while the Summary window is
        // shown; headless with no saved layout it may be closed, leaving the kernel
        // list null forever. Force it open ourselves rather than depending on another
        // test having flipped this shared setting earlier in the process.
        ShowSummaryGuard summary_guard;
        SettingsManager::GetInstance().GetAppWindowSettings().show_summary = true;

        // Summary data loads asynchronously; let Update() populate the kernel list.
        // KernelCount() is 0 while m_kernels is still null.
        TopKernelsTestPeer peer{*tk};
        for (int i = 0; i < 60 && peer.KernelCount() == 0; i++) ctx->Yield(2);

        // Pick a real kernel index, skipping the synthetic "Others" bucket (which
        // ToggleSelectKernel treats as a deselect). Bail if there's nothing else.
        size_t target = 0;
        if (peer.PaddedIdx() == target) target = 1;
        if (peer.KernelCount() <= target)
        {
            ctx->LogWarning("SKIP: summary has no selectable top kernel for this trace");
            return;
        }

        // This drives the model (ToggleSelectKernel) and asserts selection state.
        // The pie is ImPlot-canvas drawn with no widget ID, so the click-to-index
        // hit-test is out of scope here; only the selection wiring is covered.

        // Clear to a known baseline (the harness reuses one process interactively).
        peer.ClearSelection();
        ctx->Yield(2);
        IM_CHECK(peer.SelectedIdx() == std::nullopt);

        peer.Select(target);
        ctx->Yield(2);
        IM_CHECK(peer.SelectedIdx().has_value());
        IM_CHECK(peer.SelectedIdx().value() == target);

        peer.ClearSelection();
        ctx->Yield(2);
        IM_CHECK(peer.SelectedIdx() == std::nullopt);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_summary_top_kernel_name");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        SummaryView* sv = TraceViewTestPeer{*tv}.SummaryViewPtr();
        IM_CHECK(sv != nullptr);
        if (sv == nullptr) return;
        TopKernels* tk = SummaryViewTestPeer{*sv}.TopKernelsPtr();
        IM_CHECK(tk != nullptr);
        if (tk == nullptr) return;

        // The Summary fetch (FetchSummary) and TopKernels::Update both run only when
        // the Summary window is shown; headless with no saved layout it may be closed,
        // leaving the kernel list null forever. Force it open before draining the load.
        ShowSummaryGuard summary_guard;
        SettingsManager::GetInstance().GetAppWindowSettings().show_summary = true;

        // Summary data loads asynchronously; let Update() populate the kernel list.
        // KernelCount() is 0 while m_kernels is still null.
        TopKernelsTestPeer peer{*tk};
        for (int i = 0; i < 60 && peer.KernelCount() == 0; i++) ctx->Yield(2);

        IM_CHECK(peer.KernelCount() > 0);
        IM_CHECK(!peer.KernelName(0).empty());
        for (size_t i=1; i < peer.KernelCount();i++){
            IM_CHECK(peer.ExecTimeSum(0) >= peer.ExecTimeSum(i));
        }

    };

    t = IM_REGISTER_TEST(e, "app", "sys_summary_display_mode_switch");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        SummaryView* sv = TraceViewTestPeer{*tv}.SummaryViewPtr();
        IM_CHECK(sv != nullptr);
        if (sv == nullptr) return;
        TopKernels* tk = SummaryViewTestPeer{*sv}.TopKernelsPtr();
        IM_CHECK(tk != nullptr);
        if (tk == nullptr) return;

        // The Pie/Bar/Table switcher renders only once the Summary window is
        // shown and its kernel list has loaded; force it open and drain the
        // async fetch first (mirrors sys_summary_top_kernel_name).
        ShowSummaryGuard summary_guard;
        SettingsManager::GetInstance().GetAppWindowSettings().show_summary = true;
        TopKernelsTestPeer peer{*tk};
        for (int i = 0; i < 60 && peer.KernelCount() == 0; i++) ctx->Yield(2);
        IM_CHECK(peer.KernelCount() > 0);

        // Reaching this point means the load path ran (not the skip), so the
        // mode assertions below genuinely exercise the switcher. Default is Pie.
        ctx->Yield(2);
        IM_CHECK(peer.IsDisplayPie());

        // The switcher is three IconButtons in the "Summary" window. IconButton
        // does PushID(glyph)+Button(glyph), so each button's ref ends in the icon
        // glyph twice (rocprovfis_icon_defines.h). The "**/" hops the dynamic
        // layout ids between the window and the button (clamped_view/RightColumn/
        // TopRow hashes that vary per build). Poll the mode after each click rather
        // than a fixed yield: the click settles over a variable number of frames.
        std::string bar_ref = std::string("//Summary/**/") + ICON_CHART_BAR + "/" + ICON_CHART_BAR;
        ctx->ItemClick(bar_ref.c_str());
        for (int i = 0; i < 30 && !peer.IsDisplayBar(); i++) ctx->Yield(2);
        IM_CHECK(peer.IsDisplayBar());

        std::string table_ref = std::string("//Summary/**/") + ICON_LIST + "/" + ICON_LIST;
        ctx->ItemClick(table_ref.c_str());
        for (int i = 0; i < 30 && !peer.IsDisplayTable(); i++) ctx->Yield(2);
        IM_CHECK(peer.IsDisplayTable());

        std::string pie_ref = std::string("//Summary/**/") + ICON_CHART_PIE + "/" + ICON_CHART_PIE;
        ctx->ItemClick(pie_ref.c_str());
        for (int i = 0; i < 30 && !peer.IsDisplayPie(); i++) ctx->Yield(2);
        IM_CHECK(peer.IsDisplayPie());
    };

    // The total duration across the Summary's top-kernels list should equal an
    // independent SQL SUM over every dispatch. The list is the top-N kernels plus an
    // "Others" row that holds the leftover duration of all the rest (PadTopKernels
    // fills it with total minus the top-N sum). So adding up the whole list gives the
    // grand total over every dispatch, which is what the full-range SQL SUM measures.
    t = IM_REGISTER_TEST(e, "app", "sys_summary_kernel_duration_total_matches_db");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        SummaryView* sv = TraceViewTestPeer{*tv}.SummaryViewPtr();
        IM_CHECK(sv != nullptr);
        if (sv == nullptr) return;
        TopKernels* tk = SummaryViewTestPeer{*sv}.TopKernelsPtr();
        IM_CHECK(tk != nullptr);
        if (tk == nullptr) return;

        // FetchTopKernels + TopKernels::Update only run while the Summary window is
        // shown. Force it open and drain the async fetch.
        ShowSummaryGuard summary_guard;
        SettingsManager::GetInstance().GetAppWindowSettings().show_summary = true;
        TopKernelsTestPeer peer{*tk};
        for (int i = 0; i < 60 && peer.KernelCount() == 0; i++) ctx->Yield(2);
        IM_CHECK(peer.KernelCount() > 0);
        if (peer.KernelCount() == 0) return;

        // Optiq's grand total is the sum over every displayed entry (top-N plus "Others").
        double optiq_total = 0.0;
        for (size_t i = 0; i < peer.KernelCount(); i++) optiq_total += peer.ExecTimeSum(i);

        // The independent SQL runs on the db this process opened. Optiq trims each
        // row's duration to the visible time range, which only equals the full
        // (end - start) when the whole trace is in view. So this query uses no time
        // filter, otherwise the two sides would be measuring different spans.
        Project* project = AppWindow::GetInstance()->GetCurrentProject();
        IM_CHECK(project != nullptr);
        if (project == nullptr) return;
        const std::string db_path = project->GetID();

        const char* sql =
            DbHasTimestampTable(db_path)
                ? "SELECT SUM(TE.value - TS.value) FROM rocpd_kernel_dispatch K "
                  "INNER JOIN rocpd_timestamp TS ON TS.id = K.start_id "
                  "INNER JOIN rocpd_timestamp TE ON TE.id = K.end_id"
                : "SELECT SUM(K.end - K.start) FROM rocpd_kernel_dispatch K";
        double sql_total = 0.0;
        const bool have_sql = DbScalarQuery(db_path, sql, sql_total);
        IM_CHECK(have_sql);
        if (!have_sql) return;

        // Compare with a relative tolerance, not exact equality. Both sides add large
        // ns values into doubles, which carries a little floating-point round-off,
        // while a genuine error would be off by whole kernels (millions of ns).
        const double denom = std::max({ std::fabs(optiq_total), std::fabs(sql_total), 1.0 });
        IM_CHECK(std::fabs(optiq_total - sql_total) / denom < 1e-6);
    };

    // The invocation counts shown in the Summary should add up to COUNT(*) over every
    // dispatch. Caveat: when more kernels exist than the Summary lists, it appends an
    // "Others" row that reports 0 invocations, so the visible counts only reach the
    // true total when that row is absent. When it is present the rows are just the
    // top-N, so skip rather than compare a partial sum to the full count.
    t = IM_REGISTER_TEST(e, "app", "sys_summary_kernel_count_matches_db");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        SummaryView* sv = TraceViewTestPeer{*tv}.SummaryViewPtr();
        IM_CHECK(sv != nullptr);
        if (sv == nullptr) return;
        TopKernels* tk = SummaryViewTestPeer{*sv}.TopKernelsPtr();
        IM_CHECK(tk != nullptr);
        if (tk == nullptr) return;

        ShowSummaryGuard summary_guard;
        SettingsManager::GetInstance().GetAppWindowSettings().show_summary = true;
        TopKernelsTestPeer peer{*tk};
        for (int i = 0; i < 60 && peer.KernelCount() == 0; i++) ctx->Yield(2);
        IM_CHECK(peer.KernelCount() > 0);
        if (peer.KernelCount() == 0) return;

        if (peer.PaddedIdx().has_value())
        {
            ctx->LogWarning("SKIP: top-N truncated (Others bucket present); dispatch count "
                            "is not a grand total through the displayed entries");
            return;
        }

        // No "Others" row here, so every kernel is shown and the invocation counts
        // add up to the full dispatch count.
        uint64_t optiq_count = 0;
        for (size_t i = 0; i < peer.KernelCount(); i++) optiq_count += peer.Invocations(i);

        Project* project = AppWindow::GetInstance()->GetCurrentProject();
        IM_CHECK(project != nullptr);
        if (project == nullptr) return;
        const std::string db_path = project->GetID();

        // Counting rows needs no timestamp join, so unlike the duration query this one
        // is the same on every schema version.
        double sql_count_d = 0.0;
        const bool have_sql =
            DbScalarQuery(db_path, "SELECT COUNT(*) FROM rocpd_kernel_dispatch", sql_count_d);
        IM_CHECK(have_sql);
        if (!have_sql) return;

        // Both sides are exact integers, so assert exact equality.
        IM_CHECK(optiq_count == static_cast<uint64_t>(sql_count_d));
    };

    t = IM_REGISTER_TEST(e, "app", "compute_kernel_table_loads_sorted");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        ComputeView* cv = GetComputeViewOrSkip(ctx);
        if (!cv) return;
        TabContainer* tc = ComputeViewTestPeer{*cv}.TabContainerPtr();
        IM_CHECK(tc != nullptr);
        if (tc == nullptr) return;

        // The kernel metric table renders only while the "Kernel Details" tab is
        // active (TabContainer renders just the active tab's content).
        tc->SetActiveTab("compute_kernel_details_view");
        ctx->Yield(3);
        const TabItem* tab = tc->GetActiveTab();
        IM_CHECK(tab != nullptr);
        if (tab == nullptr) return;
        ComputeKernelDetailsView* kd =
            dynamic_cast<ComputeKernelDetailsView*>(tab->m_widget.get());
        IM_CHECK(kd != nullptr);
        if (kd == nullptr) return;
        KernelMetricTable* kt = ComputeKernelDetailsViewTestPeer{*kd}.KernelMetricTablePtr();
        IM_CHECK(kt != nullptr);
        if (kt == nullptr) return;

        // The table fetches its rows in response to a workload-selection event.
        // The initial auto-select (ComputeView::CreateView) fires before this view
        // subscribes, so the table starts empty; drive the fetch ourselves for the
        // already-selected workload.
        ComputeSelection* sel = ComputeViewTestPeer{*cv}.ComputeSelectionPtr();
        IM_CHECK(sel != nullptr);
        if (sel == nullptr) return;
        const uint32_t workload = sel->GetSelectedWorkload();
        IM_CHECK(workload != ComputeSelection::INVALID_SELECTION_ID);
        kt->FetchData(workload);

        // Wait for the rows to arrive: the table registers its own ImGui window
        // (name contains "kernel_selection_table") only once BeginTable runs, which
        // requires non-empty data. Its presence proves the table rendered populated.
        ImGuiWindow* table_win = nullptr;
        for (int i = 0; i < 120 && table_win == nullptr; i++)
        {
            ctx->Yield(2);
            for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows)
                if (strstr(w->Name, "kernel_selection_table")) { table_win = w; break; }
        }
        IM_CHECK(table_win != nullptr);

        // On load the table sorts by the Duration column (index 2), descending
        // (KernelMetricTable ctor: DURATION_COLUMN_INDEX + kRPVControllerSortOrderDescending).
        KernelMetricTableTestPeer peer{*kt};
        IM_CHECK(peer.SortColumnIndex() == 2);

        // Prove the rows actually loaded in that order rather than just trusting the
        // ctor default (which would pass even on an empty/unsorted fetch): the model's
        // Duration cells are plain ns integers, so assert the column is monotonically
        // non-increasing. Header col 2 is "Duration (ns)".
        const std::vector<std::vector<std::string>>& rows =
            cv->GetDataProvider()->ComputeModel().GetKernelSelectionTable().GetTableData();
        if (rows.size() < 2)
        {
            ctx->LogWarning("SKIP: fewer than two kernel rows to verify sort order");
            return;
        }
        for (size_t r = 1; r < rows.size(); r++)
        {
            IM_CHECK(rows[r - 1].size() > 2 && rows[r].size() > 2);
            if (rows[r - 1].size() <= 2 || rows[r].size() <= 2) return;
            const double prev = std::strtod(rows[r - 1][2].c_str(), nullptr);
            const double cur  = std::strtod(rows[r][2].c_str(), nullptr);
            IM_CHECK(prev >= cur);
        }
    };

    t = IM_REGISTER_TEST(e, "app", "sys_timeline_select_named_track_event");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;
        std::shared_ptr<TimelineSelection> sel = tv->GetTimelineSelection();
        IM_CHECK(sel != nullptr);
        if (sel == nullptr) return;

        // Event chart items populate after the track's data fetch drains, so poll
        // for a flame track that has at least one event before reaching in.
        FlameTrackItem* flame    = nullptr;
        uint64_t        track_id = 0;
        for (int i = 0; i < 60 && flame == nullptr; i++)
        {
            for (FlameTrackItem* candidate :
                 TimelineViewTestPeer{*tlv}.DisplayedFlameTracks())
            {
                if (FlameTrackItemTestPeer{*candidate}.ChartItemCount() > 0)
                {
                    flame    = candidate;
                    track_id = candidate->GetID();
                    break;
                }
            }
            if (flame == nullptr) ctx->Yield(2);
        }
        if (flame == nullptr)
        {
            ctx->LogWarning("SKIP: no flame track has events to select by identity");
            return;
        }

        // Target the earliest event by timestamp (chart-item order is not stable,
        // so identity comes from the (name, start_ts) pair, not an index).
        uint64_t    uuid     = 0;
        std::string name;
        double      start_ts = 0.0;
        bool        have_event =
            FlameTrackItemTestPeer{*flame}.EarliestEvent(uuid, name, start_ts);
        IM_CHECK(have_event);
        if (!have_event) return;

        // Selection is dispatched through EventManager, so clear then yield before
        // asserting the empty baseline.
        sel->UnselectAllEvents();
        ctx->Yield(3);
        std::vector<uint64_t> ids;
        sel->GetSelectedEvents(ids);
        IM_CHECK(ids.empty());

        // Select that exact event by its (track, uuid) identity, the same call the
        // flame track makes on a bar click.
        sel->SelectTrackEvent(track_id, uuid);
        ctx->Yield(3);
        IM_CHECK(sel->EventSelected(uuid));
        ids.clear();
        sel->GetSelectedEvents(ids);
        IM_CHECK(ids.size() == 1 && ids[0] == uuid);

        // Leave a clean selection for following tests.
        sel->UnselectAllEvents();
        ctx->Yield(2);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_timeline_measure_tool");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;
        MeasurementController* mc = TraceViewTestPeer{*tv}.MeasurementControllerPtr();
        IM_CHECK(mc != nullptr);
        if (mc == nullptr) return;

        // Two distinct timestamps inside the trace's visible range; the measured
        // span between them must be > 0.
        const ViewCoords coords = tlv->GetViewCoords();
        const double     span   = coords.v_max_x - coords.v_min_x;
        IM_CHECK(span > 0.0);
        if (span <= 0.0) return;
        const double t0 = coords.v_min_x + span * 0.25;
        const double t1 = coords.v_min_x + span * 0.75;

        // Clear to a known baseline: measurement state persists on the TraceView
        // across tests in the reused process.
        mc->ExitMeasurementMode();
        mc->ClearMeasurement();
        ctx->Yield(2);
        IM_CHECK(mc->IsMeasurementMode() == false);

        // Enter mode and place both points via the same controller calls the menu
        // item and freehand click handler drive.
        mc->EnterMeasurementMode();
        mc->SetFreehandMeasurementPoint(t0);
        mc->SetFreehandMeasurementPoint(t1);
        ctx->Yield(2);

        // Capture observation, restore, THEN assert: IM_CHECK early-returns on
        // failure, so leaving measurement mode active would leak into later tests.
        const MeasurementState state = mc->GetMeasurementState();
        const double duration =
            std::fabs(mc->GetEffectiveTimestamp(1) - mc->GetEffectiveTimestamp(0));

        mc->ExitMeasurementMode();
        mc->ClearMeasurement();
        ctx->Yield(2);
        const bool inactive_after = (mc->IsMeasurementMode() == false);

        IM_CHECK(state == MeasurementState::kComplete);
        IM_CHECK(duration > 0.0);
        IM_CHECK(inactive_after);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_timeline_track_expand_collapse");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;

        ctx->Yield(3);

        // Expanding only changes height when the track has enough levels for its
        // expanded height to exceed the default; find a flame track for which that
        // holds so the height assertion is meaningful.
        FlameTrackItem* flame = nullptr;
        for (FlameTrackItem* candidate : TimelineViewTestPeer{*tlv}.DisplayedFlameTracks())
        {
            FlameTrackItemTestPeer peer{*candidate};
            const float level_h = peer.LevelHeight();
            if (level_h > 0.0f &&
                peer.ExpandedTrackHeight() > peer.DefaultTrackHeight())
            {
                flame = candidate;
                break;
            }
        }
        if (flame == nullptr)
        {
            ctx->LogWarning("SKIP: no flame track deep enough for expand to change height");
            return;
        }

        FlameTrackItemTestPeer peer{*flame};
        const bool  orig_expanded = peer.IsExpanded();
        const float orig_height   = flame->GetTrackHeight();

        // The expand/collapse arrow sits in a meta area with no stable ref, so
        // drive the same side effect the button triggers. Force a collapsed
        // baseline, then expand, capturing both states.
        peer.SetExpanded(false);
        ctx->Yield(2);
        const bool  collapsed_state  = peer.IsExpanded();
        const float collapsed_height = flame->GetTrackHeight();

        peer.SetExpanded(true);
        ctx->Yield(2);
        const bool  expanded_state  = peer.IsExpanded();
        const float expanded_height = flame->GetTrackHeight();

        // Restore original state + exact height so later tests see the same layout.
        peer.SetExpanded(orig_expanded);
        peer.SetTrackHeight(orig_height);
        ctx->Yield(2);
        const bool  restored_state  = peer.IsExpanded();
        const float restored_height = flame->GetTrackHeight();

        IM_CHECK(collapsed_state == false);
        IM_CHECK(expanded_state == true);
        IM_CHECK(expanded_height != collapsed_height);
        IM_CHECK(restored_state == orig_expanded);
        IM_CHECK(restored_height == orig_height);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_stacked_event_select_distinct");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;
        std::shared_ptr<TimelineSelection> sel = tv->GetTimelineSelection();
        IM_CHECK(sel != nullptr);
        if (sel == nullptr) return;

        // Hide the Summary window so it doesn't sit over the timeline and catch
        // the bar clicks below.
        ShowSummaryGuard summary_guard;
        SettingsManager::GetInstance().GetAppWindowSettings().show_summary = false;
        ctx->Yield(2);

        // Start from an empty selection. Selection is async, so yield first.
        TraceViewTestPeer{*tv}.ClearEventSelection();
        ctx->Yield(3);
        std::vector<uint64_t> ids;
        sel->GetSelectedEvents(ids);
        IM_CHECK(ids.empty());

        // Look for a stacked, overlapping pair of bars to click. Scan every
        // flame track, since the first one may not contain such a pair.
        ctx->Yield(3);
        ImVec2 a(0.0f, 0.0f), b(0.0f, 0.0f);
        bool   have_targets = false;
        for (FlameTrackItem* flame : TimelineViewTestPeer{*tlv}.DisplayedFlameTracks())
        {
            if (flame == nullptr) continue;
            const unsigned int flame_window_id = FlameTrackItemTestPeer{*flame}.FlameWindowId();
            if (TwoStackedEventScreenCenters(ctx, flame_window_id, a, b))
            {
                have_targets = true;
                break;
            }
        }
        if (!have_targets)
        {
            ctx->LogWarning("SKIP: no lane with overlapping stacked events to disambiguate");
            return;
        }

        // Click the first bar and wait for the selection to settle on one event.
        ctx->MouseMoveToPos(a);
        ctx->MouseClick(0);
        for (int i = 0; i < 60 && ids.size() != 1; i++)
        {
            ctx->Yield(2);
            ids.clear();
            sel->GetSelectedEvents(ids);
        }
        IM_CHECK(ids.size() == 1);
        if (ids.size() != 1) return;
        const uint64_t id_a = ids.front();

        // Click the second bar. A plain click replaces the selection, so wait
        // for it to land on a single event other than the first. The loop is
        // bounded so that if the selection never changes off the first event,
        // it falls through to the assertion below instead of spinning forever.
        ctx->MouseMoveToPos(b);
        ctx->MouseClick(0);
        ids.clear();
        sel->GetSelectedEvents(ids);
        for (int i = 0; i < 60 && (ids.size() != 1 || ids.front() == id_a); i++)
        {
            ctx->Yield(2);
            ids.clear();
            sel->GetSelectedEvents(ids);
        }
        IM_CHECK(ids.size() == 1);
        if (ids.size() != 1) return;
        const uint64_t id_b = ids.front();

        // Clicking two different stacked bars must select two different events.
        IM_CHECK(id_a != id_b);

        // Leave a clean selection for following tests.
        TraceViewTestPeer{*tv}.ClearEventSelection();
        ctx->Yield(2);
        ids.clear();
        sel->GetSelectedEvents(ids);
        IM_CHECK(ids.empty());
    };

    t = IM_REGISTER_TEST(e, "app", "sys_shared_db_open_dedups_and_switches");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        namespace fs = std::filesystem;

        AppWindow* app = AppWindow::GetInstance();
        IM_CHECK(app != nullptr);
        if (app == nullptr) return;

        // Sample dbs resolve relative to the working directory (the repo root).
        // Skip if either is missing.
        auto resolve_sample = [](const char* rel) -> std::string {
            fs::path p(rel);
            if (!fs::exists(p)) return std::string();
            return fs::weakly_canonical(p).string();
        };
        const std::string db_a = resolve_sample("sample/rocpd-transpose.db");
        const std::string db_b = resolve_sample("sample/rocprof_compute_23ed6f36.db");
        if (db_a.empty() || db_b.empty())
        {
            ctx->LogWarning("SKIP: sample dbs (rocpd-transpose.db / rocprof_compute_23ed6f36.db) not found");
            return;
        }

        // A project's id is its source-db path, so a .db and a .rpv that point
        // at that same .db share one id.
        const std::string id_a = db_a;
        const std::string id_b = db_b;

        // Open DB_A; afterward its project must exist.
        app->OpenFile(db_a);
        ctx->Yield(3);
        IM_CHECK(app->GetProject(id_a) != nullptr);

        // Open DB_B as a second, active tab so the later switch back to DB_A is
        // actually observable.
        app->OpenFile(db_b);
        ctx->Yield(3);
        IM_CHECK(app->GetProject(id_b) != nullptr);
        IM_CHECK(app->GetCurrentProject() != nullptr);
        if (app->GetCurrentProject() == nullptr) return;
        IM_CHECK(app->GetCurrentProject()->GetID() == id_b);

        // Write a temp .rpv pointing at DB_A by absolute path, so it resolves
        // back to DB_A's id no matter where the .rpv lives. Escape the path so
        // the JSON stays valid.
        const fs::path rpv_path = fs::temp_directory_path() / "rocprofvis_shared_db_dedup.rpv";
        std::string escaped;
        for (char c : db_a)
        {
            if (c == '\\' || c == '"') escaped.push_back('\\');
            escaped.push_back(c);
        }
        {
            std::ofstream out(rpv_path);
            IM_CHECK(out.is_open());
            if (!out.is_open()) return;
            out << "{\"general\": {\"version\": \"1.0\", \"trace_path\": \""
                << escaped << "\"}}";
        }

        app->OpenFile(rpv_path.string());
        ctx->Yield(3);

        // Opening the .rpv must switch back to the existing DB_A tab instead of
        // opening a duplicate.
        IM_CHECK(app->GetCurrentProject() != nullptr);
        if (app->GetCurrentProject() == nullptr) return;
        IM_CHECK(app->GetCurrentProject()->GetID() == id_a);

        // No project should be keyed at the .rpv path itself.
        IM_CHECK(app->GetProject(rpv_path.string()) == nullptr);

        // Remove the temp .rpv and dismiss the dedup popup so it can't cover
        // later tests. The tabs stay open (no safe close hook).
        std::error_code ec;
        fs::remove(rpv_path, ec);
        ctx->PopupCloseAll();
        ctx->Yield(2);
    };
}
