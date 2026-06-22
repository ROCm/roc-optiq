#include "app_tests.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"
#include "imgui.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_project.h"
#include "rocprofvis_trace_view.h"
#include "rocprofvis_analysis_view.h"
#include "rocprofvis_events_view.h"
#include "rocprofvis_timeline_view.h"
#include "rocprofvis_minimap.h"
#include "compute/rocprofvis_compute_view.h"
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
        IM_CHECK(tv != nullptr);
        if (tv == nullptr) return;
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

    t = IM_REGISTER_TEST(e, "app", "minimap_toggle_drives_click");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        AppWindow* app = AppWindow::GetInstance();
        Project* project = app->GetCurrentProject();
        IM_CHECK(project != nullptr);
        if (project == nullptr) return;
        TraceView* tv = dynamic_cast<TraceView*>(project->GetView().get());
        IM_CHECK(tv != nullptr);
        if (tv == nullptr) return;
        Minimap* mm = tv->GetMinimapForTest();
        IM_CHECK(mm != nullptr);
        if (mm == nullptr) return;

        IM_CHECK(mm->GetShowEventsForTest() == true);

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
}
