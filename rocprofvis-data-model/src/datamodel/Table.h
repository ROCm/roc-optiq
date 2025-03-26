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

#ifndef RPV_DATAMODEL_TABLE_H
#define RPV_DATAMODEL_TABLE_H


#include "../common/CommonTypes.h"
#include "TableRow.h"
#include <vector>


class RpvDmTrace;

// RpvDmTable is class of table object, container of RpvDmTableRow objects
class RpvDmTable : public RpvObject {
    public:
        // RpvDmTable class constructor
        // @param  ctx - Trace object context
        // @param description - pointer to table description string
        // @param query - pointer to table query string

        RpvDmTable(  RpvDmTrace* ctx, rocprofvis_dm_charptr_t description, rocprofvis_dm_charptr_t query) : 
                                                        m_ctx(ctx), m_query(query), m_description(description) {}; 
        // RpvDmTable class destructor, not required unless declared as virtual
        ~RpvDmTable(){}
        // Method to add column name to a vector array of columns
        // @param column_name - pointer to column name string
        // @return status of operation
        rocprofvis_dm_result_t                  AddColumn(rocprofvis_dm_charptr_t column_name);
        // Method to create and add empty row object to a vector array of rows
        // @param column_name - pointer to column name string
        // @return status of operation
        rocprofvis_dm_table_row_t               AddRow();
        // Returns number of columns
        rocprofvis_dm_size_t                    GetNumberOfColumns() {return m_columns.size();};
        // Returns number of rows
        rocprofvis_dm_size_t                    GetNumberOfRows() {return m_rows.size();};
        // Method to get amount of memory used by RpvDmTable class object
        // @return used memory size
        rocprofvis_dm_size_t                    GetMemoryFootprint();

        // Returns trace context pointer
        RpvDmTrace*                             Ctx() {return m_ctx;};
        // Returns pointer to query string
        rocprofvis_dm_charptr_t                 Query() {return m_query.c_str();}
        // Returns pointer to description string
        rocprofvis_dm_charptr_t                 Description() {return m_description.c_str();}

        // Method to read RpvDmTable object property as uint64
        // @param property - property enumeration rocprofvis_dm_table_property_t
        // @param index - index of any indexed property
        // @param value - pointer reference to uint64_t return value
        // @return status of operation   
        rocprofvis_dm_result_t                  GetPropertyAsUint64(rocprofvis_dm_property_t property, 
                                                            rocprofvis_dm_property_index_t index, 
                                                            uint64_t* value) override;  
        // Method to read RpvDmTable object property as char*
        // @param property - property enumeration rocprofvis_dm_table_property_t
        // @param index - index of any indexed property
        // @param value - pointer reference to char* return value
        // @return status of operation  
        rocprofvis_dm_result_t                  GetPropertyAsCharPtr(rocprofvis_dm_property_t property, 
                                                            rocprofvis_dm_property_index_t index, 
                                                            char** value) override;
        // Method to read RpvDmTable object property as handle (aka void*)
        // @param property - property enumeration rocprofvis_dm_table_property_t
        // @param index - index of any indexed property
        // @param value - pointer reference to handle (aka void*) return value
        // @return status of operation
        rocprofvis_dm_result_t                  GetPropertyAsHandle(rocprofvis_dm_property_t property, 
                                                            rocprofvis_dm_property_index_t index, 
                                                            rocprofvis_dm_handle_t* value) override;
#ifdef TEST
        // Method to get property symbol for testing/debugging
        // @param property - property enumeration rocprofvis_dm_table_property_t
        // @return pointer to property symbol string 
        const char*                             GetPropertySymbol(rocprofvis_dm_property_t property) override;
#endif

    private:
        // pointer to Trace context object
        RpvDmTrace*                                         m_ctx;
        // vector array of column names
        std::vector<std::string>                            m_columns;
        // vector array of row objects
        std::vector<RpvDmTableRow>                          m_rows;
        // table query string
        std::string                                         m_query;
        // table description string
        std::string                                         m_description;

        // Method to get pointer to column name at provided index
        // @param index - column index
        // @param column - reference to column string
        // @return status of operation
        rocprofvis_dm_result_t          GetColumnNameAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & column);
        // Method to get handle to row (RpvDmTableRow) object at provided index
        // @param index - row index
        // @param row - reference to row (RpvDmTableRow) object
        // @return status of operation
        rocprofvis_dm_result_t          GetRowHandleAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_handle_t & row);

};

#endif //RPV_DATAMODEL_TABLE_H