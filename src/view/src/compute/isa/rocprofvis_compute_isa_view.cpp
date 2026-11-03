// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_isa_view.h"

#include <algorithm>
#include <utility>

#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_events.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_requests.h"
#include "spdlog/spdlog.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_notification_manager.h"
#include "widgets/rocprofvis_tab_container.h"

namespace RocProfVis
{
namespace View
{

constexpr PcSamplingLayer PC_SAMPLING_LAYERS[] = { PcSamplingLayer::kIsa,
                                                   PcSamplingLayer::kSource,
                                                   PcSamplingLayer::kStalls };

static uint64_t
pc_sampling_request_id(PcSamplingLayer layer)
{
    switch(layer)
    {
        case PcSamplingLayer::kIsa: return DataProvider::FETCH_PC_SAMPLING_ISA_REQUEST_ID;
        case PcSamplingLayer::kSource:
            return DataProvider::FETCH_PC_SAMPLING_SOURCE_REQUEST_ID;
        case PcSamplingLayer::kStalls:
            return DataProvider::FETCH_PC_SAMPLING_STALLS_REQUEST_ID;
    }
    ROCPROFVIS_ASSERT(false);
    return DataProvider::FETCH_PC_SAMPLING_ISA_REQUEST_ID;
}

constexpr const char* ISA_VIEW_DISABLED_TOOLTIP =
    "This database file has no ISA lines, so ISA View is inactive.";

TabItem
ComputeIsaView::CreateTabItem(DataProvider& data_provider)
{
    return RocWidget::CreateTabItem("ISA View", TAB_ID,
                                    std::make_shared<ComputeIsaView>(data_provider));
}

TabItem
ComputeIsaView::CreateTabItem(DataProvider& data_provider, bool has_isa_lines)
{
    if(has_isa_lines)
    {
        return CreateTabItem(data_provider);
    }

    TabItem tab            = RocWidget::CreateTabItem("ISA View", TAB_ID, nullptr);
    tab.m_enabled          = false;
    tab.m_disabled_tooltip = ISA_VIEW_DISABLED_TOOLTIP;
    return tab;
}

ComputeIsaView::ComputeIsaView(DataProvider& data_provider)
: RocWidget()
, m_settings(SettingsManager::GetInstance())
, m_data_provider(data_provider)
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
               uint64_t source_file_uuid, uint32_t generation, uint64_t request_token,
               rocprofvis_result_t result) {
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
        {
            SelectWorkload(event->GetId());
        }
    };
    m_workload_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeWorkloadSelectionChanged), workload_changed);

    auto kernel_changed = [this](std::shared_ptr<RocEvent> e) {
        auto event = std::dynamic_pointer_cast<ComputeSelectionChangedEvent>(e);
        if(event && event->GetSourceId() == m_data_provider.GetTraceFilePath())
        {
            LoadData(event->GetId());
        }
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
        for(const WorkloadInfo* candidate :
            m_data_provider.ComputeModel().GetWorkloadList())
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

    const KernelInfo* kernel_info =
        m_data_provider.ComputeModel().GetKernelInfo(m_current_workload_id, kernel_id);
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
    {
        QueuePcSamplingFetch(PcSamplingLayer::kSource);
    }
    QueuePcSamplingFetch(PcSamplingLayer::kStalls);
}

void
ComputeIsaView::ClearCodeData()
{
    m_source.widget->Load({}, 0, m_sampling_data);
    m_isa.widget->Load({}, 0, m_sampling_data);
}

void
ComputeIsaView::ClearSelectionData()
{
    m_isa.Reset();
    m_source.Reset();
    m_stalls         = {};
    m_sampling_data  = {};
    m_sample_counts_loaded = false;
    m_line_selection = {};
    ClearCodeData();
}

