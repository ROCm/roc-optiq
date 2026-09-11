// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "imgui.h"

#include <deque>
#include <functional>
#include <optional>
#include <string>

namespace RocProfVis
{
namespace View
{

class ConfirmationDialog {
public:
    ConfirmationDialog() = default;
    ConfirmationDialog(bool& skip_dialog_setting)
    : m_skip_dialog_setting(skip_dialog_setting)
    {}
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
    void Show(const std::string& title, const std::string& message,
              std::function<void()> on_close_callback = nullptr);
    void Render();

private:
    struct message_info_t
    {
        std::string           title;
        std::string           message;
        std::function<void()> on_close;
    };

    std::string                m_title;
    std::string                m_message;
    std::function<void()>      m_on_close;
    std::deque<message_info_t> m_pending_messages;
    bool                       m_should_open = false;
    bool                       m_is_open     = false;
};

}  // namespace View
}  // namespace RocProfVis

