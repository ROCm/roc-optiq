// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_notification_manager.h"
#include "rocprofvis_settings_manager.h"
#include "imgui.h"
#include <algorithm>

#include "spdlog/spdlog.h"

namespace RocProfVis
{
namespace View
{

float
Notification::GetOpacity(double current_time) const
{
    double elapsed_time;

    if(is_hiding)
    {
        // Fading out because Hide() was called
        elapsed_time        = current_time - hide_time;
        float fade_progress = 1.0f - static_cast<float>(elapsed_time / fade_duration);
        return std::max(0.0f, fade_progress);
    }

    elapsed_time = current_time - start_time;

    // Fading In
    if(elapsed_time < fade_duration)
    {
        return static_cast<float>(elapsed_time / fade_duration);
    }

    // Fully Visible
    if(is_persistent || elapsed_time < fade_duration + duration)
    {
        return 1.0f;
    }

    // Fading Out (for timed notifications)
    elapsed_time -= (fade_duration + duration);
    float fade_progress = 1.0f - static_cast<float>(elapsed_time / fade_duration);
    return std::max(0.0f, fade_progress);
}

bool
Notification::IsExpired(double current_time) const
{
    if(is_hiding)
    {
        return current_time > hide_time + fade_duration;
    }
    if(is_persistent)
    {
        return false;  // Persistent notifications only expire when explicitly hidden
    }
    return current_time > start_time + duration + fade_duration;
}

// --- NotificationManager Method Implementations ---

NotificationManager::NotificationManager()
: m_default_duration(1.5f)
, m_default_fade_duration(0.25f)
, m_notification_spacing(5.0f)
, m_next_uid(0)
{}

void
NotificationManager::Show(const std::string& message, NotificationLevel level)
{
    Show(message, level, m_default_duration);
}

void
NotificationManager::Show(const std::string& message, NotificationLevel level,
                          double duration)
{
    m_notifications.push_back({ "",  // No ID needed for timed notifications
                                message, level, ImGui::GetTime(),
                                static_cast<float>(duration), m_default_fade_duration,
                                false,  // is_persistent
                                false,  // is_hiding
                                0.0 });
    auto& n = m_notifications.back();
    n.uid = "##note_" + std::to_string(++m_next_uid);
}

void
NotificationManager::ShowPersistent(const std::string& id, const std::string& message,
                                    NotificationLevel level)
{
    auto it = std::find_if(m_notifications.begin(), m_notifications.end(),
                           [&](const Notification& n) { return n.id == id; });

    if(it != m_notifications.end())
    {
        // Update existing notification
        it->message    = message;
        it->level      = level;
        it->start_time = ImGui::GetTime();  // Reset start time to fade in again
        it->is_hiding  = false;             // If it was hiding, show it again
    }
    else
    {
        // Add a new persistent notification
        m_notifications.push_back({ id, message, level, ImGui::GetTime(),
                                    0.0f,  // duration is irrelevant for persistent
                                    m_default_fade_duration,
                                    true,   // is_persistent
                                    false,  // is_hiding
                                    0.0 });
        auto& n = m_notifications.back();
        n.uid = "##note_" + std::to_string(++m_next_uid);

    }
}

void
NotificationManager::Hide(const std::string& id)
{
    auto it =
        std::find_if(m_notifications.begin(), m_notifications.end(),
                     [&](const Notification& n) { return n.id == id && !n.is_hiding; });

    if(it != m_notifications.end())
    {
        it->is_hiding = true;
        it->hide_time = ImGui::GetTime();
    }
}

void
NotificationManager::ClearAll()
{
    m_notifications.clear();
}

void
NotificationManager::Render()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2               base_pos = viewport->WorkPos;
    float                y_offset = 0.0f;
    const float          padding  = 10.0f;

    // Use fixed height for notifications
    float notification_height = ImGui::GetTextLineHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f;

    double current_time = ImGui::GetTime();

    for(auto it = m_notifications.rbegin(); it != m_notifications.rend(); ++it)
    {
        auto& notification = *it;
        float       opacity      = notification.GetOpacity(current_time);

        if(opacity <= 0.0f)
        {
            continue;  // Skip rendering if fully transparent
        }

        ImVec2 window_pos(base_pos.x + viewport->WorkSize.x - padding,
                          base_pos.y + viewport->WorkSize.y - padding - y_offset);
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always,
                                ImVec2(1.0f, 1.0f));  // Pivot at bottom-right
        ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, notification_height), ImVec2(FLT_MAX, notification_height));
        ImGui::SetNextWindowBgAlpha(opacity);
        ImVec4 bg_color = GetBgColorForLevel(notification.level);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

        ImGui::PushStyleColor(ImGuiCol_WindowBg,
                              ImVec4(bg_color.x, bg_color.y, bg_color.z, opacity));
        ImGui::Begin(notification.uid.c_str(), NULL, flags);

        ImVec4 text_color = GetFgColorForLevel(notification.level);
        text_color.w      = opacity;
        ImGui::PushStyleColor(ImGuiCol_Text, text_color);
        ImGui::TextUnformatted(notification.message.c_str());
        ImGui::PopStyleColor();

        y_offset += notification_height + m_notification_spacing;

        ImGui::End();
        ImGui::PopStyleColor();

        // Check if we need to hide (start fading) the notification
        if(!notification.is_persistent && !notification.is_hiding &&
           current_time > notification.start_time + notification.duration)
        {
            notification.is_hiding = true;
            notification.hide_time = current_time;
        }
    }

    // Clean up expired notifications
    m_notifications.erase(std::remove_if(m_notifications.begin(), m_notifications.end(),
                                         [current_time](const Notification& n) {
                                             return n.IsExpired(current_time);
                                         }),
                          m_notifications.end());

}

ImVec4
NotificationManager::GetBgColorForLevel(NotificationLevel level)
{
    switch(level)
    {
        default:
        case NotificationLevel::Info:
        case NotificationLevel::Success: 
            return ImGui::ColorConvertU32ToFloat4(SettingsManager::GetInstance().GetColor(Colors::kBgPanel));
        case NotificationLevel::Warning: 
            return ImGui::ColorConvertU32ToFloat4(SettingsManager::GetInstance().GetColor(Colors::kBgWarning));
        case NotificationLevel::Error: 
            return ImGui::ColorConvertU32ToFloat4(SettingsManager::GetInstance().GetColor(Colors::kBgError));
    }
}

ImVec4
NotificationManager::GetFgColorForLevel(NotificationLevel level)
{
    switch(level)
    {
        default:
        case NotificationLevel::Info:
        case NotificationLevel::Success:
        case NotificationLevel::Warning:
        case NotificationLevel::Error: 
            return ImGui::ColorConvertU32ToFloat4(SettingsManager::GetInstance().GetColor(Colors::kTextMain));
    }
}

}  // namespace View
}  // namespace RocProfVis