ComputeIsaView::PcSamplingFetchState&
ComputeIsaView::FetchStateFor(PcSamplingLayer layer)
{
    switch(layer)
    {
        case PcSamplingLayer::kIsa: return m_isa.fetch;
        case PcSamplingLayer::kSource: return m_source.fetch;
        case PcSamplingLayer::kStalls: return m_stalls;
    }
    spdlog::error("FetchStateFor: unhandled PcSamplingLayer value {}",
                  static_cast<uint32_t>(layer));
    ROCPROFVIS_ASSERT(false);
    return m_isa.fetch;
}

void
ComputeIsaView::QueuePcSamplingFetch(PcSamplingLayer layer)
{
    PcSamplingFetchState& state = FetchStateFor(layer);
    state.queued                = true;
    state.request_token         = ++m_next_request_token;
    state.status                = CodeLoadState::kLoading;
    UpdateWidgetLoadStates();
}

void
ComputeIsaView::ClearPendingPcSamplingFetches()
{
    for(PcSamplingLayer layer : PC_SAMPLING_LAYERS)
    {
        FetchStateFor(layer).queued = false;
    }
}

bool
ComputeIsaView::HasValidPcSamplingSelection() const
{
    return m_current_kernel_id != ComputeSelection::INVALID_SELECTION_ID &&
           m_current_workload_id != ComputeSelection::INVALID_SELECTION_ID;
}

void
ComputeIsaView::CancelPcSamplingFetch(PcSamplingLayer layer)
{
    PcSamplingFetchState& state = FetchStateFor(layer);
    // Reject callbacks before asking the provider to cancel the active request.
    if(std::exchange(state.in_flight, false))
    {
        m_data_provider.CancelRequest(pc_sampling_request_id(layer));
    }
}

void
ComputeIsaView::CancelInFlightFetches()
{
    for(PcSamplingLayer layer : PC_SAMPLING_LAYERS)
    {
        CancelPcSamplingFetch(layer);
    }
}

