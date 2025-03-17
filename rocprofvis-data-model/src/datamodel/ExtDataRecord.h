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

#ifndef RPV_DATAMODEL_EXT_DATA_RECORD_H
#define RPV_DATAMODEL_EXT_DATA_RECORD_H

/*  RpvPmcRecord is a data storage class for performance counters data
**  It's POD (plain old data) class for memory usage optimization.
**  PMC (Performance metric counter) event represents a point on timeline
**  where X-dimension is timestamp anf Y-dimension is value.
*/ 

#include "../common/CommonTypes.h"

class RpvDmExtDataRecord 
{
    public:
        RpvDmExtDataRecord(rocprofvis_dm_charptr_t category, rocprofvis_dm_charptr_t name, rocprofvis_dm_charptr_t data):
            m_category(category), m_name(name), m_data(data) {};
        rocprofvis_dm_charptr_t        Category() {return m_category.c_str();}
        rocprofvis_dm_charptr_t        Name() {return m_name.c_str();}
        bool                           Compare(rocprofvis_dm_charptr_t category, rocprofvis_dm_charptr_t name) {return (m_category.compare(category) == 0) && (m_name.compare(name) == 0);}
        rocprofvis_dm_charptr_t        Data() {return m_data.c_str();}
    private:
        rocprofvis_dm_string_t              m_category;
        rocprofvis_dm_string_t              m_name;
        rocprofvis_dm_string_t              m_data;
};



#endif //RPV_DATAMODEL_EXT_DATA_RECORD_H