#include "GpuTrack.h"
#include "DataFlow.h"
#include "EventRecord.h"
#include "cassert"

GpuTrack::GpuTrack( Trace* ctx, 
                    const uint32_t gpuId, 
                    const uint32_t queueId,
                    const uint64_t numEvents):
                    EventTrack(ctx, TrackType::GPU, numEvents),
                    m_gpuId(gpuId),
                    m_queueId(queueId)  {

    m_group_name = "GPU " + std::to_string(m_gpuId);
    m_name = "Queue " + std::to_string(m_queueId);
                    
}

uint32_t GpuTrack::addFlowRecord(FlowRecord & data)
{
    std::pair<DataFlowMap::iterator,bool> ret = m_flowMap.insert(DataFlowPair(data.recordId, DataFlowRecord(data.flowId.trackId, TrackType::CPU, data.flowId.recordId, data.flowId.timestamp)));
    assert(ret.second == true && "Duplicated IDs are not allowed");
    return (uint32_t)m_flowMap.size()-1;
}



