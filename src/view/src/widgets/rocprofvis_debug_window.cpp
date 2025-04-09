// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_debug_window.h"
#include "rocprofvis_core.h"
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

DebugWindow::DebugWindow(): m_persitent_debug_messages(500) {}

void
DebugWindow::Render()
{
    rocprofvis_core_get_log_entries((void*) this, &DebugWindow::Process);
    ImGui::Begin("Debug Window", nullptr, ImGuiWindowFlags_None);
    for(const std::string& message: m_persitent_debug_messages.GetContainer() ) {
        ImGui::Text(message.c_str());
    }
    
    ImGui::Separator();
    
    for(const std::string& message : m_transient_debug_messages)
    {
        ImGui::Text(message.c_str());
    }


    ImGui::End();
}

void
DebugWindow::AddDebugMessage(const std::string& message)
{
    m_transient_debug_messages.push_back(message);
}

void
DebugWindow::AddPersitentDebugMessage(const std::string& message)
{
    m_persitent_debug_messages.Push(message);
}

void
DebugWindow::Reset()
{
    m_transient_debug_messages.clear();
}

void
DebugWindow::Process(void* user_ptr, char const* log)
{
    DebugWindow* self = (DebugWindow*) user_ptr;
    if (self && log)
    {
        self->AddPersitentDebugMessage(log);
    }
}