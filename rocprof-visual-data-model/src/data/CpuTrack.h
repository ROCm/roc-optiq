#ifndef CPU_TRACK__H
#define CPU_TRACK__H

#include "EventTrack.h"


class CpuTrack : public EventTrack {
    public:
        CpuTrack(   Trace* ctx, 
                    const uint32_t processId, 
                    const uint32_t threadId,
                    const uint64_t numEvents);

        uint32_t    addFlowRecord(FlowRecord & data) override;  

    private:
        uint32_t    m_processId; 
        uint32_t    m_threadId; 

};

#endif //CPU_TRACK__H