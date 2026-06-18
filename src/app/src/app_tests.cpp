#include "app_tests.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"
#include "imgui.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_project.h"
#include "rocprofvis_trace_view.h"
#include "rocprofvis_analysis_view.h"
#include "rocprofvis_events_view.h"
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

        IM_CHECK(ev->GetEventItemCountForTest() == 0);

        // Timeline events are canvas-drawn (no widget IDs), so click by coordinate.
        // Anchor x in fixed pixels past the fixed ~400px sidebar; the vertical
        // position varies, so sweep the height and stop on the first hit.
        // WIP: coarse blind sweep, deterministic computed-coordinate click to come.
        const ImVec2 size       = ImGui::GetIO().DisplaySize;
        const float  graph_x0   = 400.0f + 10.0f;
        const float  graph_x1   = size.x - 20.0f;
        const float  graph_y0   = size.y * 0.10f;
        const float  graph_y1   = size.y * 0.90f;
        const int    rows       = 20;
        const int    cols       = 30;

        bool landed = false;
        for (int r = 0; r < rows && !landed; ++r)
        {
            const float yt = (rows > 1) ? (float)r / (float)(rows - 1) : 0.0f;
            const float py = graph_y0 + (graph_y1 - graph_y0) * yt;
            for (int c = 0; c < cols && !landed; ++c)
            {
                const float xt = (cols > 1) ? (float)c / (float)(cols - 1) : 0.0f;
                const float px = graph_x0 + (graph_x1 - graph_x0) * xt;
                ctx->MouseTeleportToPos(ImVec2(px, py));
                ctx->MouseClick(0);
                ctx->Yield(3);
                if (ev->GetEventItemCountForTest() > 0)
                {
                    landed = true;
                }
            }
        }

        IM_CHECK(ev->GetEventItemCountForTest() > 0);
    };
}
