#ifndef TRACE_TYPES__H
#define TRACE_TYPES__H
#include <cstdint>
#include <string>

#define INVALID_RECORD_ID 0xFFFFFFFFFFFFFFFF
#define INVALID_RECORD_INDEX 0xFFFFFFFF
#define INVALID_RECORD_SIZE 0xFFFFFFFF
#define INVALID_TRACK_INDEX 0xFFFF
#define INVALID_TRACK_SIZE 0xFFFF

typedef enum {
    CPU = 1,
    GPU = 2,
    METRICS = 3
} RecordType;

typedef RecordType TrackType;

typedef union{
    struct CPU_RECORD
    {
        uint64_t id;
        uint64_t timestamp;
        int64_t duration;
        uint32_t apiCallId;
        uint32_t argumentsId;
    } Cpu;
    struct GPU_RECORD
    {
        uint64_t id;
        uint64_t timestamp;
        int64_t duration;
        uint32_t typeId;
        uint32_t descriptionId;
    } Gpu;
    struct METRIC_RECORD
    {
        uint64_t timestamp;
        double value;
    } Metric;
} DataRecord;

typedef struct 
{
    RecordType type;
    DataRecord data;
} TrackData;

typedef struct
{
    uint64_t recordId;
    uint64_t timestamp;
    uint16_t trackId;
} FlowId;

typedef struct 
{
    uint64_t recordId;
    FlowId   flowId;
} FlowRecord;


typedef struct{
    TrackType type;
    union{
        struct CPU_TRACK{
            uint32_t processId;
            uint32_t threadId;
        } Cpu;
        struct GPU_TRACK{
            uint32_t gpuId;
            uint32_t queueId;
        } Gpu;
        struct METRIC_TRACK{
            uint32_t gpuId;
            uint32_t metricId;
        } Metric;  
        uint64_t value;
    } data;
} TrackProperties;


typedef uint32_t (*addTrack) (const void* handler, TrackProperties & data,  uint32_t size);
typedef uint64_t (*addTrackRecord) (const void* handler, uint16_t trackId, RecordType type, DataRecord & data);
typedef uint32_t (*addStringRecord) (const void* handler, const char* stringValue);
typedef uint32_t (*addFlowRecord) (const void* handler, uint16_t trackId, FlowRecord & data);
typedef void (*setTraceParameters) (const void* handler, const uint64_t minTime, const uint64_t maxTime, const size_t stringListSize);
typedef void* (*addTrackArray) (const void* handler, uint16_t trackId, uint64_t startTime, uint64_t endTime);


#endif //TRACE_TYPES__H