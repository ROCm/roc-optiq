// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "app_tests.h"

#include <algorithm>
#include <cmath>
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

// Inspect solid rectangles, excluding text glyphs and antialiased outlines.
static float
segment_width(const ImDrawList& draw_list, const ImRect& bounds, ImU32 color)
{
    const ImVec2 white_uv = ImGui::GetFontTexUvWhitePixel();
    float        width    = 0.0f;
    for(int i = 0; i + 3 < draw_list.VtxBuffer.Size; ++i)
    {
        const ImDrawVert* v     = &draw_list.VtxBuffer[i];
        bool              solid = true;
        for(int j = 0; j < 4; ++j)
        {
            solid &= (v[j].col & ~IM_COL32_A_MASK) == (color & ~IM_COL32_A_MASK) &&
                     v[j].uv.x == white_uv.x && v[j].uv.y == white_uv.y;
        }
        if(solid && bounds.Contains(v[0].pos) && bounds.Contains(v[2].pos) &&
           v[0].pos.x == v[3].pos.x && v[1].pos.x == v[2].pos.x &&
           v[0].pos.y == v[1].pos.y && v[2].pos.y == v[3].pos.y &&
           v[2].pos.y > v[0].pos.y)
        {
            width = std::max(width, v[2].pos.x - v[0].pos.x);
        }
    }
    return width;
}

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
    float                             issue_cell_width       = 0.0f;
    float                             no_instruction_width   = 0.0f;
    float                             barrier_width          = 0.0f;
    float                             valu_width             = 0.0f;
    float                             branch_taken_width     = 0.0f;
    float                             branch_not_taken_width = 0.0f;

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
        data.instruction_sample_lookups     = { { 1, "NO_INST" },
                                                { 2, "BARRIER" },
                                                { 3, "BRANCH_NOT_TAKEN" },
                                                { 4, "BRANCH_TAKEN" } };
        data.instruction_samples            = {
            { 1, 1, 1649 }, { 1, 2, 252 }, { 2, 3, 461 }, { 2, 4, 40 }
        };
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
            vars.issue_cell_width =
                table->Columns[4].WorkMaxX - table->Columns[4].WorkMinX;
            const float row_y  = table->RowPosY1 - ImGui::GetTextLineHeight() * 0.5f;
            vars.issue_point   = ImVec2(table->Columns[4].WorkMinX + 8.0f, row_y);
            vars.stall_point   = ImVec2(table->Columns[1].WorkMinX + 8.0f, row_y);
            vars.header_point =
                ImVec2(table->Columns[1].WorkMinX + 8.0f,
                       table->OuterRect.Min.y + ImGui::GetTextLineHeight() * 0.5f);
            const ImDrawList& draw_list   = *table->InnerWindow->DrawList;
            const float       line_height = ImGui::GetTextLineHeight();
            const ImRect      first_row(
                ImVec2(table->Columns[4].WorkMinX, table->RowPosY1 - line_height),
                ImVec2(table->Columns[4].WorkMaxX + 1.0f, table->RowPosY1));
            const ImRect second_row(
                ImVec2(first_row.Min.x, table->RowPosY1),
                ImVec2(first_row.Max.x, table->RowPosY1 + line_height));
            SettingsManager& settings = SettingsManager::GetInstance();
            vars.no_instruction_width =
                segment_width(draw_list, first_row, settings.GetColor(Colors::kTextDim));
            vars.barrier_width = segment_width(draw_list, first_row,
                                               settings.GetColor(Colors::kIsaBarrier));
            vars.valu_width =
                segment_width(draw_list, first_row, settings.GetColor(Colors::kIsaValu));
            vars.branch_taken_width = segment_width(
                draw_list, second_row, settings.GetColor(Colors::kIsaBranch));
            vars.branch_not_taken_width =
                segment_width(draw_list, second_row,
                              settings.GetColor(Colors::kPcSamplingBranchNotTaken));
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
            ctx->ItemClick("Sampled state");
            ctx->PopupCloseAll();
            ctx->Yield(3);
            IM_CHECK(vars.issue_visible);
            IM_CHECK(vars.text.find("NO_INST 86.7%") != std::string::npos);
            IM_CHECK(vars.text.find("Not taken 92%") != std::string::npos);
            IM_CHECK(vars.text.find("NO_INST: 1,649") == std::string::npos);
            IM_CHECK(vars.text.find("BRANCH_NOT_TAKEN:") == std::string::npos);
            IM_CHECK(vars.no_instruction_width > 0.0f);
            IM_CHECK(vars.barrier_width > 0.0f);
            IM_CHECK(vars.branch_taken_width > 0.0f);
            IM_CHECK(vars.branch_not_taken_width > 0.0f);
            const float barrier_total = vars.no_instruction_width + vars.barrier_width;
            const float branch_total =
                vars.branch_taken_width + vars.branch_not_taken_width;
            // The distribution fills the cell beneath the label, including its space.
            IM_CHECK(std::abs(barrier_total / vars.issue_cell_width - 1.0f) < 0.001f);
            IM_CHECK(std::abs(vars.no_instruction_width / barrier_total -
                              1649.0f / 1901.0f) < 0.001f);
            IM_CHECK(std::abs(branch_total / barrier_total - 1.0f) < 0.001f);
            IM_CHECK(std::abs(vars.branch_taken_width / branch_total - 40.0f / 501.0f) <
                     0.001f);
            ctx->SetRef("ISA sampling test");
            ctx->MouseMoveToPos(vars.issue_point);
            ctx->Yield(3);
            IM_CHECK(vars.text.find("86.7%") != std::string::npos);
            IM_CHECK(vars.text.find("13.3%") != std::string::npos);
            IM_CHECK(vars.text.find("1,649") != std::string::npos);
            IM_CHECK(vars.text.find("252") != std::string::npos);
            IM_CHECK(vars.text.find("Issued: 252 (13.3%)") != std::string::npos);
            IM_CHECK(vars.text.find("Stalled: 1,649 (86.7%)") != std::string::npos);
            IM_CHECK(vars.text.find("Main stall: BARRIER_WAIT") != std::string::npos);

            vars.status.instruction_samples_loaded = false;
            vars.failed                            = true;
            ctx->Yield(3);
            IM_CHECK(vars.text.find("load failed") != std::string::npos);
            IM_CHECK(vars.text.find("NO_INST: 1,649") == std::string::npos);
            IM_CHECK(vars.no_instruction_width == 0.0f);
            IM_CHECK(vars.barrier_width == 0.0f);
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
        // Invalid progress must not stretch or hide a complete issue distribution.
        vars.data.pc_sample_states[0].issue_count = 2000;
        vars.Load();
        vars.status = { true, true, true };
        vars.failed = false;
        ctx->Yield(3);
        IM_CHECK(vars.no_instruction_width > 0.0f);
        IM_CHECK(vars.barrier_width > 0.0f);
        const float valid_issue_width = vars.no_instruction_width + vars.barrier_width;
        const float valid_branch_width =
            vars.branch_taken_width + vars.branch_not_taken_width;
        IM_CHECK(std::abs(valid_branch_width / valid_issue_width - 1.0f) < 0.001f);
        IM_CHECK(vars.text.find("inconsistent with total samples") != std::string::npos);
        vars.data.pc_sample_states[0].issue_count = 252;
        // A single recorded state does not imply all samples issued.
        vars.data.instruction_sample_lookups.push_back({ 5, "VALU" });
        vars.data.instruction_samples = { { 1, 5, 1901 }, { 2, 3, 461 }, { 2, 4, 40 } };
        vars.Load();
        ctx->Yield(3);
        IM_CHECK(vars.text.find("VALU 100%") != std::string::npos);
        IM_CHECK(vars.valu_width > 0.0f);
        IM_CHECK(std::abs(vars.valu_width / valid_issue_width - 1.0f) < 0.001f);
        IM_CHECK(vars.text.find("Issued: 252 (13.3%)") != std::string::npos);
        IM_CHECK(vars.text.find("Stalled: 1,649 (86.7%)") != std::string::npos);
        vars.data.instruction_samples.clear();
        vars.Load();
        vars.status = { true, true, true };
        vars.failed = false;
        ctx->Yield(3);
        IM_CHECK(vars.text.find("not recorded") != std::string::npos);
        IM_CHECK(vars.text.find("load failed") == std::string::npos);
        IM_CHECK(vars.no_instruction_width == 0.0f);
        IM_CHECK(vars.barrier_width == 0.0f);

        vars.data.pc_sample_states[0] = { 1, 10, 0, 0, 0 };
        vars.data.pc_sample_stall_reasons.clear();
        vars.data.instruction_samples = { { 1, 1, 0 }, { 1, 2, 0 } };
        vars.Load();
        ctx->Yield(3);
        IM_CHECK(vars.text.find("NO_INST") != std::string::npos);
        IM_CHECK(vars.text.find("BARRIER") != std::string::npos);
        IM_CHECK(vars.text.find("0.0%") != std::string::npos);
        IM_CHECK(vars.text.find("not recorded") == std::string::npos);
        IM_CHECK(vars.no_instruction_width == 0.0f);
        IM_CHECK(vars.barrier_width == 0.0f);
    };
}
