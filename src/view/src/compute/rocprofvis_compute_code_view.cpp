// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_code_view.h"
#include "rocprofvis_compute_selection.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_events.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_requests.h"

#include <algorithm>
#include <string>
#include <unordered_map>

namespace RocProfVis
{
namespace View
{

ComputeCodeView::ComputeCodeView(DataProvider& data_provider)
: RocWidget()
, m_settings(SettingsManager::GetInstance())
, m_data_provider(data_provider)
, m_control_panel_height(0.0f)
, m_current_source_file_id(ComputeSelection::INVALID_SELECTION_ID)
, m_current_code_object_uuid(ComputeSelection::INVALID_SELECTION_ID)
, m_current_kernel_id(ComputeSelection::INVALID_SELECTION_ID)
, m_current_workload_id(ComputeSelection::INVALID_SELECTION_ID)
, m_show_metadata_enabled(false)
{
    m_source_code = std::make_shared<SourceCodeWidget>(m_line_selection);
    m_isa_code    = std::make_shared<IsaCodeWidget>(m_line_selection);

    auto isa_item           = LayoutItem::CreateFromWidget(m_isa_code);
    isa_item->m_child_flags = ImGuiChildFlags_None;

    m_source_layout_item                = LayoutItem::CreateFromWidget(m_source_code);
    m_source_layout_item->m_child_flags = ImGuiChildFlags_None;
    m_source_layout_item->m_visible     = false;

    m_horizontal_split_container =
        std::make_shared<HSplitContainer>(isa_item, m_source_layout_item);
    m_horizontal_split_container->SetSplit(0.5f);
    m_horizontal_split_container->ShowSplitter(true);

    SubscribeToEvents();

    m_data_provider.SetFetchPcSamplingCallback(
        [this](const std::string&, uint32_t kernel_id, uint32_t source_file_id,
               uint32_t generation, bool success) {
            OnPcSamplingReady(kernel_id, source_file_id, generation, success);
        });
}

ComputeCodeView::~ComputeCodeView()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeKernelSelectionChanged),
        m_kernel_selection_changed_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeWorkloadSelectionChanged),
        m_workload_selection_changed_token);
}

void
ComputeCodeView::SubscribeToEvents()
{
    auto workload_changed = [this](std::shared_ptr<RocEvent> e) {
        auto event = std::dynamic_pointer_cast<ComputeSelectionChangedEvent>(e);
        if(event && event->GetSourceId() == m_data_provider.GetTraceFilePath())
            SelectWorkload(event->GetId());
    };
    m_workload_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeWorkloadSelectionChanged), workload_changed);

    auto kernel_changed = [this](std::shared_ptr<RocEvent> e) {
        auto event = std::dynamic_pointer_cast<ComputeSelectionChangedEvent>(e);
        if(event && event->GetSourceId() == m_data_provider.GetTraceFilePath())
            LoadData(event->GetId());
    };
    m_kernel_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeKernelSelectionChanged), kernel_changed);
}

void
ComputeCodeView::SelectWorkload(uint32_t workload_id)
{
    m_current_workload_id = workload_id;
}

