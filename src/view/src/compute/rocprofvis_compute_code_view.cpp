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
{
    m_source_code = std::make_shared<SourceCodeWidget>(m_line_selection);
    m_isa_code    = std::make_shared<IsaCodeWidget>(m_line_selection);

    auto source_item             = LayoutItem::CreateFromWidget(m_source_code);
    source_item->m_child_flags   = ImGuiChildFlags_None;

    m_isa_layout_item              = LayoutItem::CreateFromWidget(m_isa_code);
    m_isa_layout_item->m_child_flags = ImGuiChildFlags_None;
    m_isa_layout_item->m_visible   = false;

    m_horizontal_split_container = std::make_shared<HSplitContainer>(source_item, m_isa_layout_item);
    m_horizontal_split_container->SetSplit(0.5f);
    m_horizontal_split_container->ShowSplitter(true);

    SubscribeToEvents();

    m_data_provider.SetFetchPcSamplingCallback(
        [this](const std::string&, uint32_t kernel_id, uint32_t source_file_id, bool success) {
            OnPcSamplingReady(kernel_id, source_file_id, success);
        });
}

ComputeCodeView::~ComputeCodeView()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeKernelSelectionChanged),
        m_kernel_selection_changed_token);
}

void
ComputeCodeView::SubscribeToEvents()
{
    auto kernel_changed = [this](std::shared_ptr<RocEvent> e) {
        auto event = std::dynamic_pointer_cast<ComputeSelectionChangedEvent>(e);
        if(event && event->GetSourceId() == m_data_provider.GetTraceFilePath())
            LoadData(event->GetId());
    };
    m_kernel_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeKernelSelectionChanged), kernel_changed);
}

void
ComputeCodeView::LoadData(uint32_t kernel_id)
{
    m_current_kernel_id = kernel_id;

    for(const WorkloadInfo* workload : m_data_provider.ComputeModel().GetWorkloadList())
    {
        if(workload->kernels.count(kernel_id))
        {
            m_current_workload_id = workload->id;
            break;
        }
    }
    if(m_current_workload_id == ComputeSelection::INVALID_SELECTION_ID)
        return;

    const KernelInfo* kernel_info = m_data_provider.ComputeModel().GetKernelInfo(
        m_current_workload_id, kernel_id);
    if(!kernel_info)
        return;

    // Source file list is already populated eagerly — just refresh the selection.
    LoadSourceFileList(kernel_info->pc_sampling_data);

    // Clear stale widget data and kick off the async fetch for the selected file.
    m_source_code->Load({}, 0);
    m_isa_code->Load({}, 0);
    FetchPcSamplingForCurrentFile();
}

void
ComputeCodeView::FetchPcSamplingForCurrentFile()
{
    if(m_current_kernel_id == 0 || m_current_source_file_id == 0)
        return;
    m_data_provider.FetchPcSampling(
        PcSamplingRequestParams(m_current_workload_id, m_current_kernel_id, m_current_source_file_id));
}

void
ComputeCodeView::OnPcSamplingReady(uint32_t kernel_id, uint32_t source_file_id, bool success)
{
    if(!success || kernel_id != m_current_kernel_id || source_file_id != m_current_source_file_id)
        return;

    const KernelInfo* kernel_info = m_data_provider.ComputeModel().GetKernelInfo(
        m_current_workload_id, m_current_kernel_id);
    if(!kernel_info)
        return;

    const PcSamplingData& data = kernel_info->pc_sampling_data;

    if(m_current_source_file_id != 0)
        m_source_code->Load(data, m_current_source_file_id);

    if(!data.code_objects.empty())
        m_current_code_object_id = data.code_objects[0].id;

    if(m_current_code_object_id != 0)
        m_isa_code->Load(data, m_current_code_object_id);
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
        m_current_source_file_id = m_source_files.empty() ? 0 : m_source_files.begin()->second;
}

void
ComputeCodeView::Render()
{
    RenderControlPanel();

    ImGui::PushFont(m_settings.GetFontManager().GetFont(FontType::kCode), 0.0f);

    m_horizontal_split_container->Render();

    ImGui::PopFont();
}

