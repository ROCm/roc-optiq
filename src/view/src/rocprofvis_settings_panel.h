#pragma once

// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

namespace RocProfVis
{
namespace View
{

class SettingsPanel
{
public:
    SettingsPanel();
    ~SettingsPanel();

    void Render();

    // Getter and setter for open state
    bool IsOpen();
    void SetOpen(bool);

private:
    bool m_is_open = false;
};

}  // namespace View
}  // namespace RocProfVis