// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_table_view.h"
#include "icons/rocprovfis_icon_defines.h"
#include "model/compute/rocprofvis_compute_data_model.h"
#include "rocprofvis_compute_selection.h"
#include "rocprofvis_requests.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_gui_helpers.h"

#include <algorithm>
#include <cctype>
#include <cfloat>

namespace RocProfVis
{
namespace View
{

constexpr const char* JSON_KEY_PINNED_METRICS    = "pins";
constexpr const char* JSON_KEY_PINNED_METRICS_ID = "id";
constexpr const char* JSON_KEY_SHOWN_TABLES      = "tables";
constexpr const char* JSON_KEY_COMPARE_COLUMNS   = "compare_columns";
constexpr const char* JSON_KEY_COMPARE_UNMATCHED = "compare_unmatched";
constexpr const char* JSON_KEY_COMPARE_HIGHLIGHT = "compare_highlight";

// Width of the Tables picker, in frame heights (matches the toolbar combos).
constexpr float TABLE_PICKER_WIDTH_FRAMES = 12.0f;
// Tallest the picker popup grows, as a fraction of the main viewport height.
constexpr float TABLE_PICKER_MAX_HEIGHT_RATIO = 0.6f;
// Width of the Compare options combo, in frame heights.
constexpr float COMPARE_OPTIONS_WIDTH_FRAMES = 6.0f;

// Shown until the user picks otherwise: System Speed-of-Light, the headline
// per-kernel table.
constexpr uint64_t DEFAULT_SHOWN_TABLE =
    MetricId::GetTableKey(METRIC_CAT_SOL, METRIC_TABLE_SOL);

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

// "2.1  System Speed-of-Light": the dotted prefix matches the metric ids shown
// in the table's rows.
static std::string
TableLabel(uint32_t category_id, const AvailableMetrics::Table& table)
{
    return std::to_string(category_id) + "." + std::to_string(table.id) + "  " +
           table.name;
}

ComputeTableView::ComputeTableView(DataProvider&                     data_provider,
                                   std::shared_ptr<ComputeSelection> compute_selection,
                                   bool has_available_metrics)
: RocWidget()
, m_data_provider(data_provider)
, m_compute_selection(compute_selection)
, m_client_id(IdGenerator::GetInstance().GenerateId())
, m_has_available_metrics(has_available_metrics)
, m_pinned_metric_table(data_provider, compute_selection, m_client_id)
, m_compare_workload_id(ComputeSelection::INVALID_SELECTION_ID)
, m_compare_kernel_id(ComputeSelection::INVALID_SELECTION_ID)
, m_compare_client_id(IdGenerator::GetInstance().GenerateId())
{
    m_enabled_tables.insert(DEFAULT_SHOWN_TABLE);
    m_pinned_metric_table.SetEmbedded(true);
    m_pinned_metric_table.SetPinMetricCallback([this](MetricId metric_id) {
        m_pinned_metrics.erase(metric_id);
        auto table_it = m_table_widgets.find(metric_id.GetTableKey());
        if(table_it != m_table_widgets.end())
        {
            table_it->second.ChangePinState(metric_id);
        }
        m_pinned_metric_table.RefillTable(m_pinned_metrics);
    });

    auto workload_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<ComputeSelectionChangedEvent>(e);
        if(evt && evt->GetSourceId() == m_data_provider.GetTraceFilePath())
        {
            ResetWorkloadData();
        }
    };

