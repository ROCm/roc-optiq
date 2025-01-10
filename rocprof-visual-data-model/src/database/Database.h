#ifndef DATABASE_H
#define DATABASE_H

#include <cstdint>
#include <string>
#include <vector>
#include "../data/TraceTypes.h"


typedef void (*DbReadProgress)(const int, const char*);

typedef struct 
{
        void* handler;
        addTrack funcAddTrack;
        addTrackRecord funcAddTrackRecord;
        addTrackArray funcAddTrackArray;
        addStringRecord funcAddStringRecord;
        addFlowRecord funcAddFlowRecord;
        setTraceParameters funcSetTraceParameters;   
} DbBindSctucture;

typedef struct
{
    uint64_t  startTime;
    uint64_t  endTime;
    std::vector<uint16_t> tracks;
} TraceReadConfig;

class Database
{
    public:
        Database(const char* path);
        virtual bool open() = 0;
        virtual void close() = 0;
        virtual bool isOpen() = 0; 
        virtual bool readTraceProperties(DbReadProgress progressCallback=nullptr) = 0;
        virtual bool readTraceChunkAllTracks(DbReadProgress progressCallback=nullptr) = 0;
        virtual bool readTraceChunkTrackByTrack(DbReadProgress progressCallback=nullptr) = 0;
        void bindTrace(DbBindSctucture bindData) { m_bindData = bindData;};
        bool isTraceBound() {return m_bindData.handler!=nullptr;}
        void configureTraceReadTimePeriod(uint64_t startTime, uint64_t endTime);
        void addTrackToTraceReadConfig(uint16_t trackId);
        void resetReadRequest();

    protected:
        DbBindSctucture m_bindData;
        TraceReadConfig readConfig;
        std::string m_path;
        double getLoadProgress() { return loadProgress; };
        void resetLoadProgress() { loadProgress = 0; }
        void showLoadProgress(double step, const char* action, DbReadProgress progressCallback);

    private:
        double loadProgress;

};
#endif //DATABASE_H