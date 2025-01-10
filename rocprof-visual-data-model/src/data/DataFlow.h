#ifndef DATAFLOW__H
#define DATAFLOW__H

#include "TraceTypes.h"
#include <map>

typedef union{
    struct {
        uint64_t lastTimestamp;
    } GPU;
    struct {
        uint64_t endpointId;
    } CPU;
    uint64_t value;
} EndpointInfo;

class DataFlowRecord {
    public:
        DataFlowRecord(uint16_t targetTrackId, TrackType targetType, uint64_t targetId, uint64_t targetTimestamp):
                                    m_trackId(targetTrackId), m_timestamp(targetTimestamp) {targetType == TrackType::CPU ? m_endpointInfo.CPU.endpointId = targetId : m_endpointInfo.GPU.lastTimestamp=targetTimestamp;};
        uint16_t getTrackId() {return m_trackId;}
        uint64_t getEndpointTimeStamp() {return m_timestamp;};
        EndpointInfo getEndpointInfo() {return m_endpointInfo;};
        void setNextTargetTimestamp(uint64_t targetTimestamp) {m_endpointInfo.GPU.lastTimestamp=targetTimestamp;}
    
    private:

        //timestamp is needed anyways to load proper chunk of data that get pointed.
        //it can also serve as event identification for GPU endpoint. Kernel execution on GPU cannot have multiple records with the same timestamp for a single queue.
        uint64_t m_timestamp;

        //For GPU endpoint, using another timestamp to point the end of HipGraph execution. All records in the queue between two timestamps will belong to the HipGraph
        //For CPU endpoint we cannot rely on timestamp as only identification, because multiple records of the same thread can hipotetically have same timestamp 
        EndpointInfo m_endpointInfo;

        uint16_t m_trackId;
};

typedef std::map<uint64_t, DataFlowRecord> DataFlowMap;
typedef std::pair<uint64_t, DataFlowRecord> DataFlowPair;

#endif // DATAFLOW__H