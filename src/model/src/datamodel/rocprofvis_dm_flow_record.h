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

#pragma once

#include "rocprofvis_common_types.h"

// FlowRecord  is a data storage class for flow trace parameters
class FlowRecord 
{
    public:
        // FlowRecord class constructor. Object stores data flow endpoint parameters
        // @param timestamp - endpoint timestamp
        // @param event_id - endpoint 60-bit event id and 4-bit operation type
        // @param track_id - endpoint track id
        FlowRecord(const rocprofvis_dm_timestamp_t timestamp,
                   const rocprofvis_dm_event_id_t  event_id,
                   const rocprofvis_dm_track_id_t  track_id,
                   const rocprofvis_dm_index_t     category_id,
                   const rocprofvis_dm_index_t     symbol_id,
                   const rocprofvis_dm_event_level_t level)
        :
            m_timestamp(timestamp), m_event_id(event_id), m_track_id(track_id), m_category_id(category_id),m_symbol_id(symbol_id),m_level(level) {};
        // Returns endpoint timestamp
        rocprofvis_dm_timestamp_t       Timestamp() {return m_timestamp;}
        // Returns endpoint event id (60-bit event id and 4-bit operation type)
        rocprofvis_dm_event_id_t        EventId() {return m_event_id;}
        // Returns endpoint track id
        rocprofvis_dm_track_id_t        TrackId() {return m_track_id;}
        // Returns endpoint category id
        rocprofvis_dm_index_t           CategoryId() { return m_category_id; }
        // Returns endpoint category id
        rocprofvis_dm_index_t           SymbolId() { return m_symbol_id; }
        // Returns endpoint level in track
        rocprofvis_dm_event_level_t     Level() { return m_level; }
    private:
        // endpoint timestamp
        rocprofvis_dm_timestamp_t           m_timestamp;
        // endpoint 60-bit event id and 4-bit operation type
        rocprofvis_dm_event_id_t            m_event_id;
        // endpoint track id
        rocprofvis_dm_track_id_t            m_track_id;
        // category id
        rocprofvis_dm_index_t               m_category_id;
        // name id
        rocprofvis_dm_index_t               m_symbol_id;
        // endpoint level in track
        rocprofvis_dm_event_level_t         m_level;
};

