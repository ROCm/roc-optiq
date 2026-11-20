// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_kernel_details_panes.h"

namespace
{
// Smallest box of a panel that needs no more, in frame heights / font sizes so
// it scales with the UI font.
constexpr float PANE_MIN_FRAMES    = 5.0f;
constexpr float PANE_MIN_WIDTH_EMS = 12.0f;
}  // namespace

namespace RocProfVis
{
namespace View
{

ComputeKernelDetailsPane::ComputeKernelDetailsPane(const IKernelCompare& compare)
: m_compare(compare)
{}

ComputeKernelDetailsPane::~ComputeKernelDetailsPane() = default;

bool
ComputeKernelDetailsPane::IsAvailable() const
{
    return true;
}

int32_t
ComputeKernelDetailsPane::HeaderButtons(HeaderButton*) const
{
    return 0;
}

void
ComputeKernelDetailsPane::OnHeaderButton(int32_t)
{}

ImVec2
ComputeKernelDetailsPane::MinSize() const
{
    return DefaultMinSize();
}

void
ComputeKernelDetailsPane::Update()
{}

void
ComputeKernelDetailsPane::SetShown(bool)
{}

void
ComputeKernelDetailsPane::OnWorkloadSelected(uint32_t)
{}

void
ComputeKernelDetailsPane::OnKernelSelected(uint32_t)
{}

void
ComputeKernelDetailsPane::OnMetricsFetched(uint64_t)
{}

void
ComputeKernelDetailsPane::OnCompareStarted()
{}

void
ComputeKernelDetailsPane::OnCompareSwapping()
{}

void
ComputeKernelDetailsPane::ApplyCompare(bool)
{}

void
ComputeKernelDetailsPane::SaveLayout(jt::Json&) const
{}

bool
ComputeKernelDetailsPane::LoadLayout(jt::Json&)
{
    return true;
}

void
ComputeKernelDetailsPane::ResetLayout()
{}

ImVec2
ComputeKernelDetailsPane::DefaultMinSize()
{
    return ImVec2(ImGui::GetFontSize() * PANE_MIN_WIDTH_EMS,
                  ImGui::GetFrameHeightWithSpacing() * PANE_MIN_FRAMES);
}

void
ComputeKernelDetailsPane::AddHeaderButton(HeaderButton* buttons, int32_t& count,
                                          const HeaderButton& button)
{
    IM_ASSERT(count < MAX_HEADER_BUTTONS);
    buttons[count] = button;
    count++;
}

ComputeSidedPane::ComputeSidedPane(const IKernelCompare& compare)
: ComputeKernelDetailsPane(compare)
, m_side(CompareSide::kA)
{}

int32_t
ComputeSidedPane::HeaderButtons(HeaderButton* buttons) const
{
    int32_t count = 0;
    if(m_compare.IsComparing())
    {
        AddHeaderButton(buttons, count,
                        { nullptr, "A", ShowATooltip(), m_side == CompareSide::kA,
                          SHOW_A_BUTTON });
        AddHeaderButton(buttons, count,
                        { nullptr, "B", ShowBTooltip(), m_side == CompareSide::kB,
                          SHOW_B_BUTTON });
    }
    return count;
}

void
ComputeSidedPane::OnHeaderButton(int32_t id)
{
    if(id == SHOW_A_BUTTON)
    {
        ShowSide(CompareSide::kA);
    }
    else if(id == SHOW_B_BUTTON)
    {
        ShowSide(CompareSide::kB);
    }
}

void
ComputeSidedPane::OnCompareStarted()
{
    m_side = CompareSide::kA;
}

const char*
ComputeSidedPane::ShowATooltip() const
{
    return "Show A (the selected kernel)";
}

const char*
ComputeSidedPane::ShowBTooltip() const
{
    return "Show B (the compare target)";
}

CompareSide
ComputeSidedPane::ShownSide() const
{
    return m_compare.IsComparing() ? m_side : CompareSide::kA;
}

void
ComputeSidedPane::ShowSide(CompareSide side)
{
    m_side = side;
    ApplyCompare(false);
}

}  // namespace View
}  // namespace RocProfVis
