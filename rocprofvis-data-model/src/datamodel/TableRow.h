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

#ifndef RPV_DATAMODEL_TABLE_ROW_H
#define RPV_DATAMODEL_TABLE_ROW_H


#include "../common/CommonTypes.h"
#include "RpvObject.h"
#include <vector>


class RpvDmTable;

// RpvDmTableRow is class of table row object
class RpvDmTableRow : public RpvObject {
    public:
        // RpvDmTableRow class constructor
        // @param  ctx - Table object context
        RpvDmTableRow(  RpvDmTable* ctx) : m_ctx(ctx) {}; 
        // RpvDmTableRow class destructor, not required unless declared as virtual
        ~RpvDmTableRow(){}
        // Method to add a cell to a row
        // @param value pointer to table value string
        // @return status of operation
        rocprofvis_dm_result_t          AddCellValue(rocprofvis_dm_charptr_t value);
        // Returns number of cells in a row
        rocprofvis_dm_size_t            GetNumberOfCells() {return m_values.size();};
        // Method to get amount of memory used by RpvDmTableRow class object
        // @return used memory size
        rocprofvis_dm_size_t            GetMemoryFootprint();

        // Return table object context pointer
        // @return context pointer
        RpvDmTable*                     Ctx() {return m_ctx;};

        // Method to read RpvDmTableRow object property as uint64
        // @param property - property enumeration rocprofvis_dm_table_row_property_t
        // @param index - index of any indexed property
        // @param value - pointer reference to uint64_t return value
        // @return status of operation   
        rocprofvis_dm_result_t          GetPropertyAsUint64(rocprofvis_dm_property_t property, 
                                                            rocprofvis_dm_property_index_t index, 
                                                            uint64_t* value) override;  
        // Method to read RpvDmTableRow object property as char*
        // @param property - property enumeration rocprofvis_dm_table_row_property_t
        // @param index - index of any indexed property
        // @param value - pointer reference to char* return value
        // @return status of operation   
        rocprofvis_dm_result_t          GetPropertyAsCharPtr(rocprofvis_dm_property_t property, 
                                                            rocprofvis_dm_property_index_t index, 
                                                            char** value) override;
#ifdef TEST
        // Method to get property symbol for testing/debugging
        // @param property - property enumeration rocprofvis_dm_table_row_property_t
        // @return pointer to property symbol string 
        const char*                     GetPropertySymbol(rocprofvis_dm_property_t property) override;
#endif

    private:
        // Context pointer
        RpvDmTable*                     m_ctx;
        // Vector array of string values
        std::vector<std::string>        m_values;

        // Method to get pointer to table value string at provided index
        // @param index - record index
        // @param value - reference to table value string
        // @return status of operation
        rocprofvis_dm_result_t          GetValueAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & value);

};

#endif //RPV_DATAMODEL_TABLE_ROW_H