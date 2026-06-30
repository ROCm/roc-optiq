// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_measurement_controller.h"

namespace RocProfVis
{
namespace View
{

MeasurementController::MeasurementController()
: m_measurement_state(MeasurementState::kInactive)
, m_measurements()
, m_freehand_mode(false)
{}

void
MeasurementController::EnterMeasurementMode()
{
    // Resume based on the most recent measurement: a complete one keeps showing and
    // the next click starts a new measurement; a partial one waits for its end.
    if(m_measurements.empty())
        m_measurement_state = MeasurementState::kWaitingForFirst;
    else if(m_measurements.back().Complete())
        m_measurement_state = MeasurementState::kComplete;
    else
        m_measurement_state = MeasurementState::kWaitingForSecond;
}

void
MeasurementController::ExitMeasurementMode()
{
    m_measurement_state = MeasurementState::kInactive;
}

bool
MeasurementController::IsMeasurementMode() const
{
    return m_measurement_state != MeasurementState::kInactive;
}

MeasurementState
MeasurementController::GetMeasurementState() const
{
    return m_measurement_state;
}

Measurement*
MeasurementController::BeginMeasurement()
{
    bool needs_new_slot = m_measurements.empty() || m_measurements.back().Complete();
    if(needs_new_slot)
    {
        if(static_cast<int>(m_measurements.size()) >= MAX_MEASUREMENTS)
        {
            return nullptr;
        }
        m_measurements.emplace_back();
    }
    else
    {
        // Restart the in-progress (partial) active measurement in place.
        m_measurements.back() = Measurement{};
    }
    return &m_measurements.back();
}

bool
MeasurementController::AddEventPoint(double timestamp, double duration, uint64_t track_id,
                                    uint32_t level, const std::string& name,
                                    uint64_t event_uuid)
{
    if(m_measurement_state == MeasurementState::kWaitingForSecond &&
       !m_measurements.empty())
    {
        m_measurements.back().points[1] =
            { timestamp, duration, track_id, level, name, event_uuid, true, false };
        m_measurement_state = MeasurementState::kComplete;
        return true;
    }

    Measurement* measurement = BeginMeasurement();
    if(measurement == nullptr)
    {
        return false;
    }
    measurement->points[0] =
        { timestamp, duration, track_id, level, name, event_uuid, true, false };
    m_measurement_state = MeasurementState::kWaitingForSecond;
    return true;
}

bool
MeasurementController::AddFreehandPoint(double timestamp)
{
    MeasurementPoint point;
    point.timestamp = timestamp;
    point.valid     = true;
    point.freehand  = true;

    if(m_measurement_state == MeasurementState::kWaitingForSecond &&
       !m_measurements.empty())
    {
        m_measurements.back().points[1]           = point;
        m_measurements.back().freehand_offsets[1] = 0.0;
        m_measurement_state                       = MeasurementState::kComplete;
        return true;
    }

    Measurement* measurement = BeginMeasurement();
    if(measurement == nullptr)
    {
        return false;
    }
    measurement->points[0]           = point;
    measurement->freehand_offsets[0] = 0.0;
    m_measurement_state              = MeasurementState::kWaitingForSecond;
    return true;
}

void
MeasurementController::ClearActiveMeasurement()
{
    // Drop the in-progress measurement (the last slot) when it is partial, leaving
    // any completed measurements intact.
    if(!m_measurements.empty() && !m_measurements.back().Complete())
        m_measurements.pop_back();

    if(m_measurement_state != MeasurementState::kInactive)
        m_measurement_state = m_measurements.empty() || m_measurements.back().Complete()
                                  ? MeasurementState::kWaitingForFirst
                                  : MeasurementState::kWaitingForSecond;
}

void
MeasurementController::ClearAll()
{
    bool was_active = m_measurement_state != MeasurementState::kInactive;
    m_measurements.clear();
    m_measurement_state =
        was_active ? MeasurementState::kWaitingForFirst : MeasurementState::kInactive;
}

const MeasurementPoint&
MeasurementController::GetPoint(int index) const
{
    static const MeasurementPoint empty{};
    if(m_measurements.empty()) return empty;
    return m_measurements.back().points[index & 1];
}

MeasureEdge
MeasurementController::GetEdge(int index) const
{
    if(m_measurements.empty()) return MeasureEdge::kStart;
    return m_measurements.back().edges[index & 1];
}

void
MeasurementController::SetEdge(int index, MeasureEdge edge)
{
    if(m_measurements.empty()) return;
    m_measurements.back().edges[index & 1]            = edge;
    m_measurements.back().freehand_offsets[index & 1] = 0.0;
}

double
MeasurementController::GetEffectiveTimestamp(int index) const
{
    if(m_measurements.empty()) return 0.0;
    return EffectiveTimestamp(m_measurements.back(), index);
}

double
MeasurementController::GetFreehandOffset(int index) const
{
    if(m_measurements.empty()) return 0.0;
    return m_measurements.back().freehand_offsets[index & 1];
}

void
MeasurementController::SetFreehandOffset(int index, double offset)
{
    if(m_measurements.empty()) return;
    m_measurements.back().freehand_offsets[index & 1] = offset;
}

void   MeasurementController::SetFreehandMode(bool enabled) { m_freehand_mode = enabled; }
bool   MeasurementController::IsFreehandMode() const        { return m_freehand_mode; }

int
MeasurementController::GetMeasurementCount() const
{
    return static_cast<int>(m_measurements.size());
}

const Measurement&
MeasurementController::GetMeasurement(int index) const
{
    static const Measurement empty{};
    if(index < 0 || index >= static_cast<int>(m_measurements.size())) return empty;
    return m_measurements[index];
}

const std::vector<Measurement>&
MeasurementController::Measurements() const
{
    return m_measurements;
}

void
MeasurementController::AddRestoredMeasurement(const Measurement& measurement)
{
    if(static_cast<int>(m_measurements.size()) >= MAX_MEASUREMENTS) return;
    if(!measurement.Complete()) return;
    m_measurements.push_back(measurement);
}

double
MeasurementController::EffectiveTimestamp(const Measurement& measurement, int index)
{
    int                     i  = index & 1;
    const MeasurementPoint& pt = measurement.points[i];
    if(!pt.valid) return 0.0;
    if(pt.freehand) return pt.timestamp + measurement.freehand_offsets[i];

    double base = (measurement.edges[i] == MeasureEdge::kStart)
                      ? pt.timestamp
                      : pt.timestamp + pt.duration;
    return base + measurement.freehand_offsets[i];
}

}  // namespace View
}  // namespace RocProfVis
