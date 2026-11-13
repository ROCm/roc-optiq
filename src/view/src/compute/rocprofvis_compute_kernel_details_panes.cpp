// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_kernel_details_panes.h"
#include "isa/rocprofvis_compute_isa_view.h"
#include "rocprofvis_compute_roofline.h"
#include "rocprofvis_compute_selection.h"
#include "rocprofvis_compute_table_view.h"
#include "rocprofvis_compute_workload_view.h"

namespace
{
// Smallest box of a panel that needs no more, in frame heights / font sizes so
// it scales with the UI font.
constexpr float PANE_MIN_FRAMES    = 5.0f;
constexpr float PANE_MIN_WIDTH_EMS = 12.0f;
// The roofline's three labelled peak / bandwidth combos share one row.
constexpr float ROOFLINE_MIN_WIDTH_EMS = 44.0f;
// The metric tables' toolbar row: the table picker and expand / collapse, and
// while comparing the Compare options too.
constexpr float METRIC_TABLES_MIN_WIDTH_FRAMES         = 18.0f;
constexpr float METRIC_TABLES_COMPARE_MIN_WIDTH_FRAMES = 25.0f;

// Header button ids.
constexpr int32_t SHOW_A_BUTTON     = 0;
constexpr int32_t SHOW_B_BUTTON     = 1;
constexpr int32_t SHOW_DELTA_BUTTON = 2;
constexpr int32_t FIT_BUTTON        = 3;

constexpr const char* JSON_KEY_LAYOUT_FIT_MEMORY_CHART = "fit_memory_chart";
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

ComputeMemoryChartPane::ComputeMemoryChartPane(
    const IKernelCompare& compare, DataProvider& data_provider,
    std::shared_ptr<ComputeSelection> compute_selection)
: ComputeSidedPane(compare)
, m_chart(data_provider, compute_selection)
, m_chart_b(data_provider, compute_selection)
, m_chart_b_workload(ComputeSelection::INVALID_SELECTION_ID)
{
    // B's chart never follows the selection.
    m_chart_b.SetSource(ComputeSelection::INVALID_SELECTION_ID,
                        ComputeSelection::INVALID_SELECTION_ID);
}

const char*
ComputeMemoryChartPane::Title() const
{
    return "Memory Chart";
}

int32_t
ComputeMemoryChartPane::HeaderButtons(HeaderButton* buttons) const
{
    int32_t count = ComputeSidedPane::HeaderButtons(buttons);
    if(m_compare.IsComparing() && m_compare.DeltaAvailable())
    {
        AddHeaderButton(buttons, count,
                        { nullptr, "\xCE\x94", "Show B - A (how B differs from A)",
                          m_side == CompareSide::kDelta, SHOW_DELTA_BUTTON });
    }
    // A compact (overview) chart is always fitted, so Fit only applies at size.
    if(!DisplayedChart().IsCompact())
    {
        AddHeaderButton(buttons, count,
                        { nullptr, "Fit",
                          "Shrink the whole chart to fit the box "
                          "(off: actual size, scroll)",
                          m_chart.GetFitToView(), FIT_BUTTON });
    }
    return count;
}

void
ComputeMemoryChartPane::OnHeaderButton(int32_t id)
{
    if(id == SHOW_DELTA_BUTTON)
    {
        ShowSide(CompareSide::kDelta);
    }
    else if(id == FIT_BUTTON)
    {
        SetFit(!m_chart.GetFitToView());
    }
    else
    {
        ComputeSidedPane::OnHeaderButton(id);
    }
}

void
ComputeMemoryChartPane::RenderBody(bool maximized)
{
    // Maximized it is always at actual size; in a box it falls back to a
    // fitted overview when the box is too small to read it.
    ComputeMemoryChartView& chart = DisplayedChart();
    chart.SetAutoCompact(!maximized);
    chart.Render();
}

void
ComputeMemoryChartPane::OnWorkloadSelected(uint32_t workload_id)
{
    m_chart.LoadWorkloadLayout(workload_id);
}

void
ComputeMemoryChartPane::OnKernelSelected(uint32_t)
{
    m_chart.FetchMemChartMetrics();
}

void
ComputeMemoryChartPane::OnMetricsFetched(uint64_t client_id)
{
    if(m_chart.GetClientId() == client_id)
    {
        m_chart.UpdateMetrics();
    }
    else if(m_chart_b.GetClientId() == client_id)
    {
        m_chart_b.UpdateMetrics();
        // A's B - A values read B's metrics.
        m_chart.RefreshValues();
    }
}

void
ComputeMemoryChartPane::OnCompareStarted()
{
    // Open on the difference where there is one to show.
    m_side = m_compare.DeltaAvailable() ? CompareSide::kDelta : CompareSide::kA;
}

void
ComputeMemoryChartPane::ApplyCompare(bool target_changed)
{
    if(m_compare.IsComparing())
    {
        const uint32_t workload_id = m_compare.CompareWorkloadId();
        if(target_changed)
        {
            if(workload_id != m_chart_b_workload)
            {
                m_chart_b.LoadWorkloadLayout(workload_id);
                m_chart_b_workload = workload_id;
            }
            m_chart_b.SetSource(workload_id, m_compare.CompareKernelId());
            m_chart_b.FetchMemChartMetrics();
        }
        if(m_side == CompareSide::kDelta && !m_compare.DeltaAvailable())
        {
            m_side = CompareSide::kA;
        }
    }
    m_chart.SetDeltaTarget(ShownSide() == CompareSide::kDelta ? &m_chart_b : nullptr);
}

void
ComputeMemoryChartPane::SaveLayout(jt::Json& json) const
{
    json[JSON_KEY_LAYOUT_FIT_MEMORY_CHART] = m_chart.GetFitToView();
}

bool
ComputeMemoryChartPane::LoadLayout(jt::Json& json)
{
    bool result = true;
    if(json.contains(JSON_KEY_LAYOUT_FIT_MEMORY_CHART))
    {
        if(json[JSON_KEY_LAYOUT_FIT_MEMORY_CHART].isBool())
        {
            SetFit(json[JSON_KEY_LAYOUT_FIT_MEMORY_CHART].getBool());
        }
        else
        {
            result = false;
        }
    }
    return result;
}

void
ComputeMemoryChartPane::ResetLayout()
{
    SetFit(false);
}

const char*
ComputeMemoryChartPane::ShowBTooltip() const
{
    return m_compare.DeltaAvailable()
               ? ComputeSidedPane::ShowBTooltip()
               : "Show B (the compare target). B - A needs A and B on the\n"
                 "same GPU architecture.";
}

void
ComputeMemoryChartPane::SetFit(bool fit)
{
    m_chart.SetFitToView(fit);
    m_chart_b.SetFitToView(fit);
}

const ComputeMemoryChartView&
ComputeMemoryChartPane::DisplayedChart() const
{
    return ShownSide() == CompareSide::kB ? m_chart_b : m_chart;
}

ComputeMemoryChartView&
ComputeMemoryChartPane::DisplayedChart()
{
    return ShownSide() == CompareSide::kB ? m_chart_b : m_chart;
}

ComputeRooflinePane::ComputeRooflinePane(const IKernelCompare& compare,
                                         DataProvider&         data_provider)
: ComputeKernelDetailsPane(compare)
, m_roofline(std::make_shared<Roofline>(data_provider, Roofline::SingleKernel))
{
    m_roofline->SetChromeless(true);
}

const char*
ComputeRooflinePane::Title() const
{
    return "Roofline";
}

void
ComputeRooflinePane::RenderBody(bool)
{
    m_roofline->Render();
}

ImVec2
ComputeRooflinePane::MinSize() const
{
    return ImVec2(ImGui::GetFontSize() * ROOFLINE_MIN_WIDTH_EMS, DefaultMinSize().y);
}

void
ComputeRooflinePane::Update()
{
    m_roofline->Update();
}

void
ComputeRooflinePane::OnWorkloadSelected(uint32_t workload_id)
{
    m_roofline->SetWorkload(workload_id);
}

void
ComputeRooflinePane::OnKernelSelected(uint32_t kernel_id)
{
    m_roofline->SetKernel(kernel_id);
}

void
ComputeRooflinePane::ApplyCompare(bool target_changed)
{
    if(m_compare.IsComparing())
    {
        m_roofline->SetMode(Roofline::Compare);
        if(target_changed)
        {
            m_roofline->SetCompareTarget(m_compare.CompareWorkloadId(),
                                         m_compare.CompareKernelId());
        }
    }
    else
    {
        m_roofline->SetMode(Roofline::SingleKernel);
    }
}

ComputeMetricTablesPane::ComputeMetricTablesPane(
    const IKernelCompare& compare, DataProvider& data_provider,
    std::shared_ptr<ComputeSelection> compute_selection, bool has_available_metrics)
: ComputeKernelDetailsPane(compare)
, m_table_view(std::make_shared<ComputeTableView>(data_provider, compute_selection,
                                                  has_available_metrics))
{
    m_table_view->SetChromeless(true);
}

const char*
ComputeMetricTablesPane::Title() const
{
    return "Metric Tables";
}

void
ComputeMetricTablesPane::RenderBody(bool)
{
    m_table_view->Render();
}

ImVec2
ComputeMetricTablesPane::MinSize() const
{
    const float frames = m_compare.IsComparing() ? METRIC_TABLES_COMPARE_MIN_WIDTH_FRAMES
                                                 : METRIC_TABLES_MIN_WIDTH_FRAMES;
    return ImVec2(ImGui::GetFrameHeight() * frames, DefaultMinSize().y);
}

void
ComputeMetricTablesPane::Update()
{
    m_table_view->Update();
}

void
ComputeMetricTablesPane::OnCompareSwapping()
{
    m_table_view->SwapComparePins();
}

void
ComputeMetricTablesPane::ApplyCompare(bool)
{
    if(m_compare.IsComparing())
    {
        m_table_view->SetCompareTarget(m_compare.CompareWorkloadId(),
                                       m_compare.CompareKernelId());
    }
    else
    {
        m_table_view->ClearCompareTarget();
    }
}

ComputeIsaPane::ComputeIsaPane(const IKernelCompare& compare, DataProvider& data_provider,
                               bool has_isa_lines)
: ComputeSidedPane(compare)
, m_isa_view(has_isa_lines ? std::make_shared<ComputeIsaView>(data_provider) : nullptr)
{}

const char*
ComputeIsaPane::Title() const
{
    return "ISA";
}

bool
ComputeIsaPane::IsAvailable() const
{
    return m_isa_view != nullptr;
}

void
ComputeIsaPane::RenderBody(bool)
{
    if(m_isa_view)
    {
        m_isa_view->Render();
    }
}

void
ComputeIsaPane::Update()
{
    if(m_isa_view)
    {
        m_isa_view->Update();
    }
}

void
ComputeIsaPane::ApplyCompare(bool)
{
    if(m_isa_view)
    {
        if(ShownSide() == CompareSide::kB)
        {
            m_isa_view->ShowKernel(m_compare.CompareWorkloadId(),
                                   m_compare.CompareKernelId());
        }
        else
        {
            m_isa_view->FollowSelection();
        }
    }
}

ComputeWorkloadPane::ComputeWorkloadPane(
    const IKernelCompare& compare, DataProvider& data_provider,
    std::shared_ptr<ComputeSelection> compute_selection)
: ComputeSidedPane(compare)
, m_compute_selection(compute_selection)
, m_workload_view(std::make_shared<ComputeWorkloadView>(data_provider, compute_selection))
{}

const char*
ComputeWorkloadPane::Title() const
{
    return "Workload Details";
}

void
ComputeWorkloadPane::RenderBody(bool)
{
    // B of A's workload shows the same details, so say it is not stale.
    if(ShownSide() == CompareSide::kB &&
       m_compare.CompareWorkloadId() == m_compute_selection->GetSelectedWorkload())
    {
        ImGui::TextDisabled("A and B are in the same workload.");
    }
    m_workload_view->Render();
}

void
ComputeWorkloadPane::Update()
{
    m_workload_view->Update();
}

void
ComputeWorkloadPane::ApplyCompare(bool)
{
    if(ShownSide() == CompareSide::kB)
    {
        m_workload_view->ShowWorkload(m_compare.CompareWorkloadId());
    }
    else
    {
        m_workload_view->FollowSelection();
    }
}

const char*
ComputeWorkloadPane::ShowATooltip() const
{
    return "Show A's workload (the selected kernel's)";
}

const char*
ComputeWorkloadPane::ShowBTooltip() const
{
    return "Show B's workload (the compare target's)";
}

}  // namespace View
}  // namespace RocProfVis
