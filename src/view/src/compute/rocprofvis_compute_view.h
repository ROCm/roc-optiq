// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_data_provider.h"
#include "rocprofvis_root_view.h"

namespace RocProfVis
{
namespace View
{

class SettingsManager;
class ComputeTester;

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

    std::shared_ptr<RocWidget> GetToolbar() override;
    void                       RenderEditMenuOptions() override;

private:
    void RenderToolbar();

    bool m_view_created;

    DataProvider                     m_data_provider;
    std::shared_ptr<RocCustomWidget> m_tool_bar;
    std::unique_ptr<ComputeTester>   m_tester;
};

class ComputeTester : public RocWidget
{
public:
    ComputeTester(DataProvider& data_provider);
    ~ComputeTester();

    void Update();
    void Render();

private:
    void RenderCategory(const AvailableMetrics::Category& cat);
    DataProvider& m_data_provider;
};

}  // namespace View
}  // namespace RocProfVis
