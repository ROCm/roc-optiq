// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "imgui.h"
#include <string>
#include <functional>

namespace RocProfVis
{
namespace View
{

class ConfirmationDialog {
public:
    void Show(const std::string& title, const std::string& message, std::function<void()> on_confirm_callback);
    void Render();

private:
    std::string m_title;
    std::string m_message;
    std::function<void()> m_on_confirm;
    bool m_should_open = false;
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

