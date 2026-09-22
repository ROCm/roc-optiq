// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_isa_view.h"
#include "rocprofvis_compute_selection.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_events.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_requests.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_tab_container.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>

namespace RocProfVis
{
namespace View
{

constexpr uint64_t INVALID_SOURCE_LINE_NUMBER = 0;
constexpr uint32_t NO_SCROLL_TARGET = 0;

constexpr uint64_t LOW_CONFIDENCE_SAMPLE_COUNT = 10;

constexpr ImVec4 HEATMAP_LOW_COLOR {0.24f, 0.70f, 0.28f, 0.5f};
constexpr ImVec4 HEATMAP_MID_COLOR {0.90f, 0.78f, 0.18f, 0.5f};
constexpr ImVec4 HEATMAP_HIGH_COLOR{0.82f, 0.24f, 0.24f, 0.5f};

struct HeaderTooltipText
{
    const char* user_description;
    const char* developer_information;
};

constexpr const char* ISA_VIEW_DISABLED_TOOLTIP =
    "This database file has no ISA lines, so ISA View is inactive.";
constexpr const char* DEVELOPER_INFORMATION_LABEL = "Developer information";

constexpr HeaderTooltipText SOURCE_CODE_HEADER_TOOLTIP {
    "Source text for this line from the selected source file.",
    "DB field: compute_source_line.content\n"
    "Row key: compute_source_line.source_line_uuid\n"
    "Filter: source_file_uuid = selected source file"
};

constexpr HeaderTooltipText SAMPLES_HEADER_TOOLTIP {
    "How often PC sampling observed the GPU at this instruction.\n"
    "Larger values identify hotter instructions worth investigating.\n"
    "The bar compares each row with the hottest displayed instruction.\n"
    "Samples are observations, not elapsed time.",
    "DB field: compute_pc_sample_state.total_count\n"
    "Group key: compute_pc_sample_state.instruction_uuid\n"
    "Value: SUM(total_count) per instruction_uuid\n"
    "Bar: instruction samples / MAX(displayed instruction samples)\n"
    "Kernel share: instruction samples / SUM(kernel total_count)"
};

constexpr HeaderTooltipText ISA_INSTRUCTION_HEADER_TOOLTIP {
    "GPU machine instruction for this row.\n"
    "It shows the operation and operands from the kernel disassembly.",
    "DB field: compute_instruction_line.instruction\n"
    "Row key: compute_instruction_line.instruction_uuid\n"
    "DB filter: compute_kernel_symbol.kernel_uuid = selected kernel\n"
    "View filter: selected code object UUID"
};

constexpr HeaderTooltipText ISSUE_PERCENT_HEADER_TOOLTIP {
    "How often this instruction was issued for execution when sampled.\n"
    "Higher values mean it was usually making progress instead of waiting.",
    "DB fields: compute_pc_sample_state.issue_count, total_count\n"
    "Group key: compute_pc_sample_state.instruction_uuid\n"
    "Value: 100 * SUM(issue_count) / SUM(total_count)\n"
    "If SUM(total_count) is zero, the displayed value is 0%."
};

constexpr HeaderTooltipText STALL_PERCENT_HEADER_TOOLTIP {
    "How often this instruction was unable to issue and was waiting when sampled.\n"
    "Higher values identify where to investigate, but not the cause of the wait.",
    "DB fields: compute_pc_sample_state.stall_count, total_count\n"
    "Group key: compute_pc_sample_state.instruction_uuid\n"
    "Value: 100 * SUM(stall_count) / SUM(total_count)\n"
    "If SUM(total_count) is zero, the displayed value is 0%."
};

constexpr const char* LOW_CONFIDENCE_SAMPLES_CELL_TOOLTIP_FORMAT =
    "%s samples\n%.1f%% of kernel samples\n"
    "%.1f%% relative to the hottest instruction\n\n"
    "Low-confidence estimate: percentages based on fewer than %llu samples may be "
    "unstable.";
constexpr const char* SAMPLES_CELL_TOOLTIP_FORMAT =
    "%s samples\n%.1f%% of kernel samples\n"
    "%.1f%% relative to the hottest instruction";

namespace
{
void
RenderTableHeaderWithTooltip(int column, const char* label, const HeaderTooltipText& tooltip)
{
    ImGui::TableSetColumnIndex(column);
    ImGui::TableHeader(label);
    if(!ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
    {
        return;
    }

    BeginTooltipStyled();
    ImGui::TextUnformatted(tooltip.user_description);
//#ifdef ROCPROFVIS_DEVELOPER_MODE
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled(DEVELOPER_INFORMATION_LABEL);
    ImGui::TextUnformatted(tooltip.developer_information);
//#endif
    EndTooltipStyled();
}
}  // namespace

TabItem
ComputeIsaView::CreateTabItem(DataProvider& data_provider)
{
    return RocWidget::CreateTabItem(
        "ISA View", TAB_ID, std::make_shared<ComputeIsaView>(data_provider));
}

TabItem
ComputeIsaView::CreateTabItem(DataProvider& data_provider, bool has_isa_lines)
{
    if(has_isa_lines)
    {
        return CreateTabItem(data_provider);
    }

    TabItem tab = RocWidget::CreateTabItem("ISA View", TAB_ID, nullptr);
    tab.m_enabled          = false;
    tab.m_disabled_tooltip = ISA_VIEW_DISABLED_TOOLTIP;
    return tab;
}

ComputeIsaView::ComputeIsaView(DataProvider& data_provider)
: RocWidget()
, m_settings(SettingsManager::GetInstance())
, m_data_provider(data_provider)
, m_control_panel_height(0.0f)
, m_current_kernel_id(ComputeSelection::INVALID_SELECTION_ID)
, m_current_workload_id(ComputeSelection::INVALID_SELECTION_ID)
, m_show_metadata_enabled(true)
{
    m_isa.widget    = std::make_shared<IsaCodeWidget>(m_line_selection);
    m_source.widget = std::make_shared<SourceCodeWidget>(m_line_selection);

    auto isa_item           = LayoutItem::CreateFromWidget(m_isa.widget);
    isa_item->m_child_flags = ImGuiChildFlags_None;

    m_source_layout_item                = LayoutItem::CreateFromWidget(m_source.widget);
    m_source_layout_item->m_child_flags = ImGuiChildFlags_None;
    m_source_layout_item->m_visible     = false;

    m_horizontal_split_container =
        std::make_shared<HSplitContainer>(isa_item, m_source_layout_item);
    m_horizontal_split_container->SetSplit(0.5f);
    m_horizontal_split_container->ShowSplitter(true);

    SubscribeToEvents();

    m_data_provider.SetFetchPcSamplingCallback(
        [this](const std::string&, PcSamplingLayer layer, uint32_t kernel_id,
               uint64_t source_file_uuid, uint32_t generation,
               uint64_t request_token, rocprofvis_result_t result) {
            OnPcSamplingReady(layer, kernel_id, source_file_uuid, generation,
                              request_token, result);
        });
}

ComputeIsaView::~ComputeIsaView()
{
    m_data_provider.SetFetchPcSamplingCallback(nullptr);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeKernelSelectionChanged),
        m_kernel_selection_changed_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeWorkloadSelectionChanged),
        m_workload_selection_changed_token);
}