    m_workload_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeWorkloadSelectionChanged),
        workload_changed_handler);

    auto kernel_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<ComputeSelectionChangedEvent>(e);
        if(evt && evt->GetSourceId() == m_data_provider.GetTraceFilePath())
        {
            FetchAllMetrics();
        }
    };

    m_kernel_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeKernelSelectionChanged),
        kernel_changed_handler);

    auto metrics_fetched_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<ComputeMetricsFetchedEvent>(e);
        if(evt && evt->GetSourceId() == m_data_provider.GetTraceFilePath())
        {
            // A fetch refused while another ran is retried once one lands.
            if(m_fetch_pending)
            {
                FetchAllMetrics();
            }
            if(m_compare_fetch_pending)
            {
                FetchCompareMetrics();
            }
            if(evt->GetClientId() == m_client_id)
            {
                m_metrics_loading = false;
                RebuildTableDataCache();
                m_pinned_metric_table.RefillTable(m_pinned_metrics);
            }
            else if(m_compare_active && evt->GetClientId() == m_compare_client_id)
            {
                m_compare_loading = false;
                RebuildTableDataCache();
                m_pinned_metric_table.RefillTable(m_pinned_metrics);
            }
        }
    };

    m_metrics_fetched_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeMetricsFetched), metrics_fetched_handler);

    m_widget_name = GenUniqueName("ComputeTableView");
    m_preset = std::make_unique<Preset>(*this);
}

ComputeTableView::~ComputeTableView()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeWorkloadSelectionChanged),
        m_workload_selection_changed_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeKernelSelectionChanged),
        m_kernel_selection_changed_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeMetricsFetched),
        m_metrics_fetched_token);
}

void
ComputeTableView::SetCompareTarget(uint32_t workload_id, uint32_t kernel_id)
{
    if(m_compare_active && workload_id == m_compare_workload_id &&
       kernel_id == m_compare_kernel_id)
    {
        return;
    }
    m_compare_active      = true;
    m_compare_workload_id = workload_id;
    m_compare_kernel_id   = kernel_id;
    FetchCompareMetrics();
    RebuildTableDataCache();
    RefreshPins();
}

void
ComputeTableView::ClearCompareTarget()
{
    if(!m_compare_active)
    {
        return;
    }
    m_compare_active        = false;
    m_compare_fetch_pending = false;
    m_compare_loading       = false;
    m_data_provider.ComputeModel().ClearKernelMetricValues(m_compare_client_id);
    RebuildTableDataCache();
    RefreshPins();
}

void
ComputeTableView::RefreshPins()
{
    if(m_compare_active)
    {
        m_pinned_metric_table.SetCompareSource(m_compare_client_id, m_compare_workload_id,
                                               m_compare_kernel_id, m_compare_options);
    }
    else
    {
        m_pinned_metric_table.ClearCompareSource();
    }
    m_pinned_metric_table.RefillTable(m_pinned_metrics);
}

void
ComputeTableView::FetchCompareMetrics()
{
    m_data_provider.ComputeModel().ClearKernelMetricValues(m_compare_client_id);
    m_compare_fetch_pending = false;
    m_compare_loading       = false;
    const WorkloadInfo* workload =
        m_data_provider.ComputeModel().GetWorkload(m_compare_workload_id);
    if(!m_compare_active || !workload ||
       m_compare_kernel_id == ComputeSelection::INVALID_SELECTION_ID)
    {
        return;
    }

    // Every table, as for A, so showing another needs no extra round trip.
    std::vector<uint32_t>                       kernel_ids = { m_compare_kernel_id };
    std::vector<MetricsRequestParams::MetricID> metric_ids;
    for(const AvailableMetrics::Category* category :
        workload->available_metrics.ordered_categories)
    {
        for(const AvailableMetrics::Table* table : category->ordered_tables)
        {
            metric_ids.push_back({ category->id, table->id, std::nullopt });
        }
    }
    if(metric_ids.empty())
    {
        return;
    }
    m_compare_loading = true;
    if(!m_data_provider.FetchMetrics(MetricsRequestParams(workload->id, kernel_ids,
                                                          metric_ids, m_compare_client_id)))
    {
        m_compare_fetch_pending = true;
    }
}

