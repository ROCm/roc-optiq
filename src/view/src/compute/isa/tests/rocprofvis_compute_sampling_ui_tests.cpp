// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "app_tests.h"

#include <memory>
#include <string>

#include "imgui_internal.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"

#include "compute/isa/rocprofvis_compute_code_widgets.h"

namespace RocProfVis
{
namespace View
{

struct SamplingWidgetTestVars
{
    LineSelection                     selection;
    PcSamplingData                    data;
    std::unique_ptr<IsaCodeWidget>    isa;
    std::unique_ptr<SourceCodeWidget> source;
    SamplingLoadStatus                status{ true, true, true };
    bool                              failed        = false;
    bool                              source_pane   = false;
    bool                              issue_visible = false;
    ImVec2                            header_point;
    ImVec2                            issue_point;
    ImVec2                            stall_point;
    std::string                       text;

    void Load()
    {
        const IsaDataBuilder::PreparedSamplingData prepared =
            IsaDataBuilder::PrepareSampling(data);
        isa->Load(data, 1, prepared);
        source->Load(data, 11, prepared);
    }

    void Initialize()
    {
        isa                              = std::make_unique<IsaCodeWidget>(selection);
        source                           = std::make_unique<SourceCodeWidget>(selection);
        data.code_objects                = { { 1,
                                               { { 1,
                                                   1,
                                                   { { 10, 1, 0x35C0, "s_barrier" },
                                                     { 20, 2, 0x25EC, "s_cbranch_scc1 4" } } } } } };
        data.instruction_type_lookup_map = { { 1, "BARRIER" }, { 2, "BRANCH" } };
        data.source_files                = { { 11,
                                               "sampling.cpp",
                                               { { 101, 1, "barrier();" },
                                                 { 102, 2, "if (condition) branch();" } } } };
        data.instruction_source_lines    = { { 10, 101, 11, 0 }, { 20, 102, 11, 0 } };
        data.pc_sample_states = { { 1, 10, 1901, 252, 1649 }, { 2, 20, 501, 501, 0 } };
        data.pc_sample_stall_reason_lookups = { { 1, "BARRIER_WAIT" },
                                                { 2, "OTHER_WAIT" } };
        data.pc_sample_stall_reasons = { { 1, 1, 1649 }, { 1, 2, 252 }, { 2, 2, 501 } };
        data.instruction_sample_lookups = { { 1, "NO_INST" },
                                            { 2, "BARRIER" },
                                            { 3, "BRANCH_NOT_TAKEN" } };
        data.instruction_samples  = { { 1, 1, 1649 }, { 1, 2, 252 }, { 2, 3, 501 } };
        data.sample_states_loaded = true;
        data.stall_details_loaded = true;
        data.instruction_samples_loaded = true;
        Load();
        isa->SetLoadState(CodeLoadState::kLoaded);
        source->SetLoadState(CodeLoadState::kLoaded);
    }
};

}  // namespace View
}  // namespace RocProfVis

