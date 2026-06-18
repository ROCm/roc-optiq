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
        IM_CHECK(project != nullptr);          // stop here if no trace
        if (project == nullptr) return;
        TraceView* tv = dynamic_cast<TraceView*>(project->GetView().get());
        IM_CHECK(tv != nullptr);
        if (tv == nullptr) return;
        AnalysisView* av = tv->GetAnalysisViewForTest();
        IM_CHECK(av != nullptr);
    };
}
