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
#include "rocprofvis_minimap.h"
#include "compute/rocprofvis_compute_view.h"
#include "compute/rocprofvis_compute_selection.h"
#include "widgets/rocprofvis_tab_container.h"
using namespace RocProfVis::View;

void RegisterAppTests(ImGuiTestEngine* e)
{
    ImGuiTest* t = nullptr;

    t = IM_REGISTER_TEST(e, "app", "file_menu_exists");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        ctx->SetRef("Main Window");
        IM_CHECK(ctx->ItemExists("##MenuBar/File"));
    
    };

    t = IM_REGISTER_TEST(e, "app", "file_menu_opens");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        ctx->SetRef("Main Window");
        ctx->ItemClick("##MenuBar/File");
        IM_CHECK(ctx->ItemExists("//Menu_00/Open"));
        // Close the menu so its popup doesn't intercept clicks in later tests.
        ctx->PopupCloseAll();
    };
    t = IM_REGISTER_TEST(e, "app", "events_view_populates");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        AppWindow* app = AppWindow::GetInstance();
        Project* project = app->GetCurrentProject();
        IM_CHECK(project != nullptr);
        if (project == nullptr) return;
        TraceView* tv = dynamic_cast<TraceView*>(project->GetView().get());
        if (tv == nullptr)
        {
            ctx->LogWarning("SKIP: no trace view loaded (open a system/trace profile to exercise this)");
            return;
        }
        AnalysisView* av = tv->GetAnalysisViewForTest();
        IM_CHECK(av != nullptr);
        if (av == nullptr) return;
        EventsView* ev = av->GetEventsViewForTest();
        IM_CHECK(ev != nullptr);
        if (ev == nullptr) return;

        // A prior run may have left an event selected; the clear is dispatched
        // through EventManager, so yield before asserting the empty baseline.
        tv->ClearEventSelectionForTest();
        ctx->Yield(3);
        IM_CHECK(ev->GetEventItemCountForTest() == 0);

        // Timeline events are canvas-drawn (no widget IDs). Click the screen
        // center of the first rendered event box, captured by the renderer with
        // the geometry it draws/hit-tests with, so the click is window- and
        // trace-independent (no hard-coded sidebar/track offsets).
        TimelineView* tlv = tv->GetTimelineViewForTest();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;

        ctx->Yield(3);

        ImVec2 event_center(0.0f, 0.0f);
        bool   have_center = tlv->GetFirstEventScreenCenterForTest(event_center);
        IM_CHECK(have_center);
        if (!have_center) return;

        // Selection is deferred a frame, so move/release with the mouse parked.
        ctx->MouseMoveToPos(event_center);
        ctx->Yield(2);
        ctx->MouseDown(0);
        ctx->Yield(1);
        ctx->MouseUp(0);
        ctx->Yield(3);

        IM_CHECK(ev->GetEventItemCountForTest() > 0);
    };

    t = IM_REGISTER_TEST(e, "app", "timeline_zoom_hotkey");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        AppWindow* app = AppWindow::GetInstance();
        Project* project = app->GetCurrentProject();
        IM_CHECK(project != nullptr);
        if (project == nullptr) return;
        TraceView* tv = dynamic_cast<TraceView*>(project->GetView().get());
        if (tv == nullptr)
        {
            ctx->LogWarning("SKIP: no trace view loaded (open a system/trace profile to exercise this)");
            return;
        }
        TimelineView* tlv = tv->GetTimelineViewForTest();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;

        // The W/S zoom hotkeys only fire while the timeline has pseudo-focus,
        // which a mouse-down inside the graph sets. Park the cursor on the first
        // rendered event (its captured center is a point known to be in-graph)
        // and press the mouse to acquire focus.
        ctx->Yield(3);
        ImVec2 event_center(0.0f, 0.0f);
        bool   have_center = tlv->GetFirstEventScreenCenterForTest(event_center);
        IM_CHECK(have_center);
        if (!have_center) return;

        ctx->MouseMoveToPos(event_center);
        ctx->MouseDown(0);
        ctx->Yield(1);
        ctx->MouseUp(0);
        ctx->Yield(2);

        const float zoom_before = tv->GetTimelineViewForTest()->GetViewCoords().z;

        // Zoom in: hotkey is consumed in the timeline's per-frame input handler,
        // so keep the cursor parked in-graph while pressing.
        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_W);
        ctx->Yield(3);
        const float zoom_in = tv->GetTimelineViewForTest()->GetViewCoords().z;
        IM_CHECK(zoom_in > zoom_before);

        // Zoom out returns toward the starting zoom.
        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_S);
        ctx->Yield(3);
        const float zoom_out = tv->GetTimelineViewForTest()->GetViewCoords().z;
        IM_CHECK(zoom_out < zoom_in);
    };

    t = IM_REGISTER_TEST(e, "app", "bookmark_save_restore_hotkey");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        AppWindow* app = AppWindow::GetInstance();
        Project* project = app->GetCurrentProject();
        IM_CHECK(project != nullptr);
        if (project == nullptr) return;
        TraceView* tv = dynamic_cast<TraceView*>(project->GetView().get());
        if (tv == nullptr)
        {
            ctx->LogWarning("SKIP: no trace view loaded (open a system/trace profile to exercise this)");
            return;
        }
        TimelineView* tlv = tv->GetTimelineViewForTest();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;

        // Ctrl+1 always writes bookmark slot 1, and m_bookmarks is a slot-keyed
        // map, so a re-save overwrites rather than grows it. Clear first so the
        // 0->1 assertion holds on every invocation in a persistent process (the
        // interactive harness reuses one process; headless is fresh each run).
        tv->ClearBookmarksForTest();
        ctx->Yield(1);
        IM_CHECK(tv->GetBookmarkCountForTest() == 0);

        // HandleHotKeys is gated on IsWindowFocused(RootAndChildWindows) for
        // "Main Window". Headless that focus is implicit, but interactively the
        // Test Engine window holds it and the chord is dropped. Focus explicitly,
        // then click an event to land the cursor in-graph.
        ctx->Yield(3);
        ImVec2 event_center(0.0f, 0.0f);
        bool   have_center = tlv->GetFirstEventScreenCenterForTest(event_center);
        IM_CHECK(have_center);
        if (!have_center) return;

        ctx->WindowFocus("Main Window");
        ctx->MouseMoveToPos(event_center);
        ctx->MouseClick(0);
        ctx->Yield(2);

        // Save the current view, then record the coords the bookmark captured.
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_1);
        ctx->Yield(3);
        IM_CHECK(tv->GetBookmarkCountForTest() == 1);
        const ViewCoords saved = tv->GetTimelineViewForTest()->GetViewCoords();
        const double saved_span = saved.v_max_x - saved.v_min_x;
        IM_CHECK(saved_span > 0.0);
        if (saved_span <= 0.0) return;

        // Zoom in so the view range changes away from the saved one.
        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_W);
        ctx->Yield(3);
        const ViewCoords moved = tv->GetTimelineViewForTest()->GetViewCoords();
        IM_CHECK((moved.v_max_x - moved.v_min_x) < saved_span);

        // Restore: bare "1" moves the view back to the saved range. Restore
        // reconstructs zoom/offset from the saved span through a float, so assert
        // within a small relative tolerance rather than exact equality.
        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_1);
        ctx->Yield(3);
        const ViewCoords restored = tv->GetTimelineViewForTest()->GetViewCoords();
        const double tol = saved_span * 0.01;
        IM_CHECK(fabs(restored.v_min_x - saved.v_min_x) < tol);
        IM_CHECK(fabs(restored.v_max_x - saved.v_max_x) < tol);
    };

    t = IM_REGISTER_TEST(e, "app", "event_multi_select");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        AppWindow* app = AppWindow::GetInstance();
        Project* project = app->GetCurrentProject();
        IM_CHECK(project != nullptr);
        if (project == nullptr) return;
        TraceView* tv = dynamic_cast<TraceView*>(project->GetView().get());
        if (tv == nullptr)
        {
            ctx->LogWarning("SKIP: no trace view loaded (open a system/trace profile to exercise this)");
            return;
        }
        TimelineView* tlv = tv->GetTimelineViewForTest();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;
        std::shared_ptr<TimelineSelection> sel = tv->GetTimelineSelection();
        IM_CHECK(sel != nullptr);
        if (sel == nullptr) return;

        // Start from a clean selection.
        tv->ClearEventSelectionForTest();
        ctx->Yield(3);
        std::vector<uint64_t> ids;
        sel->GetSelectedEvents(ids);
        IM_CHECK(ids.empty());

        // Need two distinct, clickable events in one flame track.
        ctx->Yield(3);
        ImVec2 first(0.0f, 0.0f), second(0.0f, 0.0f);
        bool have_two = tlv->GetTwoEventScreenCentersForTest(first, second);
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
        tv->ClearEventSelectionForTest();
        ctx->Yield(2);
    };

    t = IM_REGISTER_TEST(e, "app", "minimap_toggle_drives_click");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        AppWindow* app = AppWindow::GetInstance();
        Project* project = app->GetCurrentProject();
        IM_CHECK(project != nullptr);
        if (project == nullptr) return;
        TraceView* tv = dynamic_cast<TraceView*>(project->GetView().get());
        if (tv == nullptr)
        {
            ctx->LogWarning("SKIP: no trace view loaded (open a system/trace profile to exercise this)");
            return;
        }
        Minimap* mm = tv->GetMinimapForTest();
        IM_CHECK(mm != nullptr);
        if (mm == nullptr) return;

        IM_CHECK(mm->GetShowEventsForTest() == true);
        // Counter overlay state before touching events, to confirm the two
        // layers toggle independently (checklist: "Layers independent").
        const bool counters_before = mm->GetShowCountersForTest();

        // The ICON_COMPASS toolbar button and the ##events checkbox are both
        // canvas-styled / nested in a BeginChild, so neither has a stable widget
        // path. Click each by the screen center its renderer captured.
        ctx->Yield(3);
        ImVec2 btn_center(0.0f, 0.0f);
        bool have_btn = tv->GetMinimapButtonScreenCenterForTest(btn_center);
        IM_CHECK(have_btn);
        if (!have_btn) return;

        ctx->MouseTeleportToPos(btn_center);
        ctx->MouseClick(0);
        ctx->Yield(3);

        ImVec2 cb_center(0.0f, 0.0f);
        bool have_cb = mm->GetEventsCheckboxScreenCenterForTest(cb_center);
        IM_CHECK(have_cb);
        if (!have_cb) return;

        ctx->MouseTeleportToPos(cb_center);
        ctx->MouseClick(0);
        ctx->Yield(2);
        IM_CHECK(mm->GetShowEventsForTest() == false);
        // Toggling events must not disturb the counter overlay.
        IM_CHECK(mm->GetShowCountersForTest() == counters_before);

        // Toggle back to confirm the click is bidirectional.
        ctx->MouseTeleportToPos(cb_center);
        ctx->MouseClick(0);
        ctx->Yield(2);
        IM_CHECK(mm->GetShowEventsForTest() == true);

        // Close the Minimap window so it doesn't cover later tests' widgets.
        ctx->MouseTeleportToPos(btn_center);
        ctx->MouseClick(0);
        ctx->Yield(2);
    };

    t = IM_REGISTER_TEST(e, "app", "compute_view_tab_switch");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        AppWindow* app = AppWindow::GetInstance();
        Project* project = app->GetCurrentProject();
        IM_CHECK(project != nullptr);
        if (project == nullptr) return;
        // Needs a compute profile loaded; with a trace this cast is null and the
        // test logs a skip (Test Engine has no skip status, so a bare return
        // would read as a green assertion).
        ComputeView* cv = dynamic_cast<ComputeView*>(project->GetView().get());
        if (cv == nullptr)
        {
            ctx->LogWarning("SKIP: no compute view loaded (open a compute profile to exercise this)");
            return;
        }
        TabContainer* tc = cv->GetTabContainerForTest();
        if (tc == nullptr)
        {
            ctx->LogWarning("SKIP: compute view has no tab container");
            return;
        }

        IM_CHECK(tc->GetTabCountForTest() >= 2);
        if (tc->GetTabCountForTest() < 2) return;

        ctx->Yield(3);
        const int start_idx = tc->GetActiveTabIndexForTest();
        IM_CHECK(start_idx >= 0);

        // Tab headers are wrapped in PushID (unstable path) and a raw mouse click
        // does not reliably activate a tab, so drive it by the captured id.
        const int target_idx = (start_idx == 0) ? 1 : 0;
        ImGuiID tab_id = tc->GetTabHeaderIdForTest(target_idx);
        IM_CHECK(tab_id != 0);
        if (tab_id == 0) return;

        ctx->ItemClick(tab_id);
        ctx->Yield(3);

        IM_CHECK(tc->GetActiveTabIndexForTest() == target_idx);
    };

    t = IM_REGISTER_TEST(e, "app", "compute_workload_auto_selected");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        AppWindow* app = AppWindow::GetInstance();
        Project* project = app->GetCurrentProject();
        IM_CHECK(project != nullptr);
        if (project == nullptr) return;
        ComputeView* cv = dynamic_cast<ComputeView*>(project->GetView().get());
        if (cv == nullptr)
        {
            ctx->LogWarning("SKIP: no compute view loaded (open a compute profile to exercise this)");
            return;
        }
        ComputeSelection* sel = cv->GetComputeSelectionForTest();
        IM_CHECK(sel != nullptr);
        if (sel == nullptr) return;

        // Loading a compute profile auto-selects the first workload, which
        // cascades to selecting that workload's first kernel. Both selection
        // ids must therefore be valid (not the INVALID sentinel) once loaded.
        ctx->Yield(3);
        IM_CHECK(sel->GetSelectedWorkload() != ComputeSelection::INVALID_SELECTION_ID);
        IM_CHECK(sel->GetSelectedKernel() != ComputeSelection::INVALID_SELECTION_ID);
    };

    t = IM_REGISTER_TEST(e, "app", "timeline_pan_hotkey");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        AppWindow* app = AppWindow::GetInstance();
        Project* project = app->GetCurrentProject();
        IM_CHECK(project != nullptr);
        if (project == nullptr) return;
        TraceView* tv = dynamic_cast<TraceView*>(project->GetView().get());
        if (tv == nullptr)
        {
            ctx->LogWarning("SKIP: no trace view loaded (open a system/trace profile to exercise this)");
            return;
        }
        TimelineView* tlv = tv->GetTimelineViewForTest();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;

        // Pan (A/D) shares the timeline's m_pseudo_focus gate, set by a mouse-down
        // in the graph; park on the first event to acquire it.
        ctx->Yield(3);
        ImVec2 event_center(0.0f, 0.0f);
        bool   have_center = tlv->GetFirstEventScreenCenterForTest(event_center);
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

    t = IM_REGISTER_TEST(e, "app", "timeline_vscroll");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        AppWindow* app = AppWindow::GetInstance();
        Project* project = app->GetCurrentProject();
        IM_CHECK(project != nullptr);
        if (project == nullptr) return;
        TraceView* tv = dynamic_cast<TraceView*>(project->GetView().get());
        if (tv == nullptr)
        {
            ctx->LogWarning("SKIP: no trace view loaded (open a system/trace profile to exercise this)");
            return;
        }
        TimelineView* tlv = tv->GetTimelineViewForTest();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;

        ctx->Yield(3);
        ImVec2 event_center(0.0f, 0.0f);
        bool   have_center = tlv->GetFirstEventScreenCenterForTest(event_center);
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
        if (tlv->GetMaxYScrollForTest() <= 0.0f)
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
}