void
ComputeIsaView::SubscribeToEvents()
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
ComputeIsaView::SelectWorkload(uint32_t workload_id)
{
    m_current_workload_id = workload_id;
}

void
ComputeIsaView::LoadData(uint32_t kernel_id)
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
        CancelInFlightFetches();
        ClearSelectionData();
        return;
    }

    const KernelInfo* kernel_info = m_data_provider.ComputeModel().GetKernelInfo(
        m_current_workload_id, kernel_id);
    if(!kernel_info)
    {
        m_current_workload_id = ComputeSelection::INVALID_SELECTION_ID;
        CancelInFlightFetches();
        ClearSelectionData();
        return;
    }
    if(!kernel_info->has_isa_lines)
    {
        CancelInFlightFetches();
        ClearSelectionData();
        return;
    }

    CancelInFlightFetches();
    ClearSelectionData();
    ++m_fetch_generation;
    QueuePcSamplingFetch(PcSamplingLayer::kIsa);
    if(m_source_layout_item->m_visible)
        QueuePcSamplingFetch(PcSamplingLayer::kSource);
    if(m_show_metadata_enabled) QueuePcSamplingFetch(PcSamplingLayer::kStalls);
}

void
ComputeIsaView::ClearCodeData()
{
    m_source.widget->Load({}, 0);
    m_isa.widget->Load({}, 0);
}

void
ComputeIsaView::ClearSelectionData()
{
    m_isa.ResetFetch();
    m_source.ResetFetch();
    m_stalls = {};
    m_line_selection = {};
    m_source.widget->ChangeStallVisibility(false);
    m_isa.widget->ChangeStallVisibility(false);
    ClearCodeData();
}

FetchStateType&
ComputeIsaView::FetchStateFor(PcSamplingLayer layer)
{
    switch(layer)
    {
        case PcSamplingLayer::kIsa:    return m_isa;
        case PcSamplingLayer::kSource: return m_source;
        case PcSamplingLayer::kStalls: return m_stalls;
    }
    spdlog::error("FetchStateFor: unhandled PcSamplingLayer value {}",
                  static_cast<uint32_t>(layer));
    ROCPROFVIS_ASSERT(false);
    return m_isa;
}

