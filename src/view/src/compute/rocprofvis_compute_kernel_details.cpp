// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_kernel_details.h"
#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_compute_kernel_metric_table.h"
#include "rocprofvis_compute_selection.h"
#include "rocprofvis_compute_table_view.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "imgui.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <functional>
#include <utility>

namespace
{
// Thickness of every draggable gutter between panes.
constexpr float PANE_GUTTER = 3.0f;
// Scrollbar thickness inside the workspace; slimmer than the app default so
// the many small panes keep their room for content.
constexpr float WORKSPACE_SCROLLBAR_SIZE = 8.0f;

// Share of the large box in the "main" templates, and the kernel rail's share.
constexpr float MAIN_SPLIT_RATIO = 0.62f;
constexpr float HALF             = 0.5f;
constexpr float THIRD            = 1.0f / 3.0f;
constexpr float TWO_THIRDS       = 2.0f / 3.0f;
constexpr float RAIL_LIST_RATIO  = 0.17f;  // kernel tabs | workspace
constexpr float RAIL_TABLE_RATIO = 0.45f;  // kernel table | workspace

// Smallest rail and workspace widths, in font sizes so they scale with the UI
// font; the panels set their own (ComputeKernelDetailsPane::MinSize).
constexpr float WORKSPACE_MIN_WIDTH_EMS  = 20.0f;
// Most of the workspace's width the panels' minimum widths may claim, so a
// gutter always has room to move.
constexpr float PANE_MIN_MAX_WORKSPACE_SHARE = 0.85f;
constexpr float RAIL_LIST_MIN_WIDTH_EMS  = 11.0f;
constexpr float RAIL_TABLE_MIN_WIDTH_EMS = 24.0f;

// Toolbar combos, in frame heights, and the Layout menu's row / thumbnail sizes.
constexpr float LAYOUT_COMBO_WIDTH_FRAMES = 11.0f;
constexpr float LAYOUT_ROW_HEIGHT_FRAMES  = 1.35f;
constexpr float THUMBNAIL_ASPECT          = 1.6f;
constexpr float THUMBNAIL_INSET           = 3.0f;
constexpr float THUMBNAIL_LABEL_GAP       = 8.0f;
constexpr float THUMBNAIL_GAP             = 1.0f;
constexpr float THUMBNAIL_ROUNDING        = 1.5f;
constexpr float THUMBNAIL_FILL_ALPHA      = 0.3f;
// An empty box's panel combo, in frame heights.
constexpr float EMPTY_BOX_COMBO_WIDTH_FRAMES = 9.0f;

// Box title handle and drop feedback.
constexpr const char* PANE_DRAG_PAYLOAD      = "COMPUTE_PANE_BOX";
constexpr float       TITLE_HANDLE_PAD_X     = 4.0f;
constexpr float       DROP_FILL_ALPHA        = 0.14f;
constexpr float       DROP_BORDER_THICKNESS  = 2.0f;
constexpr float       DROP_LABEL_PAD         = 6.0f;
constexpr int32_t     MAXIMIZED_SLOT         = -1;
constexpr int32_t     EMPTY_SLOT_VALUE       = -1;
constexpr int32_t     NO_LAYOUT_NODE         = -1;

// Kernel tab geometry.
constexpr float  KERNEL_TAB_PAD          = 6.0f;
constexpr float  KERNEL_TAB_LINE_GAP     = 2.0f;
constexpr float  KERNEL_TAB_BAR_HEIGHT   = 3.0f;
constexpr float  KERNEL_TAB_ACCENT_WIDTH = 3.0f;
constexpr float  KERNEL_TAB_ROUNDING     = 4.0f;
constexpr float  KERNEL_TAB_TRACK_ALPHA  = 0.6f;
constexpr float  KERNEL_TOOLTIP_WIDTH    = 480.0f;
constexpr size_t KERNEL_NAME_MAX_CHARS   = 512;
// Templated names run to thousands of characters; the tooltip shows this many.
constexpr size_t KERNEL_TOOLTIP_MAX_CHARS = 400;
// Anti-aliased edges reach this far past a tab's rect.
constexpr float  KERNEL_TAB_CULL_MARGIN  = 1.0f;
// The tab's stats line, which may lead with the workload's name.
constexpr size_t KERNEL_TAB_STATS_SIZE   = 256;

// A / B badges.
constexpr float COMPARE_BADGE_PAD_X        = 5.0f;
constexpr float COMPARE_BADGE_ROUNDING     = 3.0f;
constexpr float COMPARE_BADGE_HOVER_ALPHA  = 0.5f;
// Most of a kernel tab the A / B marker column may take on a narrow rail.
constexpr float KERNEL_TAB_SLOT_MAX_SHARE  = 0.5f;
// Header buttons every box's panel carries after its own: maximize (or
// restore) and hide.
constexpr int32_t COMMON_PANE_HEADER_BUTTONS = 2;

constexpr const char* JSON_KEY_LAYOUT_TEMPLATE          = "template";
constexpr const char* JSON_KEY_LAYOUT_SLOTS             = "slots";
constexpr const char* JSON_KEY_LAYOUT_MAXIMIZED         = "maximized";
constexpr const char* JSON_KEY_LAYOUT_MAXIMIZED_PANE    = "maximized_pane";
constexpr const char* JSON_KEY_LAYOUT_KERNEL_LIST       = "kernel_list";
constexpr const char* JSON_KEY_LAYOUT_KERNEL_LIST_TABLE = "kernel_list_table";
constexpr const char* JSON_KEY_COMPARE_FOLLOW_BY_NAME   = "compare_follow_by_name";
}  // namespace

namespace RocProfVis
{
namespace View
{

// Name, box count and box rectangles (x0, y0, x1, y1 in unit space, for the
// Layout menu thumbnails) of a template; indexed by ComputeLayoutTemplate.
struct LayoutTemplateInfo
{
    const char* name;
    int32_t     slots;
    float       rects[COMPUTE_LAYOUT_MAX_SLOTS][4];
};

static const LayoutTemplateInfo LAYOUT_TEMPLATES[] = {
    { "Main + Two Below", 3,
      { { 0, 0, 1, MAIN_SPLIT_RATIO },
        { 0, MAIN_SPLIT_RATIO, HALF, 1 },
        { HALF, MAIN_SPLIT_RATIO, 1, 1 } } },
    { "Single", 1, { { 0, 0, 1, 1 } } },
    { "Side by Side", 2, { { 0, 0, HALF, 1 }, { HALF, 0, 1, 1 } } },
    { "Stacked", 2, { { 0, 0, 1, HALF }, { 0, HALF, 1, 1 } } },
    { "Main + Two Right", 3,
      { { 0, 0, MAIN_SPLIT_RATIO, 1 },
        { MAIN_SPLIT_RATIO, 0, 1, HALF },
        { MAIN_SPLIT_RATIO, HALF, 1, 1 } } },
    { "Three Columns", 3,
      { { 0, 0, THIRD, 1 }, { THIRD, 0, TWO_THIRDS, 1 }, { TWO_THIRDS, 0, 1, 1 } } },
    { "Main + Three Below", 4,
      { { 0, 0, 1, MAIN_SPLIT_RATIO },
        { 0, MAIN_SPLIT_RATIO, THIRD, 1 },
        { THIRD, MAIN_SPLIT_RATIO, TWO_THIRDS, 1 },
        { TWO_THIRDS, MAIN_SPLIT_RATIO, 1, 1 } } },
    { "2 x 2 Grid", 4,
      { { 0, 0, HALF, HALF },
        { HALF, 0, 1, HALF },
        { 0, HALF, HALF, 1 },
        { HALF, HALF, 1, 1 } } },
};
static_assert(sizeof(LAYOUT_TEMPLATES) / sizeof(LAYOUT_TEMPLATES[0]) ==
                  static_cast<size_t>(ComputeLayoutTemplate::kCount),
              "one LAYOUT_TEMPLATES entry per ComputeLayoutTemplate");

// Layout menu order: by box count, simplest first.
static const ComputeLayoutTemplate LAYOUT_MENU_ORDER[] = {
    ComputeLayoutTemplate::kSingle,         ComputeLayoutTemplate::kSideBySide,
    ComputeLayoutTemplate::kStacked,        ComputeLayoutTemplate::kMainBelow,
    ComputeLayoutTemplate::kMainRight,      ComputeLayoutTemplate::kThreeColumns,
    ComputeLayoutTemplate::kMainThreeBelow, ComputeLayoutTemplate::kGrid,
};

constexpr int32_t PANE_COUNT = static_cast<int32_t>(ComputePane::kCount);
static_assert(COMPUTE_LAYOUT_MAX_SLOTS <= PANE_COUNT,
              "ResetLayout gives each box a distinct panel");
constexpr int32_t MAX_PANE_HEADER_BUTTONS =
    ComputeKernelDetailsPane::MAX_HEADER_BUTTONS + COMMON_PANE_HEADER_BUTTONS;

static const LayoutTemplateInfo&
TemplateInfo(ComputeLayoutTemplate layout)
{
    return LAYOUT_TEMPLATES[static_cast<size_t>(layout)];
}

static ComputePane
PaneAt(int32_t index)
{
    return static_cast<ComputePane>(index);
}

static bool
IsSlotIndex(int32_t slot)
{
    return slot >= 0 && slot < COMPUTE_LAYOUT_MAX_SLOTS;
}

// The kernel rail's share of the tab; the full table needs far more width than
// the tab list.
static float
RailSplitRatio(bool table)
{
    return table ? RAIL_TABLE_RATIO : RAIL_LIST_RATIO;
}

// Draws a template's boxes into [min, min + size].
static void
DrawLayoutThumbnail(ImDrawList* draw_list, ImVec2 min, ImVec2 size,
                    ComputeLayoutTemplate layout, ImU32 border)
{
    const LayoutTemplateInfo& info = TemplateInfo(layout);
    const ImU32               fill = ApplyAlpha(border, THUMBNAIL_FILL_ALPHA);
    for(int32_t i = 0; i < info.slots; ++i)
    {
        const float* r = info.rects[i];
        const ImVec2 a(min.x + r[0] * size.x + THUMBNAIL_GAP,
                       min.y + r[1] * size.y + THUMBNAIL_GAP);
        const ImVec2 b(min.x + r[2] * size.x - THUMBNAIL_GAP,
                       min.y + r[3] * size.y - THUMBNAIL_GAP);
        draw_list->AddRectFilled(a, b, fill, THUMBNAIL_ROUNDING);
        draw_list->AddRect(a, b, border, THUMBNAIL_ROUNDING);
    }
}

// A pane drawn by `render`. The pane carries no item spacing: a vertical split
// advances past each pane with the pane's spacing, which would widen that
// gutter. The content still gets the normal spacing, and the default window
// padding for the card it paints.
static LayoutItem::Ptr
MakeRenderPane(const std::function<void()>& render)
{
    std::shared_ptr<RocCustomWidget> wrapper =
        std::make_shared<RocCustomWidget>([render]() {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                                SettingsManager::GetInstance().GetDefaultStyle().ItemSpacing);
            render();
            ImGui::PopStyleVar();
        });
    LayoutItem::Ptr pane   = LayoutItem::CreateFromWidget(wrapper);
    pane->m_child_flags    = ImGuiChildFlags_None;
    pane->m_window_padding = SettingsManager::GetInstance().GetDefaultStyle().WindowPadding;
    return pane;
}

// A pane holding a nested split; only the split's own panes scroll.
static LayoutItem::Ptr
MakeSplitPane(const std::shared_ptr<RocWidget>& split)
{
    LayoutItem::Ptr pane = LayoutItem::CreateFromWidget(split);
    pane->m_child_flags  = ImGuiChildFlags_None;
    pane->m_window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    return pane;
}

// The card every pane paints; its body scrolls in its own child.
static void
BeginPaneCard(const char* id)
{
    SettingsManager& settings = SettingsManager::GetInstance();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgPanel));
    ImGui::PushStyleColor(ImGuiCol_Border, settings.GetColor(Colors::kBorderColor));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, settings.GetDefaultStyle().ChildRounding);
    ImGui::BeginChild(id, ImVec2(0, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

static void
EndPaneCard()
{
    ImGui::EndChild();
}

// The width a pane card needs for a row `row_width` wide inside its padding and
// border.
static float
PaneCardWidth(float row_width)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    return row_width + (style.WindowPadding.x + style.ChildBorderSize) * 2.0f;
}

static ImVec2
BadgeSize(const char* text)
{
    const ImVec2 text_size = ImGui::CalcTextSize(text);
    return ImVec2(text_size.x + COMPARE_BADGE_PAD_X * 2.0f, text_size.y);
}

// "A" / "B" pill in the comparison colors, top-left at `min`.
static void
DrawBadge(ImDrawList* draw_list, ImVec2 min, const char* text, Colors color)
{
    SettingsManager& settings = SettingsManager::GetInstance();
    const ImVec2     size     = BadgeSize(text);
    draw_list->AddRectFilled(min, ImVec2(min.x + size.x, min.y + size.y),
                             settings.GetColor(color), COMPARE_BADGE_ROUNDING);
    draw_list->AddText(ImVec2(min.x + COMPARE_BADGE_PAD_X, min.y),
                       settings.GetColor(Colors::kTextMain), text);
}

// A badge as an item on the current line, centred on a frame's height.
static void
Badge(const char* text, Colors color, const char* tooltip)
{
    const ImVec2 size    = BadgeSize(text);
    const float  frame_h = ImGui::GetFrameHeight();
    const ImVec2 pos     = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(size.x, frame_h));
    DrawBadge(ImGui::GetWindowDrawList(), ImVec2(pos.x, pos.y + (frame_h - size.y) * 0.5f),
              text, color);
    if(tooltip && ImGui::IsItemHovered())
    {
        SetTooltipStyled("%s", tooltip);
    }
}

