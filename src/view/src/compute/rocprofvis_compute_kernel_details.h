// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_compute_memory_chart.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_presets.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_compute_widget.h"
#include "widgets/rocprofvis_split_containers.h"
#include "widgets/rocprofvis_widget.h"

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace RocProfVis
{
namespace View
{

class DataProvider;
class ComputeSelection;
class ComputeIsaView;
class ComputeTableView;
class Roofline;
class KernelMetricTable;
struct KernelInfo;

// Workspace panels of Kernel Details, in default placement order. Stored as ints
// in AppWindowSettings::compute_layout_slots and in saved presets: append only.
enum class ComputePane : int32_t
{
    kMemoryChart,
    kRoofline,
    kMetricTables,
    kIsa,
    kCount
};

// Box arrangements of the workspace. Stored as an int in
// AppWindowSettings::compute_layout_template and in saved presets: append only
// (kMainBelow is 0, the default).
enum class ComputeLayoutTemplate : int32_t
{
    kMainBelow,       // One large box over two side by side.
    kSingle,
    kSideBySide,
    kStacked,
    kMainRight,       // One large box left of two stacked.
    kThreeColumns,
    kMainThreeBelow,  // One large box over three side by side.
    kGrid,            // 2 x 2.
    kCount
};

// What a panel shows while comparing: the selected kernel (A), the compare
// target (B), or B - A.
enum class CompareSide : uint8_t
{
    kA,
    kB,
    kDelta,
};

// Per-kernel workspace. A kernel rail on the left lists every workload's
// kernels as vertical tabs (or the full kernel metric table) and is where A and
// B are chosen; the rest is a box layout
// picked from ComputeLayoutTemplate, each box showing one panel. Panels move by
// dragging a box's title onto another box (the two swap) or by the box's panel
// picker; any box can be maximized to fill the tab. Template, box contents and
// maximize live in AppWindowSettings (so they persist and match View > Compute
// Profiler Panels) and are saved with user presets.
//
// Compare sets a second kernel (B, from any workload) against the selection
// (A): the roofline plots both, metric tables add B and difference columns, and
// the memory chart and ISA switch between A, B and (memory chart, same GPU
// architecture only) B - A.
class ComputeKernelDetailsView : public RocWidget
{
public:
    static constexpr const char* TAB_ID = "compute_kernel_details_view";

    static TabItem CreateTabItem(DataProvider&                            data_provider,
                                 const std::shared_ptr<ComputeSelection>& compute_selection,
                                 bool has_available_metrics, bool has_isa_lines);

    ComputeKernelDetailsView(DataProvider&                     data_provider,
                             std::shared_ptr<ComputeSelection> compute_selection,
                             bool has_available_metrics, bool has_isa_lines);
    ~ComputeKernelDetailsView();

    void Render() override;
    void Update() override;

    friend struct ComputeKernelDetailsViewTestPeer;

private:
    // Everything the split tree depends on; it is rebuilt when this changes.
    struct LayoutState
    {
        ComputeLayoutTemplate layout      = ComputeLayoutTemplate::kMainBelow;
        bool                  maximized   = false;
        bool                  kernel_rail = false;
    };

    // One header button; either an icon-font glyph or a text label.
    struct HeaderButton
    {
        const char* icon;
        const char* label;
        const char* tooltip;
        bool        active;
    };

    // The compare target (B) and how it follows the selection (A).
    struct CompareState
    {
        bool     enabled = false;
        uint32_t workload_id;
        uint32_t kernel_id;
        // When A changes to another kernel, B moves to the kernel of the same
        // name in B's workload (when B's workload is not A's).
        bool     follow_by_name = true;
        // A as last seen, so a repeated notification is not a change.
        uint32_t baseline_workload_id;
        uint32_t baseline_kernel_id;
    };

    // Saves the layout (template, boxes, maximize, kernel rail, chart fit) with
    // user presets; Reset() restores the default arrangement.
    class Preset : public PresetComponent
    {
    public:
        Preset(ComputeKernelDetailsView& widget);

        bool ToJson(jt::Json& json) override;
        bool FromJson(jt::Json& json) override;
        void Reset() override;

    private:
        ComputeKernelDetailsView& m_widget;
    };

    void SubscribeToEvents();
    void CreatePanes();

    // Panels and boxes.
    bool           IsPaneAvailable(ComputePane pane) const;
    bool&          PaneVisibleSetting(ComputePane pane) const;
    const char*    PaneTitle(ComputePane pane) const;
    ComputeLayoutTemplate CurrentTemplate() const;
    int32_t        ActiveSlotCount() const;
    ComputePane    SlotPane(int32_t slot) const;
    int32_t        ActiveSlotOf(ComputePane pane) const;
    ComputePane    MaximizedPane() const;
    void           AssignSlot(int32_t slot, ComputePane pane);
    void           SwapSlots(int32_t a, int32_t b);
    void           ClearSlot(int32_t slot);
    void           PlacePane(ComputePane pane);
    void           SetTemplate(ComputeLayoutTemplate layout);
    void           ResetLayout();
    // Applies Panels / View-menu toggles to the boxes, then mirrors the boxes
    // back into the toggles.
    void           SyncPanelToggles();

    // Split tree.
    LayoutState     CurrentLayoutState() const;
    void            RebuildLayout(const LayoutState& state);
    LayoutItem::Ptr BuildTemplate(ComputeLayoutTemplate layout);
    LayoutItem::Ptr HSplitPane(const LayoutItem::Ptr& left, const LayoutItem::Ptr& right,
                               float ratio);
    LayoutItem::Ptr VSplitPane(const LayoutItem::Ptr& top, const LayoutItem::Ptr& bottom,
                               float ratio);
    void            UpdatePaneMinSizes();
    void            SetKernelRailTableMode(bool table);

    // Drawing.
    void RenderToolbar();
    void RenderLayoutMenu();
    void RenderPanelsMenu();
    // Title (a drag handle for box `slot` when `draggable`, followed by the
    // box's panel picker when `pickable`), then buttons right-aligned. Returns
    // the clicked button index or -1; title_double_clicked reports a
    // double-click on the title.
    int  RenderPaneHeader(const char* title, int32_t slot, bool draggable, bool pickable,
                          const HeaderButton* buttons, int count,
                          bool& title_double_clicked);
    void RenderPanelPicker(int32_t slot);
    void RenderSlot(int32_t slot);
    void RenderPane(ComputePane pane, int32_t slot);
    void RenderPaneBody(ComputePane pane, int32_t slot);
    void RenderEmptySlot(int32_t slot);
    void HandleSlotDrop(int32_t slot, const ImVec2& slot_min, const ImVec2& slot_max);
    void RenderKernelRail();
    void RenderKernelList();
    // The kernel table's sort and filters, when they reorder or narrow the list.
    void RenderKernelListOrder();
    // `workload_label` names the workload on the tab when headers cannot.
    void RenderKernelTab(const KernelInfo& kernel, uint32_t workload_id, bool selected,
                         float total_duration, const char* workload_label);
    void RefreshKernelList();

    // Comparison.
    bool CanCompare() const;
    void EnableCompare();
    void DisableCompare();
    void SetCompareTarget(uint32_t workload_id, uint32_t kernel_id);
    // B becomes the selection and the old selection becomes B.
    void SwapCompare();
    // Applies the compare rules to a selection change (see CompareState).
    void OnBaselineChanged();
    // Pushes the comparison to the panels; `target_changed` refetches B.
    void ApplyCompare(bool target_changed);
    bool DeltaAvailable() const;
    const KernelInfo* FindKernelByName(uint32_t workload_id, const std::string& name) const;
    std::vector<const KernelInfo*> KernelsByDuration(uint32_t workload_id) const;
    ComputeMemoryChartView&        DisplayedMemoryChart();
    // A and B, Follow A, Swap, the architectures and Stop, above the kernel list.
    void RenderCompareCard();

    DataProvider&          m_data_provider;
    ComputeMemoryChartView m_memory_chart;
    // The compare target's chart; its own layout, so B of another GPU
    // architecture draws its own hierarchy.
    ComputeMemoryChartView m_memory_chart_b;

    std::shared_ptr<ComputeSelection>  m_compute_selection;
    std::shared_ptr<Roofline>          m_roofline;
    std::shared_ptr<KernelMetricTable> m_kernel_metric_table;
    std::shared_ptr<ComputeTableView>  m_table_view;
    std::shared_ptr<ComputeIsaView>    m_isa_view;  // Null when the database has no ISA lines.

    uint64_t m_client_id;

    // One pane per box, the maximized pane, and the kernel rail.
    std::array<LayoutItem::Ptr, COMPUTE_LAYOUT_MAX_SLOTS> m_slot_panes;
    LayoutItem::Ptr                                      m_maximized_pane;
    LayoutItem::Ptr                                      m_rail_pane;

    std::shared_ptr<HSplitContainer>              m_root_split;  // rail | workspace
    std::vector<std::shared_ptr<HSplitContainer>> m_hsplits;     // Current template's splits.
    std::vector<std::shared_ptr<VSplitContainer>> m_vsplits;
    LayoutState                                   m_built_layout;
    bool                                          m_layout_built = false;

    // Panel toggles as last mirrored from the boxes; a difference is a user toggle.
    std::array<bool, static_cast<size_t>(ComputePane::kCount)> m_synced_toggles{};
    bool                                                      m_toggles_synced = false;

    // Kernel rail, the one place kernels (A and B) are chosen: every workload's
    // kernels by total duration, and the list's search text.
    struct KernelGroup
    {
        const WorkloadInfo*            workload;
        std::vector<const KernelInfo*> kernels;
        float                          total_duration;
    };
    std::vector<KernelGroup> m_kernel_groups;
    // The list shows the kernel table's rows, in its order and after its
    // filters, so both modes list the same; until the table has rows, the groups.
    struct ListedKernel
    {
        const WorkloadInfo* workload;
        const KernelInfo*   kernel;
        float               workload_duration;
    };
    std::vector<ListedKernel> ListedKernels() const;
    std::string              m_kernel_filter;
    // Last selection the list showed; a change from elsewhere scrolls it into view.
    uint32_t                 m_kernel_list_selection;
    uint32_t                 m_kernel_list_selection_workload;

    CompareState m_compare;
    CompareSide  m_memory_chart_side = CompareSide::kA;
    CompareSide  m_isa_side          = CompareSide::kA;
    // Workload whose layout m_memory_chart_b has loaded.
    uint32_t     m_memory_chart_b_workload;

    std::unique_ptr<Preset> m_preset;

    EventManager::SubscriptionToken m_workload_selection_changed_token;
    EventManager::SubscriptionToken m_kernel_selection_changed_token;
    EventManager::SubscriptionToken m_metrics_fetched_token;
    EventManager::SubscriptionToken m_new_table_data_token;
    EventManager::SubscriptionToken m_send_metric_to_kernel_details_token;
};

}  // namespace View
}  // namespace RocProfVis
