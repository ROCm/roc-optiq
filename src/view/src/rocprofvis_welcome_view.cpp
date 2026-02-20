// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_welcome_view.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_profiling_dialog.h"
#include "rocprofvis_project.h"
#include "widgets/rocprofvis_gui_helpers.h"

#include "imgui.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <memory>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <thread>

namespace RocProfVis
{
namespace View
{

WelcomeView::WelcomeView()
: m_hover_open_trace(false)
, m_hover_run_profiling(false)
, m_show_profiling_dialog(false)
{
    m_widget_name = GenUniqueName("WelcomeView");
    LoadRecentFiles();
}

WelcomeView::~WelcomeView()
{
}

void WelcomeView::Update()
{
    // Nothing to update
}

void WelcomeView::Render()
{
    RenderWelcomeContent();
    ShowProfilingDialog();
}

std::shared_ptr<RocWidget> WelcomeView::GetToolbar()
{
    // No toolbar for welcome view
    return nullptr;
}

void WelcomeView::RenderWelcomeContent()
{
    ImGui::BeginChild("##welcome_content", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);

    // Calculate center position for content
    ImVec2 window_size = ImGui::GetWindowSize();
    float content_width = 800.0f;
    float content_height = 600.0f;
    float center_x = (window_size.x - content_width) * 0.5f;
    float center_y = (window_size.y - content_height) * 0.5f;

    if(center_x < 20.0f) center_x = 20.0f;
    if(center_y < 20.0f) center_y = 20.0f;

    ImGui::SetCursorPos(ImVec2(center_x, center_y));

    ImGui::BeginChild("##welcome_centered", ImVec2(content_width, content_height), false,
                      ImGuiWindowFlags_NoScrollbar);

    // Title
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Use default font, maybe bigger in the future
    const char* title = "ROC-Optiq - ROCm Profiler Visualizer";
    float title_width = ImGui::CalcTextSize(title).x;
    ImGui::SetCursorPosX((content_width - title_width) * 0.5f);
    ImGui::TextUnformatted(title);
    ImGui::PopFont();

    ImGui::Spacing();
    ImGui::Spacing();

    // Main action cards
    float card_width = 360.0f;
    float card_height = 200.0f;
    float card_spacing = 40.0f;
    float cards_total_width = card_width * 2 + card_spacing;
    float cards_start_x = (content_width - cards_total_width) * 0.5f;

    ImGui::SetCursorPosX(cards_start_x);

    // Begin horizontal layout for cards
    ImGui::BeginGroup();

    // Left card - Open Trace
    RenderOpenTraceCard();

    ImGui::SameLine(0, card_spacing);

    // Right card - Run Profiling
    RenderRunProfilingCard();

    ImGui::EndGroup();

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();

    // Recent files section
    RenderRecentFiles();

    // Drag-drop hint
    ImGui::Spacing();
    ImGui::Spacing();
    const char* hint = "Tip: Drag and drop trace files here to open them";
    float hint_width = ImGui::CalcTextSize(hint).x;
    ImGui::SetCursorPosX((content_width - hint_width) * 0.5f);
    ImGui::TextDisabled("%s", hint);

    ImGui::EndChild();
    ImGui::EndChild();
}

void WelcomeView::RenderOpenTraceCard()
{
    float card_width = 360.0f;
    float card_height = 200.0f;

    ImGui::BeginChild("##open_trace_card", ImVec2(card_width, card_height), true,
                      ImGuiWindowFlags_NoScrollbar);

    ImVec2 card_pos = ImGui::GetCursorPos();

    // Check if mouse is hovering over the card
    ImVec2 mouse_pos = ImGui::GetMousePos();
    ImVec2 card_screen_pos = ImGui::GetWindowPos();
    card_screen_pos.x += card_pos.x;
    card_screen_pos.y += card_pos.y;
    bool is_hovered = mouse_pos.x >= card_screen_pos.x &&
                      mouse_pos.x <= card_screen_pos.x + card_width &&
                      mouse_pos.y >= card_screen_pos.y &&
                      mouse_pos.y <= card_screen_pos.y + card_height &&
                      ImGui::IsWindowHovered(ImGuiHoveredFlags_None);

    // Highlight on hover
    if(is_hovered)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 min = card_screen_pos;
        ImVec2 max = ImVec2(card_screen_pos.x + card_width, card_screen_pos.y + card_height);
        ImU32 col = IM_COL32(80, 80, 120, 100);
        draw_list->AddRectFilled(min, max, col, 4.0f);
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Icon (folder emoji as placeholder)
    const char* icon = "  Open Trace";
    float icon_width = ImGui::CalcTextSize(icon).x;
    ImGui::SetCursorPosX((card_width - icon_width) * 0.5f);
    ImGui::TextUnformatted(icon);

    ImGui::Spacing();
    ImGui::Spacing();

    // Description
    const char* desc = "Load existing trace files";
    float desc_width = ImGui::CalcTextSize(desc).x;
    ImGui::SetCursorPosX((card_width - desc_width) * 0.5f);
    ImGui::TextWrapped("%s", desc);

    ImGui::Spacing();
    ImGui::Spacing();

    // Button
    float button_width = 150.0f;
    ImGui::SetCursorPosX((card_width - button_width) * 0.5f);
    if(ImGui::Button("Browse Files", ImVec2(button_width, 0)))
    {
        AppWindow::GetInstance()->ShowOpenFileDialog(
            "Open Trace File",
            {
                {"Trace Files", {"db", "rpd", "yaml"}},
                {"All Files", {"*"}}
            },
            "",
            [](const std::string& file_path)
            {
                if(!file_path.empty())
                {
                    AppWindow::GetInstance()->OpenFile(file_path);
                }
            }
        );
    }

    ImGui::EndChild();
}

void WelcomeView::RenderRunProfilingCard()
{
    float card_width = 360.0f;
    float card_height = 200.0f;

    ImGui::BeginChild("##run_profiling_card", ImVec2(card_width, card_height), true,
                      ImGuiWindowFlags_NoScrollbar);

    ImVec2 card_pos = ImGui::GetCursorPos();

    // Check if mouse is hovering over the card
    ImVec2 mouse_pos = ImGui::GetMousePos();
    ImVec2 card_screen_pos = ImGui::GetWindowPos();
    card_screen_pos.x += card_pos.x;
    card_screen_pos.y += card_pos.y;
    bool is_hovered = mouse_pos.x >= card_screen_pos.x &&
                      mouse_pos.x <= card_screen_pos.x + card_width &&
                      mouse_pos.y >= card_screen_pos.y &&
                      mouse_pos.y <= card_screen_pos.y + card_height &&
                      ImGui::IsWindowHovered(ImGuiHoveredFlags_None);

    // Highlight on hover
    if(is_hovered)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 min = card_screen_pos;
        ImVec2 max = ImVec2(card_screen_pos.x + card_width, card_screen_pos.y + card_height);
        ImU32 col = IM_COL32(80, 80, 120, 100);
        draw_list->AddRectFilled(min, max, col, 4.0f);
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Icon (rocket emoji as placeholder)
    const char* icon = "  Run Profiling";
    float icon_width = ImGui::CalcTextSize(icon).x;
    ImGui::SetCursorPosX((card_width - icon_width) * 0.5f);
    ImGui::TextUnformatted(icon);

    ImGui::Spacing();
    ImGui::Spacing();

    // Description
    const char* desc = "Profile an application\nlocally or via SSH";
    float desc_width = ImGui::CalcTextSize(desc).x;
    ImGui::SetCursorPosX((card_width - desc_width) * 0.5f);
    ImGui::TextWrapped("%s", desc);

    ImGui::Spacing();
    ImGui::Spacing();

    // Button
    float button_width = 150.0f;
    ImGui::SetCursorPosX((card_width - button_width) * 0.5f);
    if(ImGui::Button("Start Profiling", ImVec2(button_width, 0)))
    {
        m_show_profiling_dialog = true;
    }

    ImGui::EndChild();
}

void WelcomeView::ShowProfilingDialog()
{
    // Use AppWindow's centralized profiling dialog instead of creating our own
    // This ensures all profiling uses the same dialog instance and callbacks
    if(m_show_profiling_dialog)
    {
        AppWindow::GetInstance()->ShowProfilingDialog();
        m_show_profiling_dialog = false;
    }
}

void WelcomeView::RenderRecentFiles()
{
    if(m_recent_files.empty())
    {
        return;
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextUnformatted("Recent Files:");
    ImGui::Spacing();

    for(size_t i = 0; i < m_recent_files.size() && i < 5; ++i)
    {
        const auto& file = m_recent_files[i];

        ImGui::PushID(static_cast<int>(i));

        // Make the file name clickable
        if(ImGui::Selectable(file.file_name.c_str(), false))
        {
            AppWindow::GetInstance()->OpenFile(file.file_path);
        }

        // Show tooltip with full path and last opened time
        if(ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(file.file_path.c_str());
            ImGui::TextDisabled("Last opened: %s", file.last_opened.c_str());
            ImGui::EndTooltip();
        }

        ImGui::PopID();
    }
}

void WelcomeView::LoadRecentFiles()
{
    m_recent_files.clear();

    const auto& recent_files_list = m_settings_manager.GetInternalSettings().recent_files;

    for(const auto& file_path : recent_files_list)
    {
        RecentFile rf;
        rf.file_path = file_path;

        // Extract file name
        std::filesystem::path path(file_path);
        rf.file_name = path.filename().string();

        // Get last modified time (as a proxy for last opened if we don't track it separately)
        try
        {
            if(std::filesystem::exists(path))
            {
                auto ftime = std::filesystem::last_write_time(path);
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    ftime - std::filesystem::file_time_type::clock::now() +
                    std::chrono::system_clock::now()
                );
                auto cftime = std::chrono::system_clock::to_time_t(sctp);
                std::tm* tm = std::localtime(&cftime);
                std::ostringstream oss;
                oss << std::put_time(tm, "%b %d, %Y");
                rf.last_opened = oss.str();
            }
            else
            {
                rf.last_opened = "File not found";
            }
        }
        catch(...)
        {
            rf.last_opened = "Unknown";
        }

        m_recent_files.push_back(rf);
    }
}

}  // namespace View
}  // namespace RocProfVis
