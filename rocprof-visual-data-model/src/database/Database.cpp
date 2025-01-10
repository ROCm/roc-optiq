#include "Database.h"

Database::Database(const char* path) : m_path(path) { 
    m_bindData.handler = nullptr; 
    loadProgress = 0; 
    readConfig.startTime=0;
    readConfig.endTime=0;
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