void
ComputeCodeView::LoadData(uint32_t kernel_id)
{
    m_current_kernel_id = kernel_id;

    const WorkloadInfo* workload =
        m_data_provider.ComputeModel().GetWorkload(m_current_workload_id);
    if(!workload || !workload->kernels.count(kernel_id))
    {
        m_current_workload_id = ComputeSelection::INVALID_SELECTION_ID;
        for(const WorkloadInfo* candidate : m_data_provider.ComputeModel().GetWorkloadList())
        {
            if(candidate->kernels.count(kernel_id))
            {
                m_current_workload_id = candidate->id;
                break;
            }
        }
    }
    if(m_current_workload_id == ComputeSelection::INVALID_SELECTION_ID)
    {
        m_pending_refetch = false;
        if(m_fetch_in_progress)
            m_data_provider.CancelRequest(m_active_request_id);
        ClearSelectionData();
        return;
    }

    const KernelInfo* kernel_info = m_data_provider.ComputeModel().GetKernelInfo(
        m_current_workload_id, kernel_id);
    if(!kernel_info)
    {
        m_current_workload_id = ComputeSelection::INVALID_SELECTION_ID;
        m_pending_refetch     = false;
        if(m_fetch_in_progress)
            m_data_provider.CancelRequest(m_active_request_id);
        ClearSelectionData();
        return;
    }

    // Source file list is already populated eagerly - just refresh the selection.
    LoadSourceFileList(kernel_info->pc_sampling_data);

    // Clear stale widget data. The mandatory code-object, ISA, and sample-state
    // request starts from Render, when the PC Sampling View is open.
    ClearCodeData();
    m_pending_refetch = true;
    if(m_fetch_in_progress)
        m_data_provider.CancelRequest(m_active_request_id);
}

void
ComputeCodeView::ClearCodeData()
{
    m_source_code->Load({}, 0);
    m_isa_code->Load({}, 0);
}

void
ComputeCodeView::ClearSelectionData()
{
    m_source_files.clear();
    m_current_source_file_id = ComputeSelection::INVALID_SELECTION_ID;
    m_current_code_object_uuid = ComputeSelection::INVALID_SELECTION_ID;
    ClearCodeData();
}

void
ComputeCodeView::FetchMandatoryPcSampling()
{
    if(m_current_kernel_id == ComputeSelection::INVALID_SELECTION_ID ||
       m_current_workload_id == ComputeSelection::INVALID_SELECTION_ID)
    {
        m_pending_refetch = false;
        if(m_fetch_in_progress)
            m_data_provider.CancelRequest(m_active_request_id);
        return;
    }

    if(m_fetch_in_progress)
    {
        m_pending_refetch = true;
        m_data_provider.CancelRequest(m_active_request_id);
        return;
    }

    // This ID correlates the callback with the selected kernel and source file;
    // it does not permit multiple Code View requests to run concurrently.
    const uint64_t request_id = RequestIdBuilder::MakeClientRequestId(
        RequestType::kFetchPcSampling,
        (static_cast<uint64_t>(m_current_kernel_id) << 32) | m_current_source_file_id);

    ++m_fetch_generation;
    m_pending_refetch = false;
    if(m_data_provider.FetchPcSampling(
           PcSamplingRequestParams(m_current_workload_id, m_current_kernel_id,
                                   m_current_source_file_id, m_fetch_generation)))
    {
        m_active_request_id = request_id;
        m_fetch_in_progress = true;
    }
}

void
ComputeCodeView::OnPcSamplingReady(uint32_t kernel_id, uint32_t source_file_id,
                                   uint32_t generation, bool success)
{
    const uint64_t request_id = RequestIdBuilder::MakeClientRequestId(
        RequestType::kFetchPcSampling,
        (static_cast<uint64_t>(kernel_id) << 32) | source_file_id);
    if(!m_fetch_in_progress || request_id != m_active_request_id ||
       generation != m_fetch_generation)
    {
        return;
    }

    m_fetch_in_progress = false;

    // Start a deferred fetch from Render, after DataProvider has erased the
    // completed request from its request map.
    if(m_pending_refetch)
    {
        return;
    }

    if(kernel_id != m_current_kernel_id || source_file_id != m_current_source_file_id)
        return;

    if(!success)
        return;

    const KernelInfo* kernel_info = m_data_provider.ComputeModel().GetKernelInfo(
        m_current_workload_id, m_current_kernel_id);
    if(!kernel_info)
        return;

    const PcSamplingData& data = kernel_info->pc_sampling_data;

    if(m_current_source_file_id != ComputeSelection::INVALID_SELECTION_ID)
        m_source_code->Load(data, m_current_source_file_id);

    if(!data.code_objects.empty())
        m_current_code_object_uuid = data.code_objects[0].code_object_uuid;

    if(m_current_code_object_uuid != ComputeSelection::INVALID_SELECTION_ID)
        m_isa_code->Load(data, m_current_code_object_uuid);
}

