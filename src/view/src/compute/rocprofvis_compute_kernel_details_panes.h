// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "imgui.h"
#include "json.h"
#include "rocprofvis_compute_memory_chart.h"

#include <cstdint>
#include <memory>

namespace RocProfVis
{
namespace View
{

class ComputeIsaView;
class ComputeSelection;
class ComputeTableView;
class ComputeWorkloadView;
class DataProvider;
class Roofline;

// What a panel shows while comparing: the selected kernel (A), the compare
// target (B), or B - A.
enum class CompareSide : uint8_t
{
    kA,
    kB,
    kDelta,
};

// The comparison as the panels read it; ComputeKernelDetailsView implements it.
class IKernelCompare
{
public:
    virtual ~IKernelCompare() = default;

    virtual bool     IsComparing() const       = 0;
    // B, the compare target.
    virtual uint32_t CompareWorkloadId() const = 0;
    virtual uint32_t CompareKernelId() const   = 0;
    // B - A reads B's values through A's memory chart, so it needs A and B on
    // one GPU architecture.
    virtual bool     DeltaAvailable() const    = 0;
};

// One panel of the Kernel Details workspace. ComputeKernelDetailsView places it
// in a box and draws its card and header (title, panel picker, maximize, hide),
// so a wrapped widget skips its own header; the panel adds its own header
// buttons, draws its body, reports the smallest box it fits in and shows the
// comparison its own way.
class ComputeKernelDetailsPane
{
public:
    // One header button; either an icon-font glyph or a text label. `id` is
    // what OnHeaderButton() is given when it is clicked.
    struct HeaderButton
    {
        const char* icon;
        const char* label;
        const char* tooltip;
        bool        active;
        int32_t     id;
    };
    // Most header buttons a panel adds (A, B, B - A, Fit).
    static constexpr int32_t MAX_HEADER_BUTTONS = 4;

    explicit ComputeKernelDetailsPane(const IKernelCompare& compare);
    virtual ~ComputeKernelDetailsPane();

    virtual const char* Title() const = 0;
    // False when the database lacks the panel's data.
    virtual bool        IsAvailable() const;
    // Fills `buttons` (room for MAX_HEADER_BUTTONS) with the panel's header
    // buttons, which precede maximize and hide; returns how many.
    virtual int32_t     HeaderButtons(HeaderButton* buttons) const;
    virtual void        OnHeaderButton(int32_t id);
    // `maximized`: the panel fills the tab rather than a box.
    virtual void        RenderBody(bool maximized) = 0;
    // Smallest box the panel fits in, in pixels at the current font.
    virtual ImVec2      MinSize() const;
    virtual void        Update();
    // Whether the panel is drawn this frame: in a box, or maximized.
    virtual void        SetShown(bool shown);

    virtual void OnWorkloadSelected(uint32_t workload_id);
    virtual void OnKernelSelected(uint32_t kernel_id);
    // A memory chart's metrics have arrived for the client `client_id`.
    virtual void OnMetricsFetched(uint64_t client_id);
    // Comparing has started; ApplyCompare follows.
    virtual void OnCompareStarted();
    // A and B are about to trade places (the selection still is the old A);
    // ApplyCompare follows.
    virtual void OnCompareSwapping();
    // Shows the comparison as it stands; `target_changed` refetches B.
    virtual void ApplyCompare(bool target_changed);

    // Settings saved with the layout in user presets, as keys of the layout's
    // JSON object. LoadLayout returns false on a key of the wrong type.
    virtual void SaveLayout(jt::Json& json) const;
    virtual bool LoadLayout(jt::Json& json);
    virtual void ResetLayout();

    // Smallest box of a panel that needs no more, and of an empty box.
    static ImVec2 DefaultMinSize();

protected:
    static void AddHeaderButton(HeaderButton* buttons, int32_t& count,
                                const HeaderButton& button);

    const IKernelCompare& m_compare;
};

// A panel that shows A or B while comparing, picked by its A / B header buttons.
class ComputeSidedPane : public ComputeKernelDetailsPane
{
public:
    explicit ComputeSidedPane(const IKernelCompare& compare);

    int32_t HeaderButtons(HeaderButton* buttons) const override;
    void    OnHeaderButton(int32_t id) override;
    void    OnCompareStarted() override;

    friend struct ComputeKernelDetailsViewTestPeer;

protected:
    virtual const char* ShowATooltip() const;
    virtual const char* ShowBTooltip() const;
    // m_side while comparing, else A.
    CompareSide ShownSide() const;
    void        ShowSide(CompareSide side);

