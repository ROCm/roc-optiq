// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_compute_kernel_details_panes.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_presets.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_compute_widget.h"
#include "widgets/rocprofvis_split_containers.h"
#include "widgets/rocprofvis_widget.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

class DataProvider;
class ComputeSelection;
class KernelMetricTable;
struct KernelInfo;

// Most boxes a layout template has.
constexpr int32_t COMPUTE_LAYOUT_MAX_SLOTS = 4;

// Workspace panels of Kernel Details, in default placement order. Stored as ints
// in saved presets: append only.
enum class ComputePane : int32_t
{
    kMemoryChart,
    kRoofline,
    kMetricTables,
    kIsa,
    kWorkloadDetails,
    kPinnedMetrics,
    kCount
};

// Box arrangements of the workspace. Stored as an int in saved presets: append
// only.
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

// Per-kernel workspace. A kernel rail on the left lists every workload's
// kernels as vertical tabs (or the full kernel metric table) and is where A and
// B are chosen; the rest is a box layout
// picked from ComputeLayoutTemplate, each box showing one panel. Panels move by
// dragging a box's title onto another box (the two swap) or by the box's panel
// picker; an empty box offers the panels not shown; any box can be maximized to
// fill the tab. Template, box contents and maximize belong to this view, so
// each open trace has its own; user presets save and restore them.
//
// Each panel is a ComputeKernelDetailsPane; this view handles the boxes, the
// kernel rail and the comparison, and reaches the panels only through that
// interface. Compare sets a second kernel (B, from any workload) against the
// selection (A): the roofline plots both, metric tables add B and difference
// columns, and the memory chart, ISA and workload details switch between A, B
// and (memory chart, same GPU architecture only) B - A.
class ComputeKernelDetailsView : public RocWidget, private IKernelCompare
{
public:
    ComputeKernelDetailsView(DataProvider&                     data_provider,
                             std::shared_ptr<ComputeSelection> compute_selection,
                             bool has_available_metrics, bool has_isa_lines);
    ~ComputeKernelDetailsView();

    void Render() override;
    void Update() override;

    // The Layout combo, for ComputeView's toolbar.
    void RenderLayoutControls();
    // Whether the kernel rail is shown, for the View menu.
    bool& KernelListVisible() { return m_layout.show_kernel_list; }

    friend struct ComputeKernelDetailsViewTestPeer;

private:
    // The arrangement the user chose. Template and pane values are stored as
    // ComputeLayoutTemplate / ComputePane ints, as presets save them, and are
    // checked when read.
    struct LayoutSettings
    {
        int32_t layout_template = static_cast<int32_t>(ComputeLayoutTemplate::kMainBelow);
        // Pane per box; -1 = empty.
        int32_t slots[COMPUTE_LAYOUT_MAX_SLOTS] = { 0, 1, 2, 3 };
        bool    maximized         = false;  // maximized_pane fills the tab.
        int32_t maximized_pane    = 0;
        bool    show_kernel_list  = true;
        bool    kernel_list_table = false;  // The rail shows the full metric table.
    };

    // Everything the split tree depends on; it is rebuilt when this changes.
    struct LayoutState
    {
        ComputeLayoutTemplate layout      = ComputeLayoutTemplate::kMainBelow;
        bool                  maximized   = false;
        bool                  kernel_rail = false;
    };

    // A node of the current template's split tree: a box, or a split of two
    // nodes (indices into m_layout_nodes).
    struct LayoutNode
    {
        int32_t                          slot;  // The box, for a leaf; else -1.
        std::shared_ptr<HSplitContainer> hsplit;
        std::shared_ptr<VSplitContainer> vsplit;
        int32_t                          first;
        int32_t                          second;
        LayoutItem::Ptr                  item;
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

    // Saves the layout (template, boxes, maximize, kernel rail, the panels' own
    // settings) with user presets; Reset() restores the default arrangement.
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
    void CreatePanes(DataProvider& data_provider, bool has_available_metrics,
                     bool has_isa_lines);
    void CreateLayoutItems();

    // Panels and boxes.
    ComputeKernelDetailsPane& Pane(ComputePane pane) const;
    bool           IsPaneAvailable(ComputePane pane) const;
    // The available panel a stored pane value names, or kCount.
    ComputePane    AvailablePane(int32_t value) const;
    ComputeLayoutTemplate CurrentTemplate() const;
    int32_t        ActiveSlotCount() const;
    ComputePane    SlotPane(int32_t slot) const;
    int32_t        ActiveSlotOf(ComputePane pane) const;
    ComputePane    MaximizedPane() const;
    void           AssignSlot(int32_t slot, ComputePane pane);
    void           SwapSlots(int32_t a, int32_t b);
    void           ClearSlot(int32_t slot);
    void           SetTemplate(ComputeLayoutTemplate layout);
    void           ResetLayout();

    // Split tree.
    LayoutState CurrentLayoutState() const;
    void        RebuildLayout(const LayoutState& state);
    // Builds `layout`'s tree into m_layout_nodes; returns the root node.
    int32_t     BuildTemplate(ComputeLayoutTemplate layout);
    int32_t     BoxNode(int32_t slot);
    int32_t     HSplitNode(int32_t left, int32_t right, float ratio);
    int32_t     VSplitNode(int32_t top, int32_t bottom, float ratio);
    void        UpdatePaneMinSizes();
    void        UpdatePanesShown();
    // Gives the splits under `node` their sides' minimum sizes, with the panels'
    // minimum widths scaled by `width_scale`; returns the node's own minimum size.
    ImVec2      ApplyNodeMinSizes(int32_t node, float width_scale);
    ImVec2      SlotMinSize(int32_t slot) const;
    void        SetKernelRailTableMode(bool table);

