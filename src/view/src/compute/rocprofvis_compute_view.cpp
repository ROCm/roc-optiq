// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_view.h"
#include "model/compute/rocprofvis_compute_data_model.h"
#include "rocprofvis_compute_comparison.h"
#include "rocprofvis_compute_kernel_details.h"
#include "rocprofvis_compute_selection.h"
#include "rocprofvis_compute_summary.h"
#include "rocprofvis_compute_table_view.h"
#include "rocprofvis_presets.h"
#ifdef ROCPROFVIS_DEVELOPER_MODE
#    include "rocprofvis_compute_tester.h"
#endif
#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_compute_workload_view.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_notification_manager.h"
#include "rocprofvis_compute_isa_view.h"

#include "spdlog/spdlog.h"
#include <algorithm>

namespace RocProfVis
{
namespace View
{

constexpr const char* INVALID_COMPUTE_DATABASE_MESSAGE =
    "The file could not be loaded as a compatible compute profiling "
    "database. Its schema may be invalid or unsupported, or required "
    "compute-profile data may be missing.";

static bool
HasAvailableMetrics(const std::vector<const WorkloadInfo*>& workloads)
{
    return std::any_of(
        workloads.begin(), workloads.end(), [](const WorkloadInfo* workload) {
            return workload &&
                   std::any_of(workload->available_metrics.ordered_categories.begin(),
                               workload->available_metrics.ordered_categories.end(),
                               [](const AvailableMetrics::Category* category) {
                                   return category && !category->ordered_tables.empty();
                               });
        });
}

static bool
HasIsaLines(const std::vector<const WorkloadInfo*>& workloads)
{
    return std::any_of(
        workloads.begin(), workloads.end(), [](const WorkloadInfo* workload) {
            return workload &&
                   std::any_of(workload->kernels.begin(), workload->kernels.end(),
                               [](const auto& kernel) {
                                   return kernel.second.has_isa_lines;
                               });
        });
}

ComputeView::ComputeView()
: m_view_created(false)
, m_error_dialog_state(ErrorDialogState::kNone)
, m_toolbar_available_width(0.0f)
, m_compute_selection(nullptr)
, m_preset_browser(nullptr)
, m_tab_container(nullptr)
, m_popup_info({})
{
    m_tool_bar = std::make_shared<RocCustomWidget>([this]() { this->RenderToolbar(); });
    m_widget_name = GenUniqueName("ComputeView");

    m_data_provider.SetTraceLoadedCallback([this](const std::string& trace_path,
                                                  uint64_t           response_code) {
        if(response_code != kRocProfVisResultSuccess)
        {
            spdlog::error("Failed to load trace: {}", response_code);
            QueueDatabaseErrorDialog(trace_path, INVALID_COMPUTE_DATABASE_MESSAGE);
        }
    });

    m_data_provider.SetFetchMetricsCallback(
        [this](const std::string& trace_path, uint64_t client_id, bool success) {
            if(!success)
            {
                NotificationManager::GetInstance().Show(
                    "Failed to fetch metrics for trace: " + trace_path,
                    NotificationLevel::Error);
            }
            else
            {
#ifdef ROCPROFVIS_DEVELOPER_MODE
                NotificationManager::GetInstance().Show(
                    "Successfully fetched metrics for client: " + std::to_string(client_id),
                    NotificationLevel::Success);
#endif
                // trigger metrics fetched event to update the UI
                EventManager::GetInstance()->AddEvent(
                    std::make_shared<ComputeMetricsFetchedEvent>(client_id, trace_path));
            }
        });

    m_data_provider.SetTableDataReadyCallback(
        [](const std::string& trace_path, uint64_t request_id, uint64_t response_code) {
            if(response_code != kRocProfVisResultSuccess)
            {
                NotificationManager::GetInstance().Show(
                    "Failed to fetch table data for trace: " + trace_path,
                    NotificationLevel::Error);
            }

            // Trigger new table data event to update the UI.
            EventManager::GetInstance()->AddEvent(
                std::make_shared<TableDataEvent>(trace_path, request_id, response_code));
        });

    // The forwarding TraceView installs. Without it a compute tab runs requests
    // that report no progress at all.
    m_data_provider.SetRequestProgressUpdateCallback(
        [this](const RequestInfo& request, uint64_t pct, const std::string& message) {
            EventManager::GetInstance()->AddEvent(
                std::make_shared<RequestProgressUpdateEvent>(
                    request.request_id, request.request_type, pct, message,
                    m_data_provider.GetTraceFilePath()));
        });
}

ComputeView::~ComputeView()
{
    // Every callback above captures this, and the provider outlives the view
    // while its detached cleanup runs.
    m_data_provider.SetTraceLoadedCallback(nullptr);
    m_data_provider.SetFetchMetricsCallback(nullptr);
    m_data_provider.SetTableDataReadyCallback(nullptr);
    m_data_provider.SetRequestProgressUpdateCallback(nullptr);
}

std::optional<DataProviderCleanupWork>
ComputeView::DetachProviderCleanup()
{
    DataProviderCleanupWork cleanup_work = m_data_provider.DetachCleanupWork();
    return cleanup_work;
}

void
ComputeView::Update()
{
    m_data_provider.Update();

    const ProviderState new_state = m_data_provider.GetState();

    if(!m_view_created && m_error_dialog_state == ErrorDialogState::kNone &&
       (new_state == ProviderState::kReady || new_state == ProviderState::kError))
    {
        CreateView();
        m_view_created = (m_tab_container != nullptr);
    }

    if(new_state == ProviderState::kReady)
    {
        if(m_preset_browser)
        {
            m_preset_browser->Update();
        }
        if(m_tab_container)
        {
            m_tab_container->Update();
        }
    }

    ShowPendingDatabaseErrorDialog();
}

void
ComputeView::CreateView()
{
    m_compute_selection.reset();
    m_preset_browser.reset();
    m_tab_container.reset();

    const WorkloadInfo* initial_workload = ValidateDatabase();
    if(!initial_workload)
    {
        return;
    }

    m_compute_selection = std::make_shared<ComputeSelection>(m_data_provider);
    m_compute_selection->SelectWorkload(initial_workload->id);
    m_preset_browser = std::make_unique<PresetBrowser>();
    CreateTabContainer();
}

const WorkloadInfo*
ComputeView::ValidateDatabase()
{
    if(m_data_provider.GetState() == ProviderState::kError)
    {
        QueueDatabaseErrorDialog(m_data_provider.GetTraceFilePath(),
                                 INVALID_COMPUTE_DATABASE_MESSAGE);
        return nullptr;
    }

    const std::vector<const WorkloadInfo*>& workloads =
        m_data_provider.ComputeModel().GetWorkloadList();
    if(workloads.empty())
    {
        QueueDatabaseErrorDialog(
            m_data_provider.GetTraceFilePath(),
            "The file contains no compute workloads. A compute profile must contain "
            "at least one workload and one kernel before it can be displayed.");
        return nullptr;
    }

    const auto workload_with_kernels =
        std::find_if(workloads.begin(), workloads.end(), [](const WorkloadInfo* workload) {
            return workload && !workload->kernels.empty();
        });
    if(workload_with_kernels == workloads.end())
    {
        QueueDatabaseErrorDialog(
            m_data_provider.GetTraceFilePath(),
            "The file contains compute workloads, but none of them contains kernel "
            "data. A compute profile must contain at least one workload with a kernel "
            "before it can be displayed.");
        return nullptr;
    }

    return *workload_with_kernels;
}

void
ComputeView::CreateTabContainer()
{
    const std::vector<const WorkloadInfo*>& workloads =
        m_data_provider.ComputeModel().GetWorkloadList();
    const bool database_has_metrics   = HasAvailableMetrics(workloads);

    m_tab_container = std::make_shared<TabContainer>();
    m_tab_container->AddTab(
        ComputeSummaryView::CreateTabItem(m_data_provider, m_compute_selection));
    m_tab_container->AddTab(
        ComputeKernelDetailsView::CreateTabItem(m_data_provider,
                                                m_compute_selection));

    m_tab_container->AddTab(ComputeTableView::CreateTabItem(
        m_data_provider, m_compute_selection, database_has_metrics));
    m_tab_container->AddTab(ComputeComparisonView::CreateTabItem(
        m_data_provider, m_compute_selection, database_has_metrics));
    m_tab_container->AddTab(
        ComputeWorkloadView::CreateTabItem(m_data_provider, m_compute_selection));

#ifdef ROCPROFVIS_DEVELOPER_MODE
    const bool database_has_isa_lines = HasIsaLines(workloads);
    m_tab_container->AddTab(
        ComputeIsaView::CreateTabItem(m_data_provider, database_has_isa_lines));
    m_tab_container->AddTab(
        ComputeTester::CreateTabItem(m_data_provider, m_compute_selection));
#endif
    m_tab_container->SetAllowToolTips(false);
}

void
ComputeView::DestroyView()
{
    m_view_created       = false;
    m_error_dialog_state = ErrorDialogState::kNone;
    m_popup_info         = {};
    m_tab_container.reset();
    m_compute_selection.reset();
    m_preset_browser.reset();
}

bool
ComputeView::LoadTrace(rocprofvis_controller_t* controller, const std::string& file_path)
{
    m_error_dialog_state = ErrorDialogState::kNone;
    m_popup_info         = {};

    bool result = false;
    result      = m_data_provider.FetchTrace(controller, file_path);
    return result;
}

void
ComputeView::Render()
{
    if(m_data_provider.GetState() == ProviderState::kLoading)
    {
        RenderLoadingScreen(m_data_provider.GetProgressMessage());
    }
    else
    {
        if(m_preset_browser)
        {
            m_preset_browser->Render();
        }
        if(m_tab_container)
        {
            m_tab_container->Render();
        }
    }
}

void
ComputeView::QueueDatabaseErrorDialog(const std::string& file_path,
                                      const std::string& message)
{
    if(m_error_dialog_state != ErrorDialogState::kNone)
    {
        return;
    }

    m_error_dialog_state = ErrorDialogState::kPending;
    m_popup_info.title   = "Invalid Compute Database";
    m_popup_info.message = message;
    if(!file_path.empty())
    {
        m_popup_info.message += "\n\nFile: " + file_path;
    }
}

void
ComputeView::ShowPendingDatabaseErrorDialog()
{
    if(m_error_dialog_state != ErrorDialogState::kPending)
    {
        return;
    }

    m_error_dialog_state      = ErrorDialogState::kShown;
    AppWindow*        app_window = AppWindow::GetInstance();
    const std::string project_id = m_data_provider.GetTraceFilePath();
    app_window->ShowMessageDialog(
        m_popup_info.title, m_popup_info.message,
        [app_window, project_id]() { app_window->CloseProjectTab(project_id); });
}

std::shared_ptr<RocWidget>
ComputeView::GetToolbar()
{
    return m_tool_bar;
}

void
ComputeView::RenderToolbar()
{
    const ImGuiStyle& style = SettingsManager::GetInstance().GetDefaultStyle();
    ImGui::PushStyleColor(
        ImGuiCol_ChildBg,
        ImGui::ColorConvertU32ToFloat4(m_settings_manager.GetColor(Colors::kBgPanel)));
    ImGui::PushStyleColor(ImGuiCol_Border,
                          ImGui::ColorConvertU32ToFloat4(
                              m_settings_manager.GetColor(Colors::kBorderColor)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(style.WindowPadding.x + 4.0f,
                               style.WindowPadding.y + 2.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::BeginChild("Toolbar", ImVec2(-1, 0),
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(style.FramePadding.x, style.FramePadding.y + 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, style.FrameRounding);
    ImGui::AlignTextToFramePadding();

    RenderWorkloadSelection();
    VerticalSeparator(&m_settings_manager);
    if(m_toolbar_available_width != 0.0)
    {
        ImGui::Dummy(
            ImVec2(m_toolbar_available_width, ImGui::GetFrameHeightWithSpacing()));
    }
    VerticalSeparator(&m_settings_manager);
    RenderPresets();

    ImGui::SameLine();
    m_toolbar_available_width =
        std::max(0.0f, m_toolbar_available_width + ImGui::GetContentRegionAvail().x);

    // pop content style
    ImGui::PopStyleVar(2);
    ImGui::EndChild();
    // pop child window style
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void
ComputeView::RenderWorkloadSelection()
{
    if(!m_compute_selection)
    {
        return;
    }

    const ImGuiStyle& style          = SettingsManager::GetInstance().GetDefaultStyle();

    const std::vector<const WorkloadInfo*>& workloads =
        m_data_provider.ComputeModel().GetWorkloadList();

    uint32_t            workload_id       = m_compute_selection->GetSelectedWorkload();
    const WorkloadInfo* selected_workload =
        m_data_provider.ComputeModel().GetWorkload(workload_id);
    ImGui::Text("Workload:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFrameHeight() * 10.0f);
    ImGui::BeginDisabled(workloads.empty());
    PushComboStyles();
    if(ImGui::BeginCombo("##Workloads",
                         selected_workload ? selected_workload->name.c_str() : "-"))
    {

        for(const WorkloadInfo* workload : workloads)
        {
            if(ImGui::Selectable(workload->name.c_str(),
                                 workload_id == workload->id))
            {
                m_compute_selection->SelectWorkload(workload->id);
            }
        }
        ImGui::EndCombo();
    }
    PopComboStyles();
    ImGui::EndDisabled();
    ImGui::SameLine(0, style.ItemSpacing.x);
    VerticalSeparator();
    ImGui::SameLine(0, style.ItemSpacing.x);
    ImGui::Text("Kernel:");
    ImGui::SameLine();
    uint32_t kernel_id = m_compute_selection->GetSelectedKernel();
    const KernelInfo* kernel_info = m_data_provider.ComputeModel().GetKernelInfo(workload_id, kernel_id);

    std::vector<const KernelInfo*> kernel_info_list =
        m_data_provider.ComputeModel().GetKernelInfoList(workload_id);
    ImGui::SetNextItemWidth(ImGui::GetFrameHeight() * 10.0f);
    ImGui::BeginDisabled(kernel_info_list.empty());
    PushComboStyles();
    if(ImGui::BeginCombo("##Kernels", kernel_info ? kernel_info->name.c_str() : "-"))
    {
        for(const KernelInfo* info : kernel_info_list)
        {
            if(ImGui::Selectable(info->name.c_str(), kernel_id == info->id))
            {
                m_compute_selection->SelectKernel(info->id);
            }
        }
        ImGui::EndCombo();
    }
    PopComboStyles();
    ImGui::EndDisabled();
}

void
ComputeView::RenderPresets()
{
    if(m_preset_browser)
    {
        const ImGuiStyle& style = SettingsManager::GetInstance().GetDefaultStyle();
        ImGui::TextUnformatted("Presets");
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + style.ItemSpacing.x);
        if(IconButton(ICON_CHEVRON_DOWN,
                      m_settings_manager.GetFontManager().GetFont(FontType::kIcon),
                      ImVec2(0.0f, 0.0f), nullptr, false, style.FramePadding,
                      m_settings_manager.GetColor(Colors::kTransparent),
                      m_settings_manager.GetColor(Colors::kButtonHovered),
                      m_settings_manager.GetColor(Colors::kTransparent)))
        {
            m_preset_browser->Show();
        }
        m_preset_browser->SetPosition(ImGui::GetItemRectMax().x + style.WindowPadding.x +
                                          0.5f * style.ItemSpacing.x,
                                      ImGui::GetItemRectMax().y + style.WindowPadding.y +
                                          0.5f * style.ItemSpacing.y);
    }
}

}  // namespace View
}  // namespace RocProfVis
