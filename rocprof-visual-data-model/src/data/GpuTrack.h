#ifndef GPU_TRACK__H
#define GPU_TRACK__H

#include "EventTrack.h"


class GpuTrack : public EventTrack {
    public:
        GpuTrack(   Trace* ctx,
                    const uint32_t gpuId, 
                    const uint32_t queueId,
                    const uint64_t numEvents );

        uint32_t    addFlowRecord(FlowRecord & data) override; 

    private:
        uint32_t    m_gpuId; 
        uint32_t    m_queueId; 


};

#endif //GPU_TRACK__H