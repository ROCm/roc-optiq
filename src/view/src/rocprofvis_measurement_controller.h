// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include <cstdint>
#include <string>
#include <vector>

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

// One measurement is a pair of ruler points plus the per-ruler edge snap and
// freehand drag offset. The effective timestamp of a ruler is derived from these.
struct Measurement
{
    MeasurementPoint points[2];
    MeasureEdge      edges[2]            = { MeasureEdge::kStart, MeasureEdge::kStart };
    double           freehand_offsets[2] = { 0.0, 0.0 };

    bool Complete() const { return points[0].valid && points[1].valid; }
};

class MeasurementController
{
public:
    // Upper bound on how many measurements can exist at once. Kept small so the
    // timeline overlay and toolbar stay readable.
    static constexpr int MAX_MEASUREMENTS = 5;

    MeasurementController();
    ~MeasurementController()                                       = default;
    MeasurementController(const MeasurementController&)            = delete;
    MeasurementController& operator=(const MeasurementController&) = delete;

    void             EnterMeasurementMode();
    void             ExitMeasurementMode();
    bool             IsMeasurementMode() const;
    MeasurementState GetMeasurementState() const;

    // Place the next ruler. When the active measurement is already complete a new
    // measurement is started. Returns false (placing nothing) when starting a new
    // measurement would exceed MAX_MEASUREMENTS.
    bool AddEventPoint(double timestamp, double duration, uint64_t track_id,
                       uint32_t level, const std::string& name, uint64_t event_uuid);
    bool AddFreehandPoint(double timestamp);

    // Clears the in-progress (active) measurement only, staying in measure mode.
    void ClearActiveMeasurement();
    // Removes every measurement.
    void ClearAll();

    // Accessors for the active (most recently edited) measurement. These back the
    // ruler-drag and edge-snap interactions in the timeline/toolbar.
    const MeasurementPoint& GetPoint(int index) const;
    MeasureEdge             GetEdge(int index) const;
    void                    SetEdge(int index, MeasureEdge edge);
    double                  GetEffectiveTimestamp(int index) const;
    double                  GetFreehandOffset(int index) const;
    void                    SetFreehandOffset(int index, double offset_ns);

    void   SetFreehandMode(bool enabled);
    bool   IsFreehandMode() const;

    // Collection access used by rendering, copy, and persistence.
    int                            GetMeasurementCount() const;
    const Measurement&             GetMeasurement(int index) const;
    const std::vector<Measurement>& Measurements() const;
    // Appends a fully formed measurement (used when restoring from a project file).
    void                           AddRestoredMeasurement(const Measurement& measurement);

    // Computes the on-screen timestamp of one ruler, honoring edge snap and offset.
    static double EffectiveTimestamp(const Measurement& measurement, int index);

private:
    // Returns a clean slot ready to receive a first ruler, or nullptr when the
    // measurement cap has been reached. Recycles the active slot if it is still
    // in progress rather than growing the list.
    Measurement* BeginMeasurement();

    MeasurementState         m_measurement_state;
    std::vector<Measurement> m_measurements;
    bool                     m_freehand_mode;
};

}  // namespace View
}  // namespace RocProfVis
