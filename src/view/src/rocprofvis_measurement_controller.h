// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include <cstdint>
#include <string>

namespace RocProfVis
{
namespace View
{

enum class MeasurementState
{
    kInactive,
    kWaitingForFirst,
    kWaitingForSecond,
    kComplete
};

enum class MeasureEdge
{
    kStart,
    kEnd
};

struct MeasurementPoint
{
    double      timestamp = 0.0;
    double      duration  = 0.0;
    uint64_t    track_id  = 0;
    uint32_t    level     = 0;
    std::string name;
    uint64_t    event_uuid = 0;
    bool        valid      = false;
    bool        freehand   = false;
};

class MeasurementController
{
public:
    MeasurementController();
    ~MeasurementController()                                       = default;
    MeasurementController(const MeasurementController&)            = delete;
    MeasurementController& operator=(const MeasurementController&) = delete;

    void             EnterMeasurementMode();
    void             ExitMeasurementMode();
    bool             IsMeasurementMode() const;
    MeasurementState GetMeasurementState() const;

    void SetMeasurementPoint(double timestamp, double duration,
                             uint64_t track_id, uint32_t level,
                             const std::string& name, uint64_t event_uuid);
    void SetFreehandMeasurementPoint(double timestamp);
    void ClearMeasurement();

    const MeasurementPoint& GetPoint(int index) const;
    MeasureEdge             GetEdge(int index) const;
    void                    SetEdge(int index, MeasureEdge edge);
    double                  GetEffectiveTimestamp(int index) const;

    void   SetFreehandMode(bool enabled);
    bool   IsFreehandMode() const;
    void   SetFreehandOffset(int index, double offset_ns);
    double GetFreehandOffset(int index) const;

private:
    MeasurementState m_measurement_state;
    MeasurementPoint m_points[2];
    MeasureEdge      m_edges[2];
    bool             m_freehand_mode;
    double           m_freehand_offsets[2];
};

}  // namespace View
}  // namespace RocProfVis