std::vector<ComputeTableView::ListedTable>
ComputeTableView::ListedTables(const WorkloadInfo& workload) const
{
    const WorkloadInfo* b_workload =
        m_compare_active ? m_data_provider.ComputeModel().GetWorkload(m_compare_workload_id)
                         : nullptr;
    auto find_table = [](const WorkloadInfo* source, uint32_t category_id,
                         uint32_t table_id) -> const AvailableMetrics::Table* {
        if(!source)
        {
            return nullptr;
        }
        std::unordered_map<uint32_t, AvailableMetrics::Category>::const_iterator category =
            source->available_metrics.tree.find(category_id);
        if(category == source->available_metrics.tree.end())
        {
            return nullptr;
        }
        std::unordered_map<uint32_t, AvailableMetrics::Table>::const_iterator table =
            category->second.tables.find(table_id);
        return table != category->second.tables.end() ? &table->second : nullptr;
    };

    std::vector<ListedTable> listed;
    for(const AvailableMetrics::Category* category :
        workload.available_metrics.ordered_categories)
    {
        for(const AvailableMetrics::Table* table : category->ordered_tables)
        {
            listed.push_back({ category->id, &category->name, table,
                               find_table(b_workload, category->id, table->id) });
        }
    }
    // Tables only B's workload has (e.g. another GPU architecture) come last.
    if(b_workload && b_workload != &workload)
    {
        for(const AvailableMetrics::Category* category :
            b_workload->available_metrics.ordered_categories)
        {
            for(const AvailableMetrics::Table* table : category->ordered_tables)
            {
                if(!find_table(&workload, category->id, table->id))
                {
                    listed.push_back({ category->id, &category->name, nullptr, table });
                }
            }
        }
    }
    return listed;
}

void
ComputeTableView::ResetWorkloadData()
{
    m_table_widgets.clear();
    m_data_provider.ComputeModel().ClearKernelMetricValues(m_client_id);
}

void
ComputeTableView::FetchAllMetrics()
{
    m_data_provider.ComputeModel().ClearKernelMetricValues(m_client_id);
    m_table_widgets.clear();
    m_fetch_pending   = false;
    m_metrics_loading = false;

    uint32_t workload_id = m_compute_selection->GetSelectedWorkload();
    uint32_t kernel_id   = m_compute_selection->GetSelectedKernel();
    if(workload_id == ComputeSelection::INVALID_SELECTION_ID ||
       kernel_id == ComputeSelection::INVALID_SELECTION_ID)
    {
        return;
    }

    const WorkloadInfo* workload =
        m_data_provider.ComputeModel().GetWorkload(workload_id);
    if(!workload)
        return;

    // Every table is fetched, not just the shown ones, so turning a table on
    // (or showing a pin from a hidden table) needs no extra round trip.
    std::vector<uint32_t>                   kernel_ids = { kernel_id };
    std::vector<MetricsRequestParams::MetricID> metric_ids;
    for(const auto* cat : workload->available_metrics.ordered_categories)
    {
        for(const auto* tbl : cat->ordered_tables)
            metric_ids.push_back({ cat->id, tbl->id, std::nullopt });
    }

    if(metric_ids.empty())
    {
        return;
    }

    m_metrics_loading = true;

    bool success = m_data_provider.FetchMetrics(
        MetricsRequestParams(workload->id, kernel_ids, metric_ids, m_client_id));

    if(!success)
    {
        m_fetch_pending = true;
    }
}

void
ComputeTableView::Update()
{
    m_pinned_metric_table.Update();
}

