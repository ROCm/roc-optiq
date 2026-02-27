// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "compute/rocprofvis_compute_data_provider.h"
#include "model/compute/rocprofvis_compute_model_types.h"
#include "rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

class ComputeWidgetLegacy : public RocWidget
{
public:
    ComputeWidgetLegacy(std::shared_ptr<ComputeDataProvider> data_provider);
    void Update() = 0;
    void Render() = 0;

protected:
    std::shared_ptr<ComputeDataProvider> m_data_provider;
    std::string m_id;
};

class ComputePlotLegacy : public ComputeWidgetLegacy
{
public:
    ComputePlotLegacy(std::shared_ptr<ComputeDataProvider> data_provider, rocprofvis_controller_compute_plot_types_t type);
    void Update() override;
    void Render() = 0;

protected:
    rocprofvis_controller_compute_plot_types_t m_type;
    ComputePlotModel* m_model;
};

class ComputeTableLegacy : public ComputeWidgetLegacy
{
public:
    ComputeTableLegacy(std::shared_ptr<ComputeDataProvider> data_provider, rocprofvis_controller_compute_table_types_t type);
    void Update() override;
    void Render() override;
    void Search(const std::string& term);

private:
    rocprofvis_controller_compute_table_types_t m_type;
    ComputeTableModel* m_model;
};

class ComputePlotPieLegacy : public ComputePlotLegacy
{
public:
    ComputePlotPieLegacy(std::shared_ptr<ComputeDataProvider> data_provider, rocprofvis_controller_compute_plot_types_t type);
    void Render() override;
};

class ComputePlotBarLegacy : public ComputePlotLegacy
{
public:
    ComputePlotBarLegacy(std::shared_ptr<ComputeDataProvider> data_provider, rocprofvis_controller_compute_plot_types_t type);
    void Render() override;
};

class ComputeMetricLegacy : public ComputeWidgetLegacy
{
public:
    ComputeMetricLegacy(std::shared_ptr<ComputeDataProvider> data_provider, rocprofvis_controller_compute_metric_types_t type, const std::string& label, const std::string& unit);
    void Update() override;
    void Render() {};
    std::string GetFormattedString() const;

private:
    rocprofvis_controller_compute_metric_types_t m_type;
    ComputeMetricModel* m_model;
    std::string m_name;
    std::string m_unit;
    std::string m_formatted_string;
};

class ComputePlotRooflineLegacy : public ComputePlotLegacy
{
public:
    enum GroupMode {
        GroupModeKernel,
        GroupModeDispatch
    };
    ComputePlotRooflineLegacy(std::shared_ptr<ComputeDataProvider> data_provider, rocprofvis_controller_compute_plot_types_t type);
    void Update() override;
    void Render() override;
    void UpdateGroupMode();
    void SetGroupMode(const GroupMode& mode);

private:
    GroupMode m_group_mode;
    bool m_group_dirty;
    std::vector<const char*> m_ceilings_names;
    std::vector<std::vector<double>*> m_ceilings_x;
    std::vector<std::vector<double>*> m_ceilings_y;
    std::vector<const char*> m_ai_names;
    std::vector<std::vector<double>*> m_ai_x;
    std::vector<std::vector<double>*> m_ai_y;
};

class MetricTableCache
{
public:
    using MetricValueLookup = std::function<std::shared_ptr<MetricValue>(uint32_t entry_id)>;

    struct Row
    {
        std::string                name;
        std::vector<std::string>   values;
        std::string              unit;
    };

    void Populate(const AvailableMetrics::Table& table,
                  const MetricValueLookup&       get_value);

    void Clear();
    void Render() const;

    bool Empty() const;

private:
    std::string              m_title;
    std::string              m_table_id;
    std::vector<std::string> m_column_names;
    std::vector<Row>         m_rows;
};

constexpr uint32_t METRIC_CAT_SOL   = 2;
constexpr uint32_t METRIC_TABLE_SOL = 1;

class DataProvider;
class ComputeSelection;

class MetricTableWidget : public RocWidget
{
public:
    MetricTableWidget(DataProvider& data_provider,
                      std::shared_ptr<ComputeSelection> compute_selection,
                      uint32_t category_id, uint32_t table_id);

    void Render() override;

    void     FetchMetrics();
    void     UpdateTable();
    void     Clear();
    uint64_t GetClientId() const { return m_client_id; }

private:
    DataProvider& m_data_provider;
    std::shared_ptr<ComputeSelection> m_compute_selection;
    uint32_t m_category_id;
    uint32_t m_table_id;
    uint64_t m_client_id;

    MetricTableCache m_table;
};

}  // namespace View
}  // namespace RocProfVis
