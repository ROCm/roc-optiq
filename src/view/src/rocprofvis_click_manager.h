// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
namespace RocProfVis
{
namespace View
{

enum class Layer
{
    kNone = -1,
    kGraphLayer,
    kInteractiveLayer,
    kScrubberLayer,
    kCount
};

enum class MeasurementState
{
    kInactive,
    kWaitingForFirst,
    kWaitingForSecond,
    kComplete
};

struct MeasurementPoint
{
    double      timestamp = 0.0;
    double      duration  = 0.0;
    uint64_t    track_id  = 0;
    uint32_t    level     = 0;
    std::string name;
    bool        valid = false;
};

class TimelineFocusManager
{
public:
    static TimelineFocusManager& GetInstance();

    void  RequestLayerFocus(Layer layer);
    Layer GetFocusedLayer() const;
    Layer EvaluateFocusedLayer();

    // Right-click context menu channel - persists until explicitly cleared
    void  SetRightClickLayer(Layer layer);
    Layer GetRightClickLayer() const;
    void  ClearRightClickLayer();

    // Measurement mode
    void             EnterMeasurementMode();
    void             ExitMeasurementMode();
    bool             IsMeasurementMode() const;
    MeasurementState GetMeasurementState() const;
    void             SetMeasurementPoint(double timestamp, double duration,
                                         uint64_t track_id, uint32_t level,
                                         const std::string& name);
    void                    ClearMeasurement();
    const MeasurementPoint& GetMeasurementPoint1() const;
    const MeasurementPoint& GetMeasurementPoint2() const;

private:
    TimelineFocusManager();
    ~TimelineFocusManager()                              = default;
    TimelineFocusManager(const TimelineFocusManager&)            = delete;
    TimelineFocusManager& operator=(const TimelineFocusManager&) = delete;

    Layer                 m_layer_focused;
    std::map<Layer, bool> m_all_layers_focused;
    Layer                 m_right_click_layer;

    MeasurementState m_measurement_state;
    MeasurementPoint m_measurement_point1;
    MeasurementPoint m_measurement_point2;
};

}  // namespace View
}  // namespace RocProfVis
