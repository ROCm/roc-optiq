#ifndef METRIC_TRACK__H
#define METRIC_TRACK__H

#include "Track.h"
#include "MetricRecord.h"
#include <string>
#include <vector>

class Trace;
class Track;


class MetricArray : public TrackArray {
    public:
        MetricArray( Track* ctx, uint64_t start, uint64_t end) : TrackArray(ctx, start, end) {}; 
        uint64_t            addRecord(DataRecord & data) override;
        size_t              getRecordArrayMemoryFootprint() override;
        size_t              getNumberOfRecords() override {return m_samples.size();}
        uint32_t            timestampToIndex(const uint64_t timestamp) override;

        uint64_t            getRecordTimestampAt(const int index) override;
        double              getRecordValueAt(const int index) override;

    private:

        std::vector<MetricRecord> m_samples;

};

class MetricTrack : public Track {
    public:
        MetricTrack( Trace* ctx, 
                const uint32_t gpuId, 
                const uint32_t metricId,
                const uint64_t numRecords); 

        TrackArray*             addTrackArray(uint64_t startTimestamp, uint64_t endTimestamp) override;


    private:
        uint32_t                m_gpuId; 
        uint32_t                m_metricId; 

};

#endif //METRIC_TRACK__H