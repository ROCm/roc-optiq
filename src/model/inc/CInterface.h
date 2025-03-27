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

#ifndef RPV_DATAMODEL_INTERFACE_H
#define RPV_DATAMODEL_INTERFACE_H

/*
* When built with CFFI, headers must not include directives
* Also CFFI does not support function override with different set of parameters
* This header can only be include by C/C++ code. CFFI build must use Interface.h and InterfaceTypes.h
* This header makes interface functions declarations pure C
* This header also adds property access methods, which return values using pointer references
* and status of the operation as return value
*/

#include "CInterfaceTypes.h"

extern "C"
{
 #include "Interface.h" 
}

/***********************************Universal property getters**************************************
*
* There are 10 property getter methods. Two methods to get property of each type:
*       uint64, int64, double, char* and rocprofvis_dm_handle_t
* First set of methods returns values as pointer references and result of operation as return value
* Second set returns property values directly. No result of operation is returned.
* In second case if operation fails, 0 or nullptr will be returned.
****************************************************************************************************/

// Method to get object property value as uint64. Value is returned as pointer reference.
rocprofvis_dm_result_t  rocprofvis_dm_get_property_as_uint64(
                                    rocprofvis_dm_handle_t, 	            // object handle
                                    rocprofvis_dm_property_t,               // property enumeration
                                    rocprofvis_dm_property_index_t,         // indexed property index
                                    uint64_t*                               // return value as uint64_t* 
                                    );                                     

// Method to get object property value as int64. Value is returned as pointer reference.
rocprofvis_dm_result_t  rocprofvis_dm_get_property_as_int64(
                                    rocprofvis_dm_handle_t,                 // object handle 	
                                    rocprofvis_dm_property_t,               // property enumeration
                                    rocprofvis_dm_property_index_t,         // indexed property index
                                    int64_t*                                // return value as int64_t*
                                    ); 

// Method to get object property value as double. Value is returned as pointer reference.
rocprofvis_dm_result_t  rocprofvis_dm_get_property_as_double(
                                    rocprofvis_dm_handle_t,                 // object handle 	
                                    rocprofvis_dm_property_t,               // property enumeration               
                                    rocprofvis_dm_property_index_t,         // indexed property index
                                    double*                                 // return value as double*
                                    ); 

// Method to get object property value as char*. Value is returned as pointer reference.
rocprofvis_dm_result_t  rocprofvis_dm_get_property_as_charptr(
                                    rocprofvis_dm_handle_t,                 // object handle	
                                    rocprofvis_dm_property_t,               // property enumeration
                                    rocprofvis_dm_property_index_t,         // indexed property index
                                    char**                                  // return value as char**
                                    ); 

// Method to get object property value as handle (void*). Value is returned as pointer reference.
rocprofvis_dm_result_t  rocprofvis_dm_get_property_as_handle(
                                    rocprofvis_dm_handle_t,                 // object handle 	
                                    rocprofvis_dm_property_t,               // property enumeration
                                    rocprofvis_dm_property_index_t,         // indexed property index
                                    rocprofvis_dm_handle_t*                 // return value as void**
                                    ); 

#endif //RPV_DATAMODEL_INTERFACE_H