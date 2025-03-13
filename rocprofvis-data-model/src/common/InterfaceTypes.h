// MIT License
//
// Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef RPV_DATAMODEL_IFACE_TYPES_H
#define RPV_DATAMODEL_IFACE_TYPES_H

#include <cstdint>

#define INVALID_ID 0xFFFFFFFFFFFFFFFF
#define INVALID_TIMESTAMP 0xFFFFFFFFFFFFFFFF
#define INVALID_INDEX 0xFFFFFFFF

/*******************************Types******************************/

typedef 	void* 		                  rocprofvis_dm_handle_t;
typedef     rocprofvis_dm_handle_t        rocprofvis_dm_trace_t;
typedef     rocprofvis_dm_handle_t        rocprofvis_dm_database_t;
typedef     rocprofvis_dm_handle_t        rocprofvis_dm_track_t;
typedef     rocprofvis_dm_handle_t        rocprofvis_dm_slice_t;
typedef     rocprofvis_dm_handle_t        rocprofvis_dm_flowtrace_t;
typedef     rocprofvis_dm_handle_t        rocprofvis_dm_stacktrace_t;
typedef     rocprofvis_dm_handle_t        rocprofvis_dm_extdata_t;
typedef     rocprofvis_dm_handle_t        rocprofvis_dm_table_t;
typedef     rocprofvis_dm_handle_t        rocprofvis_dm_table_row_t;
typedef     uint32_t                      rocprofvis_dm_index_t; 
typedef     uint64_t                      rocprofvis_dm_timestamp_t; 
typedef     uint32_t                      rocprofvis_dm_property_t; 
typedef     uint64_t                      rocprofvis_dm_property_index_t;
typedef     const char*                   rocprofvis_dm_json_blob_t;
typedef     uint32_t                      rocprofvis_dm_track_id_t;


typedef     const char*                   rocprofvis_db_filename_t;
typedef     const char*                   rocprofvis_db_status_message_t;
typedef     uint16_t                      rocprofvis_db_progress_percent_t;
typedef     uint32_t*                     rocprofvis_db_track_selection_t;
typedef     uint16_t                      rocprofvis_db_num_of_tracks_t;
typedef     void*                         rocprofvis_db_future_t; 
typedef     uint64_t                      rocprofvis_db_timeout_ms_t; 
typedef     const char*                   rocprofvis_dm_charptr_t;



/*******************************Enumerations******************************/

enum rocprofvis_dm_result_t
{
    // Operation was successful
    kRocProfVisDmResultSuccess = 0,
    // Operation failed with a non-specific error
    kRocProfVisDmResultUnknownError = 1,
    // Operation failed due to a timeout
    kRocProfVisDmResultTimeout = 2,
    // Operation failed as the data was not yet loaded
    kRocProfVisDmResultNotLoaded = 3,
    // Operation failed due to resources allocation failure
    kRocProfVisDmResultAllocFailure = 4,
    // Operation failed due to invalid parameter passed
    kRocProfVisDmResultInvalidParameter = 5,
    // Operation failed due to database access problem
    kRocProfVisDmResultDbAccessFailed = 6,
    // Operation failed due to invalid property 
    kRocProfVisDmResultInvalidProperty = 7,
    // Operation failed due to unsuported property 
    kRocProfVisDmResultNotSupported = 8,
};

enum roprofvis_dm_track_category_t {
    kRocProfVisDmNotATrack = 0,
    kRocProfVisDmPmcTrack = 1,
    kRocProfVisDmRegionTrack = 2,
    kRocProfVisDmKernelTrack = 3,
    kRocProfVisDmSQTTTrack = 4,
    kRocProfVisDmNICTrack = 5
};

enum roprofvis_dm_event_operation_t {
    kRocProfVisDmOperationNoOp = 0,
    kRocProfVisDmOperationLaunch = 1,
    kRocProfVisDmOperationDispatch = 2,
    kRocProfVisDmOperationMemoryAllocate = 3,
    kRocProfVisDmOperationMemoryCopy = 4
};

enum rocprofvis_db_type_t {
    kAutodetect = 0, 
	kRocpdSqlite = 1,
	kRocprofSqlite = 2
};

enum rocprofvis_db_status_t {
    kRPVDbSuccess = 0,
    kRPVDbError = 1,
    kRPVDbBusy = 2
};