void
ComputeIsaView::QueuePcSamplingFetch(PcSamplingLayer layer)
{
    FetchStateType& state = FetchStateFor(layer);
    state.queued           = true;
    state.request_token    = ++m_next_request_token;
}

void
ComputeIsaView::ClearPendingPcSamplingFetches()
{
    m_isa.queued    = false;
    m_source.queued = false;
    m_stalls.queued = false;
}

bool
ComputeIsaView::HasValidPcSamplingSelection() const
{
    return m_current_kernel_id != ComputeSelection::INVALID_SELECTION_ID &&
           m_current_workload_id != ComputeSelection::INVALID_SELECTION_ID;
}

void
ComputeIsaView::CancelInFlightFetches()
{
    if(m_isa.in_flight)
        m_data_provider.CancelRequest(DataProvider::FETCH_PC_SAMPLING_ISA_REQUEST_ID);
    if(m_source.in_flight)
        m_data_provider.CancelRequest(DataProvider::FETCH_PC_SAMPLING_SOURCE_REQUEST_ID);
    if(m_stalls.in_flight)
        m_data_provider.CancelRequest(DataProvider::FETCH_PC_SAMPLING_STALLS_REQUEST_ID);
}

bool
ComputeIsaView::TryTakeNextPendingPcSamplingFetch(PcSamplingLayer& layer)
{
    if(std::exchange(m_isa.queued, false))
    {
        layer = PcSamplingLayer::kIsa;
        return true;
    }
    if(std::exchange(m_source.queued, false))
    {
        layer = PcSamplingLayer::kSource;
        return true;
    }
    if(std::exchange(m_stalls.queued, false))
    {
        layer = PcSamplingLayer::kStalls;
        return true;
    }
    return false;
}

void
ComputeIsaView::SubmitPcSamplingFetch(PcSamplingLayer layer)
{
    FetchStateType& state = FetchStateFor(layer);
    const uint64_t source_file_uuid =
        layer == PcSamplingLayer::kSource ? m_source.selected_uuid : 0;
    const PcSamplingRequestParams params(layer, m_current_workload_id,
                                          m_current_kernel_id, source_file_uuid,
                                          m_fetch_generation, state.request_token);
    if(m_data_provider.FetchPcSampling(params))
        state.in_flight = true;
    else
        QueuePcSamplingFetch(layer);
}

void
ComputeIsaView::FetchPendingPcSampling()
{
    if(!HasValidPcSamplingSelection())
    {
        ClearPendingPcSamplingFetches();
        CancelInFlightFetches();
        return;
    }

    PcSamplingLayer layer = PcSamplingLayer::kIsa;
    while(TryTakeNextPendingPcSamplingFetch(layer))
        SubmitPcSamplingFetch(layer);
}

void
ComputeIsaView::OnPcSamplingReady(PcSamplingLayer layer, uint32_t kernel_id,
                                   uint64_t source_file_uuid, uint32_t generation,
                                   uint64_t request_token, rocprofvis_result_t result)
{
    if(generation != m_fetch_generation)
        return;

    FetchStateType& state = FetchStateFor(layer);
    if(request_token != state.request_token)
        return;
    if(kernel_id != m_current_kernel_id ||
       (layer == PcSamplingLayer::kSource && m_source.selected_uuid != 0 &&
        source_file_uuid != m_source.selected_uuid))
        return;
    if(!state.in_flight) return;
    state.in_flight = false;

    if(result == kRocProfVisResultCancelled)
        return;

    if(result != kRocProfVisResultSuccess)
    {
        if(layer == PcSamplingLayer::kStalls) m_show_metadata_enabled = false;
        return;
    }

    const KernelInfo* kernel_info = m_data_provider.ComputeModel().GetKernelInfo(
        m_current_workload_id, m_current_kernel_id);
    if(!kernel_info)
        return;

    state.loaded = true;
    if(layer == PcSamplingLayer::kSource)
    {
        m_source.selected_uuid = source_file_uuid;
        LoadSourceFileList(kernel_info->pc_sampling_data);
        if(m_source.selected_uuid != 0)
            m_source.loaded_uuids.insert(m_source.selected_uuid);
    }
    RefreshCodeWidgets();
}

void
ComputeIsaView::LoadSourceFileList(const PcSamplingData& data)
{
    m_source.file_uuid_by_path.clear();
    for(auto& file : data.source_files)
        m_source.file_uuid_by_path.emplace(file.file_path, file.source_file_uuid);

    bool selection_valid = false;
    for(const auto& [path, id] : m_source.file_uuid_by_path)
    {
        if(id == m_source.selected_uuid)
        {
            selection_valid = true;
            break;
        }
    }
    if(!selection_valid)
        m_source.selected_uuid = m_source.file_uuid_by_path.empty()
                                     ? 0
                                     : m_source.file_uuid_by_path.begin()->second;
}

