// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_kernel_details.h"
#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_compute_isa_view.h"
#include "rocprofvis_compute_kernel_metric_table.h"
#include "rocprofvis_compute_roofline.h"
#include "rocprofvis_compute_selection.h"
#include "rocprofvis_compute_table_view.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "widgets/rocprofvis_gui_helpers.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cctype>
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
constexpr float THIRD            = 1.0f / 3.0f;
constexpr float TWO_THIRDS       = 2.0f / 3.0f;
constexpr float RAIL_LIST_RATIO  = 0.17f;  // kernel tabs | workspace
constexpr float RAIL_TABLE_RATIO = 0.45f;  // kernel table | workspace

// Smallest pane sizes, in frame heights / font sizes so they scale with the UI font.
constexpr float PANE_MIN_FRAMES          = 5.0f;
constexpr float PANE_MIN_WIDTH_EMS       = 12.0f;
constexpr float WORKSPACE_MIN_WIDTH_EMS  = 20.0f;
constexpr float RAIL_LIST_MIN_WIDTH_EMS  = 11.0f;
constexpr float RAIL_TABLE_MIN_WIDTH_EMS = 24.0f;

// Toolbar combos, in frame heights, and the Layout menu's row / thumbnail sizes.
constexpr float LAYOUT_COMBO_WIDTH_FRAMES = 11.0f;
constexpr float PANELS_COMBO_WIDTH_FRAMES = 9.0f;
constexpr float LAYOUT_ROW_HEIGHT_FRAMES  = 1.35f;
constexpr float THUMBNAIL_ASPECT          = 1.6f;
constexpr float THUMBNAIL_INSET           = 3.0f;
constexpr float THUMBNAIL_GAP             = 1.0f;
constexpr float THUMBNAIL_ROUNDING        = 1.5f;
constexpr float THUMBNAIL_FILL_ALPHA      = 0.3f;

// Box title handle and drop feedback.
constexpr const char* PANE_DRAG_PAYLOAD      = "COMPUTE_PANE_BOX";
constexpr float       TITLE_HANDLE_PAD_X     = 4.0f;
constexpr float       DROP_FILL_ALPHA        = 0.14f;
constexpr float       DROP_BORDER_THICKNESS  = 2.0f;
constexpr float       DROP_LABEL_PAD         = 6.0f;
constexpr int32_t     MAXIMIZED_SLOT         = -1;
constexpr int32_t     EMPTY_SLOT_VALUE       = -1;

// Kernel tab geometry.
constexpr float  KERNEL_TAB_PAD          = 6.0f;
constexpr float  KERNEL_TAB_LINE_GAP     = 2.0f;
constexpr float  KERNEL_TAB_BAR_HEIGHT   = 3.0f;
constexpr float  KERNEL_TAB_ACCENT_WIDTH = 3.0f;
constexpr float  KERNEL_TAB_ROUNDING     = 4.0f;
constexpr float  KERNEL_TAB_TRACK_ALPHA  = 0.6f;
constexpr float  KERNEL_TOOLTIP_WIDTH    = 480.0f;
constexpr size_t KERNEL_NAME_MAX_CHARS   = 512;

