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
#ifndef RPV_DATAMODEL_EVENT_RECORD_H
#define RPV_DATAMODEL_EVENT_RECORD_H

#include "../common/CommonTypes.h"

/*  RpvEventRecord is a data storage class for Event record data for any types of event
**  It's POD (plain old data) class for memory usage optimization.
**  Event represents an object with timeline location and duration as primary properties.
**  Event has event category and event description indexes pointing to string and 
**  symbol tables stored in RpvDmTrace class. Strings are reindexed when loaded
**  resulting in possibility of using 32-bit indexes
*/ 
class RpvDmEventRecord 
{
    public:
        RpvDmEventRecord(
                const rocprofvis_dm_event_id_t event_id, 
                const rocprofvis_dm_timestamp_t timestamp, 
                const rocprofvis_dm_duration_t duration, 
                const rocprofvis_dm_index_t category_index, 
                const rocprofvis_dm_index_t symbol_index
                ):
                m_event_id(event_id), 
                m_timestamp(timestamp), 
                m_duration(duration), 
                m_category_index(category_index), 
                m_symbol_index(symbol_index) {};

        const rocprofvis_dm_id_t                EventId() {return m_event_id.bitfiled.event_id;}
        const rocprofvis_dm_timestamp_t         Timestamp() {return m_timestamp;}
        const rocprofvis_dm_duration_t          Duration() {return m_duration;}
        const rocprofvis_dm_index_t             CategoryIndex() {return m_category_index;}
        const rocprofvis_dm_index_t             SymbolIndex() {return m_symbol_index;}
        const roprofvis_dm_event_operation_t    Operation() {return (roprofvis_dm_event_operation_t)m_event_id.bitfiled.event_op;}

    private:
        rocprofvis_dm_event_id_t        m_event_id;                     // 60-bit event id and 4-bit operation type , used as a search key for stacktrace, flowtrace and no-essential information
        rocprofvis_dm_timestamp_t       m_timestamp;                    // 64-bit timestamp 
        rocprofvis_dm_duration_t        m_duration;                     // signed 64-bit duration. Negative number should be invalidated by controller.
        rocprofvis_dm_index_t           m_category_index;               // 32-bit category index of array of strings  
        rocprofvis_dm_index_t           m_symbol_index;                 // 32-bit symbol index of array of strings 
};



#endif //RPV_DATAMODEL_KERNEL_EVENT_H