void
ComputeTableView::Render()
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();
    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          settings.GetColor(m_chromeless ? Colors::kTransparent
                                                         : Colors::kBgPanel));
    ImGui::PushStyleColor(ImGuiCol_Border, settings.GetColor(Colors::kBorderColor));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, style.ChildRounding);
    ImGui::BeginChild("metric_tables_card", ImVec2(0, 0),
                      m_chromeless ? ImGuiChildFlags_None
                                   : ImGuiChildFlags_Borders |
                                         ImGuiChildFlags_AlwaysUseWindowPadding);
    if(!m_chromeless)
    {
        SectionTitle("Metric Tables");
    }

    const uint32_t workload_id = m_compute_selection
                                     ? m_compute_selection->GetSelectedWorkload()
                                     : ComputeSelection::INVALID_SELECTION_ID;
    const WorkloadInfo* workload =
        m_data_provider.ComputeModel().GetWorkload(workload_id);
    if(!m_has_available_metrics)
    {
        ImGui::TextDisabled("%s", NO_METRICS_MESSAGE);
    }
    else if(!workload)
    {
        ImGui::TextDisabled("Select a workload from the toolbar.");
    }
    else if(m_compute_selection->GetSelectedKernel() ==
            ComputeSelection::INVALID_SELECTION_ID)
    {
        ImGui::TextDisabled("Select a kernel to see its metric tables.");
    }
    else
    {
        RenderToolbar(*workload);
        ImGui::BeginChild("metric_tables_scroll", ImVec2(0, 0));
        RenderTables(*workload);
        ImGui::EndChild();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

void
ComputeTableView::RenderToolbar(const WorkloadInfo& workload)
{
    SettingsManager&  settings  = SettingsManager::GetInstance();
    const ImGuiStyle& style     = settings.GetDefaultStyle();
    ImFont*           icon_font = settings.GetFontManager().GetFont(FontType::kIcon);

    RenderTablePicker(workload);
    if(m_compare_active)
    {
        ImGui::SameLine();
        RenderCompareOptions();
        if(m_compare_loading)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("Loading B...");
        }
    }

    // Expand / collapse every shown table, right-aligned on the picker's row.
    ImGui::PushFont(icon_font, 0.0f);
    const float icon_w = std::max(ImGui::CalcTextSize(ICON_ARROWS_EXPAND).x,
                                  ImGui::CalcTextSize(ICON_ARROWS_SHRINK).x) +
                         style.FramePadding.x * 2.0f;
    ImGui::PopFont();
    const float buttons_w = icon_w * 2.0f + style.ItemSpacing.x;
    ImGui::SameLine();
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),
                                  ImGui::GetCursorPosX() +
                                      ImGui::GetContentRegionAvail().x - buttons_w));
    const bool nothing_listed = m_enabled_tables.empty() && m_pinned_metrics.empty();
    ImGui::BeginDisabled(nothing_listed);
    if(IconButton(ICON_ARROWS_EXPAND, icon_font, ImVec2(0, 0), "Expand all tables", false,
                  style.FramePadding, settings.GetColor(Colors::kTransparent),
                  settings.GetColor(Colors::kButtonHovered),
                  settings.GetColor(Colors::kTransparent)))
    {
        m_header_request = HeaderRequest::kExpandAll;
    }
    ImGui::SameLine();
    if(IconButton(ICON_ARROWS_SHRINK, icon_font, ImVec2(0, 0), "Collapse all tables",
                  false, style.FramePadding, settings.GetColor(Colors::kTransparent),
                  settings.GetColor(Colors::kButtonHovered),
                  settings.GetColor(Colors::kTransparent)))
    {
        m_header_request = HeaderRequest::kCollapseAll;
    }
    ImGui::EndDisabled();
    ImGui::Spacing();
}