constexpr const char* JSON_KEY_LAYOUT_TEMPLATE          = "template";
constexpr const char* JSON_KEY_LAYOUT_SLOTS             = "slots";
constexpr const char* JSON_KEY_LAYOUT_MAXIMIZED         = "maximized";
constexpr const char* JSON_KEY_LAYOUT_MAXIMIZED_PANE    = "maximized_pane";
constexpr const char* JSON_KEY_LAYOUT_KERNEL_LIST       = "kernel_list";
constexpr const char* JSON_KEY_LAYOUT_KERNEL_LIST_TABLE = "kernel_list_table";
constexpr const char* JSON_KEY_LAYOUT_FIT_MEMORY_CHART  = "fit_memory_chart";
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
        { 0, MAIN_SPLIT_RATIO, 0.5f, 1 },
        { 0.5f, MAIN_SPLIT_RATIO, 1, 1 } } },
    { "Single", 1, { { 0, 0, 1, 1 } } },
    { "Side by Side", 2, { { 0, 0, 0.5f, 1 }, { 0.5f, 0, 1, 1 } } },
    { "Stacked", 2, { { 0, 0, 1, 0.5f }, { 0, 0.5f, 1, 1 } } },
    { "Main + Two Right", 3,
      { { 0, 0, MAIN_SPLIT_RATIO, 1 },
        { MAIN_SPLIT_RATIO, 0, 1, 0.5f },
        { MAIN_SPLIT_RATIO, 0.5f, 1, 1 } } },
    { "Three Columns", 3,
      { { 0, 0, THIRD, 1 }, { THIRD, 0, TWO_THIRDS, 1 }, { TWO_THIRDS, 0, 1, 1 } } },
    { "Main + Three Below", 4,
      { { 0, 0, 1, MAIN_SPLIT_RATIO },
        { 0, MAIN_SPLIT_RATIO, THIRD, 1 },
        { THIRD, MAIN_SPLIT_RATIO, TWO_THIRDS, 1 },
        { TWO_THIRDS, MAIN_SPLIT_RATIO, 1, 1 } } },
    { "2 x 2 Grid", 4,
      { { 0, 0, 0.5f, 0.5f }, { 0.5f, 0, 1, 0.5f }, { 0, 0.5f, 0.5f, 1 }, { 0.5f, 0.5f, 1, 1 } } },
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
ContainsIgnoreCase(const std::string& text, const std::string& needle)
{
    if(needle.empty())
    {
        return true;
    }
    std::string::const_iterator it =
        std::search(text.begin(), text.end(), needle.begin(), needle.end(),
                    [](char a, char b) {
                        return std::tolower(static_cast<unsigned char>(a)) ==
                               std::tolower(static_cast<unsigned char>(b));
                    });
    return it != text.end();
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

TabItem
ComputeKernelDetailsView::CreateTabItem(
    DataProvider&                            data_provider,
    const std::shared_ptr<ComputeSelection>& compute_selection,
    bool has_available_metrics, bool has_isa_lines)
{
    return RocWidget::CreateTabItem(
        "Kernel Details", TAB_ID,
        std::make_shared<ComputeKernelDetailsView>(data_provider, compute_selection,
                                                   has_available_metrics, has_isa_lines));
}

ComputeKernelDetailsView::ComputeKernelDetailsView(
    DataProvider& data_provider, std::shared_ptr<ComputeSelection> compute_selection,
    bool has_available_metrics, bool has_isa_lines)
: RocWidget()
, m_data_provider(data_provider)
, m_memory_chart(data_provider, compute_selection)
, m_compute_selection(compute_selection)
, m_roofline(nullptr)
, m_kernel_metric_table(nullptr)
, m_table_view(nullptr)
, m_isa_view(nullptr)
, m_client_id(IdGenerator::GetInstance().GenerateId())
, m_kernel_list_workload(ComputeSelection::INVALID_SELECTION_ID)
, m_kernel_list_total_duration(0.0f)
, m_kernel_list_selection(ComputeSelection::INVALID_SELECTION_ID)
, m_preset(nullptr)
, m_workload_selection_changed_token(EventManager::InvalidSubscriptionToken)
, m_kernel_selection_changed_token(EventManager::InvalidSubscriptionToken)
, m_metrics_fetched_token(EventManager::InvalidSubscriptionToken)
, m_new_table_data_token(EventManager::InvalidSubscriptionToken)
, m_send_metric_to_kernel_details_token(EventManager::InvalidSubscriptionToken)
{
    SubscribeToEvents();

    // Panes draw their own header (title + buttons), so the widgets skip theirs.
    m_roofline =
        std::make_shared<RocProfVis::View::Roofline>(data_provider, Roofline::SingleKernel);
    m_roofline->SetChromeless(true);
    m_kernel_metric_table =
        std::make_shared<RocProfVis::View::KernelMetricTable>(data_provider,
                                                              compute_selection);
    m_kernel_metric_table->SetFillParent(true);
    m_kernel_metric_table->SetChromeless(true);
    m_table_view = std::make_shared<ComputeTableView>(data_provider, compute_selection,
                                                      has_available_metrics);
    m_table_view->SetChromeless(true);
    if(has_isa_lines)
    {
        m_isa_view = std::make_shared<ComputeIsaView>(data_provider);
    }

    CreatePanes();
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
ComputeKernelDetailsView::CreatePanes()
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
    m_root_split->SetSplit(
        SettingsManager::GetInstance().GetAppWindowSettings().compute_kernel_list_table
            ? RAIL_TABLE_RATIO
            : RAIL_LIST_RATIO);
    m_root_split->SetSplitterSize(PANE_GUTTER);
}

void
ComputeKernelDetailsView::SubscribeToEvents()
{
    auto workload_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<ComputeSelectionChangedEvent>(e);
        if(evt && evt->GetSourceId() == m_data_provider.GetTraceFilePath())
        {
            m_memory_chart.LoadWorkloadLayout(evt->GetId());
            m_kernel_list_workload = ComputeSelection::INVALID_SELECTION_ID;
            if(m_kernel_metric_table)
            {
                m_data_provider.ComputeModel().GetKernelSelectionTable().Clear();
                m_kernel_metric_table->FetchData(evt->GetId());
            }
            if(m_roofline)
            {
                m_roofline->SetWorkload(evt->GetId());
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
            m_memory_chart.FetchMemChartMetrics();
            if(m_roofline)
            {
                m_roofline->SetKernel(evt->GetId());
            }
        }
    };

    m_kernel_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeKernelSelectionChanged),
        kernel_changed_handler);

    auto metrics_fetched_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<ComputeMetricsFetchedEvent>(e);
        if(evt && evt->GetSourceId() == m_data_provider.GetTraceFilePath())
        {
            if(m_memory_chart.GetClientId() == evt->GetClientId())
            {
                m_memory_chart.UpdateMetrics();
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

            if(table_data_event->GetResponseCode() != kRocProfVisResultSuccess)
            {
                return;
            }

            if(table_data_event->GetRequestID() == DataProvider::METRIC_PIVOT_TABLE_REQUEST_ID)
            {
                m_kernel_metric_table->HandleNewData();
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
    if(m_roofline)
    {
        m_roofline->Update();
    }
    if(m_kernel_metric_table)
    {
        m_kernel_metric_table->Update();
    }
    if(m_table_view)
    {
        m_table_view->Update();
    }
    if(m_isa_view)
    {
        m_isa_view->Update();
    }
}

bool
ComputeKernelDetailsView::IsPaneAvailable(ComputePane pane) const
{
    return pane != ComputePane::kCount && (pane != ComputePane::kIsa || m_isa_view != nullptr);
}

bool&
ComputeKernelDetailsView::PaneVisibleSetting(ComputePane pane) const
{
    AppWindowSettings& settings = SettingsManager::GetInstance().GetAppWindowSettings();
    switch(pane)
    {
        case ComputePane::kRoofline: return settings.show_compute_roofline;
        case ComputePane::kMetricTables: return settings.show_compute_metric_tables;
        case ComputePane::kIsa: return settings.show_compute_isa;
        case ComputePane::kMemoryChart:
        case ComputePane::kCount: break;
    }
    return settings.show_compute_memory_chart;
}

const char*
ComputeKernelDetailsView::PaneTitle(ComputePane pane) const
{
    switch(pane)
    {
        case ComputePane::kRoofline: return "Roofline";
        case ComputePane::kMetricTables: return "Metric Tables";
        case ComputePane::kIsa: return "ISA";
        case ComputePane::kMemoryChart:
        case ComputePane::kCount: break;
    }
    return "Memory Chart";
}

ComputeLayoutTemplate
ComputeKernelDetailsView::CurrentTemplate() const
{
    const int32_t value =
        SettingsManager::GetInstance().GetAppWindowSettings().compute_layout_template;
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
    ComputePane pane = ComputePane::kCount;
    if(slot >= 0 && slot < COMPUTE_LAYOUT_MAX_SLOTS)
    {
        const int32_t value =
            SettingsManager::GetInstance().GetAppWindowSettings().compute_layout_slots[slot];
        if(value >= 0 && value < static_cast<int32_t>(ComputePane::kCount) &&
           IsPaneAvailable(PaneAt(value)))
        {
            pane = PaneAt(value);
        }
    }
    return pane;
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
    const AppWindowSettings& settings = SettingsManager::GetInstance().GetAppWindowSettings();
    const int32_t            value    = settings.compute_maximized_pane;
    if(settings.compute_maximized && value >= 0 &&
       value < static_cast<int32_t>(ComputePane::kCount) && IsPaneAvailable(PaneAt(value)))
    {
        return PaneAt(value);
    }
    return ComputePane::kCount;
}

void
ComputeKernelDetailsView::AssignSlot(int32_t slot, ComputePane pane)
{
    if(slot < 0 || slot >= COMPUTE_LAYOUT_MAX_SLOTS)
    {
        return;
    }
    // A panel lives in one box at most: taking it from another box swaps the two.
    int32_t*      slots = SettingsManager::GetInstance().GetAppWindowSettings().compute_layout_slots;
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
    if(a >= 0 && a < COMPUTE_LAYOUT_MAX_SLOTS && b >= 0 && b < COMPUTE_LAYOUT_MAX_SLOTS &&
       a != b)
    {
        int32_t* slots =
            SettingsManager::GetInstance().GetAppWindowSettings().compute_layout_slots;
        std::swap(slots[a], slots[b]);
    }
}

void
ComputeKernelDetailsView::ClearSlot(int32_t slot)
{
    if(slot < 0 || slot >= COMPUTE_LAYOUT_MAX_SLOTS)
    {
        return;
    }
    AppWindowSettings& settings = SettingsManager::GetInstance().GetAppWindowSettings();
    if(settings.compute_maximized &&
       settings.compute_maximized_pane == settings.compute_layout_slots[slot])
    {
        settings.compute_maximized = false;
    }
    settings.compute_layout_slots[slot] = EMPTY_SLOT_VALUE;
}

void
ComputeKernelDetailsView::PlacePane(ComputePane pane)
{
    // First empty box; with every box in use, the panel takes the last one.
    const int32_t count = ActiveSlotCount();
    for(int32_t slot = 0; slot < count; ++slot)
    {
        if(SlotPane(slot) == ComputePane::kCount)
        {
            AssignSlot(slot, pane);
            return;
        }
    }
    AssignSlot(count - 1, pane);
}

void
ComputeKernelDetailsView::SetTemplate(ComputeLayoutTemplate layout)
{
    AppWindowSettings& settings      = SettingsManager::GetInstance().GetAppWindowSettings();
    settings.compute_layout_template = static_cast<int32_t>(layout);
    settings.compute_maximized       = false;
    // Boxes keep their panels in order; empty boxes take panels not yet shown, so
    // a larger arrangement starts full.
    const int32_t count = ActiveSlotCount();
    for(int32_t slot = 0; slot < count; ++slot)
    {
        if(SlotPane(slot) != ComputePane::kCount)
        {
            continue;
        }
        for(int32_t i = 0; i < static_cast<int32_t>(ComputePane::kCount); ++i)
        {
            if(IsPaneAvailable(PaneAt(i)) && ActiveSlotOf(PaneAt(i)) < 0)
            {
                AssignSlot(slot, PaneAt(i));
                break;
            }
        }
    }
}

void
ComputeKernelDetailsView::ResetLayout()
{
    AppWindowSettings& settings = SettingsManager::GetInstance().GetAppWindowSettings();
    settings.compute_layout_template = static_cast<int32_t>(ComputeLayoutTemplate::kMainBelow);
    for(int32_t slot = 0; slot < COMPUTE_LAYOUT_MAX_SLOTS; ++slot)
    {
        settings.compute_layout_slots[slot] =
            slot < static_cast<int32_t>(ComputePane::kCount) ? slot : EMPTY_SLOT_VALUE;
    }
    settings.compute_maximized        = false;
    settings.show_compute_kernel_list = true;
    SetKernelRailTableMode(false);
    m_memory_chart.SetFitToView(false);
}

void
ComputeKernelDetailsView::SyncPanelToggles()
{
    for(int32_t i = 0; i < static_cast<int32_t>(ComputePane::kCount); ++i)
    {
        const ComputePane pane = PaneAt(i);
        if(!IsPaneAvailable(pane))
        {
            continue;
        }
        const bool toggle = PaneVisibleSetting(pane);
        if(m_toggles_synced && toggle != m_synced_toggles[static_cast<size_t>(i)])
        {
            const int32_t slot = ActiveSlotOf(pane);
            if(toggle && slot < 0)
            {
                PlacePane(pane);
            }
            else if(!toggle && slot >= 0)
            {
                ClearSlot(slot);
            }
        }
    }
    for(int32_t i = 0; i < static_cast<int32_t>(ComputePane::kCount); ++i)
    {
        const ComputePane pane = PaneAt(i);
        if(IsPaneAvailable(pane))
        {
            const bool shown                           = ActiveSlotOf(pane) >= 0;
            PaneVisibleSetting(pane)                   = shown;
            m_synced_toggles[static_cast<size_t>(i)] = shown;
        }
    }
    m_toggles_synced = true;
}

ComputeKernelDetailsView::LayoutState
ComputeKernelDetailsView::CurrentLayoutState() const
{
    LayoutState state;
    state.layout      = CurrentTemplate();
    state.maximized   = MaximizedPane() != ComputePane::kCount;
    state.kernel_rail = SettingsManager::GetInstance().GetAppWindowSettings().show_compute_kernel_list;
    return state;
}

void
ComputeKernelDetailsView::RebuildLayout(const LayoutState& state)
{
    m_hsplits.clear();
    m_vsplits.clear();
    m_root_split->SetRight(state.maximized ? m_maximized_pane : BuildTemplate(state.layout));
    // Maximize fills the whole tab, so the kernel rail steps aside too (the
    // toolbar's Kernel combo still switches kernels).
    m_rail_pane->m_visible = state.kernel_rail && !state.maximized;
    m_built_layout         = state;
    m_layout_built         = true;
}

LayoutItem::Ptr
ComputeKernelDetailsView::HSplitPane(const LayoutItem::Ptr& left, const LayoutItem::Ptr& right,
                                     float ratio)
{
    std::shared_ptr<HSplitContainer> split = std::make_shared<HSplitContainer>(left, right);
    split->SetSplit(ratio);
    split->SetSplitterSize(PANE_GUTTER);
    m_hsplits.push_back(split);
    return MakeSplitPane(split);
}

LayoutItem::Ptr
ComputeKernelDetailsView::VSplitPane(const LayoutItem::Ptr& top, const LayoutItem::Ptr& bottom,
                                     float ratio)
{
    std::shared_ptr<VSplitContainer> split = std::make_shared<VSplitContainer>(top, bottom);
    split->SetSplit(ratio);
    split->SetSplitterSize(PANE_GUTTER);
    m_vsplits.push_back(split);
    return MakeSplitPane(split);
}

LayoutItem::Ptr
ComputeKernelDetailsView::BuildTemplate(ComputeLayoutTemplate layout)
{
    const std::array<LayoutItem::Ptr, COMPUTE_LAYOUT_MAX_SLOTS>& box = m_slot_panes;
    switch(layout)
    {
        case ComputeLayoutTemplate::kSingle: return box[0];
        case ComputeLayoutTemplate::kSideBySide: return HSplitPane(box[0], box[1], 0.5f);
        case ComputeLayoutTemplate::kStacked: return VSplitPane(box[0], box[1], 0.5f);
        case ComputeLayoutTemplate::kMainRight:
            return HSplitPane(box[0], VSplitPane(box[1], box[2], 0.5f), MAIN_SPLIT_RATIO);
        case ComputeLayoutTemplate::kThreeColumns:
            return HSplitPane(box[0], HSplitPane(box[1], box[2], 0.5f), THIRD);
        case ComputeLayoutTemplate::kMainThreeBelow:
            return VSplitPane(
                box[0], HSplitPane(box[1], HSplitPane(box[2], box[3], 0.5f), THIRD),
                MAIN_SPLIT_RATIO);
        case ComputeLayoutTemplate::kGrid:
            return VSplitPane(HSplitPane(box[0], box[1], 0.5f), HSplitPane(box[2], box[3], 0.5f),
                              0.5f);
        case ComputeLayoutTemplate::kMainBelow:
        case ComputeLayoutTemplate::kCount: break;
    }
    return VSplitPane(box[0], HSplitPane(box[1], box[2], 0.5f), MAIN_SPLIT_RATIO);
}

void
ComputeKernelDetailsView::UpdatePaneMinSizes()
{
    const AppWindowSettings& settings = SettingsManager::GetInstance().GetAppWindowSettings();
    const float              frame_h  = ImGui::GetFrameHeightWithSpacing();
    const float              em       = ImGui::GetFontSize();
    m_root_split->SetMinLeftWidth(em * (settings.compute_kernel_list_table
                                            ? RAIL_TABLE_MIN_WIDTH_EMS
                                            : RAIL_LIST_MIN_WIDTH_EMS));
    m_root_split->SetMinRightWidth(em * WORKSPACE_MIN_WIDTH_EMS);
    for(const std::shared_ptr<HSplitContainer>& split : m_hsplits)
    {
        split->SetMinLeftWidth(em * PANE_MIN_WIDTH_EMS);
        split->SetMinRightWidth(em * PANE_MIN_WIDTH_EMS);
    }
    for(const std::shared_ptr<VSplitContainer>& split : m_vsplits)
    {
        split->SetMinTopHeight(frame_h * PANE_MIN_FRAMES);
        split->SetMinBottomHeight(frame_h * PANE_MIN_FRAMES);
    }
}

void
ComputeKernelDetailsView::SetKernelRailTableMode(bool table)
{
    SettingsManager::GetInstance().GetAppWindowSettings().compute_kernel_list_table = table;
    // The full table needs far more width than the tab list.
    m_root_split->SetSplit(table ? RAIL_TABLE_RATIO : RAIL_LIST_RATIO);
}

void
ComputeKernelDetailsView::RenderToolbar()
{
    RenderLayoutMenu();
    ImGui::SameLine();
    VerticalSeparator();
    RenderPanelsMenu();
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
    if(ImGui::BeginCombo("##compute_layout", "",
                         ImGuiComboFlags_CustomPreview | ImGuiComboFlags_HeightLargest))
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
                ImVec2(thumb_min.x + thumb_w + THUMBNAIL_INSET * 2.0f,
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
    if(ImGui::BeginComboPreview())
    {
        const float  thumb_h = ImGui::GetTextLineHeight();
        const ImVec2 pos     = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(thumb_h * THUMBNAIL_ASPECT, thumb_h));
        DrawLayoutThumbnail(ImGui::GetWindowDrawList(), pos,
                            ImVec2(thumb_h * THUMBNAIL_ASPECT, thumb_h), current, dim);
        ImGui::SameLine();
        ImGui::TextUnformatted(TemplateInfo(current).name);
        ImGui::EndComboPreview();
    }
    PopComboStyles();
    if(ImGui::IsItemHovered())
    {
        SetTooltipStyled("How the workspace is split into boxes.\n"
                         "Drag a box's title onto another box to swap their panels,\n"
                         "or use the arrow next to a title to choose its panel.");
    }
}

void
ComputeKernelDetailsView::RenderPanelsMenu()
{
    AppWindowSettings& settings = SettingsManager::GetInstance().GetAppWindowSettings();
    int32_t            shown    = 0;
    int32_t            total    = 0;
    for(int32_t i = 0; i < static_cast<int32_t>(ComputePane::kCount); ++i)
    {
        if(IsPaneAvailable(PaneAt(i)))
        {
            total++;
            shown += ActiveSlotOf(PaneAt(i)) >= 0 ? 1 : 0;
        }
    }
    const std::string preview =
        std::to_string(shown) + " of " + std::to_string(total) + " panels";

    ImGui::TextUnformatted("Panels:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFrameHeight() * PANELS_COMBO_WIDTH_FRAMES);
    PushComboStyles();
    if(ImGui::BeginCombo("##compute_panels", preview.c_str()))
    {
        ImGui::Checkbox("Kernels", &settings.show_compute_kernel_list);
        ImGui::Separator();
        for(int32_t i = 0; i < static_cast<int32_t>(ComputePane::kCount); ++i)
        {
            const ComputePane pane = PaneAt(i);
            if(IsPaneAvailable(pane))
            {
                // Applied to the boxes by SyncPanelToggles next frame.
                ImGui::Checkbox(PaneTitle(pane), &PaneVisibleSetting(pane));
            }
        }
        ImGui::EndCombo();
    }
    PopComboStyles();
}

int
ComputeKernelDetailsView::RenderPaneHeader(const char* title, int32_t slot, bool draggable,
                                           bool pickable, const HeaderButton* buttons,
                                           int count, bool& title_double_clicked)
{
    SettingsManager&  settings   = SettingsManager::GetInstance();
    const ImGuiStyle& style      = settings.GetDefaultStyle();
    ImFont*           icon_font  = settings.GetFontManager().GetFont(FontType::kIcon);
    ImFont*           title_font = settings.GetFontManager().GetFont(FontType::kDefault);
    const float       title_px   = settings.GetFontManager().GetFontSize(FontSize::kMedLarge);
    const float       button_gap = style.ItemSpacing.x * 0.5f;

    // Title: in a box it is the drag handle (move cursor) - dragging it onto
    // another box swaps their panels.
    ImGui::PushFont(title_font, title_px);
    const ImVec2 text_size = ImGui::CalcTextSize(title);
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
    if(pickable)
    {
        ImGui::SameLine(0.0f, button_gap);
        if(IconButton(ICON_CHEVRON_DOWN, icon_font, ImVec2(0, 0), "Choose this box's panel",
                      false, style.FramePadding, settings.GetColor(Colors::kTransparent),
                      settings.GetColor(Colors::kButtonHovered),
                      settings.GetColor(Colors::kButtonActive)))
        {
            ImGui::OpenPopup("panel_picker");
        }
        RenderPanelPicker(slot);
    }

    int clicked = -1;
    if(count > 0)
    {
        // Right-align the buttons on the title's row.
        float buttons_w = button_gap * static_cast<float>(count - 1);
        for(int i = 0; i < count; ++i)
        {
            if(buttons[i].icon)
            {
                ImGui::PushFont(icon_font, 0.0f);
                buttons_w += ImGui::CalcTextSize(buttons[i].icon).x;
                ImGui::PopFont();
            }
            else
            {
                buttons_w += ImGui::CalcTextSize(buttons[i].label).x;
            }
            buttons_w += style.FramePadding.x * 2.0f;
        }
        ImGui::SameLine();
        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),
                                      ImGui::GetCursorPosX() +
                                          ImGui::GetContentRegionAvail().x - buttons_w));
        for(int i = 0; i < count; ++i)
        {
            if(i > 0)
            {
                ImGui::SameLine(0.0f, button_gap);
            }
            const ImU32 bg =
                settings.GetColor(buttons[i].active ? Colors::kButton : Colors::kTransparent);
            ImGui::PushID(i);
            bool pressed = false;
            if(buttons[i].icon)
            {
                pressed = IconButton(buttons[i].icon, icon_font, ImVec2(0, 0),
                                     buttons[i].tooltip, false, style.FramePadding, bg,
                                     settings.GetColor(Colors::kButtonHovered),
                                     settings.GetColor(Colors::kButtonActive));
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Button, bg);
                pressed = ImGui::Button(buttons[i].label);
                ImGui::PopStyleColor();
                if(buttons[i].tooltip && ImGui::IsItemHovered())
                {
                    SetTooltipStyled("%s", buttons[i].tooltip);
                }
            }
            ImGui::PopID();
            if(pressed)
            {
                clicked = i;
            }
        }
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
    for(int32_t i = 0; i < static_cast<int32_t>(ComputePane::kCount); ++i)
    {
        const ComputePane pane = PaneAt(i);
        if(!IsPaneAvailable(pane))
        {
            continue;
        }
        // A panel shown elsewhere trades places with this box's panel.
        const int32_t where = ActiveSlotOf(pane);
        const char*   hint  = where >= 0 && where != slot ? "swap" : nullptr;
        if(ImGui::MenuItem(PaneTitle(pane), hint, pane == current))
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
    const ImVec2 slot_min = ImGui::GetWindowPos();
    const ImVec2 slot_max(slot_min.x + ImGui::GetWindowWidth(),
                          slot_min.y + ImGui::GetWindowHeight());
    const ComputePane pane = SlotPane(slot);
    if(pane == ComputePane::kCount)
    {
        RenderEmptySlot(slot);
    }
    else
    {
        RenderPane(pane, slot);
    }
    HandleSlotDrop(slot, slot_min, slot_max);
}

void
ComputeKernelDetailsView::HandleSlotDrop(int32_t slot, const ImVec2& slot_min,
                                         const ImVec2& slot_max)
{
    const ImGuiPayload* payload = ImGui::GetDragDropPayload();
    if(!payload || !payload->IsDataType(PANE_DRAG_PAYLOAD))
    {
        return;
    }
    const int32_t source = *static_cast<const int32_t*>(payload->Data);
    if(source != slot && ImGui::IsMouseHoveringRect(slot_min, slot_max, false))
    {
        // Highlight the box under the cursor and say what the drop will do.
        SettingsManager& settings = SettingsManager::GetInstance();
        const ImU32      accent   = settings.GetColor(Colors::kAccent);
        const float      rounding = settings.GetDefaultStyle().ChildRounding;
        ImDrawList*      overlay  = ImGui::GetForegroundDrawList();
        overlay->AddRectFilled(slot_min, slot_max, ApplyAlpha(accent, DROP_FILL_ALPHA), rounding);
        overlay->AddRect(slot_min, slot_max, accent, rounding, 0, DROP_BORDER_THICKNESS);

        const ComputePane target = SlotPane(slot);
        const std::string label  = target == ComputePane::kCount
                                       ? std::string("Move here")
                                       : std::string("Swap with ") + PaneTitle(target);
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
    if(ImGui::BeginDragDropTargetCustom(ImRect(slot_min, slot_max), ImGui::GetID("box_drop")))
    {
        if(const ImGuiPayload* dropped = ImGui::AcceptDragDropPayload(
               PANE_DRAG_PAYLOAD, ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
        {
            SwapSlots(*static_cast<const int32_t*>(dropped->Data), slot);
        }
        ImGui::EndDragDropTarget();
    }
}

void
ComputeKernelDetailsView::RenderPane(ComputePane pane, int32_t slot)
{
    enum class HeaderAction : uint8_t
    {
        kNone,
        kFit,
        kMaximize,
        kRestore,
        kHide,
    };

    AppWindowSettings& settings  = SettingsManager::GetInstance().GetAppWindowSettings();
    const bool         maximized = slot == MAXIMIZED_SLOT;

    HeaderButton buttons[3];
    HeaderAction actions[3];
    int          count = 0;
    // A compact (overview) chart is always fitted, so Fit only applies at size.
    if(pane == ComputePane::kMemoryChart && !m_memory_chart.IsCompact())
    {
        buttons[count] = { nullptr, "Fit",
                           "Shrink the whole chart to fit the box (off: actual size, scroll)",
                           m_memory_chart.GetFitToView() };
        actions[count++] = HeaderAction::kFit;
    }
    if(maximized)
    {
        buttons[count]   = { ICON_ARROWS_SHRINK, nullptr, "Restore the layout", false };
        actions[count++] = HeaderAction::kRestore;
    }
    else
    {
        buttons[count] = { ICON_ARROWS_EXPAND, nullptr,
                           "Maximize: fill the tab, hiding the other boxes and kernels",
                           false };
        actions[count++] = HeaderAction::kMaximize;
    }
    buttons[count]   = { ICON_X_CIRCLED, nullptr,
                         "Hide (empties the box; turn it back on from Panels)", false };
    actions[count++] = HeaderAction::kHide;

    BeginPaneCard("pane_card");
    bool      title_double_clicked = false;
    const int clicked = RenderPaneHeader(PaneTitle(pane), slot, !maximized, !maximized,
                                         buttons, count, title_double_clicked);
    ImGui::BeginChild("pane_body", ImVec2(0, 0));
    RenderPaneBody(pane, slot);
    ImGui::EndChild();
    EndPaneCard();

    // Settings changes take effect as a relayout on the next frame. Double-clicking
    // the title toggles maximize.
    HeaderAction action = clicked >= 0 ? actions[clicked] : HeaderAction::kNone;
    if(title_double_clicked)
    {
        action = maximized ? HeaderAction::kRestore : HeaderAction::kMaximize;
    }
    switch(action)
    {
        case HeaderAction::kMaximize:
            settings.compute_maximized_pane = static_cast<int32_t>(pane);
            settings.compute_maximized      = true;
            break;
        case HeaderAction::kRestore: settings.compute_maximized = false; break;
        case HeaderAction::kHide:
        {
            const int32_t box = ActiveSlotOf(pane);
            if(box >= 0)
            {
                ClearSlot(box);
            }
            settings.compute_maximized = false;
            break;
        }
        case HeaderAction::kFit:
            m_memory_chart.SetFitToView(!m_memory_chart.GetFitToView());
            break;
        case HeaderAction::kNone: break;
    }
}

void
ComputeKernelDetailsView::RenderPaneBody(ComputePane pane, int32_t slot)
{
    switch(pane)
    {
        case ComputePane::kMemoryChart:
            // Maximized it is always at actual size; in a box it falls back to a
            // fitted overview when the box is too small to read it.
            m_memory_chart.SetAutoCompact(slot != MAXIMIZED_SLOT);
            m_memory_chart.Render();
            break;
        case ComputePane::kRoofline: m_roofline->Render(); break;
        case ComputePane::kMetricTables: m_table_view->Render(); break;
        case ComputePane::kIsa:
            if(m_isa_view)
            {
                m_isa_view->Render();
            }
            break;
        case ComputePane::kCount: break;
    }
}

void
ComputeKernelDetailsView::RenderEmptySlot(int32_t slot)
{
    BeginPaneCard("empty_card");
    bool title_double_clicked = false;
    RenderPaneHeader("Empty box", slot, false, true, nullptr, 0, title_double_clicked);

    ImGui::BeginChild("empty_body", ImVec2(0, 0));
    // Offer the panels not shown anywhere as one-click choices, centred.
    std::vector<ComputePane> unshown;
    for(int32_t i = 0; i < static_cast<int32_t>(ComputePane::kCount); ++i)
    {
        if(IsPaneAvailable(PaneAt(i)) && ActiveSlotOf(PaneAt(i)) < 0)
        {
            unshown.push_back(PaneAt(i));
        }
    }
    const ImGuiStyle& style = ImGui::GetStyle();
    const char*       hint  = unshown.empty() ? "Drag a panel's title here to move it."
                                              : "Drag a panel here, or show:";
    ImGui::Dummy(ImVec2(0.0f, ImGui::GetFrameHeight()));
    CenterNextTextItem(hint);
    ImGui::TextDisabled("%s", hint);
    if(!unshown.empty())
    {
        float row_w = style.ItemSpacing.x * static_cast<float>(unshown.size() - 1);
        for(ComputePane pane : unshown)
        {
            row_w += ImGui::CalcTextSize(PaneTitle(pane)).x + style.FramePadding.x * 2.0f;
        }
        CenterNextItem(row_w);
        for(size_t i = 0; i < unshown.size(); ++i)
        {
            if(i > 0)
            {
                ImGui::SameLine();
            }
            if(ImGui::Button(PaneTitle(unshown[i])))
            {
                AssignSlot(slot, unshown[i]);
            }
        }
    }
    ImGui::EndChild();
    EndPaneCard();
}

void
ComputeKernelDetailsView::RenderKernelRail()
{
    AppWindowSettings& settings = SettingsManager::GetInstance().GetAppWindowSettings();
    const bool         table    = settings.compute_kernel_list_table;
    const HeaderButton buttons[] = {
        { nullptr, "List", "Kernels as tabs, by total duration", !table },
        { nullptr, "Table", "Full kernel table: sort, filter and add metric columns",
          table },
        { ICON_X_CIRCLED, nullptr, "Hide (turn it back on from Panels)", false },
    };

    BeginPaneCard("kernel_rail_card");
    bool      title_double_clicked = false;
    const int clicked =
        RenderPaneHeader("Kernels", -1, false, false, buttons,
                         static_cast<int>(IM_ARRAYSIZE(buttons)), title_double_clicked);
    ImGui::BeginChild("kernel_rail_body", ImVec2(0, 0), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
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

    if(clicked == 0 && table)
    {
        SetKernelRailTableMode(false);
    }
    else if(clicked == 1 && !table)
    {
        SetKernelRailTableMode(true);
    }
    else if(clicked == 2)
    {
        settings.show_compute_kernel_list = false;
    }
}

void
ComputeKernelDetailsView::RefreshKernelList()
{
    const uint32_t workload_id = m_compute_selection->GetSelectedWorkload();
    if(workload_id == m_kernel_list_workload)
    {
        return;
    }
    m_kernel_list_workload = workload_id;
    m_kernel_list = m_data_provider.ComputeModel().GetKernelInfoList(workload_id);
    std::stable_sort(m_kernel_list.begin(), m_kernel_list.end(),
                     [](const KernelInfo* a, const KernelInfo* b) {
                         return a->dispatch_metrics[KernelInfo::DurationTotal] >
                                b->dispatch_metrics[KernelInfo::DurationTotal];
                     });
    double total = 0.0;
    for(const KernelInfo* kernel : m_kernel_list)
    {
        total += static_cast<double>(kernel->dispatch_metrics[KernelInfo::DurationTotal]);
    }
    m_kernel_list_total_duration = static_cast<float>(total);
}

void
ComputeKernelDetailsView::RenderKernelList()
{
    RefreshKernelList();

    ImGui::SetNextItemWidth(-FLT_MIN);
    InputTextStringWithHint("##kernel_filter", "Search kernels", m_kernel_filter);
    ImGui::Spacing();

    ImGui::BeginChild("kernel_list_scroll", ImVec2(0, 0));
    const uint32_t selected           = m_compute_selection->GetSelectedKernel();
    // A selection made elsewhere (toolbar, table) scrolls into view once.
    const bool     scroll_to_selected = selected != m_kernel_list_selection;
    bool           any_listed         = false;
    for(const KernelInfo* kernel : m_kernel_list)
    {
        if(!kernel || !ContainsIgnoreCase(kernel->name, m_kernel_filter))
        {
            continue;
        }
        any_listed = true;
        RenderKernelTab(*kernel, kernel->id == selected, m_kernel_list_total_duration);
        if(kernel->id == selected && scroll_to_selected)
        {
            ImGui::SetScrollHereY(0.5f);
        }
    }
    if(!any_listed)
    {
        ImGui::TextDisabled("%s", m_kernel_list.empty() ? "No kernels in this workload."
                                                        : "No kernels match the search.");
    }
    m_kernel_list_selection = m_compute_selection->GetSelectedKernel();
    ImGui::EndChild();
}

void
ComputeKernelDetailsView::RenderKernelTab(const KernelInfo& kernel, bool selected,
                                          float total_duration)
{
    SettingsManager& settings = SettingsManager::GetInstance();
    const float      line_h   = ImGui::GetTextLineHeight();
    const float      width    = std::max(ImGui::GetContentRegionAvail().x, 1.0f);
    const float      height   = KERNEL_TAB_PAD * 2.0f + line_h * 2.0f +
                         KERNEL_TAB_LINE_GAP * 2.0f + KERNEL_TAB_BAR_HEIGHT;

    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 max(min.x + width, min.y + height);
    ImGui::PushID(static_cast<int>(kernel.id));
    const bool clicked = ImGui::InvisibleButton("##kernel_tab", ImVec2(width, height));
    const bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    if(selected || hovered)
    {
        draw_list->AddRectFilled(min, max,
                                 settings.GetColor(selected ? Colors::kSelection
                                                            : Colors::kButtonHovered),
                                 KERNEL_TAB_ROUNDING);
    }
    if(selected)
    {
        draw_list->AddRectFilled(min, ImVec2(min.x + KERNEL_TAB_ACCENT_WIDTH, max.y),
                                 settings.GetColor(Colors::kAccent), KERNEL_TAB_ROUNDING,
                                 ImDrawFlags_RoundCornersLeft);
    }

    const double duration =
        static_cast<double>(kernel.dispatch_metrics[KernelInfo::DurationTotal]);
    const float share =
        total_duration > 0.0f ? static_cast<float>(duration / total_duration) : 0.0f;
    const TimeFormat time_format = settings.GetUserSettings().unit_settings.time_format;
    char             stats[128];
    std::snprintf(stats, sizeof(stats), "%.1f%%   %s   %llu calls", share * 100.0f,
                  nanosecond_to_formatted_str(duration, time_format, true).c_str(),
                  static_cast<unsigned long long>(
                      kernel.dispatch_metrics[KernelInfo::InvocationCount]));

    const float       text_x = min.x + KERNEL_TAB_ACCENT_WIDTH + KERNEL_TAB_PAD;
    const float       text_w = std::max(0.0f, max.x - KERNEL_TAB_PAD - text_x);
    const std::string name   = ElideWithEllipsis(kernel.name, text_w, KERNEL_NAME_MAX_CHARS);
    float             y      = min.y + KERNEL_TAB_PAD;
    draw_list->AddText(ImVec2(text_x, y), settings.GetColor(Colors::kTextMain), name.c_str());
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

    if(hovered)
    {
        BeginTooltipStyled();
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + KERNEL_TOOLTIP_WIDTH);
        ImGui::TextUnformatted(kernel.name.c_str());
        ImGui::PopTextWrapPos();
        ImGui::TextDisabled("%s", stats);
        EndTooltipStyled();
    }
    if(clicked && !selected)
    {
        m_kernel_list_selection = kernel.id;
        m_compute_selection->SelectKernel(kernel.id);
    }
}

void
ComputeKernelDetailsView::Render()
{
    SyncPanelToggles();
    const LayoutState state = CurrentLayoutState();
    if(!m_layout_built || state.layout != m_built_layout.layout ||
       state.maximized != m_built_layout.maximized ||
       state.kernel_rail != m_built_layout.kernel_rail)
    {
        RebuildLayout(state);
    }
    UpdatePaneMinSizes();

    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, WORKSPACE_SCROLLBAR_SIZE);
    ImGui::BeginChild("kernel_details", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    RenderToolbar();
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
    const AppWindowSettings& settings = SettingsManager::GetInstance().GetAppWindowSettings();
    json[JSON_KEY_LAYOUT_TEMPLATE] = settings.compute_layout_template;
    for(int32_t slot = 0; slot < COMPUTE_LAYOUT_MAX_SLOTS; ++slot)
    {
        json[JSON_KEY_LAYOUT_SLOTS][static_cast<size_t>(slot)] =
            settings.compute_layout_slots[slot];
    }
    json[JSON_KEY_LAYOUT_MAXIMIZED]         = settings.compute_maximized;
    json[JSON_KEY_LAYOUT_MAXIMIZED_PANE]    = settings.compute_maximized_pane;
    json[JSON_KEY_LAYOUT_KERNEL_LIST]       = settings.show_compute_kernel_list;
    json[JSON_KEY_LAYOUT_KERNEL_LIST_TABLE] = settings.compute_kernel_list_table;
    json[JSON_KEY_LAYOUT_FIT_MEMORY_CHART]  = m_widget.m_memory_chart.GetFitToView();
    return true;
}

bool
ComputeKernelDetailsView::Preset::FromJson(jt::Json& json)
{
    if(!json.isObject())
    {
        return false;
    }
    AppWindowSettings& settings = SettingsManager::GetInstance().GetAppWindowSettings();
    bool               result   = true;
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

    read_int(JSON_KEY_LAYOUT_TEMPLATE, settings.compute_layout_template);
    if(json.contains(JSON_KEY_LAYOUT_SLOTS) && json[JSON_KEY_LAYOUT_SLOTS].isArray())
    {
        std::vector<jt::Json>& slots = json[JSON_KEY_LAYOUT_SLOTS].getArray();
        const size_t count = std::min(slots.size(), static_cast<size_t>(COMPUTE_LAYOUT_MAX_SLOTS));
        for(size_t i = 0; i < count; ++i)
        {
            if(slots[i].isLong())
            {
                settings.compute_layout_slots[i] = static_cast<int32_t>(slots[i].getLong());
            }
        }
    }
    read_bool(JSON_KEY_LAYOUT_MAXIMIZED, settings.compute_maximized);
    read_int(JSON_KEY_LAYOUT_MAXIMIZED_PANE, settings.compute_maximized_pane);
    read_bool(JSON_KEY_LAYOUT_KERNEL_LIST, settings.show_compute_kernel_list);
    bool table = settings.compute_kernel_list_table;
    read_bool(JSON_KEY_LAYOUT_KERNEL_LIST_TABLE, table);
    m_widget.SetKernelRailTableMode(table);
    bool fit = m_widget.m_memory_chart.GetFitToView();
    read_bool(JSON_KEY_LAYOUT_FIT_MEMORY_CHART, fit);
    m_widget.m_memory_chart.SetFitToView(fit);
    return result;
}

void
ComputeKernelDetailsView::Preset::Reset()
{
    m_widget.ResetLayout();
}

}  // namespace View
}  // namespace RocProfVis