void
ComputeCodeView::RenderControlPanel()
{
    const float fallbackHeight =
        ImGui::GetFrameHeight() +
        ImGui::GetStyle().WindowPadding.y * 2.0f;

    float topHeight = m_control_panel_height > 0.0f
        ? m_control_panel_height
        : fallbackHeight;

    ImGui::BeginChild("ControlPanel", ImVec2(0.0f, topHeight), true);

    ImVec2 start = ImGui::GetCursorPos();

    ImGui::BeginGroup();

    RenderSourceFileDropdown();

    const float button_isa_width      = ImGui::CalcTextSize("Hide ISA").x      + ImGui::GetStyle().FramePadding.x * 2.0f; //TODO: thing how can avoid dublication of strings
    const float button_stall_width = ImGui::CalcTextSize("Show Stalls").x + ImGui::GetStyle().FramePadding.x * 2.0f;

    const float                      button_comments_width =
        m_isa_layout_item->m_visible ?
        (ImGui::CalcTextSize("Show Comnents").x + ImGui::GetStyle().FramePadding.x * 2.0f) : 0;

    const float buttons_width = button_isa_width + button_stall_width +
                                button_comments_width +
                                ImGui::GetStyle().ItemSpacing.x * 2.0f;

    ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - buttons_width);

    if(ImGui::Button(m_isa_layout_item->m_visible ? "Hide ISA" : "Show ISA"))
        m_isa_layout_item->m_visible = !m_isa_layout_item->m_visible;

    ImGui::SameLine();
    static bool show_metadata_enabled = false;
    if(ImGui::Button(show_metadata_enabled ? "Hide Stalls" : "Show Stalls"))
    {
        show_metadata_enabled = !show_metadata_enabled;
        m_source_code->ChangeStallVisibility(show_metadata_enabled);
        m_isa_code->ChangeStallVisibility(show_metadata_enabled);
    }

    if(m_isa_layout_item->m_visible)
    {
        ImGui::SameLine();
        static bool show_comments_enabled = false;
        if(ImGui::Button(show_comments_enabled ? "Hide Comnents" : "Show Comments"))
        {
            show_comments_enabled = !show_comments_enabled;
            m_isa_code->ShowComments(show_comments_enabled);
        }
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
                FetchPcSamplingForCurrentFile();
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
BaseCodeWidget::CalcutlateLineNumberWidth(uint32_t count)
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

    // Build a map: source_line_id -> max total_sample_count from linked ISA lines
    // so we can normalise the metadata column per source line.
    std::unordered_map<uint32_t, uint32_t> samples_by_source_line_id;
    for(const auto& depend : data.isa_to_source_deps)
    {
        for(const auto& code_object : data.code_objects)
        {
            for(const auto& isa_line : code_object.isa_lines)
            {
                if(isa_line.id == depend.isa_line_id && depend.depth == 0)
                {
                    auto& stall = samples_by_source_line_id[depend.source_line_id];
                    stall = std::max(stall, isa_line.stall_record.total_sample_count);
                }
            }
        }
    }

    uint32_t max_samples = 1;
    for(const auto& kv : samples_by_source_line_id)
        max_samples = std::max(max_samples, kv.second);

    for(const auto& source_line : source_file->source_lines)
    {
        float stall = 0.0f;
        auto it = samples_by_source_line_id.find(source_line.id);
        if(it != samples_by_source_line_id.end())
            stall =
                static_cast<float>(it->second) / static_cast<float>(max_samples) * 100.0f;
        m_lines.push_back({ source_line.id, source_line.content, stall });
    }

    CalcutlateLineNumberWidth(m_lines.size());
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

    if(IsStallShown())
        ImGui::TableSetupColumn(
            "Stalls", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed,
                                ImGui::CalcTextSize("100.0%").x);

    ImGui::TableSetupColumn(
        "#", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed,
        m_line_num_width);
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

    if(IsStallShown())
    {
        ImGui::TextDisabled("%.1f%%", source_row.metadata);
        ImGui::TableSetColumnIndex(1);
    }

    ImGui::TextColored(m_line_num_color, "%*u", m_line_num_digits, display_num);

    ImGui::TableSetColumnIndex(columns_count - 1);
    ImGui::TextUnformatted(source_row.content.c_str());
}

//----------------------------------------------------------------

IsaCodeWidget::IsaCodeWidget(LineSelection& selection)
: BaseCodeWidget(selection)
{
}

void
IsaCodeWidget::Load(const PcSamplingData& data, uint32_t code_object_id)
{
    m_entries.clear();
    m_line_selection = {0, 0};

    const CodeObject* code_object = nullptr;
    for(const auto& code_obj : data.code_objects)
    {
        if(code_obj.id == code_object_id)
        {
            code_object = &code_obj;
            break;
        }
    }
    if(!code_object)
        return;

    // Build depth-0 source_line_id lookup: isa_line_id -> source_line_id
    std::unordered_map<uint32_t, uint32_t> source_by_isa;
    for(const auto& dep : data.isa_to_source_deps)
    {
        if(dep.depth == 0)
            source_by_isa.emplace(dep.isa_line_id, dep.source_line_id);
    }

    // Find max sample count across this code object's ISA lines for normalisation
    uint32_t max_samples = 1;
    for(const auto& il : code_object->isa_lines)
        max_samples = std::max(max_samples, il.stall_record.total_sample_count);

    for(const auto& il : code_object->isa_lines)
    {
        uint32_t source_line_id = 0;
        auto sit = source_by_isa.find(il.id);
        if(sit != source_by_isa.end())
            source_line_id = sit->second;

        float metadata = static_cast<float>(il.stall_record.total_sample_count) /
                         static_cast<float>(max_samples) * 100.0f;

        m_entries.push_back({il.id, source_line_id, il.instruction, il.comment, metadata});
    }

    CalcutlateLineNumberWidth(m_entries.size());
}

void
IsaCodeWidget::Render()
{
    if(m_entries.empty())
    {
        ImGui::TextDisabled("No ISA loaded");
        return;
    }

    const int columns_count = 2 + (IsStallShown() ? 1 : 0) + (m_show_comments ? 1 : 0);

    if(!ImGui::BeginTable("IsaCode", columns_count, m_table_flags))
        return;

    if(IsStallShown())
        ImGui::TableSetupColumn(
            "Stalls", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed,
                                ImGui::CalcTextSize("100.0%").x);

    ImGui::TableSetupColumn(
        "#", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed,
        m_line_num_width);
    ImGui::TableSetupColumn("ISA", ImGuiTableColumnFlags_WidthStretch);

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

    if(IsStallShown())
    {
        ImGui::TextDisabled("%.1f%%", isa_row.stall);
        ImGui::TableSetColumnIndex(++column);
    }

    ImGui::TextColored(m_line_num_color, "%*u", m_line_num_digits, index + 1);

    ImGui::TableSetColumnIndex(++column);
    ImGui::TextUnformatted(isa_row.instruction.c_str());

    if(m_show_comments)
    {
        ImGui::TableSetColumnIndex(++column);
        ImGui::TextColored(m_comment_color, "%s", ("//" + isa_row.comment).c_str());
    }
}

}  // namespace View
}  // namespace RocProfVis