void
ComputeTableView::RenderTablePicker(const WorkloadInfo& workload)
{
    const std::vector<ListedTable> listed = ListedTables(workload);
    size_t                         shown  = 0;
    for(const ListedTable& table : listed)
    {
        if(m_enabled_tables.count(table.Key()) > 0)
        {
            shown++;
        }
    }
    const std::string preview =
        std::to_string(shown) + " of " + std::to_string(listed.size()) + " shown";

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Tables:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFrameHeight() * TABLE_PICKER_WIDTH_FRAMES);
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(0.0f, 0.0f),
        ImVec2(FLT_MAX, ImGui::GetMainViewport()->WorkSize.y * TABLE_PICKER_MAX_HEIGHT_RATIO));
    PushComboStyles();
    if(ImGui::BeginCombo("##metric_table_picker", preview.c_str(),
                         ImGuiComboFlags_HeightLargest))
    {
        if(ImGui::IsWindowAppearing())
        {
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::SetNextItemWidth(-FLT_MIN);
        InputTextStringWithHint("##metric_table_filter", "Search tables", m_picker_filter);

        // Bulk toggles apply to the tables the search currently lists.
        const bool show_listed = ImGui::SmallButton("Show all");
        ImGui::SameLine();
        const bool hide_listed = ImGui::SmallButton("Hide all");

        bool               any_listed    = false;
        const std::string* last_category = nullptr;
        for(const ListedTable& table : listed)
        {
            const AvailableMetrics::Table& named = table.a ? *table.a : *table.b;
            const std::string label = TableLabel(table.category_id, named);
            if(!ContainsIgnoreCase(*table.category_name, m_picker_filter) &&
               !ContainsIgnoreCase(label, m_picker_filter))
            {
                continue;
            }
            const uint64_t key = table.Key();
            if(show_listed)
            {
                m_enabled_tables.insert(key);
            }
            else if(hide_listed)
            {
                m_enabled_tables.erase(key);
            }
            if(last_category != table.category_name)
            {
                ImGui::SeparatorText(table.category_name->c_str());
                last_category = table.category_name;
            }
            any_listed = true;
            bool on    = m_enabled_tables.count(key) > 0;
            if(ImGui::Checkbox(label.c_str(), &on))
            {
                if(on)
                {
                    m_enabled_tables.insert(key);
                }
                else
                {
                    m_enabled_tables.erase(key);
                }
            }
            if(m_compare_active && (!table.a || !table.b))
            {
                ImGui::SameLine();
                ImGui::TextDisabled("%s", table.a ? "A only" : "B only");
            }
        }
        if(!any_listed)
        {
            ImGui::TextDisabled("No tables match \"%s\".", m_picker_filter.c_str());
        }
        ImGui::EndCombo();
    }
    PopComboStyles();
}

void
ComputeTableView::RenderCompareOptions()
{
    constexpr const char* COLUMN_LABELS[] = { "A (baseline)", "B (target)",
                                              "\xCE\x94 (B - A)", "\xCE\x94% (of A)" };
    static_assert(sizeof(COLUMN_LABELS) / sizeof(COLUMN_LABELS[0]) ==
                      static_cast<size_t>(MetricCompareColumn::kCount),
                  "one label per MetricCompareColumn");
    constexpr float HIGHLIGHT_DRAG_SPEED = 0.5f;
    constexpr float HIGHLIGHT_MAX_PCT    = 100.0f;

    bool changed = false;
    ImGui::SetNextItemWidth(ImGui::GetFrameHeight() * COMPARE_OPTIONS_WIDTH_FRAMES);
    PushComboStyles();
    if(ImGui::BeginCombo("##metric_compare_options", "Compare"))
    {
        ImGui::TextDisabled("Columns per value");
        for(size_t column = 0; column < static_cast<size_t>(MetricCompareColumn::kCount);
            ++column)
        {
            bool on = m_compare_options.columns.test(column);
            // At least one column stays on.
            ImGui::BeginDisabled(on && m_compare_options.columns.count() == 1);
            if(ImGui::Checkbox(COLUMN_LABELS[column], &on))
            {
                m_compare_options.columns.set(column, on);
                changed = true;
            }
            ImGui::EndDisabled();
        }
        ImGui::Separator();
        changed |= ImGui::Checkbox("Metrics only one side has", &m_compare_options.show_unmatched);
        if(ImGui::IsItemHovered())
        {
            SetTooltipStyled("Listed dimmed, e.g. architecture-specific metrics when A and B\n"
                             "ran on different GPUs.");
        }
        ImGui::TextUnformatted("Shade changes of at least");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::DragFloat("##compare_highlight", &m_compare_options.highlight_pct,
                         HIGHLIGHT_DRAG_SPEED, 0.0f, HIGHLIGHT_MAX_PCT, "%.1f%%",
                         ImGuiSliderFlags_AlwaysClamp);
        // Every table is rebuilt, so apply once the drag or edit ends.
        changed |= ImGui::IsItemDeactivatedAfterEdit();
        ImGui::EndCombo();
    }
    PopComboStyles();
    if(ImGui::IsItemHovered())
    {
        SetTooltipStyled("Columns and shading of the A / B comparison.");
    }
    if(changed)
    {
        RebuildTableDataCache();
        RefreshPins();
    }
}