void
ComputeIsaView::SelectSourceFile(uint64_t source_file_uuid)
{
    if(source_file_uuid == m_source.selected_uuid) return;

    m_source.selected_uuid = source_file_uuid;
    m_source.widget->Load({}, 0);
    if(m_source.loaded_uuids.count(source_file_uuid))
    {
        m_source.queued        = false;
        m_source.request_token = ++m_next_request_token;
        if(m_source.in_flight)
        {
            m_data_provider.CancelRequest(
                DataProvider::FETCH_PC_SAMPLING_SOURCE_REQUEST_ID);
            m_source.in_flight = false;
        }
        RefreshCodeWidgets();
    }
    else
        QueuePcSamplingFetch(PcSamplingLayer::kSource);
}

void
ComputeIsaView::SelectSourceFileForScroll()
{
    const uint64_t source_file_uuid = m_line_selection.source_scroll_file;
    if(source_file_uuid == LineSelection::UNSELECTED) return;

    m_line_selection.source_scroll_file = LineSelection::UNSELECTED;
    const bool source_file_exists = std::any_of(
        m_source.file_uuid_by_path.begin(), m_source.file_uuid_by_path.end(),
        [source_file_uuid](const auto& file) { return file.second == source_file_uuid; });
    if(!source_file_exists)
    {
        m_line_selection.source_scroll_line = LineSelection::UNSELECTED;
        return;
    }

    m_source_layout_item->m_visible = true;
    SelectSourceFile(source_file_uuid);
}

void
ComputeIsaView::RefreshCodeWidgets()
{
    const KernelInfo* kernel_info = m_data_provider.ComputeModel().GetKernelInfo(
        m_current_workload_id, m_current_kernel_id);
    if(!kernel_info) return;

    const PcSamplingData& data = kernel_info->pc_sampling_data;
    if(m_isa.loaded && !data.code_objects.empty())
        m_isa.code_object_uuid = data.code_objects[0].code_object_uuid;
    if(m_isa.loaded &&
       m_isa.code_object_uuid != ComputeSelection::INVALID_SELECTION_ID)
        m_isa.widget->Load(data, m_isa.code_object_uuid);

    if(m_source_layout_item->m_visible &&
       m_source.loaded_uuids.count(m_source.selected_uuid))
        m_source.widget->Load(data, m_source.selected_uuid);

    const bool show_stalls = m_show_metadata_enabled && m_stalls.loaded;
    m_source.widget->ChangeStallVisibility(show_stalls);
    m_isa.widget->ChangeStallVisibility(show_stalls);
}

void
ComputeIsaView::Update()
{
    SelectSourceFileForScroll();
    FetchPendingPcSampling();
}

void
ComputeIsaView::Render()
{
    RenderControlPanel();

    ImGui::PushFont(m_settings.GetFontManager().GetFont(FontType::kCode), 0.0f);

    m_line_selection.hovered_this_frame = false;
    m_horizontal_split_container->Render();
    if(!m_line_selection.hovered_this_frame)
        m_line_selection.hovered_line = LineSelection::UNSELECTED;

    ImGui::PopFont();
}