void
ComputeCodeView::LoadSourceFileList(const PcSamplingData& data)
{
    m_source_files.clear();
    for (auto& file : data.source_files)
        m_source_files.emplace(file.file_path, file.id);

    bool selection_valid = false;
    for(const auto& [path, id] : m_source_files)
    {
        if(id == m_current_source_file_id)
        {
            selection_valid = true;
            break;
        }
    }
    if(!selection_valid)
        m_current_source_file_id = m_source_files.empty()
            ? ComputeSelection::INVALID_SELECTION_ID
            : m_source_files.begin()->second;
}

void
ComputeCodeView::Render()
{
    if(m_pending_refetch && !m_fetch_in_progress)
    {
        FetchMandatoryPcSampling();
    }

    RenderControlPanel();

    ImGui::PushFont(m_settings.GetFontManager().GetFont(FontType::kCode), 0.0f);

    m_horizontal_split_container->Render();

    ImGui::PopFont();
}

void
ComputeCodeView::RenderControlPanel()
{
    constexpr const char* hide_source_code_str = "Hide Source Code";
    constexpr const char* show_source_code_str = "Show Source Code";
    constexpr const char* show_stalls_str      = "Show Stalls";
    constexpr const char* hide_stalls_str      = "Hide Stalls";

    const float fallbackHeight =
        ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f;

    float topHeight =
        m_control_panel_height > 0.0f ? m_control_panel_height : fallbackHeight;

    ImGui::BeginChild("ControlPanel", ImVec2(0.0f, topHeight), true);

    ImVec2 start = ImGui::GetCursorPos();

    ImGui::BeginGroup();

    RenderSourceFileDropdown();

    const float button_source_code_width =
        std::max(ImGui::CalcTextSize(show_source_code_str).x,
                 ImGui::CalcTextSize(hide_source_code_str).x) +
        ImGui::GetStyle().FramePadding.x * 2.0f;
    const float button_stall_width = std::max(ImGui::CalcTextSize(show_stalls_str).x,
                                              ImGui::CalcTextSize(hide_stalls_str).x) +
                                     ImGui::GetStyle().FramePadding.x * 2.0f;

    const float button_comments_width =
        ImGui::CalcTextSize("Show Comments").x + ImGui::GetStyle().FramePadding.x * 2.0f;

    const float buttons_width = button_source_code_width + button_stall_width +
                                button_comments_width +
                                ImGui::GetStyle().ItemSpacing.x * 2.0f;

    ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() -
                    buttons_width);

    if(ImGui::Button(m_source_layout_item->m_visible ? hide_source_code_str
                                                     : show_source_code_str))
    {
        m_source_layout_item->m_visible = !m_source_layout_item->m_visible;
    }

    ImGui::SameLine();
    if(ImGui::Button(m_show_metadata_enabled ? hide_stalls_str : show_stalls_str))
    {
        m_show_metadata_enabled = !m_show_metadata_enabled;
        m_source_code->ChangeStallVisibility(m_show_metadata_enabled);
        m_isa_code->ChangeStallVisibility(m_show_metadata_enabled);
    }

    ImGui::SameLine();
    static bool show_comments_enabled = false;
    if(ImGui::Button(show_comments_enabled ? "Hide Comments" : "Show Comments"))
    {
        show_comments_enabled = !show_comments_enabled;
        m_isa_code->ShowComments(show_comments_enabled);
    }

    ImGui::EndGroup();

    ImVec2 end = ImGui::GetCursorPos();

    float contentHeight = end.y - start.y;
    m_control_panel_height =
        contentHeight +
        ImGui::GetStyle().WindowPadding.y * 2.0f;

    ImGui::EndChild();
}

