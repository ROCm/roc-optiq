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

#ifndef RPV_DATAMODEL_PMC_RECORD_H
#define RPV_DATAMODEL_PMC_RECORD_H

/*  RpvPmcRecord is a data storage class for performance counters data
**  It's POD (plain old data) class for memory usage optimization.
**  PMC (Performance metric counter) event represents a point on timeline
**  where X-dimension is timestamp anf Y-dimension is value.
*/ 

#include "../common/CommonTypes.h"

class RpvDmPmcRecord 
{
    public:
        RpvDmPmcRecord(const rocprofvis_dm_timestamp_t timestamp, const rocprofvis_dm_value_t value):
            m_timestamp(timestamp), m_value(value) {};
        const rocprofvis_dm_timestamp_t     Timestamp() {return m_timestamp;}
        const rocprofvis_dm_value_t         Value() {return m_value;}
    private:
        rocprofvis_dm_timestamp_t           m_timestamp;
        rocprofvis_dm_value_t               m_value;
};



#endif //RPV_DATAMODEL_PMC_RECORD_H