void
ComputeIsaView::RenderControlPanel()
{
    constexpr const char* hide_source_code_str = "Hide Source Code";
    constexpr const char* show_source_code_str = "Show Source Code";
    constexpr const char* show_stalls_str      = "Show Sampling Details";
    constexpr const char* hide_stalls_str      = "Hide Sampling Details";

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
    const float buttons_width = button_source_code_width + button_stall_width +
                                ImGui::GetStyle().ItemSpacing.x;

    ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() -
                    buttons_width);

    if(ImGui::Button(m_source_layout_item->m_visible ? hide_source_code_str
                                                     : show_source_code_str))
    {
        m_source_layout_item->m_visible = !m_source_layout_item->m_visible;
        if(m_source_layout_item->m_visible)
        {
            if(!m_source.loaded_uuids.count(m_source.selected_uuid))
                QueuePcSamplingFetch(PcSamplingLayer::kSource);
            else
                RefreshCodeWidgets();
        }
        else
        {
            m_source.queued = false;
        }
    }

    ImGui::SameLine();
    if(ImGui::Button(m_show_metadata_enabled ? hide_stalls_str : show_stalls_str))
    {
        m_show_metadata_enabled = !m_show_metadata_enabled;
        if(m_show_metadata_enabled && !m_stalls.loaded)
            QueuePcSamplingFetch(PcSamplingLayer::kStalls);
        else
        {
            if(!m_show_metadata_enabled) m_stalls.queued = false;
            RefreshCodeWidgets();
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
ComputeIsaView::RenderSourceFileDropdown()
{
    constexpr const float DROPDOWN_SIZE = 300.0f;
    if(!m_source_layout_item->m_visible || m_source.file_uuid_by_path.empty()) return;

    auto filename_of = [](const std::string& str) -> const char* {
        const auto pos = str.find_last_of("/\\");
        return pos == std::string::npos ? str.c_str() : str.c_str() + pos + 1;
    };

    const auto selected_file_it =
        std::find_if(m_source.file_uuid_by_path.begin(), m_source.file_uuid_by_path.end(),
                     [this](const auto& pair) {
                         return pair.second == m_source.selected_uuid;
                     });

    const char* preview = selected_file_it != m_source.file_uuid_by_path.end()
                              ? filename_of(selected_file_it->first)
                              : "<none>";

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Source file:");
    ImGui::SameLine();

    ImGui::SetNextItemWidth(DROPDOWN_SIZE);
    if(ImGui::BeginCombo("##source_file", preview))
    {
        for(const auto& [path, id] : m_source.file_uuid_by_path)
        {
            const bool selected = (id == m_source.selected_uuid);
            if(ImGui::Selectable(filename_of(path), selected) && !selected)
            {
                m_line_selection = {};
                SelectSourceFile(id);
            }
            if(selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

BaseCodeWidget::BaseCodeWidget(LineSelection& selection)
: m_line_selection(selection)
, m_settings(SettingsManager::GetInstance())
{
    m_line_num_color = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);

    m_table_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_NoPadOuterX |
        ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
}

void
BaseCodeWidget::CalculateLineNumberWidth(size_t count)
{
    m_line_num_digits = 1;
    for(size_t number = count; number >= 10; number /= 10)
        m_line_num_digits++;

    m_line_num_width =
        ImGui::CalcTextSize("0").x * static_cast<float>(m_line_num_digits + 1);
}

void
BaseCodeWidget::PushStyles()
{
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,
                        ImVec2(ImGui::GetStyle().CellPadding.x, 0.0f));

    ImGui::PushStyleColor(ImGuiCol_Header, m_settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                          m_settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                          m_settings.GetColor(Colors::kTransparent));
}

SourceCodeWidget::SourceCodeWidget(LineSelection& selection)
: BaseCodeWidget(selection)
{
}

void
SourceCodeWidget::Load(const PcSamplingData& data, uint64_t source_file_uuid)
{
    m_lines.clear();

    const SourceFile* source_file = nullptr;
    for(const auto& file : data.source_files)
    {
        if(file.source_file_uuid == source_file_uuid)
        {
            source_file = &file;
            break;
        }
    }
    if(!source_file)
        return;

    uint64_t max_line_number = 0;
    for(const auto& source_line : source_file->source_lines)
    {
        if(source_line.line_number == INVALID_SOURCE_LINE_NUMBER)
        {
            continue;
        }

        m_lines.push_back({ source_line.content, source_line.source_line_uuid,
                            source_line.line_number });
        max_line_number = std::max(max_line_number, source_line.line_number);
    }

    CalculateLineNumberWidth(static_cast<size_t>(max_line_number));
}

void
SourceCodeWidget::Render()
{
    if(m_lines.empty())
    {
        ImGui::TextDisabled("No file loaded");
        return;
    }

    const int columns_count = 2;

    if(!ImGui::BeginTable("SourceCode", columns_count, m_table_flags))
        return;

    ImGui::TableSetupScrollFreeze(0, 1);

    ImGui::TableSetupColumn(
        "#", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed,
        m_line_num_width);

    ImGui::TableSetupColumn("Source code", ImGuiTableColumnFlags_WidthStretch);

    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
    ImGui::TableSetColumnIndex(0);
    ImGui::TableHeader("#");
    RenderTableHeaderWithTooltip(1, "Source code", SOURCE_CODE_HEADER_TOOLTIP);
    PushStyles();

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(m_lines.size()));
    const uint32_t scroll_target = GetScrollTarget(clipper);

    while(clipper.Step())
    {
        for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        {
            RenderLine(static_cast<uint32_t>(i));
            if(scroll_target != NO_SCROLL_TARGET &&
               static_cast<uint32_t>(i) + 1 == scroll_target)
                ImGui::SetScrollHereY(0.0f);
        }
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    ImGui::EndTable();
}

uint32_t
SourceCodeWidget::GetScrollTarget(ImGuiListClipper& clipper)
{
    uint32_t scroll_target = NO_SCROLL_TARGET;
    if(m_line_selection.source_scroll_line != LineSelection::UNSELECTED)
    {
        for(uint32_t i = 0; i < m_lines.size(); ++i)
        {
            if(m_lines[i].id == m_line_selection.source_scroll_line)
            {
                scroll_target = i + 1;
                m_line_selection.source_scroll_line = LineSelection::UNSELECTED;
                clipper.IncludeItemByIndex(static_cast<int>(i));
                break;
            }
        }
    }
    return scroll_target;
}

void
SourceCodeWidget::RenderLine(uint32_t index)
{
    const SourceRow& source_row  = m_lines[index];
    const uint64_t   display_num = source_row.line_number;
    const bool row_selected = source_row.id != 0 &&
                              source_row.id == m_line_selection.selected_line;
    const bool row_hovered = source_row.id != 0 &&
                             source_row.id == m_line_selection.hovered_line;

    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    ImGui::PushID(static_cast<int>(source_row.id));
    if(ImGui::Selectable("##row", row_selected,
                         ImGuiSelectableFlags_SpanAllColumns,
                         ImVec2(0.0f, ImGui::GetTextLineHeight())))
    {
        m_line_selection.selected_line = source_row.id;
        m_line_selection.isa_scroll_line = source_row.id;
    }
    const bool item_hovered = ImGui::IsItemHovered();
    if(item_hovered)
    {
        m_line_selection.hovered_line       = source_row.id;
        m_line_selection.hovered_this_frame = true;
    }

    if(row_selected)
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                               m_settings.GetColor(Colors::kSelection));
    else if(item_hovered || row_hovered)
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                               m_settings.GetColor(Colors::kHighlightChart));

    ImGui::SameLine(0.0f, 0.0f);
    ImGui::PopID();

    ImGui::TextColored(m_line_num_color, "%*llu", static_cast<int>(m_line_num_digits),
                       static_cast<unsigned long long>(display_num));

    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted(source_row.content.c_str());
}

IsaCodeWidget::IsaCodeWidget(LineSelection& selection)
: BaseCodeWidget(selection)
{
}

void
IsaCodeWidget::Load(const PcSamplingData& data, uint64_t code_object_uuid)
{
    m_entries.clear();
    m_kernel_total_samples        = 0;
    m_hottest_instruction_samples = 0;

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

    struct SourceLocation
    {
        uint64_t source_line_id = 0;
        uint64_t source_file_id = 0;
    };
    std::unordered_map<uint64_t, SourceLocation> source_by_isa;
    for(const InstructionSourceLine& dep : data.instruction_source_lines)
    {
        if(dep.frame_index == 0)
            source_by_isa.emplace(
                dep.instruction_uuid,
                SourceLocation{ dep.source_line_uuid, dep.source_file_uuid });
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
        m_kernel_total_samples += state.total_count;
    }

    for(const KernelSymbol& kernel_symbol : code_object->kernel_symbols)
    {
        for(const InstructionLine& instruction_line : kernel_symbol.instruction_lines)
        {
            uint64_t source_line_id = 0;
            uint64_t source_file_id = 0;
            if(const auto sit = source_by_isa.find(instruction_line.instruction_uuid);
               sit != source_by_isa.end())
            {
                source_line_id = sit->second.source_line_id;
                source_file_id = sit->second.source_file_id;
            }

            const InstructionSampleCounts* counts = nullptr;
            if(const auto counts_it = counts_by_instruction.find(instruction_line.instruction_uuid);
               counts_it != counts_by_instruction.end())
                counts = &counts_it->second;

            m_entries.push_back({ instruction_line.instruction,
                                  instruction_line.instruction_uuid, source_line_id,
                                  source_file_id,
                                  counts ? counts->issue_count : 0,
                                  counts ? counts->stall_count : 0,
                                  counts ? counts->total_count : 0 });
        }
    }

    for(const IsaRow& row : m_entries)
    {
        m_hottest_instruction_samples =
            std::max(m_hottest_instruction_samples, row.total_count);
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

    const int sampling_detail_columns = IsStallShown() ? 3 : 0;
    const int columns_count            = 2 + sampling_detail_columns;

    if(!ImGui::BeginTable("IsaCode", columns_count, m_table_flags))
        return;

    ImGui::TableSetupScrollFreeze(0, 1);

    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, m_line_num_width);

    if(IsStallShown())
    {
        std::string widest_sample_count_text =
            FormatSampleCount(m_hottest_instruction_samples);
        if(m_hottest_instruction_samples < LOW_CONFIDENCE_SAMPLE_COUNT)
        {
            widest_sample_count_text += "*";
        }

        const float samples_header_width = ImGui::CalcTextSize("Samples").x;
        const float sample_count_width =
            ImGui::CalcTextSize(widest_sample_count_text.c_str()).x;
        const float samples_column_width =
            std::max(samples_header_width, sample_count_width);
        ImGui::TableSetupColumn("Samples", ImGuiTableColumnFlags_WidthFixed,
                                samples_column_width);
    }

    ImGui::TableSetupColumn("ISA", ImGuiTableColumnFlags_WidthStretch);

    if(IsStallShown())
    {
        const float percentage_column_width = ImGui::CalcTextSize("Issue %").x;
        ImGui::TableSetupColumn("Issue %", ImGuiTableColumnFlags_WidthFixed,
                                percentage_column_width);
        ImGui::TableSetupColumn("Stall %", ImGuiTableColumnFlags_WidthFixed,
                                percentage_column_width);
    }

    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
    int header_column = 0;
    ImGui::TableSetColumnIndex(header_column++);
    ImGui::TableHeader("#");
    if(IsStallShown())
    {
        RenderTableHeaderWithTooltip(header_column++, "Samples", SAMPLES_HEADER_TOOLTIP);
    }
    RenderTableHeaderWithTooltip(header_column++, "ISA", ISA_INSTRUCTION_HEADER_TOOLTIP);
    if(IsStallShown())
    {
        RenderTableHeaderWithTooltip(header_column++, "Issue %",
                                     ISSUE_PERCENT_HEADER_TOOLTIP);
        RenderTableHeaderWithTooltip(header_column, "Stall %",
                                     STALL_PERCENT_HEADER_TOOLTIP);
    }
    PushStyles();

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(m_entries.size()));
    const uint32_t scroll_target = GetScrollTarget(clipper);
    while(clipper.Step())
    {
        for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        {
            RenderLine(static_cast<uint32_t>(i));
            if(scroll_target != NO_SCROLL_TARGET &&
               static_cast<uint32_t>(i) + 1 == scroll_target)
                ImGui::SetScrollHereY(0.0f);
        }
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    ImGui::EndTable();
}

uint32_t
IsaCodeWidget::GetScrollTarget(ImGuiListClipper& clipper)
{
    uint32_t scroll_target = NO_SCROLL_TARGET;
    if(m_line_selection.isa_scroll_line != LineSelection::UNSELECTED)
    {
        for(uint32_t i = 0; i < m_entries.size(); ++i)
        {
            if(m_entries[i].source_line_id == m_line_selection.isa_scroll_line)
            {
                scroll_target = i + 1;
                m_line_selection.isa_scroll_line = LineSelection::UNSELECTED;
                clipper.IncludeItemByIndex(static_cast<int>(i));
                break;
            }
        }
    }
    return scroll_target;
}

double
IsaCodeWidget::CalculatePercentage(uint64_t value, uint64_t total)
{
    return total > 0 ? static_cast<double>(value) / static_cast<double>(total) * 100.0
                     : 0.0;
}

ImU32
IsaCodeWidget::HeatmapColor(double percent)
{
    const float fraction = std::clamp(static_cast<float>(percent) / 100.0f, 0.0f, 1.0f);

    const auto interpolate_color = [](const ImVec4& from, const ImVec4& to, float amount) {
        return ImVec4(from.x + (to.x - from.x) * amount, from.y + (to.y - from.y) * amount,
                      from.z + (to.z - from.z) * amount, from.w + (to.w - from.w) * amount);
    };

    constexpr float COLOR_MIDPOINT = 0.5f;
    const ImVec4 color =
        fraction < COLOR_MIDPOINT
            ? interpolate_color(HEATMAP_LOW_COLOR, HEATMAP_MID_COLOR,
                                fraction / COLOR_MIDPOINT)
            : interpolate_color(HEATMAP_MID_COLOR, HEATMAP_HIGH_COLOR,
                                (fraction - COLOR_MIDPOINT) / COLOR_MIDPOINT);
    return ImGui::ColorConvertFloat4ToU32(color);
}

std::string
IsaCodeWidget::FormatSampleCount(uint64_t value)
{
    const std::string digits = std::to_string(value);
    std::string       formatted_count;
    formatted_count.reserve(digits.size() + digits.size() / 3);

    int digits_since_separator = 0;
    for(auto it = digits.rbegin(); it != digits.rend(); ++it)
    {
        if(digits_since_separator == 3)
        {
            formatted_count.push_back(',');
            digits_since_separator = 0;
        }
        formatted_count.push_back(*it);
        ++digits_since_separator;
    }
    std::reverse(formatted_count.begin(), formatted_count.end());
    return formatted_count;
}

void
IsaCodeWidget::RenderSamplesCell(uint64_t sample_count)
{
    const double kernel_sample_share =
        CalculatePercentage(sample_count, m_kernel_total_samples);
    const double relative_hotness =
        CalculatePercentage(sample_count, m_hottest_instruction_samples);
    const float  fill_fraction =
        std::clamp(static_cast<float>(relative_hotness) / 100.0f, 0.0f, 1.0f);
    const ImVec2 cell_start = ImGui::GetCursorScreenPos();
    const float  cell_width = std::max(0.0f, ImGui::GetContentRegionAvail().x);
    const float  cell_height = ImGui::GetTextLineHeightWithSpacing();
    const ImVec2 cell_end(cell_start.x + cell_width, cell_start.y + cell_height);
    if(fill_fraction > 0.0f)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const ImVec2 bar_end(cell_start.x + cell_width * fill_fraction, cell_end.y);
        draw_list->PushClipRect(cell_start, cell_end, true);
        draw_list->AddRectFilled(cell_start, bar_end, HeatmapColor(relative_hotness));
        draw_list->PopClipRect();
    }

    const std::string count_text = FormatSampleCount(sample_count);
    const std::string display_text =
        sample_count < LOW_CONFIDENCE_SAMPLE_COUNT ? count_text + "*" : count_text;
    const float text_width = ImGui::CalcTextSize(display_text.c_str()).x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, cell_width - text_width));
    ImGui::TextUnformatted(display_text.c_str());

    if(ImGui::IsMouseHoveringRect(cell_start, cell_end))
    {
        if(sample_count < LOW_CONFIDENCE_SAMPLE_COUNT)
        {
            SetTooltipStyled(LOW_CONFIDENCE_SAMPLES_CELL_TOOLTIP_FORMAT,
                             count_text.c_str(), kernel_sample_share, relative_hotness,
                             static_cast<unsigned long long>(LOW_CONFIDENCE_SAMPLE_COUNT));
        }
        else
        {
            SetTooltipStyled(SAMPLES_CELL_TOOLTIP_FORMAT,
                             count_text.c_str(), kernel_sample_share, relative_hotness);
        }
    }
}

