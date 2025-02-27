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

#ifndef RPV_DATAMODEL_TRACK_H
#define RPV_DATAMODEL_TRACK_H

#include "../common/CommonTypes.h"
#include "RpvObject.h"
#include <vector>


class RpvDmTrace;
class RpvDmTrackSlice;


/* Base class for RpvPmcTrack, RpvRegionTrack, RpvKernelTrack, etc. 
*/

class RpvDmTrack : public RpvObject {
public:
    RpvDmTrack( RpvDmTrace* ctx,
                rocprofvis_dm_track_params_t* params) :
                m_ctx(ctx),
                m_track_params(params) {};

    rocprofvis_dm_size_t                                NumberOfSlices() { return m_slices.size(); }
    RpvDmTrace* Ctx() { return m_ctx; }
    roprofvis_dm_track_category_t                       Category() { return m_track_params->track_category; }
    rocprofvis_dm_track_id_t                            TrackId() { return m_track_params->track_id; }
    rocprofvis_dm_node_id_t                             NodeId() { return m_track_params->node_id; }
    rocprofvis_dm_charptr_t                             Process() { return m_track_params->process.c_str(); }
    rocprofvis_dm_charptr_t                             SubProcess() { return m_track_params->name.c_str(); }

    rocprofvis_dm_result_t                              GetSliceAtIndex(rocprofvis_dm_index_t index, rocprofvis_dm_slice_t & slice);
    rocprofvis_dm_result_t                              GetSliceAtTime(rocprofvis_dm_timestamp_t start, rocprofvis_dm_slice_t & slice);
    rocprofvis_dm_result_t                              DeleteSlice(rocprofvis_dm_slice_t slice);
    rocprofvis_dm_slice_t                               AddSlice(rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end);

    rocprofvis_dm_size_t                                GetMemoryFootprint();
    rocprofvis_dm_result_t                              GetExtendedInfo(rocprofvis_dm_json_blob_t & json);

    rocprofvis_dm_result_t                              GetPropertyAsUint64(rocprofvis_dm_track_property_t property, rocprofvis_dm_property_index_t index, uint64_t* value) override;
    rocprofvis_dm_result_t                              GetPropertyAsInt64(rocprofvis_dm_track_property_t property, rocprofvis_dm_property_index_t index, int64_t* value) override;
    rocprofvis_dm_result_t                              GetPropertyAsCharPtr(rocprofvis_dm_track_property_t property, rocprofvis_dm_property_index_t index, char** value) override;
    rocprofvis_dm_result_t                              GetPropertyAsDouble(rocprofvis_dm_track_property_t property, rocprofvis_dm_property_index_t index, double* value) override;
    rocprofvis_dm_result_t                              GetPropertyAsHandle(rocprofvis_dm_track_property_t property, rocprofvis_dm_property_index_t index, rocprofvis_dm_handle_t* value) override;

private:    

    RpvDmTrace* m_ctx;
    rocprofvis_dm_track_params_t* m_track_params;     // track essential parameters are shared between data model and database
    std::vector<RpvDmTrackSlice*>                       m_slices;

};
#endif //RPV_DATAMODEL_TRACK_H
