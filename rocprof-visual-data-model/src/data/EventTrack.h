#ifndef EVENT_TRACK__H
#define EVENT_TRACK__H

#include "Track.h"
#include "EventRecord.h"
#include <string>
#include <vector>

class Trace;


class EventArray : public TrackArray {
    public:
        EventArray( Track* ctx, uint64_t start, uint64_t end) : TrackArray(ctx, start, end) {}; 
        uint64_t            addRecord(DataRecord & data) override;
        size_t              getRecordArrayMemoryFootprint() override;
        size_t              getNumberOfRecords() override {return m_samples.size();}
        uint32_t            timestampToIndex(const uint64_t timestamp) override;

        uint64_t            getRecordIdAt(const int index) override;
        uint64_t            getRecordTimestampAt(const int index) override;
        int64_t             getRecordDurationAt(const int index) override;
        const char*         getRecordTypeNameAt(const int index) override;
        const char*         getRecordDescriptionAt(const int index) override;

    private:

        std::vector<EventRecord> m_samples;

};

class EventTrack : public Track {
    public:
        EventTrack( Trace* ctx, TrackType type, uint64_t numEvents) : Track(ctx,type,numEvents) {}; 
        TrackArray*         addTrackArray(uint64_t startTimestamp, uint64_t endTimestamp) override;

        size_t              getNumberOfFlowRecords() override {return m_flowMap.size();}
        size_t              getFlowArrayMemoryFootprint() override;
        DataFlowRecord*     getFlowRecordAt(uint64_t id) override;

    private:


    protected:

        DataFlowMap m_flowMap;
};

#endif //EVENT_TRACK__H