static ImFont*
IconFont()
{
    return SettingsManager::GetInstance().GetFontManager().GetFont(FontType::kIcon);
}

static float
IconWidth(const char* icon)
{
    ImGui::PushFont(IconFont(), 0.0f);
    const float width = ImGui::CalcTextSize(icon).x;
    ImGui::PopFont();
    return width;
}

// An icon button with frame padding, on `bg`, in the app's hover / press colors.
// It is padded and sized like a text button beside it; the icon font's line
// height differs.
static bool
FramedIconButton(const char* icon, const char* tooltip, ImU32 bg)
{
    SettingsManager& settings = SettingsManager::GetInstance();
    return IconButton(icon, IconFont(), ImVec2(0.0f, ImGui::GetFrameHeight()), tooltip, false,
                      ImGui::GetStyle().FramePadding, bg,
                      settings.GetColor(Colors::kButtonHovered),
                      settings.GetColor(Colors::kButtonActive));
}

// Continues the line so that `width` worth of items ends at the right edge.
static void
SameLineRightAligned(float width)
{
    ImGui::SameLine();
    const float x = ImGui::GetCursorPosX();
    ImGui::SetCursorPosX(std::max(x, x + ImGui::GetContentRegionAvail().x - width));
}

// Whether the mouse is over the trailing ellipsis of `shown`, a name elided
// from `full` and drawn from `pos` on a line `height` tall.
static bool
IsEllipsisHovered(const std::string& shown, const std::string& full, ImVec2 pos,
                  float height)
{
    if(shown == full)
    {
        return false;
    }
    const float  end_x = pos.x + ImGui::CalcTextSize(shown.c_str()).x;
    const ImVec2 min(end_x - ImGui::CalcTextSize(TEXT_ELLIPSIS).x, pos.y);
    return ImGui::IsMouseHoveringRect(min, ImVec2(end_x, pos.y + height));
}

// A tooltip with a kernel's name, wrapped and cut short so it stays a readable
// size.
static void
KernelNameTooltip(const std::string& name)
{
    BeginTooltipStyled();
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + KERNEL_TOOLTIP_WIDTH);
    if(name.size() > KERNEL_TOOLTIP_MAX_CHARS)
    {
        const std::string capped = name.substr(0, KERNEL_TOOLTIP_MAX_CHARS) + TEXT_ELLIPSIS;
        ImGui::TextUnformatted(capped.c_str());
    }
    else
    {
        ImGui::TextUnformatted(name.c_str());
    }
    ImGui::PopTextWrapPos();
    EndTooltipStyled();
}

ComputeKernelDetailsView::ComputeKernelDetailsView(
    DataProvider& data_provider, std::shared_ptr<ComputeSelection> compute_selection,
    bool has_available_metrics, bool has_isa_lines)
: RocWidget()
, m_data_provider(data_provider)
, m_compute_selection(compute_selection)
, m_kernel_metric_table(nullptr)
, m_client_id(IdGenerator::GetInstance().GenerateId())
, m_layout_root(NO_LAYOUT_NODE)
, m_kernel_list_selection(ComputeSelection::INVALID_SELECTION_ID)
, m_kernel_list_selection_workload(ComputeSelection::INVALID_SELECTION_ID)
, m_preset(nullptr)
, m_workload_selection_changed_token(EventManager::InvalidSubscriptionToken)
, m_kernel_selection_changed_token(EventManager::InvalidSubscriptionToken)
, m_metrics_fetched_token(EventManager::InvalidSubscriptionToken)
, m_new_table_data_token(EventManager::InvalidSubscriptionToken)
, m_send_metric_to_kernel_details_token(EventManager::InvalidSubscriptionToken)
{
    m_compare.workload_id          = ComputeSelection::INVALID_SELECTION_ID;
    m_compare.kernel_id            = ComputeSelection::INVALID_SELECTION_ID;
    m_compare.baseline_workload_id = ComputeSelection::INVALID_SELECTION_ID;
    m_compare.baseline_kernel_id   = ComputeSelection::INVALID_SELECTION_ID;

    m_kernel_metric_table =
        std::make_shared<RocProfVis::View::KernelMetricTable>(data_provider,
                                                              compute_selection);
    m_kernel_metric_table->SetFillParent(true);
    m_kernel_metric_table->SetChromeless(true);
    m_kernel_metric_table->SetCompareCallback(
        [this](uint32_t workload_id, uint32_t kernel_id) {
            SetCompareTarget(workload_id, kernel_id);
        });
    CreatePanes(data_provider, has_available_metrics, has_isa_lines);
    SubscribeToEvents();

    CreateLayoutItems();
    m_preset      = std::make_unique<Preset>(*this);
    m_widget_name = GenUniqueName("ComputeKernelDetailsView");
}

ComputeKernelDetailsView::~ComputeKernelDetailsView()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeWorkloadSelectionChanged),
        m_workload_selection_changed_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeKernelSelectionChanged),
        m_kernel_selection_changed_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeMetricsFetched), m_metrics_fetched_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kNewTableData), m_new_table_data_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeShowMetricInKernelDetails),
        m_send_metric_to_kernel_details_token);
}

void
ComputeKernelDetailsView::CreatePanes(DataProvider& data_provider,
                                      bool has_available_metrics, bool has_isa_lines)
{
    const IKernelCompare&                    compare   = *this;
    const std::shared_ptr<ComputeSelection>& selection = m_compute_selection;
    m_panes[static_cast<size_t>(ComputePane::kMemoryChart)] =
        std::make_unique<ComputeMemoryChartPane>(compare, data_provider, selection);
    m_panes[static_cast<size_t>(ComputePane::kRoofline)] =
        std::make_unique<ComputeRooflinePane>(compare, data_provider);
    std::shared_ptr<ComputeTableView> table_view =
        std::make_shared<ComputeTableView>(data_provider, selection, has_available_metrics);
    m_panes[static_cast<size_t>(ComputePane::kMetricTables)] =
        std::make_unique<ComputeMetricTablesPane>(compare, table_view);
    m_panes[static_cast<size_t>(ComputePane::kIsa)] =
        std::make_unique<ComputeIsaPane>(compare, data_provider, has_isa_lines);
    m_panes[static_cast<size_t>(ComputePane::kWorkloadDetails)] =
        std::make_unique<ComputeWorkloadPane>(compare, data_provider, selection);
    m_panes[static_cast<size_t>(ComputePane::kPinnedMetrics)] =
        std::make_unique<ComputePinnedMetricsPane>(compare, table_view);
    static_assert(static_cast<int32_t>(ComputePane::kPinnedMetrics) + 1 == PANE_COUNT,
                  "CreatePanes creates every ComputePane");
}

void
ComputeKernelDetailsView::CreateLayoutItems()
{
    for(int32_t slot = 0; slot < COMPUTE_LAYOUT_MAX_SLOTS; ++slot)
    {
        m_slot_panes[static_cast<size_t>(slot)] =
            MakeRenderPane([this, slot]() { RenderSlot(slot); });
    }
    m_maximized_pane = MakeRenderPane([this]() {
        const ComputePane pane = MaximizedPane();
        if(pane != ComputePane::kCount)
        {
            RenderPane(pane, MAXIMIZED_SLOT);
        }
    });
    m_rail_pane = MakeRenderPane([this]() { RenderKernelRail(); });

    m_root_split = std::make_shared<HSplitContainer>(m_rail_pane, nullptr);
    m_root_split->SetSplit(RailSplitRatio(m_layout.kernel_list_table));
    m_root_split->SetSplitterSize(PANE_GUTTER);
}

