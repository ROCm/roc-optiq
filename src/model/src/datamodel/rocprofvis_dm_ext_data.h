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
#include "rocprofvis_dm_base.h"
#include "rocprofvis_dm_ext_data_record.h"
#include <vector>
#include <memory>

namespace RocProfVis
{
namespace DataModel
{

class Trace;

// ExtData is class of container object for extended data records (ExtDataRecord)
// Can be used to store extended data records for event or for track
class ExtData : public DmBase {
    public:
        // Default ExtData constructor, for statically allocated objects
        ExtData() :
                            m_ctx(nullptr),  
                            m_id({0}),
                            m_json_blob("") {}; 
        // ExtData class constructor
        // param  ctx - Trace object context
        // @param id - 60-bit event id and 4-bit operation type (operation type is 0 when container is used for Track-related extended data)
        ExtData(       Trace* ctx, 
                            rocprofvis_dm_event_id_t id) :
                            m_ctx(ctx),  
                            m_id(id),
                            m_json_blob("") {}; 
        // ExtData class destructor, not required unless declared as virtual
        ~ExtData(){}
        // Return trace context pointer
        // @return context pointer
        Trace*                     Ctx() {return m_ctx;};
        // Returns event id
        // @return 60-bit event id and 4-bit operation type
        rocprofvis_dm_event_id_t        EventId() {return m_id;}
        // Returns class mutex
        std::shared_mutex* Mutex() override { return &m_lock; }
        // Method to add extended data record
        // @param data - reference to a structure with the record data
        // @return status of operation
        rocprofvis_dm_result_t          AddRecord( rocprofvis_db_ext_data_t & data);
        // Method to get number of extended data records
        // @return number of records
        rocprofvis_dm_size_t            GetNumberOfRecords() {return m_extdata_records.size();};
        // Method to get amount of memory used by ExtData class object
        // @return used memory size
        rocprofvis_dm_size_t            GetMemoryFootprint();
        // Method to check if container already has same record, for prefenting duplication
        // @param data - reference to a structure with the record data
        // @return True if record exists
        bool                            HasRecord(rocprofvis_db_ext_data_t & data);

        // Method to read ExtData object property as uint64
        // @param property - property enumeration rocprofvis_dm_extdata_property_t
        // @param index - index of any indexed property
        // @param value - pointer reference to uint64_t return value
        // @return status of operation
        rocprofvis_dm_result_t          GetPropertyAsUint64(rocprofvis_dm_property_t property, 
                                                            rocprofvis_dm_property_index_t index, 
                                                            uint64_t* value) override;  
        // Method to read ExtData object property as char*
        // @param property - property enumeration rocprofvis_dm_extdata_property_t
        // @param index - index of any indexed property
        // @param value - pointer reference to char* return value
        // @return status of operation
        rocprofvis_dm_result_t          GetPropertyAsCharPtr(rocprofvis_dm_property_t property, 
                                                            rocprofvis_dm_property_index_t index, 
                                                            char** value) override;
#ifdef TEST
        // Method to get property symbol for testing/debugging
        // @param property - property enumeration rocprofvis_dm_extdata_property_t
        // @return pointer to property symbol string 
        const char*                     GetPropertySymbol(rocprofvis_dm_property_t property) override;
#endif

    private:
        // 60-bit event id and 4-bit operation type
        rocprofvis_dm_event_id_t        m_id;
        // pointer to Trace context object
        Trace*                     m_ctx;
        // vector array of extended data records
        std::vector<ExtDataRecord> m_extdata_records;
        // string object to keep json blob generated by concatenating extended data records. 
        std::string                     m_json_blob;
        // object mutex, for shared access
        mutable std::shared_mutex   m_lock;

        // method to get record category string at provided record index
        // @param index - record index
        // @category  - reference to category string
        // @return status of operation
        rocprofvis_dm_result_t          GetRecordCategoryAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & category);
        // method to get record name string at provided record index
        // @param index - record index
        // @category  - reference to name string
        // @return status of operation
        rocprofvis_dm_result_t          GetRecordNameAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & name);
        // method to get record data string at provided record index
        // @param index - record index
        // @category  - reference to data string
        // @return status of operation
        rocprofvis_dm_result_t          GetRecordDataAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t &data);
        // method to get data type at provided record index
        // @param index - record index
        // @type  - reference to data type
        // @return status of operation
        rocprofvis_dm_result_t          GetRecordTypeAt(const rocprofvis_dm_property_index_t index, uint64_t &type);
        // method to get data category enumeration at provided record index
        // @param index - record index
        // @type  - reference to data category enumeration
        // @return status of operation
        rocprofvis_dm_result_t GetRecordCategoryEnumAt(const rocprofvis_dm_property_index_t index, uint64_t& category_enum);


};

}  // namespace DataModel
}  // namespace RocProfVis