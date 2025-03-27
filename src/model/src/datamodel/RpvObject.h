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

#ifndef RPV_DATAMODEL_OBJECT_H
#define RPV_DATAMODEL_OBJECT_H

#include "CommonTypes.h"

// base class for all classes that implement property access interface methods
class RpvObject {
public:
    // virtual destructor must be defined in order to free derived classes resources  
    virtual ~RpvObject(){}
    // Virtual method to read object property as uint64
    // @param property - property enumeration 
    // @param index - index of any indexed property
    // @param value - pointer reference to uint64_t return value
    // @return status of operation
    virtual rocprofvis_dm_result_t GetPropertyAsUint64(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, uint64_t* value);
    // Virtual method to read object property as int64
    // @param property - property enumeration 
    // @param index - index of any indexed property
    // @param value - pointer reference to int64_t return value
    // @return status of operation
    virtual rocprofvis_dm_result_t GetPropertyAsInt64(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, int64_t* value);
    // Virtual method to read object property as char*
    // @param property - property enumeration 
    // @param index - index of any indexed property
    // @param value - pointer reference to char* return value
    // @return status of operation
    virtual rocprofvis_dm_result_t GetPropertyAsCharPtr(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, char** value);
    // Virtual method to read object property as double
    // @param property - property enumeration 
    // @param index - index of any indexed property
    // @param value - pointer reference to double return value
    // @return status of operation
    virtual rocprofvis_dm_result_t GetPropertyAsDouble(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, double* value);
    // Virtual method to read object property as handle (aka void*)
    // @param property - property enumeration 
    // @param index - index of any indexed property
    // @param value - pointer reference to handle (aka void*) return value
    // @return status of operation
    virtual rocprofvis_dm_result_t GetPropertyAsHandle(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, rocprofvis_dm_handle_t* value);
#ifdef TEST
    // Virtual method to get property symbol for testing/debugging
    // @param property - property enumeration rocprofvis_dm_extdata_property_t
    // @return pointer to property symbol string 
    virtual const char*            GetPropertySymbol(rocprofvis_dm_property_t property);
#endif
};

#endif //RPV_DATAMODEL_OBJECT_H