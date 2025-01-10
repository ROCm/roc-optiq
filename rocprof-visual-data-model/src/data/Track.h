#ifndef TRACK__H
#define TRACK__H

#include "TraceInterface.h"
#include "TraceTypes.h"
#include "DataFlow.h"
#include <vector>


class Track;
class Trace;


class TrackArray  {
    public:
        TrackArray( Track* ctx, uint64_t start, uint64_t end) : m_ctx(ctx), m_requestedStartTimestamp(start), m_requestedEndTimestamp(end) {}; 

        Track*                      getTrackCtx() {return m_ctx;};
        uint64_t                    getRequestedStartTime() {return m_requestedStartTimestamp;}
        uint64_t                    getRequestedEndTime() {return m_requestedEndTimestamp;}

        virtual uint64_t            addRecord(DataRecord & data) = 0;
        virtual size_t              getRecordArrayMemoryFootprint() = 0;
        virtual size_t              getNumberOfRecords() = 0;
        virtual uint32_t            timestampToIndex(const uint64_t timestamp)=0;

        virtual uint64_t            getRecordTimestampAt(const int index) = 0;
        virtual uint64_t            getRecordIdAt(const int index);
        virtual double              getRecordValueAt(const int index);
        virtual int64_t             getRecordDurationAt(const int index);
        virtual const char*         getRecordTypeNameAt(const int index);
        virtual const char*         getRecordDescriptionAt(const int index);

    private:

        uint64_t                    m_requestedStartTimestamp;
        uint64_t                    m_requestedEndTimestamp;
        Track*                      m_ctx;
};

class Track {
    public:
        Track(Trace* ctx, TrackType type, uint64_t numRecords);

        TrackType                   getType();
        const char*                 getGroupName();
        const char*                 getName();
        
        Trace*                      getTraceCtx();

        virtual TrackArray*         addTrackArray(uint64_t startTimestamp, uint64_t endTimestamp) = 0;

        size_t                      getTrackMemoryFootprint();
        size_t                      getNumberOfChunks() {return m_chunks.size();}
        TrackArray*                 getTrackArrayAtTimestamp(uint64_t timestamp);
        TrackArray*                 getTrackLastChunk();
        void                        deleteTraceChunk(TrackArray*);

        virtual uint32_t            addFlowRecord(FlowRecord & data);
        virtual DataFlowRecord*     getFlowRecordAt(uint64_t id);
        virtual size_t              getFlowArrayMemoryFootprint();
        virtual size_t              getNumberOfFlowRecords();

    protected:    
        Trace*                      m_ctx;
        
        TrackType                   m_type;
        std::string                 m_group_name;
        std::string                 m_name;

        std::vector<TrackArray*>    m_chunks;
        uint64_t                    m_numRecords;

};

#endif //TRACK__H