void
ComputeIsaView::SubmitPcSamplingFetch(PcSamplingLayer layer)
{
    PcSamplingFetchState& state = FetchStateFor(layer);
    const uint64_t        source_file_uuid =
        layer == PcSamplingLayer::kSource ? m_source.selected_uuid : 0;
    const PcSamplingRequestParams params(layer, m_current_workload_id,
                                         m_current_kernel_id, source_file_uuid,
                                         m_fetch_generation, state.request_token);
    state.in_flight = m_data_provider.FetchPcSampling(params);
    if(!state.in_flight)
    {
        state.status = CodeLoadState::kFailed;
        if(layer == PcSamplingLayer::kStalls)
        {
            RefreshCodeWidgets(layer);
        }
        NotifyLoadFailure(layer);
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

    for(PcSamplingLayer layer : PC_SAMPLING_LAYERS)
    {
        if(std::exchange(FetchStateFor(layer).queued, false))
        {
            SubmitPcSamplingFetch(layer);
        }
    }
}

void
ComputeIsaView::OnPcSamplingReady(PcSamplingLayer layer, uint32_t kernel_id,
                                  uint64_t source_file_uuid, uint32_t generation,
                                  uint64_t request_token, rocprofvis_result_t result)
{
    if(generation != m_fetch_generation)
    {
        return;
    }

    PcSamplingFetchState& state = FetchStateFor(layer);
    if(request_token != state.request_token)
    {
        return;
    }
    if(kernel_id != m_current_kernel_id ||
       (layer == PcSamplingLayer::kSource && m_source.selected_uuid != 0 &&
        source_file_uuid != m_source.selected_uuid))
    {
        return;
    }
    if(!state.in_flight)
    {
        return;
    }
    state.in_flight = false;

    if(result == kRocProfVisResultCancelled)
    {
        state.status = CodeLoadState::kNotLoaded;
        UpdateWidgetLoadStates();
        return;
    }
    state.status = result == kRocProfVisResultSuccess ? CodeLoadState::kLoaded
                                                      : CodeLoadState::kFailed;
    if(result != kRocProfVisResultSuccess)
    {
        if(layer == PcSamplingLayer::kStalls)
        {
            RefreshCodeWidgets(layer);
        }
        UpdateWidgetLoadStates();
        NotifyLoadFailure(layer);
        return;
    }
    const KernelInfo* kernel_info = m_data_provider.ComputeModel().GetKernelInfo(
        m_current_workload_id, m_current_kernel_id);
    if(!kernel_info)
    {
        state.status = CodeLoadState::kFailed;
        UpdateWidgetLoadStates();
        NotifyLoadFailure(layer);
        return;
    }

    if(layer == PcSamplingLayer::kStalls &&
       (!kernel_info->pc_sampling_data.sample_states_loaded ||
        !kernel_info->pc_sampling_data.stall_details_loaded))
    {
        state.status = CodeLoadState::kFailed;
    }
    if(layer == PcSamplingLayer::kSource)
    {
        m_source.selected_uuid = source_file_uuid;
        LoadSourceFileList(kernel_info->pc_sampling_data);
        if(m_source.selected_uuid != 0)
        {
            m_source.loaded_uuids.insert(m_source.selected_uuid);
        }
    }
    RefreshCodeWidgets(layer);
    UpdateWidgetLoadStates();
    if(state.status == CodeLoadState::kFailed)
    {
        NotifyLoadFailure(layer);
    }
}

void
ComputeIsaView::NotifyLoadFailure(PcSamplingLayer layer)
{
    constexpr double NOTIFICATION_DURATION_SECONDS = 5.0;

    const char*       message = nullptr;
    NotificationLevel level  = NotificationLevel::Error;
    switch(layer)
    {
        case PcSamplingLayer::kIsa:
            message = "ISA instructions could not be loaded. Use Retry in the ISA pane.";
            break;
        case PcSamplingLayer::kSource:
            message = "Source code could not be loaded. Use Retry in the source pane.";
            break;
        case PcSamplingLayer::kStalls:
            if(m_sample_counts_loaded)
            {
                message = "Stall data could not be loaded. Showing total samples only.";
                level   = NotificationLevel::Warning;
            }
            else
            {
                message = "Sampling data could not be loaded.";
            }
            break;
    }
    if(message)
    {
        NotificationManager::GetInstance().Show(message, level,
                                                NOTIFICATION_DURATION_SECONDS);
    }
}

void
ComputeIsaView::LoadSourceFileList(const PcSamplingData& data)
{
    m_source.file_uuid_by_path.clear();
    for(const SourceFile& file : data.source_files)
    {
        m_source.file_uuid_by_path.emplace(file.file_path, file.source_file_uuid);
    }

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
    {
        m_source.selected_uuid = m_source.file_uuid_by_path.empty()
                                     ? 0
                                     : m_source.file_uuid_by_path.begin()->second;
    }
}

void
ComputeIsaView::SelectSourceFile(uint64_t source_file_uuid)
{
    if(source_file_uuid == m_source.selected_uuid)
    {
        return;
    }

    m_source.selected_uuid = source_file_uuid;
    m_source.widget->Load({}, 0, m_sampling_data);
    if(m_source.loaded_uuids.count(source_file_uuid))
    {
        m_source.fetch.queued        = false;
        m_source.fetch.request_token = ++m_next_request_token;
        CancelPcSamplingFetch(PcSamplingLayer::kSource);
        m_source.fetch.status = CodeLoadState::kLoaded;
        RefreshSourceWidget();
        UpdateWidgetLoadStates();
    }
    else
    {
        QueuePcSamplingFetch(PcSamplingLayer::kSource);
    }
}

void
ComputeIsaView::SelectSourceFileForScroll()
{
    const uint64_t source_file_uuid = m_line_selection.source_scroll_file;
    if(source_file_uuid == LineSelection::UNSELECTED)
    {
        return;
    }

    m_line_selection.source_scroll_file = LineSelection::UNSELECTED;
    const bool source_file_exists       = std::any_of(
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
    if(!kernel_info)
    {
        return;
    }

    const PcSamplingData& data = kernel_info->pc_sampling_data;
    switch(layer)
    {
        case PcSamplingLayer::kIsa:
        {
            if(!data.code_objects.empty())
            {
                m_isa.code_object_uuid = data.code_objects[0].code_object_uuid;
            }
            m_isa.widget->Load(data, m_isa.code_object_uuid, m_sampling_data);
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
            m_sample_counts_loaded = data.sample_states_loaded;
            m_sampling_data = IsaDataBuilder::PrepareSampling(data);
            m_isa.widget->UpdateSampling(m_sampling_data);
            m_source.widget->UpdateSampling(data, m_sampling_data);
            break;
        }
    }
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
        m_source.widget->Load(kernel_info->pc_sampling_data, m_source.selected_uuid,
                              m_sampling_data);
    }
}

void
ComputeIsaView::UpdateWidgetLoadStates()
{
    m_isa.widget->SetLoadState(m_isa.fetch.status);
    m_source.widget->SetLoadState(m_source.fetch.status);

    using SamplingLoadState = SamplingStateRenderer::LoadState;

    SamplingLoadState sampling_state = SamplingLoadState::kLoading;
    if(m_stalls.status == CodeLoadState::kLoaded)
    {
        sampling_state = SamplingLoadState::kReady;
    }
    else if(m_sample_counts_loaded)
    {
        sampling_state = m_stalls.status == CodeLoadState::kLoading
                             ? SamplingLoadState::kLoadingDetails
                             : SamplingLoadState::kTotalsOnly;
    }
    else if(m_stalls.status == CodeLoadState::kFailed)
    {
        sampling_state = SamplingLoadState::kFailed;
    }
    m_isa.widget->SetSamplingLoadState(sampling_state);
    m_source.widget->SetSamplingLoadState(sampling_state);
}

void
ComputeIsaView::RetryFailedLayers()
{
    if(m_isa.widget->TakeCodeRetryRequest() &&
       m_isa.fetch.status == CodeLoadState::kFailed)
    {
        QueuePcSamplingFetch(PcSamplingLayer::kIsa);
    }
    if(m_source.widget->TakeCodeRetryRequest() &&
       m_source.fetch.status == CodeLoadState::kFailed)
    {
        QueuePcSamplingFetch(PcSamplingLayer::kSource);
    }
    const bool isa_retry    = m_isa.widget->TakeSamplingRetryRequest();
    const bool source_retry = m_source.widget->TakeSamplingRetryRequest();
    if((isa_retry || source_retry) && m_stalls.status == CodeLoadState::kFailed)
    {
        QueuePcSamplingFetch(PcSamplingLayer::kStalls);
    }
}

void
ComputeIsaView::Update()
{
    RetryFailedLayers();
    SelectSourceFileForScroll();
    FetchPendingPcSampling();
    UpdateWidgetLoadStates();
}

void
ComputeIsaView::Render()
{
    RenderControlPanel();

    ImGui::PushFont(m_settings.GetFontManager().GetFont(FontType::kCode), 0.0f);

    m_line_selection.hovered_this_frame = false;
    m_horizontal_split_container->Render();
    if(!m_line_selection.hovered_this_frame)
    {
        m_line_selection.hovered_line = LineSelection::UNSELECTED;
    }

    ImGui::PopFont();
}

void
ComputeIsaView::RenderControlPanel()
{
    constexpr const char* hide_source_code_str = "Hide Source Code";
    constexpr const char* show_source_code_str = "Show Source Code";

    const float fallback_height =
        ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f;

    float top_height =
        m_control_panel_height > 0.0f ? m_control_panel_height : fallback_height;

    ImGui::BeginChild("ControlPanel", ImVec2(0.0f, top_height), true);

    ImVec2 start = ImGui::GetCursorPos();

    ImGui::BeginGroup();

    RenderSourceFileDropdown();

    const float button_source_code_width =
        std::max(ImGui::CalcTextSize(show_source_code_str).x,
                 ImGui::CalcTextSize(hide_source_code_str).x) +
        ImGui::GetStyle().FramePadding.x * 2.0f;
    RenderDevelopmentNotice(button_source_code_width);

    if(ImGui::Button(m_source_layout_item->m_visible ? hide_source_code_str
                                                     : show_source_code_str))
    {
        m_source_layout_item->m_visible = !m_source_layout_item->m_visible;
        if(m_source_layout_item->m_visible)
        {
            if(!m_source.loaded_uuids.count(m_source.selected_uuid))
            {
                QueuePcSamplingFetch(PcSamplingLayer::kSource);
            }
            else
            {
                RefreshSourceWidget();
            }
        }
        else
        {
            m_source.fetch.queued = false;
        }
    }

    ImGui::EndGroup();

    ImVec2 end = ImGui::GetCursorPos();

    float content_height   = end.y - start.y;
    m_control_panel_height = content_height + ImGui::GetStyle().WindowPadding.y * 2.0f;

    ImGui::EndChild();
}

void
ComputeIsaView::RenderDevelopmentNotice(float button_width)
{
    constexpr const char* LABEL   = "Tech Preview";
    constexpr const char* TOOLTIP = "Tech Preview Feature.\n"
                                    "Functionality may change.";

    const ImGuiStyle& style     = ImGui::GetStyle();
    const float       row_start = ImGui::GetCursorPosX();
    const float       row_end   = row_start + ImGui::GetContentRegionAvail().x;
    const float       button_x  = std::max(row_start, row_end - button_width);
    const bool        has_source_selector =
        m_source_layout_item->m_visible && !m_source.file_uuid_by_path.empty();

    if(has_source_selector)
    {
        ImGui::SameLine();
    }

    ImGui::BeginGroup();
    const ImU32 color = m_settings.GetColor(Colors::kTextDim);
    ImGui::AlignTextToFramePadding();
    PanelIcon(ICON_INFORMATION, Colors::kTextDim, &m_settings, style.FontSizeBase);
    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(color), "%s", LABEL);
    ImGui::EndGroup();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
    {
        SetTooltipStyled("%s", TOOLTIP);
    }

    // Leave the cursor ready for the right-aligned visibility button.
    const float notice_right = ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x;
    if(notice_right + style.ItemSpacing.x <= button_x)
    {
        ImGui::SameLine();
    }
    ImGui::SetCursorPosX(button_x);
}

