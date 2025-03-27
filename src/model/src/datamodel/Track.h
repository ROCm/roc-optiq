// Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved.
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

#ifndef RPV_DATAMODEL_TRACK_H
#define RPV_DATAMODEL_TRACK_H

#include "CommonTypes.h"
#include "TrackSlice.h"
#include "ExtData.h"
#include <vector>
#include <memory> 


class RpvDmTrace;
class RpvDmTrackSlice;

// RpvDmTrack is class of Track object, container of track parameters and time slices
// Base class for RpvPmcTrack, RpvRegionTrack, RpvKernelTrack, etc. 
class RpvDmTrack : public RpvObject {
public:
    // RpvDmTrack class constructor
    RpvDmTrack( RpvDmTrace* ctx, rocprofvis_dm_track_params_t* params);
    // RpvDmTrack class destructor, not required unless declared as virtual
    ~RpvDmTrack(){}
    // Return number of slices
    rocprofvis_dm_size_t                                NumberOfSlices() { return m_slices.size(); }
    // Returns context pointer
    RpvDmTrace*                                         Ctx() { return m_ctx; }
    // Returns track category enumeration value
    rocprofvis_dm_track_category_t                      Category() { return m_track_params->track_category; }
    // Returns track ID. Track id is currently equal to track index in array of tracks
    rocprofvis_dm_track_id_t                            TrackId() { return m_track_params->track_id; }
    // Returns node id
    rocprofvis_dm_node_id_t                             NodeId() { return m_track_params->process_id[TRACK_ID_NODE]; }
    // Returns pointer to process string
    rocprofvis_dm_charptr_t                             Process() { return m_track_params->process_name[TRACK_ID_PID_OR_AGENT].c_str(); }
    // Returns pointer to subprocess string
    rocprofvis_dm_charptr_t                             SubProcess() { return m_track_params->process_name[TRACK_ID_TID_OR_QUEUE].c_str(); }
    // Returns pointer to category string
    rocprofvis_dm_charptr_t                             CategoryString();
    // Method to get slice handle at provided index
    // @param index - index of slice 
    // @param slice - handle to slice
    // @return status of operation 
    rocprofvis_dm_result_t                              GetSliceAtIndex(rocprofvis_dm_property_index_t index, rocprofvis_dm_slice_t & slice);
    // Method to get slice handle at provided timestamp
    // @param start - slice start timestamp
    // @param slice - handle to slice
    // @return status of operation 
    rocprofvis_dm_result_t                              GetSliceAtTime(rocprofvis_dm_timestamp_t start, rocprofvis_dm_slice_t & slice);
    // Method to delete slice with provided handle
    // @param slice - handle to slice
    // @return status of operation 
    rocprofvis_dm_result_t                              DeleteSlice(rocprofvis_dm_slice_t slice);
    // Method to add empty slice object
    // @param start - slice start timestamp
    // @param start - slice end timestamp
    // @return handle to slice 
    rocprofvis_dm_slice_t                               AddSlice(rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end);
    // Method to get amount of memory used by RpvDmTrack class object
    // @return used memory size
    rocprofvis_dm_size_t                                GetMemoryFootprint();
    // Method to get track extended data as JSON Blob
    // @param json - reference to a JSON format string pointer
    // @return status of operation    
    rocprofvis_dm_result_t                              GetExtendedInfoAsJsonBlob(rocprofvis_dm_json_blob_t & json);
    // Method to read RpvDmTrack object property as uint64
    // @param property - property enumeration rocprofvis_dm_track_property_t
    // @param index - index of any indexed property
    // @param value - pointer reference to uint64_t return value
    // @return status of operation  
    rocprofvis_dm_result_t                              GetPropertyAsUint64(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, uint64_t* value) override;
    // Method to read RpvDmTrack object property as char*
    // @param property - property enumeration rocprofvis_dm_track_property_t
    // @param index - index of any indexed property
    // @param value - pointer reference to char* return value
    // @return status of operation    
    rocprofvis_dm_result_t                              GetPropertyAsCharPtr(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, char** value) override;
    // Method to read RpvDmTrack object property as rocprofvis_dm_handle_t
    // @param property - property enumeration rocprofvis_dm_track_property_t
    // @param index - index of any indexed property
    // @param value - pointer reference to rocprofvis_dm_handle_t return value
    // @return status of operation  
    rocprofvis_dm_result_t                              GetPropertyAsHandle(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, rocprofvis_dm_handle_t* value) override;
#ifdef TEST
        // Method to get property symbol for testing/debugging
        // @param property - property enumeration rocprofvis_dm_track_property_t
        // @return pointer to property symbol string 
        const char*                                     GetPropertySymbol(rocprofvis_dm_property_t property) override;
#endif

private:    
    // Trace context pointer
    RpvDmTrace* m_ctx;
    // pointer to track parameters structure, shared with database component
    rocprofvis_dm_track_params_t*                       m_track_params;     // track essential parameters are shared between data model and database
    // array of time slice objects
    std::vector<std::unique_ptr<RpvDmTrackSlice>>       m_slices;
    // track extended data object 
    RpvDmExtData                                        m_ext_data;

};
#endif //RPV_DATAMODEL_TRACK_H