    CompareSide m_side;
};

// The memory chart of A, of B, or (A and B on one GPU architecture) B - A.
class ComputeMemoryChartPane : public ComputeSidedPane
{
public:
    ComputeMemoryChartPane(const IKernelCompare& compare, DataProvider& data_provider,
                           std::shared_ptr<ComputeSelection> compute_selection);

    const char* Title() const override;
    int32_t     HeaderButtons(HeaderButton* buttons) const override;
    void        OnHeaderButton(int32_t id) override;
    void        RenderBody(bool maximized) override;
    void        OnWorkloadSelected(uint32_t workload_id) override;
    void        OnKernelSelected(uint32_t kernel_id) override;
    void        OnMetricsFetched(uint64_t client_id) override;
    void        OnCompareStarted() override;
    void        ApplyCompare(bool target_changed) override;
    void        SaveLayout(jt::Json& json) const override;
    bool        LoadLayout(jt::Json& json) override;
    void        ResetLayout() override;

protected:
    const char* ShowBTooltip() const override;

private:
    // A and B share one Fit setting, so switching sides keeps the view.
    void                          SetFit(bool fit);
    const ComputeMemoryChartView& DisplayedChart() const;
    ComputeMemoryChartView&       DisplayedChart();

    ComputeMemoryChartView m_chart;
    // The compare target's chart; its own layout, so B of another GPU
    // architecture draws its own hierarchy.
    ComputeMemoryChartView m_chart_b;
    // Workload whose layout m_chart_b has loaded.
    uint32_t               m_chart_b_workload;
};

// The roofline; it plots A and B together while comparing.
class ComputeRooflinePane : public ComputeKernelDetailsPane
{
public:
    ComputeRooflinePane(const IKernelCompare& compare, DataProvider& data_provider);

    const char* Title() const override;
    void        RenderBody(bool maximized) override;
    ImVec2      MinSize() const override;
    void        Update() override;
    void        OnWorkloadSelected(uint32_t workload_id) override;
    void        OnKernelSelected(uint32_t kernel_id) override;
    void        ApplyCompare(bool target_changed) override;

    friend struct ComputeKernelDetailsViewTestPeer;

private:
    std::shared_ptr<Roofline> m_roofline;
};

// The metric tables; they add B and difference columns while comparing.
class ComputeMetricTablesPane : public ComputeKernelDetailsPane
{
public:
    ComputeMetricTablesPane(const IKernelCompare&             compare,
                            std::shared_ptr<ComputeTableView> table_view);

    const char* Title() const override;
    void        RenderBody(bool maximized) override;
    ImVec2      MinSize() const override;
    void        Update() override;
    void        OnCompareSwapping() override;
    void        ApplyCompare(bool target_changed) override;

    friend struct ComputeKernelDetailsViewTestPeer;

private:
    std::shared_ptr<ComputeTableView> m_table_view;
};

// The metric tables' pins in a panel of their own, so they stay in view; the
// Metric Tables panel leaves them out while this one is shown.
class ComputePinnedMetricsPane : public ComputeKernelDetailsPane
{
public:
    ComputePinnedMetricsPane(const IKernelCompare&             compare,
                             std::shared_ptr<ComputeTableView> table_view);

    const char* Title() const override;
    void        RenderBody(bool maximized) override;
    void        SetShown(bool shown) override;

private:
    std::shared_ptr<ComputeTableView> m_table_view;
};

// The ISA of A or B: one view that shows B's kernel, since the data provider
// has a single PC-sampling callback.
class ComputeIsaPane : public ComputeSidedPane
{
public:
    ComputeIsaPane(const IKernelCompare& compare, DataProvider& data_provider,
                   bool has_isa_lines);

    const char* Title() const override;
    bool        IsAvailable() const override;
    void        RenderBody(bool maximized) override;
    void        Update() override;
    void        ApplyCompare(bool target_changed) override;

    friend struct ComputeKernelDetailsViewTestPeer;

private:
    // Null when the database has no ISA lines.
    std::shared_ptr<ComputeIsaView> m_isa_view;
};

// The workload details of A's or B's workload.
class ComputeWorkloadPane : public ComputeSidedPane
{
public:
    ComputeWorkloadPane(const IKernelCompare& compare, DataProvider& data_provider,
                        std::shared_ptr<ComputeSelection> compute_selection);

    const char* Title() const override;
    void        RenderBody(bool maximized) override;
    void        Update() override;
    void        ApplyCompare(bool target_changed) override;

    friend struct ComputeKernelDetailsViewTestPeer;

protected:
    const char* ShowATooltip() const override;
    const char* ShowBTooltip() const override;

private:
    std::shared_ptr<ComputeSelection>    m_compute_selection;
    std::shared_ptr<ComputeWorkloadView> m_workload_view;
};

}  // namespace View
}  // namespace RocProfVis