enum  rocprofvis_dm_trace_property_t {
	kRPVDMStartTimeUInt64,
	kRPVDMEndTimeUInt64,
	kRPVDMNumberOfTracksUInt64,
    kRPVDMNumberOfTablesUInt64,
    kRPVDMTraceMemoryFootprintUInt64,
    kRPVDMTrackHandleIndexed,
	kRPVDMFlowTraceHandleByEventID,
	kRPVDMStackTraceHandleByEventID,
    kRPVDMExtInfoHandleByEventID,
    kRPVDMTableHandleIndexed,
	kRPVDMDatabaseHandle,
    kRPVDMEventInfoJsonCharPtrIndexed
};

enum rocprofvis_dm_track_property_t {
	kRPVDMTrackCategoryEnumUInt64,
    kRPVDMTrackIdUInt64,
    kRPVDMTrackNodeIdUInt64,
    kRPVDMTrackMainProcessNameCharPtr,
    kRPVDMTrackSubProcessNameCharPtr,
    kRPVDMTNumberOfSlicesUInt64,
    kRPVDMTrackMemoryFootprintUInt64,
	kRPVDMSliceHandleIndexed,	
    kRPVDMSliceHandleTimed,
    kRPVDMTNumberOfExtDataRecordsUInt64,
    kRPVDMTrackExtDataCategoryCharPtrIndexed,
	kRPVDMTrackExtDataNameCharPtrIndexed,
    kRPVDMTrackExtDataValueCharPtrIndexed,
	kRPVDMTrackInfoJsonCharPtr, 
};

enum rocprofvis_dm_slice_property_t {
	kRPVDMRecordIndexByTimestampUInt64,
    kRPVDMSliceMemoryFootprintUInt64,
	kRPVDMNumberOfRecordsUInt64,
	kRPVDMTimestampUInt64Indexed,
	kRPVDMPmcValueDoubleIndexed,
	kRPVDMEventIdUInt64Indexed,
    kRPVDMEventIdOperationEnumIndexed,
	kRPVDMEventDurationInt64Indexed,
	kRPVDMEventTypeStringCharPtrIndexed,
	kRPVDMEventSymbolStringCharPtrIndexed
};

enum rocprofvis_dm_flowtrace_property_t {
	kRPVDMNumberOfEndpointsUInt64,
    kRPVDMEndpointTrackIDUInt64Indexed,
	kRPVDMEndpointIDUInt64Indexed,
	kRPVDMEndpointTimestampUInt64Indexed,
};

enum rocprofvis_dm_stacktrace_property_t {
	kRPVDMNumberOfFramesUInt64,
	kRPVDMFrameDepthUInt32Indexed,
    kRPVDMFrameSymbolCharPtrIndexed,
    kRPVDMFrameArgsCharPtrIndexed,
    kRPVDMFrameCodeLineCharPtrIndexed,
};

enum rocprofvis_dm_extdata_property_t {
    kRPVDMNumberOfExtDataRecordsUInt64,
    kRPVDMExtDataCategoryCharPtrIndexed,
    kRPVDMExtDataNameCharPtrIndexed,
    kRPVDMExtDataCharPtrIndexed,
    kRPVDMExtDataJsonBlobCharPtr
};

enum rocprofvis_dm_table_property_t {
    kRPVDMNumberOfTableColumsUInt64,
    kRPVDMNumberOfTableRowsUInt64,
    kRPVDMExtTableDescriptionCharPtr,
    kRPVDMExtTableQueryCharPtr,
    kRPVDMExtTableColumnNameCharPtrIndexed,
    kRPVDMExtTableRowHandleIndexed
};

enum rocprofvis_dm_table_row_property_t {
    kRPVDMNumberOfTableRowCellsUInt64,
    kRPVDMExtTableRowCellValueCharPtrIndexed
};

enum rocprofvis_dm_event_property_type_t {
    kRPVDMEventFlowTrace,
    kRPVDMEventStackTrace,
    kRPVDMEventExtData,
};



typedef union { 
    struct {
        uint64_t    event_id:60;
        uint64_t    event_op:4;
    } bitfiled;
    uint64_t        value;
} rocprofvis_dm_event_id_t;

/*******************************Callbacks******************************/

typedef void ( *rocprofvis_db_progress_callback_t)(
		        rocprofvis_db_filename_t,
                rocprofvis_db_progress_percent_t, 
                rocprofvis_db_status_t, 
                rocprofvis_db_status_message_t
);

#endif //RPV_DATAMODEL_IFACE_TYPES_H