void
ComputeTableView::ApplyHeaderRequest()
{
    if(m_header_request != HeaderRequest::kNone)
    {
        ImGui::SetNextItemOpen(m_header_request == HeaderRequest::kExpandAll);
    }
}

void
ComputeTableView::RenderTables(const WorkloadInfo& workload)
{
    bool listed = false;
    if(!m_pinned_metrics.empty())
    {
        listed = true;
        ApplyHeaderRequest();
        if(ImGui::CollapsingHeader("Pinned Metrics###pinned_metrics",
                                   ImGuiTreeNodeFlags_DefaultOpen))
        {
            m_pinned_metric_table.Render();
            ImGui::Spacing();
        }
    }

    for(const ListedTable& table : ListedTables(workload))
    {
        const uint64_t key = table.Key();
        if(m_enabled_tables.count(key) == 0)
        {
            continue;
        }
        listed = true;

        // "###" keeps the header's open state keyed by table, not by label.
        std::string label = TableLabel(table.category_id, table.a ? *table.a : *table.b);
        if(m_compare_active && (!table.a || !table.b))
        {
            label += table.a ? "  (A only)" : "  (B only)";
        }
        label += "###table_" + std::to_string(key);
        bool keep_shown = true;
        ApplyHeaderRequest();
        if(ImGui::CollapsingHeader(label.c_str(), &keep_shown, ImGuiTreeNodeFlags_DefaultOpen))
        {
            std::unordered_map<uint64_t, MetricTable>::iterator it = m_table_widgets.find(key);
            if(it != m_table_widgets.end())
            {
                it->second.Render();
            }
            else
            {
                ImGui::TextDisabled("%s", m_metrics_loading ? "Loading..."
                                                            : "No data for this table.");
            }
            ImGui::Spacing();
        }
        if(!keep_shown)
        {
            m_enabled_tables.erase(key);
        }
    }
    m_header_request = HeaderRequest::kNone;

    if(!listed)
    {
        RenderEmptyState();
    }
}

void
ComputeTableView::RenderEmptyState()
{
    constexpr const char* TITLE = "No metric tables shown.";
    constexpr const char* HINT  = "Turn tables on from the Tables list above.";
    ImGui::Dummy(ImVec2(0.0f, ImGui::GetFrameHeight()));
    CenterNextTextItem(TITLE);
    ImGui::TextDisabled("%s", TITLE);
    CenterNextTextItem(HINT);
    ImGui::TextDisabled("%s", HINT);
}

void
ComputeTableView::RebuildTableDataCache()
{
    m_table_widgets.clear();

    auto&    model       = m_data_provider.ComputeModel();
    uint32_t workload_id = m_compute_selection->GetSelectedWorkload();

    if(workload_id == ComputeSelection::INVALID_SELECTION_ID)
    {
        return;
    }

    const WorkloadInfo* workload = model.GetWorkload(workload_id);
    if(!workload)
        return;

    for(const ListedTable& table : ListedTables(*workload))
    {
        AddTable(table);
    }

    RestoreMetricPining();
}