void
ComputeKernelDetailsView::SubscribeToEvents()
{
    auto workload_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<ComputeSelectionChangedEvent>(e);
        if(evt && evt->GetSourceId() == m_data_provider.GetTraceFilePath())
        {
            for(const std::unique_ptr<ComputeKernelDetailsPane>& pane : m_panes)
            {
                pane->OnWorkloadSelected(evt->GetId());
            }
            if(m_kernel_metric_table)
            {
                m_kernel_metric_table->FetchData(evt->GetId());
            }
        }
    };

    m_workload_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeWorkloadSelectionChanged),
        workload_changed_handler);

    auto kernel_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<ComputeSelectionChangedEvent>(e);
        if(evt && evt->GetSourceId() == m_data_provider.GetTraceFilePath())
        {
            for(const std::unique_ptr<ComputeKernelDetailsPane>& pane : m_panes)
            {
                pane->OnKernelSelected(evt->GetId());
            }
            OnBaselineChanged();
        }
    };

    m_kernel_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeKernelSelectionChanged),
        kernel_changed_handler);

    auto metrics_fetched_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<ComputeMetricsFetchedEvent>(e);
        if(evt && evt->GetSourceId() == m_data_provider.GetTraceFilePath())
        {
            for(const std::unique_ptr<ComputeKernelDetailsPane>& pane : m_panes)
            {
                pane->OnMetricsFetched(evt->GetClientId());
            }
        }
    };
    m_metrics_fetched_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeMetricsFetched), metrics_fetched_handler);

    auto new_table_data_handler = [this](std::shared_ptr<RocEvent> e) {
        if(auto table_data_event = std::dynamic_pointer_cast<TableDataEvent>(e))
        {
            if(m_data_provider.GetTraceFilePath() != table_data_event->GetSourceId())
            {
                return;
            }

            if(table_data_event->GetRequestID() == DataProvider::METRIC_PIVOT_TABLE_REQUEST_ID)
            {
                m_kernel_metric_table->HandleNewData(
                    table_data_event->GetResponseCode() == kRocProfVisResultSuccess,
                    table_data_event->GetErrorMessage());
            }
        }
    };

    m_new_table_data_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kNewTableData), new_table_data_handler);

    auto metric_navigation_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<ComputeAddMetricToKernelDetailsEvent>(e);
        if(!evt || evt->GetSourceId() != m_data_provider.GetTraceFilePath())
        {
            return;
        }

        if(m_kernel_metric_table)
        {
            m_kernel_metric_table->SetExternalQuery(
                MetricId{ evt->GetCategoryId(), evt->GetTableId(), evt->GetEntryId() },
                evt->GetValueName());
        }
    };

    m_send_metric_to_kernel_details_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeShowMetricInKernelDetails),
        metric_navigation_handler);
}

void
ComputeKernelDetailsView::Update()
{
    if(m_kernel_metric_table)
    {
        m_kernel_metric_table->Update();
    }
    for(const std::unique_ptr<ComputeKernelDetailsPane>& pane : m_panes)
    {
        pane->Update();
    }
}

ComputeKernelDetailsPane&
ComputeKernelDetailsView::Pane(ComputePane pane) const
{
    IM_ASSERT(pane != ComputePane::kCount);
    return *m_panes[static_cast<size_t>(pane)];
}

bool
ComputeKernelDetailsView::IsPaneAvailable(ComputePane pane) const
{
    return pane != ComputePane::kCount && Pane(pane).IsAvailable();
}

ComputePane
ComputeKernelDetailsView::AvailablePane(int32_t value) const
{
    const ComputePane pane = value >= 0 && value < PANE_COUNT ? PaneAt(value)
                                                              : ComputePane::kCount;
    return IsPaneAvailable(pane) ? pane : ComputePane::kCount;
}

ComputeLayoutTemplate
ComputeKernelDetailsView::CurrentTemplate() const
{
    const int32_t value = m_layout.layout_template;
    return value >= 0 && value < static_cast<int32_t>(ComputeLayoutTemplate::kCount)
               ? static_cast<ComputeLayoutTemplate>(value)
               : ComputeLayoutTemplate::kMainBelow;
}

int32_t
ComputeKernelDetailsView::ActiveSlotCount() const
{
    return TemplateInfo(CurrentTemplate()).slots;
}

ComputePane
ComputeKernelDetailsView::SlotPane(int32_t slot) const
{
    return IsSlotIndex(slot) ? AvailablePane(m_layout.slots[slot])
                             : ComputePane::kCount;
}

int32_t
ComputeKernelDetailsView::ActiveSlotOf(ComputePane pane) const
{
    const int32_t count = ActiveSlotCount();
    for(int32_t slot = 0; slot < count; ++slot)
    {
        if(SlotPane(slot) == pane)
        {
            return slot;
        }
    }
    return -1;
}

ComputePane
ComputeKernelDetailsView::MaximizedPane() const
{
    return m_layout.maximized ? AvailablePane(m_layout.maximized_pane) : ComputePane::kCount;
}

void
ComputeKernelDetailsView::AssignSlot(int32_t slot, ComputePane pane)
{
    if(!IsSlotIndex(slot))
    {
        return;
    }
    // A panel lives in one box at most: taking it from another box swaps the two.
    int32_t*      slots = m_layout.slots;
    const int32_t value = static_cast<int32_t>(pane);
    for(int32_t other = 0; other < COMPUTE_LAYOUT_MAX_SLOTS; ++other)
    {
        if(slots[other] == value)
        {
            std::swap(slots[other], slots[slot]);
            return;
        }
    }
    slots[slot] = value;
}

void
ComputeKernelDetailsView::SwapSlots(int32_t a, int32_t b)
{
    if(IsSlotIndex(a) && IsSlotIndex(b) && a != b)
    {
        std::swap(m_layout.slots[a], m_layout.slots[b]);
    }
}

void
ComputeKernelDetailsView::ClearSlot(int32_t slot)
{
    if(!IsSlotIndex(slot))
    {
        return;
    }
    if(m_layout.maximized && m_layout.maximized_pane == m_layout.slots[slot])
    {
        m_layout.maximized = false;
    }
    m_layout.slots[slot] = EMPTY_SLOT_VALUE;
}

void
ComputeKernelDetailsView::SetTemplate(ComputeLayoutTemplate layout)
{
    m_layout.layout_template = static_cast<int32_t>(layout);
    m_layout.maximized       = false;
    // Boxes keep their panels in order; empty boxes take panels not yet shown, so
    // a larger arrangement starts full.
    const int32_t count = ActiveSlotCount();
    for(int32_t slot = 0; slot < count; ++slot)
    {
        if(SlotPane(slot) != ComputePane::kCount)
        {
            continue;
        }
        for(int32_t i = 0; i < PANE_COUNT; ++i)
        {
            const ComputePane pane = PaneAt(i);
            if(IsPaneAvailable(pane) && ActiveSlotOf(pane) < 0)
            {
                AssignSlot(slot, pane);
                break;
            }
        }
    }
}

void
ComputeKernelDetailsView::ResetLayout()
{
    m_layout = LayoutSettings();
    SetKernelRailTableMode(false);
    for(const std::unique_ptr<ComputeKernelDetailsPane>& pane : m_panes)
    {
        pane->ResetLayout();
    }
}

ComputeKernelDetailsView::LayoutState
ComputeKernelDetailsView::CurrentLayoutState() const
{
    LayoutState state;
    state.layout      = CurrentTemplate();
    state.maximized   = MaximizedPane() != ComputePane::kCount;
    state.kernel_rail = m_layout.show_kernel_list;
    return state;
}

void
ComputeKernelDetailsView::RebuildLayout(const LayoutState& state)
{
    m_layout_nodes.clear();
    m_layout_root = NO_LAYOUT_NODE;
    if(state.maximized)
    {
        m_root_split->SetRight(m_maximized_pane);
    }
    else
    {
        m_layout_root = BuildTemplate(state.layout);
        m_root_split->SetRight(m_layout_nodes[static_cast<size_t>(m_layout_root)].item);
    }
    // Maximize fills the whole tab, so the kernel rail steps aside too; kernels
    // are picked again once restored.
    m_rail_pane->m_visible = state.kernel_rail && !state.maximized;
    m_built_layout         = state;
    m_layout_built         = true;
}

int32_t
ComputeKernelDetailsView::BoxNode(int32_t slot)
{
    LayoutNode node{ slot, nullptr, nullptr, NO_LAYOUT_NODE, NO_LAYOUT_NODE,
                     m_slot_panes[static_cast<size_t>(slot)] };
    m_layout_nodes.push_back(std::move(node));
    return static_cast<int32_t>(m_layout_nodes.size()) - 1;
}

int32_t
ComputeKernelDetailsView::HSplitNode(int32_t left, int32_t right, float ratio)
{
    const LayoutItem::Ptr left_item  = m_layout_nodes[static_cast<size_t>(left)].item;
    const LayoutItem::Ptr right_item = m_layout_nodes[static_cast<size_t>(right)].item;
    std::shared_ptr<HSplitContainer> split =
        std::make_shared<HSplitContainer>(left_item, right_item);
    split->SetSplit(ratio);
    split->SetSplitterSize(PANE_GUTTER);
    LayoutNode node{ NO_LAYOUT_NODE, split, nullptr, left, right, MakeSplitPane(split) };
    m_layout_nodes.push_back(std::move(node));
    return static_cast<int32_t>(m_layout_nodes.size()) - 1;
}

int32_t
ComputeKernelDetailsView::VSplitNode(int32_t top, int32_t bottom, float ratio)
{
    const LayoutItem::Ptr top_item    = m_layout_nodes[static_cast<size_t>(top)].item;
    const LayoutItem::Ptr bottom_item = m_layout_nodes[static_cast<size_t>(bottom)].item;
    std::shared_ptr<VSplitContainer> split =
        std::make_shared<VSplitContainer>(top_item, bottom_item);
    split->SetSplit(ratio);
    split->SetSplitterSize(PANE_GUTTER);
    LayoutNode node{ NO_LAYOUT_NODE, nullptr, split, top, bottom, MakeSplitPane(split) };
    m_layout_nodes.push_back(std::move(node));
    return static_cast<int32_t>(m_layout_nodes.size()) - 1;
}

int32_t
ComputeKernelDetailsView::BuildTemplate(ComputeLayoutTemplate layout)
{
    switch(layout)
    {
        case ComputeLayoutTemplate::kSingle: return BoxNode(0);
        case ComputeLayoutTemplate::kSideBySide:
            return HSplitNode(BoxNode(0), BoxNode(1), HALF);
        case ComputeLayoutTemplate::kStacked:
            return VSplitNode(BoxNode(0), BoxNode(1), HALF);
        case ComputeLayoutTemplate::kMainRight:
            return HSplitNode(BoxNode(0), VSplitNode(BoxNode(1), BoxNode(2), HALF),
                              MAIN_SPLIT_RATIO);
        case ComputeLayoutTemplate::kThreeColumns:
            return HSplitNode(BoxNode(0), HSplitNode(BoxNode(1), BoxNode(2), HALF),
                              THIRD);
        case ComputeLayoutTemplate::kMainThreeBelow:
            return VSplitNode(
                BoxNode(0),
                HSplitNode(BoxNode(1), HSplitNode(BoxNode(2), BoxNode(3), HALF), THIRD),
                MAIN_SPLIT_RATIO);
        case ComputeLayoutTemplate::kGrid:
            return VSplitNode(HSplitNode(BoxNode(0), BoxNode(1), HALF),
                              HSplitNode(BoxNode(2), BoxNode(3), HALF), HALF);
        case ComputeLayoutTemplate::kMainBelow:
        case ComputeLayoutTemplate::kCount: break;
    }
    return VSplitNode(BoxNode(0), HSplitNode(BoxNode(1), BoxNode(2), HALF),
                      MAIN_SPLIT_RATIO);
}

