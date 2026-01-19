// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/rocprofvis_widget.h"
#include <memory>

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

protected:
    void RenderLoadingScreen(const char* progress_label);

    SettingsManager& m_settings_manager;
};

}  // namespace View
}  // namespace RocProfVis