void
ComputeCodeView::RenderSourceFileDropdown()
{
    constexpr const float DROPDAWN_SIZE = 300.0f;
    if(m_source_files.empty())
        return;

    auto filename_of = [](const std::string& str) -> const char* {
        const auto pos = str.find_last_of("/\\");
        return pos == std::string::npos ? str.c_str() : str.c_str() + pos + 1;
    };

    const auto selected_file_it = std::find_if(m_source_files.begin(), m_source_files.end(),
        [this](const auto& pair) { return pair.second == m_current_source_file_id; });

    const char* preview = selected_file_it != m_source_files.end()
                                    ? filename_of(selected_file_it->first)
                                    : "<none>";

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Source file:");
    ImGui::SameLine();

    ImGui::SetNextItemWidth(DROPDAWN_SIZE);
    if(ImGui::BeginCombo("##source_file", preview))
    {
        for(const auto& [path, id] : m_source_files)
        {
            const bool selected = (id == m_current_source_file_id);
            if(ImGui::Selectable(filename_of(path), selected) && !selected)
            {
                m_current_source_file_id = id;
                m_source_code->Load({}, 0);
                m_isa_code->Load({}, 0);
                FetchMandatoryPcSampling();
            }
            if(selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

//----------------------------------------------------------------

BaseCodeWidget::BaseCodeWidget(LineSelection& selection)
: m_line_selection(selection)
, m_settings(SettingsManager::GetInstance())
{
    m_selected_colour =
        ImGui::GetColorU32(m_settings.GetColor(Colors::kSelection));
    m_hovered_colour  =
        ImGui::GetColorU32(m_settings.GetColor(Colors::kHighlightChart));
    m_line_num_color = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    m_comment_color  = { 0.34f, 0.65f, 0.29f, 1.0f };

    m_table_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_NoPadOuterX |
        ImGuiTableFlags_BordersInnerV;
}

void
BaseCodeWidget::CalculateLineNumberWidth(uint32_t count)
{
    for(uint32_t number = count; number >= 10; number /= 10)
        m_line_num_digits++;

    m_line_num_width =
        ImGui::CalcTextSize("0").x * static_cast<float>(m_line_num_digits + 1);
}

void
BaseCodeWidget::PushStyles()
{
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,
                        ImVec2(ImGui::GetStyle().CellPadding.x, 0.0f));

    ImGui::PushStyleColor(ImGuiCol_Header, m_settings.GetColor(Colors::kSelection));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                          m_settings.GetColor(Colors::kHighlightChart));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                          m_settings.GetColor(Colors::kHighlightChart));
}

//----------------------------------------------------------------

SourceCodeWidget::SourceCodeWidget(LineSelection& selection)
: BaseCodeWidget(selection)
{
}

void
SourceCodeWidget::Load(const PcSamplingData& data, uint32_t source_file_id)
{
    m_lines.clear();
    m_line_selection = {0, 0};

    const SourceFile* source_file = nullptr;
    for(const auto& file : data.source_files)
    {
        if(file.id == source_file_id)
        {
            source_file = &file;
            break;
        }
    }
    if(!source_file)
        return;

    // STUB: summarised_stalls not yet derived from real stall data.
    uint32_t stub_counter = 0;
    for(const auto& source_line : source_file->source_lines)
    {
        const float stub_stall = static_cast<float>((stub_counter * 7 + 13) % 101);
        stub_counter++;
        m_lines.push_back({ source_line.content, source_line.id, stub_stall });
    }

    CalculateLineNumberWidth(m_lines.size());
}

void
SourceCodeWidget::Render()
{
    if(m_lines.empty())
    {
        ImGui::TextDisabled("No file loaded");
        return;
    }

    const int columns_count = IsStallShown() ? 3 : 2;

    if(!ImGui::BeginTable("SourceCode", columns_count, m_table_flags))
        return;

    ImGui::TableSetupScrollFreeze(0, 0);

    ImGui::TableSetupColumn(
        "#", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed,
        m_line_num_width);

    if(IsStallShown())
        ImGui::TableSetupColumn(
            "Stalls", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed,
                                ImGui::CalcTextSize("100.0%").x);

    ImGui::TableSetupColumn("Source code", ImGuiTableColumnFlags_WidthStretch);

    ImGui::TableHeadersRow();
    PushStyles();

    ImGuiListClipper clipper;
    clipper.Begin(m_lines.size());

    while(clipper.Step())
    {
        for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        {
            RenderLine(i, columns_count);
        }
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    ImGui::EndTable();
}

void
SourceCodeWidget::RenderLine(uint32_t index, uint32_t columns_count)
{
    const SourceRow& source_row = m_lines[index];
    uint32_t display_num = index + 1;

    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    ImGui::PushID(static_cast<int>(source_row.id));
    if(ImGui::Selectable("##row", source_row.id == m_line_selection.selected_line,
                         ImGuiSelectableFlags_SpanAllColumns,
                         ImVec2(0.0f, ImGui::GetTextLineHeight())))
    {
        m_line_selection.selected_line = source_row.id;
    }
    if(ImGui::IsItemHovered()) m_line_selection.hovered_line = source_row.id;

    if(source_row.id == m_line_selection.selected_line)
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, m_selected_colour);
    else if(source_row.id == m_line_selection.hovered_line &&
            m_line_selection.hovered_line != 0)
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, m_hovered_colour);

    ImGui::SameLine(0.0f, 0.0f);
    ImGui::PopID();

    ImGui::TextColored(m_line_num_color, "%*u", m_line_num_digits, display_num);

    int col = 1;
    if(IsStallShown())
    {
        ImGui::TableSetColumnIndex(col++);
        ImGui::TextDisabled("%.1f%%", source_row.summarised_stalls);
    }

    ImGui::TableSetColumnIndex(col);
    ImGui::TextUnformatted(source_row.content.c_str());
}

