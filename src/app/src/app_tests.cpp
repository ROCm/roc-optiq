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
}
