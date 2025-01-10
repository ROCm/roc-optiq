#include "CpuTrack.h"
#include "EventRecord.h"
#include <cassert>

CpuTrack::CpuTrack( Trace* ctx,
                    const uint32_t processId, 
                    const uint32_t threadId,
                    const uint64_t numEvents):
                    EventTrack(ctx, TrackType::CPU, numEvents),
                    m_processId(processId),
                    m_threadId(threadId)  {

    m_group_name = "PID " + std::to_string(m_processId);
    m_name = "TID " + std::to_string(m_threadId);
};


uint32_t CpuTrack::addFlowRecord(FlowRecord & data)
{
    if(!m_flowMap.empty())
    {
        DataFlowMap::iterator lastRecord = std::prev(m_flowMap.end());
        if (lastRecord->first == data.recordId)
        {          
            assert(lastRecord->second.getTrackId() == data.flowId.trackId && "HipGraph cannot be executed by different GPUs");
            lastRecord->second.setNextTargetTimestamp(data.flowId.timestamp);
            return (uint32_t)m_flowMap.size()-1;
        } 
    }
    std::pair<DataFlowMap::iterator,bool> ret = m_flowMap.insert(DataFlowPair(data.recordId, DataFlowRecord(data.flowId.trackId, TrackType::GPU, data.flowId.recordId, data.flowId.timestamp)));
    assert(ret.second == true && "Duplicated IDs are not allowed");
    return (uint32_t)m_flowMap.size()-1;
}




