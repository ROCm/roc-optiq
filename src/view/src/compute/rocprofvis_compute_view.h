// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_data_provider.h"
#include "rocprofvis_root_view.h"
#include "widgets/rocprofvis_tab_container.h"

#include <string>

namespace RocProfVis
{
namespace View
{

class ComputeSelection;
class PresetBrowser;

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
    void RenderToolbar();
    void RenderWorkloadSelection();
    void RenderPresets();
    void InitializeMetricTabStates();

    bool  m_view_created;
    float m_toolbar_available_width;

    std::shared_ptr<ComputeSelection> m_compute_selection;
    std::unique_ptr<PresetBrowser>    m_preset_browser;

    std::shared_ptr<TabContainer> m_tab_container;

    std::string m_database_error_message;

    DataProvider                     m_data_provider;
    std::shared_ptr<RocCustomWidget> m_tool_bar;
};

}  // namespace View
}  // namespace RocProfVis
