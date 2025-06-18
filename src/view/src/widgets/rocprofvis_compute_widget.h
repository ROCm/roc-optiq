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

}  // namespace View
}  // namespace RocProfVis
