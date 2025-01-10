#ifndef TRACE__H
#define TRACE__H

#include "MetricTrack.h"
#include "GpuTrack.h"
#include "CpuTrack.h"
#include <memory>

class Database;

class TraceInternal{
    public:
        TraceInternal();
        const char* getStringAt(uint32_t index);
        size_t getStringArrayMemoryFootprint() {return m_stringArrayMemoryFootprint;};
        size_t getStringItemCount() {return m_strings.size();};

    protected:
        
        static uint32_t addTrack(const void* handler, TrackProperties & data,  uint32_t size);
        static uint64_t addTrackRecord(const void* handler, const uint16_t trackId, const RecordType type, DataRecord & data);
        static void*    addTrackArray(const void* handler, const uint16_t trackId, uint64_t startTime, uint64_t endTime);
        static uint32_t addStringRecord(const void* handler,  const char* stringValue);
        static uint32_t addFlowRecord(const void* handler, const uint16_t trackId, FlowRecord & data);
        static void setTraceParameters(const void* handler, const uint64_t minTime, const uint64_t maxTime, const size_t stringListSize);

        std::vector<std::unique_ptr<Track>> m_tracks;
        uint64_t                            m_minTime;
        uint64_t                            m_maxTime;

    private:
        std::vector<std::string>            m_strings;
        size_t                              m_stringArrayMemoryFootprint;
};

class Trace : public TraceInternal {
    public:
        bool        bindDatabase(Database* db); 
        size_t      getNumberOfTracks() {return m_tracks.size();};
        Track*      getTrackAt(uint32_t index) {return (index > m_tracks.size() ? nullptr : m_tracks[index].get());};
        uint64_t    getMinTime() {return m_minTime;};
        uint64_t    getMaxTime() {return m_maxTime;};
        void        deleteTraceChunksAt(uint64_t timestamp);

};


#endif //TRACE__H