    // Drawing.
    void RenderLayoutMenu();
    // Title (a drag handle for box `slot` when `draggable`, followed by the
    // box's panel picker when `pickable`), then buttons right-aligned. Returns
    // the clicked button's index or -1; title_double_clicked reports a
    // double-click on the title, and row_width (when set) the width the row
    // needs to fit without clipping.
    int32_t RenderPaneHeader(const char* title, int32_t slot, bool draggable,
                             bool pickable,
                             const ComputeKernelDetailsPane::HeaderButton* buttons,
                             int32_t count, bool& title_double_clicked,
                             float* row_width = nullptr);
    void RenderPanelPicker(int32_t slot);
    void RenderSlot(int32_t slot);
    void RenderPane(ComputePane pane, int32_t slot);
    void RenderEmptySlot(int32_t slot);
    // Makes the last item, box `slot`'s card, the drop target for box titles.
    void HandleSlotDrop(int32_t slot);
    void RenderKernelRail();
    void RenderKernelList();
    // `can_compare` is CanCompare().
    void RenderKernelTab(const KernelInfo& kernel, uint32_t workload_id, bool selected,
                         bool can_compare, float total_duration);
    void RefreshKernelList();

    // Comparison.
    bool     IsComparing() const override;
    uint32_t CompareWorkloadId() const override;
    uint32_t CompareKernelId() const override;
    bool     DeltaAvailable() const override;
    bool CanCompare() const;
    void EnableCompare();
    void DisableCompare();
    void SetCompareTarget(uint32_t workload_id, uint32_t kernel_id);
    // B becomes the selection and the old selection becomes B.
    void SwapCompare();
    // Applies the compare rules to a selection change (see CompareState).
    void OnBaselineChanged();
    // Pushes the comparison to the kernel rail and every panel; `target_changed`
    // refetches B.
    void ApplyCompare(bool target_changed);
    const KernelInfo* FindKernelByName(uint32_t workload_id, const std::string& name) const;
    std::vector<const KernelInfo*> KernelsByDuration(uint32_t workload_id) const;
    // A and B, Follow A, Swap, the architectures and Stop, above the kernel list.
    void RenderCompareCard();

    DataProvider& m_data_provider;

    std::shared_ptr<ComputeSelection>  m_compute_selection;
    std::shared_ptr<KernelMetricTable> m_kernel_metric_table;
    // Indexed by ComputePane; every panel exists, available or not.
    std::array<std::unique_ptr<ComputeKernelDetailsPane>,
               static_cast<size_t>(ComputePane::kCount)>
        m_panes;

    uint64_t m_client_id;

    LayoutSettings m_layout;
    // Card widths that fit the kernel rail's and each box's header row, as last
    // drawn; the rail and boxes are kept at least this wide.
    float                                       m_rail_header_width = 0.0f;
    std::array<float, COMPUTE_LAYOUT_MAX_SLOTS> m_slot_header_widths{};

    // One pane per box, the maximized pane, and the kernel rail.
    std::array<LayoutItem::Ptr, COMPUTE_LAYOUT_MAX_SLOTS> m_slot_panes;
    LayoutItem::Ptr                                      m_maximized_pane;
    LayoutItem::Ptr                                      m_rail_pane;

    std::shared_ptr<HSplitContainer> m_root_split;  // rail | workspace
    std::vector<LayoutNode>          m_layout_nodes;  // Current template's tree.
    int32_t                          m_layout_root;  // Its root node, or -1.
    LayoutState                      m_built_layout;
    bool                             m_layout_built = false;

    // Kernel rail, the one place kernels (A and B) are chosen: every workload's
    // kernels by total duration, and the list's search text.
    struct KernelGroup
    {
        const WorkloadInfo*            workload;
        std::vector<const KernelInfo*> kernels;
        float                          total_duration;
    };
    std::vector<KernelGroup> m_kernel_groups;
    // The list keeps its own order whatever the kernel table's sort and filters:
    // the groups' kernels, workload by workload.
    struct ListedKernel
    {
        const WorkloadInfo* workload;
        const KernelInfo*   kernel;
        float               workload_duration;
    };
    std::vector<ListedKernel> ListedKernels() const;
    // Rebuilds m_shown_kernels when the groups or the search text changed.
    void                      RefreshShownKernels(bool filter_changed);
    std::string               m_kernel_filter;
    // ListedKernels() after the search, as the list draws it.
    std::vector<ListedKernel> m_shown_kernels;
    bool                      m_shown_kernels_dirty = true;
    // Whether anything was listed before the search narrowed it.
    bool                      m_any_kernels_listed  = false;
    // Last selection the list showed; a change from elsewhere scrolls it into view.
    uint32_t                 m_kernel_list_selection;
    uint32_t                 m_kernel_list_selection_workload;

    CompareState m_compare;

    std::unique_ptr<Preset> m_preset;

    EventManager::SubscriptionToken m_workload_selection_changed_token;
    EventManager::SubscriptionToken m_kernel_selection_changed_token;
    EventManager::SubscriptionToken m_metrics_fetched_token;
    EventManager::SubscriptionToken m_new_table_data_token;
    EventManager::SubscriptionToken m_send_metric_to_kernel_details_token;
};

}  // namespace View
}  // namespace RocProfVis
