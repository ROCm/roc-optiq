// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_debug_window.h"
#include "imgui.h"
#include "rocprofvis_core.h"

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

DebugWindow::DebugWindow()
: m_persitent_debug_messages(500)
, m_do_h_split(false)
{
    // create layout
    LayoutItem nav_bar(0, 30.0f);
    nav_bar.m_item = std::make_shared<RocCustomWidget>([this]() { this->RenderNav(); });
    nav_bar.m_window_padding = ImVec2(10, 2);
    nav_bar.m_item_spacing   = ImVec2(4, 4);
    nav_bar.m_bg_color       = ImColor(228, 228, 228, 255);

    LayoutItem content(0, 0.0f);

    std::shared_ptr<RocWidget> rp =
        std::make_shared<RocCustomWidget>([this]() { this->RenderPersitent(); });
    std::shared_ptr<RocWidget> rt =
        std::make_shared<RocCustomWidget>([this]() { this->RenderTransient(); });

    LayoutItem top;
    top.m_item           = rp;
    top.m_window_padding = ImVec2(4, 4);
    LayoutItem bottom;
    bottom.m_item           = rt;
    bottom.m_window_padding = ImVec2(4, 4);

    m_v_spilt_container = std::make_shared<VSplitContainer>(top, bottom);
    m_v_spilt_container->SetMinTopHeight(10.0f);
    m_v_spilt_container->SetMinBottomHeight(10.0f);

    LayoutItem left;
    left.m_item           = rt;
    left.m_window_padding = ImVec2(4, 4);
    LayoutItem right;
    right.m_item           = rp;
    right.m_window_padding = ImVec2(4, 4);

    m_h_spilt_container = std::make_shared<HSplitContainer>(left, right);
    m_h_spilt_container->SetMinLeftWidth(10.0f);
    m_h_spilt_container->SetMinRightWidth(10.0f);

    content.m_window_padding = ImVec2(2, 0);
    if(m_do_h_split)
    {
        content.m_item       = m_h_spilt_container;
        m_split_button_label = "Split Vertically";
    }
    else
    {
        content.m_item       = m_v_spilt_container;
        m_split_button_label = "Split Horizontally";
    }
    content.m_child_flags = ImGuiChildFlags_None;

    std::vector<LayoutItem> layoutItems;
    layoutItems.push_back(nav_bar);
    layoutItems.push_back(content);

    m_main_container = std::make_shared<VFixedContainer>(layoutItems);
}

void
DebugWindow::RenderNav()
{
    if(ImGui::Button(m_split_button_label.c_str()))
    {
        m_do_h_split = !m_do_h_split;
        LayoutItem tmp(0, 0.0f);
        tmp.m_window_padding     = ImVec2(2, 0);
        tmp.m_child_flags = ImGuiChildFlags_None;

        if(m_do_h_split)
        {
            tmp.m_item           = m_h_spilt_container;
            m_split_button_label = "Split Vertically";
        }
        else
        {
            tmp.m_item           = m_v_spilt_container;
            m_split_button_label = "Split Horizontally";
        }
        m_main_container->SetAt(1, tmp);
    }

    ImGui::SameLine();

    if(ImGui::Button("Clear Log"))
    {
        ClearPersitent();
    }
}

void
DebugWindow::RenderPersitent()
{
    ImGui::Text("Log Messages");
    ImGui::Separator();
    for(const std::string& message : m_persitent_debug_messages.GetContainer())
    {
        ImGui::Text(message.c_str());
    }
}

void
DebugWindow::RenderTransient()
{
    ImGui::Text("Debug Window Messages");
    ImGui::Separator();
    for(const std::string& message : m_transient_debug_messages)
    {
        ImGui::Text(message.c_str());
    }
}

void
DebugWindow::Render()
{
    rocprofvis_core_get_log_entries((void*) this, &DebugWindow::Process);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    ImGui::Begin("Debug Window", nullptr, ImGuiWindowFlags_None);
    m_main_container->Render();
    ImGui::End();

    ImGui::PopStyleVar(2);
}

void
DebugWindow::AddDebugMessage(const std::string& message)
{
    m_transient_debug_messages.push_back(message);
}

void
DebugWindow::AddPersitentDebugMessage(const std::string& message)
{
    // escape the message so characters that could be interpreted as format specifiers and
    // escape sequences are not interpreted by ImGui
    std::string escaped_message;
    escaped_message.reserve(message.size());
    for(char c : message)
    {
        if(c == '\\')
        {
            escaped_message.push_back('\\');
        }
        else if(c == '%')
        {
            escaped_message.push_back('%');
        }
        escaped_message.push_back(c);
    }

    m_persitent_debug_messages.Push(escaped_message);
}

void
DebugWindow::ClearTransient()
{
    m_transient_debug_messages.clear();
}

void
DebugWindow::SetMaxMessageCount(int count)
{
    m_persitent_debug_messages.Resize(count);
}

void
DebugWindow::ClearPersitent()
{
    m_persitent_debug_messages.Clear();
}

void
DebugWindow::Process(void* user_ptr, char const* log)
{
    DebugWindow* self = (DebugWindow*) user_ptr;
    if(self && log)
    {
        self->AddPersitentDebugMessage(log);
    }
}