//----------------------------------------------------------------

IsaCodeWidget::IsaCodeWidget(LineSelection& selection)
: BaseCodeWidget(selection)
{
}

void
IsaCodeWidget::Load(const PcSamplingData& data, uint64_t code_object_uuid)
{
    m_entries.clear();
    m_line_selection = {0, 0};

    const CodeObjectStore* code_object = nullptr;
    for(const auto& code_obj : data.code_objects)
    {
        if(code_obj.code_object_uuid == code_object_uuid)
        {
            code_object = &code_obj;
            break;
        }
    }
    if(!code_object)
        return;

    // Build depth-0 source_line_id lookup: isa_line_id -> source_line_id
    std::unordered_map<uint64_t, uint32_t> source_by_isa;
    for(const auto& dep : data.isa_to_source_deps)
    {
        if(dep.depth == 0)
            source_by_isa.emplace(dep.isa_line_id, dep.source_line_id);
    }

    struct InstructionSampleCounts
    {
        uint64_t total_count = 0;
        uint64_t issue_count = 0;
        uint64_t stall_count = 0;
    };
    std::unordered_map<uint64_t, InstructionSampleCounts> counts_by_instruction;
    counts_by_instruction.reserve(data.pc_sample_states.size());
    for(const PcSampleState& state : data.pc_sample_states)
    {
        InstructionSampleCounts& counts = counts_by_instruction[state.instruction_uuid];
        counts.total_count += state.total_count;
        counts.issue_count += state.issue_count;
        counts.stall_count += state.stall_count;
    }

    for(const auto& isa_line : code_object->isa_lines)
    {
        uint32_t source_line_id = 0;
        if(const auto sit = source_by_isa.find(isa_line.instruction_uuid);
           sit != source_by_isa.end())
            source_line_id = sit->second;

        const InstructionSampleCounts* counts = nullptr;
        if(const auto counts_it = counts_by_instruction.find(isa_line.instruction_uuid);
           counts_it != counts_by_instruction.end())
            counts = &counts_it->second;

        m_entries.push_back({
            isa_line.instruction,
            isa_line.comment,
            isa_line.instruction_uuid,
            source_line_id,
            counts ? counts->issue_count : 0,
            counts ? counts->stall_count : 0,
            counts ? counts->total_count : 0
        });
    }

    CalculateLineNumberWidth(m_entries.size());
}

