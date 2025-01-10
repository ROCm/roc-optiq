#include "data/TraceInterface.h"
#include "database/sqlite/rocpd/RocpdDatabase.h"
#include "database/DbInterface.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <random>

unsigned long long startTime;
unsigned long long endTime;
unsigned int startTrack;
unsigned int endTrack;
double totalTime;

void db_read_progress(const int progress, const char* action)
{
    std::cout << "\rLoaded : " << progress << "% - " << action << std::string(40,' ') << std::flush;
}

void query_read_progress(const int progress, const char* action)
{
    std::cout << "Query : " << action << std::endl;
}

bool generateChunkConfig(TraceHandler trace, DBHandler db, unsigned int numChunksX, unsigned int numChunksY , unsigned int chunkX, unsigned int chunkY)
{
    unsigned int numOfTracks = get_number_of_tracks(trace);
    unsigned long long traceMinTime = get_trace_min_time(trace);
    unsigned long long traceMaxTime = get_trace_max_time(trace);
    unsigned long long traceTime = traceMaxTime - traceMinTime;
    unsigned long long sizeX = traceTime/numChunksX;
    unsigned int sizeY = (numOfTracks+numChunksY-1)/numChunksY;
    if (sizeY < 1) return false;
    if (sizeX < 1000000) return false;
    startTime =  traceMinTime+chunkX*sizeX;
    endTime = startTime + sizeX;
    startTrack = chunkY*sizeY;
    endTrack = chunkY*sizeY+sizeY;
    if (endTrack > numOfTracks) endTrack = numOfTracks;
    configure_trace_read_time_period(db, startTime, endTime);
    std::cout << "Reading " << sizeX << "ns chunk starting at " << startTime;
    std::cout << " for tracks {";
    for (unsigned int i=startTrack; i < endTrack; i++)
    {
        add_track_to_trace_read_config(db, i);
        std::cout << i << ",";
    }
    std::cout << "}" << std::endl;
    return true;
}

void testChunk(TraceHandler trace, DBHandler db, bool allthrough)
{
    auto t0 = std::chrono::steady_clock::now();
    if (allthrough)
        read_trace_chunk_all_tracks(db,  query_read_progress); else
            read_trace_chunk_track_by_track(db, query_read_progress);
    auto t1 = std::chrono::steady_clock::now();
    std::chrono::duration<double> diff = t1 - t0;
    totalTime+=diff.count();
    std::cout << "Read took " << diff.count() << " seconds" << std::endl;

    unsigned long long totalMemoryFootprint = 0;
    unsigned long long totalNumberOfRecords = 0;
    for (unsigned int i=startTrack; i < endTrack; i++)
    {
        TrackHandler track = get_track_at(trace, i);
        if (track)
        {
            std::string groupname = get_track_group_name(track);
            std::string nodename = get_track_name(track);
            RecordArrayHandler chunk = get_track_last_acquaired_chunk(track);
            if (chunk)
            {
                unsigned long long memoryFootprint = get_chunk_memory_footprint(chunk);
                unsigned long long numberOfRecords = get_chunk_number_of_records(chunk);
                totalMemoryFootprint += memoryFootprint;
                totalNumberOfRecords += numberOfRecords;
                std::cout << groupname << " : " << nodename << " has " << numberOfRecords << " records, " << memoryFootprint << " of memory" << std::endl;
            }
        }

    }
    std::cout << "Total for this chunk: " << totalNumberOfRecords << " records, " << totalMemoryFootprint << " of memory" << std::endl;
    delete_trace_chunks_at(trace, startTime);
}

int main(int argc, char **argv)
{
    if (argc > 1)
    {
        std::srand(std::time(nullptr));
        TraceHandler trace = create_trace();
        if (trace!=nullptr)
        {
            DBHandler db = open_rocpd_database(argv[1]);
            if (db != nullptr)
            {
                if (bind_trace_to_database(trace, db))
                {
                    auto t0 = std::chrono::steady_clock::now();
                    if (read_trace_properties(db, db_read_progress))
                    {
                        auto t1 = std::chrono::steady_clock::now();
                        std::chrono::duration<double> diff = t1 - t0;
                        std::cout << std::endl << "Reading trace properties took " << diff.count() << " s" << std::endl;
                        unsigned cont=0;
                        do
                        {
                            totalTime=0;
                            unsigned numChunksX = 1;
                            unsigned numChunksY = 1;
                            std::cout << "Enter amount of chunks to split trace horizontally:";
                            std::cin >> numChunksX;
                            std::cout << "Enter amount of chunks to split trace vertically:";
                            std::cin >> numChunksY;
                            for (unsigned x = 0; x < numChunksX; x++)
                            {
                                for (unsigned y = 0; y < numChunksY; y++)
                                {
                                    if (generateChunkConfig(trace, db, numChunksX, numChunksY, x, y))
                                    {
                                        testChunk(trace, db, numChunksX >= numChunksY);
                                    }
                                }
                            }
                            std::cout << "Reading all chunks took " << totalTime << "seconds" << std::endl;
                            std::cout << "Enter 1 to continue and 0 to exit:";
                            std::cin >> cont;
                        } while (cont);
                    }  
                    else {
                        std::cout << "Error : Failed to bind trace to database!" << std::endl;
                    }
                }
                else {
                    std::cout << "Error : Failed to bind trace to database!" << std::endl;
                }
                close_rocpd_database(db);
            } else {
                std::cout << "Error : Failed to open database!" << std::endl;
            }
        } else
        {
             std::cout << "Error : Failed to create trace!" << std::endl;
        }
    } else
    {
        std::cout << "USAGE:" << std::endl << "\ttest.exe [database]" << std::endl;
    }
}

