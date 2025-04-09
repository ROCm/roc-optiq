#pragma once

#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_types.h"
#include "rocprofvis_structs.h"
#include "rocprofvis_raw_track_data.h"

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

class DataProvider
{
public:
    DataProvider();
    ~DataProvider();

    /*
    *   Close the controller.
    */
    void CloseController();

    /*
    *   Free all requests. This does not cancel the requests on the controller end._yn
    */
    void FreeRequests();
    
    /*
    *   Free all track data
    */
    void FreeAllTracks();

    /*
    * Opens a trace file and loads the data into the controller.
    * Any previous data will be cleared.
    * @param file_path: The path to the trace file to load.
    * 
    */
    bool FetchTrace(const std::string& file_path);

    /*
    * Fetches a track from the controller. Stores the data in a raw track buffer.
    * @param index: The index of the track to select
    * @param start_ts: The start timestamp of the track
    * @param end_ts: The end timestamp of the track
    * @param horz_pixel_range: The horizontal pixel range of the view
    * @param lod: The level of detail to use
    */
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