void
RegisterIsaSamplingTests(ImGuiTestEngine* engine)
{
    using namespace RocProfVis::View;
    ImGuiTest* test = IM_REGISTER_TEST(engine, "compute_isa", "issue_state_columns");
    test->SetVarsDataType<SamplingWidgetTestVars>();
    test->GuiFunc = [](ImGuiTestContext* ctx) {
        SamplingWidgetTestVars& vars = ctx->GetVars<SamplingWidgetTestVars>();
        if(!vars.isa)
        {
            vars.Initialize();
        }
        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(1000.0f, 500.0f), ImGuiCond_Always);
        ImGui::Begin("ISA sampling test", nullptr, ImGuiWindowFlags_NoSavedSettings);
        const SamplingLoadStates states = vars.status.Resolve(false, vars.failed);
        vars.isa->SetSamplingLoadStates(states);
        vars.source->SetSamplingLoadStates(states);
        // Capture rendered text, including hover tooltips, without changing the
        // clipboard.
        ImGui::LogToBuffer();
        if(vars.source_pane)
        {
            vars.source->Render();
        }
        else
        {
            vars.isa->Render();
        }
        vars.text = GImGui->LogBuffer.c_str();
        ImGui::LogFinish();
        const char*       table_name = vars.source_pane ? "SourceCodeSamplingIssueStates"
                                                        : "IsaCodeSamplingIssueStates";
        const ImGuiTable* table      = ImGui::TableFindByID(ImGui::GetID(table_name));
        if(table && table->ColumnsCount > 4)
        {
            vars.issue_visible = table->Columns[4].IsEnabled;
            const float row_y  = table->RowPosY1 - ImGui::GetTextLineHeight() * 0.5f;
            vars.issue_point   = ImVec2(table->Columns[4].WorkMinX + 8.0f, row_y);
            vars.stall_point   = ImVec2(table->Columns[1].WorkMinX + 8.0f, row_y);
            vars.header_point =
                ImVec2(table->Columns[1].WorkMinX + 8.0f,
                       table->OuterRect.Min.y + ImGui::GetTextLineHeight() * 0.5f);
        }
        ImGui::End();
    };
    test->TestFunc = [](ImGuiTestContext* ctx) {
        SamplingWidgetTestVars& vars = ctx->GetVars<SamplingWidgetTestVars>();
        for(bool source_pane : { false, true })
        {
            vars.source_pane = source_pane;
            vars.failed      = false;
            vars.status      = { true, true, true };
            ctx->Yield(3);
            ctx->SetRef("ISA sampling test");
            IM_CHECK(!vars.issue_visible);
            ctx->MouseMoveToPos(vars.header_point);
            ctx->MouseClick(ImGuiMouseButton_Right);
            ctx->Yield(2);
            IM_CHECK(!GImGui->OpenPopupStack.empty());
            ImGuiWindow* popup = GImGui->OpenPopupStack.back().Window;
            IM_CHECK(popup != nullptr);
            ctx->SetRef(ImGuiTestRef(popup->ID));
            ctx->ItemClick("Issue-state counts");
            ctx->PopupCloseAll();
            ctx->Yield(3);
            IM_CHECK(vars.issue_visible);
            IM_CHECK(vars.text.find("NO_INST: 1,649; BARRIER: 252") != std::string::npos);
            IM_CHECK(vars.text.find("BRANCH_NOT_TAKEN: 501") != std::string::npos);
            ctx->SetRef("ISA sampling test");
            ctx->MouseMoveToPos(vars.issue_point);
            ctx->Yield(3);
            IM_CHECK(vars.text.find("86.7%") != std::string::npos);
            IM_CHECK(vars.text.find("13.3%") != std::string::npos);

            vars.status.instruction_samples_loaded = false;
            vars.failed                            = true;
            ctx->Yield(3);
            IM_CHECK(vars.text.find("load failed") != std::string::npos);
            IM_CHECK(vars.text.find("NO_INST: 1,649") == std::string::npos);
            ctx->MouseMoveToPos(vars.header_point);
            ctx->MouseClick(ImGuiMouseButton_Right);
            ctx->Yield(2);
            IM_CHECK(!GImGui->OpenPopupStack.empty());
            popup = GImGui->OpenPopupStack.back().Window;
            IM_CHECK(popup != nullptr);
            ctx->SetRef(ImGuiTestRef(popup->ID));
            ctx->ItemClick("Refetch sampling data");
            ctx->Yield(2);
            const bool retry = source_pane ? vars.source->TakeSamplingRetryRequest()
                                           : vars.isa->TakeSamplingRetryRequest();
            IM_CHECK(retry);
            ctx->SetRef("ISA sampling test");
            ctx->MouseMoveToPos(vars.stall_point);
            ctx->Yield(3);
            IM_CHECK(vars.text.find("BARRIER_WAIT") != std::string::npos);

            vars.status = { true, false, true };
            ctx->MouseMoveToPos(vars.issue_point);
            ctx->Yield(3);
            IM_CHECK(vars.text.find("86.7%") != std::string::npos);
        }
        vars.data.instruction_samples.clear();
        vars.Load();
        vars.status = { true, true, true };
        vars.failed = false;
        ctx->Yield(3);
        IM_CHECK(vars.text.find("not recorded") != std::string::npos);
        IM_CHECK(vars.text.find("load failed") == std::string::npos);
    };
}
