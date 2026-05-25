// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ui_test_registry.h"

#include "imgui.h"
#include "imgui_te_engine.h"
#include "implot.h"
#include "rocprofvis_settings_manager.h"
#include "spdlog/spdlog.h"

int
main()
{
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io    = ImGui::GetIO();
    io.DisplaySize = ImVec2(1280.0f, 720.0f);
    io.DeltaTime   = 1.0f / 60.0f;
    io.IniFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    RocProfVis::View::SettingsManager::GetInstance().Init();

    ImGuiTestEngine* engine = ImGuiTestEngine_CreateContext();
    ImGuiTestEngineIO& test_io = ImGuiTestEngine_GetIO(engine);
    test_io.ConfigRunSpeed = ImGuiTestRunSpeed_Fast;
    test_io.ConfigVerboseLevel = ImGuiTestVerboseLevel_Info;
    test_io.ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Debug;

    const int expected_test_count =
        RocProfVis::View::Test::RegisterRocOptiqUiTests(engine);

    ImGuiTestEngine_Start(engine, ImGui::GetCurrentContext());
    ImGuiTestEngine_QueueTests(engine, ImGuiTestGroup_Unknown, "all");

    for(int frame = 0; frame < 1200; ++frame)
    {
        io.DisplaySize = ImVec2(1280.0f, 720.0f);
        io.DeltaTime   = 1.0f / 60.0f;

        ImGui::NewFrame();
        ImGui::Render();
        ImGuiTestEngine_PostSwap(engine);

        if(frame > 0 && !test_io.IsRunningTests &&
           ImGuiTestEngine_IsTestQueueEmpty(engine))
        {
            break;
        }
    }

    ImGuiTestEngineResultSummary summary;
    ImGuiTestEngine_GetResultSummary(engine, &summary);
    const bool success = summary.CountTested == expected_test_count &&
                         summary.CountSuccess == expected_test_count &&
                         summary.CountInQueue == 0;

    spdlog::info("roc-optiq-ui-tests: {}/{} passed, {} queued",
                 summary.CountSuccess, summary.CountTested, summary.CountInQueue);
    if(!success)
    {
        ImVector<ImGuiTest*> tests;
        ImGuiTestEngine_GetTestList(engine, &tests);
        for(ImGuiTest* test : tests)
        {
            if(test->Output.Status != ImGuiTestStatus_Success)
            {
                spdlog::error("FAILED [{}] {}/{}",
                              static_cast<int>(test->Output.Status), test->Category,
                              test->Name);
                if(!test->Output.Log.IsEmpty())
                {
                    spdlog::error("{}", test->Output.Log.GetText());
                }
            }
        }
    }

    ImGuiTestEngine_Stop(engine);
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    ImGuiTestEngine_DestroyContext(engine);

    return success ? 0 : 1;
}
