#ifndef DATABASE_H
#define DATABASE_H

#include <cstdint>
#include <string>
#include <vector>
#include "../data/TraceTypes.h"
#include <thread>
#include <atomic>


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
        bool readTracePropertiesAsync(DbReadProgress progressCallback=nullptr);
        bool readTraceChunkAllTracksAsync(DbReadProgress progressCallback=nullptr);
        bool readTraceChunkTrackByTrackAsync(DbReadProgress progressCallback=nullptr);  
        static void readTracePropertiesStatic(Database* db, DbReadProgress progressCallback);
        static void readTraceChunkAllTracksStatic(Database* db, DbReadProgress progressCallback);
        static void readTraceChunkTrackByTrackStatic(Database* db, DbReadProgress progressCallback);  
        bool checkAsyncInProgress();   

    protected:
        DbBindSctucture m_bindData;
        TraceReadConfig readConfig;
        std::string m_path;
        double getLoadProgress() { return loadProgress; };
        void resetLoadProgress() { loadProgress = 0; }
        void showLoadProgress(double step, const char* action, DbReadProgress progressCallback);

    private:
        double loadProgress;
        std::thread m_thread;
        std::atomic_bool m_threadResult;
        std::atomic_bool m_threadWorking;

};
#endif //DATABASE_H