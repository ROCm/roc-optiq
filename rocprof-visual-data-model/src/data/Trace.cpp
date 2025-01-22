#include "Trace.h"
#include "../database/Database.h"
#include <algorithm>
#include <cassert>

TraceInternal::TraceInternal()
{
    m_minTime = std::numeric_limits<uint64_t>::max();
    m_maxTime = std::numeric_limits<uint64_t>::min();
    m_stringArrayMemoryFootprint=0;
    m_db = nullptr;
}

uint32_t TraceInternal::addTrack(const void* handler, TrackProperties & props,  uint32_t size)
{
    Trace* trace = (Trace*)handler;
    try{
        if (props.type == RecordType::METRICS) {
            assert(trace->m_tracks.size() < INVALID_TRACK_SIZE);
            trace->m_tracks.push_back(std::make_unique<MetricTrack>(trace,props.data.Metric.gpuId,props.data.Metric.metricId,size));
        } else
        if (props.type == RecordType::CPU) {
            assert(trace->m_tracks.size() < INVALID_TRACK_SIZE);
            trace->m_tracks.push_back(std::make_unique<CpuTrack>(trace,props.data.Cpu.processId,props.data.Cpu.threadId,size));
        } else   
        if (props.type == RecordType::GPU) {
            assert(trace->m_tracks.size() < INVALID_TRACK_SIZE);
            trace->m_tracks.push_back(std::make_unique<GpuTrack>(trace,props.data.Gpu.gpuId,props.data.Gpu.queueId,size));
        }    
    }
    catch (std::exception ex)
    {
        return INVALID_TRACK_INDEX;
    }
    return (uint32_t)trace->m_tracks.size()-1;
}


void* TraceInternal::addTrackArray(const void* handler, const uint16_t trackId, uint64_t startTime, uint64_t endTime)
{
    Trace* trace = (Trace*)handler;
    assert(trackId < trace->m_tracks.size());
    return trace->m_tracks[trackId].get()->addTrackArray(startTime,endTime);
}

uint64_t TraceInternal::addTrackRecord(const void* handler, uint16_t trackId, RecordType type, DataRecord & data)
{
    Trace* trace = (Trace*)handler;
    assert(trackId < trace->m_tracks.size());
    if (type == trace->m_tracks[trackId].get()->getType())
    {  
        TrackArray* array = trace->m_tracks[trackId].get()->getTrackLastChunk();
        if (array != nullptr)
            return array->addRecord(data);
    }
    return INVALID_RECORD_INDEX;
}


uint32_t TraceInternal::addStringRecord(const void* handler,  const char* stringValue)
{
    Trace* trace = (Trace*)handler;
    try {
        trace->m_strings.push_back(stringValue);
        trace->m_stringArrayMemoryFootprint+=strlen(stringValue);
    }
    catch(std::exception ex)
    {
        return INVALID_RECORD_INDEX;
    }
    return (uint32_t)trace->m_strings.size()-1;
}

uint32_t TraceInternal::addFlowRecord(const void* handler, const uint16_t trackId, FlowRecord & data)
{
    Trace* trace = (Trace*)handler;
    return trace->m_tracks[trackId].get()->addFlowRecord(data);
}

void TraceInternal::setTraceParameters(const void* handler, const uint64_t minTime, const uint64_t maxTime, const size_t stringListSize)
{
    Trace* trace = (Trace*)handler;
    trace->m_minTime = minTime;
    trace->m_maxTime = maxTime;
    trace->m_strings.reserve(stringListSize);
}

const char* TraceInternal::getStringAt(uint32_t index)
{
    return (index < m_strings.size() ? m_strings[index].c_str() : "");
}

bool Trace::bindDatabase(Database* db)
{
    assert(db && "Database context cannot be NULL");
    DbBindSctucture bindData = {0}; 
    bindData.handler=this;
    bindData.funcAddTrack = addTrack;
    bindData.funcAddTrackRecord = addTrackRecord;
    bindData.funcAddTrackArray = addTrackArray;
    bindData.funcAddStringRecord = addStringRecord;
    bindData.funcAddFlowRecord = addFlowRecord;
    bindData.funcSetTraceParameters = setTraceParameters;
    db->bindTrace(bindData);
    m_db = db;
    return true;
}

void   Trace::deleteTraceChunksAt(uint64_t timestamp)
{
    for (int i=0; i < m_tracks.size(); i++)
    {
        TrackArray* arrayPtr = m_tracks[i].get()->getTrackArrayAtTimestamp(timestamp);
        if (arrayPtr!=nullptr)
        {
            m_tracks[i].get()->deleteTraceChunk(arrayPtr);
        }
    }
}

bool  Trace::validateTrackHandler(void* handler)
{
    for (int i=0; i < m_tracks.size(); i++)
    {
        if (m_tracks[i].get() == handler) return true;
    }
    return false;
}