void
ComputeTableView::AddTable(const ListedTable& listed)
{
    const uint64_t key = listed.Key();
    auto         [it, inserted]  =
        m_table_widgets.try_emplace(key, m_data_provider.GetTraceFilePath());
    MetricTable& widget          = it->second;
    widget.SetEmbedded(true);
    auto pin_metric_func = [this, &widget](MetricId metric_id) {
        if (widget.IsMetricPinned(metric_id))
        {
            m_pinned_metrics.insert(metric_id);
        }
        else
        {
            m_pinned_metrics.erase(metric_id);
        }
        m_pinned_metric_table.RefillTable(m_pinned_metrics);
    };
    widget.SetPinMetricCallback(pin_metric_func);

    auto& model = m_data_provider.ComputeModel();
    uint32_t kernel_id = m_compute_selection->GetSelectedKernel();
    if(kernel_id == ComputeSelection::INVALID_SELECTION_ID)
    {
        return;
    }

    const uint32_t category_id = listed.category_id;
    auto get_a = [&](uint32_t eid) {
        return listed.a ? model.GetKernelMetricValue(m_client_id, kernel_id, category_id,
                                                     listed.a->id, eid)
                        : nullptr;
    };
    if(!m_compare_active)
    {
        widget.Populate(*listed.a, get_a);
        return;
    }
    auto get_b = [&](uint32_t eid) {
        return listed.b ? model.GetKernelMetricValue(m_compare_client_id, m_compare_kernel_id,
                                                     category_id, listed.b->id, eid)
                        : nullptr;
    };
    widget.PopulateComparison(category_id, listed.a, listed.b, get_a, get_b,
                              m_compare_options);
}

void
ComputeTableView::RestoreMetricPining()
{
    for(MetricId id : m_pinned_metrics)
    {
        auto table_it = m_table_widgets.find(id.GetTableKey());
        if(table_it != m_table_widgets.end())
        {
            table_it->second.ChangePinState(id);
        }
    }
}

ComputeTableView::Preset::Preset(ComputeTableView& widget)
: PresetComponent(PresetManager::ComputeTableView,
                  widget.m_data_provider.GetTraceFilePath())
, m_widget(widget)
{}

bool
ComputeTableView::Preset::ToJson(jt::Json& json)
{
    if(!m_widget.m_pinned_metrics.empty())
    {
        jt::Json& pins = json[JSON_KEY_PINNED_METRICS];
        int       i    = 0;
        for(const MetricId& id : m_widget.m_pinned_metrics)
        {
            pins[i][JSON_KEY_PINNED_METRICS_ID][0] = id.category_id;
            pins[i][JSON_KEY_PINNED_METRICS_ID][1] = id.table_id;
            pins[i][JSON_KEY_PINNED_METRICS_ID][2] = id.entry_id;
            i++;
        }
    }
    if(!m_widget.m_enabled_tables.empty())
    {
        jt::Json& tables = json[JSON_KEY_SHOWN_TABLES];
        int       i      = 0;
        for(uint64_t key : m_widget.m_enabled_tables)
        {
            tables[i][0] = MetricId::ExtractCategoryId(key);
            tables[i][1] = MetricId::ExtractTableId(key);
            i++;
        }
    }
    const MetricCompareOptions& compare = m_widget.m_compare_options;
    for(size_t column = 0; column < compare.columns.size(); ++column)
    {
        json[JSON_KEY_COMPARE_COLUMNS][column] = compare.columns.test(column);
    }
    json[JSON_KEY_COMPARE_UNMATCHED] = compare.show_unmatched;
    json[JSON_KEY_COMPARE_HIGHLIGHT] = static_cast<double>(compare.highlight_pct);
    return true;
}