void
IsaCodeWidget::Render()
{
    if(m_entries.empty())
    {
        ImGui::TextDisabled("No ISA loaded");
        return;
    }

    const int stall_columns   = IsStallShown() ? 3 : 0;
    const int comment_columns = m_show_comments ? 1 : 0;
    const int columns_count   = 2 + stall_columns + comment_columns;

    if(!ImGui::BeginTable("IsaCode", columns_count, m_table_flags))
        return;

    ImGui::TableSetupColumn(
        "#", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed,
        m_line_num_width);

    ImGui::TableSetupColumn("ISA", ImGuiTableColumnFlags_WidthStretch);

    if(IsStallShown())
    {
        const float num_col_width = ImGui::CalcTextSize("Total Count").x;
        ImGui::TableSetupColumn("Total Count", ImGuiTableColumnFlags_WidthFixed,
                                num_col_width);
        ImGui::TableSetupColumn("Issue Count", ImGuiTableColumnFlags_WidthFixed,
                                num_col_width);
        ImGui::TableSetupColumn("Stall Count", ImGuiTableColumnFlags_WidthFixed,
                                num_col_width);
    }

    if(m_show_comments)
        ImGui::TableSetupColumn("Comments", ImGuiTableColumnFlags_WidthStretch);

    ImGui::TableHeadersRow();
    PushStyles();

    ImGuiListClipper clipper;
    clipper.Begin(m_entries.size());
    while(clipper.Step())
    {
        for(uint32_t i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        {
            RenderLine(i, columns_count);
        }
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    ImGui::EndTable();
}

void
IsaCodeWidget::RenderLine(uint32_t index, uint32_t columns_count)
{
    const IsaRow& isa_row = m_entries[index];
    const bool    row_selected = isa_row.source_line_id != 0 &&
                              isa_row.source_line_id == m_line_selection.selected_line;
    const bool row_hovered = isa_row.source_line_id != 0 &&
                             isa_row.source_line_id == m_line_selection.hovered_line;

    ImGui::TableNextRow();

    if(row_selected)
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, m_selected_colour);
    else if(row_hovered && m_line_selection.hovered_line != 0)
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, m_hovered_colour);

    int column = 0;
    ImGui::TableSetColumnIndex(column);
    ImGui::PushID(static_cast<int>(isa_row.id));
    if(ImGui::Selectable("##row", row_selected,
                         ImGuiSelectableFlags_SpanAllColumns,
                         ImVec2(0.0f, ImGui::GetTextLineHeight())))
    {
        m_line_selection.selected_line = isa_row.source_line_id;
    }
    if(ImGui::IsItemHovered())
        m_line_selection.hovered_line = isa_row.source_line_id;

    ImGui::SameLine(0.0f, 0.0f);
    ImGui::PopID();

    ImGui::TextColored(m_line_num_color, "%*u", m_line_num_digits, index + 1);

    ImGui::TableSetColumnIndex(++column);
    ImGui::TextUnformatted(isa_row.instruction.c_str());

    if(IsStallShown())
    {
        ImGui::TableSetColumnIndex(++column);
        ImGui::TextDisabled("%llu", static_cast<unsigned long long>(isa_row.total_count));
        ImGui::TableSetColumnIndex(++column);
        ImGui::TextDisabled("%llu", static_cast<unsigned long long>(isa_row.issue_count));
        ImGui::TableSetColumnIndex(++column);
        ImGui::TextDisabled("%llu", static_cast<unsigned long long>(isa_row.stall_count));
    }

    if(m_show_comments)
    {
        ImGui::TableSetColumnIndex(++column);
        ImGui::TextColored(m_comment_color, "%s", ("//" + isa_row.comment).c_str());
    }
}

}  // namespace View
}  // namespace RocProfVis
