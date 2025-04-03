// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_debug_window.h"
#include "imgui.h"

using namespace RocProfVis::View;

DebugWindow* DebugWindow::s_instance = nullptr;

DebugWindow*
DebugWindow::GetInstance()
{
    if(!s_instance)
    {
        s_instance = new DebugWindow();
    }

    return s_instance;
}

DebugWindow::DebugWindow() {}

void
DebugWindow::Render()
{
    ImGui::Begin("Debug Window", nullptr, ImGuiWindowFlags_None);
    for(const std::string& message : m_debug_messages)
    {
        ImGui::Text(message.c_str());
        //ImGui::NewLine();
    }
    ImGui::End();
}

void
DebugWindow::AddDebugMessage(const std::string& message)
{
    m_debug_messages.push_back(message);
}

void
DebugWindow::Reset()
{
    m_debug_messages.clear();
}