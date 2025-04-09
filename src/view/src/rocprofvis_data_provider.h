#pragma once

#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_types.h"
#include "rocprofvis_structs.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace RocProfVis
{
namespace View
{

enum class ProviderState
{
    kInit,
    kLoading,
    kReady,
    kError
};

typedef struct track_info_t
{
    uint64_t                           index;  // index of the track in the controller
    uint64_t                           id;     // id of the track in the controller
    std::string                        name;   // name of the track
    rocprofvis_controller_track_type_t track_type;  // the type of track
    double                             min_ts;      // starting time stamp of track
    double                             max_ts;      // ending time stamp of track
} track_info_t;

typedef struct data_req_info_t
{
    uint64_t                        index;
    rocprofvis_controller_future_t* graph_future;
    rocprofvis_controller_array_t*  graph_array;
    rocprofvis_handle_t*            graph_obj;
    ProviderState                   loading_state;
} data_req_info_t;

class RawTrackData
{
public:
    RawTrackData(rocprofvis_controller_track_type_t track_type, uint64_t index,
                 double start_ts, double end_ts);
    virtual ~RawTrackData() = 0;
    rocprofvis_controller_track_type_t GetType();
    double                             GetStartTs();
    double                             GetEndTs();
    uint64_t                           GetIndex();

protected:
    rocprofvis_controller_track_type_t m_track_type;

    double m_start_ts;  // starting time stamp of track data
    double m_end_ts;    // ending time stamp of track data

    uint64_t m_index;  // index of the track
};

class RawTrackSampleData : public RawTrackData
{
public:
    RawTrackSampleData(uint64_t count, uint64_t index, double start_ts, double end_ts);
    virtual ~RawTrackSampleData();
    std::vector<rocprofvis_trace_counter_t>& GetData();

private:
    std::vector<rocprofvis_trace_counter_t> m_data;
};

class RawTrackEventData : public RawTrackData
{
public:
    RawTrackEventData(uint64_t count, uint64_t index, double start_ts, double end_ts);
    virtual ~RawTrackEventData();
    std::vector<rocprofvis_trace_event_t>& GetData();

private:
    std::vector<rocprofvis_trace_event_t> m_data;
};

class DataProvider
{
public:
    DataProvider();
    ~DataProvider();

    void CloseController();
    void FreeRequests();
    void FreeAllTracks();

    bool FetchTrace(const std::string& file_path);
    bool FetchTrack(uint64_t index, double start_ts, double end_ts, int horz_pixel_range,
                    int lod);

    /*
    * Release memory buffer holding raw data for selected track
    * @param index: The index of the track to select
    */
    bool FreeTrack(uint64_t index);

    /*
     * Output track list meta data.
     */
    void DumpMetaData();

    /*
     * Output raw data of a track to the log.
     * @param index: The index of the track to dump.
     */
    bool DumpTrack(uint64_t index);

    /*
     * Performs all data processing.  Call this from the "game loop".
     */
    void Update();

    /*
    * Gets the start timestamp of the trace
    */
    double GetStartTime();

    /*
    * Gets the end timestamp of the trace
    */
    double GetEndTime();

private:
    void HandleLoadTrace();
    void HandleLoadTrackMetaData();
    void HandleLoadGraphs();

    void ProcessRequest(data_req_info_t& req);
    void CreateRawFlameData(uint64_t index, rocprofvis_controller_array_t* track_data,
                            double min_ts, double max_ts);
    void CreateRawSampleData(uint64_t index, rocprofvis_controller_array_t* track_data,
                             double min_ts, double max_ts);

    rocprofvis_controller_future_t*   m_trace_future;
    rocprofvis_controller_t*          m_trace_controller;
    rocprofvis_controller_timeline_t* m_trace_timeline;

    ProviderState m_state;

    uint64_t m_num_graphs = 0;  // number of graphs contained in the trace
    double   m_min_ts     = 0;  // timeline start point
    double   m_max_ts     = 0;  // timeline end point

    std::vector<track_info_t>  m_track_metadata;
    std::vector<RawTrackData*> m_raw_trackdata;

    std::unordered_map<int64_t, data_req_info_t> m_requests;
};

}  // namespace View
}  // namespace RocProfVis
