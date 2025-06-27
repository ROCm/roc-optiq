// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "rocprofvis_compute_data_provider2.h"
#include "rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

class ComputeWidget : public RocWidget
{
public:
    ComputeWidget(std::shared_ptr<ComputeDataProvider2> data_provider);
    void Update() = 0;
    void Render() = 0;

protected:
    std::shared_ptr<ComputeDataProvider2> m_data_provider;
    std::string m_id;
};

class ComputePlot : public ComputeWidget
{
public:
    ComputePlot(std::shared_ptr<ComputeDataProvider2> data_provider, rocprofvis_controller_compute_plot_types_t type);
    void Update() override;
    void Render() = 0;

protected:
    rocprofvis_controller_compute_plot_types_t m_type;
    ComputePlotModel* m_model;
};

class ComputeTable : public ComputeWidget
{
public:
    ComputeTable(std::shared_ptr<ComputeDataProvider2> data_provider, rocprofvis_controller_compute_table_types_t type);
    void Update() override;
    void Render() override;
    void Search(const std::string& term);

private:
    rocprofvis_controller_compute_table_types_t m_type;
    ComputeTableModel* m_model;
};

class ComputePlotPie : public ComputePlot
{
public:
    ComputePlotPie(std::shared_ptr<ComputeDataProvider2> data_provider, rocprofvis_controller_compute_plot_types_t type);
    void Render() override;
};

class ComputePlotBar : public ComputePlot
{
public:
    ComputePlotBar(std::shared_ptr<ComputeDataProvider2> data_provider, rocprofvis_controller_compute_plot_types_t type);
    void Render() override;
};

class ComputeMetric : public ComputeWidget
{
public:
    ComputeMetric(std::shared_ptr<ComputeDataProvider2> data_provider, rocprofvis_controller_compute_metric_types_t type, const std::string& label, const std::string& unit);
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

class ComputePlotRoofline : public ComputePlot
{
public:
    enum GroupMode {
        GroupModeKernel,
        GroupModeDispatch
    };
    ComputePlotRoofline(std::shared_ptr<ComputeDataProvider> data_provider, rocprofvis_controller_compute_plot_types_t type);
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

}  // namespace View
}  // namespace RocProfVis
