// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_data_provider.h"
#include "rocprofvis_root_view.h"

#include <memory>
#include <string>

namespace RocProfVis
{
namespace View
{

class ComputeKernelDetailsView;
class ComputeSelection;
class ComputeSummaryView;
class ComputeTester;
class PresetBrowser;
struct WorkloadInfo;

class ComputeView : public RootView
{
public:
    ComputeView();
    ~ComputeView();

    void Update() override;
    void Render() override;

    bool LoadTrace(rocprofvis_controller_t* controller, const std::string& file_path);

    void CreateView();
    void DestroyView();

    DataProvider* GetDataProvider() override { return &m_data_provider; }

    std::shared_ptr<RocWidget> GetToolbar() override;
    std::optional<DataProviderCleanupWork> DetachProviderCleanup() override;

    friend struct ComputeViewTestPeer;

private:
    const WorkloadInfo* ValidateDatabase();
    void CreateKernelDetails();
    void RenderToolbar();
    void RenderPresets();
    // Floating "Summary" window, toggled by View > Show Summary.
    void RenderSummaryWindow();
#ifdef ROCPROFVIS_DEVELOPER_MODE
    // Floating "Compute Tester" window, toggled from Developer Options.
    void RenderComputeTesterWindow();
#endif
    void QueueDatabaseErrorDialog(const std::string& file_path,
                                  const std::string& message);
    void ShowPendingDatabaseErrorDialog();

    enum class ErrorDialogState
    {
        kNone,
        kPending,
        kShown
    };

    bool             m_view_created;
    ErrorDialogState m_error_dialog_state;
    float            m_toolbar_available_width;

    std::shared_ptr<ComputeSelection>   m_compute_selection;
    std::unique_ptr<PresetBrowser>      m_preset_browser;
    std::shared_ptr<ComputeSummaryView> m_summary_view;

    // The whole compute UI: one workspace, so no tab bar.
    std::shared_ptr<ComputeKernelDetailsView> m_kernel_details;
#ifdef ROCPROFVIS_DEVELOPER_MODE
    std::shared_ptr<ComputeTester> m_compute_tester;
#endif

    struct popup_info_t
    {
        std::string title;
        std::string message;
    };

    popup_info_t m_popup_info;

    DataProvider                     m_data_provider;
    std::shared_ptr<RocCustomWidget> m_tool_bar;
};

}  // namespace View
}  // namespace RocProfVis
