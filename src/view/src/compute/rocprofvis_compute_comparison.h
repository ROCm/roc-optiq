// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "model/compute/rocprofvis_compute_model_types.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_widget.h"
#include <bitset>
#include <optional>

namespace RocProfVis
{
namespace View
{

class DataProvider;
class ComputeSelection;
class TabContainer;
class VFixedContainer;

class ComputeComparisonView : public RocWidget
{
public:
    ComputeComparisonView(DataProvider&                     data_provider,
                          std::shared_ptr<ComputeSelection> compute_selection);
    ~ComputeComparisonView();

    void Update() override;
    void Render() override;

    void SubscribeEvents();
    void UnsubscribeEvents();

private:
    class Table : public RocWidget
    {
    public:
        static constexpr size_t MAX_ROW_TAGS = 8;

        // Visuals supported by each cell...
        struct DisplayProps
        {
            struct Color
            {
                Colors color;
                ImU32  alpha;
            };
            std::optional<Color>       bg_color;
            std::optional<Color>       text_color;
            std::optional<const char*> icon;
        };
        // Clients populate data via Rows of Values...
        struct Value
        {
            std::string           name;
            std::optional<double> data;
            DisplayProps          display_props;
        };
        struct Row
        {
            struct ID
            {
                std::string      metric_id;
                std::string_view entry_name;

                bool operator==(const ID& other) const
                {
                    return metric_id == other.metric_id && entry_name == other.entry_name;
                }
            };
            // Internal persistant Value storage, never changes once added...
            struct Value
            {
                std::string  name;
                std::string  data;
                DisplayProps display_props;
            };
            // Render representation, rebuilt/reordered as needed when rows/columns
            // change...
            struct Cell
            {
                std::string_view    data;
                const DisplayProps* display_props;
            };
            ID                                     id;
            const AvailableMetrics::Entry*         entry;
            std::unordered_map<std::string, Value> values_map;
            std::vector<Cell>                      cells;
            DisplayProps                           display_props;
            std::bitset<MAX_ROW_TAGS>              tags;
            mutable bool                           selected;

            bool operator==(const Row& other) const { return id == other.id; }
        };

        Table(std::string     title,
              ImGuiTableFlags flags     = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg,
              int scroll_freeze_columns = 0, int scroll_freeze_rows = 0);
        ~Table();

        void Update() override;
        void Render() override;

        // Getters...
        const std::vector<Row>&         Rows() const;
        const std::vector<std::string>& OrderedValueNames() const;

        // Row manipulation...
        void ReserveRows(size_t rows);
        void AddRow(const AvailableMetrics::Entry* entry, std::vector<Value>& values,
                    DisplayProps          display_props = {},
                    std::optional<size_t> tag           = std::nullopt);
        void AddRow(const Table& table, const size_t index);
        void RemoveRow(size_t index);
        void ClearRows();

        // Customization...
        void ApplyRowFilter(size_t tag);
        void RemoveRowFilter(size_t tag);

        // Setters...
        void SetMaxSize(ImVec2 max_size);
        void SetRowSelectionHandler(
            const std::function<void(const Table& table, const size_t index,
                                     const bool state)>& handler);

    private:
        struct Column
        {
            enum Type
            {
                Selection,
                MetricID,
                MetricName,
                Value,
                Unit,
            };
            Type        type;
            std::string name;
            uint32_t    ref_count;
        };

        // Public properties...
        std::string               m_title;
        ImGuiTableFlags           m_flags;
        ImVec2                    m_max_size;
        int                       m_scroll_freeze_columns;
        int                       m_scroll_freeze_rows;
        std::bitset<MAX_ROW_TAGS> m_row_filter;
        std::optional<size_t>     m_remove_row_index;

        // Internal properties...
        std::vector<std::string>                m_ordered_value_names;
        std::unordered_map<std::string, Column> m_value_columns;
        std::vector<Column>                     m_columns;
        std::vector<Row>                        m_rows;

        // Internal state...
        bool   m_rows_changed;
        bool   m_row_filter_changed;
        bool   m_scrollable;
        size_t m_visible_row_count;

        SettingsManager& m_settings;
        std::function<void(const Table& table, const size_t index, const bool state)>
            m_row_selection_callback;
    };
    struct CategoryModel
    {
        const AvailableMetrics::Category*   category;
        std::vector<std::shared_ptr<Table>> tables;
    };
    struct BookmarkModel
    {
        Table::Row::ID                 row_id;
        const AvailableMetrics::Entry* entry;
        const Table::Row*              row;

        bool operator==(const BookmarkModel& other) const
        {
            return row_id == other.row_id;
        }
    };

    void FetchMetrics();
    void UpdateMetrics();

    void RenderToolbar();
    void RenderCategory(const size_t i);
    void RenderBookmarks() const;
    void RenderTables() const;

    void AddBookmark(const Table& table, const size_t index);
    void RemoveBookmark(const Table& table, const size_t index);
    void UpdateBookmarks();

    // User options...
    uint32_t m_target_workload_id;
    uint32_t m_target_kernel_id;
    bool     m_filter_common_metrics;

    // Internal state...
    bool        m_inputs_changed;
    bool        m_data_changed;
    bool        m_loading;
    bool        m_retry_fetch;
    std::string m_active_tab_id;

    // Models...
    std::vector<CategoryModel> m_categories;
    std::vector<BookmarkModel> m_bookmarks;

    // Layout...
    std::shared_ptr<VFixedContainer> m_layout;
    std::unique_ptr<TabContainer>    m_tab_container;
    std::unique_ptr<Table>           m_bookmark_table;
    LayoutItem*                      m_bookmark_item;
    float                            m_max_bookmark_height;
    float                            m_toolbar_available_width;

    // Requests...
    uint64_t                          m_client_id_baseline;
    uint64_t                          m_client_id_target;
    uint64_t                          m_baseline_request_id;
    uint64_t                          m_target_request_id;
    DataProvider&                     m_data_provider;
    SettingsManager&                  m_settings;
    std::shared_ptr<ComputeSelection> m_compute_selection;

    EventManager::SubscriptionToken m_workload_selection_changed_token;
    EventManager::SubscriptionToken m_kernel_selection_changed_token;
    EventManager::SubscriptionToken m_metrics_fetched_token;
};

}  // namespace View
}  // namespace RocProfVis
