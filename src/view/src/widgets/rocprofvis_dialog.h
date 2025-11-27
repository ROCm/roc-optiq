// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#pragma once

#include "imgui.h"
#include <string>
#include <functional>
#include <optional>

namespace RocProfVis
{
namespace View
{

class ConfirmationDialog {
public:
    ConfirmationDialog() = default;
    ConfirmationDialog(bool& skip_dialog_setting)
    : m_skip_dialog_setting(skip_dialog_setting)
    {};
    void Show(const std::string& title, const std::string& message,
              std::function<void()> on_confirm_callback,
              std::function<void()> on_cancel_callback = nullptr);
    void Render();

private:
    void                                              DrawCheckboxOption();
    std::string                                       m_title;
    std::string                                       m_message;
    std::function<void()>                             m_on_confirm;
    std::function<void()>                             m_on_cancel;
    bool                                              m_should_open = false;
    const std::optional<std::reference_wrapper<bool>> m_skip_dialog_setting;
};

class MessageDialog {
public:
    void Show(const std::string& title, const std::string& message);
    void Render();
private:
    std::string m_title;
    std::string m_message;
    bool m_should_open = false;
};

}  // namespace View
}  // namespace RocProfVis

