// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_isa_view.h"

#include <algorithm>
#include <utility>

#include "rocprofvis_data_provider.h"
#include "rocprofvis_events.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_requests.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_tab_container.h"
#include "spdlog/spdlog.h"

namespace RocProfVis
{
namespace View
{

constexpr const char* ISA_VIEW_DISABLED_TOOLTIP =
    "This database file has no ISA lines, so ISA View is inactive.";
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
, m_show_sampling_details(true)
{
    m_isa.widget    = std::make_shared<IsaCodeWidget>(m_line_selection);
    m_source.widget = std::make_shared<SourceCodeWidget>(m_line_selection);

    auto isa_item           = LayoutItem::CreateFromWidget(m_isa.widget);
    isa_item->m_child_flags = ImGuiChildFlags_None;

    m_source_layout_item                = LayoutItem::CreateFromWidget(m_source.widget);
    m_source_layout_item->m_child_flags = ImGuiChildFlags_None;
    m_source_layout_item->m_visible     = true;

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
    if(m_show_sampling_details) QueuePcSamplingFetch(PcSamplingLayer::kStalls);
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
    m_isa.widget->ChangeSamplingVisibility(false);
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
    state.failed           = false;
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
    {
        state.in_flight = true;
    }
    else
    {
        state.failed = true;
    }
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
        state.failed = true;
        return;
    }

    state.failed = false;
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
    RefreshCodeWidgets(layer);
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
        RefreshSourceWidget();
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
ComputeIsaView::RefreshCodeWidgets(PcSamplingLayer layer)
{
    const KernelInfo* kernel_info = m_data_provider.ComputeModel().GetKernelInfo(
        m_current_workload_id, m_current_kernel_id);
    if(!kernel_info) return;

    const PcSamplingData& data = kernel_info->pc_sampling_data;
    switch(layer)
    {
        case PcSamplingLayer::kIsa:
        {
            if(!data.code_objects.empty())
            {
                m_isa.code_object_uuid = data.code_objects[0].code_object_uuid;
            }
            m_isa.widget->Load(data, m_isa.code_object_uuid);
            break;
        }
        case PcSamplingLayer::kSource:
        {
            m_isa.widget->UpdateSourceLocations(data);
            RefreshSourceWidget();
            break;
        }
        case PcSamplingLayer::kStalls:
        {
            m_isa.widget->UpdateSampling(data);
            break;
        }
    }
    UpdateSamplingVisibility();
}

void
ComputeIsaView::RefreshSourceWidget()
{
    const KernelInfo* kernel_info = m_data_provider.ComputeModel().GetKernelInfo(
        m_current_workload_id, m_current_kernel_id);
    if(!kernel_info)
    {
        return;
    }
    if(m_source_layout_item->m_visible &&
       m_source.loaded_uuids.count(m_source.selected_uuid))
    {
        m_source.widget->Load(kernel_info->pc_sampling_data, m_source.selected_uuid);
    }
}

void
ComputeIsaView::UpdateSamplingVisibility()
{
    m_isa.widget->ChangeSamplingVisibility(
        m_show_sampling_details && m_stalls.loaded && m_isa.loaded);
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
    constexpr const char* hide_source_code_str       = "Hide Source Code";
    constexpr const char* show_source_code_str       = "Show Source Code";
    constexpr const char* show_sampling_details_str  = "Show Sampling Details";
    constexpr const char* hide_sampling_details_str  = "Hide Sampling Details";
    constexpr const char* retry_sampling_details_str = "Retry Sampling Details";

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
    const float button_sampling_details_width =
        std::max({ ImGui::CalcTextSize(show_sampling_details_str).x,
                   ImGui::CalcTextSize(hide_sampling_details_str).x,
                   ImGui::CalcTextSize(retry_sampling_details_str).x }) +
        ImGui::GetStyle().FramePadding.x * 2.0f;
    const float buttons_width = button_source_code_width +
                                button_sampling_details_width +
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
                RefreshSourceWidget();
        }
        else
        {
            m_source.queued = false;
        }
    }

    ImGui::SameLine();
    const bool retry_sampling_details = m_show_sampling_details && m_stalls.failed;
    const char* sampling_details_label =
        retry_sampling_details
            ? retry_sampling_details_str
            : (m_show_sampling_details ? hide_sampling_details_str
                                       : show_sampling_details_str);
    if(ImGui::Button(sampling_details_label))
    {
        if(retry_sampling_details)
        {
            QueuePcSamplingFetch(PcSamplingLayer::kStalls);
        }
        else
        {
            m_show_sampling_details = !m_show_sampling_details;
            if(m_show_sampling_details && !m_stalls.loaded)
            {
                QueuePcSamplingFetch(PcSamplingLayer::kStalls);
            }
            else
            {
                if(!m_show_sampling_details)
                {
                    m_stalls.queued = false;
                }
                UpdateSamplingVisibility();
            }
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

}  // namespace View
}  // namespace RocProfVis
