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
#ifndef RPV_DATABASE_H
#define RPV_DATABASE_H

#include "Future.h"
#include <vector>

class Database
{
    public:
        Database(   
                    rocprofvis_db_filename_t path):
                    m_path(path),
                    m_binding_info({0}) {
        };

        virtual rocprofvis_dm_result_t  Open() = 0;
        virtual rocprofvis_dm_result_t  Close() = 0;
        virtual bool                    IsOpen() = 0; 


        static rocprofvis_db_type_t     Autodetect(   
                                                                rocprofvis_db_filename_t filename);

        rocprofvis_dm_result_t          BindTrace(
                                                                rocprofvis_dm_db_bind_struct binding_info);

        rocprofvis_dm_result_t          ReadTraceMetadataAsync( 
                                                                rocprofvis_db_future_t object);
        rocprofvis_dm_result_t          ReadTraceSliceAsync( 
                                                                rocprofvis_dm_timestamp_t start,
                                                                rocprofvis_dm_timestamp_t end,
                                                                rocprofvis_db_num_of_tracks_t num,
                                                                rocprofvis_db_track_selection_t tracks,
                                                                rocprofvis_db_future_t object);

        rocprofvis_dm_result_t          ReadEventPropertyAsync(
                                                                rocprofvis_dm_event_property_type_t type,
                                                                rocprofvis_dm_event_id_t event_id,
                                                                rocprofvis_db_future_t object);
        rocprofvis_dm_result_t          ExecuteQueryAsync(
                                                                rocprofvis_dm_charptr_t query,
                                                                rocprofvis_dm_charptr_t description,
                                                                rocprofvis_db_future_t object);
    private:
    // static
        static rocprofvis_dm_result_t   ReadTraceMetadataStatic(
                                                                Database* db, 
                                                                Future* object);
        static rocprofvis_dm_result_t   ReadTraceSliceStatic(
                                                                Database* db,
                                                                rocprofvis_dm_timestamp_t start,
                                                                rocprofvis_dm_timestamp_t end,
                                                                rocprofvis_db_num_of_tracks_t num,
                                                                rocprofvis_db_track_selection_t tracks,
                                                                Future* object);

        static rocprofvis_dm_result_t   ReadEventPropertyStatic(
                                                                Database* db, 
                                                                rocprofvis_dm_event_property_type_t type,
                                                                rocprofvis_dm_event_id_t event_id,
                                                                Future* object);
        static rocprofvis_dm_result_t   ExecuteQueryStatic(
                                                                Database* db,
                                                                rocprofvis_dm_charptr_t query,
                                                                rocprofvis_dm_charptr_t description,
                                                                Future* object);
    // pure virtual
        virtual rocprofvis_dm_result_t  ReadTraceMetadata(
                                                                Future* object) = 0;
        virtual rocprofvis_dm_result_t  ReadTraceSlice(
                                                                rocprofvis_dm_timestamp_t start,
                                                                rocprofvis_dm_timestamp_t end,
                                                                rocprofvis_db_num_of_tracks_t num,
                                                                rocprofvis_db_track_selection_t tracks,
                                                                Future* object) = 0;
        virtual rocprofvis_dm_result_t  ReadFlowTraceInfo(
                                                                rocprofvis_dm_event_id_t event_id,
                                                                Future* object) = 0;
        virtual rocprofvis_dm_result_t  ReadStackTraceInfo(
                                                                rocprofvis_dm_event_id_t event_id,
                                                                Future* object) = 0;
        virtual rocprofvis_dm_result_t  ReadExtEventInfo(
                                                                rocprofvis_dm_event_id_t event_id,
                                                                Future* object) = 0;
        virtual rocprofvis_dm_result_t  ExecuteQuery(
                                                                rocprofvis_dm_charptr_t query,
                                                                rocprofvis_dm_charptr_t description,
                                                                Future* future) = 0; 
    private:
        rocprofvis_dm_db_bind_struct m_binding_info;
        rocprofvis_db_filename_t m_path;
        std::vector<std::unique_ptr<rocprofvis_dm_track_params_t>> m_track_properties;

    protected:
        rocprofvis_dm_db_bind_struct *  BindObject() {return &m_binding_info;}
        rocprofvis_dm_result_t          AddTrackProperties(rocprofvis_dm_track_params_t& props);
        rocprofvis_db_filename_t        Path() {return m_path;}
        rocprofvis_dm_size_t            NumTrackProperties() { return m_track_properties.size(); }
        rocprofvis_dm_track_params_t*   TrackPropertiesLast() { return m_track_properties.back().get(); }
        rocprofvis_dm_track_params_t*   TrackPropertiesAt(rocprofvis_dm_index_t index) { return m_track_properties[index].get(); }
        rocprofvis_dm_trace_params_t*   TraceParameters() { return m_binding_info.trace_parameters; }
        rocprofvis_dm_result_t          FindTrackIdByName(const char* process, const char* name, rocprofvis_dm_track_id_t & track_id );
        void                            ShowProgress(double step, rocprofvis_dm_charptr_t action, rocprofvis_db_status_t status, Future* future);
};

#endif //RPV_DATABASE_H