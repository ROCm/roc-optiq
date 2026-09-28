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

// Width of the Tables picker, in frame heights (matches the toolbar combos).
constexpr float TABLE_PICKER_WIDTH_FRAMES = 12.0f;
// Tallest the picker popup grows, as a fraction of the main viewport height.
constexpr float TABLE_PICKER_MAX_HEIGHT_RATIO = 0.6f;

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
            if(m_fetch_pending)
                FetchAllMetrics();
            if(evt->GetClientId() == m_client_id)
            {
                m_metrics_loading = false;
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
    size_t total = 0;
    size_t shown = 0;
    for(const AvailableMetrics::Category* category :
        workload.available_metrics.ordered_categories)
    {
        for(const AvailableMetrics::Table* table : category->ordered_tables)
        {
            total++;
            if(m_enabled_tables.count(MetricId::GetTableKey(category->id, table->id)) > 0)
            {
                shown++;
            }
        }
    }
    const std::string preview =
        std::to_string(shown) + " of " + std::to_string(total) + " shown";

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

        bool any_listed = false;
        for(const AvailableMetrics::Category* category :
            workload.available_metrics.ordered_categories)
        {
            const bool category_matches = ContainsIgnoreCase(category->name, m_picker_filter);
            bool       header_drawn     = false;
            for(const AvailableMetrics::Table* table : category->ordered_tables)
            {
                const std::string label = TableLabel(category->id, *table);
                if(!category_matches && !ContainsIgnoreCase(label, m_picker_filter))
                {
                    continue;
                }
                const uint64_t key = MetricId::GetTableKey(category->id, table->id);
                if(show_listed)
                {
                    m_enabled_tables.insert(key);
                }
                else if(hide_listed)
                {
                    m_enabled_tables.erase(key);
                }
                if(!header_drawn)
                {
                    ImGui::SeparatorText(category->name.c_str());
                    header_drawn = true;
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

    for(const AvailableMetrics::Category* category :
        workload.available_metrics.ordered_categories)
    {
        for(const AvailableMetrics::Table* table : category->ordered_tables)
        {
            const uint64_t key = MetricId::GetTableKey(category->id, table->id);
            if(m_enabled_tables.count(key) == 0)
            {
                continue;
            }
            listed = true;

            // "###" keeps the header's open state keyed by table, not by label.
            const std::string label =
                TableLabel(category->id, *table) + "###table_" + std::to_string(key);
            bool keep_shown = true;
            ApplyHeaderRequest();
            if(ImGui::CollapsingHeader(label.c_str(), &keep_shown,
                                       ImGuiTreeNodeFlags_DefaultOpen))
            {
                std::unordered_map<uint64_t, MetricTable>::iterator it =
                    m_table_widgets.find(key);
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

    for(const auto* cat : workload->available_metrics.ordered_categories)
    {
        for(const auto* tbl : cat->ordered_tables)
        {
            AddTable(cat->id, tbl);
        }
    }

    RestoreMetricPining();
}

void
ComputeTableView::AddTable(uint32_t category_id, const AvailableMetrics::Table* table)
{
    uint64_t     key = MetricId::GetTableKey(category_id, table->id);
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

    widget.Populate(*table, [&](uint32_t eid) {
        return model.GetKernelMetricValue(m_client_id, kernel_id, category_id, table->id,
                                          eid);
    });
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
    m_widget.m_pinned_metric_table.RefillTable(m_widget.m_pinned_metrics);
    m_widget.m_enabled_tables = { DEFAULT_SHOWN_TABLE };
}

}  // namespace View
}  // namespace RocProfVis
