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

/* Base class for PMC and Event time slices
** Defines set of virtual methods to access an array of POD storage objects 
*/

class RpvDmTrace;

class RpvDmStackTrace : public RpvObject {
    public:
        RpvDmStackTrace(    RpvDmTrace* ctx, 
                            rocprofvis_dm_event_id_t event_id) : 
                            m_ctx(ctx), 
                            m_event_id(event_id){}; 
        ~RpvDmStackTrace(){}
        rocprofvis_dm_result_t  AddRecord( rocprofvis_db_stack_data_t & data);
        rocprofvis_dm_size_t    GetNumberOfRecords() {return m_stack_frames.size();};
        rocprofvis_dm_size_t    GetMemoryFootprint();

        // getters
        RpvDmTrace*                     Ctx() {return m_ctx;};
        rocprofvis_dm_event_id_t        EventId() {return m_event_id;}

        
        rocprofvis_dm_result_t          GetPropertyAsUint64(rocprofvis_dm_property_t property, 
                                                            rocprofvis_dm_property_index_t index, 
                                                            uint64_t* value) override;
        rocprofvis_dm_result_t          GetPropertyAsCharPtr(rocprofvis_dm_property_t property, 
                                                            rocprofvis_dm_property_index_t index, 
                                                            char** value) override;
#ifdef TEST
        const char*                     GetPropertySymbol(rocprofvis_dm_property_t property) override;
#endif
    private:

        rocprofvis_dm_event_id_t                     m_event_id;
        RpvDmTrace*                                  m_ctx;
        std::vector<RpvDmStackFrameRecord>           m_stack_frames;

        // accessors
        rocprofvis_dm_result_t          GetRecordSymbolAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & symbol);
        rocprofvis_dm_result_t          GetRecordArgsAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t &args);
        rocprofvis_dm_result_t          GetRecordCodeLineAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & code_line);
        rocprofvis_dm_result_t          GetRecordDepthAt(const rocprofvis_dm_property_index_t index, uint32_t & depth);
};

#endif //RPV_DATAMODEL_STACK_TRACE_H