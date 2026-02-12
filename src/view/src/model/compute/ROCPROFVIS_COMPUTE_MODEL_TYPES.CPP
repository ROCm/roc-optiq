#include "rocprofvis_compute_model_types.h"

namespace RocProfVis
{
namespace View
{
ChartData
WorkloadInfo::GenerateChartData() const
{
    ChartData data;
    double    total = 0.0;
    for(const auto& kernel : kernels)
    {
        data.m_labels.push_back(kernel.second.name.c_str());
        data.m_x_values.push_back(static_cast<double>(kernel.second.duration_total));
        data.m_y_values.push_back(static_cast<double>(data.m_y_values.size()));
        data.m_x_max = std::max(data.m_x_max, data.m_x_values.back());
        total += data.m_x_values.back();
    }

    data.m_fractions.resize(
        data.m_x_values
            .size());  // TODO: It need only for PieChart thing how to read of there
    for(size_t i = 0; i < data.m_x_values.size(); ++i)
    {
        data.m_fractions[i] = data.m_x_values[i] / total;
    }

    return data;
};

}
}
