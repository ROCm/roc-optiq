// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "imgui.h"
#include <deque>
#include <string>

namespace RocProfVis
{
namespace View
{

// Enum indicating notification type
enum class NotificationLevel
{
    Info,
    Success,
    Warning,
    Error
};

struct Notification
{
    std::string       id;
    std::string       message;
    NotificationLevel level;

    // Timing
    double start_time;
    float  duration;
    float  fade_duration;

    // State
    bool   is_persistent;
    bool   is_hiding;
    double hide_time;  // Time when Hide() was called

    // Calculate current opacity based on the notification's state and time
    float GetOpacity(double current_time) const;

    // A notification is ready to be removed after it has fully faded out
    bool IsExpired(double current_time) const;
};

class NotificationManager
{
public:
    static NotificationManager& GetInstance()
    {
        static NotificationManager instance;
        return instance;
    }

    NotificationManager(const NotificationManager&) = delete;
    void operator=(const NotificationManager&)      = delete;

    void Show(const std::string& message,
              NotificationLevel  level = NotificationLevel::Info);
    void Show(const std::string& message, NotificationLevel level, double duration);
    void ShowPersistent(const std::string& id, const std::string& message,
                        NotificationLevel level = NotificationLevel::Info);
    void Hide(const std::string& id);
    void ClearAll();
    void Render();

private:
    NotificationManager();

    ImVec4 GetBgColorForLevel(NotificationLevel level);
    ImVec4 GetFgColorForLevel(NotificationLevel level);

    std::deque<Notification> m_notifications;
    float                    m_default_duration;
    float                    m_default_fade_duration;

    float m_notification_spacing;
};

}  // namespace View
}  // namespace RocProfVis
