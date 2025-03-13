#include "Database.h"
#include <cassert>

Database::Database(const char* path) : m_path(path) { 
    m_bindData.handler = nullptr; 
    loadProgress = 0; 
    readConfig.startTime=0;
    readConfig.endTime=0;
    m_threadWorking = false;
}

void Database::showLoadProgress(double step, const char* action, DbReadProgress progressCallback) { 
    loadProgress += step; 
    if (progressCallback) progressCallback((int)loadProgress, action); 
};

void Database::configureTraceReadTimePeriod(uint64_t startTime, uint64_t endTime)
{
    readConfig.startTime=startTime;
    readConfig.endTime=endTime;
}

void Database::addTrackToTraceReadConfig(uint16_t trackId)
{
    readConfig.tracks.push_back(trackId);
}

void Database::resetReadRequest()
{
    readConfig.tracks.clear();
    readConfig.startTime=0;
    readConfig.endTime=0;
}

void Database::readTracePropertiesStatic(Database* db, DbReadProgress progressCallback)
{
    db->m_threadWorking = true;
    db->m_threadResult = db->readTraceProperties(progressCallback);
    db->m_threadWorking = false;
}

void Database::readTraceChunkTrackByTrackStatic(Database* db, DbReadProgress progressCallback)
{
    db->m_threadWorking = true;
    db->m_threadResult = db->readTraceChunkTrackByTrack(progressCallback);
    db->m_threadWorking = false;
}

void Database::readTraceChunkAllTracksStatic(Database* db, DbReadProgress progressCallback)
{
    db->m_threadWorking = true;
    db->m_threadResult = db->readTraceChunkAllTracks(progressCallback);
    db->m_threadWorking = false;
}

bool Database::readTracePropertiesAsync(DbReadProgress progressCallback)
{
    assert(!checkAsyncInProgress()  && "Thread is in use");
    m_thread = std::thread(Database::readTracePropertiesStatic,this,progressCallback);
    return true;
}

bool Database::readTraceChunkAllTracksAsync(DbReadProgress progressCallback)
{
    assert(!checkAsyncInProgress() && "Thread is in use");
    m_thread = std::thread(Database::readTraceChunkAllTracksStatic,this,progressCallback);
    return true;
}

bool Database::readTraceChunkTrackByTrackAsync(DbReadProgress progressCallback) 
{
    bool result;
    assert(!checkAsyncInProgress() && "Thread is in use");
    m_thread = std::thread(Database::readTraceChunkTrackByTrackStatic,this,progressCallback);
    return true;
}

bool Database::checkAsyncInProgress()
{
    if (!m_threadWorking && m_thread.joinable())
    {
        m_thread.join();
    }  
    return m_threadWorking;
} 