void
ComputeKernelDetailsView::UpdatePaneMinSizes()
{
    const float em       = ImGui::GetFontSize();
    const float rail_min = std::max(
        em * (m_layout.kernel_list_table ? RAIL_TABLE_MIN_WIDTH_EMS : RAIL_LIST_MIN_WIDTH_EMS),
        m_rail_header_width);
    m_root_split->SetMinLeftWidth(rail_min);
    float workspace_min = em * WORKSPACE_MIN_WIDTH_EMS;
    if(m_layout_root != NO_LAYOUT_NODE)
    {
        // Panels that want more room than the workspace has would pin every
        // gutter between them, so their minimums shrink together to fit.
        const float total_w = ImGui::GetContentRegionAvail().x;
        const float rail_w =
            m_rail_pane->m_visible
                ? std::max(rail_min, total_w * m_root_split->GetSplit()) + PANE_GUTTER
                : 0.0f;
        const float limit  = (total_w - rail_w) * PANE_MIN_MAX_WORKSPACE_SHARE;
        float       needed = ApplyNodeMinSizes(m_layout_root, 1.0f).x;
        if(limit > 0.0f && needed > limit)
        {
            needed = ApplyNodeMinSizes(m_layout_root, limit / needed).x;
        }
        workspace_min = std::max(workspace_min, needed);
    }
    m_root_split->SetMinRightWidth(workspace_min);
}

void
ComputeKernelDetailsView::UpdatePanesShown()
{
    const ComputePane maximized = MaximizedPane();
    for(int32_t i = 0; i < PANE_COUNT; ++i)
    {
        const ComputePane pane = PaneAt(i);
        Pane(pane).SetShown(maximized != ComputePane::kCount ? pane == maximized
                                                              : ActiveSlotOf(pane) >= 0);
    }
}

ImVec2
ComputeKernelDetailsView::ApplyNodeMinSizes(int32_t node, float width_scale)
{
    const LayoutNode& item = m_layout_nodes[static_cast<size_t>(node)];
    if(item.hsplit)
    {
        const ImVec2 left  = ApplyNodeMinSizes(item.first, width_scale);
        const ImVec2 right = ApplyNodeMinSizes(item.second, width_scale);
        item.hsplit->SetMinLeftWidth(left.x);
        item.hsplit->SetMinRightWidth(right.x);
        return ImVec2(left.x + PANE_GUTTER + right.x, std::max(left.y, right.y));
    }
    if(item.vsplit)
    {
        const ImVec2 top    = ApplyNodeMinSizes(item.first, width_scale);
        const ImVec2 bottom = ApplyNodeMinSizes(item.second, width_scale);
        item.vsplit->SetMinTopHeight(top.y);
        item.vsplit->SetMinBottomHeight(bottom.y);
        return ImVec2(std::max(top.x, bottom.x), top.y + PANE_GUTTER + bottom.y);
    }
    ImVec2 size = SlotMinSize(item.slot);
    size.x *= width_scale;
    return size;
}

ImVec2
ComputeKernelDetailsView::SlotMinSize(int32_t slot) const
{
    const ComputePane pane = SlotPane(slot);
    ImVec2 size = pane == ComputePane::kCount ? ComputeKernelDetailsPane::DefaultMinSize()
                                              : Pane(pane).MinSize();
    if(IsSlotIndex(slot))
    {
        size.x = std::max(size.x, m_slot_header_widths[static_cast<size_t>(slot)]);
    }
    return size;
}

void
ComputeKernelDetailsView::SetKernelRailTableMode(bool table)
{
    m_layout.kernel_list_table = table;
    m_root_split->SetSplit(RailSplitRatio(table));
}

void
ComputeKernelDetailsView::RenderLayoutControls()
{
    // The app toolbar has no item spacing; the label, combo and its dropdown
    // need the app's default.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        SettingsManager::GetInstance().GetDefaultStyle().ItemSpacing);
    RenderLayoutMenu();
    ImGui::PopStyleVar();
}

void
ComputeKernelDetailsView::RenderLayoutMenu()
{
    SettingsManager&            settings = SettingsManager::GetInstance();
    const ComputeLayoutTemplate current  = CurrentTemplate();
    const ImU32                 dim      = settings.GetColor(Colors::kTextDim);
    const ImU32                 accent   = settings.GetColor(Colors::kAccent);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Layout:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFrameHeight() * LAYOUT_COMBO_WIDTH_FRAMES);
    PushComboStyles();
    if(ImGui::BeginCombo("##compute_layout", "", ImGuiComboFlags_HeightLargest))
    {
        const float row_h   = ImGui::GetFrameHeight() * LAYOUT_ROW_HEIGHT_FRAMES;
        const float thumb_h = row_h - THUMBNAIL_INSET * 2.0f;
        const float thumb_w = thumb_h * THUMBNAIL_ASPECT;
        for(ComputeLayoutTemplate layout : LAYOUT_MENU_ORDER)
        {
            ImGui::PushID(static_cast<int>(layout));
            const bool   selected = layout == current;
            const ImVec2 row_min  = ImGui::GetCursorScreenPos();
            if(ImGui::Selectable("##layout", selected, ImGuiSelectableFlags_None,
                                 ImVec2(0.0f, row_h)))
            {
                SetTemplate(layout);
            }
            ImDrawList*  draw_list = ImGui::GetWindowDrawList();
            const ImVec2 thumb_min(row_min.x + THUMBNAIL_INSET, row_min.y + THUMBNAIL_INSET);
            DrawLayoutThumbnail(draw_list, thumb_min, ImVec2(thumb_w, thumb_h), layout,
                                selected ? accent : dim);
            const char* name = TemplateInfo(layout).name;
            draw_list->AddText(
                ImVec2(thumb_min.x + thumb_w + THUMBNAIL_LABEL_GAP,
                       row_min.y + (row_h - ImGui::GetTextLineHeight()) * 0.5f),
                settings.GetColor(Colors::kTextMain), name);
            ImGui::PopID();
        }
        ImGui::Separator();
        if(ImGui::Selectable("Reset layout"))
        {
            ResetLayout();
        }
        ImGui::EndCombo();
    }
    // The preview, the template's thumbnail and name, is drawn over the combo's
    // frame (the last item again once the popup has ended), left of its arrow.
    if(ImGui::IsItemVisible())
    {
        const ImGuiStyle& style     = ImGui::GetStyle();
        const ImVec2      frame_min = ImGui::GetItemRectMin();
        const ImVec2      frame_max = ImGui::GetItemRectMax();
        const float       thumb_h   = ImGui::GetTextLineHeight();
        const ImVec2      thumb_size(thumb_h * THUMBNAIL_ASPECT, thumb_h);
        const ImVec2      thumb_min(frame_min.x + style.FramePadding.x,
                                    frame_min.y + style.FramePadding.y);
        ImDrawList*       draw_list = ImGui::GetWindowDrawList();
        draw_list->PushClipRect(frame_min,
                                ImVec2(std::max(frame_min.x,
                                                frame_max.x - ImGui::GetFrameHeight()),
                                       frame_max.y),
                                true);
        DrawLayoutThumbnail(draw_list, thumb_min, thumb_size, current, dim);
        draw_list->AddText(
            ImVec2(thumb_min.x + thumb_size.x + THUMBNAIL_LABEL_GAP, thumb_min.y),
            ImGui::GetColorU32(ImGuiCol_Text), TemplateInfo(current).name);
        draw_list->PopClipRect();
    }
    PopComboStyles();
    if(ImGui::IsItemHovered())
    {
        SetTooltipStyled("How the workspace is split into boxes.\n"
                         "Drag a box's title onto another box to swap their panels,\n"
                         "or use the arrow next to a title to choose its panel.");
    }
}

int32_t
ComputeKernelDetailsView::RenderPaneHeader(
    const char* title, int32_t slot, bool draggable, bool pickable,
    const ComputeKernelDetailsPane::HeaderButton* buttons, int32_t count,
    bool& title_double_clicked, float* row_width)
{
    SettingsManager&  settings   = SettingsManager::GetInstance();
    const ImGuiStyle& style      = settings.GetDefaultStyle();
    const float       button_gap = style.ItemSpacing.x * 0.5f;

    // Title: in a box it is the drag handle (move cursor) - dragging it onto
    // another box swaps their panels. PushFont takes an unscaled size; the
    // title is drawn at the scaled size it was measured at.
    ImGui::PushFont(settings.GetFontManager().GetFont(FontType::kDefault),
                    settings.GetFontManager().GetFontSize(FontSize::kMedLarge));
    ImFont*      title_font = ImGui::GetFont();
    const float  title_px   = ImGui::GetFontSize();
    const ImVec2 text_size  = ImGui::CalcTextSize(title);
    ImGui::PopFont();
    const ImVec2 title_min  = ImGui::GetCursorScreenPos();
    const ImVec2 title_size(text_size.x + TITLE_HANDLE_PAD_X * 2.0f,
                            std::max(ImGui::GetFrameHeight(), text_size.y));
    ImGui::InvisibleButton("##pane_title", title_size);
    const bool title_hovered = ImGui::IsItemHovered();
    const bool title_active  = ImGui::IsItemActive();
    title_double_clicked     = title_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    ImDrawList* draw_list    = ImGui::GetWindowDrawList();
    if(draggable && (title_hovered || title_active))
    {
        draw_list->AddRectFilled(title_min,
                                 ImVec2(title_min.x + title_size.x, title_min.y + title_size.y),
                                 settings.GetColor(Colors::kButtonHovered), style.FrameRounding);
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    }
    // An empty box (a picker but nothing to drag) has a placeholder title.
    const bool placeholder_title = pickable && !draggable;
    draw_list->AddText(title_font, title_px,
                       ImVec2(title_min.x + TITLE_HANDLE_PAD_X,
                              title_min.y + (title_size.y - text_size.y) * 0.5f),
                       settings.GetColor(placeholder_title ? Colors::kTextDim
                                                           : Colors::kTextMain),
                       title);
    if(draggable)
    {
        if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
        {
            ImGui::SetDragDropPayload(PANE_DRAG_PAYLOAD, &slot, sizeof(slot));
            ImGui::TextUnformatted(title);
            ImGui::TextDisabled("Drop on another box to swap");
            ImGui::EndDragDropSource();
        }
        else if(BeginItemTooltipStyled())
        {
            ImGui::TextUnformatted("Drag onto another box to swap panels");
            ImGui::TextDisabled("Double-click to maximize or restore");
            EndTooltipStyled();
        }
    }
    float row_w = title_size.x;
    if(pickable)
    {
        ImGui::SameLine(0.0f, button_gap);
        if(FramedIconButton(ICON_CHEVRON_DOWN, "Choose this box's panel",
                            settings.GetColor(Colors::kTransparent)))
        {
            ImGui::OpenPopup("panel_picker");
        }
        row_w = ImGui::GetItemRectMax().x - title_min.x;
        RenderPanelPicker(slot);
    }

    int32_t clicked = -1;
    if(count > 0)
    {
        // Right-align the buttons on the title's row.
        float buttons_w = button_gap * static_cast<float>(count - 1);
        for(int32_t i = 0; i < count; ++i)
        {
            buttons_w += buttons[i].icon ? IconWidth(buttons[i].icon)
                                         : ImGui::CalcTextSize(buttons[i].label).x;
            buttons_w += ImGui::GetStyle().FramePadding.x * 2.0f;
        }
        row_w += ImGui::GetStyle().ItemSpacing.x + buttons_w;
        SameLineRightAligned(buttons_w);
        for(int32_t i = 0; i < count; ++i)
        {
            const ComputeKernelDetailsPane::HeaderButton& button = buttons[i];
            if(i > 0)
            {
                ImGui::SameLine(0.0f, button_gap);
            }
            const ImU32 bg =
                settings.GetColor(button.active ? Colors::kButton : Colors::kTransparent);
            ImGui::PushID(i);
            bool pressed = false;
            if(button.icon)
            {
                pressed = FramedIconButton(button.icon, button.tooltip, bg);
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Button, bg);
                pressed = ImGui::Button(button.label);
                ImGui::PopStyleColor();
                if(button.tooltip && ImGui::IsItemHovered())
                {
                    SetTooltipStyled("%s", button.tooltip);
                }
            }
            ImGui::PopID();
            if(pressed)
            {
                clicked = i;
            }
        }
    }

    if(row_width)
    {
        *row_width = row_w;
    }
    ImGui::PushStyleColor(ImGuiCol_Separator, settings.GetColor(Colors::kBorderColor));
    ImGui::Separator();
    ImGui::PopStyleColor();
    return clicked;
}

