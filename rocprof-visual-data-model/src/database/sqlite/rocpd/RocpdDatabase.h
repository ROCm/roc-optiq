#ifndef ROCPD_DATABASE_H
#define ROCPD_DATABASE_H

#include "../SqliteDatabase.h"
#include <vector>
#include <map>

typedef struct
{
    uint32_t id;
    uint32_t dId;
    std::string name;
} MetricName;

typedef struct
{
    uint32_t trackId;
    uint64_t recordId;
} LinkageId;

typedef struct
{
    uint64_t apiId;
    uint64_t opId;
} FlowPair;

typedef std::map<uint64_t, uint32_t> StringMap;
typedef std::pair<uint64_t,uint32_t> StringPair;
typedef std::map<uint64_t, LinkageId> LinkageMap;
typedef std::pair<uint64_t,LinkageId> LinkagePair;


class RocpdDatabase : public SqliteDatabase
{
    public:
        RocpdDatabase(const char* path);
        bool readTraceChunkAllTracks(DbReadProgress progressCallback=nullptr);
        bool readTraceChunkTrackByTrack(DbReadProgress progressCallback=nullptr);
        bool readTraceProperties(DbReadProgress progressCallback=nullptr);
    private:

        static int callback_get_min_time(void *data, int argc, char **argv, char **azColName);
        static int callback_get_max_time(void *data, int argc, char **argv, char **azColName);
        static int callback_add_string_count(void *data, int argc, char **argv, char **azColName);
        static int callback_add_cpu_track(void *data, int argc, char **argv, char **azColName);
        static int callback_add_gpu_track(void *data, int argc, char **argv, char **azColName);
        static int callback_add_metric_track(void *data, int argc, char **argv, char **azColName);
        static int callback_add_kernel_launch_record(void *data, int argc, char **argv, char **azColName);
        static int callback_add_kernel_execute_record(void *data, int argc, char **argv, char **azColName);
        static int callback_add_metrics_record(void *data, int argc, char **argv, char **azColName);
        static int callback_add_kernel_launch_record_all_tracks(void *data, int argc, char **argv, char **azColName);
        static int callback_add_kernel_execute_record_all_tracks(void *data, int argc, char **argv, char **azColName);
        static int callback_add_metrics_record_all_tracks(void *data, int argc, char **argv, char **azColName);
        static int callback_add_strings_record(void *data, int argc, char **argv, char **azColName);
        static int callback_add_flow_info(void *data, int argc, char **argv, char **azColName);
        
        static int callback_fill_flow_array(void *data, int argc, char **argv, char **azColName);

        static const char* getRecordQuery(TrackProperties props);
        const bool convertStringId(const uint64_t dbStringId, uint32_t & stringId);
        const char* getMetricNameByTrackId(uint32_t trackId);
        const uint16_t getTrackIdByTrackProperties(TrackProperties & props);
        const uint16_t getTrackIdByMetricNameAndGpuId(char* metricName, uint16_t gpuId);
        const bool isTrackRequested(uint16_t trackId);

        long long                       minTime;
        long long                       maxTime;
        size_t                          stringCount;
        std::vector<TrackProperties>    trackProps;
        StringMap                       stringRefs;
        std::vector<MetricName>         metricNames;
        int                             trackId;
        std::vector<FlowPair>        testLinkageArray;


};
#endif //ROCPD_DATABASE_H