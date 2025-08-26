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

namespace RocProfVis
{
namespace DataModel
{

// ExtDataRecord is a data storage class for extended data parameters
class ExtDataRecord 
{
    public:
        // ExtDataRecord class constructor
        // @param category - extended data category string
        // @param name - extended data name string
        // @param data - extended data string
        ExtDataRecord(rocprofvis_dm_charptr_t category, rocprofvis_dm_charptr_t name,
                      rocprofvis_dm_charptr_t data, rocprofvis_db_data_type_t type,
                      rocprofvis_event_data_category_enum_t category_enum)
        :
            m_category(category), m_name(name), m_data(data), m_type(type), m_category_enum(category_enum) {};
        // Returns pointer to extended data category string 
        rocprofvis_dm_charptr_t        Category() {return m_category.c_str();}
        // Returns pointer to extended data name string 
        rocprofvis_dm_charptr_t        Name() {return m_name.c_str();}
        // Returns pointer to extended data string 
        rocprofvis_dm_charptr_t        Data() {return m_data.c_str();}
        // Returns pointer to extended data string
        uint64_t                       Type() { return m_type; }
        uint64_t                       CategoryEnum() { return m_category_enum; }
        // Checks if category and name tags are equal to the ones stored in record
        // @param category - pointer to category string to compare
        // @param name - pointer to name string to compare
        // @return True if equal
        bool                           Equal(rocprofvis_dm_charptr_t category, rocprofvis_dm_charptr_t name) {return (m_category.compare(category) == 0) && (m_name.compare(name) == 0);}

    private:
        // category string object
        rocprofvis_dm_string_t              m_category;
        // name string object
        rocprofvis_dm_string_t              m_name;
        // data string object
        rocprofvis_dm_string_t              m_data;
        // data type
        rocprofvis_db_data_type_t           m_type;
        // data category enumeration
        rocprofvis_event_data_category_enum_t m_category_enum;
        
};

}  // namespace DataModel
}  // namespace RocProfVis