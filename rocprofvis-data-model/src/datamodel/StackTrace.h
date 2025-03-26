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

#ifndef RPV_DATAMODEL_STACK_TRACE_H
#define RPV_DATAMODEL_STACK_TRACE_H


#include "../common/CommonTypes.h"
#include "RpvObject.h"
#include "StackRecord.h"
#include "vector"
#include "memory"


class RpvDmTrace;

// RpvDmStackTrace is class of container object for stack trace records (RpvDmStackRecord)
class RpvDmStackTrace : public RpvObject {
    public:
        // RpvDmStackTrace class constructor
        // @param  ctx - Trace object context
        // @param event_id - 60-bit event id and 4-bit operation type 
        RpvDmStackTrace(    RpvDmTrace* ctx, 
                            rocprofvis_dm_event_id_t event_id) : 
                            m_ctx(ctx), 
                            m_event_id(event_id){}; 
        // RpvDmStackTrace class destructor, not required unless declared as virtual
        ~RpvDmStackTrace(){}
        // Method to add stack trace record
        // @param data - reference to a structure with the record data
        // @return status of operation
        rocprofvis_dm_result_t          AddRecord( rocprofvis_db_stack_data_t & data);
        // Method to get number of stack trace records
        // @return number of records
        rocprofvis_dm_size_t            GetNumberOfRecords() {return m_stack_frames.size();};
        // Method to get amount of memory used by RpvDmStackTrace class object
        // @return used memory size
        rocprofvis_dm_size_t            GetMemoryFootprint();

        // Return trace context pointer
        // @return context pointer
        RpvDmTrace*                     Ctx() {return m_ctx;};
        // Returns event id
        // @return 60-bit event id and 4-bit operation type
        rocprofvis_dm_event_id_t        EventId() {return m_event_id;}


        // Method to read RpvDmStackTrace object property as uint64
        // @param property - property enumeration rocprofvis_dm_stacktrace_property_t
        // @param index - index of any indexed property
        // @param value - pointer reference to uint64_t return value
        // @return status of operation        
        rocprofvis_dm_result_t          GetPropertyAsUint64(rocprofvis_dm_property_t property, 
                                                            rocprofvis_dm_property_index_t index, 
                                                            uint64_t* value) override;
        // Method to read RpvDmStackTrace object property as char*
        // @param property - property enumeration rocprofvis_dm_stacktrace_property_t
        // @param index - index of any indexed property
        // @param value - pointer reference to char* return value
        // @return status of operation 
        rocprofvis_dm_result_t          GetPropertyAsCharPtr(rocprofvis_dm_property_t property, 
                                                            rocprofvis_dm_property_index_t index, 
                                                            char** value) override;
#ifdef TEST
        // Method to get property symbol for testing/debugging
        // @param property - property enumeration rocprofvis_dm_stacktrace_property_t
        // @return pointer to property symbol string 
        const char*                     GetPropertySymbol(rocprofvis_dm_property_t property) override;
#endif
    private:
        // 60-bit event id and 4-bit operation type
        rocprofvis_dm_event_id_t                     m_event_id;
        // pointer to Trace context object
        RpvDmTrace*                                  m_ctx;
        // vector array of stack trace records
        std::vector<RpvDmStackFrameRecord>           m_stack_frames;

        // Method to get pointer to stack frame symbol string at provided index
        // @param index - record index
        // @param category - reference to symbol string
        // @return status of operation
        rocprofvis_dm_result_t          GetRecordSymbolAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & symbol);
        // Method to get pointer to stack frame arguments string at provided index
        // @param index - record index
        // @param args - reference to arguments string
        // @return status of operation
        rocprofvis_dm_result_t          GetRecordArgsAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t &args);
        // Method to get pointer to stack frame code line string at provided index
        // @param index - record index
        // @param code_line - reference to code line string
        // @return status of operation
        rocprofvis_dm_result_t          GetRecordCodeLineAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & code_line);
        // Method to get stack frame depth at provided index
        // @param index - record index
        // @param depth - reference to stack depth value
        // @return status of operation
        rocprofvis_dm_result_t          GetRecordDepthAt(const rocprofvis_dm_property_index_t index, uint32_t & depth);
};

#endif //RPV_DATAMODEL_STACK_TRACE_H