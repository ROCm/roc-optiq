// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

namespace RocProfVis
{
namespace View
{
class TimeToPixelManager
{
public:
    TimeToPixelManager();
    ~TimeToPixelManager();
    void ComputePixelsPerNs(double min_x, double max_x, float zoom, float graph_size_x,
                            double view_time_offset_ns);

private:
    double m_min_x;
    double m_max_x;
    double m_v_min_x;
    double m_v_max_x;
    double m_view_time_offset_ns;
    double m_v_width;
    float  m_zoom;
    double m_range_x;
    float  m_graph_size_x;
    double m_pixels_per_ns;
};
}  // namespace View
}  // namespace RocProfVis
