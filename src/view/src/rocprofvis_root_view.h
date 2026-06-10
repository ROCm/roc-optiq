// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_data_provider.h"
#include "widgets/rocprofvis_widget.h"
#include <memory>
#include <optional>

namespace RocProfVis
{
namespace View
{

class SettingsManager;

class RootView : public RocWidget
{
public:
    RootView();
    virtual ~RootView();

    virtual std::shared_ptr<RocWidget> GetToolbar();

    virtual void RenderEditMenuOptions();

    virtual std::optional<DataProviderCleanupWork> DetachProviderCleanup();

    virtual DataProvider* GetDataProvider() { return nullptr; };

    // True when this view has render-driven work that emits no events yet (e.g.
    // a debounce timer that only advances while rendering). Drives lazy render.
    virtual bool WantsContinuousRender() const { return false; }

protected:
    void RenderLoadingScreen(const char* progress_label);

    SettingsManager& m_settings_manager;
};

}  // namespace View
}  // namespace RocProfVis
