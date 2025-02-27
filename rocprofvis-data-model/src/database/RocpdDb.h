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

#ifndef RPV_ROCPD_DATABASE_H
#define RPV_ROCPD_DATABASE_H

#include "SqliteDb.h"

#include <map>

typedef std::map<uint64_t, uint32_t> StringMap;
typedef std::pair<uint64_t,uint32_t> StringPair;

class RocpdDatabase : public SqliteDatabase
{
public:
    RocpdDatabase(rocprofvis_db_filename_t path,
        rocprofvis_db_read_progress progress_callback = nullptr) :
        SqliteDatabase(path, progress_callback) {
    };

    rocprofvis_dm_result_t  ReadTraceMetadata(Future* object) override;
    rocprofvis_dm_result_t  ReadTraceSlice(
        rocprofvis_dm_timestamp_t start,
        rocprofvis_dm_timestamp_t end,
        rocprofvis_db_num_of_tracks_t num,
        rocprofvis_db_track_selection_t tracks,
        Future* object) override;
    rocprofvis_dm_result_t  ReadFlowTraceInfo(
        rocprofvis_dm_event_id_t event_id,
        Future* object) override;
    rocprofvis_dm_result_t  ReadStackTraceInfo(
        rocprofvis_dm_event_id_t event_id,
        Future* object) override;
    
    
    rocprofvis_dm_result_t UpdateStringMap(StringPair ids);

    static int CallbackGetMinTime(void* data, int argc, char** argv, char** azColName);
    static int CallbackGetMaxTime(void* data, int argc, char** argv, char** azColName);
    static int CallBackAddTrack(void* data, int argc, char** argv, char** azColName);
    static int CallBackAddString(void *data, int argc, char **argv, char **azColName);
    static int CallbackAddEventRecord(void *data, int argc, char **argv, char **azColName);
    static int CallbackAddPmcRecord(void *data, int argc, char **argv, char **azColName);
private:
    
    StringMap m_string_map;

    const bool ReIndexStringId(const uint64_t dbStringId, uint32_t& stringId);

};
#endif //RPV_ROCPD_DATABASE_H