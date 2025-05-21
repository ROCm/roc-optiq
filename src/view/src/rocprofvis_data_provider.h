#pragma once

#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_types.h"
#include "rocprofvis_raw_track_data.h"
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

enum class RequestType
{
    kFetchTrack,
    kFetchGraph,
    kFetchTrackEventTable
};

typedef struct track_info_t
{
    uint64_t                           index;  // index of the track in the controller
    uint64_t                           id;     // id of the track in the controller
    std::string                        name;   // name of the track
    rocprofvis_controller_track_type_t track_type;   // the type of track
    double                             min_ts;       // starting time stamp of track
    double                             max_ts;       // ending time stamp of track
    uint64_t                           num_entries;  // number of entries in the track
} track_info_t;

typedef struct data_req_info_t
{
    uint64_t                           index;  // index of track that is being requested
    rocprofvis_controller_future_t*    request_future;  // future for the request
    rocprofvis_controller_array_t*     request_array;   // array of data for the request
    rocprofvis_handle_t*               request_obj_handle;  // object for the request
    rocprofvis_controller_arguments_t* request_args;        // arguments for the request
    ProviderState                      loading_state;       // state of the request
    RequestType                        request_type;        // type of request
    double start_ts;  // start time stamp of data being requested
    double end_ts;    // end time stamp of data being requested
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
     *   Free all requests. This does not cancel the requests on the controller end.
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

    bool FetchWholeTrack(uint64_t index, double start_ts, double end_ts,
                         int horz_pixel_range, int lod);

    /*
     * Fetches an event table from the controller for a single track.
     * @param index: The index of the track to select
     * @param start_ts: The start timestamp of the event table
     * @param end_ts: The end timestamp of the event table
     */
    // bool FetchEventTable(uint64_t index, double start_ts, double end_ts);
    bool FetchEventTable(uint64_t index, double start_ts, double end_ts,
                         uint64_t start_row = -1, uint64_t req_row_count = -1);

    bool FetchMultiTrackEventTable(const std::vector<uint64_t>& track_indices,
                                   double start_ts, double end_ts,
                                   uint64_t start_row = -1, uint64_t req_row_count = -1);

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

    void DumpEventTable();

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

    /*
     * Get access to the raw track data.  The returned pointer should only be
     * considered valid until the next call to Update() or any of the memory
     * freeing functions and FetchTrace() or CloseController().
     * @param index: The index of the track data to fetch.
     * @return: Pointer to the raw track data
     * or null if data for this track has not been fetched.
     */
    const RawTrackData* GetRawTrackData(uint64_t index);

    const track_info_t* GetTrackInfo(uint64_t index);

    uint64_t GetTrackCount();

    const std::string& GetTraceFilePath();

    ProviderState GetState();

    void SetTrackDataReadyCallback(
        const std::function<void(uint64_t, const std::string&)>& callback);
    void SetTraceLoadedCallback(const std::function<void(const std::string&)>& callback);

private:
    void HandleLoadTrace();
    void HandleLoadTrackMetaData();
    void HandleRequests();

    void ProcessRequest(data_req_info_t& req);
    void ProcessGraphRequest(data_req_info_t& req);
    void ProcessTrackRequest(data_req_info_t& req);
    void ProcessEventTableRequest(data_req_info_t& req);

    bool SetupEventTableCommonArguments(rocprofvis_controller_arguments_t* args,
                                        double start_ts, double end_ts,
                                        uint64_t start_row, uint64_t req_row_count);

    void CreateRawEventData(uint64_t index, rocprofvis_controller_array_t* track_data,
                            double min_ts, double max_ts);
    void CreateRawSampleData(uint64_t index, rocprofvis_controller_array_t* track_data,
                             double min_ts, double max_ts);

    rocprofvis_controller_future_t*   m_trace_future;
    rocprofvis_controller_t*          m_trace_controller;
    rocprofvis_controller_timeline_t* m_trace_timeline;

    ProviderState m_state;

    uint64_t    m_num_graphs;       // number of graphs contained in the trace
    double      m_min_ts;           // timeline start point
    double      m_max_ts;           // timeline end point
    std::string m_trace_file_path;  // path to the trace file

    std::vector<track_info_t>  m_track_metadata;
    std::vector<RawTrackData*> m_raw_trackdata;

    std::vector<std::string>              m_event_table_header;
    std::vector<std::vector<std::string>> m_event_table_data;

    std::unordered_map<int64_t, data_req_info_t> m_requests;

    // Called when new track data is ready
    std::function<void(uint64_t, const std::string&)> m_track_data_ready_callback;
    // Called when a new trace is loaded
    std::function<void(const std::string&)> m_trace_data_ready_callback;
};

}  // namespace View
}  // namespace RocProfVis