void
IsaCodeWidget::RenderPercentBarCell(double percent)
{
    const float fraction = std::clamp(static_cast<float>(percent) / 100.0f, 0.0f, 1.0f);
    if(fraction > 0.0f)
    {
        const ImVec2 cell_start = ImGui::GetCursorScreenPos();
        const float  cell_width = ImGui::GetContentRegionAvail().x;
        const float  cell_height = ImGui::GetTextLineHeightWithSpacing();
        const ImVec2 cell_end(cell_start.x + cell_width, cell_start.y + cell_height);
        const ImVec2 bar_end(cell_start.x + cell_width * fraction, cell_end.y);

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->PushClipRect(cell_start, cell_end, true);
        draw_list->AddRectFilled(cell_start, bar_end, HeatmapColor(percent));
        draw_list->PopClipRect();
    }
    ImGui::TextDisabled("%.1f%%", percent);
}

void
IsaCodeWidget::RenderLine(uint32_t index)
{
    const IsaRow& isa_row = m_entries[index];
    const bool    row_selected = isa_row.source_line_id != 0 &&
                              isa_row.source_line_id == m_line_selection.selected_line;
    const bool row_hovered = isa_row.source_line_id != 0 &&
                             isa_row.source_line_id == m_line_selection.hovered_line;

    ImGui::TableNextRow();

    int column = 0;
    ImGui::TableSetColumnIndex(column);
    ImGui::PushID(static_cast<int>(isa_row.id));
    if(ImGui::Selectable("##row", row_selected, ImGuiSelectableFlags_SpanAllColumns,
                         ImVec2(0.0f, ImGui::GetTextLineHeight())))
    {
        if(isa_row.source_line_id != LineSelection::UNSELECTED)
        {
            m_line_selection.selected_line = isa_row.source_line_id;
            m_line_selection.source_scroll_line = isa_row.source_line_id;
            m_line_selection.source_scroll_file = isa_row.source_file_id;
        }
    }
    const bool item_hovered = ImGui::IsItemHovered();
    if(item_hovered)
    {
        m_line_selection.hovered_line       = isa_row.source_line_id;
        m_line_selection.hovered_this_frame = true;
    }

    if(row_selected)
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                               m_settings.GetColor(Colors::kSelection));
    else if(item_hovered || row_hovered)
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                               m_settings.GetColor(Colors::kHighlightChart));

    ImGui::SameLine(0.0f, 0.0f);
    ImGui::PopID();

    ImGui::TextColored(m_line_num_color, "%*u", static_cast<int>(m_line_num_digits), index + 1);

    if(IsStallShown())
    {
        ImGui::TableSetColumnIndex(++column);
        RenderSamplesCell(isa_row.total_count);
    }

    ImGui::TableSetColumnIndex(++column);
    ImGui::TextUnformatted(isa_row.instruction.c_str());

    if(IsStallShown())
    {
        ImGui::TableSetColumnIndex(++column);
        RenderPercentBarCell(
            CalculatePercentage(isa_row.issue_count, isa_row.total_count));
        ImGui::TableSetColumnIndex(++column);
        RenderPercentBarCell(
            CalculatePercentage(isa_row.stall_count, isa_row.total_count));
    }

}

}  // namespace View
}  // namespace RocProfVis