void
ComputeIsaView::RenderSourceFileDropdown()
{
    constexpr const float DROPDOWN_SIZE = 300.0f;
    if(!m_source_layout_item->m_visible || m_source.file_uuid_by_path.empty())
    {
        return;
    }

    auto filename_of = [](const std::string& str) -> const char* {
        const auto pos = str.find_last_of("/\\");
        return pos == std::string::npos ? str.c_str() : str.c_str() + pos + 1;
    };

    const auto selected_file_it = std::find_if(
        m_source.file_uuid_by_path.begin(), m_source.file_uuid_by_path.end(),
        [this](const auto& pair) { return pair.second == m_source.selected_uuid; });

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
            const std::string item_id = std::to_string(id);
            ImGui::PushID(item_id.c_str());
            const bool selected = (id == m_source.selected_uuid);
            if(ImGui::Selectable(filename_of(path), selected) && !selected)
            {
                m_line_selection = {};
                SelectSourceFile(id);
            }
            if(ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                SetTooltipStyled("%s", path.c_str());
            }
            if(selected)
            {
                ImGui::SetItemDefaultFocus();
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    if(selected_file_it != m_source.file_uuid_by_path.end() &&
       ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
    {
        SetTooltipStyled("%s", selected_file_it->first.c_str());
    }
}

}  // namespace View
}  // namespace RocProfVis
