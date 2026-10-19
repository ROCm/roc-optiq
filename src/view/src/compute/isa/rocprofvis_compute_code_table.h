// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

#include "imgui.h"

namespace RocProfVis
{
namespace View
{

// Shared table lifetime, row styles, and headers for the compute code panes.
class ComputeCodeTable
{
public:
    struct HeaderTooltipText
    {
        const char* user_description;
        const char* developer_information;
    };

    ComputeCodeTable(const char* id, int32_t columns, ImGuiTableFlags flags,
                     const ImVec2& window_padding);
    ~ComputeCodeTable();

    ComputeCodeTable(const ComputeCodeTable&)            = delete;
    ComputeCodeTable& operator=(const ComputeCodeTable&) = delete;

    [[nodiscard]] bool IsOpen() const { return m_open; }

    void BeginRows(ImU32 background_color);

    static void RenderCenteredHeader(int column, const char* label);
    static void RenderHeaderWithTooltip(int column, const char* label,
                                        const HeaderTooltipText& tooltip,
                                        void (*render_user_content)() = nullptr);

private:
    bool m_open              = false;
    bool m_row_styles_pushed = false;
};

}  // namespace View
}  // namespace RocProfVis