bool
ComputeTableView::Preset::FromJson(jt::Json& json)
{
    bool result = true;
    if(json.isObject() && json.contains(JSON_KEY_PINNED_METRICS))
    {
        jt::Json& pins = json[JSON_KEY_PINNED_METRICS];
        if(pins.isArray())
        {
            for(jt::Json& obj : pins.getArray())
            {
                result &= obj.isObject() && obj.contains(JSON_KEY_PINNED_METRICS_ID) &&
                          obj[JSON_KEY_PINNED_METRICS_ID].isArray() &&
                          obj[JSON_KEY_PINNED_METRICS_ID].getArray().size() == 3 &&
                          obj[JSON_KEY_PINNED_METRICS_ID].getArray()[0].isLong() &&
                          obj[JSON_KEY_PINNED_METRICS_ID].getArray()[1].isLong() &&
                          obj[JSON_KEY_PINNED_METRICS_ID].getArray()[2].isLong();
            }
            if(result)
            {
                Reset();
                for(jt::Json& obj : pins.getArray())
                {
                    MetricId id;
                    id.category_id = static_cast<uint32_t>(
                        obj[JSON_KEY_PINNED_METRICS_ID].getArray()[0].getLong());
                    id.table_id = static_cast<uint32_t>(
                        obj[JSON_KEY_PINNED_METRICS_ID].getArray()[1].getLong());
                    id.entry_id = static_cast<uint32_t>(
                        obj[JSON_KEY_PINNED_METRICS_ID].getArray()[2].getLong());
                    m_widget.m_pinned_metrics.insert(std::move(id));
                }
                m_widget.RestoreMetricPining();
                m_widget.m_pinned_metric_table.RefillTable(m_widget.m_pinned_metrics);
            }
        }
    }
    if(result && json.isObject() && json.contains(JSON_KEY_SHOWN_TABLES))
    {
        jt::Json& tables = json[JSON_KEY_SHOWN_TABLES];
        if(tables.isArray())
        {
            for(jt::Json& obj : tables.getArray())
            {
                result &= obj.isArray() && obj.getArray().size() == 2 &&
                          obj.getArray()[0].isLong() && obj.getArray()[1].isLong();
            }
            if(result)
            {
                m_widget.m_enabled_tables.clear();
                for(jt::Json& obj : tables.getArray())
                {
                    m_widget.m_enabled_tables.insert(MetricId::GetTableKey(
                        static_cast<uint32_t>(obj.getArray()[0].getLong()),
                        static_cast<uint32_t>(obj.getArray()[1].getLong())));
                }
            }
        }
    }
    if(result && json.isObject())
    {
        MetricCompareOptions compare = m_widget.m_compare_options;
        if(json.contains(JSON_KEY_COMPARE_COLUMNS) && json[JSON_KEY_COMPARE_COLUMNS].isArray())
        {
            std::vector<jt::Json>& columns = json[JSON_KEY_COMPARE_COLUMNS].getArray();
            for(size_t column = 0; column < columns.size() && column < compare.columns.size();
                ++column)
            {
                if(columns[column].isBool())
                {
                    compare.columns.set(column, columns[column].getBool());
                }
            }
        }
        if(json.contains(JSON_KEY_COMPARE_UNMATCHED) && json[JSON_KEY_COMPARE_UNMATCHED].isBool())
        {
            compare.show_unmatched = json[JSON_KEY_COMPARE_UNMATCHED].getBool();
        }
        if(json.contains(JSON_KEY_COMPARE_HIGHLIGHT) &&
           json[JSON_KEY_COMPARE_HIGHLIGHT].isNumber())
        {
            compare.highlight_pct =
                static_cast<float>(json[JSON_KEY_COMPARE_HIGHLIGHT].getNumber());
        }
        // A table shows at least one comparison column.
        if(compare.columns.any())
        {
            m_widget.m_compare_options = compare;
            m_widget.RebuildTableDataCache();
            m_widget.RefreshPins();
        }
    }
    return result;
}

void
ComputeTableView::Preset::Reset()
{
    for(const MetricId& id : m_widget.m_pinned_metrics)
    {
        if(m_widget.m_table_widgets.count(id.GetTableKey()) > 0)
        {
            m_widget.m_table_widgets.at(id.GetTableKey()).ChangePinState(id);
        }
    }
    m_widget.m_pinned_metrics.clear();
    m_widget.m_enabled_tables  = { DEFAULT_SHOWN_TABLE };
    m_widget.m_compare_options = MetricCompareOptions();
    m_widget.RebuildTableDataCache();
    m_widget.RefreshPins();
}

}  // namespace View
}  // namespace RocProfVis
