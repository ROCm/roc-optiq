#include "EventTrack.h"
#include <cassert>

uint64_t EventArray::addRecord(DataRecord & data) 
{
    try {
        if (getTrackCtx()->getType() == TrackType::CPU) {
            m_samples.push_back(EventRecord(data.Cpu.id, data.Cpu.timestamp, data.Cpu.duration, data.Cpu.apiCallId, data.Cpu.argumentsId)); 
        } else
        if (getTrackCtx()->getType()  == TrackType::GPU) {
            m_samples.push_back(EventRecord(data.Gpu.id, data.Gpu.timestamp, data.Gpu.duration, data.Gpu.typeId, data.Gpu.descriptionId)); 
        }
    }
    catch(std::exception ex)
    {
        return INVALID_RECORD_ID;
    }
    return m_samples.back().getTimestamp();
}

size_t  EventArray::getRecordArrayMemoryFootprint()
{
    return sizeof(std::vector<EventRecord>) + (sizeof(EventRecord) * m_samples.size());
}

uint32_t EventArray::timestampToIndex(const uint64_t timestamp)
{
    std::vector<EventRecord>::iterator it = std::find_if( m_samples.begin(), m_samples.end(),
            [&timestamp](EventRecord & x) { return x.getTimestamp() >= timestamp;});
    if (it != m_samples.end())
    {
        return (uint32_t)(it - m_samples.begin());
    }
    return INVALID_RECORD_INDEX;
            
}

uint64_t    EventArray::getRecordIdAt(const int index) {
    assert(index < m_samples.size() && "Index out of range");
    if (index < m_samples.size())
    {
        return m_samples[index].getId();
    } 
    return 0;
}

uint64_t    EventArray::getRecordTimestampAt(const int index) {
    assert(index < m_samples.size() && "Index out of range");
    if (index < m_samples.size())
    {
        return m_samples[index].getTimestamp();
    } 
    return 0;
}

int64_t     EventArray::getRecordDurationAt(const int index) {
    assert(index < m_samples.size() && "Index out of range");
    if (index < m_samples.size())
    {
        return m_samples[index].getDuration();
    } 
    return 0;
}

const char* EventArray::getRecordTypeNameAt(const int index) {
    assert(index < m_samples.size() && "Index out of range");
    if (index < m_samples.size())
    {
        return m_samples[index].getTypeName(getTrackCtx()->getTraceCtx());
    } 
    return 0;
}

const char* EventArray::getRecordDescriptionAt(const int index) {
    assert(index < m_samples.size() && "Index out of range");
    if (index < m_samples.size())
    {
        return m_samples[index].getDescription(getTrackCtx()->getTraceCtx());
    }
    return 0;
}


DataFlowRecord*  EventTrack::getFlowRecordAt(uint64_t id)
{
    DataFlowMap::iterator record = m_flowMap.find(id);
    if (record != m_flowMap.end())
    {
        return &record->second;
    }
    return nullptr;
}


size_t   EventTrack::getFlowArrayMemoryFootprint()
{
    return sizeof(DataFlowMap) + m_flowMap.size() * sizeof(DataFlowPair);
}

TrackArray*   EventTrack::addTrackArray(uint64_t startTimestamp, uint64_t endTimestamp)
{
    EventArray* array =  new EventArray(this,startTimestamp, endTimestamp);
    m_chunks.push_back(array);
    return array;
}