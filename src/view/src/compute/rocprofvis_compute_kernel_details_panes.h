// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "imgui.h"
#include "json.h"

#include <cstdint>

namespace RocProfVis
{
namespace View
{

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
// comparison its own way. ComputeKernelDetailsView subscribes to the selection
// and metric events and forwards them through the On* calls, so a wrapped
// widget does not subscribe to them itself. Each concrete panel lives with the
// widget it wraps.
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
    // Metrics have arrived for the client `client_id`.
    virtual void OnMetricsFetched(uint64_t client_id);
    // Comparing has started; ApplyCompare follows.
    virtual void OnCompareStarted();
    // A (a kernel of `a_workload_id`) and B (of `b_workload_id`) trade places;
    // ApplyCompare follows.
    virtual void OnCompareSwapping(uint32_t a_workload_id, uint32_t b_workload_id);
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
    // Header button ids; a derived panel numbers its own from FIRST_OWN_BUTTON.
    static constexpr int32_t SHOW_A_BUTTON    = 0;
    static constexpr int32_t SHOW_B_BUTTON    = 1;
    static constexpr int32_t FIRST_OWN_BUTTON = 2;

    virtual const char* ShowATooltip() const;
    virtual const char* ShowBTooltip() const;
    // m_side while comparing, else A.
    CompareSide ShownSide() const;
    void        ShowSide(CompareSide side);

    CompareSide m_side;
};

}  // namespace View
}  // namespace RocProfVis