void
ComputeKernelDetailsView::RenderPanelPicker(int32_t slot)
{
    if(!ImGui::BeginPopup("panel_picker"))
    {
        return;
    }
    ImGui::TextDisabled("Show in this box");
    ImGui::Separator();
    const ComputePane current = SlotPane(slot);
    for(int32_t i = 0; i < PANE_COUNT; ++i)
    {
        const ComputePane pane = PaneAt(i);
        if(!IsPaneAvailable(pane))
        {
            continue;
        }
        // A panel shown elsewhere trades places with this box's panel.
        const int32_t where = ActiveSlotOf(pane);
        const char*   hint  = where >= 0 && where != slot ? "swap" : nullptr;
        if(ImGui::MenuItem(Pane(pane).Title(), hint, pane == current))
        {
            AssignSlot(slot, pane);
        }
    }
    if(current != ComputePane::kCount)
    {
        ImGui::Separator();
        if(ImGui::MenuItem("Empty this box"))
        {
            ClearSlot(slot);
        }
    }
    ImGui::EndPopup();
}

void
ComputeKernelDetailsView::RenderSlot(int32_t slot)
{
    const ComputePane pane = SlotPane(slot);
    if(pane == ComputePane::kCount)
    {
        RenderEmptySlot(slot);
    }
    else
    {
        RenderPane(pane, slot);
    }
}

