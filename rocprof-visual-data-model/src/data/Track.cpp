#include "Track.h"
#include <cassert>

Track::Track(Trace* ctx, TrackType type, uint64_t numRecords) : m_ctx(ctx), m_type(type), m_numRecords(numRecords) {};

TrackType Track::getType() {
    return m_type;
}

const char* Track::getGroupName() {
    return m_group_name.c_str();
}

const char* Track::getName() {
    return m_name.c_str();
}

Trace* Track::getTraceCtx() {
    return m_ctx;
}

uint32_t Track::addFlowRecord(FlowRecord & data) {
    return INVALID_RECORD_INDEX;
}

DataFlowRecord* Track::getFlowRecordAt(uint64_t id)
{
    assert(false && "Only Event track may have data flow records");
    return 0;
}

size_t Track::getFlowArrayMemoryFootprint() {
    assert(false && "Only Event tracks have flow records");
    return 0;
}

size_t Track::getNumberOfFlowRecords() {
    assert(false && "Only Event tracks have flow records");
    return 0;
}

uint64_t TrackArray::getRecordIdAt(const int index)
{
    assert(false && "Only Event record has duration property");
    return 0;
}

double TrackArray::getRecordValueAt(const int index)
{
    assert(false && "Only Metric record has value property");
    return 0;
}

int64_t TrackArray::getRecordDurationAt(const int index)
{
    assert(false && "Only Event record has duration property");
    return 0;
}

const char* TrackArray::getRecordTypeNameAt(const int index)
{
    assert(false && "Only Event record has duration property event type property");
    return nullptr;
}

const char* TrackArray::getRecordDescriptionAt(const int index)
{
    assert(false && "Only Event record has duration property event description property");
    return nullptr;
}


size_t Track::getTrackMemoryFootprint() 
{
    size_t memoryFootprint = 0;
    for (int i = 0; i < m_chunks.size(); i++)
    {
        memoryFootprint += m_chunks[i]->getRecordArrayMemoryFootprint();
    }
    return memoryFootprint;
}

TrackArray* Track::getTrackArrayAtTimestamp(uint64_t timestamp) 
{
    for (int i = 0; i < m_chunks.size(); i++)
    {
        if (m_chunks[i]->getRequestedStartTime() == timestamp)
        {
            return m_chunks[i];
        }
    }
    return nullptr;
    
}

void Track::deleteTraceChunk(TrackArray* chunkPtr)
{
    for (int i = 0; i < m_chunks.size(); i++)
    {
        if (chunkPtr == m_chunks[i])
        {
            delete m_chunks[i];
            m_chunks.erase(m_chunks.begin()+i);
            return;
        }
    }
    assert(false && "Track array doesn't have element with provided address");
}

TrackArray* Track::getTrackLastChunk()
{
    if (m_chunks.size() > 0)
    {
        size_t i = m_chunks.size()-1;
        return m_chunks[i];
    }
    assert (false && "No track array hasn't been created");
    return nullptr;
}