void
ComputeKernelDetailsView::HandleSlotDrop(int32_t slot)
{
    if(!ImGui::BeginDragDropTarget())
    {
        return;
    }
    const ImGuiPayload* payload = ImGui::GetDragDropPayload();
    if(payload && payload->IsDataType(PANE_DRAG_PAYLOAD) &&
       *static_cast<const int32_t*>(payload->Data) != slot)
    {
        // Highlight the box under the cursor and say what the drop will do.
        SettingsManager& settings = SettingsManager::GetInstance();
        const ImU32      accent   = settings.GetColor(Colors::kAccent);
        const float      rounding = settings.GetDefaultStyle().ChildRounding;
        const ImVec2     slot_min = ImGui::GetItemRectMin();
        const ImVec2     slot_max = ImGui::GetItemRectMax();
        ImDrawList*      overlay  = ImGui::GetForegroundDrawList();
        overlay->AddRectFilled(slot_min, slot_max, ApplyAlpha(accent, DROP_FILL_ALPHA), rounding);
        overlay->AddRect(slot_min, slot_max, accent, rounding, 0, DROP_BORDER_THICKNESS);

        const ComputePane target = SlotPane(slot);
        const std::string label  = target == ComputePane::kCount
                                       ? std::string("Move here")
                                       : std::string("Swap with ") + Pane(target).Title();
        const ImVec2      text   = ImGui::CalcTextSize(label.c_str());
        const ImVec2      center((slot_min.x + slot_max.x) * 0.5f,
                                 (slot_min.y + slot_max.y) * 0.5f);
        const ImVec2      pill_min(center.x - text.x * 0.5f - DROP_LABEL_PAD,
                                   center.y - text.y * 0.5f - DROP_LABEL_PAD);
        const ImVec2      pill_max(center.x + text.x * 0.5f + DROP_LABEL_PAD,
                                   center.y + text.y * 0.5f + DROP_LABEL_PAD);
        overlay->AddRectFilled(pill_min, pill_max, settings.GetColor(Colors::kBgPanel),
                               rounding);
        overlay->AddRect(pill_min, pill_max, accent, rounding);
        overlay->AddText(ImVec2(pill_min.x + DROP_LABEL_PAD, pill_min.y + DROP_LABEL_PAD),
                         settings.GetColor(Colors::kTextMain), label.c_str());
    }
    if(const ImGuiPayload* dropped = ImGui::AcceptDragDropPayload(
           PANE_DRAG_PAYLOAD, ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
    {
        SwapSlots(*static_cast<const int32_t*>(dropped->Data), slot);
    }
    ImGui::EndDragDropTarget();
}

void
ComputeKernelDetailsView::RenderPane(ComputePane pane, int32_t slot)
{
    ComputeKernelDetailsPane& panel     = Pane(pane);
    const bool                maximized = slot == MAXIMIZED_SLOT;

    // The panel's own buttons, then maximize (or restore) and hide.
    ComputeKernelDetailsPane::HeaderButton buttons[MAX_PANE_HEADER_BUTTONS];
    const int32_t maximize_button = panel.HeaderButtons(buttons);
    const int32_t hide_button     = maximize_button + 1;
    IM_ASSERT(maximize_button <= ComputeKernelDetailsPane::MAX_HEADER_BUTTONS);
    if(maximized)
    {
        buttons[maximize_button] = { ICON_ARROWS_SHRINK, nullptr, "Restore the layout",
                                     false, maximize_button };
    }
    else
    {
        buttons[maximize_button] = {
            ICON_ARROWS_EXPAND, nullptr,
            "Maximize: fill the tab, hiding the other boxes and kernels", false,
            maximize_button
        };
    }
    buttons[hide_button] = { ICON_X_CIRCLED, nullptr, "Hide (empties the box)", false,
                             hide_button };

    BeginPaneCard("pane_card");
    bool          title_double_clicked = false;
    float         header_w             = 0.0f;
    const int32_t clicked =
        RenderPaneHeader(panel.Title(), slot, !maximized, !maximized, buttons,
                         hide_button + 1, title_double_clicked, &header_w);
    if(IsSlotIndex(slot))
    {
        m_slot_header_widths[static_cast<size_t>(slot)] = PaneCardWidth(header_w);
    }
    ImGui::BeginChild("pane_body", ImVec2(0, 0));
    panel.RenderBody(maximized);
    ImGui::EndChild();
    EndPaneCard();
    if(!maximized)
    {
        HandleSlotDrop(slot);
    }

    // Layout changes take effect as a relayout on the next frame. Double-clicking
    // the title toggles maximize.
    const bool toggle_maximize = title_double_clicked || clicked == maximize_button;
    if(toggle_maximize && maximized)
    {
        m_layout.maximized = false;
    }
    else if(toggle_maximize)
    {
        m_layout.maximized_pane = static_cast<int32_t>(pane);
        m_layout.maximized      = true;
    }
    else if(clicked == hide_button)
    {
        const int32_t box = ActiveSlotOf(pane);
        if(box >= 0)
        {
            ClearSlot(box);
        }
        m_layout.maximized = false;
    }
    else if(clicked >= 0)
    {
        panel.OnHeaderButton(buttons[clicked].id);
    }
}

void
ComputeKernelDetailsView::RenderEmptySlot(int32_t slot)
{
    BeginPaneCard("empty_card");
    bool title_double_clicked = false;
    float header_w = 0.0f;
    RenderPaneHeader("Empty box", slot, false, true, nullptr, 0, title_double_clicked,
                     &header_w);
    if(IsSlotIndex(slot))
    {
        m_slot_header_widths[static_cast<size_t>(slot)] = PaneCardWidth(header_w);
    }

    ImGui::BeginChild("empty_body", ImVec2(0, 0));
    // Offer the panels not shown anywhere in a centred dropdown, which stays one
    // width however many there are.
    bool any_unshown = false;
    for(int32_t i = 0; i < PANE_COUNT && !any_unshown; ++i)
    {
        const ComputePane pane = PaneAt(i);
        any_unshown            = IsPaneAvailable(pane) && ActiveSlotOf(pane) < 0;
    }
    const char* hint = any_unshown ? "Drag a panel here, or show:"
                                   : "Drag a panel's title here to move it.";
    ImGui::Dummy(ImVec2(0.0f, ImGui::GetFrameHeight()));
    CenterNextTextItem(hint);
    ImGui::TextDisabled("%s", hint);
    if(any_unshown)
    {
        const float combo_w = std::min(ImGui::GetFrameHeight() * EMPTY_BOX_COMBO_WIDTH_FRAMES,
                                       ImGui::GetContentRegionAvail().x);
        CenterNextItem(combo_w);
        ImGui::SetNextItemWidth(combo_w);
        PushComboStyles();
        if(ImGui::BeginCombo("##empty_box_panel", "Choose a panel"))
        {
            for(int32_t i = 0; i < PANE_COUNT; ++i)
            {
                const ComputePane pane = PaneAt(i);
                if(IsPaneAvailable(pane) && ActiveSlotOf(pane) < 0 &&
                   ImGui::Selectable(Pane(pane).Title()))
                {
                    AssignSlot(slot, pane);
                }
            }
            ImGui::EndCombo();
        }
        PopComboStyles();
    }
    ImGui::EndChild();
    EndPaneCard();
    HandleSlotDrop(slot);
}

void
ComputeKernelDetailsView::RenderKernelRail()
{
    // Indices of the header buttons below.
    constexpr int32_t LIST_BUTTON  = 0;
    constexpr int32_t TABLE_BUTTON = 1;
    constexpr int32_t HIDE_BUTTON  = 2;

    const bool table = m_layout.kernel_list_table;
    const ComputeKernelDetailsPane::HeaderButton buttons[] = {
        { nullptr, "List", "Kernels as tabs, by total duration", !table, LIST_BUTTON },
        { nullptr, "Table", "Full kernel table: sort, filter and add metric columns",
          table, TABLE_BUTTON },
        { ICON_X_CIRCLED, nullptr, "Hide (turn it back on from View > Show Kernel List)",
          false, HIDE_BUTTON },
    };

    BeginPaneCard("kernel_rail_card");
    bool          title_double_clicked = false;
    float         header_w             = 0.0f;
    const int32_t clicked =
        RenderPaneHeader("Kernels", -1, false, false, buttons,
                         static_cast<int32_t>(IM_ARRAYSIZE(buttons)),
                         title_double_clicked, &header_w);
    m_rail_header_width = PaneCardWidth(header_w);
    ImGui::BeginChild("kernel_rail_body", ImVec2(0, 0), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if(m_compare.enabled)
    {
        RenderCompareCard();
    }
    if(table)
    {
        m_kernel_metric_table->Render();
    }
    else
    {
        RenderKernelList();
    }
    ImGui::EndChild();
    EndPaneCard();

    if(clicked == LIST_BUTTON && table)
    {
        SetKernelRailTableMode(false);
    }
    else if(clicked == TABLE_BUTTON && !table)
    {
        SetKernelRailTableMode(true);
    }
    else if(clicked == HIDE_BUTTON)
    {
        m_layout.show_kernel_list = false;
    }
}

void
ComputeKernelDetailsView::RefreshKernelList()
{
    // Workloads and kernels are fixed once the database has loaded.
    if(!m_kernel_groups.empty())
    {
        return;
    }
    for(const WorkloadInfo* workload : m_data_provider.ComputeModel().GetWorkloadList())
    {
        if(!workload)
        {
            continue;
        }
        KernelGroup group{ workload, KernelsByDuration(workload->id), 0.0f };
        double      total = 0.0;
        for(const KernelInfo* kernel : group.kernels)
        {
            total += static_cast<double>(kernel->dispatch_metrics[KernelInfo::DurationTotal]);
        }
        group.total_duration = static_cast<float>(total);
        m_kernel_groups.push_back(std::move(group));
    }
}

void
ComputeKernelDetailsView::RenderKernelList()
{
    RefreshKernelList();

    ImGui::SetNextItemWidth(-FLT_MIN);
    InputTextStringWithHint("##kernel_filter", "Search kernels", m_kernel_filter);
    RenderKernelListOrder();
    ImGui::Spacing();

    ImGui::BeginChild("kernel_list_scroll", ImVec2(0, 0));
    const uint32_t selected_workload = m_compute_selection->GetSelectedWorkload();
    const uint32_t selected_kernel   = m_compute_selection->GetSelectedKernel();
    // A selection made elsewhere (the kernel table, Swap) scrolls into view once.
    const bool scroll_to_selected = selected_kernel != m_kernel_list_selection ||
                                    selected_workload != m_kernel_list_selection_workload;

    std::vector<ListedKernel> shown      = ListedKernels();
    const bool                any_listed = !shown.empty();
    shown.erase(std::remove_if(shown.begin(), shown.end(),
                               [this](const ListedKernel& item) {
                                   return !contains_ignore_case(item.kernel->name,
                                                                m_kernel_filter);
                               }),
                shown.end());
    // Several workloads get a header per workload while the order keeps each
    // workload's kernels together; a sort that mixes them names the workload
    // on each tab instead.
    const bool            grouped = m_kernel_groups.size() > 1;
    bool                  mixed   = false;
    std::vector<uint32_t> seen_workloads;
    for(size_t i = 0; grouped && i < shown.size(); ++i)
    {
        const uint32_t workload_id = shown[i].workload->id;
        if(i > 0 && shown[i - 1].workload->id == workload_id)
        {
            continue;
        }
        if(std::find(seen_workloads.begin(), seen_workloads.end(), workload_id) !=
           seen_workloads.end())
        {
            mixed = true;
            break;
        }
        seen_workloads.push_back(workload_id);
    }

    const bool can_compare     = CanCompare();
    uint32_t   header_workload = ComputeSelection::INVALID_SELECTION_ID;
    for(const ListedKernel& item : shown)
    {
        const uint32_t workload_id = item.workload->id;
        if(grouped && !mixed && workload_id != header_workload)
        {
            ImGui::SeparatorText(item.workload->name.c_str());
            header_workload = workload_id;
        }
        const bool selected =
            workload_id == selected_workload && item.kernel->id == selected_kernel;
        ImGui::PushID(static_cast<int>(workload_id));
        RenderKernelTab(*item.kernel, workload_id, selected, can_compare,
                        item.workload_duration,
                        mixed ? item.workload->name.c_str() : nullptr);
        ImGui::PopID();
        if(selected && scroll_to_selected)
        {
            ImGui::SetScrollHereY(0.5f);
        }
    }
    if(shown.empty())
    {
        const bool filtered =
            m_kernel_metric_table && m_kernel_metric_table->GetActiveFilterCount() > 0;
        ImGui::TextDisabled("%s", any_listed ? "No kernels match the search."
                                  : filtered ? "No kernels match the Table's filters."
                                             : "No kernels.");
    }
    m_kernel_list_selection          = m_compute_selection->GetSelectedKernel();
    m_kernel_list_selection_workload = m_compute_selection->GetSelectedWorkload();
    ImGui::EndChild();
}

void
ComputeKernelDetailsView::RenderKernelListOrder()
{
    if(!m_kernel_metric_table || !m_kernel_metric_table->HasShownKernels())
    {
        return;
    }
    const bool   sorted  = !m_kernel_metric_table->IsDefaultSort();
    const size_t filters = m_kernel_metric_table->GetActiveFilterCount();
    if(!sorted && filters == 0)
    {
        return;
    }
    ImGui::PushTextWrapPos(0.0f);
    if(sorted)
    {
        ImGui::TextDisabled("Table sort: %s (%s)",
                            m_kernel_metric_table->GetSortColumnName().c_str(),
                            m_kernel_metric_table->IsSortAscending() ? "ascending"
                                                                     : "descending");
    }
    if(filters > 0)
    {
        size_t total = 0;
        for(const KernelGroup& group : m_kernel_groups)
        {
            total += group.kernels.size();
        }
        ImGui::TextDisabled("Table filters: %zu of %zu kernels",
                            m_kernel_metric_table->GetShownKernels().size(), total);
        const char* clear_label = "Clear filters";
        ImGui::SameLine();
        if(ImGui::GetContentRegionAvail().x <
           ImGui::CalcTextSize(clear_label).x + ImGui::GetStyle().FramePadding.x * 2.0f)
        {
            ImGui::NewLine();
        }
        if(ImGui::SmallButton(clear_label))
        {
            m_kernel_metric_table->ClearAllFilters();
        }
    }
    ImGui::PopTextWrapPos();
}

std::vector<ComputeKernelDetailsView::ListedKernel>
ComputeKernelDetailsView::ListedKernels() const
{
    std::vector<ListedKernel> listed;
    if(m_kernel_metric_table && m_kernel_metric_table->HasShownKernels())
    {
        const ComputeDataModel& model = m_data_provider.ComputeModel();
        for(const KernelMetricTable::ShownKernel& shown :
            m_kernel_metric_table->GetShownKernels())
        {
            const KernelInfo* kernel = model.GetKernelInfo(shown.workload_id, shown.kernel_id);
            if(!kernel)
            {
                continue;
            }
            for(const KernelGroup& group : m_kernel_groups)
            {
                if(group.workload->id == shown.workload_id)
                {
                    listed.push_back({ group.workload, kernel, group.total_duration });
                    break;
                }
            }
        }
        return listed;
    }
    for(const KernelGroup& group : m_kernel_groups)
    {
        for(const KernelInfo* kernel : group.kernels)
        {
            listed.push_back({ group.workload, kernel, group.total_duration });
        }
    }
    return listed;
}

void
ComputeKernelDetailsView::RenderKernelTab(const KernelInfo& kernel, uint32_t workload_id,
                                          bool selected, bool can_compare,
                                          float total_duration,
                                          const char* workload_label)
{
    SettingsManager& settings = SettingsManager::GetInstance();
    const float      line_h   = ImGui::GetTextLineHeight();
    const float      width    = std::max(ImGui::GetContentRegionAvail().x, 1.0f);
    const float      height   = KERNEL_TAB_PAD * 2.0f + line_h * 2.0f +
                         KERNEL_TAB_LINE_GAP * 2.0f + KERNEL_TAB_BAR_HEIGHT;
    // While comparing, A is the selection and B may be any listed kernel.
    const bool is_a = m_compare.enabled && selected;
    const bool is_b = m_compare.enabled && workload_id == m_compare.workload_id &&
                      kernel.id == m_compare.kernel_id;
    // A column at the right end of the tab holds the A / B marker; clicking it
    // makes the kernel B (or, on B, stops comparing).
    const ImVec2 badge_size = BadgeSize("B");
    const float  slot_w     = std::min(badge_size.x + KERNEL_TAB_PAD * 2.0f,
                                       width * KERNEL_TAB_SLOT_MAX_SHARE);
    const float  body_w     = std::max(width - slot_w, 1.0f);
    const bool   can_be_b   = !selected && can_compare;

    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 max(min.x + width, min.y + height);
    ImGui::PushID(static_cast<int>(kernel.id));
    const bool clicked      = ImGui::InvisibleButton("##kernel_tab", ImVec2(body_w, height));
    const bool body_hovered = ImGui::IsItemHovered();
    if(ImGui::BeginPopupContextItem("kernel_tab_menu"))
    {
        if(is_b)
        {
            if(ImGui::MenuItem("Stop comparing"))
            {
                DisableCompare();
            }
        }
        else if(ImGui::MenuItem("Compare with this kernel (B)", nullptr, false, can_be_b))
        {
            SetCompareTarget(workload_id, kernel.id);
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine(0.0f, 0.0f);
    const bool slot_clicked = ImGui::InvisibleButton("##kernel_b_slot", ImVec2(slot_w, height));
    const bool slot_hovered = ImGui::IsItemHovered();
    ImGui::PopID();

    if(slot_clicked)
    {
        if(is_b)
        {
            DisableCompare();
        }
        else if(can_be_b)
        {
            SetCompareTarget(workload_id, kernel.id);
        }
    }
    if(clicked && !selected)
    {
        m_compute_selection->Select(workload_id, kernel.id);
    }
    // The rest only draws, so a tab scrolled out of view stops here.
    if(!ImGui::IsRectVisible(
           ImVec2(min.x - KERNEL_TAB_CULL_MARGIN, min.y - KERNEL_TAB_CULL_MARGIN),
           ImVec2(max.x + KERNEL_TAB_CULL_MARGIN, max.y + KERNEL_TAB_CULL_MARGIN)))
    {
        return;
    }

    const bool  hovered   = body_hovered || slot_hovered;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    if(selected || hovered)
    {
        draw_list->AddRectFilled(min, max,
                                 settings.GetColor(selected ? Colors::kSelection
                                                            : Colors::kButtonHovered),
                                 KERNEL_TAB_ROUNDING);
    }
    if(selected || is_b)
    {
        draw_list->AddRectFilled(min, ImVec2(min.x + KERNEL_TAB_ACCENT_WIDTH, max.y),
                                 settings.GetColor(is_b ? Colors::kComparisonTarget
                                                        : Colors::kAccent),
                                 KERNEL_TAB_ROUNDING, ImDrawFlags_RoundCornersLeft);
    }

    const double duration =
        static_cast<double>(kernel.dispatch_metrics[KernelInfo::DurationTotal]);
    const float share =
        total_duration > 0.0f ? static_cast<float>(duration / total_duration) : 0.0f;
    const TimeFormat time_format = settings.GetUserSettings().unit_settings.time_format;
    char             stats[KERNEL_TAB_STATS_SIZE];
    std::snprintf(stats, sizeof(stats), "%s%s%.1f%%   %s   %llu calls",
                  workload_label ? workload_label : "", workload_label ? "   " : "",
                  share * 100.0f,
                  nanosecond_to_formatted_str(duration, time_format, true).c_str(),
                  static_cast<unsigned long long>(
                      kernel.dispatch_metrics[KernelInfo::InvocationCount]));

    const float       text_x = min.x + KERNEL_TAB_ACCENT_WIDTH + KERNEL_TAB_PAD;
    const float       text_w = std::max(0.0f, min.x + body_w - text_x);
    float             y      = min.y + KERNEL_TAB_PAD;

    // The marker column: A and B as badges; elsewhere a faint B on hover.
    const ImVec2 badge_min(min.x + body_w + (slot_w - badge_size.x) * 0.5f, y);
    if(is_a || is_b)
    {
        DrawBadge(draw_list, badge_min, is_a ? "A" : "B",
                  is_a ? Colors::kComparisonBase : Colors::kComparisonTarget);
    }
    else if(hovered && can_be_b)
    {
        const ImU32 target = settings.GetColor(Colors::kComparisonTarget);
        const ImVec2 badge_max(badge_min.x + badge_size.x, badge_min.y + badge_size.y);
        if(slot_hovered)
        {
            draw_list->AddRectFilled(badge_min, badge_max,
                                     ApplyAlpha(target, COMPARE_BADGE_HOVER_ALPHA),
                                     COMPARE_BADGE_ROUNDING);
        }
        draw_list->AddRect(badge_min, badge_max, target, COMPARE_BADGE_ROUNDING);
        draw_list->AddText(ImVec2(badge_min.x + COMPARE_BADGE_PAD_X, badge_min.y),
                           settings.GetColor(slot_hovered ? Colors::kTextMain
                                                          : Colors::kTextDim),
                           "B");
    }
    const std::string name = ElideWithEllipsis(kernel.name, text_w, KERNEL_NAME_MAX_CHARS);
    const ImVec2      name_pos(text_x, y);
    draw_list->AddText(name_pos, settings.GetColor(Colors::kTextMain), name.c_str());
    y += line_h + KERNEL_TAB_LINE_GAP;
    draw_list->AddText(ImVec2(text_x, y), settings.GetColor(Colors::kTextDim), stats);
    y += line_h + KERNEL_TAB_LINE_GAP;

    // Share of the workload's total kernel time.
    draw_list->AddRectFilled(
        ImVec2(text_x, y), ImVec2(text_x + text_w, y + KERNEL_TAB_BAR_HEIGHT),
        ApplyAlpha(settings.GetColor(Colors::kBorderColor), KERNEL_TAB_TRACK_ALPHA),
        KERNEL_TAB_BAR_HEIGHT * 0.5f);
    draw_list->AddRectFilled(ImVec2(text_x, y),
                             ImVec2(text_x + text_w * share, y + KERNEL_TAB_BAR_HEIGHT),
                             settings.GetColor(Colors::kAccent),
                             KERNEL_TAB_BAR_HEIGHT * 0.5f);

    if(body_hovered && IsEllipsisHovered(name, kernel.name, name_pos, line_h))
    {
        KernelNameTooltip(kernel.name);
    }
    else if(slot_hovered && (is_a || is_b || can_be_b))
    {
        SetTooltipStyled("%s", is_a   ? "A: the selected kernel"
                               : is_b ? "B: compared with A. Click to stop comparing."
                                      : "Compare with this kernel (B)");
    }
}

bool
ComputeKernelDetailsView::CanCompare() const
{
    size_t kernels = 0;
    for(const WorkloadInfo* workload : m_data_provider.ComputeModel().GetWorkloadList())
    {
        kernels += workload ? workload->kernels.size() : 0;
    }
    return kernels > 1;
}

std::vector<const KernelInfo*>
ComputeKernelDetailsView::KernelsByDuration(uint32_t workload_id) const
{
    std::vector<const KernelInfo*> kernels =
        m_data_provider.ComputeModel().GetKernelInfoList(workload_id);
    std::stable_sort(kernels.begin(), kernels.end(),
                     [](const KernelInfo* a, const KernelInfo* b) {
                         return a->dispatch_metrics[KernelInfo::DurationTotal] >
                                b->dispatch_metrics[KernelInfo::DurationTotal];
                     });
    return kernels;
}

const KernelInfo*
ComputeKernelDetailsView::FindKernelByName(uint32_t workload_id, const std::string& name) const
{
    for(const KernelInfo* kernel : m_data_provider.ComputeModel().GetKernelInfoList(workload_id))
    {
        if(kernel && kernel->name == name)
        {
            return kernel;
        }
    }
    return nullptr;
}

bool
ComputeKernelDetailsView::IsComparing() const
{
    return m_compare.enabled;
}

uint32_t
ComputeKernelDetailsView::CompareWorkloadId() const
{
    return m_compare.workload_id;
}

uint32_t
ComputeKernelDetailsView::CompareKernelId() const
{
    return m_compare.kernel_id;
}

bool
ComputeKernelDetailsView::DeltaAvailable() const
{
    if(!m_compare.enabled)
    {
        return false;
    }
    const uint32_t a_workload = m_compute_selection->GetSelectedWorkload();
    if(a_workload == m_compare.workload_id)
    {
        return true;
    }
    // B - A reads B's values through A's chart, so both need the same layout.
    const ComputeDataModel& model = m_data_provider.ComputeModel();
    return ComputeMemoryChartView::WorkloadArch(model.GetWorkload(a_workload)) ==
           ComputeMemoryChartView::WorkloadArch(model.GetWorkload(m_compare.workload_id));
}

void
ComputeKernelDetailsView::EnableCompare()
{
    // B is set (SetCompareTarget) before comparing starts.
    if(m_compare.enabled ||
       !m_data_provider.ComputeModel().GetKernelInfo(m_compare.workload_id,
                                                     m_compare.kernel_id))
    {
        return;
    }
    const uint32_t a_workload = m_compute_selection->GetSelectedWorkload();
    const uint32_t a_kernel   = m_compute_selection->GetSelectedKernel();
    m_compare.enabled              = true;
    m_compare.baseline_workload_id = a_workload;
    m_compare.baseline_kernel_id   = a_kernel;
    for(const std::unique_ptr<ComputeKernelDetailsPane>& pane : m_panes)
    {
        pane->OnCompareStarted();
    }
    ApplyCompare(true);
}

void
ComputeKernelDetailsView::DisableCompare()
{
    if(!m_compare.enabled)
    {
        return;
    }
    m_compare.enabled = false;
    ApplyCompare(false);
}

void
ComputeKernelDetailsView::SetCompareTarget(uint32_t workload_id, uint32_t kernel_id)
{
    // A kernel is not compared with itself.
    if(workload_id == m_compute_selection->GetSelectedWorkload() &&
       kernel_id == m_compute_selection->GetSelectedKernel())
    {
        return;
    }
    const bool changed = workload_id != m_compare.workload_id || kernel_id != m_compare.kernel_id;
    m_compare.workload_id = workload_id;
    m_compare.kernel_id   = kernel_id;
    if(!m_compare.enabled)
    {
        EnableCompare();
    }
    else if(changed)
    {
        ApplyCompare(true);
    }
}

void
ComputeKernelDetailsView::SwapCompare()
{
    const uint32_t a_workload = m_compute_selection->GetSelectedWorkload();
    const uint32_t a_kernel   = m_compute_selection->GetSelectedKernel();
    const uint32_t b_workload = m_compare.workload_id;
    const uint32_t b_kernel   = m_compare.kernel_id;
    if(!m_data_provider.ComputeModel().GetKernelInfo(b_workload, b_kernel))
    {
        return;
    }
    m_compare.workload_id = a_workload;
    m_compare.kernel_id   = a_kernel;
    // Recorded as A already, so the selection notifications below do not re-run
    // the follow rule against the new B.
    m_compare.baseline_workload_id = b_workload;
    m_compare.baseline_kernel_id   = b_kernel;
    for(const std::unique_ptr<ComputeKernelDetailsPane>& pane : m_panes)
    {
        pane->OnCompareSwapping();
    }
    m_compute_selection->Select(b_workload, b_kernel);
    ApplyCompare(true);
}

void
ComputeKernelDetailsView::OnBaselineChanged()
{
    const uint32_t a_workload = m_compute_selection->GetSelectedWorkload();
    const uint32_t a_kernel   = m_compute_selection->GetSelectedKernel();
    if(a_workload == m_compare.baseline_workload_id && a_kernel == m_compare.baseline_kernel_id)
    {
        return;
    }
    const uint32_t previous_workload = m_compare.baseline_workload_id;
    const uint32_t previous_kernel   = m_compare.baseline_kernel_id;
    m_compare.baseline_workload_id   = a_workload;
    m_compare.baseline_kernel_id     = a_kernel;
    if(!m_compare.enabled)
    {
        return;
    }

    const ComputeDataModel& model          = m_data_provider.ComputeModel();
    bool                    target_changed = false;
    if(a_workload == m_compare.workload_id && a_kernel == m_compare.kernel_id)
    {
        // A moved onto B: they trade places rather than compare a kernel with itself.
        if(model.GetKernelInfo(previous_workload, previous_kernel))
        {
            m_compare.workload_id = previous_workload;
            m_compare.kernel_id   = previous_kernel;
            target_changed        = true;
        }
    }
    else if(m_compare.follow_by_name && a_workload != m_compare.workload_id)
    {
        const KernelInfo* kernel = model.GetKernelInfo(a_workload, a_kernel);
        const KernelInfo* match =
            kernel ? FindKernelByName(m_compare.workload_id, kernel->name) : nullptr;
        if(match && match->id != m_compare.kernel_id)
        {
            m_compare.kernel_id = match->id;
            target_changed      = true;
        }
    }
    // A's workload may have changed architecture, so B - A is re-checked too.
    ApplyCompare(target_changed);
}

void
ComputeKernelDetailsView::ApplyCompare(bool target_changed)
{
    if(m_compare.enabled)
    {
        m_kernel_metric_table->SetMarkedKernel(m_compare.workload_id,
                                               m_compare.kernel_id);
    }
    else
    {
        m_kernel_metric_table->SetMarkedKernel(ComputeSelection::INVALID_SELECTION_ID,
                                               ComputeSelection::INVALID_SELECTION_ID);
    }
    for(const std::unique_ptr<ComputeKernelDetailsPane>& pane : m_panes)
    {
        pane->ApplyCompare(target_changed);
    }
}

void
ComputeKernelDetailsView::RenderCompareCard()
{
    SettingsManager&        settings      = SettingsManager::GetInstance();
    const ImGuiStyle&       style         = settings.GetDefaultStyle();
    const ComputeDataModel& model         = m_data_provider.ComputeModel();
    const uint32_t          a_workload_id = m_compute_selection->GetSelectedWorkload();
    const KernelInfo*       a_kernel =
        model.GetKernelInfo(a_workload_id, m_compute_selection->GetSelectedKernel());
    const WorkloadInfo* a_workload = model.GetWorkload(a_workload_id);
    const WorkloadInfo* b_workload = model.GetWorkload(m_compare.workload_id);
    const KernelInfo*   b_kernel = model.GetKernelInfo(m_compare.workload_id, m_compare.kernel_id);
    const bool          several_workloads = model.GetWorkloadList().size() > 1;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgFrame));
    ImGui::PushStyleColor(ImGuiCol_Border, settings.GetColor(Colors::kBorderColor));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, style.ChildRounding);
    ImGui::BeginChild("compare_card", ImVec2(0, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY |
                          ImGuiChildFlags_AlwaysUseWindowPadding);

    // A and B at the card's full width, each with its workload directly under the
    // kernel name when there are several. Both are picked in the list below.
    auto side_row = [&](const char* letter, Colors color, const char* tooltip,
                        const KernelInfo* kernel, const WorkloadInfo* workload,
                        const char* missing) {
        const bool workload_line = several_workloads && workload != nullptr;
        Badge(letter, color, tooltip);
        ImGui::SameLine();
        const float text_x = ImGui::GetCursorPosX();
        if(workload_line)
        {
            ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 0.0f);
        }
        ImGui::AlignTextToFramePadding();
        std::string name;
        if(kernel)
        {
            name = ElideWithEllipsis(kernel->name, ImGui::GetContentRegionAvail().x,
                                     KERNEL_NAME_MAX_CHARS);
            ImGui::TextUnformatted(name.c_str());
        }
        else
        {
            ImGui::TextDisabled("%s", missing);
        }
        if(workload_line)
        {
            ImGui::PopStyleVar();
        }
        if(kernel && ImGui::IsItemHovered() &&
           IsEllipsisHovered(name, kernel->name, ImGui::GetItemRectMin(),
                             ImGui::GetItemRectSize().y))
        {
            KernelNameTooltip(kernel->name);
        }
        if(workload_line)
        {
            ImGui::SetCursorPosX(text_x);
            ImGui::TextDisabled("%s", workload->name.c_str());
        }
    };
    side_row("A", Colors::kComparisonBase, "A: the selected kernel", a_kernel, a_workload,
             "No kernel selected");
    side_row("B", Colors::kComparisonTarget, "B: compared with A", b_kernel, b_workload,
             "Pick B with the B beside a kernel");

    const bool  follow         = m_compare.follow_by_name;
    const char* follow_tooltip =
        follow ? "Follow A: on. Selecting another A moves B to the kernel of the same\n"
                 "name in B's workload (when B's workload is not A's)."
               : "Follow A: off. B stays put when A changes.";
    const ImU32 follow_bg =
        settings.GetColor(follow ? Colors::kButton : Colors::kTransparent);
    if(FramedIconButton(ICON_CHAIN, follow_tooltip, follow_bg))
    {
        m_compare.follow_by_name = !follow;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!b_kernel);
    if(FramedIconButton(ICON_ARROWS_CYCLE, "Swap A and B (B becomes the selected kernel)",
                        settings.GetColor(Colors::kTransparent)))
    {
        SwapCompare();
    }
    ImGui::EndDisabled();

    // Architecture: B - A and like-for-like metrics need A and B on the same GPU
    // (what DeltaAvailable() checks).
    const std::string a_arch = ComputeMemoryChartView::WorkloadArch(a_workload);
    const std::string b_arch = ComputeMemoryChartView::WorkloadArch(b_workload);
    if(!a_arch.empty() || !b_arch.empty())
    {
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        if(a_arch == b_arch)
        {
            ImGui::TextDisabled("%s", a_arch.c_str());
        }
        else
        {
            ImGui::TextColored(
                ImGui::ColorConvertU32ToFloat4(settings.GetColor(Colors::kTextWarning)),
                "%s vs %s", a_arch.empty() ? "?" : a_arch.c_str(),
                b_arch.empty() ? "?" : b_arch.c_str());
            if(ImGui::IsItemHovered())
            {
                SetTooltipStyled(
                    "A and B ran on different GPU architectures: the memory chart shows\n"
                    "A or B (no B - A), and some metrics exist on one side only.");
            }
        }
    }

    // Stop comparing, right-aligned on the controls' row.
    SameLineRightAligned(IconWidth(ICON_X_CIRCLED) + ImGui::GetStyle().FramePadding.x * 2.0f);
    if(FramedIconButton(ICON_X_CIRCLED, "Stop comparing",
                        settings.GetColor(Colors::kTransparent)))
    {
        DisableCompare();
    }

    if(b_kernel && m_compare.follow_by_name && a_kernel &&
       m_compare.workload_id != a_workload_id && b_kernel->name != a_kernel->name)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("No kernel named like A in B's workload.");
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
    ImGui::Spacing();
}

void
ComputeKernelDetailsView::Render()
{
    const LayoutState state = CurrentLayoutState();
    if(!m_layout_built || state.layout != m_built_layout.layout ||
       state.maximized != m_built_layout.maximized ||
       state.kernel_rail != m_built_layout.kernel_rail)
    {
        RebuildLayout(state);
    }
    UpdatePanesShown();

    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, WORKSPACE_SCROLLBAR_SIZE);
    ImGui::BeginChild("kernel_details", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    UpdatePaneMinSizes();
    // Splits size their children from the space left, so no spacing may be added
    // between a child and its gutter.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    m_root_split->Render();
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

ComputeKernelDetailsView::Preset::Preset(ComputeKernelDetailsView& widget)
: PresetComponent(PresetManager::ComputeKernelDetailsLayout,
                  widget.m_data_provider.GetTraceFilePath())
, m_widget(widget)
{}

bool
ComputeKernelDetailsView::Preset::ToJson(jt::Json& json)
{
    const LayoutSettings& layout   = m_widget.m_layout;
    json[JSON_KEY_LAYOUT_TEMPLATE] = layout.layout_template;
    for(int32_t slot = 0; slot < COMPUTE_LAYOUT_MAX_SLOTS; ++slot)
    {
        json[JSON_KEY_LAYOUT_SLOTS][static_cast<size_t>(slot)] = layout.slots[slot];
    }
    json[JSON_KEY_LAYOUT_MAXIMIZED]         = layout.maximized;
    json[JSON_KEY_LAYOUT_MAXIMIZED_PANE]    = layout.maximized_pane;
    json[JSON_KEY_LAYOUT_KERNEL_LIST]       = layout.show_kernel_list;
    json[JSON_KEY_LAYOUT_KERNEL_LIST_TABLE] = layout.kernel_list_table;
    json[JSON_KEY_COMPARE_FOLLOW_BY_NAME]   = m_widget.m_compare.follow_by_name;
    for(const std::unique_ptr<ComputeKernelDetailsPane>& pane : m_widget.m_panes)
    {
        pane->SaveLayout(json);
    }
    return true;
}

bool
ComputeKernelDetailsView::Preset::FromJson(jt::Json& json)
{
    if(!json.isObject())
    {
        return false;
    }
    LayoutSettings& layout = m_widget.m_layout;
    bool            result = true;
    auto read_bool = [&json, &result](const char* key, bool& out) {
        if(json.contains(key))
        {
            if(json[key].isBool())
            {
                out = json[key].getBool();
            }
            else
            {
                result = false;
            }
        }
    };
    auto read_int = [&json, &result](const char* key, int32_t& out) {
        if(json.contains(key))
        {
            if(json[key].isLong())
            {
                out = static_cast<int32_t>(json[key].getLong());
            }
            else
            {
                result = false;
            }
        }
    };

    read_int(JSON_KEY_LAYOUT_TEMPLATE, layout.layout_template);
    if(json.contains(JSON_KEY_LAYOUT_SLOTS) && json[JSON_KEY_LAYOUT_SLOTS].isArray())
    {
        std::vector<jt::Json>& slots = json[JSON_KEY_LAYOUT_SLOTS].getArray();
        const size_t count = std::min(slots.size(), static_cast<size_t>(COMPUTE_LAYOUT_MAX_SLOTS));
        for(size_t i = 0; i < count; ++i)
        {
            if(slots[i].isLong())
            {
                layout.slots[i] = static_cast<int32_t>(slots[i].getLong());
            }
        }
    }
    read_bool(JSON_KEY_LAYOUT_MAXIMIZED, layout.maximized);
    read_int(JSON_KEY_LAYOUT_MAXIMIZED_PANE, layout.maximized_pane);
    read_bool(JSON_KEY_LAYOUT_KERNEL_LIST, layout.show_kernel_list);
    bool table = layout.kernel_list_table;
    read_bool(JSON_KEY_LAYOUT_KERNEL_LIST_TABLE, table);
    m_widget.SetKernelRailTableMode(table);
    read_bool(JSON_KEY_COMPARE_FOLLOW_BY_NAME, m_widget.m_compare.follow_by_name);
    for(const std::unique_ptr<ComputeKernelDetailsPane>& pane : m_widget.m_panes)
    {
        if(!pane->LoadLayout(json))
        {
            result = false;
        }
    }
    return result;
}

void
ComputeKernelDetailsView::Preset::Reset()
{
    m_widget.ResetLayout();
    m_widget.m_compare.follow_by_name = true;
}

}  // namespace View
}  // namespace RocProfVis
